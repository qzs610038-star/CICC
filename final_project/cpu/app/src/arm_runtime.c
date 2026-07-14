#include "arm_runtime.h"

#include <string.h>

#if ARM_BACKEND == ARM_BACKEND_SIMULATED
#include "arm_positions.h"
#endif

#if ARM_BACKEND != ARM_BACKEND_SIMULATED
static void set_disabled_status(arm_runtime_t *runtime)
{
    runtime->status.arm_enabled = 0u;
    runtime->status.arm_busy = 0u;
    runtime->status.arm_done = 0u;
    runtime->status.arm_fault = 0u;
    runtime->status.apb_state = ARM_RUNTIME_APB_STATE_DISABLED;
    runtime->status.error = ARM_ERR_NONE;
}
#endif

#if ARM_BACKEND == ARM_BACKEND_SIMULATED
static void refresh_simulated_status(arm_runtime_t *runtime)
{
    arm_state_t state = arm_controller_get_state(&runtime->controller);

    runtime->status.arm_enabled = 1u;
    runtime->status.error = arm_controller_get_error(&runtime->controller);
    runtime->status.arm_done = (state == ARM_STATE_DONE) ? 1u : 0u;
    runtime->status.arm_fault =
        (state == ARM_STATE_FAULT || state == ARM_STATE_ESTOP) ? 1u : 0u;
    runtime->status.arm_busy =
        (state != ARM_STATE_IDLE && state != ARM_STATE_DONE &&
         state != ARM_STATE_FAULT && state != ARM_STATE_ESTOP) ? 1u : 0u;

    if (runtime->status.arm_fault) {
        runtime->status.apb_state = ARM_RUNTIME_APB_STATE_FAULT;
    } else if (runtime->status.arm_done) {
        runtime->status.apb_state = ARM_RUNTIME_APB_STATE_DONE;
    } else if (runtime->status.arm_busy) {
        runtime->status.apb_state = ARM_RUNTIME_APB_STATE_BUSY;
    } else {
        runtime->status.apb_state = ARM_RUNTIME_APB_STATE_READY;
    }
}
#endif

void arm_runtime_init(arm_runtime_t *runtime,
                      const arm_controller_plan_t *plan)
{
    if (!runtime) {
        return;
    }

    memset(runtime, 0, sizeof(*runtime));
#if ARM_BACKEND == ARM_BACKEND_SIMULATED
    arm_sim_transport_init(&runtime->sim);
    runtime->plan = plan ? plan : &g_arm_default_plan;
    arm_controller_init(&runtime->controller, 1u);
    arm_controller_configure(&runtime->controller, runtime->plan,
                             arm_sim_transport_ops(), &runtime->sim);
    refresh_simulated_status(runtime);
#else
    (void)plan;
    set_disabled_status(runtime);
#endif
}

void arm_runtime_tick(arm_runtime_t *runtime, uint32_t now_ms)
{
    if (!runtime) {
        return;
    }
#if ARM_BACKEND == ARM_BACKEND_SIMULATED
    arm_controller_tick(&runtime->controller, now_ms);
    refresh_simulated_status(runtime);
#else
    (void)now_ms;
    set_disabled_status(runtime);
#endif
}

int arm_runtime_accept_request(arm_runtime_t *runtime)
{
#if ARM_BACKEND == ARM_BACKEND_SIMULATED
    int rc;
#endif

    if (!runtime) {
        return -1;
    }
#if ARM_BACKEND == ARM_BACKEND_SIMULATED
    refresh_simulated_status(runtime);
    if (runtime->status.arm_busy || runtime->status.arm_fault) {
        runtime->status.rejected_requests++;
        return -1;
    }
    rc = arm_controller_request_grab(&runtime->controller);
    if (rc == 0) {
        runtime->status.accepted_requests++;
    } else {
        runtime->status.rejected_requests++;
    }
    refresh_simulated_status(runtime);
    return rc;
#else
    runtime->status.rejected_requests++;
    set_disabled_status(runtime);
    return -1;
#endif
}

void arm_runtime_cancel(arm_runtime_t *runtime, arm_error_t reason)
{
    if (!runtime) {
        return;
    }
#if ARM_BACKEND == ARM_BACKEND_SIMULATED
    arm_controller_cancel(&runtime->controller, reason);
    refresh_simulated_status(runtime);
#else
    (void)reason;
    set_disabled_status(runtime);
#endif
}

void arm_runtime_get_status(const arm_runtime_t *runtime,
                            arm_runtime_status_t *status_out)
{
    if (runtime && status_out) {
        *status_out = runtime->status;
    }
}

int arm_runtime_set_sim_scenario(arm_runtime_t *runtime,
                                 arm_sim_scenario_t scenario)
{
    if (!runtime) {
        return -1;
    }
#if ARM_BACKEND == ARM_BACKEND_SIMULATED
    arm_sim_transport_set_scenario(&runtime->sim, scenario);
    arm_controller_init(&runtime->controller, 1u);
    arm_controller_configure(&runtime->controller,
                             runtime->plan ? runtime->plan : &g_arm_default_plan,
                             arm_sim_transport_ops(), &runtime->sim);
    refresh_simulated_status(runtime);
    return 0;
#else
    (void)scenario;
    return -1;
#endif
}

void arm_runtime_get_sim_counters(const arm_runtime_t *runtime,
                                  uint16_t *send_count,
                                  uint16_t *read_count,
                                  uint16_t *gripper_count,
                                  uint16_t *retry_speed)
{
    if (send_count) {
        *send_count = runtime ? runtime->sim.send_count : 0u;
    }
    if (read_count) {
        *read_count = runtime ? runtime->sim.read_count : 0u;
    }
    if (gripper_count) {
        *gripper_count = runtime ? runtime->sim.gripper_count : 0u;
    }
    if (retry_speed) {
        *retry_speed = runtime ? runtime->sim.retry_speed : 0u;
    }
}
