#ifndef ROUND_CONTROLLER_H
#define ROUND_CONTROLLER_H

#include <stdint.h>
#include "task_matcher.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ROUND_STATE_CONFIG = 0,
    ROUND_STATE_WAIT_PLACE_CONFIRM,
    ROUND_STATE_ACQUIRE_STABLE,
    ROUND_STATE_LATCH_RECOGNITION,
    ROUND_STATE_LATCH_DECISION,
    ROUND_STATE_EXECUTE_OR_SKIP,
    ROUND_STATE_WAIT_ARM_DONE,
    ROUND_STATE_ROUND_DONE,
    ROUND_STATE_WAIT_REMOVE_CONFIRM,
    ROUND_STATE_ARM_FAULT
} round_controller_state_t;

typedef enum {
    ROUND_EVENT_NONE = 0,
    ROUND_EVENT_APPLY_CONFIG,
    ROUND_EVENT_PLACE_CONFIRM,
    ROUND_EVENT_REMOVE_CONFIRM,
    ROUND_EVENT_ABANDON_ROUND,
    ROUND_EVENT_SOFT_RESET_ROUND,
    ROUND_EVENT_SESSION_RESET
} round_event_t;

typedef enum {
    ROUND_EVENT_ACK_NONE = 0,
    ROUND_EVENT_ACK_ACCEPTED,
    ROUND_EVENT_ACK_REJECTED
} round_event_ack_status_t;

typedef struct {
    uint32_t acquire_timeout_ms;
    uint32_t arm_timeout_ms;
} round_controller_config_t;

typedef struct {
    uint32_t now_ms;
    uint8_t event_valid;
    uint16_t event_seq;
    round_event_t event;
    uint8_t observation_valid;
    task_match_result_t match;
    uint8_t arm_enabled;
    uint8_t arm_busy;
    uint8_t arm_done;
    uint8_t arm_fault;
} round_controller_input_t;

typedef struct {
    round_controller_state_t state;
    uint16_t round_seq;
    uint8_t event_ack_valid;
    uint16_t event_ack_seq;
    round_event_ack_status_t event_ack_status;
    uint8_t request_arm_grab;
    uint8_t result_valid;
    uint8_t decision_action;
    uint8_t is_target;
    reason_code_t reason;
} round_controller_output_t;

typedef struct {
    round_controller_state_t state;
    round_controller_config_t cfg;
    uint16_t round_seq;
    uint8_t have_event_seq;
    uint16_t last_event_seq;
    uint8_t result_valid;
    uint8_t decision_action;
    uint8_t is_target;
    reason_code_t reason;
    uint8_t arm_request_sent;
    uint32_t state_enter_ms;
    uint32_t acquire_deadline_ms;
    uint32_t arm_deadline_ms;
} round_controller_t;

void round_controller_init(round_controller_t *rc,
                           const round_controller_config_t *cfg,
                           uint32_t now_ms);
void round_controller_tick(round_controller_t *rc,
                           const round_controller_input_t *in,
                           round_controller_output_t *out);
round_controller_state_t round_controller_get_state(const round_controller_t *rc);

#ifdef __cplusplus
}
#endif

#endif /* ROUND_CONTROLLER_H */
