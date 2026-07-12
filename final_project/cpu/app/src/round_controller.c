#include "round_controller.h"

#define ROUND_DEFAULT_ACQUIRE_TIMEOUT_MS 3000u
#define ROUND_DEFAULT_ARM_TIMEOUT_MS     15000u

static int time_reached(uint32_t now_ms, uint32_t target_ms)
{
    return (int32_t)(now_ms - target_ms) >= 0;
}

static void enter_state(round_controller_t *rc, round_controller_state_t state,
                        uint32_t now_ms)
{
    rc->state = state;
    rc->state_enter_ms = now_ms;

    if (state == ROUND_STATE_ACQUIRE_STABLE) {
        rc->acquire_deadline_ms = now_ms + rc->cfg.acquire_timeout_ms;
    } else if (state == ROUND_STATE_WAIT_ARM_DONE) {
        rc->arm_deadline_ms = now_ms + rc->cfg.arm_timeout_ms;
    }
}

static void clear_result(round_controller_t *rc)
{
    rc->result_valid = 0u;
    rc->decision_action = MATCH_ACTION_NONE;
    rc->is_target = 0u;
    rc->reason = REASON_OBSERVATION_UNKNOWN;
    rc->arm_request_sent = 0u;
}

static void latch_result(round_controller_t *rc,
                         const task_match_result_t *match)
{
    rc->result_valid = 1u;
    rc->decision_action = match->action;
    rc->is_target = match->is_target;
    rc->reason = match->reason;
}

static void make_result(round_controller_t *rc, uint8_t action,
                        uint8_t is_target, reason_code_t reason)
{
    rc->result_valid = 1u;
    rc->decision_action = action;
    rc->is_target = is_target;
    rc->reason = reason;
}

static int consume_new_event(round_controller_t *rc,
                             const round_controller_input_t *in,
                             round_event_t *event)
{
    if (!in || !in->event_valid || in->event == ROUND_EVENT_NONE) {
        return 0;
    }

    if (rc->have_event_seq && in->event_seq == rc->last_event_seq) {
        return 0;
    }

    rc->have_event_seq = 1u;
    rc->last_event_seq = in->event_seq;
    *event = in->event;
    return 1;
}

void round_controller_init(round_controller_t *rc,
                           const round_controller_config_t *cfg,
                           uint32_t now_ms)
{
    if (!rc) {
        return;
    }

    rc->cfg.acquire_timeout_ms =
        (cfg && cfg->acquire_timeout_ms) ? cfg->acquire_timeout_ms :
        ROUND_DEFAULT_ACQUIRE_TIMEOUT_MS;
    rc->cfg.arm_timeout_ms =
        (cfg && cfg->arm_timeout_ms) ? cfg->arm_timeout_ms :
        ROUND_DEFAULT_ARM_TIMEOUT_MS;
    rc->round_seq = 0u;
    rc->have_event_seq = 0u;
    rc->last_event_seq = 0u;
    clear_result(rc);
    rc->state_enter_ms = now_ms;
    rc->acquire_deadline_ms = 0u;
    rc->arm_deadline_ms = 0u;
    enter_state(rc, ROUND_STATE_CONFIG, now_ms);
}

