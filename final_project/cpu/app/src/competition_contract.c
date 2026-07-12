#include "competition_contract.h"
#include <string.h>

static int task_requires_size(competition_task_mode_t mode)
{
    return mode == COMP_TASK_SIZE_DELTA_1CM_CUBE ||
           mode == COMP_TASK_SIZE_WITHIN_0P5CM_CUBE;
}

static int seq_is_newer(uint16_t candidate, uint16_t previous)
{
    return candidate != 0u && (int16_t)(candidate - previous) > 0;
}

static void clear_result(competition_contract_t *contract)
{
    memset(&contract->result, 0, sizeof(contract->result));
    contract->result.round_state = contract->transaction.state;
    contract->result.size_state = contract->size_state;
    contract->result.decision = COMP_DECISION_WAIT;
}

static void write_result(competition_contract_t *contract,
                         const vision_result_t *observation,
                         competition_evaluation_t evaluation)
{
    contract->result.event_seq = contract->transaction.event_seq;
    contract->result.round_state = contract->transaction.state;
    contract->result.size_state = contract->size_state;
    contract->result.decision = evaluation.decision;
    contract->result.reason = evaluation.reason;
    if (observation != 0) {
        contract->result.color_id = observation->color_id;
        contract->result.shape_id = observation->shape_id;
        /* Do not publish uncalibrated size as a physical measurement. */
        contract->result.size_cm_x10 = contract->size_state == SIZE_STATE_AVAILABLE
                                     ? observation->size_cm_x10 : 0u;
    }
}

void competition_contract_init(competition_contract_t *contract,
                               size_state_t size_state)
{
    if (contract != 0) {
        memset(contract, 0, sizeof(*contract));
        contract->size_state = size_state;
        competition_round_txn_init(&contract->transaction);
        clear_result(contract);
    }
}

int competition_contract_stage_target(competition_contract_t *contract,
                                      const target_config_t *config)
{
    if (contract == 0 || config == 0) {
        return -1;
    }
    if (config->valid && competition_target_validate(&config->target) != 0) {
        return -1;
    }
    contract->staging = *config;
    return 0;
}

int competition_contract_apply_target(competition_contract_t *contract)
{
    if (contract == 0 ||
        contract->transaction.state == COMP_ROUND_TXN_STATE_WAIT_OBSERVATION ||
        contract->transaction.state == COMP_ROUND_TXN_STATE_WAIT_ACK) {
        return -1;
    }
    contract->active = contract->staging;
    return contract->active.valid ? 0 : -1;
}

int competition_contract_handle_event(competition_contract_t *contract,
                                      const operator_event_t *event,
                                      uint32_t now_ms, uint32_t timeout_ms)
{
    if (contract == 0 || event == 0) {
        return -1;
    }
    if (event->event_seq == contract->last_event_seq) {
        return 0; /* Debounced retransmission: no second transaction. */
    }
    if (!seq_is_newer(event->event_seq, contract->last_event_seq)) {
        return -1;
    }

    switch (event->type) {
    case COMP_EVENT_PLACE:
        if (!contract->active.valid ||
            competition_round_txn_start(&contract->transaction,
                                        &contract->active.target,
                                        event->event_seq, now_ms,
                                        timeout_ms) != 0) {
            return -1;
        }
        clear_result(contract);
        contract->result.event_seq = event->event_seq;
        break;
    case COMP_EVENT_REMOVE:
        if (contract->transaction.state != COMP_ROUND_TXN_STATE_COMPLETE &&
            contract->transaction.state != COMP_ROUND_TXN_STATE_ABANDONED &&
            contract->transaction.state != COMP_ROUND_TXN_STATE_TIMEOUT) {
            return -1;
        }
        competition_round_txn_soft_reset(&contract->transaction);
        clear_result(contract);
        break;
    case COMP_EVENT_ABANDON:
        if (competition_round_txn_abandon(&contract->transaction) != 0) {
            return -1;
        }
        write_result(contract, 0, (competition_evaluation_t){
            COMP_DECISION_SKIP, COMP_REASON_OPERATOR_ABANDONED });
        break;
    case COMP_EVENT_RESET:
        competition_round_txn_soft_reset(&contract->transaction);
        memset(&contract->staging, 0, sizeof(contract->staging));
        memset(&contract->active, 0, sizeof(contract->active));
        clear_result(contract);
        break;
    default:
        return -1;
    }

    contract->last_event_seq = event->event_seq;
    return 0;
}

int competition_contract_observe(competition_contract_t *contract,
                                 const vision_result_t *observation,
                                 uint32_t now_ms)
{
    competition_evaluation_t evaluation;

    if (contract == 0 ||
        contract->transaction.state != COMP_ROUND_TXN_STATE_WAIT_OBSERVATION) {
        return -1;
    }
    if (contract->size_state == SIZE_STATE_UNAVAILABLE &&
        task_requires_size(contract->transaction.target.mode)) {
        evaluation.decision = COMP_DECISION_WAIT;
        evaluation.reason = COMP_REASON_SIZE_UNAVAILABLE;
        write_result(contract, observation, evaluation);
        return 0;
    }

    evaluation = competition_round_txn_observe(&contract->transaction,
                                               observation, now_ms);
    write_result(contract, observation, evaluation);
    return 0;
}

int competition_contract_ack_result(competition_contract_t *contract,
                                    uint16_t event_seq)
{
    if (contract == 0 ||
        competition_round_txn_ack(&contract->transaction, event_seq) != 0) {
        return -1;
    }
    contract->result.round_state = contract->transaction.state;
    return 0;
}

int competition_contract_tick(competition_contract_t *contract, uint32_t now_ms)
{
    if (contract == 0) {
        return -1;
    }
    if (competition_round_txn_tick(&contract->transaction, now_ms) > 0) {
        write_result(contract, 0, (competition_evaluation_t){
            COMP_DECISION_WAIT, COMP_REASON_ROUND_TIMEOUT });
        return 1;
    }
    return 0;
}

const result_status_t *competition_contract_get_result(const competition_contract_t *contract)
{
    return contract == 0 ? 0 : &contract->result;
}
