#ifndef SINGLE_CAMERA_RUNTIME_H
#define SINGLE_CAMERA_RUNTIME_H

#include <stdint.h>

#include "single_camera_feature_adapter.h"

/* Transport results deliberately do not encode any MMIO address or protocol. */
typedef enum {
    SC_TRANSPORT_OK = 0,
    SC_TRANSPORT_NO_DATA = 1,
    SC_TRANSPORT_SNAPSHOT_TORN = 2,
    SC_TRANSPORT_UNAVAILABLE = 3,
    SC_TRANSPORT_ERROR = -1
} sc_transport_status_t;

typedef struct {
    uint16_t round_id;
    uint16_t frame_id;
    uint16_t config_revision;
    uint8_t input_flags;
    sc_observation_t observation;
    sc_decision_t decision;
    sc_reason_t reason;
    uint8_t arm_enabled;
} sc_round_result_t;

typedef struct {
    int (*read_feature_snapshot)(void *context, sc_feature_snapshot_t *snapshot);
    int (*ack_feature_frame)(void *context, uint16_t frame_id);
    int (*submit_round_result)(void *context, const sc_round_result_t *result);
    int (*emit_diagnostic_event)(void *context, const char *line);
    const char *source_name;
    void *context;
} sc_runtime_transport_t;

typedef struct {
    sc_runtime_transport_t transport;
    sc_f1_controller_t controller;
    uint16_t expected_config_revision;
    uint16_t last_frame_id;
    uint32_t event_sequence;
    uint32_t ack_count;
    uint32_t arm_request_count;
    uint32_t arm_send_count;
    uint8_t has_last_frame;
    uint8_t fatal;
    /* Traceability: snapshot of the last successfully ACKed frame this round,
       restored in ABANDON terminal results.  Cleared per round. */
    uint16_t last_acked_frame_id;
    uint8_t  last_acked_flags;
    uint8_t  has_acked_frame_this_round;
} sc_runtime_t;

/* ARM is hard-disabled in this seam. No arm transport exists. */
#define SC_RUNTIME_ARM_ENABLED 0u

int sc_runtime_init(sc_runtime_t *runtime, const sc_runtime_transport_t *transport,
                    uint16_t expected_config_revision, uint8_t size_available);
int sc_runtime_start_round(sc_runtime_t *runtime, const sc_target_t *target,
                           uint16_t place_event_seq, uint32_t now_ms,
                           uint32_t timeout_ms);
int sc_runtime_process_one(sc_runtime_t *runtime, uint32_t now_ms);
int sc_runtime_tick(sc_runtime_t *runtime, uint32_t now_ms);
int sc_runtime_abandon(sc_runtime_t *runtime, uint16_t abandon_event_seq);
const char *sc_runtime_decision_name(sc_decision_t decision);
const char *sc_runtime_event_schema(void);

#endif /* SINGLE_CAMERA_RUNTIME_H */
