/*==========================================================================
 *  test_task_matcher.c  —  task_matcher 模块单元测试
 *
 *  编译 (from tests/):
 *    gcc -std=c99 -Wall -Wextra \
 *        -DAPB_VISION_BASE_PLACEHOLDER=0xF0000000u \
 *        -I../app/include \
 *        test_task_matcher.c ../app/src/task_matcher.c ../app/src/vision_classifier.c \
 *        -o test_task_matcher.exe && ./test_task_matcher.exe
 *==========================================================================*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "task_matcher.h"

/*--------------------------------------------------------------------------
 *  简易测试框架
 *--------------------------------------------------------------------------*/
static int _test_failures = 0;
static int _test_count    = 0;
static int _test_start    = 0;

static void _check(const char *file, int line, int cond, const char *msg)
{
    _test_count++;
    if (!cond) { _test_failures++; printf("  FAIL [%s:%d] %s\n", file, line, msg); }
}
#define CHECK(cond) _check(__FILE__, __LINE__, (cond), #cond)
#define CHECK_EQ(a, b) _check(__FILE__, __LINE__, ((a) == (b)), #a " == " #b)
#define CHECK_NE(a, b) _check(__FILE__, __LINE__, ((a) != (b)), #a " != " #b)

#define TEST(name) \
    printf("  %-55s", name " "); fflush(stdout); _test_start = _test_failures

#define PASS() \
    do { int d = _test_failures - _test_start; \
         if (d == 0) printf("PASS\n"); else printf("%d FAILED\n", d); } while(0)

/*--------------------------------------------------------------------------
 *  Helpers
 *--------------------------------------------------------------------------*/
static vision_result_t make_obs(uint8_t color, uint8_t shape, uint8_t size)
{
    vision_result_t r;
    memset(&r, 0, sizeof(r));
    r.color_id    = color;
    r.shape_id    = shape;
    r.size_cm_x10 = size;
    r.confidence  = 200;
    return r;
}

static task_target_t make_target(uint8_t color, uint8_t shape, uint8_t size)
{
    task_target_t t;
    t.target_color     = color;
    t.target_shape     = shape;
    t.target_size_cm_x10 = size;
    return t;
}

/*==========================================================================
 *  测试组 1: 无目标 / 初始化
 *==========================================================================*/

static void test_no_target_returns_none(void)
{
    TEST("eval: no target set → NONE");
    task_matcher_init();
    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    uint8_t a = task_matcher_evaluate(&obs, 100, 200);
    CHECK_EQ(a, MATCH_ACTION_NONE);
    PASS();
}

static void test_init_clears_target(void)
{
    TEST("init: clears previously set target → get_target returns NULL");
    task_matcher_init();
    task_target_t t = make_target(COLOR_BLUE, SHAPE_CYLINDER, 25);
    task_matcher_set_target(&t);
    CHECK(task_matcher_get_target() != 0);

    task_matcher_init();
    CHECK(task_matcher_get_target() == 0);
    PASS();
}

static void test_set_null_clears_target(void)
{
    TEST("set: NULL clears target");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);
    CHECK(task_matcher_get_target() != 0);

    task_matcher_set_target(0);
    CHECK(task_matcher_get_target() == 0);
    PASS();
}

static void test_set_null_clears_grab(void)
{
    TEST("set: NULL target invalidates old grab coords");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    /* grab once */
    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 100, 200), MATCH_ACTION_GRAB);

    /* clear target — old grab should become invalid */
    task_matcher_set_target(0);

    uint16_t cx = 99, cy = 99;
    int rc = task_matcher_get_grab_coord(&cx, &cy);
    CHECK_EQ(rc, -1);
    CHECK_EQ(cx, 99);   /* unchanged */
    CHECK_EQ(cy, 99);
    PASS();
}

/*==========================================================================
 *  测试组 2: 精确匹配 → GRAB
 *==========================================================================*/

static void test_exact_match_grab(void)
{
    TEST("eval: exact match (color+shape+size) → GRAB");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    uint8_t a = task_matcher_evaluate(&obs, 150, 250);
    CHECK_EQ(a, MATCH_ACTION_GRAB);
    PASS();
}

static void test_grab_stores_coords(void)
{
    TEST("coord: GRAB match saves correct grab coordinates");
    task_matcher_init();
    task_target_t t = make_target(COLOR_BLUE, SHAPE_CYLINDER, 30);
    task_matcher_set_target(&t);

    vision_result_t obs = make_obs(COLOR_BLUE, SHAPE_CYLINDER, 30);
    uint8_t a = task_matcher_evaluate(&obs, 320, 240);
    CHECK_EQ(a, MATCH_ACTION_GRAB);

    uint16_t cx = 0, cy = 0;
    int rc = task_matcher_get_grab_coord(&cx, &cy);
    CHECK_EQ(rc, 0);
    CHECK_EQ(cx, 320);
    CHECK_EQ(cy, 240);
    PASS();
}

