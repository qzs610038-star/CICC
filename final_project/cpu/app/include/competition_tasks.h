/* Competition task rules independent of FPGA register definitions. */
#ifndef COMPETITION_TASKS_H
#define COMPETITION_TASKS_H

#include <stdint.h>
#include "vision_classifier.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COMP_TASK_COLOR_CUBE = 1,
    COMP_TASK_SHAPE_COLOR_CUBE = 2,
    COMP_TASK_SIZE_DELTA_1CM_CUBE = 3,
    COMP_TASK_SIZE_WITHIN_0P5CM_CUBE = 4
} competition_task_mode_t;

typedef enum {
    COMP_DECISION_WAIT = 0,
    COMP_DECISION_EXECUTE = 1,
    COMP_DECISION_SKIP = 2
} competition_decision_t;

typedef enum {
    COMP_REASON_NONE = 0,
    COMP_REASON_TARGET_MATCH = 1,
    COMP_REASON_COLOR_MISMATCH = 2,
    COMP_REASON_SHAPE_MISMATCH = 3,
    COMP_REASON_SIZE_RELATION_MISMATCH = 4,
    COMP_REASON_OBSERVATION_UNSTABLE = 5,
    COMP_REASON_INVALID_TARGET = 6,
    COMP_REASON_SIZE_UNAVAILABLE = 7,
    COMP_REASON_OPERATOR_ABANDONED = 8,
    COMP_REASON_ROUND_TIMEOUT = 9
} competition_reason_t;

typedef struct {
    competition_task_mode_t mode;
    uint8_t target_color;
    uint8_t reference_size_cm_x10;
} competition_target_t;

typedef struct {
    competition_decision_t decision;
    competition_reason_t reason;
} competition_evaluation_t;

/* Validate the operator configuration before a round starts. */
int competition_target_validate(const competition_target_t *target);

/* Evaluate one stable CPU classification against an official task rule. */
competition_evaluation_t competition_evaluate(const competition_target_t *target,
                                              const vision_result_t *observation);

const char *competition_reason_text(competition_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif /* COMPETITION_TASKS_H */
