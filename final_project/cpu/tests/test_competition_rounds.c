/* Host tests for official task rules and one-round transaction semantics. */
#include <stdio.h>
#include <string.h>
#include "competition_tasks.h"
#include "competition_round_transaction.h"

static int failures;
static int checks;

#define CHECK(expr) do { checks++; if (!(expr)) { failures++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } } while (0)

static vision_result_t obs(uint8_t color, uint8_t shape, uint8_t size)
{
    vision_result_t value;
    memset(&value, 0, sizeof(value));
    value.color_id = color;
    value.shape_id = shape;
    value.size_cm_x10 = size;
    value.confidence = 200u;
    return value;
}

static competition_target_t target(competition_task_mode_t mode, uint8_t color,
                                   uint8_t reference_size)
{
    competition_target_t value;
    value.mode = mode;
    value.target_color = color;
    value.reference_size_cm_x10 = reference_size;
    return value;
}

static void test_task_rules(void)
{
    competition_evaluation_t value;
    competition_target_t t1 = target(COMP_TASK_COLOR_CUBE, COLOR_WHITE, 0);
    competition_target_t t2 = target(COMP_TASK_SHAPE_COLOR_CUBE, COLOR_BLACK, 0);
    competition_target_t t3 = target(COMP_TASK_SIZE_DELTA_1CM_CUBE, 0, 20);
    competition_target_t t4 = target(COMP_TASK_SIZE_WITHIN_0P5CM_CUBE, 0, 30);
    vision_result_t white_cube = obs(COLOR_WHITE, SHAPE_CUBE, 20);
    vision_result_t red_cube = obs(COLOR_RED, SHAPE_CUBE, 20);
    vision_result_t black_cylinder = obs(COLOR_BLACK, SHAPE_CYLINDER, 20);
    vision_result_t cube_30 = obs(COLOR_RED, SHAPE_CUBE, 30);
    vision_result_t cube_25 = obs(COLOR_RED, SHAPE_CUBE, 25);
    vision_result_t blue_cube_25 = obs(COLOR_BLUE, SHAPE_CUBE, 25);
    vision_result_t blue_cube_20 = obs(COLOR_BLUE, SHAPE_CUBE, 20);
    vision_result_t blue_cube_unknown_size = obs(COLOR_BLUE, SHAPE_CUBE, 0);

    value = competition_evaluate(&t1, &white_cube);
    CHECK(value.decision == COMP_DECISION_EXECUTE);
    value = competition_evaluate(&t1, &red_cube);
    CHECK(value.decision == COMP_DECISION_SKIP && value.reason == COMP_REASON_COLOR_MISMATCH);
    value = competition_evaluate(&t2, &black_cylinder);
    CHECK(value.decision == COMP_DECISION_SKIP && value.reason == COMP_REASON_SHAPE_MISMATCH);
    value = competition_evaluate(&t3, &cube_30);
    CHECK(value.decision == COMP_DECISION_EXECUTE);
    value = competition_evaluate(&t3, &cube_25);
    CHECK(value.decision == COMP_DECISION_SKIP && value.reason == COMP_REASON_SIZE_RELATION_MISMATCH);
    value = competition_evaluate(&t4, &blue_cube_25);
    CHECK(value.decision == COMP_DECISION_EXECUTE);
    value = competition_evaluate(&t4, &blue_cube_20);
    CHECK(value.decision == COMP_DECISION_SKIP && value.reason == COMP_REASON_SIZE_RELATION_MISMATCH);
    value = competition_evaluate(&t4, &blue_cube_unknown_size);
    CHECK(value.decision == COMP_DECISION_WAIT && value.reason == COMP_REASON_OBSERVATION_UNSTABLE);
}

static void test_twenty_round_mock(void)
{
    competition_round_txn_t controller;
    uint16_t seq;
    unsigned int i;
    const competition_target_t targets[4] = {
        { COMP_TASK_COLOR_CUBE, COLOR_RED, 0 },
        { COMP_TASK_SHAPE_COLOR_CUBE, COLOR_BLUE, 0 },
        { COMP_TASK_SIZE_DELTA_1CM_CUBE, 0, 20 },
        { COMP_TASK_SIZE_WITHIN_0P5CM_CUBE, 0, 30 }
    };
    const vision_result_t target_obs[4] = {
        { COLOR_RED, SHAPE_CUBE, 20, 200 },
        { COLOR_BLUE, SHAPE_CUBE, 25, 200 },
        { COLOR_YELLOW, SHAPE_CUBE, 30, 200 },
        { COLOR_BLACK, SHAPE_CUBE, 25, 200 }
    };

    competition_round_txn_init(&controller);
    for (i = 0; i < 20u; ++i) {
        competition_evaluation_t first;
        competition_evaluation_t repeated;
        uint32_t now_ms = i * 100u;
        unsigned int task_index = i / 5u;
        int should_execute = (i % 5u) == 0u;

        seq = (uint16_t)(i + 1u);
        CHECK(competition_round_txn_start(&controller, &targets[task_index],
                                         seq, now_ms, 50u) == 0);
        if (should_execute) {
            first = competition_round_txn_observe(
                &controller, &target_obs[task_index], now_ms + 1u);
            CHECK(first.decision == COMP_DECISION_EXECUTE);
        } else {
            vision_result_t non_target = target_obs[task_index];
            if (task_index < 2u) non_target.color_id = COLOR_YELLOW;
            else non_target.size_cm_x10 = (task_index == 3u) ? 20u : 25u;
            first = competition_round_txn_observe(&controller, &non_target,
                                                  now_ms + 1u);
            CHECK(first.decision == COMP_DECISION_SKIP);
        }
        {
            vision_result_t later_observation = obs(COLOR_WHITE, SHAPE_CUBE, 20);
            repeated = competition_round_txn_observe(
                &controller, &later_observation, now_ms + 2u);
        }
        CHECK(repeated.decision == COMP_DECISION_WAIT);
        CHECK(competition_round_txn_ack(&controller,
                                        (uint16_t)(seq + 1u)) == -1);
        CHECK(competition_round_txn_ack(&controller, seq) == 0);
        CHECK(controller.state == COMP_ROUND_TXN_STATE_COMPLETE);
    }
}

static void test_timeout_abandon_and_reset(void)
{
    competition_round_txn_t controller;
    competition_target_t t = target(COMP_TASK_COLOR_CUBE, COLOR_RED, 0);

    competition_round_txn_init(&controller);
    CHECK(competition_round_txn_start(&controller, &t, 7u, 100u, 10u) == 0);
    CHECK(competition_round_txn_tick(&controller, 110u) == 1);
    CHECK(controller.state == COMP_ROUND_TXN_STATE_TIMEOUT);
    CHECK(competition_round_txn_start(&controller, &t, 8u, 200u, 10u) == 0);
    CHECK(competition_round_txn_abandon(&controller) == 0);
    CHECK(controller.state == COMP_ROUND_TXN_STATE_ABANDONED);
    competition_round_txn_soft_reset(&controller);
    CHECK(controller.state == COMP_ROUND_TXN_STATE_IDLE &&
          controller.event_seq == 0u);
}

int main(void)
{
    test_task_rules();
    test_twenty_round_mock();
    test_timeout_abandon_and_reset();
    printf("competition_rounds: %d/%d passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
