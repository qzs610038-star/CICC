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

/*==========================================================================
 *  审查补测（2026-07-13）：封堵 B 清单第 9/10/11 项覆盖缺口
 *
 *  以下三类行为 round_controller.c 已正确实现，但原测试集未直接覆盖：
 *    - 第 9 项：ABANDON_ROUND 在非运动态结束本轮并保留原因。
 *    - 第 10 项：机械臂运动中（WAIT_ARM_DONE）软复位/放弃必须被忽略，
 *               不得盲目回到放物流程，也不得重复动作脉冲。
 *    - 第 11 项：ARM_FAULT 输入 / 臂超时进入故障态，且不得清空本轮证据
 *               （result_valid、is_target 必须保留）。
 *  本组仅新增 Host/Mock 测试，未改动 round_controller.c/.h 逻辑。
 *==========================================================================*/

/* 驱动状态机到 WAIT_ARM_DONE（目标 + arm_enabled，已发出一次抓取请求）。
 * 进入 WAIT_ARM_DONE 的时刻为 now=130，arm_deadline = 130 + arm_timeout。 */
static void drive_to_wait_arm_done(round_controller_t *rc,
                                   round_controller_output_t *out,
                                   const round_controller_config_t *cfg)
{
    round_controller_input_t in;

    round_controller_init(rc, cfg, 0);
    send_event(rc, out, 1, 1, ROUND_EVENT_APPLY_CONFIG);
    send_event(rc, out, 2, 2, ROUND_EVENT_PLACE_CONFIRM);

    in = base_input(100);
    in.observation_valid = 1;
    in.arm_enabled = 1;
    in.match = make_match(MATCH_ACTION_GRAB, 1, REASON_TARGET_MATCH);
    round_controller_tick(rc, &in, out);
    in = base_input(110);
    in.arm_enabled = 1;
    round_controller_tick(rc, &in, out);
    in.now_ms = 120;
    round_controller_tick(rc, &in, out);
    in.now_ms = 130;
    round_controller_tick(rc, &in, out);
}

static void test_abandon_round_in_acquire_keeps_reason(void)
{
    TEST("round: ABANDON in non-motion state ends round and keeps reason");
    round_controller_t rc;
    round_controller_output_t out;

    round_controller_init(&rc, 0, 0);
    send_event(&rc, &out, 1, 1, ROUND_EVENT_APPLY_CONFIG);
    send_event(&rc, &out, 2, 2, ROUND_EVENT_PLACE_CONFIRM);
    CHECK_EQ(out.state, ROUND_STATE_ACQUIRE_STABLE);

    send_event(&rc, &out, 30, 3, ROUND_EVENT_ABANDON_ROUND);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(out.event_ack_valid, 1);
    CHECK_EQ(out.event_ack_seq, 3);
    CHECK_EQ(out.result_valid, 1);
    CHECK_EQ(out.decision_action, MATCH_ACTION_NONE);
    CHECK_EQ(out.reason, REASON_OPERATOR_ABANDON);

    /* 放弃后本轮已完成，REMOVE_CONFIRM 仍能推进到下一轮 */
    send_event(&rc, &out, 40, 4, ROUND_EVENT_REMOVE_CONFIRM);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);
    CHECK_EQ(out.round_seq, 1);
    PASS();
}

static void test_arm_motion_blocks_soft_reset_and_abandon(void)
{
    TEST("round: arm in motion ignores soft-reset and abandon");
    round_controller_t rc;
    round_controller_output_t out;
    round_controller_input_t in;

    drive_to_wait_arm_done(&rc, &out, 0);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_ARM_DONE);

    /* WAIT_ARM_DONE 期间软复位必须被忽略，不得回到放物流程或重复脉冲 */
    send_event(&rc, &out, 140, 3, ROUND_EVENT_SOFT_RESET_ROUND);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_ARM_DONE);
    CHECK_EQ(out.request_arm_grab, 0);

    /* WAIT_ARM_DONE 期间放弃也必须被忽略 */
    send_event(&rc, &out, 150, 4, ROUND_EVENT_ABANDON_ROUND);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_ARM_DONE);
    CHECK_EQ(out.request_arm_grab, 0);

    /* 只有机械臂真正完成后才推进到本轮完成 */
    in = base_input(160);
    in.arm_done = 1;
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    PASS();
}