static void test_grab_overwrites_old_coords(void)
{
    TEST("coord: second GRAB overwrites previous coordinates");
    task_matcher_init();
    task_target_t t = make_target(COLOR_YELLOW, SHAPE_CONE, 25);
    task_matcher_set_target(&t);

    /* First match */
    vision_result_t o1 = make_obs(COLOR_YELLOW, SHAPE_CONE, 25);
    (void)task_matcher_evaluate(&o1, 100, 100);

    /* Advance round lock to allow second GRAB */
    task_matcher_next_round();
    task_matcher_set_target(&t);

    /* Second match at different position */
    vision_result_t o2 = make_obs(COLOR_YELLOW, SHAPE_CONE, 25);
    (void)task_matcher_evaluate(&o2, 400, 500);

    uint16_t cx = 0, cy = 0;
    (void)task_matcher_get_grab_coord(&cx, &cy);
    CHECK_EQ(cx, 400);
    CHECK_EQ(cy, 500);
    PASS();
}

/*==========================================================================
 *  测试组 3: 不匹配 → SKIP
 *==========================================================================*/

static void test_color_mismatch_skip(void)
{
    TEST("eval: color mismatch → SKIP");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    vision_result_t obs = make_obs(COLOR_BLUE, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    PASS();
}

static void test_shape_mismatch_skip(void)
{
    TEST("eval: shape mismatch → SKIP");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CYLINDER, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    PASS();
}

static void test_size_mismatch_skip(void)
{
    TEST("eval: size mismatch → SKIP");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 30);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    PASS();
}

static void test_size_unavailable_returns_none(void)
{
    TEST("eval: target has size but obs.size=0 → NONE (wait Cam1)");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    /* Cam0-only snapshot: color+shape OK but size=0 (Cam1 not ready) */
    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 0);
    CHECK_EQ(task_matcher_evaluate(&obs, 100, 200), MATCH_ACTION_NONE);

    /* grab should NOT be valid — we didn't GRAB */
    uint16_t cx = 99, cy = 99;
    CHECK_EQ(task_matcher_get_grab_coord(&cx, &cy), -1);
    PASS();
}

static void test_size_unavailable_but_wildcard_ok(void)
{
    TEST("eval: target_size=0 wildcard + obs.size=0 → GRAB (no Cam1 needed)");
    task_matcher_init();
    /* size_sel=00 (wildcard): any size is OK, including 0 */
    task_target_t t = make_target(COLOR_BLUE, SHAPE_CUBE, 0);
    task_matcher_set_target(&t);

    vision_result_t obs = make_obs(COLOR_BLUE, SHAPE_CUBE, 0);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_GRAB);
    PASS();
}

static void test_multi_mismatch_skip(void)
{
    TEST("eval: both color+shape mismatch → SKIP");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    vision_result_t obs = make_obs(COLOR_BLUE, SHAPE_CONE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    PASS();
}

/*==========================================================================
 *  测试组 4: 观测无效 → NONE（正式主线 DEBUG_MODE=0 行为）
 *
 *  注意：DEBUG_MODE=1 时这些用例应返回 MATCH_ACTION_ERROR。
 *  当前编译默认 DEBUG_MODE=0，验证正式主线安全策略。
 *==========================================================================*/

static void test_unknown_color_none(void)
{
    TEST("eval: color=UNKNOWN → NONE (production mode)");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    vision_result_t obs = make_obs(COLOR_UNKNOWN, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_NONE);
    PASS();
}

static void test_unknown_shape_none(void)
{
    TEST("eval: shape=UNKNOWN → NONE (production mode)");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    vision_result_t obs = make_obs(COLOR_RED, SHAPE_UNKNOWN, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_NONE);
    PASS();
}

static void test_null_obs_none(void)
{
    TEST("eval: NULL obs → NONE (production: defense without false alarm)");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    CHECK_EQ(task_matcher_evaluate(0, 100, 200), MATCH_ACTION_NONE);
    PASS();
}

/*==========================================================================
 *  测试组 5: 通配符（target=UNKNOWN/0 时跳过该维度）
 *==========================================================================*/

static void test_target_color_wildcard(void)
{
    TEST("wildcard: target_color=UNKNOWN matches any color");
    task_matcher_init();
    task_target_t t = make_target(COLOR_UNKNOWN, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    /* Any color with CUBE size 20 should GRAB */
    vision_result_t o1 = make_obs(COLOR_RED,    SHAPE_CUBE, 20);
    vision_result_t o2 = make_obs(COLOR_BLUE,   SHAPE_CUBE, 20);
    vision_result_t o3 = make_obs(COLOR_YELLOW, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&o1, 0, 0), MATCH_ACTION_GRAB);
    task_matcher_next_round(); task_matcher_set_target(&t);
    CHECK_EQ(task_matcher_evaluate(&o2, 0, 0), MATCH_ACTION_GRAB);
    task_matcher_next_round(); task_matcher_set_target(&t);
    CHECK_EQ(task_matcher_evaluate(&o3, 0, 0), MATCH_ACTION_GRAB);

    /* Wrong shape → SKIP (need fresh round) */
    task_matcher_next_round(); task_matcher_set_target(&t);
    vision_result_t o4 = make_obs(COLOR_RED, SHAPE_CYLINDER, 20);
    CHECK_EQ(task_matcher_evaluate(&o4, 0, 0), MATCH_ACTION_SKIP);
    PASS();
}

static void test_target_shape_wildcard(void)
{
    TEST("wildcard: target_shape=UNKNOWN matches any shape");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_UNKNOWN, 25);
    task_matcher_set_target(&t);

    vision_result_t o1 = make_obs(COLOR_RED, SHAPE_CUBE,     25);
    vision_result_t o2 = make_obs(COLOR_RED, SHAPE_CYLINDER, 25);
    vision_result_t o3 = make_obs(COLOR_RED, SHAPE_CONE,     25);
    CHECK_EQ(task_matcher_evaluate(&o1, 0, 0), MATCH_ACTION_GRAB);
    task_matcher_next_round(); task_matcher_set_target(&t);
    CHECK_EQ(task_matcher_evaluate(&o2, 0, 0), MATCH_ACTION_GRAB);
    task_matcher_next_round(); task_matcher_set_target(&t);
    CHECK_EQ(task_matcher_evaluate(&o3, 0, 0), MATCH_ACTION_GRAB);

    /* Wrong color → SKIP (need fresh round) */
    task_matcher_next_round(); task_matcher_set_target(&t);
    vision_result_t o4 = make_obs(COLOR_BLUE, SHAPE_CUBE, 25);
    CHECK_EQ(task_matcher_evaluate(&o4, 0, 0), MATCH_ACTION_SKIP);
    PASS();
}

