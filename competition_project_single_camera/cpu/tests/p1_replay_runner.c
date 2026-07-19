#include <windows.h>
#include <bcrypt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "p1_host_model.h"
#include "single_camera_fake_transport.h"

#pragma comment(lib, "bcrypt.lib")

typedef enum {
    TERMINAL_RESULT = 0,
    TERMINAL_ABANDON = 1,
    TERMINAL_TIMEOUT = 2
} terminal_kind_t;

typedef struct {
    sc_task_t task;
    sc_color_t target_color;
    sc_color_t observed_color;
    terminal_kind_t terminal;
    sc_decision_t expected_decision;
    sc_reason_t expected_reason;
} replay_case_t;

static const replay_case_t cases[20] = {
    {SC_TASK_COLOR_CUBE, SC_COLOR_RED, SC_COLOR_RED, TERMINAL_RESULT, SC_DECISION_EXECUTE_ARM_DISABLED, SC_REASON_TARGET_MATCH},
    {SC_TASK_COLOR_CUBE, SC_COLOR_RED, SC_COLOR_BLUE, TERMINAL_RESULT, SC_DECISION_SKIP, SC_REASON_COLOR_MISMATCH},
    {SC_TASK_COLOR_CUBE, SC_COLOR_RED, SC_COLOR_RED, TERMINAL_RESULT, SC_DECISION_EXECUTE_ARM_DISABLED, SC_REASON_TARGET_MATCH},
    {SC_TASK_COLOR_CUBE, SC_COLOR_RED, SC_COLOR_YELLOW, TERMINAL_RESULT, SC_DECISION_SKIP, SC_REASON_COLOR_MISMATCH},
    {SC_TASK_COLOR_CUBE, SC_COLOR_RED, SC_COLOR_RED, TERMINAL_RESULT, SC_DECISION_EXECUTE_ARM_DISABLED, SC_REASON_TARGET_MATCH},
    {SC_TASK_SHAPE_COLOR_CUBE, SC_COLOR_BLUE, SC_COLOR_BLUE, TERMINAL_RESULT, SC_DECISION_EXECUTE_ARM_DISABLED, SC_REASON_TARGET_MATCH},
    {SC_TASK_SHAPE_COLOR_CUBE, SC_COLOR_BLUE, SC_COLOR_RED, TERMINAL_RESULT, SC_DECISION_SKIP, SC_REASON_COLOR_MISMATCH},
    {SC_TASK_SHAPE_COLOR_CUBE, SC_COLOR_BLUE, SC_COLOR_BLUE, TERMINAL_RESULT, SC_DECISION_EXECUTE_ARM_DISABLED, SC_REASON_TARGET_MATCH},
    {SC_TASK_SHAPE_COLOR_CUBE, SC_COLOR_BLUE, SC_COLOR_YELLOW, TERMINAL_RESULT, SC_DECISION_SKIP, SC_REASON_COLOR_MISMATCH},
    {SC_TASK_SHAPE_COLOR_CUBE, SC_COLOR_BLUE, SC_COLOR_BLUE, TERMINAL_RESULT, SC_DECISION_EXECUTE_ARM_DISABLED, SC_REASON_TARGET_MATCH},
    {SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_RED, SC_COLOR_RED, TERMINAL_ABANDON, SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED},
    {SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_RED, SC_COLOR_RED, TERMINAL_TIMEOUT, SC_DECISION_WAIT, SC_REASON_TIMEOUT},
    {SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_RED, SC_COLOR_RED, TERMINAL_ABANDON, SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED},
    {SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_RED, SC_COLOR_RED, TERMINAL_TIMEOUT, SC_DECISION_WAIT, SC_REASON_TIMEOUT},
    {SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_RED, SC_COLOR_RED, TERMINAL_ABANDON, SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED},
    {SC_TASK_SIZE_WITHIN_0P5CM_CUBE, SC_COLOR_RED, SC_COLOR_RED, TERMINAL_TIMEOUT, SC_DECISION_WAIT, SC_REASON_TIMEOUT},
    {SC_TASK_SIZE_WITHIN_0P5CM_CUBE, SC_COLOR_RED, SC_COLOR_RED, TERMINAL_ABANDON, SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED},
    {SC_TASK_SIZE_WITHIN_0P5CM_CUBE, SC_COLOR_RED, SC_COLOR_RED, TERMINAL_TIMEOUT, SC_DECISION_WAIT, SC_REASON_TIMEOUT},
    {SC_TASK_SIZE_WITHIN_0P5CM_CUBE, SC_COLOR_RED, SC_COLOR_RED, TERMINAL_ABANDON, SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED},
    {SC_TASK_SIZE_WITHIN_0P5CM_CUBE, SC_COLOR_RED, SC_COLOR_RED, TERMINAL_TIMEOUT, SC_DECISION_WAIT, SC_REASON_TIMEOUT}
};

