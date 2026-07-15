#include <stdio.h>

#include "single_camera_classifier.h"

static int checks;
static int failures;

#define CHECK(expr) do { checks++; if (!(expr)) { failures++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } } while (0)

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

    CHECK(sc_classify_features(&cylinder, 0, &result) == 0);
    CHECK(result.shape == SC_SHAPE_CYLINDER && result.stable);
    CHECK(sc_classify_features(&cone, 0, &result) == 0);
    CHECK(result.shape == SC_SHAPE_CONE && result.stable);
    CHECK(sc_classify_features(&no_foreground, 0, &result) == 0);
    CHECK(result.shape == SC_SHAPE_UNKNOWN && !result.stable);
    CHECK(sc_classify_features(&unknown_color, 0, &result) == 0);
    CHECK(result.color == SC_COLOR_UNKNOWN && !result.stable);
}

int main(void)
{
    test_five_colors_and_cube_priority();
    test_non_cube_and_missing_features_are_safe();
    printf("single_camera_classifier: %d/%d passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