static void test_target_size_wildcard(void)
{
    TEST("wildcard: target_size=0 matches any size");
    task_matcher_init();
    task_target_t t = make_target(COLOR_YELLOW, SHAPE_CONE, 0);
    task_matcher_set_target(&t);

    vision_result_t o1 = make_obs(COLOR_YELLOW, SHAPE_CONE, 20);
    vision_result_t o2 = make_obs(COLOR_YELLOW, SHAPE_CONE, 25);
    vision_result_t o3 = make_obs(COLOR_YELLOW, SHAPE_CONE, 30);
    CHECK_EQ(task_matcher_evaluate(&o1, 0, 0), MATCH_ACTION_GRAB);
    task_matcher_next_round(); task_matcher_set_target(&t);
    CHECK_EQ(task_matcher_evaluate(&o2, 0, 0), MATCH_ACTION_GRAB);
    task_matcher_next_round(); task_matcher_set_target(&t);
    CHECK_EQ(task_matcher_evaluate(&o3, 0, 0), MATCH_ACTION_GRAB);

    /* size=0 observation (Cam0 only) also matches wildcard */
    task_matcher_next_round(); task_matcher_set_target(&t);
    vision_result_t o4 = make_obs(COLOR_YELLOW, SHAPE_CONE, 0);
    CHECK_EQ(task_matcher_evaluate(&o4, 0, 0), MATCH_ACTION_GRAB);
    PASS();
}

static void test_all_wildcards(void)
{
    TEST("wildcard: all wildcards → matches any valid observation");
    task_matcher_init();
    task_target_t t = make_target(COLOR_UNKNOWN, SHAPE_UNKNOWN, 0);
    task_matcher_set_target(&t);

    vision_result_t o1 = make_obs(COLOR_RED,   SHAPE_CUBE,     20);
    vision_result_t o2 = make_obs(COLOR_BLUE,  SHAPE_CYLINDER, 30);
    vision_result_t o3 = make_obs(COLOR_WHITE, SHAPE_CONE,     25);
    CHECK_EQ(task_matcher_evaluate(&o1, 0, 0), MATCH_ACTION_GRAB);
    task_matcher_next_round(); task_matcher_set_target(&t);
    CHECK_EQ(task_matcher_evaluate(&o2, 0, 0), MATCH_ACTION_GRAB);
    task_matcher_next_round(); task_matcher_set_target(&t);
    CHECK_EQ(task_matcher_evaluate(&o3, 0, 0), MATCH_ACTION_GRAB);
    PASS();
}

/*==========================================================================
 *  测试组 6: 未匹配时 grab_coord 不可用
 *==========================================================================*/

static void test_no_grab_before_eval(void)
{
    TEST("coord: get_grab_coord returns -1 before any evaluate");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    uint16_t cx = 99, cy = 99;
    int rc = task_matcher_get_grab_coord(&cx, &cy);
    CHECK_EQ(rc, -1);
    /* cx, cy should not have been overwritten */
    CHECK_EQ(cx, 99);
    CHECK_EQ(cy, 99);
    PASS();
}

static void test_no_grab_after_skip(void)
{
    TEST("coord: get_grab_coord returns -1 after SKIP");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 30);
    task_matcher_set_target(&t);

    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 20);  /* size mismatch */
    (void)task_matcher_evaluate(&obs, 100, 100);

    uint16_t cx = 99, cy = 99;
    int rc = task_matcher_get_grab_coord(&cx, &cy);
    CHECK_EQ(rc, -1);
    CHECK_EQ(cx, 99);
    CHECK_EQ(cy, 99);
    PASS();
}

