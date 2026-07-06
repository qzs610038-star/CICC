/*==========================================================================
 *  main.c  —  vision_classifier QEMU 裸机测试
 *
 *  编译: 见 build_qemu.bat
 *  运行: qemu-system-riscv32 -M spike -nographic -semihosting -kernel test.elf
 *  输出: printf → semihosting → QEMU 控制台
 *==========================================================================*/

#include <stdio.h>
#include <string.h>

/* 硬件占位，绕过 board_io.h 的 #error 守卫 */
#ifndef IO_APB_SLAVE_0_BASE
#define IO_APB_SLAVE_0_BASE  0x00000000u
#endif

#include "board_io.h"
#include "vision_classifier.h"

static int tests_run   = 0;
static int tests_pass  = 0;
static int tests_fail  = 0;

#define TEST(name)  do { tests_run++; printf("  %-50s", name); } while(0)
#define PASS()      do { tests_pass++; printf("PASS\n"); } while(0)
#define FAIL(fmt,...) do { tests_fail++; printf("FAIL: " fmt "\n", ##__VA_ARGS__); } while(0)

#define ASSERT_EQ(a, b, label)  do { \
    if ((a) != (b)) { FAIL("%s: expected %d, got %d", label, (int)(b), (int)(a)); return; } \
} while(0)

/*--------------------------------------------------------------------------
 *  构造 feature_snapshot_t
 *--------------------------------------------------------------------------*/
static feature_snapshot_t make_snap(uint32_t red, uint32_t blue, uint32_t yel,
                                     uint16_t bx0, uint16_t by0,
                                     uint16_t bx1, uint16_t by1,
                                     uint32_t height_px, uint16_t frame_id)
{
    feature_snapshot_t s;
    memset(&s, 0, sizeof(s));
    s.red_area  = red;
    s.blue_area = blue;
    s.yel_area  = yel;
    s.bbox_min  = ((uint32_t)by0 << 16) | bx0;
    s.bbox_max  = ((uint32_t)by1 << 16) | bx1;
    s.center    = ((uint32_t)((by0+by1)/2) << 16) | ((bx0+bx1)/2);
    s.height_px = height_px;
    s.frame_id  = frame_id;
    return s;
}

/*--------------------------------------------------------------------------
 *  测试组 1: 颜色分类
 *--------------------------------------------------------------------------*/
static void test_color_red(void)
{
    TEST("color: RED dominant");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(8000, 100, 50, 50,50, 200,200, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.color_id, COLOR_RED, "color_id");
    PASS();
}

static void test_color_blue(void)
{
    TEST("color: BLUE dominant");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(50, 9000, 30, 50,50, 200,200, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.color_id, COLOR_BLUE, "color_id");
    PASS();
}

static void test_color_yellow(void)
{
    TEST("color: YELLOW dominant");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(30, 20, 7000, 50,50, 200,200, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.color_id, COLOR_YELLOW, "color_id");
    PASS();
}

