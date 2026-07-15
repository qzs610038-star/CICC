#include "single_camera_f1.h"

#include <string.h>

static int valid_color(sc_color_t color)
{
    return color >= SC_COLOR_WHITE && color <= SC_COLOR_YELLOW;
}

static int valid_target(const sc_target_t *target)
{
    if (target == 0) return 0;
    switch (target->task) {
    case SC_TASK_COLOR_CUBE:
    case SC_TASK_SHAPE_COLOR_CUBE:
        return valid_color(target->target_color);
    case SC_TASK_SIZE_DELTA_1CM_CUBE:
    case SC_TASK_SIZE_WITHIN_0P5CM_CUBE:
        return target->reference_size_cm_x10 == 20u ||
               target->reference_size_cm_x10 == 30u;
    default:
        return 0;
    }
}

static void clear_round(sc_f1_controller_t *controller)
{
    memset(&controller->observation, 0, sizeof(controller->observation));
    controller->decision = SC_DECISION_WAIT;
    controller->reason = SC_REASON_NONE;
    controller->result_valid = 0u;
}

void sc_f1_init(sc_f1_controller_t *controller, uint8_t size_available)
{
    if (controller == 0) return;
    memset(controller, 0, sizeof(*controller));
    controller->state = SC_STATE_WAIT_PLACE;
    controller->size_available = size_available ? 1u : 0u;
}

int sc_f1_apply_target(sc_f1_controller_t *controller, const sc_target_t *target)
{
    if (controller == 0 || !valid_target(target) ||
        controller->state == SC_STATE_ACQUIRING) {
        return -1;
    }
    controller->target = *target;
    controller->state = SC_STATE_WAIT_PLACE;
    clear_round(controller);
    return 0;
}

int sc_f1_place(sc_f1_controller_t *controller, uint16_t event_seq,
                uint32_t now_ms, uint32_t timeout_ms)
{
    if (controller == 0 || timeout_ms == 0u || !valid_target(&controller->target)) {
        return -1;
    }
    if (controller->place_seq == event_seq && controller->round_seq != 0u) {
        return 0; /* Debounced duplicate PLACE: never make a second round. */
    }
    if (controller->state == SC_STATE_ACQUIRING) {
        return -1;
    }

    clear_round(controller);
    controller->round_seq++;
    controller->place_seq = event_seq;
    controller->deadline_ms = now_ms + timeout_ms;
    controller->state = SC_STATE_ACQUIRING;
    return 0;
}

int sc_f1_observe(sc_f1_controller_t *controller,
                  const sc_observation_t *observation, uint32_t now_ms)
{
    uint8_t size_delta;

    if (controller == 0 || observation == 0 ||
        controller->state != SC_STATE_ACQUIRING) {
        return -1;
    }
    if ((int32_t)(now_ms - controller->deadline_ms) >= 0) {
        return sc_f1_tick(controller, now_ms);
    }
    if (!observation->stable || observation->color == SC_COLOR_UNKNOWN ||
        observation->shape == SC_SHAPE_UNKNOWN) {
        return 0;
    }

    controller->observation = *observation;
    if (observation->shape != SC_SHAPE_CUBE) {
        controller->decision = SC_DECISION_SKIP;
        controller->reason = SC_REASON_SHAPE_MISMATCH;
    } else if ((controller->target.task == SC_TASK_SIZE_DELTA_1CM_CUBE ||
                controller->target.task == SC_TASK_SIZE_WITHIN_0P5CM_CUBE) &&
               !controller->size_available) {
        controller->decision = SC_DECISION_WAIT;
        controller->reason = SC_REASON_SIZE_UNAVAILABLE;
        return 0;
    } else if ((controller->target.task == SC_TASK_COLOR_CUBE ||
                controller->target.task == SC_TASK_SHAPE_COLOR_CUBE) &&
               observation->color != controller->target.target_color) {
        controller->decision = SC_DECISION_SKIP;
        controller->reason = SC_REASON_COLOR_MISMATCH;
    } else if (controller->target.task == SC_TASK_SIZE_DELTA_1CM_CUBE ||
               controller->target.task == SC_TASK_SIZE_WITHIN_0P5CM_CUBE) {
        if (observation->size_cm_x10 == 0u) return 0;
        size_delta = observation->size_cm_x10 > controller->target.reference_size_cm_x10
                   ? (uint8_t)(observation->size_cm_x10 - controller->target.reference_size_cm_x10)
                   : (uint8_t)(controller->target.reference_size_cm_x10 - observation->size_cm_x10);
        if ((controller->target.task == SC_TASK_SIZE_DELTA_1CM_CUBE && size_delta != 10u) ||
            (controller->target.task == SC_TASK_SIZE_WITHIN_0P5CM_CUBE && size_delta > 5u)) {
            controller->decision = SC_DECISION_SKIP;
            controller->reason = SC_REASON_SIZE_RELATION_MISMATCH;
        } else {
            controller->decision = SC_DECISION_EXECUTE_ARM_DISABLED;
            controller->reason = SC_REASON_TARGET_MATCH;
        }
    } else {
        controller->decision = SC_DECISION_EXECUTE_ARM_DISABLED;
        controller->reason = SC_REASON_TARGET_MATCH;
    }

    controller->result_valid = 1u;
    controller->state = SC_STATE_RESULT_LATCHED;
    return 0;
}

int sc_f1_abandon(sc_f1_controller_t *controller, uint16_t event_seq)
{
    if (controller == 0 || controller->state != SC_STATE_ACQUIRING ||
        event_seq == controller->place_seq) {
        return -1;
    }
    controller->decision = SC_DECISION_SKIP;
    controller->reason = SC_REASON_OPERATOR_ABANDONED;
    controller->result_valid = 1u;
    controller->state = SC_STATE_ABANDONED;
    return 0;
}

int sc_f1_tick(sc_f1_controller_t *controller, uint32_t now_ms)
{
    if (controller == 0) return -1;
    if (controller->state == SC_STATE_ACQUIRING &&
        (int32_t)(now_ms - controller->deadline_ms) >= 0) {
        controller->decision = SC_DECISION_WAIT;
        controller->reason = SC_REASON_TIMEOUT;
        controller->result_valid = 1u;
        controller->state = SC_STATE_TIMEOUT;
        return 1;
    }
    return 0;
}

const char *sc_f1_reason_text(sc_reason_t reason)
{
    switch (reason) {
    case SC_REASON_TARGET_MATCH: return "TARGET_MATCH_ARM_DISABLED";
    case SC_REASON_COLOR_MISMATCH: return "COLOR_MISMATCH_SKIP";
    case SC_REASON_SHAPE_MISMATCH: return "SHAPE_MISMATCH_SKIP";
    case SC_REASON_SIZE_RELATION_MISMATCH: return "SIZE_RELATION_MISMATCH_SKIP";
    case SC_REASON_OBSERVATION_UNSTABLE: return "OBSERVATION_UNSTABLE";
    case SC_REASON_SIZE_UNAVAILABLE: return "SIZE_UNAVAILABLE";
    case SC_REASON_OPERATOR_ABANDONED: return "OPERATOR_ABANDONED";
    case SC_REASON_TIMEOUT: return "ROUND_TIMEOUT";
    case SC_REASON_INVALID_TARGET: return "INVALID_TARGET";
    default: return "NONE";
    }
}
