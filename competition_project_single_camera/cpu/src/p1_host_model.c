#include "p1_host_model.h"

#include <string.h>

static int serial16_newer(uint16_t candidate, uint16_t reference)
{
    const uint16_t delta = (uint16_t)(candidate - reference);
    return delta != 0u && delta < 0x8000u;
}

void p1_result_packer_init(p1_result_packer_t *packer)
{
    if (packer != 0) memset(packer, 0, sizeof(*packer));
}

int p1_result_stage(p1_result_packer_t *packer, const p1_result_t *result)
{
    if (packer == 0 || result == 0 || result->arm_enabled != P1_ARM_ENABLED) {
        return -1;
    }
    packer->staging[0] = ((uint32_t)result->round_id << 16u) | result->frame_id;
    packer->staging[1] = result->config_seq;
    packer->staging[2] = ((uint32_t)result->color << 24u) |
                         ((uint32_t)result->shape << 16u) |
                         ((uint32_t)result->size_cm_x10 << 8u) |
                         result->target;
    packer->staging[3] = ((uint32_t)result->decision << 24u) |
                         ((uint32_t)result->reason << 16u) |
                         ((uint32_t)result->input_flags << 8u) |
                         result->arm_enabled;
    packer->staging[4] = 0u;
    packer->staging[5] = 0u;
    packer->staging[6] = 0u;
    packer->staging[7] = 0u;
    return 0;
}

int p1_result_commit(p1_result_packer_t *packer, uint16_t round_id)
{
    if (packer == 0 || (uint16_t)(packer->staging[0] >> 16u) != round_id) return -1;
    if (packer->has_active && !serial16_newer(round_id, packer->active_round_id)) {
        return -1;
    }
    memcpy(packer->active, packer->staging, sizeof(packer->active));
    packer->active_round_id = round_id;
    packer->has_active = 1u;
    return 0;
}

void p1_input_init(p1_input_model_t *model)
{
    if (model != 0) memset(model, 0, sizeof(*model));
}

void p1_input_stage_config(p1_input_model_t *model, const p1_config_t *config)
{
    if (model != 0 && config != 0) model->staging_config = *config;
}

static p1_event_status_t classify_seq(p1_input_model_t *model, uint16_t event_seq)
{
    if (!model->has_event_seq) return P1_EVENT_ACCEPTED;
    if (event_seq == model->last_event_seq) return P1_EVENT_DUPLICATE;
    if (!serial16_newer(event_seq, model->last_event_seq)) return P1_EVENT_STALE;
    return P1_EVENT_ACCEPTED;
}

p1_event_status_t p1_input_event(p1_input_model_t *model,
                                 p1_input_kind_t kind,
                                 uint16_t event_seq,
                                 uint16_t *acked_event_seq)
{
    p1_event_status_t status;

    if (model == 0 || acked_event_seq == 0) return P1_EVENT_INVALID_STATE;
    status = classify_seq(model, event_seq);
    if (status != P1_EVENT_ACCEPTED) return status;

    switch (kind) {
    case P1_INPUT_APPLY:
        model->pending_config = model->staging_config;
        model->config_pending = 1u;
        break;
    case P1_INPUT_PLACE:
        if (!model->config_active || model->object_present) return P1_EVENT_INVALID_STATE;
        model->object_present = 1u;
        model->result_latched = 0u;
        model->round_id++;
        break;
    case P1_INPUT_REMOVE:
        if (!model->object_present) return P1_EVENT_INVALID_STATE;
        model->object_present = 0u;
        model->result_latched = 0u;
        break;
    case P1_INPUT_ABANDON:
        if (!model->object_present) return P1_EVENT_INVALID_STATE;
        model->object_present = 0u;
        model->result_latched = 0u;
        break;
    case P1_INPUT_RESET:
        model->config_pending = 0u;
        model->config_active = 0u;
        model->object_present = 0u;
        model->result_latched = 0u;
        break;
    default:
        return P1_EVENT_INVALID_STATE;
    }

    model->last_event_seq = event_seq;
    model->has_event_seq = 1u;
    *acked_event_seq = event_seq;
    return P1_EVENT_ACCEPTED;
}

p1_event_status_t p1_input_frame_boundary(p1_input_model_t *model)
{
    if (model == 0 || !model->config_pending) return P1_EVENT_INVALID_STATE;
    model->active_config = model->pending_config;
    model->config_pending = 0u;
    model->config_active = 1u;
    return P1_EVENT_ACCEPTED;
}

p1_event_status_t p1_input_latch_result(p1_input_model_t *model,
                                        uint16_t round_id)
{
    if (model == 0 || !model->object_present || model->result_latched ||
        model->round_id != round_id) return P1_EVENT_INVALID_STATE;
    model->result_latched = 1u;
    return P1_EVENT_ACCEPTED;
}
