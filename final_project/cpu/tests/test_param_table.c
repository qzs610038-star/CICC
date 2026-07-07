/*==========================================================================
 *  test_param_table.c  —  param_table 模块单元测试
 *
 *  编译 (from tests/):
 *    gcc -std=c99 -Wall -Wextra \
 *        -DAPB_VISION_BASE_PLACEHOLDER=0xF0000000u \
 *        -I../app/include \
 *        test_param_table.c ../app/src/param_table.c ../app/src/vision_classifier.c \
 *        -o test_param_table.exe && ./test_param_table.exe
 *==========================================================================*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "param_table.h"

/*--------------------------------------------------------------------------
 *  简易测试框架
 *--------------------------------------------------------------------------*/
static int _test_failures  = 0;
static int _test_count     = 0;
static int _test_start     = 0;   /* snapshot at TEST() call */

static void _check(const char *file, int line,
                   int cond, const char *msg)
{
    _test_count++;
    if (!cond) {
        _test_failures++;
        printf("  FAIL [%s:%d] %s\n", file, line, msg);
    }
}
#define CHECK(cond) _check(__FILE__, __LINE__, (cond), #cond)
#define CHECK_EQ(a, b) _check(__FILE__, __LINE__, ((a) == (b)), #a " == " #b)
#define CHECK_NE(a, b) _check(__FILE__, __LINE__, ((a) != (b)), #a " != " #b)

#define TEST(name) \
    printf("  %-55s", name " "); fflush(stdout); _test_start = _test_failures

#define PASS() \
    do { \
        int delta = _test_failures - _test_start; \
        if (delta == 0) { printf("PASS\n"); } \
        else { printf("%d FAILED\n", delta); } \
    } while(0)

/*==========================================================================
 *  Helper: make a valid default config with one tweakable field
 *==========================================================================*/
static classifier_cfg_t valid_cfg(void)
{
    classifier_cfg_t c;
    classifier_cfg_default(&c);
    return c;
}

/*==========================================================================
 *  测试组 1: 初始化
 *==========================================================================*/

static void test_init_returns_default(void)
{
    TEST("init: get() returns valid default config");
    param_table_init();
    const classifier_cfg_t *c = param_table_get();
    CHECK(c != 0);
    CHECK(c->filter_window == 5);
    CHECK(c->filter_confirm == 3);
    CHECK(c->white_luma_ratio > c->black_luma_ratio);
    PASS();
}

static void test_init_not_calibrated(void)
{
    TEST("init: is_calibrated() == 0 before set");
    param_table_init();
    CHECK(param_table_is_calibrated() == 0);
    PASS();
}

static void test_init_nvm_load_returns_neg1(void)
{
    TEST("init: NVM load returns -1 (NVM not available)");
    param_table_init();
    int rc = param_table_load_calibrated();
    CHECK(rc == -1);
    PASS();
}

static void test_init_nvm_save_returns_neg1(void)
{
    TEST("init: NVM save returns -1 (no data)");
    param_table_init();
    int rc = param_table_save_calibrated();
    CHECK(rc == -1);
    PASS();
}

/*==========================================================================
 *  测试组 2: 校验 — 颜色阈值
 *==========================================================================*/

static void test_validate_min_area_too_large(void)
{
    TEST("validate: min_red_area > 10M → ERR_THRESHOLD");
    classifier_cfg_t c = valid_cfg();
    c.min_red_area = 20000000u;
    CHECK(param_table_validate(&c) == PARAM_ERR_THRESHOLD);
    PASS();
}

static void test_validate_min_area_zero_ok(void)
{
    TEST("validate: min_*_area=0 is valid (disable threshold)");
    classifier_cfg_t c = valid_cfg();
    c.min_red_area  = 0;
    c.min_blue_area = 0;
    c.min_yel_area  = 0;
    CHECK(param_table_validate(&c) == PARAM_OK);
    PASS();
}

/*==========================================================================
 *  测试组 3: 校验 — 亮度比
 *==========================================================================*/

static void test_validate_white_luma_gt_1(void)
{
    TEST("validate: white_luma_ratio > 1.0 → ERR_LUMA_RATIO");
    classifier_cfg_t c = valid_cfg();
    c.white_luma_ratio = 1.5f;
    CHECK(param_table_validate(&c) == PARAM_ERR_LUMA_RATIO);
    PASS();
}

static void test_validate_black_luma_lt_0(void)
{
    TEST("validate: black_luma_ratio < 0 → ERR_LUMA_RATIO");
    classifier_cfg_t c = valid_cfg();
    c.black_luma_ratio = -0.1f;
    CHECK(param_table_validate(&c) == PARAM_ERR_LUMA_RATIO);
    PASS();
}

