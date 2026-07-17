#include "single_camera_runtime.h"

#include <stdio.h>
#include <string.h>

static const char *color_name(sc_color_t color)
{
    switch (color) {
    case SC_COLOR_WHITE: return "WHITE";
    case SC_COLOR_BLACK: return "BLACK";
    case SC_COLOR_RED: return "RED";
    case SC_COLOR_BLUE: return "BLUE";
    case SC_COLOR_YELLOW: return "YELLOW";
    default: return "UNKNOWN";
    }
}
static const char *shape_name(sc_shape_t shape)
{
    switch (shape) {
    case SC_SHAPE_CUBE: return "CUBE";
    case SC_SHAPE_CYLINDER: return "CYLINDER";
    case SC_SHAPE_CONE: return "CONE";
    default: return "UNKNOWN";
    }
}

const char *sc_runtime_decision_name(sc_decision_t decision)
{
    switch (decision) {
    case SC_DECISION_EXECUTE_ARM_DISABLED: return "EXECUTE_ARM_DISABLED";
    case SC_DECISION_SKIP: return "SKIP";
    default: return "WAIT";
    }
}

const char *sc_runtime_event_schema(void)
{
    return "@E|v=1|seq|event|round|frame|cfg|flags|class|decision|reason|ack|arm|source";
}

static int emit_event(sc_runtime_t *runtime, const char *event_type,
                      uint16_t frame_id, uint8_t flags,
                      const sc_observation_t *observation,
                      sc_decision_t decision, sc_reason_t reason,
                      const char *reason_override)
{
    char line[256];
    const char *classification = "UNKNOWN";
    const char *reason_text = reason_override ? reason_override : sc_f1_reason_text(reason);
    unsigned long next_sequence;

    if (runtime == 0 || runtime->transport.emit_diagnostic_event == 0) return -1;
    if (observation != 0) {
        classification = color_name(observation->color);
    }
    next_sequence = (unsigned long)(runtime->event_sequence + 1u);
    (void)snprintf(line, sizeof(line),
                   "@E|v=1|seq=%lu|event=%s|round=%u|frame=%u|cfg=%u|flags=%u|class=%s_%s|decision=%s|reason=%s|ack=%lu|arm=0|source=%s",
                   next_sequence, event_type,
                   (unsigned int)runtime->controller.round_seq,
                   (unsigned int)frame_id,
                   (unsigned int)runtime->expected_config_revision,
                   (unsigned int)flags,
                   classification,
                   observation ? shape_name(observation->shape) : "UNKNOWN",
                   sc_runtime_decision_name(decision), reason_text,
                   (unsigned long)runtime->ack_count,
                   runtime->transport.source_name ? runtime->transport.source_name : "unknown");
    runtime->event_sequence++;
    return runtime->transport.emit_diagnostic_event(runtime->transport.context, line);
}

static int fatal(sc_runtime_t *runtime, uint16_t frame_id, uint8_t flags,
                 const char *reason)
{
    runtime->fatal = 1u;
    (void)emit_event(runtime, "FATAL", frame_id, flags, 0,
                     SC_DECISION_WAIT, SC_REASON_NONE, reason);
    return -1;
}

