#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "arm_build_profile.h"
#include "arm_runtime.h"
#include "round_controller.h"

#if ARM_BACKEND == ARM_BACKEND_SIMULATED
#include "arm_positions.h"
#endif

static void fill_arm_fields(round_controller_input_t *in,
                            const arm_runtime_t *runtime)
{
    arm_runtime_status_t status;

    memset(&status, 0, sizeof(status));
    arm_runtime_get_status(runtime, &status);
    in->arm_enabled = status.arm_enabled;
    in->arm_busy = status.arm_busy;
    in->arm_done = status.arm_done;
    in->arm_fault = status.arm_fault;
}

static void step_round(round_controller_t *round,
                       arm_runtime_t *runtime,
                       uint32_t *now_ms,
                       uint8_t event_valid,
                       uint16_t event_seq,
                       round_event_t event,
                       uint8_t observation_valid,
                       const task_match_result_t *match,
                       round_controller_output_t *out)
{
    round_controller_input_t in;

    arm_runtime_tick(runtime, *now_ms);
    memset(&in, 0, sizeof(in));
    in.now_ms = *now_ms;
    in.event_valid = event_valid;
    in.event_seq = event_seq;
    in.event = event;
    in.observation_valid = observation_valid;
    if (match) {
        in.match = *match;
    }
    fill_arm_fields(&in, runtime);
    round_controller_tick(round, &in, out);
    if (out->request_arm_grab) {
        assert(arm_runtime_accept_request(runtime) == 0);
    }
    *now_ms += 50u;
}

static void send_event(round_controller_t *round,
                       arm_runtime_t *runtime,
                       uint32_t *now_ms,
                       uint16_t *event_seq,
                       round_event_t event,
                       round_controller_output_t *out)
{
    *event_seq = (uint16_t)(*event_seq + 1u);
    step_round(round, runtime, now_ms, 1u, *event_seq, event,
               0u, 0, out);
    assert(out->event_ack_valid == 1u);
    assert(out->event_ack_status == ROUND_EVENT_ACK_ACCEPTED);
}

static void run_twenty_round_session(arm_runtime_t *runtime,
                                      uint16_t expected_requests)
{
    static const uint8_t target_pattern[20] = {
        1u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 0u, 0u,
        1u, 0u, 0u, 1u, 0u, 0u, 0u, 1u, 0u, 0u
    };
    round_controller_t round;
    round_controller_output_t out;
    task_match_result_t target_match;
    task_match_result_t skip_match;
    uint32_t now_ms = 0u;
    uint16_t event_seq = 0u;
    uint16_t target_count = 0u;
    uint16_t i;

    memset(&target_match, 0, sizeof(target_match));
    target_match.action = MATCH_ACTION_GRAB;
    target_match.is_target = 1u;
    target_match.reason = REASON_TARGET_MATCH;
    memset(&skip_match, 0, sizeof(skip_match));
    skip_match.action = MATCH_ACTION_SKIP;
    skip_match.is_target = 0u;
    skip_match.reason = REASON_COLOR_MISMATCH;

    round_controller_init(&round, 0, now_ms);
    send_event(&round, runtime, &now_ms, &event_seq,
               ROUND_EVENT_APPLY_CONFIG, &out);

    for (i = 0u; i < 20u; ++i) {
        uint32_t guard = 0u;
        uint8_t is_target = target_pattern[i];

        send_event(&round, runtime, &now_ms, &event_seq,
                   ROUND_EVENT_PLACE_CONFIRM, &out);
        step_round(&round, runtime, &now_ms, 0u, 0u, ROUND_EVENT_NONE,
                   1u, is_target ? &target_match : &skip_match, &out);
        step_round(&round, runtime, &now_ms, 0u, 0u, ROUND_EVENT_NONE,
                   0u, 0, &out);
        step_round(&round, runtime, &now_ms, 0u, 0u, ROUND_EVENT_NONE,
                   0u, 0, &out);
        step_round(&round, runtime, &now_ms, 0u, 0u, ROUND_EVENT_NONE,
                   0u, 0, &out);

        if (is_target) {
            target_count++;
            while (out.state != ROUND_STATE_ROUND_DONE && guard++ < 400u) {
                step_round(&round, runtime, &now_ms, 0u, 0u,
                           ROUND_EVENT_NONE, 0u, 0, &out);
            }
            assert(out.state == ROUND_STATE_ROUND_DONE);
#if ARM_BACKEND == ARM_BACKEND_DISABLED
            assert(out.reason == REASON_ARM_NOT_READY);
            assert(out.request_arm_grab == 0u);
#endif
        } else {
            assert(out.state == ROUND_STATE_ROUND_DONE);
            assert(out.reason == REASON_COLOR_MISMATCH);
            assert(out.request_arm_grab == 0u);
        }

        send_event(&round, runtime, &now_ms, &event_seq,
                   ROUND_EVENT_REMOVE_CONFIRM, &out);
        assert(out.state == ROUND_STATE_WAIT_PLACE_CONFIRM);
    }

    assert(target_count == 7u);
    assert(runtime->status.accepted_requests == expected_requests);
}

