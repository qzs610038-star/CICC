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
    sc_observation_t blue_cube = observation(SC_COLOR_BLUE, SC_SHAPE_CUBE, 0u);
    sc_observation_t cube = observation(SC_COLOR_BLUE, SC_SHAPE_CUBE, 30u);
    sc_target_t blue_cube_target = target(SC_TASK_SHAPE_COLOR_CUBE, SC_COLOR_BLUE, 0u);
    sc_target_t size_delta_target = target(SC_TASK_SIZE_DELTA_1CM_CUBE,
                                           SC_COLOR_UNKNOWN, 20u);

    sc_f1_init(&controller, 0u);
    CHECK(sc_f1_apply_target(&controller, &blue_cube_target) == 0);

    /* Round 1: blue cone (WAIT), then blue cube (EXECUTE) in same round. */
    CHECK(sc_f1_place(&controller, 3u, 0u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &blue_cone, 1u) == 0);
    CHECK(!controller.result_valid);
    CHECK(controller.decision == SC_DECISION_WAIT);
    CHECK(sc_f1_observe(&controller, &blue_cube, 2u) == 0);
    CHECK(controller.decision == SC_DECISION_EXECUTE_ARM_DISABLED);
    CHECK(controller.reason == SC_REASON_TARGET_MATCH);

    /* Round 2: SIZE_DELTA target + size_available=0 → SIZE_UNAVAILABLE → ABANDON. */
    CHECK(sc_f1_apply_target(&controller, &size_delta_target) == 0);
    CHECK(sc_f1_place(&controller, 4u, 3u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &cube, 4u) == 0);
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

static void test_event_seq_wraparound(void)
{
    sc_f1_controller_t controller;
    sc_observation_t red_cube = observation(SC_COLOR_RED, SC_SHAPE_CUBE, 0u);
    sc_target_t red_cube_target = target(SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0u);

    sc_f1_init(&controller, 0u);
    CHECK(sc_f1_apply_target(&controller, &red_cube_target) == 0);
    /* First PLACE at seq 0xFFFE. */
    CHECK(sc_f1_place(&controller, 0xFFFEu, 0u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &red_cube, 1u) == 0);
    CHECK(controller.result_valid);
    /* Second PLACE at seq 0xFFFF — distinct, so new round. */
    CHECK(sc_f1_place(&controller, 0xFFFFu, 2u, 100u) == 0);
    CHECK(controller.round_seq == 2u);
    CHECK(sc_f1_observe(&controller, &red_cube, 3u) == 0);
    /* Third PLACE at seq 0x0000 after wraparound. */
    CHECK(sc_f1_place(&controller, 0x0000u, 4u, 100u) == 0);
    CHECK(controller.round_seq == 3u);
    CHECK(sc_f1_observe(&controller, &red_cube, 5u) == 0);
    /* Duplicate seq 0x0000 is suppressed. */
    CHECK(sc_f1_place(&controller, 0x0000u, 6u, 100u) == 0);
    CHECK(controller.round_seq == 3u);
}

static void test_task2_mixed_shape_pool_semantics(void)
{
    sc_f1_controller_t controller;
    sc_observation_t red_cube = observation(SC_COLOR_RED, SC_SHAPE_CUBE, 0u);
    sc_observation_t red_cyl = observation(SC_COLOR_RED, SC_SHAPE_CYLINDER, 0u);
    sc_observation_t blue_cube = observation(SC_COLOR_BLUE, SC_SHAPE_CUBE, 0u);
    sc_target_t task2_red = target(SC_TASK_SHAPE_COLOR_CUBE, SC_COLOR_RED, 0u);

    /* Task 2: find RED CUBE in mixed shape pool.
       Multiple observations per round — only reliable CUBE triggers a terminal decision. */
    sc_f1_init(&controller, 0u);
    CHECK(sc_f1_apply_target(&controller, &task2_red) == 0);

    /* Round 1: red cylinder (WAIT), then blue cube (SKIP) in same round. */
    CHECK(sc_f1_place(&controller, 1u, 0u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &red_cyl, 1u) == 0);
    CHECK(!controller.result_valid);
    CHECK(controller.decision == SC_DECISION_WAIT);
    CHECK(sc_f1_observe(&controller, &blue_cube, 2u) == 0);
    CHECK(controller.decision == SC_DECISION_SKIP);
    CHECK(controller.reason == SC_REASON_COLOR_MISMATCH);

    /* Round 2: red cube → EXECUTE_ARM_DISABLED. */
    CHECK(sc_f1_place(&controller, 2u, 3u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &red_cube, 4u) == 0);
    CHECK(controller.decision == SC_DECISION_EXECUTE_ARM_DISABLED);
    CHECK(controller.reason == SC_REASON_TARGET_MATCH);
}