static void test_no_grab_after_invalid(void)
{
    TEST("coord: get_grab_coord returns -1 after non-GRAB (NONE/NULL obs)");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    /* UNKNOWN → NONE in production mode；grab_valid cleared by evaluate() */
    vision_result_t obs = make_obs(COLOR_UNKNOWN, SHAPE_CUBE, 20);
    (void)task_matcher_evaluate(&obs, 100, 100);

    uint16_t cx = 99, cy = 99;
    int rc = task_matcher_get_grab_coord(&cx, &cy);
    CHECK_EQ(rc, -1);
    PASS();
}

static void test_grab_coord_null_ptr(void)
{
    TEST("coord: NULL cx/cy returns -1 without crash");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 100, 200), MATCH_ACTION_GRAB);

    /* NULL output pointers → should return -1, not hardfault */
    CHECK_EQ(task_matcher_get_grab_coord(0, 0), -1);
    PASS();
}

/*==========================================================================
 *  测试组 7: 目标切换
 *==========================================================================*/

static void test_target_switch(void)
{
    TEST("switch: changing target alters matching behavior");
    task_matcher_init();

    /* First target: RED CUBE 20 */
    task_target_t t1 = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t1);

    vision_result_t obs_red_cube_20 = make_obs(COLOR_RED,  SHAPE_CUBE, 20);
    vision_result_t obs_blu_cyl_25  = make_obs(COLOR_BLUE, SHAPE_CYLINDER, 25);

    CHECK_EQ(task_matcher_evaluate(&obs_red_cube_20, 0, 0), MATCH_ACTION_GRAB);

    /* Advance round, re-lock same target to test SKIP on wrong obs */
    task_matcher_next_round();
    task_matcher_set_target(&t1);
    CHECK_EQ(task_matcher_evaluate(&obs_blu_cyl_25,  0, 0), MATCH_ACTION_SKIP);

    /* Advance round before switching target */
    task_matcher_next_round();

    /* Switch target: BLUE CYLINDER 25 */
    task_target_t t2 = make_target(COLOR_BLUE, SHAPE_CYLINDER, 25);
    task_matcher_set_target(&t2);

    CHECK_EQ(task_matcher_evaluate(&obs_red_cube_20, 0, 0), MATCH_ACTION_SKIP);
    CHECK_EQ(task_matcher_evaluate(&obs_blu_cyl_25,  0, 0), MATCH_ACTION_GRAB);
    PASS();
}

/*==========================================================================
 *  测试组 8: get_target 返回正确值
 *==========================================================================*/

static void test_get_target_returns_set_values(void)
{
    TEST("get_target: returns exact values passed to set_target");
    task_matcher_init();
    task_target_t t = make_target(COLOR_YELLOW, SHAPE_CONE, 30);
    task_matcher_set_target(&t);

    const task_target_t *ret = task_matcher_get_target();
    CHECK(ret != 0);
    CHECK_EQ(ret->target_color,     COLOR_YELLOW);
    CHECK_EQ(ret->target_shape,     SHAPE_CONE);
    CHECK_EQ(ret->target_size_cm_x10, 30);
    PASS();
}

/*==========================================================================
 *  测试组 9: read_target_from_fpga 行为 (TARGET_SEL_AVAILABLE=0, DEBUG_MODE=0)
 *==========================================================================*/

static void test_read_target_clears_on_unavailable(void)
{
    TEST("fpga: read_target w/ TARGET_SEL=0 clears target (production)");
    task_matcher_init();

    /* Set a manual target first */
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);
    CHECK(task_matcher_get_target() != 0);

    /* TARGET_SEL_AVAILABLE=0 + DEBUG_MODE=0 → must clear target */
    int rc = task_matcher_read_target_from_fpga();
    CHECK_EQ(rc, -1);
    CHECK(task_matcher_get_target() == 0);

    /* After clear, evaluate returns NONE */
    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_NONE);
    PASS();
}

static void test_both_unknown_returns_none(void)
{
    TEST("eval: both color+shape UNKNOWN → NONE (production)");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    vision_result_t obs = make_obs(COLOR_UNKNOWN, SHAPE_UNKNOWN, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_NONE);

    /* grab should NOT be valid */
    uint16_t cx = 99, cy = 99;
    CHECK_EQ(task_matcher_get_grab_coord(&cx, &cy), -1);
    PASS();
}

/*==========================================================================
 *  测试组 10: 边界条件
 *==========================================================================*/

static void test_grab_coord_boundary_zero(void)
{
    TEST("boundary: grab coord (0,0) is valid");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_GRAB);

    uint16_t cx = 1, cy = 1;
    int rc = task_matcher_get_grab_coord(&cx, &cy);
    CHECK_EQ(rc, 0);
    CHECK_EQ(cx, 0);
    CHECK_EQ(cy, 0);
    PASS();
}

static void test_skip_clears_old_grab(void)
{
    TEST("boundary: SKIP after GRAB invalidates old grab coords");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    /* First: GRAB at (111, 222) */
    vision_result_t good = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&good, 111, 222), MATCH_ACTION_GRAB);

    /* Advance round lock so next evaluate can reach the color check */
    task_matcher_next_round();
    task_matcher_set_target(&t);

    /* Then: SKIP (wrong color) — must invalidate previous GRAB coords */
    vision_result_t bad = make_obs(COLOR_BLUE, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&bad, 999, 999), MATCH_ACTION_SKIP);

    uint16_t cx = 99, cy = 99;
    int rc = task_matcher_get_grab_coord(&cx, &cy);
    CHECK_EQ(rc, -1);
    CHECK_EQ(cx, 99);   /* unchanged */
    CHECK_EQ(cy, 99);
    PASS();
}

