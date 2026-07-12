#include "competition_tasks.h"

static int is_valid_color(uint8_t color)
{
    return color >= COLOR_WHITE && color <= COLOR_YELLOW;
}

static int is_reference_size(uint8_t size_cm_x10)
{
    return size_cm_x10 == 20u || size_cm_x10 == 30u;
}

int competition_target_validate(const competition_target_t *target)
{
    if (target == 0) {
        return -1;
    }

    switch (target->mode) {
    case COMP_TASK_COLOR_CUBE:
    case COMP_TASK_SHAPE_COLOR_CUBE:
        return is_valid_color(target->target_color) ? 0 : -1;
    case COMP_TASK_SIZE_DELTA_1CM_CUBE:
    case COMP_TASK_SIZE_WITHIN_0P5CM_CUBE:
        return is_reference_size(target->reference_size_cm_x10) ? 0 : -1;
    default:
        return -1;
    }
}

static competition_evaluation_t result(competition_decision_t decision,
                                       competition_reason_t reason)
{
    competition_evaluation_t value;
    value.decision = decision;
    value.reason = reason;
    return value;
}

competition_evaluation_t competition_evaluate(const competition_target_t *target,
                                              const vision_result_t *observation)
{
    uint8_t size_delta;

    if (competition_target_validate(target) != 0) {
        return result(COMP_DECISION_WAIT, COMP_REASON_INVALID_TARGET);
    }
    if (observation == 0 || observation->color_id == COLOR_UNKNOWN ||
        observation->shape_id == SHAPE_UNKNOWN) {
        return result(COMP_DECISION_WAIT, COMP_REASON_OBSERVATION_UNSTABLE);
    }
    if (observation->shape_id != SHAPE_CUBE) {
        return result(COMP_DECISION_SKIP, COMP_REASON_SHAPE_MISMATCH);
    }

    switch (target->mode) {
    case COMP_TASK_COLOR_CUBE:
    case COMP_TASK_SHAPE_COLOR_CUBE:
        if (observation->color_id != target->target_color) {
            return result(COMP_DECISION_SKIP, COMP_REASON_COLOR_MISMATCH);
        }
        return result(COMP_DECISION_EXECUTE, COMP_REASON_TARGET_MATCH);

    case COMP_TASK_SIZE_DELTA_1CM_CUBE:
        if (observation->size_cm_x10 == 0u) {
            return result(COMP_DECISION_WAIT, COMP_REASON_OBSERVATION_UNSTABLE);
        }
        size_delta = (observation->size_cm_x10 > target->reference_size_cm_x10)
                   ? (uint8_t)(observation->size_cm_x10 - target->reference_size_cm_x10)
                   : (uint8_t)(target->reference_size_cm_x10 - observation->size_cm_x10);
        return size_delta == 10u
             ? result(COMP_DECISION_EXECUTE, COMP_REASON_TARGET_MATCH)
             : result(COMP_DECISION_SKIP, COMP_REASON_SIZE_RELATION_MISMATCH);

    case COMP_TASK_SIZE_WITHIN_0P5CM_CUBE:
        if (observation->size_cm_x10 == 0u) {
            return result(COMP_DECISION_WAIT, COMP_REASON_OBSERVATION_UNSTABLE);
        }
        size_delta = (observation->size_cm_x10 > target->reference_size_cm_x10)
                   ? (uint8_t)(observation->size_cm_x10 - target->reference_size_cm_x10)
                   : (uint8_t)(target->reference_size_cm_x10 - observation->size_cm_x10);
        return size_delta <= 5u
             ? result(COMP_DECISION_EXECUTE, COMP_REASON_TARGET_MATCH)
             : result(COMP_DECISION_SKIP, COMP_REASON_SIZE_RELATION_MISMATCH);

    default:
        return result(COMP_DECISION_WAIT, COMP_REASON_INVALID_TARGET);
    }
}

const char *competition_reason_text(competition_reason_t reason)
{
    switch (reason) {
    case COMP_REASON_TARGET_MATCH: return "TARGET_MATCH";
    case COMP_REASON_COLOR_MISMATCH: return "COLOR_MISMATCH";
    case COMP_REASON_SHAPE_MISMATCH: return "SHAPE_MISMATCH";
    case COMP_REASON_SIZE_RELATION_MISMATCH: return "SIZE_RELATION_MISMATCH";
    case COMP_REASON_OBSERVATION_UNSTABLE: return "OBSERVATION_UNSTABLE";
    case COMP_REASON_INVALID_TARGET: return "INVALID_TARGET";
    case COMP_REASON_SIZE_UNAVAILABLE: return "SIZE_UNAVAILABLE";
    case COMP_REASON_OPERATOR_ABANDONED: return "OPERATOR_ABANDONED";
    case COMP_REASON_ROUND_TIMEOUT: return "ROUND_TIMEOUT";
    default: return "NONE";
    }
}