static void test_validate_white_le_black(void)
{
    TEST("validate: white_luma_ratio <= black → ERR_LUMA_RATIO");
    classifier_cfg_t c = valid_cfg();
    c.white_luma_ratio = 0.3f;
    c.black_luma_ratio = 0.3f;
    CHECK(param_table_validate(&c) == PARAM_ERR_LUMA_RATIO);

    c.black_luma_ratio = 0.5f;
    CHECK(param_table_validate(&c) == PARAM_ERR_LUMA_RATIO);
    PASS();
}

/*==========================================================================
 *  测试组 4: 校验 — 宽高比
 *==========================================================================*/

static void test_validate_ratio_lo_ge_hi(void)
{
    TEST("validate: cube_ratio_lo >= hi → ERR_RATIO_RANGE");
    classifier_cfg_t c = valid_cfg();
    c.cube_ratio_lo = 1000;
    c.cube_ratio_hi = 1000;
    CHECK(param_table_validate(&c) == PARAM_ERR_RATIO_RANGE);

    c.cube_ratio_lo = 1100;
    c.cube_ratio_hi = 900;
    CHECK(param_table_validate(&c) == PARAM_ERR_RATIO_RANGE);
    PASS();
}

static void test_validate_ratio_zero(void)
{
    TEST("validate: cube_ratio_lo=0 → ERR_RATIO_RANGE");
    classifier_cfg_t c = valid_cfg();
    c.cube_ratio_lo = 0;
    CHECK(param_table_validate(&c) == PARAM_ERR_RATIO_RANGE);
    PASS();
}

static void test_validate_ratio_out_of_bounds(void)
{
    TEST("validate: cube_ratio_lo < 500 → ERR_RATIO_RANGE");
    classifier_cfg_t c = valid_cfg();
    c.cube_ratio_lo = 400;
    CHECK(param_table_validate(&c) == PARAM_ERR_RATIO_RANGE);
    PASS();
}

static void test_validate_ratio_bounds_hi(void)
{
    TEST("validate: cube_ratio_hi > 2000 → ERR_RATIO_RANGE");
    classifier_cfg_t c = valid_cfg();
    c.cube_ratio_hi = 2500;
    CHECK(param_table_validate(&c) == PARAM_ERR_RATIO_RANGE);
    PASS();
}

/*==========================================================================
 *  测试组 5: 校验 — 填充率
 *==========================================================================*/

static void test_validate_fill_cube_too_low(void)
{
    TEST("validate: cube_fill_lo < 0.50 → ERR_FILL_RANGE");
    classifier_cfg_t c = valid_cfg();
    c.cube_fill_lo = 0.40f;
    CHECK(param_table_validate(&c) == PARAM_ERR_FILL_RANGE);
    PASS();
}

static void test_validate_fill_cube_too_high(void)
{
    TEST("validate: cube_fill_lo > 0.99 → ERR_FILL_RANGE");
    classifier_cfg_t c = valid_cfg();
    c.cube_fill_lo = 1.0f;
    CHECK(param_table_validate(&c) == PARAM_ERR_FILL_RANGE);
    PASS();
}

static void test_validate_fill_cyl_ge_cube(void)
{
    TEST("validate: cyl_fill_lo >= cube_fill_lo → ERR_FILL_RANGE");
    classifier_cfg_t c = valid_cfg();
    c.cyl_fill_lo  = 0.85f;
    c.cube_fill_lo = 0.85f;
    CHECK(param_table_validate(&c) == PARAM_ERR_FILL_RANGE);

    c.cube_fill_lo = 0.80f;
    CHECK(param_table_validate(&c) == PARAM_ERR_FILL_RANGE);
    PASS();
}

/*==========================================================================
 *  测试组 6: 校验 — 尺寸查表
 *==========================================================================*/

static void test_validate_height_px_monotonic(void)
{
    TEST("validate: height_px not monotonic → ERR_SIZE_TABLE");
    classifier_cfg_t c = valid_cfg();
    c.height_px_20mm = 200;
    c.height_px_25mm = 100;
    CHECK(param_table_validate(&c) == PARAM_ERR_SIZE_TABLE);

    c.height_px_20mm = 100;
    c.height_px_25mm = 200;
    c.height_px_30mm = 150;
    CHECK(param_table_validate(&c) == PARAM_ERR_SIZE_TABLE);
    PASS();
}