static void test_invalid_obs_clears_old_grab(void)
{
    TEST("boundary: locked evaluate preserves grab coords, next_round clears");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    vision_result_t good = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&good, 111, 222), MATCH_ACTION_GRAB);

    /* Grab coords ARE valid after GRAB */
    uint16_t cx = 0, cy = 0;
    CHECK_EQ(task_matcher_get_grab_coord(&cx, &cy), 0);
    CHECK_EQ(cx, 111);
    CHECK_EQ(cy, 222);

    /* Locked evaluate (round_state=GRAB_REQUESTED) returns NONE
     * but does NOT clear grab coords — coords persist until next_round */
    vision_result_t bad = make_obs(COLOR_UNKNOWN, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&bad, 0, 0), MATCH_ACTION_NONE);

    cx = 0; cy = 0;
    CHECK_EQ(task_matcher_get_grab_coord(&cx, &cy), 0);
    CHECK_EQ(cx, 111);
    CHECK_EQ(cy, 222);

    /* next_round invalidates grab coords */
    task_matcher_next_round();
    cx = 99; cy = 99;
    int rc = task_matcher_get_grab_coord(&cx, &cy);
    CHECK_EQ(rc, -1);
    CHECK_EQ(cx, 99);
    CHECK_EQ(cy, 99);
    PASS();
}

static void test_set_target_clears_old_grab(void)
{
    TEST("boundary: set_target resets grab_valid");
    task_matcher_init();
    task_target_t t1 = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t1);

    vision_result_t good = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&good, 100, 200), MATCH_ACTION_GRAB);

    /* Switch target — old grab should be invalidated */
    task_target_t t2 = make_target(COLOR_BLUE, SHAPE_CYLINDER, 25);
    task_matcher_set_target(&t2);

    uint16_t cx = 99, cy = 99;
    int rc = task_matcher_get_grab_coord(&cx, &cy);
    CHECK_EQ(rc, -1);
    CHECK_EQ(cx, 99);   /* unchanged */
    CHECK_EQ(cy, 99);
    PASS();
}

/*==========================================================================
 *  测试组 11: 四任务决赛模式
 *==========================================================================*/

/* helper: build extended target for set_target_ex.
 * round_state is intentionally left 0 (ROUND_IDLE) —
 * set_target_ex() must auto-lock to ROUND_TARGET_LOCKED per API contract. */
static task_target_t make_target_ex(uint8_t color, uint8_t shape,
                                     uint8_t size, uint8_t ref_size,
                                     uint8_t mode)
{
    task_target_t t;
    memset(&t, 0, sizeof(t));
    t.target_color       = color;
    t.target_shape       = shape;
    t.target_size_cm_x10 = size;
    t.reference_size_cm_x10 = ref_size;
    t.task_mode          = mode;
    return t;
}

/* ---- set_target_ex auto-lock contract ---- */

static void test_set_target_ex_auto_locks(void)
{
    TEST("contract: set_target_ex auto-locks even if caller left round_state=0");
    task_matcher_init();

    /* Simulate common bug: memset-to-zero, only fill business fields */
    task_target_t t;
    memset(&t, 0, sizeof(t));
    t.target_color       = COLOR_RED;
    t.target_shape       = SHAPE_CUBE;
    t.target_size_cm_x10 = 20;
    t.task_mode          = TASK_MODE_2;
    /* t.round_state left as 0 (ROUND_IDLE) — API must override this */

    task_matcher_set_target_ex(&t);

    /* Should still be in LOCKED state and able to GRAB */
    CHECK_EQ(task_matcher_get_round_state(), ROUND_TARGET_LOCKED);

    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 100, 200), MATCH_ACTION_GRAB);
    PASS();
}

/* ---- MODE_1: color+shape exact, size wildcard ---- */

static void test_mode1_exact_color_shape(void)
{
    TEST("mode1: color+shape match → GRAB (size ignored)");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_RED, SHAPE_CUBE, 0, 0, TASK_MODE_1);
    task_matcher_set_target_ex(&t);

    /* Match: RED CUBE 20mm → GRAB (size ignored in MODE_1) */
    vision_result_t o1 = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&o1, 0, 0), MATCH_ACTION_GRAB);
    PASS();
}

static void test_mode1_size_ignored(void)
{
    TEST("mode1: different sizes still GRAB (size wildcard)");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_RED, SHAPE_CUBE, 0, 0, TASK_MODE_1);
    task_matcher_set_target_ex(&t);

    /* 25mm, 30mm, size=0 (Cam0-only) all GRAB */
    vision_result_t o1 = make_obs(COLOR_RED, SHAPE_CUBE, 25);
    CHECK_EQ(task_matcher_evaluate(&o1, 0, 0), MATCH_ACTION_GRAB);
    task_matcher_next_round(); task_matcher_set_target_ex(&t);
    vision_result_t o2 = make_obs(COLOR_RED, SHAPE_CUBE, 30);
    CHECK_EQ(task_matcher_evaluate(&o2, 0, 0), MATCH_ACTION_GRAB);
    task_matcher_next_round(); task_matcher_set_target_ex(&t);
    vision_result_t o3 = make_obs(COLOR_RED, SHAPE_CUBE, 0);
    CHECK_EQ(task_matcher_evaluate(&o3, 0, 0), MATCH_ACTION_GRAB);
    PASS();
}

