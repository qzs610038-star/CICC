#include "competition_round_transaction.h"
#include <string.h>

static competition_evaluation_t wait_result(void)
{
    competition_evaluation_t value;
    value.decision = COMP_DECISION_WAIT;
    value.reason = COMP_REASON_NONE;
    return value;
}

void competition_round_txn_init(competition_round_txn_t *transaction)
{
    if (transaction != 0) {
        memset(transaction, 0, sizeof(*transaction));
        transaction->state = COMP_ROUND_TXN_STATE_IDLE;
        transaction->evaluation = wait_result();
    }
}

int competition_round_txn_start(competition_round_txn_t *transaction,
                                const competition_target_t *target,
                                uint16_t event_seq, uint32_t now_ms,
                                uint32_t timeout_ms)
{
    if (transaction == 0 || timeout_ms == 0u ||
        competition_target_validate(target) != 0) {
        return -1;
    }
    if (transaction->state == COMP_ROUND_TXN_STATE_WAIT_OBSERVATION ||
        transaction->state == COMP_ROUND_TXN_STATE_WAIT_ACK) {
        return -1;
    }

    transaction->target = *target;
    transaction->event_seq = event_seq;
    transaction->deadline_ms = now_ms + timeout_ms;
    transaction->evaluation = wait_result();
    transaction->state = COMP_ROUND_TXN_STATE_WAIT_OBSERVATION;
    return 0;
}

competition_evaluation_t competition_round_txn_observe(
    competition_round_txn_t *transaction,
    const vision_result_t *observation,
    uint32_t now_ms)
{
    if (transaction == 0 ||
        transaction->state != COMP_ROUND_TXN_STATE_WAIT_OBSERVATION) {
        return wait_result();
    }
    if ((int32_t)(now_ms - transaction->deadline_ms) >= 0) {
        transaction->state = COMP_ROUND_TXN_STATE_TIMEOUT;
        return wait_result();
    }

    transaction->evaluation = competition_evaluate(&transaction->target,
                                                    observation);
    if (transaction->evaluation.decision != COMP_DECISION_WAIT) {
        /* The first final result is latched until its matching ACK arrives. */
        transaction->state = COMP_ROUND_TXN_STATE_WAIT_ACK;
    }
    return transaction->evaluation;
}

int competition_round_txn_ack(competition_round_txn_t *transaction,
                              uint16_t event_seq)
{
    if (transaction == 0 ||
        transaction->state != COMP_ROUND_TXN_STATE_WAIT_ACK ||
        transaction->event_seq != event_seq) {
        return -1;
    }
    transaction->state = COMP_ROUND_TXN_STATE_COMPLETE;
    return 0;
}

int competition_round_txn_abandon(competition_round_txn_t *transaction)
{
    if (transaction == 0 ||
        (transaction->state != COMP_ROUND_TXN_STATE_WAIT_OBSERVATION &&
         transaction->state != COMP_ROUND_TXN_STATE_WAIT_ACK)) {
        return -1;
    }
    transaction->state = COMP_ROUND_TXN_STATE_ABANDONED;
    return 0;
}

int competition_round_txn_tick(competition_round_txn_t *transaction,
                               uint32_t now_ms)
{
    if (transaction == 0) {
        return -1;
    }
    if ((transaction->state == COMP_ROUND_TXN_STATE_WAIT_OBSERVATION ||
         transaction->state == COMP_ROUND_TXN_STATE_WAIT_ACK) &&
        (int32_t)(now_ms - transaction->deadline_ms) >= 0) {
        transaction->state = COMP_ROUND_TXN_STATE_TIMEOUT;
        return 1;
    }
    return 0;
}

void competition_round_txn_soft_reset(competition_round_txn_t *transaction)
{
    competition_round_txn_init(transaction);
}
