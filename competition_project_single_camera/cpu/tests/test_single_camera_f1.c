#include <stdio.h>
#include <string.h>

#include "single_camera_f1.h"

static int checks;
static int failures;

#define CHECK(expr) do { checks++; if (!(expr)) { failures++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } } while (0)

static sc_target_t target(sc_task_t task, sc_color_t color, uint8_t reference)
{
    sc_target_t value = { task, color, reference };
    return value;
}

static sc_observation_t observation(sc_color_t color, sc_shape_t shape, uint8_t size)
{
    sc_observation_t value = { color, shape, size, 1u };
    return value;
}

static void test_place_latch_and_next_place(void)
{
    sc_f1_controller_t controller;
    sc_observation_t red_cube = observation(SC_COLOR_RED, SC_SHAPE_CUBE, 0u);
    sc_observation_t blue_cube = observation(SC_COLOR_BLUE, SC_SHAPE_CUBE, 0u);
    sc_target_t red_cube_target = target(SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0u);

    sc_f1_init(&controller, 0u);
    CHECK(sc_f1_apply_target(&controller, &red_cube_target) == 0);
    CHECK(sc_f1_place(&controller, 1u, 0u, 100u) == 0);
    CHECK(sc_f1_place(&controller, 1u, 1u, 100u) == 0);
    CHECK(controller.round_seq == 1u && controller.state == SC_STATE_ACQUIRING);
    CHECK(sc_f1_observe(&controller, &red_cube, 2u) == 0);
    CHECK(controller.result_valid && controller.decision == SC_DECISION_EXECUTE_ARM_DISABLED);
    CHECK(controller.reason == SC_REASON_TARGET_MATCH && controller.state == SC_STATE_RESULT_LATCHED);
    CHECK(sc_f1_observe(&controller, &blue_cube, 3u) == -1);
    CHECK(sc_f1_place(&controller, 2u, 4u, 100u) == 0);
    CHECK(controller.round_seq == 2u && !controller.result_valid);
    CHECK(sc_f1_observe(&controller, &blue_cube, 5u) == 0);
    CHECK(controller.decision == SC_DECISION_SKIP && controller.reason == SC_REASON_COLOR_MISMATCH);
}

static void test_shape_size_and_abandon(void)
{
    sc_f1_controller_t controller;
    sc_observation_t blue_cone = observation(SC_COLOR_BLUE, SC_SHAPE_CONE, 0u);
    sc_observation_t cube = observation(SC_COLOR_BLUE, SC_SHAPE_CUBE, 30u);
    sc_target_t blue_cube_target = target(SC_TASK_SHAPE_COLOR_CUBE, SC_COLOR_BLUE, 0u);
    sc_target_t size_delta_target = target(SC_TASK_SIZE_DELTA_1CM_CUBE,
                                           SC_COLOR_UNKNOWN, 20u);

    sc_f1_init(&controller, 0u);
    CHECK(sc_f1_apply_target(&controller, &blue_cube_target) == 0);
    CHECK(sc_f1_place(&controller, 3u, 0u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &blue_cone, 1u) == 0);
    CHECK(controller.decision == SC_DECISION_SKIP && controller.reason == SC_REASON_SHAPE_MISMATCH);

    CHECK(sc_f1_apply_target(&controller, &size_delta_target) == 0);
    CHECK(sc_f1_place(&controller, 4u, 2u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &cube, 3u) == 0);
    CHECK(!controller.result_valid && controller.reason == SC_REASON_SIZE_UNAVAILABLE);
    CHECK(sc_f1_abandon(&controller, 5u) == 0);
    CHECK(controller.result_valid && controller.reason == SC_REASON_OPERATOR_ABANDONED);
}

static void test_timeout_and_twenty_rounds(void)
{
    sc_f1_controller_t controller;
    sc_observation_t red_cube = observation(SC_COLOR_RED, SC_SHAPE_CUBE, 0u);
    sc_target_t red_cube_target = target(SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0u);
    unsigned int i;

    sc_f1_init(&controller, 0u);
    CHECK(sc_f1_apply_target(&controller, &red_cube_target) == 0);
    CHECK(sc_f1_place(&controller, 6u, 0u, 10u) == 0);
    CHECK(sc_f1_tick(&controller, 10u) == 1);
    CHECK(controller.state == SC_STATE_TIMEOUT && controller.reason == SC_REASON_TIMEOUT);

    for (i = 0u; i < 20u; ++i) {
        uint16_t seq = (uint16_t)(10u + i);
        CHECK(sc_f1_place(&controller, seq, 100u + i, 20u) == 0);
        CHECK(sc_f1_observe(&controller, &red_cube, 101u + i) == 0);
        CHECK(controller.state == SC_STATE_RESULT_LATCHED);
    }
    CHECK(controller.round_seq == 21u);
}

int main(void)
{
    test_place_latch_and_next_place();
    test_shape_size_and_abandon();
    test_timeout_and_twenty_rounds();
    printf("single_camera_f1: %d/%d passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