static void test_mode1_color_mismatch_skip(void)
{
    TEST("mode1: wrong color → SKIP");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_RED, SHAPE_CUBE, 0, 0, TASK_MODE_1);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_BLUE, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    PASS();
}

/* ---- MODE_2: color+shape+size exact match ---- */

static void test_mode2_exact_all_match(void)
{
    TEST("mode2: color+shape+size all match → GRAB");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_BLUE, SHAPE_CYLINDER, 25, 0, TASK_MODE_2);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_BLUE, SHAPE_CYLINDER, 25);
    CHECK_EQ(task_matcher_evaluate(&obs, 100, 200), MATCH_ACTION_GRAB);

    uint16_t cx = 0, cy = 0;
    CHECK_EQ(task_matcher_get_grab_coord(&cx, &cy), 0);
    CHECK_EQ(cx, 100);
    CHECK_EQ(cy, 200);
    PASS();
}

static void test_mode2_size_mismatch_skip(void)
{
    TEST("mode2: right color+shape but wrong size → SKIP");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_RED, SHAPE_CUBE, 20, 0, TASK_MODE_2);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 30);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    PASS();
}

static void test_mode2_size_unavailable_none(void)
{
    TEST("mode2: size required but obs.size=0 → NONE");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_RED, SHAPE_CUBE, 20, 0, TASK_MODE_2);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 0);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_NONE);
    PASS();
}

/* ---- MODE_3: |obs.size - reference_size| == 10mm ---- */

static void test_mode3_delta_10mm_match(void)
{
    TEST("mode3: |obs-ref|==10mm → GRAB (ref=20, obs=30)");
    task_matcher_init();
    /* reference=20mm, target must be 30mm (delta=10) */
    task_target_t t = make_target_ex(COLOR_YELLOW, SHAPE_CUBE, 0, 20, TASK_MODE_3);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_YELLOW, SHAPE_CUBE, 30);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_GRAB);
    PASS();
}

static void test_mode3_delta_10mm_reverse(void)
{
    TEST("mode3: |obs-ref|==10mm → GRAB (ref=30, obs=20)");
    task_matcher_init();
    /* reference=30mm, obs=20mm also delta=10 */
    task_target_t t = make_target_ex(COLOR_YELLOW, SHAPE_CUBE, 0, 30, TASK_MODE_3);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_YELLOW, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_GRAB);
    PASS();
}

static void test_mode3_delta_not_10mm_skip(void)
{
    TEST("mode3: |obs-ref|!=10mm → SKIP (delta=5)");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_YELLOW, SHAPE_CUBE, 0, 20, TASK_MODE_3);
    task_matcher_set_target_ex(&t);

    /* 25 - 20 = 5, not 10 */
    vision_result_t obs = make_obs(COLOR_YELLOW, SHAPE_CUBE, 25);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    PASS();
}

static void test_mode3_ref_not_set_none(void)
{
    TEST("mode3: reference_size=0 → NONE (ref not locked)");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_YELLOW, SHAPE_CUBE, 0, 0, TASK_MODE_3);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_YELLOW, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_NONE);
    PASS();
}

static void test_mode3_size_unavailable_none(void)
{
    TEST("mode3: obs.size=0 → NONE (wait Cam1)");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_YELLOW, SHAPE_CUBE, 0, 20, TASK_MODE_3);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_YELLOW, SHAPE_CUBE, 0);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_NONE);
    PASS();
}

/* ---- MODE_4: |obs.size - target_size| <= 5mm ---- */

static void test_mode4_exact_size_match(void)
{
    TEST("mode4: |obs-target|=0 ≤5mm → GRAB");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_BLUE, SHAPE_CUBE, 25, 0, TASK_MODE_4);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_BLUE, SHAPE_CUBE, 25);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_GRAB);
    PASS();
}

static void test_mode4_delta_5mm_match(void)
{
    TEST("mode4: |obs-target|=5mm ≤5mm → GRAB (target=20, obs=25)");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_BLUE, SHAPE_CUBE, 20, 0, TASK_MODE_4);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_BLUE, SHAPE_CUBE, 25);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_GRAB);
    PASS();
}

static void test_mode4_delta_exceeds_5mm_skip(void)
{
    TEST("mode4: |obs-target|=10mm >5mm → SKIP");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_BLUE, SHAPE_CUBE, 20, 0, TASK_MODE_4);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_BLUE, SHAPE_CUBE, 30);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    PASS();
}

static void test_mode4_size_unavailable_none(void)
{
    TEST("mode4: obs.size=0 → NONE (wait Cam1)");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_BLUE, SHAPE_CUBE, 25, 0, TASK_MODE_4);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_BLUE, SHAPE_CUBE, 0);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_NONE);
    PASS();
}

