#include <stdio.h>

#include "single_camera_classifier.h"

static int checks;
static int failures;

static void record_check(int condition, int line, const char *expression)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL %s:%d: %s\n", __FILE__, line, expression);
    }
}

#define CHECK(expr) record_check(!!(expr), __LINE__, #expr)

static sc_features_t features(uint32_t red, uint32_t blue, uint32_t yellow,
                              uint32_t foreground, uint16_t width, uint16_t height,
                              uint32_t luma, uint32_t pixels)
{
    sc_features_t value = { red, blue, yellow, foreground, pixels, luma,
                            width, height };
    return value;
}

static void test_five_colors_and_cube_priority(void)
{
    sc_observation_t result;
    sc_features_t red_cube = features(900u, 20u, 15u, 900u, 30u, 30u, 36000u, 200u);
    sc_features_t blue_cube = features(10u, 900u, 10u, 900u, 30u, 30u, 36000u, 200u);
    sc_features_t yellow_cube = features(10u, 10u, 900u, 900u, 30u, 30u, 36000u, 200u);
    sc_features_t white_cube = features(20u, 20u, 20u, 900u, 30u, 30u, 40000u, 200u);
    sc_features_t black_cube = features(20u, 20u, 20u, 900u, 30u, 30u, 6000u, 200u);

    CHECK(sc_classify_features(&red_cube, 0, &result) == 0);
    CHECK(result.color == SC_COLOR_RED && result.shape == SC_SHAPE_CUBE && result.stable);
    CHECK(sc_classify_features(&blue_cube, 0, &result) == 0);
    CHECK(result.color == SC_COLOR_BLUE && result.shape == SC_SHAPE_CUBE && result.stable);
    CHECK(sc_classify_features(&yellow_cube, 0, &result) == 0);
    CHECK(result.color == SC_COLOR_YELLOW && result.shape == SC_SHAPE_CUBE && result.stable);
    CHECK(sc_classify_features(&white_cube, 0, &result) == 0);
    CHECK(result.color == SC_COLOR_WHITE && result.shape == SC_SHAPE_CUBE && result.stable);
    CHECK(sc_classify_features(&black_cube, 0, &result) == 0);
    CHECK(result.color == SC_COLOR_BLACK && result.shape == SC_SHAPE_CUBE && result.stable);
}

static void test_non_cube_and_missing_features_are_safe(void)
{
    sc_observation_t result;
    sc_features_t cylinder = features(900u, 10u, 10u, 700u, 30u, 30u, 36000u, 200u);
    sc_features_t cone = features(900u, 10u, 10u, 300u, 30u, 30u, 36000u, 200u);
    sc_features_t no_foreground = features(900u, 10u, 10u, 0u, 30u, 30u, 36000u, 200u);
    sc_features_t unknown_color = features(20u, 20u, 20u, 900u, 30u, 30u, 24000u, 200u);

    /* Classifier labels CYLINDER/CONE as stable (classifier produced a shape).
       F1 phase-A gate treats non-CUBE as WAIT, not SKIP.
       Production reliability requires field calibration. */
    CHECK(sc_classify_features(&cylinder, 0, &result) == 0);
    CHECK(result.shape == SC_SHAPE_CYLINDER);
    CHECK(SC_SHAPE_IS_DIAGNOSTIC_NON_CUBE(result.shape));
    CHECK(result.stable);  /* classifier-level stable, not F1 business-reliable */
    CHECK(sc_classify_features(&cone, 0, &result) == 0);
    CHECK(result.shape == SC_SHAPE_CONE);
    CHECK(SC_SHAPE_IS_DIAGNOSTIC_NON_CUBE(result.shape));
    CHECK(result.stable);
    CHECK(sc_classify_features(&no_foreground, 0, &result) == 0);
    CHECK(result.shape == SC_SHAPE_UNKNOWN && !result.stable);
    CHECK(sc_classify_features(&unknown_color, 0, &result) == 0);
    CHECK(result.color == SC_COLOR_UNKNOWN && !result.stable);
}

static void test_white_black_foreground_luma_dependency(void)
{
    sc_observation_t result;

    /* White requires sufficient foreground_area, roi_pixel_count, and sum_luma.
       Default white_mean_luma_min = 180. */
    {
        sc_features_t white_good = features(20u, 20u, 20u, 900u, 30u, 30u, 40000u, 200u);
        CHECK(sc_classify_features(&white_good, 0, &result) == 0);
        CHECK(result.color == SC_COLOR_WHITE && result.stable);
    }
    /* mean_luma = 24000/200 = 120 — too low for white, and no color channel hits
       the minimum area threshold of 500 either. */
    {
        sc_features_t not_white = features(20u, 20u, 20u, 900u, 30u, 30u, 24000u, 200u);
        CHECK(sc_classify_features(&not_white, 0, &result) == 0);
        CHECK(result.color == SC_COLOR_UNKNOWN && !result.stable);
    }
    /* Black requires low mean_luma. Default black_mean_luma_max = 55. */
    {
        sc_features_t black_good = features(20u, 20u, 20u, 900u, 30u, 30u, 6000u, 200u);
        CHECK(sc_classify_features(&black_good, 0, &result) == 0);
        CHECK(result.color == SC_COLOR_BLACK && result.stable);
    }
    /* mean_luma = 12000/200 = 60 — above black threshold, below white, no color. */
    {
        sc_features_t not_black = features(20u, 20u, 20u, 900u, 30u, 30u, 12000u, 200u);
        CHECK(sc_classify_features(&not_black, 0, &result) == 0);
        CHECK(result.color == SC_COLOR_UNKNOWN && !result.stable);
    }
    /* White/black must not fire when roi_pixel_count is zero. */
    {
        sc_features_t zero_roi = features(0u, 0u, 0u, 0u, 0u, 0u, 40000u, 0u);
        CHECK(sc_classify_features(&zero_roi, 0, &result) == 0);
        CHECK(result.color == SC_COLOR_UNKNOWN && !result.stable);
    }
}

