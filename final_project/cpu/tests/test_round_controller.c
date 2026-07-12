#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "round_controller.h"

static int _test_failures = 0;
static int _test_count = 0;
static int _test_start = 0;

static void _check(const char *file, int line, int cond, const char *msg)
{
    _test_count++;
    if (!cond) {
        _test_failures++;
        printf("  FAIL [%s:%d] %s\n", file, line, msg);
    }
}

#define CHECK(cond) _check(__FILE__, __LINE__, (cond), #cond)
#define CHECK_EQ(a, b) _check(__FILE__, __LINE__, ((a) == (b)), #a " == " #b)
#define TEST(name) \
    printf("  %-62s", name " "); fflush(stdout); _test_start = _test_failures
#define PASS() \
    do { int d = _test_failures - _test_start; \
         if (d == 0) printf("PASS\n"); else printf("%d FAILED\n", d); } while (0)

static task_match_result_t make_match(uint8_t action,
                                      uint8_t is_target,
                                      reason_code_t reason)
{
    task_match_result_t m;
    memset(&m, 0, sizeof(m));
    m.action = action;
    m.is_target = is_target;
    m.reason = reason;
    m.mode = TASK_MODE_COLOR_CUBE;
    return m;
}

static round_controller_input_t base_input(uint32_t now_ms)
{
    round_controller_input_t in;
    memset(&in, 0, sizeof(in));
    in.now_ms = now_ms;
    in.arm_enabled = 0;
    return in;
}

static void send_event(round_controller_t *rc,
                       round_controller_output_t *out,
                       uint32_t now_ms,
                       uint8_t seq,
                       round_event_t event)
{
    round_controller_input_t in = base_input(now_ms);
    in.event_valid = 1;
    in.event_seq = seq;
    in.event = event;
    round_controller_tick(rc, &in, out);
}

static void test_config_apply_place_to_acquire(void)
{
    TEST("round: config apply then place confirm enters acquire");
    round_controller_t rc;
    round_controller_output_t out;
    round_controller_init(&rc, 0, 0);
    CHECK_EQ(round_controller_get_state(&rc), ROUND_STATE_CONFIG);

    send_event(&rc, &out, 10, 1, ROUND_EVENT_APPLY_CONFIG);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);
    CHECK_EQ(out.event_ack_valid, 1);
    CHECK_EQ(out.event_ack_seq, 1);

    send_event(&rc, &out, 20, 2, ROUND_EVENT_PLACE_CONFIRM);
    CHECK_EQ(out.state, ROUND_STATE_ACQUIRE_STABLE);
    PASS();
}

static void test_skip_round_holds_result_until_remove(void)
{
    TEST("round: non-target SKIP completes and holds result until remove");
    round_controller_t rc;
    round_controller_output_t out;
    round_controller_input_t in;

    round_controller_init(&rc, 0, 0);
    send_event(&rc, &out, 1, 1, ROUND_EVENT_APPLY_CONFIG);
    send_event(&rc, &out, 2, 2, ROUND_EVENT_PLACE_CONFIRM);

    in = base_input(100);
    in.observation_valid = 1;
    in.match = make_match(MATCH_ACTION_SKIP, 0, REASON_COLOR_MISMATCH);
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_LATCH_RECOGNITION);

    in = base_input(110);
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_LATCH_DECISION);
    in.now_ms = 120;
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_EXECUTE_OR_SKIP);
    in.now_ms = 130;
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(out.result_valid, 1);
    CHECK_EQ(out.decision_action, MATCH_ACTION_SKIP);
    CHECK_EQ(out.reason, REASON_COLOR_MISMATCH);

    in = base_input(140);
    in.observation_valid = 1;
    in.match = make_match(MATCH_ACTION_GRAB, 1, REASON_TARGET_MATCH);
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(out.decision_action, MATCH_ACTION_SKIP);

    send_event(&rc, &out, 150, 3, ROUND_EVENT_REMOVE_CONFIRM);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);
    CHECK_EQ(out.round_seq, 1);
    PASS();
}

static void test_target_arm_disabled_finishes_not_ready(void)
{
    TEST("round: target with ARM_DISABLED finishes with ARM_NOT_READY");
    round_controller_t rc;
    round_controller_output_t out;
    round_controller_input_t in;

    round_controller_init(&rc, 0, 0);
    send_event(&rc, &out, 1, 1, ROUND_EVENT_APPLY_CONFIG);
    send_event(&rc, &out, 2, 2, ROUND_EVENT_PLACE_CONFIRM);

    in = base_input(100);
    in.observation_valid = 1;
    in.match = make_match(MATCH_ACTION_GRAB, 1, REASON_TARGET_MATCH);
    round_controller_tick(&rc, &in, &out);
    in = base_input(110);
    round_controller_tick(&rc, &in, &out);
    in.now_ms = 120;
    round_controller_tick(&rc, &in, &out);
    in.now_ms = 130;
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(out.decision_action, MATCH_ACTION_NONE);
    CHECK_EQ(out.reason, REASON_ARM_NOT_READY);
    CHECK_EQ(out.request_arm_grab, 0);
    PASS();
}