static void test_validate_height_px_zero(void)
{
    TEST("validate: height_px_20mm < 5 → ERR_SIZE_TABLE");
    classifier_cfg_t c = valid_cfg();
    c.height_px_20mm = 0;
    CHECK(param_table_validate(&c) == PARAM_ERR_SIZE_TABLE);
    PASS();
}

static void test_validate_height_px_boundary_ok(void)
{
    TEST("validate: height_px at boundary is valid");
    classifier_cfg_t c = valid_cfg();
    c.height_px_20mm = 5;
    c.height_px_25mm = 10;
    c.height_px_30mm = 15;
    CHECK(param_table_validate(&c) == PARAM_OK);
    PASS();
}

/*==========================================================================
 *  测试组 7: 校验 — 滤波窗口
 *==========================================================================*/

static void test_validate_window_zero(void)
{
    TEST("validate: filter_window=0 → ERR_WINDOW");
    classifier_cfg_t c = valid_cfg();
    c.filter_window = 0;
    CHECK(param_table_validate(&c) == PARAM_ERR_WINDOW);
    PASS();
}

static void test_validate_window_gt_8(void)
{
    TEST("validate: filter_window=9 → ERR_WINDOW");
    classifier_cfg_t c = valid_cfg();
    c.filter_window = 9;
    CHECK(param_table_validate(&c) == PARAM_ERR_WINDOW);
    PASS();
}

static void test_validate_confirm_gt_window(void)
{
    TEST("validate: filter_confirm > filter_window → ERR_WINDOW");
    classifier_cfg_t c = valid_cfg();
    c.filter_window  = 3;
    c.filter_confirm = 4;
    CHECK(param_table_validate(&c) == PARAM_ERR_WINDOW);
    PASS();
}

static void test_validate_window_boundary_ok(void)
{
    TEST("validate: window=1 confirm=1 is valid (no filtering)");
    classifier_cfg_t c = valid_cfg();
    c.filter_window  = 1;
    c.filter_confirm = 1;
    CHECK(param_table_validate(&c) == PARAM_OK);
    PASS();
}

static void test_validate_window_8_ok(void)
{
    TEST("validate: window=8 confirm=8 is valid (max strict)");
    classifier_cfg_t c = valid_cfg();
    c.filter_window  = 8;
    c.filter_confirm = 8;
    CHECK(param_table_validate(&c) == PARAM_OK);
    PASS();
}

/*==========================================================================
 *  测试组 8: param_table_set / get
 *==========================================================================*/

static void test_set_valid_returns_ok(void)
{
    TEST("set: valid config → PARAM_OK");
    param_table_init();
    classifier_cfg_t c = valid_cfg();
    c.filter_window = 4;
    int rc = param_table_set(PARAM_SLOT_CALIBRATED, &c);
    CHECK(rc == PARAM_OK);
    PASS();
}

static void test_set_invalid_refused(void)
{
    TEST("set: invalid config → error code, slot unchanged");
    param_table_init();
    const classifier_cfg_t *before = param_table_get();
    uint8_t orig_window = before->filter_window;

    classifier_cfg_t bad = valid_cfg();
    bad.filter_window = 0;
    int rc = param_table_set(PARAM_SLOT_CALIBRATED, &bad);
    CHECK(rc == PARAM_ERR_WINDOW);

    /* Default slot should still be active */
    const classifier_cfg_t *after = param_table_get();
    CHECK(after->filter_window == orig_window);
    PASS();
}

static void test_set_calibrated_sets_flag(void)
{
    TEST("set: calibrated slot sets is_calibrated()");
    param_table_init();
    CHECK(param_table_is_calibrated() == 0);

    classifier_cfg_t c = valid_cfg();
    (void)param_table_set(PARAM_SLOT_CALIBRATED, &c);
    CHECK(param_table_is_calibrated() == 1);
    PASS();
}

static void test_set_default_slot_no_flag(void)
{
    TEST("set: default slot does NOT set is_calibrated()");
    param_table_init();
    CHECK(param_table_is_calibrated() == 0);

    classifier_cfg_t c = valid_cfg();
    c.filter_window = 6;
    (void)param_table_set(PARAM_SLOT_DEFAULT, &c);
    CHECK(param_table_is_calibrated() == 0);
    PASS();
}

/*==========================================================================
 *  测试组 9: 标定流程
 *==========================================================================*/