int sc_runtime_init(sc_runtime_t *runtime, const sc_runtime_transport_t *transport,
                    uint16_t expected_config_revision, uint8_t size_available)
{
    if (runtime == 0 || transport == 0 || transport->read_feature_snapshot == 0 ||
        transport->ack_feature_frame == 0 || transport->submit_round_result == 0 ||
        transport->emit_diagnostic_event == 0 || expected_config_revision == 0u) {
        return -1;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->transport = *transport;
    runtime->expected_config_revision = expected_config_revision;
    sc_f1_init(&runtime->controller, size_available);
    return emit_event(runtime, "BOOT", 0u, 0u, 0,
                      SC_DECISION_WAIT, SC_REASON_NONE, "BOOT_HOST_ONLY");
}

int sc_runtime_start_round(sc_runtime_t *runtime, const sc_target_t *target,
                           uint16_t place_event_seq, uint32_t now_ms,
                           uint32_t timeout_ms)
{
    if (runtime == 0 || runtime->fatal) return -1;
    if (runtime->controller.place_seq == place_event_seq &&
        runtime->controller.round_seq != 0u) {
        (void)emit_event(runtime, "DUPLICATE_SUPPRESSED", 0u, 0u, 0,
                         runtime->controller.decision, runtime->controller.reason,
                         "PLACE_DUPLICATE");
        return 0;
    }
    if (sc_f1_apply_target(&runtime->controller, target) != 0 ||
        sc_f1_place(&runtime->controller, place_event_seq, now_ms, timeout_ms) != 0) {
        return fatal(runtime, 0u, 0u, "INVALID_TARGET_OR_PLACE");
    }
    return 0;
}

static int ack(sc_runtime_t *runtime, const sc_feature_snapshot_t *snapshot,
               const sc_observation_t *observation)
{
    if (runtime->transport.ack_feature_frame(runtime->transport.context,
                                             snapshot->frame_id) != SC_TRANSPORT_OK) {
        return fatal(runtime, snapshot->frame_id, snapshot->source_flags, "ACK_MISMATCH");
    }
    runtime->ack_count++;
    return emit_event(runtime, "ACK", snapshot->frame_id, snapshot->source_flags,
                      observation, runtime->controller.decision,
                      runtime->controller.reason, 0);
}

static int submit_result(sc_runtime_t *runtime, const sc_feature_snapshot_t *snapshot,
                         const sc_observation_t *observation)
{
    sc_round_result_t result;

    memset(&result, 0, sizeof(result));
    result.round_id = runtime->controller.round_seq;
    result.frame_id = snapshot->frame_id;
    result.config_revision = snapshot->config_seq;
    result.input_flags = snapshot->source_flags;
    result.observation = *observation;
    result.decision = runtime->controller.decision;
    result.reason = runtime->controller.reason;
    result.arm_enabled = SC_RUNTIME_ARM_ENABLED;
    if (runtime->transport.submit_round_result(runtime->transport.context, &result) !=
        SC_TRANSPORT_OK) {
        return fatal(runtime, snapshot->frame_id, snapshot->source_flags,
                     "ROUND_RESULT_REJECTED");
    }
    return emit_event(runtime, "ROUND_RESULT", snapshot->frame_id, snapshot->source_flags,
                      observation, result.decision, result.reason, 0);
}

static int submit_terminal_result(sc_runtime_t *runtime, const char *event_type)
{
    sc_round_result_t result;

    memset(&result, 0, sizeof(result));
    result.round_id = runtime->controller.round_seq;
    result.config_revision = runtime->expected_config_revision;
    result.observation = runtime->controller.observation;
    result.decision = runtime->controller.decision;
    result.reason = runtime->controller.reason;
    result.arm_enabled = SC_RUNTIME_ARM_ENABLED;
    if (runtime->transport.submit_round_result(runtime->transport.context, &result) !=
        SC_TRANSPORT_OK) {
        return fatal(runtime, 0u, 0u, "ROUND_RESULT_REJECTED");
    }
    return emit_event(runtime, event_type, 0u, 0u, &result.observation,
                      result.decision, result.reason, 0);
}

int sc_runtime_process_one(sc_runtime_t *runtime, uint32_t now_ms)
{
    sc_feature_snapshot_t snapshot;
    sc_observation_t observation;
    int status;

    if (runtime == 0 || runtime->fatal) return -1;
    memset(&snapshot, 0, sizeof(snapshot));
    status = runtime->transport.read_feature_snapshot(runtime->transport.context, &snapshot);
    if (status == SC_TRANSPORT_NO_DATA) return 0;
    if (status == SC_TRANSPORT_SNAPSHOT_TORN) {
        (void)emit_event(runtime, "SNAPSHOT_REJECT", snapshot.frame_id, snapshot.source_flags,
                         0, SC_DECISION_WAIT, SC_REASON_NONE, "SNAPSHOT_TORN");
        return fatal(runtime, snapshot.frame_id, snapshot.source_flags, "SNAPSHOT_TORN");
    }
    if (status != SC_TRANSPORT_OK) return fatal(runtime, snapshot.frame_id, snapshot.source_flags,
                                                  "TRANSPORT_READ_FAILED");
    if (snapshot.config_seq != runtime->expected_config_revision) {
        (void)emit_event(runtime, "SNAPSHOT_REJECT", snapshot.frame_id, snapshot.source_flags,
                         0, SC_DECISION_WAIT, SC_REASON_NONE, "CONFIG_REVISION_MISMATCH");
        return fatal(runtime, snapshot.frame_id, snapshot.source_flags,
                     "CONFIG_REVISION_MISMATCH");
    }
    if (runtime->has_last_frame && runtime->last_frame_id == snapshot.frame_id) {
        (void)emit_event(runtime, "DUPLICATE_SUPPRESSED", snapshot.frame_id,
                         snapshot.source_flags, 0, SC_DECISION_WAIT, SC_REASON_NONE,
                         "DUPLICATE_FRAME");
        return 0;
    }
    if (sc_feature_snapshot_is_usable(&snapshot) != 0) {
        (void)emit_event(runtime, "SNAPSHOT_REJECT", snapshot.frame_id, snapshot.source_flags,
                         0, SC_DECISION_WAIT, SC_REASON_NONE, "SNAPSHOT_FLAGS_REJECTED");
        return 0;
    }
    if (sc_feature_snapshot_to_observation(&snapshot, 0, &observation) != 0) {
        return fatal(runtime, snapshot.frame_id, snapshot.source_flags, "ADAPTER_FAILED");
    }
    runtime->has_last_frame = 1u;
    runtime->last_frame_id = snapshot.frame_id;
    (void)emit_event(runtime, "SNAPSHOT_ACCEPT", snapshot.frame_id, snapshot.source_flags,
                     &observation, SC_DECISION_WAIT, SC_REASON_NONE, 0);
    (void)emit_event(runtime, "CLASSIFY", snapshot.frame_id, snapshot.source_flags,
                     &observation, SC_DECISION_WAIT, SC_REASON_NONE,
                     observation.stable ? "CLASSIFIED" : "CLASSIFICATION_UNSTABLE");
    if (sc_f1_observe(&runtime->controller, &observation, now_ms) != 0) {
        return fatal(runtime, snapshot.frame_id, snapshot.source_flags, "ROUND_OBSERVE_FAILED");
    }
    if (runtime->controller.result_valid) {
        (void)emit_event(runtime, "DECISION", snapshot.frame_id, snapshot.source_flags,
                         &observation, runtime->controller.decision,
                         runtime->controller.reason, 0);
    }
    if (ack(runtime, &snapshot, &observation) != 0) return -1;
    if (runtime->controller.result_valid) return submit_result(runtime, &snapshot, &observation);
    return 0;
}

int sc_runtime_tick(sc_runtime_t *runtime, uint32_t now_ms)
{
    if (runtime == 0 || runtime->fatal) return -1;
    if (sc_f1_tick(&runtime->controller, now_ms) == 1) {
        (void)emit_event(runtime, "TIMEOUT", 0u, 0u, 0,
                         runtime->controller.decision, runtime->controller.reason, 0);
        (void)emit_event(runtime, "DECISION", 0u, 0u, 0,
                         runtime->controller.decision, runtime->controller.reason, 0);
        if (submit_terminal_result(runtime, "ROUND_RESULT") != 0) return -1;
        return 1;
    }
    return 0;
}

int sc_runtime_abandon(sc_runtime_t *runtime, uint16_t abandon_event_seq)
{
    if (runtime == 0 || runtime->fatal ||
        sc_f1_abandon(&runtime->controller, abandon_event_seq) != 0) {
        return -1;
    }
    (void)emit_event(runtime, "ABANDON", 0u, 0u, 0,
                     runtime->controller.decision, runtime->controller.reason, 0);
    (void)emit_event(runtime, "DECISION", 0u, 0u, 0,
                     runtime->controller.decision, runtime->controller.reason, 0);
    return submit_terminal_result(runtime, "ROUND_RESULT");
}
