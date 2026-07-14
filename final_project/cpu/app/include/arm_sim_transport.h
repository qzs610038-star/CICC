#ifndef ARM_SIM_TRANSPORT_H
#define ARM_SIM_TRANSPORT_H

#include <stdint.h>

#include "arm_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ARM_SIM_SCENARIO_HAPPY = 0,
    ARM_SIM_SCENARIO_READ_FAILURE,
    ARM_SIM_SCENARIO_SOFT_PASS,
    ARM_SIM_SCENARIO_RETRY_SUCCESS,
    ARM_SIM_SCENARIO_RETRY_FAILURE
} arm_sim_scenario_t;

/* This transport is intentionally memory-only.  It owns no MMIO address,
 * UART register, clock divider, pin, or protocol frame. */
typedef struct {
    int16_t target[ARM_CONTROLLER_JOINTS];
    int16_t current[ARM_CONTROLLER_JOINTS];
    uint16_t gripper;
    uint16_t last_speed;
    uint16_t retry_speed;
    uint16_t send_count;
    uint16_t read_count;
    uint16_t gripper_count;
    uint8_t read_failures_remaining;
    uint8_t read_fail_always;
    uint8_t mismatch_always;
    uint8_t mismatch_first_send_only;
    uint8_t mismatch_active;
    uint8_t settle_reads_remaining;
} arm_sim_transport_t;

void arm_sim_transport_init(arm_sim_transport_t *sim);
void arm_sim_transport_set_scenario(arm_sim_transport_t *sim,
                                    arm_sim_scenario_t scenario);
void arm_sim_transport_set_settle_reads(arm_sim_transport_t *sim,
                                        uint8_t reads);
const arm_controller_ops_t *arm_sim_transport_ops(void);

#ifdef __cplusplus
}
#endif

#endif /* ARM_SIM_TRANSPORT_H */