static void test_calibration_flow(void)
{
    TEST("calibration: enter → set → get returns calibrated");
    param_table_init();

    /* Before calibration: get returns default with window=5 */
    CHECK(param_table_get()->filter_window == 5);

    /* Enter calibration — copies default to slot 1 */
    param_table_enter_calibration();
    CHECK(param_table_is_calibrated() == 1);
    CHECK(param_table_get()->filter_window == 5);  /* still 5, copied */

    /* Tweak a value */
    classifier_cfg_t tweaked;
    memcpy(&tweaked, param_table_get(), sizeof(tweaked));
    tweaked.filter_window = 7;
    tweaked.cube_fill_lo  = 0.88f;

    int rc = param_table_set(PARAM_SLOT_CALIBRATED, &tweaked);
    CHECK(rc == PARAM_OK);

    const classifier_cfg_t *active = param_table_get();
    CHECK(active->filter_window == 7);
    CHECK(active->cube_fill_lo == 0.88f);
    PASS();
}

static void test_calibration_does_not_affect_default(void)
{
    TEST("calibration: slot 0 write does not steal active from slot 1");
    param_table_init();

    param_table_enter_calibration();
    classifier_cfg_t c;
    memcpy(&c, param_table_get(), sizeof(c));
    c.filter_window  = 2;
    c.filter_confirm = 2;   /* must satisfy confirm <= window */
    (void)param_table_set(PARAM_SLOT_CALIBRATED, &c);

    /* Now get() returns calibrated (window=2) */
    CHECK(param_table_get()->filter_window == 2);

    /* Writing to slot 0 must NOT steal active from slot 1
     * when calibrated data exists (even during calibration). */
    classifier_cfg_t def_cfg;
    classifier_cfg_default(&def_cfg);
    (void)param_table_set(PARAM_SLOT_DEFAULT, &def_cfg);
    CHECK(param_table_get()->filter_window == 2);   /* still slot 1 */

    /* Exit calibration: calibrating flag cleared, slot 1 still active */
    param_table_exit_calibration();
    CHECK(param_table_get()->filter_window == 2);   /* slot 1 still active */

    /* Writing to slot 0 after exiting calibration also doesn't steal active */
    (void)param_table_set(PARAM_SLOT_DEFAULT, &def_cfg);
    CHECK(param_table_get()->filter_window == 2);   /* slot 1 still active */
    PASS();
}

/*==========================================================================
 *  测试组 10: 完整校验覆盖
 *==========================================================================*/

static void test_validate_all_ok(void)
{
    TEST("validate: default config is valid");
    classifier_cfg_t c;
    classifier_cfg_default(&c);
    CHECK(param_table_validate(&c) == PARAM_OK);
    PASS();
}

static void test_validate_cyl_ratio_errors(void)
{
    TEST("validate: cyl_ratio errors caught");
    classifier_cfg_t c = valid_cfg();

    /* cyl_ratio_lo == 0 */
    c.cyl_ratio_lo = 0;
    CHECK(param_table_validate(&c) == PARAM_ERR_RATIO_RANGE);

    /* cyl_ratio_lo >= hi */
    c = valid_cfg();
    c.cyl_ratio_lo = 1500;
    c.cyl_ratio_hi = 1200;
    CHECK(param_table_validate(&c) == PARAM_ERR_RATIO_RANGE);

    /* cyl_ratio_lo < 400 */
    c = valid_cfg();
    c.cyl_ratio_lo = 300;
    c.cyl_ratio_hi = 500;
    CHECK(param_table_validate(&c) == PARAM_ERR_RATIO_RANGE);

    /* cyl_ratio_hi > 2000 */
    c = valid_cfg();
    c.cyl_ratio_lo = 500;
    c.cyl_ratio_hi = 2500;
    CHECK(param_table_validate(&c) == PARAM_ERR_RATIO_RANGE);
    PASS();
}

static void test_validate_cyl_fill_bound(void)
{
    TEST("validate: cyl_fill_lo < 0.30 → ERR_FILL_RANGE");
    classifier_cfg_t c = valid_cfg();
    c.cyl_fill_lo = 0.25f;
    CHECK(param_table_validate(&c) == PARAM_ERR_FILL_RANGE);
    PASS();
}

static void test_validate_height_px_over(void)
{
    TEST("validate: height_px_30mm > 4095 → ERR_SIZE_TABLE");
    classifier_cfg_t c = valid_cfg();
    c.height_px_20mm = 100;
    c.height_px_25mm = 200;
    c.height_px_30mm = 5000;
    CHECK(param_table_validate(&c) == PARAM_ERR_SIZE_TABLE);
    PASS();
}

static void test_validate_confirm_zero(void)
{
    TEST("validate: filter_confirm=0 → ERR_WINDOW");
    classifier_cfg_t c = valid_cfg();
    c.filter_confirm = 0;
    CHECK(param_table_validate(&c) == PARAM_ERR_WINDOW);
    PASS();
}