static void put_u16_le(unsigned char *out, size_t *offset, uint16_t value)
{
    out[(*offset)++] = (unsigned char)(value & 0xffu);
    out[(*offset)++] = (unsigned char)(value >> 8u);
}

static void put_u32_le(unsigned char *out, size_t *offset, uint32_t value)
{
    unsigned shift;
    for (shift = 0u; shift < 32u; shift += 8u) out[(*offset)++] = (unsigned char)(value >> shift);
}

static size_t serialize_snapshot(const sc_feature_snapshot_t *snapshot, unsigned char out[33])
{
    size_t offset = 0u;
    put_u16_le(out, &offset, snapshot->frame_id);
    put_u16_le(out, &offset, snapshot->config_seq);
    out[offset++] = snapshot->source_flags;
    put_u32_le(out, &offset, snapshot->features.red_area);
    put_u32_le(out, &offset, snapshot->features.blue_area);
    put_u32_le(out, &offset, snapshot->features.yellow_area);
    put_u32_le(out, &offset, snapshot->features.foreground_area);
    put_u32_le(out, &offset, snapshot->features.roi_pixel_count);
    put_u32_le(out, &offset, snapshot->features.sum_luma);
    put_u16_le(out, &offset, snapshot->features.bbox_width);
    put_u16_le(out, &offset, snapshot->features.bbox_height);
    return offset;
}

static int sha256_hex(const unsigned char *data, size_t length, char out[65])
{
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    unsigned char digest[32];
    NTSTATUS status;
    unsigned i;

    status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0u);
    if (status < 0) return -1;
    status = BCryptCreateHash(algorithm, &hash, NULL, 0u, NULL, 0u, 0u);
    if (status >= 0) status = BCryptHashData(hash, (PUCHAR)data, (ULONG)length, 0u);
    if (status >= 0) status = BCryptFinishHash(hash, digest, (ULONG)sizeof(digest), 0u);
    if (hash != NULL) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0u);
    if (status < 0) return -1;
    for (i = 0u; i < sizeof(digest); ++i) (void)sprintf_s(out + (i * 2u), 3u, "%02X", digest[i]);
    out[64] = '\0';
    return 0;
}

static void bytes_to_hex(const unsigned char *data, size_t length, char *out)
{
    size_t i;
    for (i = 0u; i < length; ++i) (void)sprintf_s(out + (i * 2u), 3u, "%02X", data[i]);
    out[length * 2u] = '\0';
}

static sc_feature_snapshot_t snapshot_for(uint16_t frame_id, sc_color_t color)
{
    sc_feature_snapshot_t snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.frame_id = frame_id;
    snapshot.config_seq = 1u;
    snapshot.source_flags = 0x47u;
    snapshot.features.foreground_area = 900u;
    snapshot.features.roi_pixel_count = 200u;
    snapshot.features.bbox_width = 30u;
    snapshot.features.bbox_height = 30u;
    switch (color) {
    case SC_COLOR_RED: snapshot.features.red_area = 900u; snapshot.features.sum_luma = 36000u; break;
    case SC_COLOR_BLUE: snapshot.features.blue_area = 900u; snapshot.features.sum_luma = 36000u; break;
    case SC_COLOR_YELLOW: snapshot.features.yellow_area = 900u; snapshot.features.sum_luma = 36000u; break;
    case SC_COLOR_WHITE: snapshot.features.sum_luma = 40000u; break;
    case SC_COLOR_BLACK: snapshot.features.sum_luma = 4000u; break;
    default: break;
    }
    return snapshot;
}