/* ---- MODE_3 + MODE_4: color/shape still checked ---- */

static void test_mode3_color_mismatch_skip(void)
{
    TEST("mode3: wrong color → SKIP (color still checked)");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_RED, SHAPE_CUBE, 0, 20, TASK_MODE_3);
    task_matcher_set_target_ex(&t);

    /* right delta (30-20=10) but wrong color */
    vision_result_t obs = make_obs(COLOR_BLUE, SHAPE_CUBE, 30);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    PASS();
}

static void test_mode4_shape_mismatch_skip(void)
{
    TEST("mode4: wrong shape → SKIP (shape still checked)");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_BLUE, SHAPE_CUBE, 25, 0, TASK_MODE_4);
    task_matcher_set_target_ex(&t);

    /* right size (25) but wrong shape */
    vision_result_t obs = make_obs(COLOR_BLUE, SHAPE_CYLINDER, 25);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    PASS();
}

/*==========================================================================
 *  测试组 12: 五色目标（WHITE/BLACK 不与 COLOR_UNKNOWN 混淆）
 *==========================================================================*/

static void test_white_target_match(void)
{
    TEST("color5: WHITE target matches WHITE observation");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_WHITE, SHAPE_CUBE, 20, 0, TASK_MODE_2);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_WHITE, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_GRAB);
    PASS();
}

static void test_black_target_match(void)
{
    TEST("color5: BLACK target matches BLACK observation");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_BLACK, SHAPE_CUBE, 25, 0, TASK_MODE_2);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_BLACK, SHAPE_CUBE, 25);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_GRAB);
    PASS();
}

static void test_white_not_wildcard(void)
{
    TEST("color5: WHITE target ≠ wildcard (RED obs → SKIP)");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_WHITE, SHAPE_CUBE, 20, 0, TASK_MODE_2);
    task_matcher_set_target_ex(&t);

    /* COLOR_WHITE is not COLOR_UNKNOWN — must match exactly */
    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    PASS();
}

static void test_black_not_wildcard(void)
{
    TEST("color5: BLACK target ≠ wildcard (BLUE obs → SKIP)");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_BLACK, SHAPE_CUBE, 25, 0, TASK_MODE_2);
    task_matcher_set_target_ex(&t);

    vision_result_t obs = make_obs(COLOR_BLUE, SHAPE_CUBE, 25);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    PASS();
}

static void test_white_not_unknown(void)
{
    TEST("color5: COLOR_WHITE != COLOR_UNKNOWN (sanity)");
    CHECK_NE(COLOR_WHITE, COLOR_UNKNOWN);
    CHECK_NE(COLOR_BLACK, COLOR_UNKNOWN);
    PASS();
}

/*==========================================================================
 *  测试组 13: 一轮一事务锁
 *==========================================================================*/

static void test_round_lock_prevents_double_grab(void)
{
    TEST("round: GRAB locks round → second evaluate returns NONE");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_RED, SHAPE_CUBE, 20, 0, TASK_MODE_2);
    task_matcher_set_target_ex(&t);

    /* First GRAB */
    vision_result_t o1 = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&o1, 111, 222), MATCH_ACTION_GRAB);

    /* Same frame, same match → locked, returns NONE */
    vision_result_t o2 = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&o2, 333, 444), MATCH_ACTION_NONE);

    /* Grab coords still from FIRST GRAB (not overwritten by NONE) */
    uint16_t cx = 0, cy = 0;
    CHECK_EQ(task_matcher_get_grab_coord(&cx, &cy), 0);
    CHECK_EQ(cx, 111);
    CHECK_EQ(cy, 222);
    PASS();
}

static void test_next_round_unlocks(void)
{
    TEST("round: next_round → set_target → can GRAB again");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_RED, SHAPE_CUBE, 20, 0, TASK_MODE_2);
    task_matcher_set_target_ex(&t);

    /* Round 1 GRAB */
    vision_result_t o1 = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&o1, 100, 100), MATCH_ACTION_GRAB);

    /* Advance round */
    task_matcher_next_round();
    task_matcher_set_target_ex(&t);

    /* Round 2 GRAB — should succeed */
    vision_result_t o2 = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&o2, 200, 200), MATCH_ACTION_GRAB);

    uint16_t cx = 0, cy = 0;
    CHECK_EQ(task_matcher_get_grab_coord(&cx, &cy), 0);
    CHECK_EQ(cx, 200);
    CHECK_EQ(cy, 200);
    PASS();
}

static void test_round_reset_clears_all(void)
{
    TEST("round: round_reset → ROUND_IDLE, get_target returns NULL");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_RED, SHAPE_CUBE, 20, 0, TASK_MODE_2);
    task_matcher_set_target_ex(&t);

    /* Verify target is set */
    CHECK(task_matcher_get_target() != 0);

    /* Reset */
    task_matcher_round_reset();

    /* Target cleared */
    CHECK(task_matcher_get_target() == 0);
    CHECK_EQ(task_matcher_get_round_state(), ROUND_IDLE);

    /* Evaluate returns NONE */
    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_NONE);
    PASS();
}