static void test_color_white_exclusion(void)
{
    TEST("color: WHITE via exclusion (>60% fill)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    /* bbox=25x25=625, sum=480, fr=0.768>0.60, max=180<200 → exclusion → WHITE */
    feature_snapshot_t s = make_snap(150, 150, 180, 50,50, 75,75, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.color_id, COLOR_WHITE, "color_id");
    PASS();
}

static void test_color_black_exclusion(void)
{
    TEST("color: BLACK via exclusion (<5% fill)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    cfg.black_luma_ratio = 0.05f;
    /* area=22500, sum=10, fr=0.0004<0.05 → BLACK */
    feature_snapshot_t s = make_snap(5, 3, 2, 50,50, 200,200, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.color_id, COLOR_BLACK, "color_id");
    PASS();
}

static void test_color_unknown_small(void)
{
    TEST("color: UNKNOWN (bbox too small)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(5, 3, 2, 50,50, 60,60, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.color_id, COLOR_UNKNOWN, "color_id");
    PASS();
}

/*--------------------------------------------------------------------------
 *  测试组 2: 形状分类
 *--------------------------------------------------------------------------*/
static void test_shape_cube(void)
{
    TEST("shape: CUBE (~1:1 ratio, >70% fill)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    /* 100x100 bbox, ratio=1000, fr=1.0 → cube */
    feature_snapshot_t s = make_snap(8000, 1000, 1000, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CUBE, "shape_id");
    PASS();
}

static void test_shape_cylinder(void)
{
    TEST("shape: CYLINDER (~1:1 ratio, 55-70% fill)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    /* ratio=1000, sum=6000, area=10000, fr=0.6 */
    feature_snapshot_t s = make_snap(5500, 300, 200, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CYLINDER, "shape_id");
    PASS();
}

static void test_shape_cone_topview(void)
{
    TEST("shape: CONE (~1:1 ratio, <55% fill)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(2000, 200, 100, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CONE, "shape_id");
    PASS();
}

static void test_shape_cylinder_side(void)
{
    TEST("shape: CYLINDER (elongated, >55% fill)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    /* bw=40, bh=56, ratio=1400 in [650,1500], fr=0.96 */
    feature_snapshot_t s = make_snap(2000, 100, 50, 50,50, 90,106, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CYLINDER, "shape_id");
    PASS();
}

static void test_shape_cone_side(void)
{
    TEST("shape: CONE (elongated, <55% fill)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    /* bw=30, bh=70, ratio=2333 > 1500 → extreme → CONE */
    feature_snapshot_t s = make_snap(500, 100, 50, 50,50, 80,120, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CONE, "shape_id");
    PASS();
}

static void test_shape_unknown_tiny(void)
{
    TEST("shape: UNKNOWN (bbox too small)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(20, 10, 5, 50,50, 60,60, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_UNKNOWN, "shape_id");
    PASS();
}

/*--------------------------------------------------------------------------
 *  测试组 3: 尺寸分类
 *--------------------------------------------------------------------------*/
static void test_size_cam0_area(void)
{
    TEST("size: Cam0 area-based (small → 2.0cm)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(2000, 100, 50, 50,50, 100,100, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.size_cm_x10, 20, "size_cm_x10");
    PASS();
}

static void test_size_cam1_height(void)
{
    TEST("size: Cam1 height_px → 2.5cm");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    cfg.height_px_20mm = 60;
    cfg.height_px_25mm = 75;
    cfg.height_px_30mm = 90;
    feature_snapshot_t s = make_snap(2000, 100, 50, 50,50, 100,100, 78, 1);
    vision_result_t r = classify_frame(&s, 1, &cfg);
    ASSERT_EQ(r.size_cm_x10, 25, "size_cm_x10");
    PASS();
}

static void test_size_cam1_height_zero_fallback(void)
{
    TEST("size: Cam1 height_px==0 → area fallback");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(8000, 200, 100, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 1, &cfg);
    ASSERT_EQ(r.size_cm_x10, 25, "size_cm_x10");
    PASS();
}

/*--------------------------------------------------------------------------
 *  测试组 4: 多帧滤波
 *--------------------------------------------------------------------------*/
static void test_filter_stabilize(void)
{
    TEST("filter: stabilizes after confirm frames");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    cfg.filter_window  = 5;
    cfg.filter_confirm = 3;

    mf_filter_t f;
    mf_filter_reset(&f);
    feature_snapshot_t s = make_snap(8000, 100, 50, 50,50, 150,150, 0, 1);
    vision_result_t raw = classify_frame(&s, 0, &cfg);

    mf_filter_update(&f, &raw, &cfg);
    mf_filter_update(&f, &raw, &cfg);
    vision_result_t r = mf_filter_update(&f, &raw, &cfg);
    ASSERT_EQ(r.color_id, COLOR_RED, "color_id after 3 frames");
    ASSERT_EQ(r.shape_id, SHAPE_CUBE, "shape_id after 3 frames");
    PASS();
}

static void test_filter_not_stabilized(void)
{
    TEST("filter: returns UNKNOWN before confirm");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    cfg.filter_window  = 5;
    cfg.filter_confirm = 3;

    mf_filter_t f;
    mf_filter_reset(&f);
    feature_snapshot_t s = make_snap(8000, 100, 50, 50,50, 150,150, 0, 1);
    vision_result_t raw = classify_frame(&s, 0, &cfg);

    mf_filter_update(&f, &raw, &cfg);
    vision_result_t r = mf_filter_update(&f, &raw, &cfg);
    ASSERT_EQ(r.color_id, COLOR_UNKNOWN, "color_id unstabilized");
    ASSERT_EQ(r.shape_id, SHAPE_UNKNOWN, "shape_id unstabilized");
    PASS();
}

static void test_filter_noise_causes_unknown(void)
{
    TEST("filter: noise → UNKNOWN (no coasting)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    cfg.filter_window  = 5;
    cfg.filter_confirm = 3;

    mf_filter_t f;
    mf_filter_reset(&f);

    feature_snapshot_t red_snap  = make_snap(8000, 100, 50,  50,50, 150,150, 0, 1);
    feature_snapshot_t blue_snap = make_snap(100, 8000, 50,  50,50, 150,150, 0, 2);
    vision_result_t red  = classify_frame(&red_snap, 0, &cfg);
    vision_result_t blue = classify_frame(&blue_snap, 0, &cfg);

    int i;
    for (i = 0; i < 5; i++) mf_filter_update(&f, &red, &cfg);
    /* Now inject noise */
    mf_filter_update(&f, &blue, &cfg);
    mf_filter_update(&f, &red,  &cfg);
    vision_result_t r = mf_filter_update(&f, &blue, &cfg);
    ASSERT_EQ(r.color_id, COLOR_UNKNOWN, "must be UNKNOWN not coasting");
    PASS();
}

/*--------------------------------------------------------------------------
 *  测试组 5: 两路融合
 *--------------------------------------------------------------------------*/
static void test_fuse_both_cams(void)
{
    TEST("fuse: Cam0 color+shape, Cam1 size");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    vision_result_t c0, c1;
    memset(&c0, 0, sizeof(c0));
    memset(&c1, 0, sizeof(c1));
    c0.color_id = COLOR_RED;    c0.shape_id = SHAPE_CUBE;
    c0.size_cm_x10 = 20;         c0.confidence = 200;
    c1.color_id = COLOR_RED;    c1.shape_id = SHAPE_CYLINDER;
    c1.size_cm_x10 = 30;         c1.confidence = 180;

    vision_result_t f = fuse_results(&c0, &c1, &cfg);
    ASSERT_EQ(f.color_id,    COLOR_RED,  "color");
    ASSERT_EQ(f.shape_id,    SHAPE_CUBE, "shape");
    ASSERT_EQ(f.size_cm_x10, 30,         "size (Cam1 wins)");
    ASSERT_EQ(f.confidence,  200,        "confidence");
    PASS();
}

static void test_fuse_cam1_null(void)
{
    TEST("fuse: cam1==NULL → no crash");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    vision_result_t c0;
    memset(&c0, 0, sizeof(c0));
    c0.color_id = COLOR_BLUE; c0.shape_id = SHAPE_CONE;
    c0.size_cm_x10 = 25; c0.confidence = 150;

    vision_result_t f = fuse_results(&c0, 0, &cfg);
    ASSERT_EQ(f.color_id,    COLOR_BLUE, "color");
    ASSERT_EQ(f.shape_id,    SHAPE_CONE, "shape");
    ASSERT_EQ(f.confidence,  150,        "confidence");
    PASS();
}

static void test_fuse_cam0_null(void)
{
    TEST("fuse: cam0==NULL → no crash");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    vision_result_t c1;
    memset(&c1, 0, sizeof(c1));
    c1.color_id = COLOR_YELLOW; c1.shape_id = SHAPE_CYLINDER;
    c1.size_cm_x10 = 30; c1.confidence = 220;

    vision_result_t f = fuse_results(0, &c1, &cfg);
    ASSERT_EQ(f.color_id,    COLOR_YELLOW,   "color");
    ASSERT_EQ(f.shape_id,    SHAPE_CYLINDER, "shape");
    ASSERT_EQ(f.confidence,  220,            "confidence");
    PASS();
}

/*--------------------------------------------------------------------------
 *  测试组 6: 边界条件
 *--------------------------------------------------------------------------*/
static void test_fill_ratio_clamped(void)
{
    TEST("boundary: fill_ratio clamped to 1.0");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(50000, 30000, 20000, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CUBE, "shape_id (clamped)");
    PASS();
}

static void test_config_window_bounds(void)
{
    TEST("boundary: filter_window=0 protected");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    cfg.filter_window  = 0;
    cfg.filter_confirm = 10;

    mf_filter_t f;
    mf_filter_reset(&f);
    feature_snapshot_t s = make_snap(8000, 100, 50, 50,50, 150,150, 0, 1);
    vision_result_t raw = classify_frame(&s, 0, &cfg);

    int i;
    for (i = 0; i < 5; i++) mf_filter_update(&f, &raw, &cfg);
    vision_result_t r = mf_filter_update(&f, &raw, &cfg);
    ASSERT_EQ(r.color_id, COLOR_RED, "not crashed");
    PASS();
}

/*--------------------------------------------------------------------------
 *  main
 *--------------------------------------------------------------------------*/
int main(void)
{
    printf("=== vision_classifier QEMU unit tests ===\n\n");

    printf("[1] Color classification\n");
    test_color_red();
    test_color_blue();
    test_color_yellow();
    test_color_white_exclusion();
    test_color_black_exclusion();
    test_color_unknown_small();

    printf("\n[2] Shape classification\n");
    test_shape_cube();
    test_shape_cylinder();
    test_shape_cone_topview();
    test_shape_cylinder_side();
    test_shape_cone_side();
    test_shape_unknown_tiny();

    printf("\n[3] Size classification\n");
    test_size_cam0_area();
    test_size_cam1_height();
    test_size_cam1_height_zero_fallback();

    printf("\n[4] Multi-frame filter\n");
    test_filter_stabilize();
    test_filter_not_stabilized();
    test_filter_noise_causes_unknown();

    printf("\n[5] Two-camera fusion\n");
    test_fuse_both_cams();
    test_fuse_cam1_null();
    test_fuse_cam0_null();

    printf("\n[6] Boundary conditions\n");
    test_fill_ratio_clamped();
    test_config_window_bounds();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_pass, tests_run, tests_fail);

    if (tests_fail > 0) {
        printf("\n*** SOME TESTS FAILED ***\n");
    } else {
        printf("\n*** ALL TESTS PASSED ***\n");
    }

    /* bare-metal: 死循环，QEMU 手动 Ctrl+A X 退出 */
    while (1) {}
    return 0;
}
