/* Register-independent target, event, and result contract. */
#ifndef COMPETITION_CONTRACT_H
#define COMPETITION_CONTRACT_H

#include <stdint.h>
#include "competition_round_transaction.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SIZE_STATE_UNAVAILABLE = 0,
    SIZE_STATE_AVAILABLE = 1
} size_state_t;

typedef enum {
    COMP_EVENT_PLACE = 1,
    COMP_EVENT_REMOVE = 2,
    COMP_EVENT_ABANDON = 3,
    COMP_EVENT_RESET = 4
} competition_event_type_t;

typedef struct {
    competition_event_type_t type;
    uint16_t event_seq;
} operator_event_t;

typedef struct {
    uint8_t valid;
    competition_target_t target;
} target_config_t;

typedef struct {
    uint16_t event_seq;
    competition_round_txn_state_t round_state;
    uint8_t color_id;
    uint8_t shape_id;
    uint8_t size_cm_x10;
    size_state_t size_state;
    competition_decision_t decision;
    competition_reason_t reason;
} result_status_t;

typedef struct {
    target_config_t staging;
    target_config_t active;
    uint16_t last_event_seq;
    size_state_t size_state;
    competition_round_txn_t transaction;
    result_status_t result;
} competition_contract_t;

void competition_contract_init(competition_contract_t *contract,
                               size_state_t size_state);
int competition_contract_stage_target(competition_contract_t *contract,
                                      const target_config_t *config);
int competition_contract_apply_target(competition_contract_t *contract);
int competition_contract_handle_event(competition_contract_t *contract,
                                      const operator_event_t *event,
                                      uint32_t now_ms, uint32_t timeout_ms);
int competition_contract_observe(competition_contract_t *contract,
                                 const vision_result_t *observation,
                                 uint32_t now_ms);
int competition_contract_ack_result(competition_contract_t *contract,
                                    uint16_t event_seq);
int competition_contract_tick(competition_contract_t *contract, uint32_t now_ms);
const result_status_t *competition_contract_get_result(const competition_contract_t *contract);

#ifdef __cplusplus
}
#endif

#endif /* COMPETITION_CONTRACT_H */
