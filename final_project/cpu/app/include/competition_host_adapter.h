/* Host-only adapter for exercising the CPU transaction contract end to end. */
#ifndef COMPETITION_HOST_ADAPTER_H
#define COMPETITION_HOST_ADAPTER_H

#include <stdint.h>
#include "competition_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    competition_contract_t contract;
    uint32_t now_ms;
    uint32_t round_timeout_ms;
} competition_host_adapter_t;

void competition_host_adapter_init(competition_host_adapter_t *adapter,
                                   size_state_t size_state,
                                   uint32_t round_timeout_ms);
int competition_host_adapter_configure(competition_host_adapter_t *adapter,
                                       const target_config_t *config);
int competition_host_adapter_event(competition_host_adapter_t *adapter,
                                   competition_event_type_t type,
                                   uint16_t event_seq);
int competition_host_adapter_observe(competition_host_adapter_t *adapter,
                                     const vision_result_t *observation);
int competition_host_adapter_ack(competition_host_adapter_t *adapter,
                                 uint16_t event_seq);
void competition_host_adapter_advance(competition_host_adapter_t *adapter,
                                      uint32_t elapsed_ms);
const result_status_t *competition_host_adapter_status(const competition_host_adapter_t *adapter);

#ifdef __cplusplus
}
#endif

#endif /* COMPETITION_HOST_ADAPTER_H */
