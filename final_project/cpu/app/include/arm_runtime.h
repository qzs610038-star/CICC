#ifndef ARM_RUNTIME_H
#define ARM_RUNTIME_H

#include <stdint.h>

#include "arm_build_profile.h"
#include "arm_controller.h"
#include "arm_sim_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* These are APB/OSD-facing semantic values, not arm_controller_t enum
 * values.  Their explicit mapping prevents the old main.c IDLE=1 versus
 * arm_controller.h IDLE=0 collision from leaking into the integration. */
typedef enum {
    ARM_RUNTIME_APB_STATE_UNKNOWN = 0,
    ARM_RUNTIME_APB_STATE_DISABLED = 1,
    ARM_RUNTIME_APB_STATE_READY = 2,
    ARM_RUNTIME_APB_STATE_BUSY = 3,
    ARM_RUNTIME_APB_STATE_DONE = 4,
    ARM_RUNTIME_APB_STATE_FAULT = 9
} arm_runtime_apb_state_t;

typedef struct {
    uint8_t arm_enabled;
    uint8_t arm_busy;
    uint8_t arm_done;
    uint8_t arm_fault;
    arm_runtime_apb_state_t apb_state;
    arm_error_t error;
    uint16_t accepted_requests;
    uint16_t rejected_requests;
} arm_runtime_status_t;

typedef struct {
    arm_controller_t controller;
    arm_sim_transport_t sim;
    const arm_controller_plan_t *plan;
    arm_runtime_status_t status;
} arm_runtime_t;

void arm_runtime_init(arm_runtime_t *runtime,
                      const arm_controller_plan_t *plan);
void arm_runtime_tick(arm_runtime_t *runtime, uint32_t now_ms);
int arm_runtime_accept_request(arm_runtime_t *runtime);
void arm_runtime_cancel(arm_runtime_t *runtime, arm_error_t reason);
void arm_runtime_get_status(const arm_runtime_t *runtime,
                            arm_runtime_status_t *status_out);
int arm_runtime_set_sim_scenario(arm_runtime_t *runtime,
                                 arm_sim_scenario_t scenario);
void arm_runtime_get_sim_counters(const arm_runtime_t *runtime,
                                  uint16_t *send_count,
                                  uint16_t *read_count,
                                  uint16_t *gripper_count,
                                  uint16_t *retry_speed);

#ifdef __cplusplus
}
#endif

#endif /* ARM_RUNTIME_H */