static void test_synthetic_size_tasks_three_and_four(void)
{
    sc_f1_controller_t controller;
    /* Synthetic fixtures — size from Host, not real calibration. */
    sc_observation_t size20 = observation(SC_COLOR_RED, SC_SHAPE_CUBE, 20u);
    sc_observation_t size25 = observation(SC_COLOR_RED, SC_SHAPE_CUBE, 25u);
    sc_observation_t size30 = observation(SC_COLOR_RED, SC_SHAPE_CUBE, 30u);
    sc_observation_t size0  = observation(SC_COLOR_RED, SC_SHAPE_CUBE, 0u);
    sc_target_t t3_ref20 = target(SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_UNKNOWN, 20u);
    sc_target_t t3_ref30 = target(SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_UNKNOWN, 30u);
    sc_target_t t4_ref20 = target(SC_TASK_SIZE_WITHIN_0P5CM_CUBE, SC_COLOR_UNKNOWN, 20u);
    /* Task 3 with size_available=1: |obs - ref| == 10 mm.
       ref=20: 30 matches (30-20=10). 20 and 25 do not. */
    sc_f1_init(&controller, 1u);
    CHECK(sc_f1_apply_target(&controller, &t3_ref20) == 0);
    CHECK(sc_f1_place(&controller, 1u, 0u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &size30, 1u) == 0);
    CHECK(controller.decision == SC_DECISION_EXECUTE_ARM_DISABLED);
    CHECK(controller.reason == SC_REASON_TARGET_MATCH);

    CHECK(sc_f1_place(&controller, 2u, 2u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &size20, 3u) == 0);
    CHECK(controller.decision == SC_DECISION_SKIP);
    CHECK(controller.reason == SC_REASON_SIZE_RELATION_MISMATCH);

    CHECK(sc_f1_place(&controller, 3u, 4u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &size25, 5u) == 0);
    CHECK(controller.decision == SC_DECISION_SKIP);
    CHECK(controller.reason == SC_REASON_SIZE_RELATION_MISMATCH);

    /* Task 3 ref=30: 20 matches (30-20=10). */
    CHECK(sc_f1_apply_target(&controller, &t3_ref30) == 0);
    CHECK(sc_f1_place(&controller, 4u, 6u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &size20, 7u) == 0);
    CHECK(controller.decision == SC_DECISION_EXECUTE_ARM_DISABLED);
    CHECK(controller.reason == SC_REASON_TARGET_MATCH);

    /* Task 4 ref=20: |obs-20| <= 5 mm → 20 and 25 match, 30 does not. */
    sc_f1_init(&controller, 1u);
    CHECK(sc_f1_apply_target(&controller, &t4_ref20) == 0);
    CHECK(sc_f1_place(&controller, 1u, 0u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &size20, 1u) == 0);
    CHECK(controller.decision == SC_DECISION_EXECUTE_ARM_DISABLED);

    CHECK(sc_f1_place(&controller, 2u, 2u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &size25, 3u) == 0);
    CHECK(controller.decision == SC_DECISION_EXECUTE_ARM_DISABLED);

    CHECK(sc_f1_place(&controller, 3u, 4u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &size30, 5u) == 0);
    CHECK(controller.decision == SC_DECISION_SKIP);
    CHECK(controller.reason == SC_REASON_SIZE_RELATION_MISMATCH);

    /* Task 4 ref=30 (only 20 or 30 are valid references): |obs-30| <= 5 mm → 25 and 30 match. */
    {
        sc_target_t t4_ref30 = target(SC_TASK_SIZE_WITHIN_0P5CM_CUBE, SC_COLOR_UNKNOWN, 30u);
        sc_f1_init(&controller, 1u);
        CHECK(sc_f1_apply_target(&controller, &t4_ref30) == 0);
        CHECK(sc_f1_place(&controller, 1u, 0u, 100u) == 0);
        CHECK(sc_f1_observe(&controller, &size25, 1u) == 0);
        CHECK(controller.decision == SC_DECISION_EXECUTE_ARM_DISABLED);
        CHECK(sc_f1_place(&controller, 2u, 2u, 100u) == 0);
        CHECK(sc_f1_observe(&controller, &size30, 3u) == 0);
        CHECK(controller.decision == SC_DECISION_EXECUTE_ARM_DISABLED);
        CHECK(sc_f1_place(&controller, 3u, 4u, 100u) == 0);
        CHECK(sc_f1_observe(&controller, &size20, 5u) == 0);
        CHECK(controller.decision == SC_DECISION_SKIP);
        CHECK(controller.reason == SC_REASON_SIZE_RELATION_MISMATCH);
    }

    /* Size=0 with size_available=1: observation without size → wait, not skip. */
    sc_f1_init(&controller, 1u);
    CHECK(sc_f1_apply_target(&controller, &t3_ref20) == 0);
    CHECK(sc_f1_place(&controller, 1u, 0u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &size0, 1u) == 0);
    CHECK(!controller.result_valid);
}