static void test_arm_fault_input_keeps_evidence(void)
{
    TEST("round: ARM_FAULT input enters fault and keeps round evidence");
    round_controller_t rc;
    round_controller_output_t out;
    round_controller_input_t in;

    drive_to_wait_arm_done(&rc, &out, 0);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_ARM_DONE);
    CHECK_EQ(out.is_target, 1);

    in = base_input(140);
    in.arm_fault = 1;
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_ARM_FAULT);
    CHECK_EQ(out.reason, REASON_ARM_FAULT);
    CHECK_EQ(out.result_valid, 1);      /* 证据不清空 */
    CHECK_EQ(out.is_target, 1);         /* 本轮确为目标，故障不抹除 */
    CHECK_EQ(out.request_arm_grab, 0);

    /* 故障态是吸收态：普通 tick 不自发脱离，也不再发动作请求 */
    in = base_input(150);
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_ARM_FAULT);
    CHECK_EQ(out.request_arm_grab, 0);

    /* 会话级复位可清场回到 CONFIG */
    send_event(&rc, &out, 160, 3, ROUND_EVENT_SESSION_RESET);
    CHECK_EQ(out.state, ROUND_STATE_CONFIG);
    CHECK_EQ(out.round_seq, 0);
    PASS();
}

static void test_arm_timeout_keeps_evidence(void)
{
    TEST("round: arm timeout enters fault and preserves is_target evidence");
    round_controller_t rc;
    round_controller_output_t out;
    round_controller_input_t in;
    round_controller_config_t cfg = { 3000u, 50u };  /* 短臂超时便于测试 */

    drive_to_wait_arm_done(&rc, &out, &cfg);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_ARM_DONE);

    /* 进入 WAIT_ARM_DONE 于 now=130，arm_timeout=50ms → 截止 180 */
    in = base_input(181);
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_ARM_FAULT);
    CHECK_EQ(out.reason, REASON_ARM_FAULT);
    CHECK_EQ(out.result_valid, 1);
    CHECK_EQ(out.is_target, 1);
    PASS();
}

/*==========================================================================
 *  Codex 复核补测（2026-07-13）：封堵两个 Host 可复现安全缺口
 *    [P1] arm_busy 安全门：arm_enabled=1 但 arm_busy=1 时目标轮不得发起动作。
 *    [P2] event_seq 过期/回绕：只接受向前的 8-bit 序号，过期序号不消费不 ACK，
 *         回绕（255→0）视为向前。
 *  仅新增 Host/Mock 测试；round_controller.c 的对应最小实现已同步落地。
 *==========================================================================*/

static void test_arm_busy_blocks_grab_and_completes_not_ready(void)
{
    TEST("round: arm enabled but busy blocks grab, done with ARM_NOT_READY");
    round_controller_t rc;
    round_controller_output_t out;
    round_controller_input_t in;

    round_controller_init(&rc, 0, 0);
    send_event(&rc, &out, 1, 1, ROUND_EVENT_APPLY_CONFIG);
    send_event(&rc, &out, 2, 2, ROUND_EVENT_PLACE_CONFIRM);

    in = base_input(100);
    in.observation_valid = 1;
    in.arm_enabled = 1;
    in.arm_busy = 1;                 /* 机械臂正忙 */
    in.match = make_match(MATCH_ACTION_GRAB, 1, REASON_TARGET_MATCH);
    round_controller_tick(&rc, &in, &out);   /* ACQUIRE → LATCH_RECOGNITION */
    in = base_input(110);
    in.arm_enabled = 1;
    in.arm_busy = 1;
    round_controller_tick(&rc, &in, &out);   /* → LATCH_DECISION */
    in.now_ms = 120;
    round_controller_tick(&rc, &in, &out);   /* → EXECUTE_OR_SKIP */
    in.now_ms = 130;
    round_controller_tick(&rc, &in, &out);   /* busy → ROUND_DONE，不发动作 */

    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(out.request_arm_grab, 0);
    CHECK_EQ(out.reason, REASON_ARM_NOT_READY);
    CHECK_EQ(out.result_valid, 1);
    CHECK_EQ(out.is_target, 1);
    CHECK_EQ(out.decision_action, MATCH_ACTION_NONE);
    PASS();
}