#if ARM_BACKEND == ARM_BACKEND_DISABLED
static void test_disabled_runtime_is_non_actuating(void)
{
    arm_runtime_t runtime;
    arm_runtime_status_t status;

    arm_runtime_init(&runtime, 0);
    arm_runtime_get_status(&runtime, &status);
    assert(status.arm_enabled == 0u);
    assert(status.apb_state == ARM_RUNTIME_APB_STATE_DISABLED);
    assert(arm_runtime_accept_request(&runtime) != 0);
    assert(runtime.status.accepted_requests == 0u);
    assert(runtime.status.rejected_requests == 1u);
    run_twenty_round_session(&runtime, 0u);
}
#endif

#if ARM_BACKEND == ARM_BACKEND_SIMULATED
static arm_controller_plan_t make_fast_plan(void)
{
    arm_controller_plan_t plan = g_arm_default_plan;
    plan.move_timeout_ms = 120u;
    plan.home_timeout_ms = 120u;
    plan.poll_interval_ms = 50u;
    plan.confirm_required = 2u;
    return plan;
}

static arm_runtime_status_t run_to_terminal(arm_runtime_t *runtime)
{
    arm_runtime_status_t status;
    uint32_t now_ms;

    memset(&status, 0, sizeof(status));
    for (now_ms = 0u; now_ms <= 10000u; now_ms += 50u) {
        arm_runtime_tick(runtime, now_ms);
        arm_runtime_get_status(runtime, &status);
        if (status.arm_done || status.arm_fault) {
            break;
        }
    }
    return status;
}

static void test_simulated_cases(void)
{
    const arm_sim_scenario_t scenarios[] = {
        ARM_SIM_SCENARIO_HAPPY,
        ARM_SIM_SCENARIO_READ_FAILURE,
        ARM_SIM_SCENARIO_SOFT_PASS,
        ARM_SIM_SCENARIO_RETRY_SUCCESS,
        ARM_SIM_SCENARIO_RETRY_FAILURE
    };
    const arm_error_t expected_errors[] = {
        ARM_ERR_NONE,
        ARM_ERR_POST_READ_FAILED,
        ARM_ERR_SOFT_PASS_WARNING,
        ARM_ERR_NONE,
        ARM_ERR_RETRY_FAILED
    };
    const uint8_t expect_done[] = {1u, 0u, 1u, 1u, 0u};
    arm_controller_plan_t plan = make_fast_plan();
    uint8_t i;

    for (i = 0u; i < (uint8_t)(sizeof(scenarios) / sizeof(scenarios[0])); ++i) {
        arm_runtime_t runtime;
        arm_runtime_status_t status;
        uint16_t send_count;
        uint16_t read_count;
        uint16_t gripper_count;
        uint16_t retry_speed;

        arm_runtime_init(&runtime, &plan);
        assert(arm_runtime_set_sim_scenario(&runtime, scenarios[i]) == 0);
        assert(arm_runtime_accept_request(&runtime) == 0);
        status = run_to_terminal(&runtime);
        assert(status.arm_done == expect_done[i]);
        assert(status.arm_fault == (uint8_t)(expect_done[i] ? 0u : 1u));
        assert(status.error == expected_errors[i]);
        arm_runtime_get_sim_counters(&runtime, &send_count, &read_count,
                                     &gripper_count, &retry_speed);
        assert(send_count > 0u);
        assert(read_count > 0u);
        if (scenarios[i] == ARM_SIM_SCENARIO_HAPPY) {
            assert(gripper_count == 2u);
        }
        if (scenarios[i] == ARM_SIM_SCENARIO_RETRY_SUCCESS) {
            assert(retry_speed == plan.refine_speed);
        }
    }
}

static void test_simulated_busy_and_twenty_rounds(void)
{
    arm_controller_plan_t plan = make_fast_plan();
    arm_runtime_t runtime;

    arm_runtime_init(&runtime, &plan);
    assert(arm_runtime_accept_request(&runtime) == 0);
    assert(arm_runtime_accept_request(&runtime) != 0);
    assert(runtime.status.accepted_requests == 1u);
    assert(runtime.status.rejected_requests == 1u);

    arm_runtime_init(&runtime, &plan);
    assert(arm_runtime_set_sim_scenario(&runtime, ARM_SIM_SCENARIO_HAPPY) == 0);
    run_twenty_round_session(&runtime, 7u);
}
#endif

int main(void)
{
#if ARM_BACKEND == ARM_BACKEND_DISABLED
    test_disabled_runtime_is_non_actuating();
#elif ARM_BACKEND == ARM_BACKEND_SIMULATED
    test_simulated_cases();
    test_simulated_busy_and_twenty_rounds();
#else
#error "test_arm_runtime only supports G0-G3 backends"
#endif
    return 0;
}