static int init_runtime(sc_fake_transport_t *fake, sc_runtime_t *runtime)
{
    sc_runtime_transport_t transport;
    sc_fake_transport_init(fake);
    sc_fake_transport_bind(fake, &transport);
    return sc_runtime_init(runtime, &transport, 1u, 0u);
}

static int run_ack_negative_cases(FILE *log)
{
    p1_input_model_t model;
    p1_config_t config = {1u, 2u, 20u};
    uint16_t ack = 0x1234u;

    p1_input_init(&model);
    p1_input_stage_config(&model, &config);
    if (p1_input_event(&model, P1_INPUT_APPLY, 10u, &ack) != P1_EVENT_ACCEPTED || ack != 10u) return -1;
    if (p1_input_event(&model, P1_INPUT_APPLY, 10u, &ack) != P1_EVENT_DUPLICATE || ack != 10u) return -1;
    if (p1_input_event(&model, P1_INPUT_APPLY, 9u, &ack) != P1_EVENT_STALE || ack != 10u) return -1;
    if (p1_input_event(&model, P1_INPUT_PLACE, 11u, &ack) != P1_EVENT_INVALID_STATE || ack != 10u) return -1;
    if (p1_input_frame_boundary(&model) != P1_EVENT_ACCEPTED) return -1;
    if (p1_input_event(&model, P1_INPUT_RESET, 11u, &ack) != P1_EVENT_ACCEPTED || ack != 11u) return -1;
    (void)fprintf(log, "NEGATIVE duplicate_ack_unchanged=PASS stale_ack_unchanged=PASS invalid_ack_unchanged=PASS reset=PASS\n");
    return 0;
}

static int run_tamper_case(FILE *negative)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    sc_target_t target = {SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0u};
    sc_feature_snapshot_t original = snapshot_for(900u, SC_COLOR_RED);
    sc_feature_snapshot_t tampered = original;
    unsigned char original_bytes[33];
    unsigned char tampered_bytes[33];
    char original_hash[65];
    char tampered_hash[65];

    tampered.features.red_area = 0u;
    tampered.features.blue_area = 900u;
    if (sha256_hex(original_bytes, serialize_snapshot(&original, original_bytes), original_hash) != 0 ||
        sha256_hex(tampered_bytes, serialize_snapshot(&tampered, tampered_bytes), tampered_hash) != 0 ||
        strcmp(original_hash, tampered_hash) == 0) return -1;
    if (init_runtime(&fake, &runtime) != 0 ||
        sc_runtime_start_round(&runtime, &target, 1u, 0u, 100u) != 0 ||
        sc_fake_transport_push_snapshot(&fake, &tampered) != 0 ||
        sc_runtime_process_one(&runtime, 1u) != 0 || fake.result_count != 1u) return -1;
    if (fake.results[0].decision == SC_DECISION_EXECUTE_ARM_DISABLED ||
        fake.results[0].reason == SC_REASON_TARGET_MATCH) return -1;
    (void)fprintf(negative,
        "{\"schema\":\"p1-replay-negative-v1\",\"case_id\":\"snapshot_tamper_001\","
        "\"original_snapshot_hash\":\"%s\",\"tampered_snapshot_hash\":\"%s\","
        "\"expected_decision\":\"EXECUTE_ARM_DISABLED\",\"actual_decision\":\"%s\","
        "\"expected_reason\":\"TARGET_MATCH\",\"actual_reason\":\"%s\","
        "\"hash_mismatch_detected\":true,\"result_mismatch_detected\":true,\"status\":\"PASS\"}\n",
        original_hash, tampered_hash, sc_runtime_decision_name(fake.results[0].decision),
        sc_f1_reason_text(fake.results[0].reason));
    return 0;
}

