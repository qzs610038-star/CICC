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

/* 16-bit event_seq 新旧判定（支持回绕）。
 * delta = (uint16_t)(seq - last)：
 *   delta==0        → 同一序号，视为重复；
 *   1..32767        → 向前的新序号，接受（含回绕，如 65535→0 delta=1）；
 *   32768..65535    → 过期/倒退序号，拒绝（如 8→7 delta=65535）。
 * 首个事件由调用方的 have_event_seq==0 单独放行。 */
static int seq_is_newer(uint16_t seq, uint16_t last)
{
    uint16_t delta = (uint16_t)(seq - last);
    return (delta != 0u) && (delta < 0x8000u);
}

static int consume_new_event(round_controller_t *rc,
                             const round_controller_input_t *in,
                             round_event_t *event)
{
    if (!in || !in->event_valid || in->event == ROUND_EVENT_NONE) {
        return 0;
    }

    /* 已有基准序号后，只接受“向前”的序号；重复或过期一律不消费、不 ACK。 */
    if (rc->have_event_seq && !seq_is_newer(in->event_seq, rc->last_event_seq)) {
        return 0;
    }

    rc->have_event_seq = 1u;
    rc->last_event_seq = in->event_seq;
    *event = in->event;
    return 1;
}

static int event_is_accepted(round_controller_state_t state,
                             round_event_t event)
{
    switch (event) {
    case ROUND_EVENT_SESSION_RESET:
        return 1;
    case ROUND_EVENT_SOFT_RESET_ROUND:
    case ROUND_EVENT_ABANDON_ROUND:
        return state != ROUND_STATE_WAIT_ARM_DONE;
    case ROUND_EVENT_APPLY_CONFIG:
        return state == ROUND_STATE_CONFIG;
    case ROUND_EVENT_PLACE_CONFIRM:
        return state == ROUND_STATE_WAIT_PLACE_CONFIRM;
    case ROUND_EVENT_REMOVE_CONFIRM:
        return state == ROUND_STATE_ROUND_DONE;
    case ROUND_EVENT_NONE:
    default:
        return 0;
    }
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
    uint16_t ack_seq = 0u;
    round_event_ack_status_t ack_status = ROUND_EVENT_ACK_NONE;
    uint8_t request_arm_grab = 0u;
    uint32_t now_ms;

    if (!rc || !in) {
        return;
    }

    now_ms = in->now_ms;

    if (consume_new_event(rc, in, &event)) {
        ack_valid = 1u;
        ack_seq = in->event_seq;
        ack_status = event_is_accepted(rc->state, event) ?
            ROUND_EVENT_ACK_ACCEPTED : ROUND_EVENT_ACK_REJECTED;
    }

    if (ack_status == ROUND_EVENT_ACK_ACCEPTED) {
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
        if (ack_status == ROUND_EVENT_ACK_ACCEPTED &&
            event == ROUND_EVENT_APPLY_CONFIG) {
            clear_result(rc);
            enter_state(rc, ROUND_STATE_WAIT_PLACE_CONFIRM, now_ms);
        }
        break;

    case ROUND_STATE_WAIT_PLACE_CONFIRM:
        if (ack_status == ROUND_EVENT_ACK_ACCEPTED &&
            event == ROUND_EVENT_PLACE_CONFIRM) {
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
            /* 机械臂不可接收动作时（未使能 或 正忙）不得发起新抓取。
             * F1 保守策略：识别/判断结果已锁存，但本轮不执行动作，
             * 直接进入 ROUND_DONE 并以 ARM_NOT_READY 说明原因。
             * 保留 is_target=1，动作降级为 NONE，绝不 request_arm_grab。 */
            if (!in->arm_enabled || in->arm_busy) {
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
        if (ack_status == ROUND_EVENT_ACK_ACCEPTED &&
            event == ROUND_EVENT_REMOVE_CONFIRM) {
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
        out->event_ack_status = ack_status;
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