void round_controller_tick(round_controller_t *rc,
                           const round_controller_input_t *in,
                           round_controller_output_t *out)
{
    round_event_t event = ROUND_EVENT_NONE;
    uint8_t ack_valid = 0u;
    uint8_t ack_seq = 0u;
    uint8_t request_arm_grab = 0u;
    uint32_t now_ms;

    if (!rc || !in) {
        return;
    }

    now_ms = in->now_ms;

    if (consume_new_event(rc, in, &event)) {
        ack_valid = 1u;
        ack_seq = in->event_seq;
    }

    if (ack_valid) {
        if (event == ROUND_EVENT_SESSION_RESET) {
            rc->round_seq = 0u;
            clear_result(rc);
            enter_state(rc, ROUND_STATE_CONFIG, now_ms);
        } else if (event == ROUND_EVENT_SOFT_RESET_ROUND &&
                   rc->state != ROUND_STATE_WAIT_ARM_DONE) {
            clear_result(rc);
            enter_state(rc, ROUND_STATE_WAIT_PLACE_CONFIRM, now_ms);
        } else if (event == ROUND_EVENT_ABANDON_ROUND &&
                   rc->state != ROUND_STATE_WAIT_ARM_DONE) {
            make_result(rc, MATCH_ACTION_NONE, 0u, REASON_OPERATOR_ABANDON);
            enter_state(rc, ROUND_STATE_ROUND_DONE, now_ms);
        }
    }

    if (in->arm_fault && rc->state == ROUND_STATE_WAIT_ARM_DONE) {
        make_result(rc, MATCH_ACTION_NONE, rc->is_target, REASON_ARM_FAULT);
        enter_state(rc, ROUND_STATE_ARM_FAULT, now_ms);
    }

    switch (rc->state) {
    case ROUND_STATE_CONFIG:
        if (ack_valid && event == ROUND_EVENT_APPLY_CONFIG) {
            clear_result(rc);
            enter_state(rc, ROUND_STATE_WAIT_PLACE_CONFIRM, now_ms);
        }
        break;

    case ROUND_STATE_WAIT_PLACE_CONFIRM:
        if (ack_valid && event == ROUND_EVENT_PLACE_CONFIRM) {
            clear_result(rc);
            enter_state(rc, ROUND_STATE_ACQUIRE_STABLE, now_ms);
        }
        break;

    case ROUND_STATE_ACQUIRE_STABLE:
        if (in->observation_valid) {
            latch_result(rc, &in->match);
            enter_state(rc, ROUND_STATE_LATCH_RECOGNITION, now_ms);
        } else if (time_reached(now_ms, rc->acquire_deadline_ms)) {
            make_result(rc, MATCH_ACTION_NONE, 0u, REASON_STABILITY_TIMEOUT);
            enter_state(rc, ROUND_STATE_ROUND_DONE, now_ms);
        }
        break;

    case ROUND_STATE_LATCH_RECOGNITION:
        enter_state(rc, ROUND_STATE_LATCH_DECISION, now_ms);
        break;

    case ROUND_STATE_LATCH_DECISION:
        enter_state(rc, ROUND_STATE_EXECUTE_OR_SKIP, now_ms);
        break;

    case ROUND_STATE_EXECUTE_OR_SKIP:
        if (rc->decision_action == MATCH_ACTION_GRAB) {
            if (!in->arm_enabled) {
                make_result(rc, MATCH_ACTION_NONE, 1u, REASON_ARM_NOT_READY);
                enter_state(rc, ROUND_STATE_ROUND_DONE, now_ms);
            } else {
                if (!rc->arm_request_sent) {
                    rc->arm_request_sent = 1u;
                    request_arm_grab = 1u;
                }
                enter_state(rc, ROUND_STATE_WAIT_ARM_DONE, now_ms);
            }
        } else {
            enter_state(rc, ROUND_STATE_ROUND_DONE, now_ms);
        }
        break;

    case ROUND_STATE_WAIT_ARM_DONE:
        if (in->arm_done) {
            enter_state(rc, ROUND_STATE_ROUND_DONE, now_ms);
        } else if (time_reached(now_ms, rc->arm_deadline_ms)) {
            make_result(rc, MATCH_ACTION_NONE, rc->is_target, REASON_ARM_FAULT);
            enter_state(rc, ROUND_STATE_ARM_FAULT, now_ms);
        }
        break;

    case ROUND_STATE_ROUND_DONE:
        if (ack_valid && event == ROUND_EVENT_REMOVE_CONFIRM) {
            if (rc->round_seq < 65535u) {
                rc->round_seq++;
            }
            clear_result(rc);
            enter_state(rc, ROUND_STATE_WAIT_PLACE_CONFIRM, now_ms);
        }
        break;

    case ROUND_STATE_WAIT_REMOVE_CONFIRM:
    case ROUND_STATE_ARM_FAULT:
    default:
        break;
    }

    if (out) {
        out->state = rc->state;
        out->round_seq = rc->round_seq;
        out->event_ack_valid = ack_valid;
        out->event_ack_seq = ack_seq;
        out->request_arm_grab = request_arm_grab;
        out->result_valid = rc->result_valid;
        out->decision_action = rc->decision_action;
        out->is_target = rc->is_target;
        out->reason = rc->reason;
    }
}

round_controller_state_t round_controller_get_state(const round_controller_t *rc)
{
    return rc ? rc->state : ROUND_STATE_ARM_FAULT;
}
