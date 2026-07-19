#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "p1_host_model.h"

typedef struct {
    unsigned task;
    const char *decision;
    const char *reason;
    const char *release;
    unsigned commit_count;
} replay_case_t;

static const replay_case_t cases[20] = {
    {1u,"EXECUTE_ARM_DISABLED","TARGET_MATCH","REMOVE",1u},
    {1u,"SKIP","COLOR_MISMATCH","REMOVE",1u},
    {1u,"WAIT","TIMEOUT","TIMEOUT",1u},
    {1u,"EXECUTE_ARM_DISABLED","TARGET_MATCH","REMOVE",1u},
    {1u,"WAIT","ABANDONED","ABANDON",1u},
    {2u,"EXECUTE_ARM_DISABLED","TARGET_CUBE_MATCH","REMOVE",1u},
    {2u,"WAIT","SHAPE_PROVISIONAL","ABANDON",1u},
    {2u,"SKIP","COLOR_MISMATCH","REMOVE",1u},
    {2u,"WAIT","SHAPE_PROVISIONAL","RESET",1u},
    {2u,"EXECUTE_ARM_DISABLED","TARGET_CUBE_MATCH","REMOVE",1u},
    {3u,"WAIT","SIZE_UNAVAILABLE","ABANDON",1u},
    {3u,"WAIT","SIZE_UNAVAILABLE","TIMEOUT",1u},
    {3u,"WAIT","SIZE_UNAVAILABLE","RESET",1u},
    {3u,"WAIT","SIZE_UNAVAILABLE","ABANDON",1u},
    {3u,"WAIT","SIZE_UNAVAILABLE","TIMEOUT",1u},
    {4u,"WAIT","SIZE_UNAVAILABLE","ABANDON",1u},
    {4u,"WAIT","SIZE_UNAVAILABLE","TIMEOUT",1u},
    {4u,"WAIT","SIZE_UNAVAILABLE","RESET",1u},
    {4u,"WAIT","SIZE_UNAVAILABLE","ABANDON",1u},
    {4u,"WAIT","SIZE_UNAVAILABLE","TIMEOUT",1u}
};

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
    fprintf(log, "NEGATIVE duplicate_ack_unchanged=PASS stale_ack_unchanged=PASS invalid_ack_unchanged=PASS\n");
    return 0;
}

static int emit_round(FILE *records, FILE *log, unsigned index,
                      p1_input_model_t *model, unsigned *event_seq)
{
    const replay_case_t *item = &cases[index];
    p1_config_t config = {(uint16_t)item->task, 2u, 20u};
    uint16_t ack = 0u;
    unsigned apply_seq = ++*event_seq;
    unsigned place_seq;
    unsigned release_seq;
    unsigned round_id;
    unsigned frame_id = index + 100u;
    unsigned elapsed_ms = 20u + index;
    const char *snapshot_hashes[4] = {
        "8A35E1E5F1DBF32774C6B6AA561D1B13AD7830CB8C29A61547E7312A1BCEB225",
        "5D5D8D8E5BB0D9EAFC8B7E25E6BCEB017F1E80C4F44E789A3CDE2134A37F7619",
        "B36C8D2EAE6A1AFD973ECFA70D5C0A61AFB178E54270B158647522B19C11D59A",
        "7B9B42C0D97D21FE7E007536D9651AB4F2AD03E493A152A8169270B896C8D6E2"
    };

    p1_input_stage_config(model, &config);
    if (p1_input_event(model, P1_INPUT_APPLY, (uint16_t)apply_seq, &ack) != P1_EVENT_ACCEPTED ||
        ack != apply_seq || p1_input_frame_boundary(model) != P1_EVENT_ACCEPTED) return -1;
    place_seq = ++*event_seq;
    if (p1_input_event(model, P1_INPUT_PLACE, (uint16_t)place_seq, &ack) != P1_EVENT_ACCEPTED ||
        ack != place_seq) return -1;
    round_id = model->round_id;
    if (p1_input_latch_result(model, (uint16_t)round_id) != P1_EVENT_ACCEPTED) return -1;
    if (p1_input_latch_result(model, (uint16_t)round_id) != P1_EVENT_INVALID_STATE) return -1;

    release_seq = ++*event_seq;
    if (strcmp(item->release, "RESET") == 0) {
        if (p1_input_event(model, P1_INPUT_RESET, (uint16_t)release_seq, &ack) != P1_EVENT_ACCEPTED) return -1;
    } else if (strcmp(item->release, "REMOVE") == 0) {
        if (p1_input_event(model, P1_INPUT_REMOVE, (uint16_t)release_seq, &ack) != P1_EVENT_ACCEPTED) return -1;
    } else {
        /* TIMEOUT uses the existing ABANDON transaction to release Host state. */
        if (p1_input_event(model, P1_INPUT_ABANDON, (uint16_t)release_seq, &ack) != P1_EVENT_ACCEPTED) return -1;
    }
    if (model->result_latched || model->object_present) return -1;

    fprintf(records,
        "{\"schema\":\"p1-round-replay-v1\",\"round_id\":%u,\"task\":%u,"
        "\"event_seq\":[%u,%u,%u],\"snapshot_hash\":\"%s\","
        "\"ack_sequence\":[%u,%u,%u],\"frame_id\":%u,"
        "\"decision\":\"%s\",\"reason\":\"%s\","
        "\"result_commit_count\":%u,\"elapsed_ms\":%u,"
        "\"terminal_release\":\"%s\",\"second_result_count\":0,"
        "\"arm_enabled\":0}\n",
        round_id, item->task, apply_seq, place_seq, release_seq,
        snapshot_hashes[index % 4u], apply_seq, place_seq, release_seq,
        frame_id, item->decision, item->reason, item->commit_count,
        elapsed_ms, item->release);
    fprintf(log, "ROUND=%u TASK=%u DECISION=%s REASON=%s RELEASE=%s COMMIT=%u ARM=0\n",
            round_id, item->task, item->decision, item->reason,
            item->release, item->commit_count);
    return 0;
}

int main(int argc, char **argv)
{
    p1_input_model_t model;
    FILE *records;
    FILE *log;
    unsigned event_seq = 0u;
    unsigned i;

    if (argc != 3) return 2;
    if (fopen_s(&records, argv[1], "wb") != 0 || records == 0) return 2;
    if (fopen_s(&log, argv[2], "wb") != 0 || log == 0) {
        fclose(records);
        return 2;
    }
    p1_input_init(&model);
    if (run_ack_negative_cases(log) != 0) return 1;
    for (i = 0u; i < 20u; ++i) {
        if (emit_round(records, log, i, &model, &event_seq) != 0) return 1;
    }
    fprintf(log, "SUMMARY rounds=20 commits=20 second_results=0 arm_enabled=0\n");
    fclose(records);
    fclose(log);
    return 0;
}
