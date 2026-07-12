/* One-round observation/decision transaction. It never drives the arm directly. */
#ifndef COMPETITION_ROUND_TRANSACTION_H
#define COMPETITION_ROUND_TRANSACTION_H

#include <stdint.h>
#include "competition_tasks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COMP_ROUND_TXN_STATE_IDLE = 0,
    COMP_ROUND_TXN_STATE_WAIT_OBSERVATION = 1,
    COMP_ROUND_TXN_STATE_WAIT_ACK = 2,
    COMP_ROUND_TXN_STATE_COMPLETE = 3,
    COMP_ROUND_TXN_STATE_TIMEOUT = 4,
    COMP_ROUND_TXN_STATE_ABANDONED = 5
} competition_round_txn_state_t;

typedef struct {
    uint16_t event_seq;
    uint32_t timeout_ms;
} competition_round_txn_cfg_t;

typedef struct {
    competition_round_txn_state_t state;
    uint16_t event_seq;
    uint32_t deadline_ms;
    competition_target_t target;
    competition_evaluation_t evaluation;
} competition_round_txn_t;

void competition_round_txn_init(competition_round_txn_t *transaction);
int competition_round_txn_start(competition_round_txn_t *transaction,
                                const competition_target_t *target,
                                uint16_t event_seq, uint32_t now_ms,
                                uint32_t timeout_ms);
competition_evaluation_t competition_round_txn_observe(
    competition_round_txn_t *transaction,
    const vision_result_t *observation,
    uint32_t now_ms);
int competition_round_txn_ack(competition_round_txn_t *transaction,
                              uint16_t event_seq);
int competition_round_txn_abandon(competition_round_txn_t *transaction);
int competition_round_txn_tick(competition_round_txn_t *transaction,
                               uint32_t now_ms);
void competition_round_txn_soft_reset(competition_round_txn_t *transaction);

#ifdef __cplusplus
}
#endif

#endif /* COMPETITION_ROUND_TRANSACTION_H */
