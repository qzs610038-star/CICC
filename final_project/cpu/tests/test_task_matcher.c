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
    CHECK_EQ(task_matcher_evaluate(&o2, 0, 0), MATCH_ACTION_GRAB);
    CHECK_EQ(task_matcher_evaluate(&o3, 0, 0), MATCH_ACTION_GRAB);

    /* Wrong shape → SKIP */
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
    CHECK_EQ(task_matcher_evaluate(&o2, 0, 0), MATCH_ACTION_GRAB);
    CHECK_EQ(task_matcher_evaluate(&o3, 0, 0), MATCH_ACTION_GRAB);

    /* Wrong color → SKIP */
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
    CHECK_EQ(task_matcher_evaluate(&o2, 0, 0), MATCH_ACTION_GRAB);
    CHECK_EQ(task_matcher_evaluate(&o3, 0, 0), MATCH_ACTION_GRAB);

    /* size=0 observation (Cam0 only) also matches wildcard */
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
    CHECK_EQ(task_matcher_evaluate(&o2, 0, 0), MATCH_ACTION_GRAB);
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
    CHECK_EQ(task_matcher_evaluate(&obs_blu_cyl_25,  0, 0), MATCH_ACTION_SKIP);

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
    TEST("boundary: invalid obs after GRAB invalidates old grab coords");
    task_matcher_init();
    task_target_t t = make_target(COLOR_RED, SHAPE_CUBE, 20);
    task_matcher_set_target(&t);

    vision_result_t good = make_obs(COLOR_RED, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&good, 111, 222), MATCH_ACTION_GRAB);

    /* Then: UNKNOWN obs → NONE (production mode), but grab still cleared */
    vision_result_t bad = make_obs(COLOR_UNKNOWN, SHAPE_CUBE, 20);
    CHECK_EQ(task_matcher_evaluate(&bad, 0, 0), MATCH_ACTION_NONE);

    uint16_t cx = 99, cy = 99;
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

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           _test_count - _test_failures, _test_count, _test_failures);
    return _test_failures ? 1 : 0;
}