static void test_target_config_fail_closed(void)
{
    sc_f1_controller_t controller;
    sc_target_t bad_target;

    sc_f1_init(&controller, 0u);
    /* NULL target must be rejected. */
    CHECK(sc_f1_apply_target(&controller, 0) == -1);
    /* Invalid task_mode. */
    memset(&bad_target, 0, sizeof(bad_target));
    bad_target.task = (sc_task_t)0;
    CHECK(sc_f1_apply_target(&controller, &bad_target) == -1);
    /* Task 1 without color. */
    bad_target.task = SC_TASK_COLOR_CUBE;
    bad_target.target_color = SC_COLOR_UNKNOWN;
    CHECK(sc_f1_apply_target(&controller, &bad_target) == -1);
    /* Task 3 with bad reference. */
    bad_target.task = SC_TASK_SIZE_DELTA_1CM_CUBE;
    bad_target.reference_size_cm_x10 = 25u;
    CHECK(sc_f1_apply_target(&controller, &bad_target) == -1);
}

static void test_non_target_zero_grab_intent(void)
{
    sc_f1_controller_t controller;
    sc_observation_t blue_cube = observation(SC_COLOR_BLUE, SC_SHAPE_CUBE, 0u);
    sc_observation_t red_cyl = observation(SC_COLOR_RED, SC_SHAPE_CYLINDER, 0u);
    sc_target_t red_cube_target = target(SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0u);

    sc_f1_init(&controller, 0u);
    CHECK(sc_f1_apply_target(&controller, &red_cube_target) == 0);

    /* Round 1: Non-target blue cube → SKIP, not EXECUTE. */
    CHECK(sc_f1_place(&controller, 1u, 0u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &blue_cube, 1u) == 0);
    CHECK(controller.decision != SC_DECISION_EXECUTE_ARM_DISABLED);
    CHECK(controller.decision == SC_DECISION_SKIP);
    CHECK(controller.reason == SC_REASON_COLOR_MISMATCH);

    /* Round 2: Red cylinder → uncalibrated non-cube → WAIT.
       Same round: red cube → EXECUTE. */
    CHECK(sc_f1_place(&controller, 2u, 2u, 100u) == 0);
    CHECK(sc_f1_observe(&controller, &red_cyl, 3u) == 0);
    CHECK(!controller.result_valid);
    CHECK(controller.decision == SC_DECISION_WAIT);
    {
        sc_observation_t red_cube = observation(SC_COLOR_RED, SC_SHAPE_CUBE, 0u);
        CHECK(sc_f1_observe(&controller, &red_cube, 4u) == 0);
    }
    CHECK(controller.decision == SC_DECISION_EXECUTE_ARM_DISABLED);
    CHECK(controller.reason == SC_REASON_TARGET_MATCH);
    CHECK(controller.result_valid);
    /* After latch, another observe must be rejected. */
    {
        sc_observation_t another = observation(SC_COLOR_RED, SC_SHAPE_CUBE, 0u);
        CHECK(sc_f1_observe(&controller, &another, 5u) == -1);
    }
}

static void test_continuous_frames_after_latch(void)
{
    sc_f1_controller_t controller;
    sc_observation_t red_cube = observation(SC_COLOR_RED, SC_SHAPE_CUBE, 0u);
    sc_target_t red_cube_target = target(SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0u);

    sc_f1_init(&controller, 0u);
    CHECK(sc_f1_apply_target(&controller, &red_cube_target) == 0);
    CHECK(sc_f1_place(&controller, 1u, 0u, 100u) == 0);

    /* Frame 1: latches result. */
    CHECK(sc_f1_observe(&controller, &red_cube, 1u) == 0);
    CHECK(controller.result_valid);
    CHECK(controller.decision == SC_DECISION_EXECUTE_ARM_DISABLED);

    /* Frame 2: result already latched → rejected by F1 (state != ACQUIRING). */
    CHECK(sc_f1_observe(&controller, &red_cube, 2u) == -1);
    /* Frame 3: same — still rejected. */
    CHECK(sc_f1_observe(&controller, &red_cube, 3u) == -1);

    /* Result unchanged — only one terminal decision. */
    CHECK(controller.decision == SC_DECISION_EXECUTE_ARM_DISABLED);
    CHECK(controller.reason == SC_REASON_TARGET_MATCH);
    CHECK(controller.state == SC_STATE_RESULT_LATCHED);
}

