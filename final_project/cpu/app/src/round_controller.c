#include "round_controller.h"
#include <string.h>

static competition_evaluation_t wait_result(void)
{
    competition_evaluation_t value;
    value.decision = COMP_DECISION_WAIT;
    value.reason = COMP_REASON_NONE;
    return value;
}

void round_controller_init(round_controller_t *controller)
{
    if (controller != 0) {
        memset(controller, 0, sizeof(*controller));
        controller->state = ROUND_STATE_IDLE;
        controller->evaluation = wait_result();
    }
}

int round_controller_start(round_controller_t *controller,
                           const competition_target_t *target,
                           uint16_t event_seq, uint32_t now_ms,
                           uint32_t timeout_ms)
{
    if (controller == 0 || timeout_ms == 0u ||
        competition_target_validate(target) != 0) {
        return -1;
    }
    if (controller->state == ROUND_STATE_WAIT_OBSERVATION ||
        controller->state == ROUND_STATE_WAIT_ACK) {
        return -1;
    }

    controller->target = *target;
    controller->event_seq = event_seq;
    controller->deadline_ms = now_ms + timeout_ms;
    controller->evaluation = wait_result();
    controller->state = ROUND_STATE_WAIT_OBSERVATION;
    return 0;
}

competition_evaluation_t round_controller_observe(round_controller_t *controller,
                                                   const vision_result_t *observation,
                                                   uint32_t now_ms)
{
    if (controller == 0 || controller->state != ROUND_STATE_WAIT_OBSERVATION) {
        return wait_result();
    }
    if ((int32_t)(now_ms - controller->deadline_ms) >= 0) {
        controller->state = ROUND_STATE_TIMEOUT;
        return wait_result();
    }

    controller->evaluation = competition_evaluate(&controller->target, observation);
    if (controller->evaluation.decision != COMP_DECISION_WAIT) {
        /* The first final result is latched until its matching ACK arrives. */
        controller->state = ROUND_STATE_WAIT_ACK;
    }
    return controller->evaluation;
}

int round_controller_ack(round_controller_t *controller, uint16_t event_seq)
{
    if (controller == 0 || controller->state != ROUND_STATE_WAIT_ACK ||
        controller->event_seq != event_seq) {
        return -1;
    }
    controller->state = ROUND_STATE_COMPLETE;
    return 0;
}

int round_controller_abandon(round_controller_t *controller)
{
    if (controller == 0 || (controller->state != ROUND_STATE_WAIT_OBSERVATION &&
                           controller->state != ROUND_STATE_WAIT_ACK)) {
        return -1;
    }
    controller->state = ROUND_STATE_ABANDONED;
    return 0;
}

int round_controller_tick(round_controller_t *controller, uint32_t now_ms)
{
    if (controller == 0) {
        return -1;
    }
    if ((controller->state == ROUND_STATE_WAIT_OBSERVATION ||
         controller->state == ROUND_STATE_WAIT_ACK) &&
        (int32_t)(now_ms - controller->deadline_ms) >= 0) {
        controller->state = ROUND_STATE_TIMEOUT;
        return 1;
    }
    return 0;
}

void round_controller_soft_reset(round_controller_t *controller)
{
    round_controller_init(controller);
}
