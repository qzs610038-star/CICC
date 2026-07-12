/* Full 20-round host flow. This is not firmware or a board-level test. */
#include <stdio.h>
#include <string.h>
#include "competition_host_adapter.h"

static int failures;
static int checks;
#define CHECK(expr) do { checks++; if (!(expr)) { failures++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } } while (0)

static target_config_t config_for(competition_task_mode_t mode, uint8_t color,
                                  uint8_t reference_size)
{
    target_config_t config;
    memset(&config, 0, sizeof(config));
    config.valid = 1u;
    config.target.mode = mode;
    config.target.target_color = color;
    config.target.reference_size_cm_x10 = reference_size;
    return config;
}

static vision_result_t observation(uint8_t color, uint8_t shape)
{
    vision_result_t value;
    memset(&value, 0, sizeof(value));
    value.color_id = color;
    value.shape_id = shape;
    value.confidence = 200u;
    return value;
}

static void run_final_round(competition_host_adapter_t *adapter,
                            uint16_t *seq, const vision_result_t *obs,
                            competition_decision_t expected_decision,
                            competition_reason_t expected_reason)
{
    uint16_t place_seq = (*seq)++;
    uint16_t remove_seq;
    const result_status_t *status;

    CHECK(competition_host_adapter_event(adapter, COMP_EVENT_PLACE, place_seq) == 0);
    /* A repeated PLACE is a debounced retransmission, never a second round. */
    CHECK(competition_host_adapter_event(adapter, COMP_EVENT_PLACE, place_seq) == 0);
    CHECK(competition_host_adapter_observe(adapter, obs) == 0);
    status = competition_host_adapter_status(adapter);
    CHECK(status->event_seq == place_seq);
    CHECK(status->decision == expected_decision && status->reason == expected_reason);
    CHECK(competition_host_adapter_ack(adapter, (uint16_t)(place_seq + 1u)) == -1);
    CHECK(competition_host_adapter_ack(adapter, place_seq) == 0);
    remove_seq = (*seq)++;
    CHECK(competition_host_adapter_event(adapter, COMP_EVENT_REMOVE, remove_seq) == 0);
    competition_host_adapter_advance(adapter, 1u);
}

static void run_size_deferred_round(competition_host_adapter_t *adapter,
                                    uint16_t *seq, const vision_result_t *obs)
{
    uint16_t place_seq = (*seq)++;
    uint16_t abandon_seq;
    const result_status_t *status;

    CHECK(competition_host_adapter_event(adapter, COMP_EVENT_PLACE, place_seq) == 0);
    CHECK(competition_host_adapter_observe(adapter, obs) == 0);
    status = competition_host_adapter_status(adapter);
    CHECK(status->decision == COMP_DECISION_WAIT);
    CHECK(status->reason == COMP_REASON_SIZE_UNAVAILABLE);
    CHECK(status->size_state == SIZE_STATE_UNAVAILABLE && status->size_cm_x10 == 0u);
    abandon_seq = (*seq)++;
    CHECK(competition_host_adapter_event(adapter, COMP_EVENT_ABANDON, abandon_seq) == 0);
    status = competition_host_adapter_status(adapter);
    CHECK(status->decision == COMP_DECISION_SKIP && status->reason == COMP_REASON_OPERATOR_ABANDONED);
    CHECK(competition_host_adapter_event(adapter, COMP_EVENT_REMOVE, (*seq)++) == 0);
    competition_host_adapter_advance(adapter, 1u);
}

int main(void)
{
    competition_host_adapter_t adapter;
    uint16_t seq = 1u;
    unsigned int i;
    target_config_t task1 = config_for(COMP_TASK_COLOR_CUBE, COLOR_RED, 0);
    target_config_t task2 = config_for(COMP_TASK_SHAPE_COLOR_CUBE, COLOR_BLUE, 0);
    target_config_t task3 = config_for(COMP_TASK_SIZE_DELTA_1CM_CUBE, 0, 20);
    target_config_t task4 = config_for(COMP_TASK_SIZE_WITHIN_0P5CM_CUBE, 0, 30);
    vision_result_t red_cube = observation(COLOR_RED, SHAPE_CUBE);
    vision_result_t yellow_cube = observation(COLOR_YELLOW, SHAPE_CUBE);
    vision_result_t blue_cube = observation(COLOR_BLUE, SHAPE_CUBE);
    vision_result_t blue_cone = observation(COLOR_BLUE, SHAPE_CONE);
    vision_result_t any_cube = observation(COLOR_WHITE, SHAPE_CUBE);

    competition_host_adapter_init(&adapter, SIZE_STATE_UNAVAILABLE, 100u);

    CHECK(competition_host_adapter_configure(&adapter, &task1) == 0);
    for (i = 0; i < 5u; ++i) {
        run_final_round(&adapter, &seq, i == 0u ? &red_cube : &yellow_cube,
                        i == 0u ? COMP_DECISION_EXECUTE : COMP_DECISION_SKIP,
                        i == 0u ? COMP_REASON_TARGET_MATCH : COMP_REASON_COLOR_MISMATCH);
    }

    CHECK(competition_host_adapter_configure(&adapter, &task2) == 0);
    for (i = 0; i < 5u; ++i) {
        run_final_round(&adapter, &seq, i == 0u ? &blue_cube : &blue_cone,
                        i == 0u ? COMP_DECISION_EXECUTE : COMP_DECISION_SKIP,
                        i == 0u ? COMP_REASON_TARGET_MATCH : COMP_REASON_SHAPE_MISMATCH);
    }

    CHECK(competition_host_adapter_configure(&adapter, &task3) == 0);
    for (i = 0; i < 5u; ++i) {
        run_size_deferred_round(&adapter, &seq, &any_cube);
    }

    CHECK(competition_host_adapter_configure(&adapter, &task4) == 0);
    for (i = 0; i < 5u; ++i) {
        run_size_deferred_round(&adapter, &seq, &any_cube);
    }

    printf("competition_host_flow: %d/%d passed (20 rounds, size deferred)\n",
           checks - failures, checks);
    return failures ? 1 : 0;
}
