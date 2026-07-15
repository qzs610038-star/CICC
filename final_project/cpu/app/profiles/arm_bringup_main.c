#include "arm_build_profile.h"
#include "arm_runtime.h"
#include "bsp.h"
#include "round_controller.h"

#include <stdint.h>
#include <string.h>

#if APP_PROFILE != ARM_PROFILE_ARM_BRINGUP
#error "arm_bringup_main.c is only valid for APP_PROFILE=arm_bringup"
#endif

#if ARM_BACKEND == ARM_BACKEND_SIMULATED
#include "arm_positions.h"
#endif

static void uart_puts(const char *text)
{
    while (*text) {
        bsp_putChar(*text++);
    }
}

static void uart_put_dec(uint32_t value)
{
    char digits[12];
    uint8_t count = 0u;

    if (value == 0u) {
        bsp_putChar('0');
        return;
    }
    while (value != 0u && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (count != 0u) {
        bsp_putChar(digits[--count]);
    }
}

#if ARM_BACKEND == ARM_BACKEND_SIMULATED
static void log_case(const char *name, const arm_runtime_status_t *status,
                     uint16_t sends, uint16_t reads, uint16_t gripper)
{
    uart_puts("[ARM_BRINGUP] case=");
    uart_puts(name);
    uart_puts(" done=");
    uart_put_dec(status->arm_done);
    uart_puts(" fault=");
    uart_put_dec(status->arm_fault);
    uart_puts(" error=");
    uart_put_dec((uint32_t)status->error);
    uart_puts(" sends=");
    uart_put_dec(sends);
    uart_puts(" reads=");
    uart_put_dec(reads);
    uart_puts(" gripper=");
    uart_put_dec(gripper);
    uart_puts("\r\n");
}
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

/* Fixed interleaving prevents a one-sided 7-then-13 sequence from masking
 * state carry-over bugs.  It contains exactly seven target rounds. */
static const uint8_t g_twenty_round_target_pattern[20] = {
    1u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 0u, 0u,
    1u, 0u, 0u, 1u, 0u, 0u, 0u, 1u, 0u, 0u
};

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

static int run_case(const char *name, arm_sim_scenario_t scenario,
                    uint8_t expected_done, arm_error_t expected_error)
{
    arm_controller_plan_t plan = make_fast_plan();
    arm_runtime_t runtime;
    arm_runtime_status_t status;
    uint16_t sends;
    uint16_t reads;
    uint16_t gripper;
    uint16_t retry_speed;
    uint32_t now_ms;

    arm_runtime_init(&runtime, &plan);
    if (arm_runtime_set_sim_scenario(&runtime, scenario) != 0 ||
        arm_runtime_accept_request(&runtime) != 0) {
        uart_puts("[ARM_BRINGUP] case start FAIL\r\n");
        return -1;
    }

    memset(&status, 0, sizeof(status));
    for (now_ms = 0u; now_ms <= 10000u; now_ms += 50u) {
        arm_runtime_tick(&runtime, now_ms);
        arm_runtime_get_status(&runtime, &status);
        if (status.arm_done || status.arm_fault) {
            break;
        }
    }
    arm_runtime_get_sim_counters(&runtime, &sends, &reads, &gripper,
                                 &retry_speed);
    log_case(name, &status, sends, reads, gripper);
    if (status.arm_done != expected_done || status.error != expected_error ||
        sends == 0u || reads == 0u) {
        return -1;
    }
    if (scenario == ARM_SIM_SCENARIO_RETRY_SUCCESS &&
        retry_speed != plan.refine_speed) {
        return -1;
    }
    return 0;
}

static int run_twenty_rounds(void)
{
    arm_controller_plan_t plan = make_fast_plan();
    arm_runtime_t runtime;
    round_controller_t round;
    round_controller_output_t out;
    task_match_result_t target;
    task_match_result_t skip;
    uint32_t now_ms = 0u;
    uint16_t event_seq = 0u;
    uint16_t i;
    uint16_t requests = 0u;

    arm_runtime_init(&runtime, &plan);
    if (arm_runtime_set_sim_scenario(&runtime, ARM_SIM_SCENARIO_HAPPY) != 0) {
        return -1;
    }
    round_controller_init(&round, 0, now_ms);
    memset(&target, 0, sizeof(target));
    target.action = MATCH_ACTION_GRAB;
    target.is_target = 1u;
    target.reason = REASON_TARGET_MATCH;
    memset(&skip, 0, sizeof(skip));
    skip.action = MATCH_ACTION_SKIP;
    skip.reason = REASON_COLOR_MISMATCH;

    for (i = 0u; i < 20u; ++i) {
        round_controller_input_t in;
        uint32_t guard = 0u;
        uint8_t is_target = g_twenty_round_target_pattern[i];

        memset(&in, 0, sizeof(in));
        in.now_ms = now_ms;
        in.event_valid = 1u;
        in.event_seq = ++event_seq;
        in.event = (i == 0u) ? ROUND_EVENT_APPLY_CONFIG :
                               ROUND_EVENT_PLACE_CONFIRM;
        fill_arm_fields(&in, &runtime);
        round_controller_tick(&round, &in, &out);
        now_ms += 50u;
        if (i == 0u) {
            memset(&in, 0, sizeof(in));
            in.now_ms = now_ms;
            in.event_valid = 1u;
            in.event_seq = ++event_seq;
            in.event = ROUND_EVENT_PLACE_CONFIRM;
            fill_arm_fields(&in, &runtime);
            round_controller_tick(&round, &in, &out);
            now_ms += 50u;
        }

        memset(&in, 0, sizeof(in));
        in.now_ms = now_ms;
        in.observation_valid = 1u;
        in.match = is_target ? target : skip;
        fill_arm_fields(&in, &runtime);
        round_controller_tick(&round, &in, &out);
        now_ms += 50u;
        while (out.state != ROUND_STATE_ROUND_DONE && guard++ < 400u) {
            arm_runtime_tick(&runtime, now_ms);
            memset(&in, 0, sizeof(in));
            in.now_ms = now_ms;
            fill_arm_fields(&in, &runtime);
            round_controller_tick(&round, &in, &out);
            if (out.request_arm_grab) {
                if (arm_runtime_accept_request(&runtime) != 0) {
                    return -1;
                }
                requests++;
            }
            now_ms += 50u;
        }
        if (out.state != ROUND_STATE_ROUND_DONE) {
            return -1;
        }
        memset(&in, 0, sizeof(in));
        in.now_ms = now_ms;
        in.event_valid = 1u;
        in.event_seq = ++event_seq;
        in.event = ROUND_EVENT_REMOVE_CONFIRM;
        fill_arm_fields(&in, &runtime);
        round_controller_tick(&round, &in, &out);
        now_ms += 50u;
    }

    uart_puts("[ARM_BRINGUP] twenty_rounds targets=7 requests=");
    uart_put_dec(requests);
    uart_puts(" result=");
    uart_puts(requests == 7u ? "PASS\r\n" : "FAIL\r\n");
    return requests == 7u ? 0 : -1;
}
#endif

#if ARM_BACKEND == ARM_BACKEND_DISABLED
/* This G2 self-check uses a deterministic, interleaved 20-round test clock.
 * It is deliberately isolated from the production loop, whose monotonic board
 * clock is not yet a verified G0-G3 input.  It creates no transport and must
 * never request or accept an arm action. */
static int run_disabled_twenty_rounds(void)
{
    arm_runtime_t runtime;
    arm_runtime_status_t status;
    round_controller_t round;
    round_controller_input_t in;
    round_controller_output_t out;
    task_match_result_t target;
    task_match_result_t skip;
    uint32_t test_now_ms = 0u;
    uint16_t event_seq = 0u;
    uint16_t i;

    arm_runtime_init(&runtime, 0);
    round_controller_init(&round, 0, test_now_ms);
    memset(&target, 0, sizeof(target));
    target.action = MATCH_ACTION_GRAB;
    target.is_target = 1u;
    target.reason = REASON_TARGET_MATCH;
    memset(&skip, 0, sizeof(skip));
    skip.action = MATCH_ACTION_SKIP;
    skip.reason = REASON_COLOR_MISMATCH;

    memset(&in, 0, sizeof(in));
    in.now_ms = test_now_ms;
    in.event_valid = 1u;
    in.event_seq = ++event_seq;
    in.event = ROUND_EVENT_APPLY_CONFIG;
    fill_arm_fields(&in, &runtime);
    round_controller_tick(&round, &in, &out);
    if (out.state != ROUND_STATE_WAIT_PLACE_CONFIRM) {
        return -1;
    }

    for (i = 0u; i < 20u; ++i) {
        uint32_t guard = 0u;

        ++test_now_ms;
        memset(&in, 0, sizeof(in));
        in.now_ms = test_now_ms;
        in.event_valid = 1u;
        in.event_seq = ++event_seq;
        in.event = ROUND_EVENT_PLACE_CONFIRM;
        fill_arm_fields(&in, &runtime);
        round_controller_tick(&round, &in, &out);
        if (out.state != ROUND_STATE_ACQUIRE_STABLE) {
            return -1;
        }

        ++test_now_ms;
        memset(&in, 0, sizeof(in));
        in.now_ms = test_now_ms;
        in.observation_valid = 1u;
        in.match = g_twenty_round_target_pattern[i] ? target : skip;
        fill_arm_fields(&in, &runtime);
        round_controller_tick(&round, &in, &out);

        while (out.state != ROUND_STATE_ROUND_DONE && guard++ < 8u) {
            ++test_now_ms;
            arm_runtime_tick(&runtime, test_now_ms);
            memset(&in, 0, sizeof(in));
            in.now_ms = test_now_ms;
            fill_arm_fields(&in, &runtime);
            round_controller_tick(&round, &in, &out);
        }
        if (out.state != ROUND_STATE_ROUND_DONE || out.request_arm_grab != 0u) {
            return -1;
        }
        if (g_twenty_round_target_pattern[i]) {
            if (out.reason != REASON_ARM_NOT_READY ||
                out.decision_action != MATCH_ACTION_NONE) {
                return -1;
            }
        } else if (out.reason != REASON_COLOR_MISMATCH ||
                   out.decision_action != MATCH_ACTION_SKIP) {
            return -1;
        }

        ++test_now_ms;
        memset(&in, 0, sizeof(in));
        in.now_ms = test_now_ms;
        in.event_valid = 1u;
        in.event_seq = ++event_seq;
        in.event = ROUND_EVENT_REMOVE_CONFIRM;
        fill_arm_fields(&in, &runtime);
        round_controller_tick(&round, &in, &out);
        if (out.state != ROUND_STATE_WAIT_PLACE_CONFIRM) {
            return -1;
        }
    }

    arm_runtime_get_status(&runtime, &status);
    uart_puts("[ARM_BRINGUP] disabled_twenty_rounds accepted=");
    uart_put_dec(status.accepted_requests);
    uart_puts(" rejected=");
    uart_put_dec(status.rejected_requests);
    uart_puts(" result=");
    uart_puts((status.accepted_requests == 0u &&
               status.rejected_requests == 0u) ? "PASS\r\n" : "FAIL\r\n");
    return (status.accepted_requests == 0u &&
            status.rejected_requests == 0u) ? 0 : -1;
}
#endif

int main(void)
{
    int failed = 0;

    bsp_init();
    uart_puts("\r\n[ARM_BRINGUP] CPU HELLO build=");
    uart_puts(ARM_BUILD_ID);
    uart_puts(" profile=arm_bringup backend=");
#if ARM_BACKEND == ARM_BACKEND_SIMULATED
    uart_puts("simulated NOT_FOR_FLASH\r\n");
    failed |= run_case("happy", ARM_SIM_SCENARIO_HAPPY, 1u, ARM_ERR_NONE);
    failed |= run_case("read_failure", ARM_SIM_SCENARIO_READ_FAILURE, 0u,
                       ARM_ERR_POST_READ_FAILED);
    failed |= run_case("soft_pass", ARM_SIM_SCENARIO_SOFT_PASS, 1u,
                       ARM_ERR_SOFT_PASS_WARNING);
    failed |= run_case("retry_success", ARM_SIM_SCENARIO_RETRY_SUCCESS, 1u,
                       ARM_ERR_NONE);
    failed |= run_case("retry_failure", ARM_SIM_SCENARIO_RETRY_FAILURE, 0u,
                       ARM_ERR_RETRY_FAILED);
    failed |= run_twenty_rounds();
#else
    arm_runtime_t runtime;
    arm_runtime_status_t status;
    arm_runtime_init(&runtime, 0);
    arm_runtime_get_status(&runtime, &status);
    uart_puts("disabled NOT_FOR_FLASH state=");
    uart_put_dec((uint32_t)status.apb_state);
    uart_puts("\r\n");
    failed |= run_disabled_twenty_rounds();
#endif
    uart_puts(failed == 0 ? "[ARM_BRINGUP] RESULT PASS\r\n" :
                             "[ARM_BRINGUP] RESULT FAIL\r\n");
    for (;;) {
    }
}