/*==========================================================================
 *  测试组 11: strerror
 *==========================================================================*/

static void test_strerror_coverage(void)
{
    TEST("strerror: all known codes return non-null");
    CHECK(param_table_strerror(PARAM_OK)              != 0);
    CHECK(param_table_strerror(PARAM_ERR_RATIO_RANGE) != 0);
    CHECK(param_table_strerror(PARAM_ERR_FILL_RANGE)  != 0);
    CHECK(param_table_strerror(PARAM_ERR_SIZE_TABLE)  != 0);
    CHECK(param_table_strerror(PARAM_ERR_THRESHOLD)   != 0);
    CHECK(param_table_strerror(PARAM_ERR_WINDOW)      != 0);
    CHECK(param_table_strerror(PARAM_ERR_LUMA_RATIO)  != 0);
    CHECK(param_table_strerror(999)                   != 0);  /* unknown */
    PASS();
}

static void test_strerror_distinct(void)
{
    TEST("strerror: different codes → different strings");
    const char *a = param_table_strerror(PARAM_ERR_RATIO_RANGE);
    const char *b = param_table_strerror(PARAM_ERR_FILL_RANGE);
    CHECK(strcmp(a, b) != 0);
    PASS();
}

/*==========================================================================
 *  测试组 12: 边界 & 重置
 *==========================================================================*/

static void test_set_then_init_resets(void)
{
    TEST("boundary: param_table_init() resets calibrated state");
    param_table_init();

    classifier_cfg_t c = valid_cfg();
    c.filter_window = 8;
    (void)param_table_set(PARAM_SLOT_CALIBRATED, &c);
    CHECK(param_table_get()->filter_window == 8);

    /* Re-init should reset */
    param_table_init();
    CHECK(param_table_get()->filter_window == 5);  /* back to default */
    CHECK(param_table_is_calibrated() == 0);
    PASS();
}

static void test_validate_validator_twice_idempotent(void)
{
    TEST("boundary: validate same config twice → same result");
    classifier_cfg_t c = valid_cfg();
    int r1 = param_table_validate(&c);
    int r2 = param_table_validate(&c);
    CHECK(r1 == PARAM_OK);
    CHECK(r2 == PARAM_OK);
    PASS();
}

/*==========================================================================
 *  main
 *==========================================================================*/

int main(void)
{
    printf("\n=== param_table unit tests ===\n\n");

    printf("[1] Initialization\n");
    test_init_returns_default();
    test_init_not_calibrated();
    test_init_nvm_load_returns_neg1();
    test_init_nvm_save_returns_neg1();

    printf("\n[2] Validation — color thresholds\n");
    test_validate_min_area_too_large();
    test_validate_min_area_zero_ok();

    printf("\n[3] Validation — luma ratios\n");
    test_validate_white_luma_gt_1();
    test_validate_black_luma_lt_0();
    test_validate_white_le_black();

    printf("\n[4] Validation — aspect ratios\n");
    test_validate_ratio_lo_ge_hi();
    test_validate_ratio_zero();
    test_validate_ratio_out_of_bounds();
    test_validate_ratio_bounds_hi();

    printf("\n[5] Validation — fill rates\n");
    test_validate_fill_cube_too_low();
    test_validate_fill_cube_too_high();
    test_validate_fill_cyl_ge_cube();

    printf("\n[6] Validation — size lookup table\n");
    test_validate_height_px_monotonic();
    test_validate_height_px_zero();
    test_validate_height_px_boundary_ok();

    printf("\n[7] Validation — filter window\n");
    test_validate_window_zero();
    test_validate_window_gt_8();
    test_validate_confirm_gt_window();
    test_validate_window_boundary_ok();
    test_validate_window_8_ok();

    printf("\n[8] Set / Get\n");
    test_set_valid_returns_ok();
    test_set_invalid_refused();
    test_set_calibrated_sets_flag();
    test_set_default_slot_no_flag();

    printf("\n[9] Calibration flow\n");
    test_calibration_flow();
    test_calibration_does_not_affect_default();

    printf("\n[10] Full validation sweep\n");
    test_validate_all_ok();
    test_validate_cyl_ratio_errors();
    test_validate_cyl_fill_bound();
    test_validate_height_px_over();
    test_validate_confirm_zero();

    printf("\n[11] strerror\n");
    test_strerror_coverage();
    test_strerror_distinct();

    printf("\n[12] Boundary & reset\n");
    test_set_then_init_resets();
    test_validate_validator_twice_idempotent();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           _test_count - _test_failures, _test_count, _test_failures);
    return _test_failures ? 1 : 0;
}