static void test_target_arm_enabled_requests_once(void)
{
    TEST("round: target with arm enabled requests one grab then waits done");
    round_controller_t rc;
    round_controller_output_t out;
    round_controller_input_t in;

    round_controller_init(&rc, 0, 0);
    send_event(&rc, &out, 1, 1, ROUND_EVENT_APPLY_CONFIG);
    send_event(&rc, &out, 2, 2, ROUND_EVENT_PLACE_CONFIRM);

    in = base_input(100);
    in.observation_valid = 1;
    in.arm_enabled = 1;
    in.match = make_match(MATCH_ACTION_GRAB, 1, REASON_TARGET_MATCH);
    round_controller_tick(&rc, &in, &out);
    in = base_input(110);
    in.arm_enabled = 1;
    round_controller_tick(&rc, &in, &out);
    in.now_ms = 120;
    round_controller_tick(&rc, &in, &out);
    in.now_ms = 130;
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_ARM_DONE);
    CHECK_EQ(out.request_arm_grab, 1);

    in.now_ms = 140;
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_ARM_DONE);
    CHECK_EQ(out.request_arm_grab, 0);

    in.now_ms = 150;
    in.arm_done = 1;
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    PASS();
}

static void run_one_skip_round(round_controller_t *rc,
                               round_controller_output_t *out,
                               uint32_t base_ms,
                               uint8_t *event_seq)
{
    round_controller_input_t in;

    send_event(rc, out, base_ms, (*event_seq)++, ROUND_EVENT_PLACE_CONFIRM);
    in = base_input(base_ms + 1u);
    in.observation_valid = 1;
    in.match = make_match(MATCH_ACTION_SKIP, 0, REASON_COLOR_MISMATCH);
    round_controller_tick(rc, &in, out);
    in = base_input(base_ms + 2u);
    round_controller_tick(rc, &in, out);
    in.now_ms = base_ms + 3u;
    round_controller_tick(rc, &in, out);
    in.now_ms = base_ms + 4u;
    round_controller_tick(rc, &in, out);
    CHECK_EQ(out->state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(out->decision_action, MATCH_ACTION_SKIP);
    send_event(rc, out, base_ms + 5u, (*event_seq)++,
               ROUND_EVENT_REMOVE_CONFIRM);
    CHECK_EQ(out->state, ROUND_STATE_WAIT_PLACE_CONFIRM);
}

static void test_twenty_round_mock_skip_no_deadlock(void)
{
    TEST("round: 20-round mock SKIP run has no deadlock or arm request");
    round_controller_t rc;
    round_controller_output_t out;
    uint8_t seq = 1u;
    unsigned i;

    round_controller_init(&rc, 0, 0);
    send_event(&rc, &out, 1, seq++, ROUND_EVENT_APPLY_CONFIG);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);

    for (i = 0; i < 20u; ++i) {
        run_one_skip_round(&rc, &out, 100u + (i * 10u), &seq);
        CHECK_EQ(out.request_arm_grab, 0);
    }

    CHECK_EQ(out.round_seq, 20);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);
    PASS();
}

static void test_duplicate_event_seq_ignored(void)
{
    TEST("round: duplicate event_seq is acked once and not re-consumed");
    round_controller_t rc;
    round_controller_output_t out;
    round_controller_init(&rc, 0, 0);

    send_event(&rc, &out, 1, 7, ROUND_EVENT_APPLY_CONFIG);
    CHECK_EQ(out.event_ack_valid, 1);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);

    send_event(&rc, &out, 2, 7, ROUND_EVENT_SESSION_RESET);
    CHECK_EQ(out.event_ack_valid, 0);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);
    PASS();
}

static void test_acquire_timeout_and_soft_reset(void)
{
    TEST("round: acquire timeout records reason and soft reset recovers");
    round_controller_t rc;
    round_controller_output_t out;
    round_controller_input_t in;
    round_controller_config_t cfg = { 30u, 100u };

    round_controller_init(&rc, &cfg, 0);
    send_event(&rc, &out, 1, 1, ROUND_EVENT_APPLY_CONFIG);
    send_event(&rc, &out, 2, 2, ROUND_EVENT_PLACE_CONFIRM);

    in = base_input(40);
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(out.reason, REASON_STABILITY_TIMEOUT);

    send_event(&rc, &out, 50, 3, ROUND_EVENT_SOFT_RESET_ROUND);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);
    PASS();
}

int main(void)
{
    printf("\n=== round_controller unit tests ===\n\n");

    test_config_apply_place_to_acquire();
    test_skip_round_holds_result_until_remove();
    test_target_arm_disabled_finishes_not_ready();
    test_target_arm_enabled_requests_once();
    test_duplicate_event_seq_ignored();
    test_acquire_timeout_and_soft_reset();
    test_twenty_round_mock_skip_no_deadlock();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           _test_count - _test_failures, _test_count, _test_failures);
    return _test_failures ? 1 : 0;
}