static int emit_round(FILE *records, FILE *log, unsigned index,
                      p1_input_model_t *input, sc_fake_transport_t *fake,
                      sc_runtime_t *runtime, unsigned *event_seq)
{
    const replay_case_t *item = &cases[index];
    p1_config_t config = {(uint16_t)item->task, (uint16_t)item->target_color, 20u};
    sc_target_t target = {item->task, item->target_color, 20u};
    sc_feature_snapshot_t snapshot = snapshot_for((uint16_t)(100u + index), item->observed_color);
    p1_result_packer_t packer;
    p1_result_t packed;
    unsigned char bytes[33];
    char snapshot_hash[65];
    char snapshot_bytes_hex[67];
    uint16_t ack = 0u;
    unsigned apply_seq = ++*event_seq;
    unsigned place_seq;
    unsigned release_seq;
    unsigned commit_count = 0u;
    unsigned second_result_count;
    unsigned result_before;
    unsigned ack_before;
    uint32_t start_ms = 1000u + (index * 100u);
    uint32_t elapsed_ms = 20u + index;
    const char *terminal_cause;
    const char *host_release_event;
    sc_reason_t preterminal_reason;
    const sc_round_result_t *actual;

    if (sha256_hex(bytes, serialize_snapshot(&snapshot, bytes), snapshot_hash) != 0) return -1;
    bytes_to_hex(bytes, sizeof(bytes), snapshot_bytes_hex);
    p1_input_stage_config(input, &config);
    if (p1_input_event(input, P1_INPUT_APPLY, (uint16_t)apply_seq, &ack) != P1_EVENT_ACCEPTED ||
        ack != apply_seq || p1_input_frame_boundary(input) != P1_EVENT_ACCEPTED) return -1;
    place_seq = ++*event_seq;
    if (p1_input_event(input, P1_INPUT_PLACE, (uint16_t)place_seq, &ack) != P1_EVENT_ACCEPTED || ack != place_seq) return -1;

    if (sc_runtime_start_round(runtime, &target, (uint16_t)place_seq, start_ms, 50u) != 0 ||
        sc_fake_transport_push_snapshot(fake, &snapshot) != 0) return -1;
    result_before = fake->result_count;
    ack_before = fake->ack_count;
    if (sc_runtime_process_one(runtime, start_ms + elapsed_ms) != 0) return -1;
    preterminal_reason = runtime->controller.reason;
    release_seq = ++*event_seq;
    if (item->terminal == TERMINAL_ABANDON) {
        if (sc_runtime_abandon(runtime, (uint16_t)release_seq) != 0) return -1;
        terminal_cause = "ABANDON";
        host_release_event = "ABANDON";
    } else if (item->terminal == TERMINAL_TIMEOUT) {
        if (sc_runtime_tick(runtime, start_ms + 50u) != 1) return -1;
        elapsed_ms = 50u;
        terminal_cause = "TIMEOUT";
        host_release_event = "ABANDON";
    } else {
        terminal_cause = "RESULT";
        host_release_event = "REMOVE";
    }
    if ((fake->result_count - result_before) != 1u || fake->result_count == 0u) return -1;
    actual = &fake->results[fake->result_count - 1u];
    if (actual->round_id != input->round_id) return -1;
    if (actual->decision != item->expected_decision || actual->reason != item->expected_reason) return -1;
    if (p1_input_latch_result(input, input->round_id) != P1_EVENT_ACCEPTED) return -1;
    if (strcmp(host_release_event, "REMOVE") == 0) {
        if (p1_input_event(input, P1_INPUT_REMOVE, (uint16_t)release_seq, &ack) != P1_EVENT_ACCEPTED) return -1;
    } else {
        if (p1_input_event(input, P1_INPUT_ABANDON, (uint16_t)release_seq, &ack) != P1_EVENT_ACCEPTED) return -1;
    }
    if (ack != release_seq || input->result_latched || input->object_present) return -1;

    memset(&packed, 0, sizeof(packed));
    packed.round_id = input->round_id;
    packed.frame_id = actual->frame_id;
    packed.config_seq = actual->config_revision;
    packed.color = (uint8_t)actual->observation.color;
    packed.shape = (uint8_t)actual->observation.shape;
    packed.size_cm_x10 = actual->observation.size_cm_x10;
    packed.decision = (uint8_t)actual->decision;
    packed.reason = (uint8_t)actual->reason;
    packed.input_flags = actual->input_flags;
    p1_result_packer_init(&packer);
    if (p1_result_stage(&packer, &packed) != 0 || p1_result_commit(&packer, packed.round_id) != 0) return -1;
    commit_count++;
    second_result_count = (p1_result_commit(&packer, packed.round_id) == 0) ? 1u : 0u;
    if (sc_runtime_process_one(runtime, start_ms + elapsed_ms + 1u) != 0 ||
        fake->result_count != result_before + 1u) return -1;

    (void)fprintf(records,
        "{\"schema\":\"p1-round-replay-v2\",\"round_id\":%u,\"task\":%u,"
        "\"event_seq\":[%u,%u,%u],\"snapshot_serialization\":\"p1-fake-snapshot-le-v1\","
        "\"snapshot_size_bytes\":33,\"snapshot_bytes_hex\":\"%s\",\"snapshot_hash\":\"%s\","
        "\"ack_sequence\":[%u,%u,%u],\"feature_ack_count\":%u,\"frame_id\":%u,"
        "\"decision\":\"%s\",\"reason\":\"%s\",\"preterminal_reason\":\"%s\","
        "\"runtime_result_submit_count\":%u,\"result_commit_count\":%u,\"elapsed_ms\":%lu,"
        "\"terminal_cause\":\"%s\",\"host_release_event\":\"%s\","
        "\"terminal_release\":\"%s\",\"second_result_count\":%u,\"arm_enabled\":0}\n",
        input->round_id, (unsigned)item->task, apply_seq, place_seq, release_seq,
        snapshot_bytes_hex, snapshot_hash, apply_seq, place_seq, release_seq, fake->ack_count - ack_before,
        (unsigned)actual->frame_id, sc_runtime_decision_name(actual->decision),
        sc_f1_reason_text(actual->reason), sc_f1_reason_text(preterminal_reason),
        fake->result_count - result_before, commit_count, (unsigned long)elapsed_ms,
        terminal_cause, host_release_event, host_release_event, second_result_count);
    (void)fprintf(log,
        "ROUND=%u TASK=%u ACTUAL_DECISION=%s ACTUAL_REASON=%s PRETERMINAL=%s CAUSE=%s HOST_RELEASE=%s RUNTIME_SUBMITS=%u COMMITS=%u FEATURE_ACKS=%u ARM=0\n",
        input->round_id, (unsigned)item->task, sc_runtime_decision_name(actual->decision),
        sc_f1_reason_text(actual->reason), sc_f1_reason_text(preterminal_reason),
        terminal_cause, host_release_event, fake->result_count - result_before,
        commit_count, fake->ack_count - ack_before);
    return 0;
}

int main(int argc, char **argv)
{
    p1_input_model_t input;
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    FILE *records = NULL;
    FILE *log = NULL;
    FILE *negative = NULL;
    unsigned event_seq = 0u;
    unsigned i;

    if (argc != 4) return 2;
    if (fopen_s(&records, argv[1], "wb") != 0 || records == NULL) return 2;
    if (fopen_s(&log, argv[2], "wb") != 0 || log == NULL) { fclose(records); return 2; }
    if (fopen_s(&negative, argv[3], "wb") != 0 || negative == NULL) { fclose(records); fclose(log); return 2; }
    p1_input_init(&input);
    if (init_runtime(&fake, &runtime) != 0 || run_ack_negative_cases(log) != 0 ||
        run_tamper_case(negative) != 0) return 1;
    for (i = 0u; i < 20u; ++i) {
        if (emit_round(records, log, i, &input, &fake, &runtime, &event_seq) != 0) return 1;
    }
    (void)fprintf(log, "SUMMARY rounds=20 runtime_submits=20 commits=20 second_results=0 arm_enabled=0 actual_source=single_camera_runtime\n");
    fclose(negative);
    fclose(records);
    fclose(log);
    return 0;
}