static void test_stale_event_seq_rejected(void)
{
    TEST("round: stale (older) event_seq is rejected and not acked");
    round_controller_t rc;
    round_controller_output_t out;

    round_controller_init(&rc, 0, 0);
    /* seq=8 被接受 → WAIT_PLACE_CONFIRM */
    send_event(&rc, &out, 10, 8, ROUND_EVENT_APPLY_CONFIG);
    CHECK_EQ(out.event_ack_valid, 1);
    CHECK_EQ(out.event_ack_seq, 8);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);

    /* seq=7 过期：PLACE_CONFIRM 必须被拒绝，状态不变 */
    send_event(&rc, &out, 20, 7, ROUND_EVENT_PLACE_CONFIRM);
    CHECK_EQ(out.event_ack_valid, 0);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);

    /* seq=7 过期：SESSION_RESET 也必须被拒绝，不回到 CONFIG */
    send_event(&rc, &out, 30, 7, ROUND_EVENT_SESSION_RESET);
    CHECK_EQ(out.event_ack_valid, 0);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);
    PASS();
}

static void test_event_seq_wraparound_accepted(void)
{
    TEST("round: event_seq wrap 255->0 is accepted and advances state");
    round_controller_t rc;
    round_controller_output_t out;

    round_controller_init(&rc, 0, 0);
    /* seq=255 被接受 → WAIT_PLACE_CONFIRM */
    send_event(&rc, &out, 10, 255, ROUND_EVENT_APPLY_CONFIG);
    CHECK_EQ(out.event_ack_valid, 1);
    CHECK_EQ(out.event_ack_seq, 255);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);

    /* seq=0 回绕视为向前：PLACE_CONFIRM 接受 → ACQUIRE_STABLE */
    send_event(&rc, &out, 20, 0, ROUND_EVENT_PLACE_CONFIRM);
    CHECK_EQ(out.event_ack_valid, 1);
    CHECK_EQ(out.event_ack_seq, 0);
    CHECK_EQ(out.state, ROUND_STATE_ACQUIRE_STABLE);
    PASS();
}

static void test_seq_zero_to_255_rejected(void)
{
    TEST("round: seq 0->255 (delta=255) is rejected and not acked");
    round_controller_t rc;
    round_controller_output_t out;

    round_controller_init(&rc, 0, 0);
    /* seq=0 为首事件，无条件接受 → WAIT_PLACE_CONFIRM */
    send_event(&rc, &out, 10, 0, ROUND_EVENT_APPLY_CONFIG);
    CHECK_EQ(out.event_ack_valid, 1);
    CHECK_EQ(out.event_ack_seq, 0);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);

    /* seq=255 相对 0 为过期（delta=255，落在拒绝半区间），
     * PLACE_CONFIRM 必须被拒绝，状态不变 */
    send_event(&rc, &out, 20, 255, ROUND_EVENT_PLACE_CONFIRM);
    CHECK_EQ(out.event_ack_valid, 0);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);
    PASS();
}

static void test_seq_delta_128_rejected(void)
{
    TEST("round: seq delta=128 (10->138) is rejected and not acked");
    round_controller_t rc;
    round_controller_output_t out;

    round_controller_init(&rc, 0, 0);
    /* seq=10 接受 → WAIT_PLACE_CONFIRM */
    send_event(&rc, &out, 10, 10, ROUND_EVENT_APPLY_CONFIG);
    CHECK_EQ(out.event_ack_valid, 1);
    CHECK_EQ(out.event_ack_seq, 10);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);

    /* seq=138：delta=(uint8_t)(138-10)=128，正好落在拒绝半区间边界，
     * PLACE_CONFIRM 必须被拒绝，状态不变 */
    send_event(&rc, &out, 20, 138, ROUND_EVENT_PLACE_CONFIRM);
    CHECK_EQ(out.event_ack_valid, 0);
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

    printf("\n[audit] gap-fill: abandon / arm-motion lockout / arm-fault evidence\n");
    test_abandon_round_in_acquire_keeps_reason();
    test_arm_motion_blocks_soft_reset_and_abandon();
    test_arm_fault_input_keeps_evidence();
    test_arm_timeout_keeps_evidence();

    printf("\n[codex-fix] arm_busy safety gate / event_seq stale + wraparound\n");
    test_arm_busy_blocks_grab_and_completes_not_ready();
    test_stale_event_seq_rejected();
    test_event_seq_wraparound_accepted();
    test_seq_zero_to_255_rejected();
    test_seq_delta_128_rejected();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           _test_count - _test_failures, _test_count, _test_failures);
    return _test_failures ? 1 : 0;
}
