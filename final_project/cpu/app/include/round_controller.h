/* One-round transaction controller. It never drives the arm directly. */
#ifndef ROUND_CONTROLLER_H
#define ROUND_CONTROLLER_H

#include <stdint.h>
#include "competition_tasks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ROUND_STATE_IDLE = 0,
    ROUND_STATE_WAIT_OBSERVATION = 1,
    ROUND_STATE_WAIT_ACK = 2,
    ROUND_STATE_COMPLETE = 3,
    ROUND_STATE_TIMEOUT = 4,
    ROUND_STATE_ABANDONED = 5
} round_state_t;

typedef struct {
    uint16_t event_seq;
    uint32_t timeout_ms;
} round_controller_cfg_t;

typedef struct {
    round_state_t state;
    uint16_t event_seq;
    uint32_t deadline_ms;
    competition_target_t target;
    competition_evaluation_t evaluation;
} round_controller_t;

void round_controller_init(round_controller_t *controller);
int round_controller_start(round_controller_t *controller,
                           const competition_target_t *target,
                           uint16_t event_seq, uint32_t now_ms,
                           uint32_t timeout_ms);
competition_evaluation_t round_controller_observe(round_controller_t *controller,
                                                   const vision_result_t *observation,
                                                   uint32_t now_ms);
int round_controller_ack(round_controller_t *controller, uint16_t event_seq);
int round_controller_abandon(round_controller_t *controller);
int round_controller_tick(round_controller_t *controller, uint32_t now_ms);
void round_controller_soft_reset(round_controller_t *controller);

#ifdef __cplusplus
}
#endif

#endif /* ROUND_CONTROLLER_H */