static void test_uncalibrated_non_cube_stays_wait(void)
{
    sc_f1_controller_t controller;
    sc_observation_t red_cyl = observation(SC_COLOR_RED, SC_SHAPE_CYLINDER, 0u);
    sc_observation_t red_cone = observation(SC_COLOR_RED, SC_SHAPE_CONE, 0u);
    sc_observation_t red_cube = observation(SC_COLOR_RED, SC_SHAPE_CUBE, 0u);
    sc_observation_t unknown = observation(SC_COLOR_UNKNOWN, SC_SHAPE_UNKNOWN, 0u);
    sc_target_t red_cube_target = target(SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0u);

    sc_f1_init(&controller, 0u);
    CHECK(sc_f1_apply_target(&controller, &red_cube_target) == 0);

    /* Round 1: multiple observations — UNKNOWN/CYLINDER/CONE all keep WAIT,
       then CUBE latches. */
    CHECK(sc_f1_place(&controller, 1u, 0u, 100u) == 0);

    /* UNKNOWN/unstable → stays WAIT (existing behavior). */
    unknown.stable = 0u;
    CHECK(sc_f1_observe(&controller, &unknown, 1u) == 0);
    CHECK(!controller.result_valid);

    /* CYLINDER: uncalibrated → stays WAIT, no SKIP. */
    CHECK(sc_f1_observe(&controller, &red_cyl, 2u) == 0);
    CHECK(!controller.result_valid);
    CHECK(controller.decision == SC_DECISION_WAIT);

    /* CONE: uncalibrated → stays WAIT, no SKIP. */
    CHECK(sc_f1_observe(&controller, &red_cone, 3u) == 0);
    CHECK(!controller.result_valid);
    CHECK(controller.decision == SC_DECISION_WAIT);

    /* CUBE: reliable → enters task judgment → EXECUTE. */
    CHECK(sc_f1_observe(&controller, &red_cube, 4u) == 0);
    CHECK(controller.result_valid);
    CHECK(controller.decision == SC_DECISION_EXECUTE_ARM_DISABLED);
}

static void test_reason_text_all(void)
{
    CHECK(strcmp(sc_f1_reason_text(SC_REASON_NONE), "NONE") == 0);
    CHECK(strcmp(sc_f1_reason_text(SC_REASON_TARGET_MATCH), "TARGET_MATCH_ARM_DISABLED") == 0);
    CHECK(strcmp(sc_f1_reason_text(SC_REASON_COLOR_MISMATCH), "COLOR_MISMATCH_SKIP") == 0);
    CHECK(strcmp(sc_f1_reason_text(SC_REASON_SHAPE_MISMATCH), "SHAPE_MISMATCH_SKIP") == 0);
    CHECK(strcmp(sc_f1_reason_text(SC_REASON_SIZE_RELATION_MISMATCH), "SIZE_RELATION_MISMATCH_SKIP") == 0);
    CHECK(strcmp(sc_f1_reason_text(SC_REASON_OBSERVATION_UNSTABLE), "OBSERVATION_UNSTABLE") == 0);
    CHECK(strcmp(sc_f1_reason_text(SC_REASON_SIZE_UNAVAILABLE), "SIZE_UNAVAILABLE") == 0);
    CHECK(strcmp(sc_f1_reason_text(SC_REASON_OPERATOR_ABANDONED), "OPERATOR_ABANDONED") == 0);
    CHECK(strcmp(sc_f1_reason_text(SC_REASON_TIMEOUT), "ROUND_TIMEOUT") == 0);
    CHECK(strcmp(sc_f1_reason_text(SC_REASON_INVALID_TARGET), "INVALID_TARGET") == 0);
    /* Illegal enum value — must return INVALID_REASON, not "NONE". */
    CHECK(strcmp(sc_f1_reason_text((sc_reason_t)255), "INVALID_REASON") == 0);
    CHECK(strcmp(sc_f1_reason_text((sc_reason_t)99), "INVALID_REASON") == 0);
}

int main(void)
{
    test_place_latch_and_next_place();
    test_shape_size_and_abandon();
    test_timeout_and_twenty_rounds();
    test_event_seq_wraparound();
    test_task2_mixed_shape_pool_semantics();
    test_synthetic_size_tasks_three_and_four();
    test_target_config_fail_closed();
    test_non_target_zero_grab_intent();
    test_continuous_frames_after_latch();
    test_uncalibrated_non_cube_stays_wait();
    test_reason_text_all();
    printf("single_camera_f1: %d/%d passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
