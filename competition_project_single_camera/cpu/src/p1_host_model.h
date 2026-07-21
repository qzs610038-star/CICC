#ifndef P1_HOST_MODEL_H
#define P1_HOST_MODEL_H

#include <stddef.h>
#include <stdint.h>

#define P1_RESULT_WORDS 8u
#define P1_ARM_ENABLED 0u

typedef enum {
    P1_INPUT_APPLY = 1,
    P1_INPUT_PLACE = 2,
    P1_INPUT_REMOVE = 3,
    P1_INPUT_ABANDON = 4,
    P1_INPUT_RESET = 5
} p1_input_kind_t;

typedef enum {
    P1_EVENT_ACCEPTED = 0,
    P1_EVENT_DUPLICATE = 1,
    P1_EVENT_STALE = 2,
    P1_EVENT_INVALID_STATE = 3
} p1_event_status_t;

typedef struct {
    uint16_t round_id;
    uint16_t frame_id;
    uint16_t config_seq;
    uint8_t color;
    uint8_t shape;
    uint8_t size_cm_x10;
    uint8_t target;
    uint8_t decision;
    uint8_t reason;
    uint8_t input_flags;
    uint8_t arm_enabled;
} p1_result_t;

typedef struct {
    uint32_t staging[P1_RESULT_WORDS];
    uint32_t active[P1_RESULT_WORDS];
    uint16_t active_round_id;
    uint8_t has_active;
} p1_result_packer_t;

typedef struct {
    uint16_t task;
    uint16_t target_color;
    uint16_t reference_size_cm_x10;
} p1_config_t;

typedef struct {
    p1_config_t staging_config;
    p1_config_t pending_config;
    p1_config_t active_config;
    uint16_t last_event_seq;
    uint16_t round_id;
    uint8_t has_event_seq;
    uint8_t config_pending;
    uint8_t config_active;
    uint8_t object_present;
    uint8_t result_latched;
} p1_input_model_t;

void p1_result_packer_init(p1_result_packer_t *packer);
int p1_result_stage(p1_result_packer_t *packer, const p1_result_t *result);
int p1_result_commit(p1_result_packer_t *packer, uint16_t round_id);
void p1_input_init(p1_input_model_t *model);
void p1_input_stage_config(p1_input_model_t *model, const p1_config_t *config);
p1_event_status_t p1_input_frame_boundary(p1_input_model_t *model);
p1_event_status_t p1_input_latch_result(p1_input_model_t *model,
                                        uint16_t round_id);
p1_event_status_t p1_input_event(p1_input_model_t *model,
                                 p1_input_kind_t kind,
                                 uint16_t event_seq,
                                 uint16_t *acked_event_seq);

#endif
