#ifndef SINGLE_CAMERA_F1_H
#define SINGLE_CAMERA_F1_H

#include <stdint.h>

/* These values are CPU semantics, not an FPGA wire ABI. */
typedef enum {
    SC_COLOR_UNKNOWN = 0,
    SC_COLOR_WHITE = 1,
    SC_COLOR_BLACK = 2,
    SC_COLOR_RED = 3,
    SC_COLOR_BLUE = 4,
    SC_COLOR_YELLOW = 5
} sc_color_t;

typedef enum {
    SC_SHAPE_UNKNOWN = 0,
    SC_SHAPE_CUBE = 1,
    SC_SHAPE_CYLINDER = 2,
    SC_SHAPE_CONE = 3
} sc_shape_t;

typedef struct {
    sc_color_t color;
    sc_shape_t shape;
    uint8_t size_cm_x10; /* 20, 25, 30; zero means unavailable. */
    uint8_t stable;
} sc_observation_t;

typedef enum {
    SC_TASK_COLOR_CUBE = 1,
    SC_TASK_SHAPE_COLOR_CUBE = 2,
    SC_TASK_SIZE_DELTA_1CM_CUBE = 3,
    SC_TASK_SIZE_WITHIN_0P5CM_CUBE = 4
} sc_task_t;

typedef struct {
    sc_task_t task;
    sc_color_t target_color;
    uint8_t reference_size_cm_x10;
} sc_target_t;

typedef enum {
    SC_STATE_WAIT_PLACE = 0,
    SC_STATE_ACQUIRING = 1,
    SC_STATE_RESULT_LATCHED = 2,
    SC_STATE_ABANDONED = 3,
    SC_STATE_TIMEOUT = 4
} sc_state_t;

typedef enum {
    SC_DECISION_WAIT = 0,
    SC_DECISION_EXECUTE_ARM_DISABLED = 1,
    SC_DECISION_SKIP = 2
} sc_decision_t;

typedef enum {
    SC_REASON_NONE = 0,
    SC_REASON_TARGET_MATCH = 1,
    SC_REASON_COLOR_MISMATCH = 2,
    SC_REASON_SHAPE_MISMATCH = 3,
    SC_REASON_SIZE_RELATION_MISMATCH = 4,
    SC_REASON_OBSERVATION_UNSTABLE = 5,
    SC_REASON_SIZE_UNAVAILABLE = 6,
    SC_REASON_OPERATOR_ABANDONED = 7,
    SC_REASON_TIMEOUT = 8,
    SC_REASON_INVALID_TARGET = 9
} sc_reason_t;

typedef struct {
    sc_state_t state;
    uint16_t round_seq;
    uint16_t place_seq;
    uint32_t deadline_ms;
    sc_target_t target;
    sc_observation_t observation;
    sc_decision_t decision;
    sc_reason_t reason;
    uint8_t result_valid;
    uint8_t size_available;
} sc_f1_controller_t;

void sc_f1_init(sc_f1_controller_t *controller, uint8_t size_available);
int sc_f1_apply_target(sc_f1_controller_t *controller, const sc_target_t *target);
int sc_f1_place(sc_f1_controller_t *controller, uint16_t event_seq,
                uint32_t now_ms, uint32_t timeout_ms);
int sc_f1_observe(sc_f1_controller_t *controller,
                  const sc_observation_t *observation, uint32_t now_ms);
int sc_f1_abandon(sc_f1_controller_t *controller, uint16_t event_seq);
int sc_f1_tick(sc_f1_controller_t *controller, uint32_t now_ms);
const char *sc_f1_reason_text(sc_reason_t reason);

#endif /* SINGLE_CAMERA_F1_H */
