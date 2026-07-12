#include "competition_host_adapter.h"

void competition_host_adapter_init(competition_host_adapter_t *adapter,
                                   size_state_t size_state,
                                   uint32_t round_timeout_ms)
{
    if (adapter != 0) {
        competition_contract_init(&adapter->contract, size_state);
        adapter->now_ms = 0u;
        adapter->round_timeout_ms = round_timeout_ms;
    }
}

int competition_host_adapter_configure(competition_host_adapter_t *adapter,
                                       const target_config_t *config)
{
    if (adapter == 0 || competition_contract_stage_target(&adapter->contract, config) != 0) {
        return -1;
    }
    return competition_contract_apply_target(&adapter->contract);
}

int competition_host_adapter_event(competition_host_adapter_t *adapter,
                                   competition_event_type_t type,
                                   uint16_t event_seq)
{
    operator_event_t event;
    if (adapter == 0) {
        return -1;
    }
    event.type = type;
    event.event_seq = event_seq;
    return competition_contract_handle_event(&adapter->contract, &event,
                                             adapter->now_ms,
                                             adapter->round_timeout_ms);
}

int competition_host_adapter_observe(competition_host_adapter_t *adapter,
                                     const vision_result_t *observation)
{
    return adapter == 0 ? -1 : competition_contract_observe(&adapter->contract,
                                                              observation, adapter->now_ms);
}

int competition_host_adapter_ack(competition_host_adapter_t *adapter,
                                 uint16_t event_seq)
{
    return adapter == 0 ? -1 : competition_contract_ack_result(&adapter->contract, event_seq);
}

void competition_host_adapter_advance(competition_host_adapter_t *adapter,
                                      uint32_t elapsed_ms)
{
    if (adapter != 0) {
        adapter->now_ms += elapsed_ms;
        (void)competition_contract_tick(&adapter->contract, adapter->now_ms);
    }
}

const result_status_t *competition_host_adapter_status(const competition_host_adapter_t *adapter)
{
    return adapter == 0 ? 0 : competition_contract_get_result(&adapter->contract);
}