static void test_shape_at_fill_boundaries(void)
{
    sc_observation_t result;
    sc_classifier_cfg_t cfg;
    sc_classifier_cfg_default(&cfg);

    /* bbox_area = 30*30 = 900. Fill 900 → 1000 per mille → CUBE. */
    {
        sc_features_t f = features(900u, 0u, 0u, 900u, 30u, 30u, 36000u, 200u);
        CHECK(sc_classify_features(&f, &cfg, &result) == 0);
        CHECK(result.shape == SC_SHAPE_CUBE);
    }
    /* Fill 800 fg in 900 bbox = 888 per mille. CUBE min = 850 → CUBE. */
    {
        sc_features_t f = features(900u, 0u, 0u, 800u, 30u, 30u, 36000u, 200u);
        CHECK(sc_classify_features(&f, &cfg, &result) == 0);
        CHECK(result.shape == SC_SHAPE_CUBE);
    }
    /* Fill 750 fg in 900 bbox = 833 per mille. Below CUBE (850), above CYL (650) → CYLINDER. */
    {
        sc_features_t f = features(900u, 0u, 0u, 750u, 30u, 30u, 36000u, 200u);
        CHECK(sc_classify_features(&f, &cfg, &result) == 0);
        CHECK(result.shape == SC_SHAPE_CYLINDER);
    }
    /* Fill 300 fg in 900 bbox = 333 per mille. Below CYL (650), above CONE (250) → CONE. */
    {
        sc_features_t f = features(900u, 0u, 0u, 300u, 30u, 30u, 36000u, 200u);
        CHECK(sc_classify_features(&f, &cfg, &result) == 0);
        CHECK(result.shape == SC_SHAPE_CONE);
    }
    /* Fill 200 fg in 900 bbox = 222 per mille. Below CONE (250) → UNKNOWN. */
    {
        sc_features_t f = features(900u, 0u, 0u, 200u, 30u, 30u, 36000u, 200u);
        CHECK(sc_classify_features(&f, &cfg, &result) == 0);
        CHECK(result.shape == SC_SHAPE_UNKNOWN && !result.stable);
    }
}

static void test_non_cube_macro_semantics(void)
{
    /* Verify SC_SHAPE_IS_RELIABLE_CUBE and SC_SHAPE_IS_DIAGNOSTIC_NON_CUBE. */
    CHECK(SC_SHAPE_IS_RELIABLE_CUBE(SC_SHAPE_CUBE));
    CHECK(!SC_SHAPE_IS_RELIABLE_CUBE(SC_SHAPE_CYLINDER));
    CHECK(!SC_SHAPE_IS_RELIABLE_CUBE(SC_SHAPE_CONE));
    CHECK(!SC_SHAPE_IS_RELIABLE_CUBE(SC_SHAPE_UNKNOWN));

    CHECK(!SC_SHAPE_IS_DIAGNOSTIC_NON_CUBE(SC_SHAPE_CUBE));
    CHECK(SC_SHAPE_IS_DIAGNOSTIC_NON_CUBE(SC_SHAPE_CYLINDER));
    CHECK(SC_SHAPE_IS_DIAGNOSTIC_NON_CUBE(SC_SHAPE_CONE));
    CHECK(!SC_SHAPE_IS_DIAGNOSTIC_NON_CUBE(SC_SHAPE_UNKNOWN));
}

static void test_color_area_threshold_lower_bound(void)
{
    sc_observation_t result;
    sc_classifier_cfg_t cfg;
    sc_classifier_cfg_default(&cfg);
    /* Default min_red_area = 500. red=499, blue=0, yel=0 → no color reaches threshold.
       mean_luma = 36000/200 = 180 → exactly at white_min. */
    {
        sc_features_t below = features(499u, 0u, 0u, 900u, 30u, 30u, 36000u, 200u);
        CHECK(sc_classify_features(&below, &cfg, &result) == 0);
        CHECK(result.color == SC_COLOR_WHITE);
    }
    /* red=500 exactly at threshold. */
    {
        sc_features_t at_threshold = features(500u, 0u, 0u, 900u, 30u, 30u, 36000u, 200u);
        CHECK(sc_classify_features(&at_threshold, &cfg, &result) == 0);
        CHECK(result.color == SC_COLOR_RED);
    }
}

int main(void)
{
    test_five_colors_and_cube_priority();
    test_non_cube_and_missing_features_are_safe();
    test_white_black_foreground_luma_dependency();
    test_shape_at_fill_boundaries();
    test_non_cube_macro_semantics();
    test_color_area_threshold_lower_bound();
    printf("single_camera_classifier: %d/%d passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