static void test_round_state_transitions(void)
{
    TEST("round: IDLE→LOCKED→GRAB_REQUESTED state transitions");
    task_matcher_init();

    /* Initial state */
    CHECK_EQ(task_matcher_get_round_state(), ROUND_IDLE);

    /* Set target → LOCKED */
    task_target_t t = make_target_ex(COLOR_RED, SHAPE_CUBE, 20, 0, TASK_MODE_2);
    task_matcher_set_target_ex(&t);
    CHECK_EQ(task_matcher_get_round_state(), ROUND_TARGET_LOCKED);

    /* GRAB → GRAB_REQUESTED */
    vision_result_t obs = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_GRAB);
    CHECK_EQ(task_matcher_get_round_state(), ROUND_GRAB_REQUESTED);

    /* next_round → IDLE */
    task_matcher_next_round();
    CHECK_EQ(task_matcher_get_round_state(), ROUND_IDLE);
    PASS();
}

static void test_skip_does_not_lock_round(void)
{
    TEST("round: SKIP does NOT advance round state");
    task_matcher_init();
    task_target_t t = make_target_ex(COLOR_RED, SHAPE_CUBE, 20, 0, TASK_MODE_2);
    task_matcher_set_target_ex(&t);
    CHECK_EQ(task_matcher_get_round_state(), ROUND_TARGET_LOCKED);

    /* SKIP on wrong color */
    vision_result_t bad = make_obs(COLOR_BLUE, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&bad, 0, 0), MATCH_ACTION_SKIP);

    /* Still LOCKED — can try again */
    CHECK_EQ(task_matcher_get_round_state(), ROUND_TARGET_LOCKED);

    /* Now correct obs → GRAB */
    vision_result_t good = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&good, 0, 0), MATCH_ACTION_GRAB);
    CHECK_EQ(task_matcher_get_round_state(), ROUND_GRAB_REQUESTED);
    PASS();
}

/*==========================================================================
 *  main
 *==========================================================================*/

int main(void)
{
    printf("\n=== task_matcher unit tests ===\n\n");

    printf("[1] No target / init\n");
    test_no_target_returns_none();
    test_init_clears_target();
    test_set_null_clears_target();
    test_set_null_clears_grab();

    printf("\n[2] Exact match → GRAB\n");
    test_exact_match_grab();
    test_grab_stores_coords();
    test_grab_overwrites_old_coords();

    printf("\n[3] Mismatch → SKIP / NONE (size unavailable)\n");
    test_color_mismatch_skip();
    test_shape_mismatch_skip();
    test_size_mismatch_skip();
    test_size_unavailable_returns_none();
    test_size_unavailable_but_wildcard_ok();
    test_multi_mismatch_skip();

    printf("\n[4] Invalid observation → NONE (production mode)\n");
    test_unknown_color_none();
    test_unknown_shape_none();
    test_null_obs_none();

    printf("\n[5] Wildcard targets\n");
    test_target_color_wildcard();
    test_target_shape_wildcard();
    test_target_size_wildcard();
    test_all_wildcards();

    printf("\n[6] Grab coords unavailable\n");
    test_no_grab_before_eval();
    test_no_grab_after_skip();
    test_no_grab_after_invalid();
    test_grab_coord_null_ptr();

    printf("\n[7] Target switching\n");
    test_target_switch();

    printf("\n[8] get_target\n");
    test_get_target_returns_set_values();

    printf("\n[9] FPGA target read & edge cases\n");
    test_read_target_clears_on_unavailable();
    test_both_unknown_returns_none();

    printf("\n[10] Boundary conditions\n");
    test_grab_coord_boundary_zero();
    test_skip_clears_old_grab();
    test_invalid_obs_clears_old_grab();
    test_set_target_clears_old_grab();

    printf("\n[11] Four-task modes (MODE_1..4)\n");
    test_set_target_ex_auto_locks();
    test_mode1_exact_color_shape();
    test_mode1_size_ignored();
    test_mode1_color_mismatch_skip();
    test_mode2_exact_all_match();
    test_mode2_size_mismatch_skip();
    test_mode2_size_unavailable_none();
    test_mode3_delta_10mm_match();
    test_mode3_delta_10mm_reverse();
    test_mode3_delta_not_10mm_skip();
    test_mode3_ref_not_set_none();
    test_mode3_size_unavailable_none();
    test_mode4_exact_size_match();
    test_mode4_delta_5mm_match();
    test_mode4_delta_exceeds_5mm_skip();
    test_mode4_size_unavailable_none();
    test_mode3_color_mismatch_skip();
    test_mode4_shape_mismatch_skip();

    printf("\n[12] Five-color targets (WHITE/BLACK)\n");
    test_white_target_match();
    test_black_target_match();
    test_white_not_wildcard();
    test_black_not_wildcard();
    test_white_not_unknown();

    printf("\n[13] Round state machine (one-round-one-transaction)\n");
    test_round_lock_prevents_double_grab();
    test_next_round_unlocks();
    test_round_reset_clears_all();
    test_round_state_transitions();
    test_skip_does_not_lock_round();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           _test_count - _test_failures, _test_count, _test_failures);
    return _test_failures ? 1 : 0;
}
