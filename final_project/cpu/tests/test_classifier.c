/*==========================================================================
 *  test_classifier.c  —  vision_classifier PC 端单元测试
 *
 *  编译:
 *    gcc -std=c99 -Wall -Wextra -DAPB_VISION_BASE_PLACEHOLDER=0x00000000u \
 *        -I../app/include test_classifier.c ../app/src/vision_classifier.c \
 *        -o test_classifier.exe
 *
 *  覆盖:
 *    颜色: RED/BLUE/YELLOW 主色 + WHITE/BLACK 排除法 + UNKNOWN
 *    形状: Cam0 填充率(方/圆) / Cam1 填充率(方/三角)
 *    尺寸: Cam1 height_px 查表 / Cam0 不输出尺寸
 *    滤波: 多帧稳定 / 未达 confirm → UNKNOWN / 噪声不滑行
 *    融合: 双路三大形状 / 单路退化 / NULL 安全
 *
 *  注意：本测试针对当前 fill-only 算法（2026-07-07）。
 *  算法不依赖宽高比；Cam0 只能区分方(填充率高) vs 圆(填充率中)；
 *  CONE 只有双路融合才能判定。
 *==========================================================================*/

#include <stdio.h>
#include <string.h>

#ifndef APB_VISION_BASE_PLACEHOLDER
#define APB_VISION_BASE_PLACEHOLDER  0x00000000u
#endif

#include "board_io.h"
#include "vision_classifier.h"

static int tests_run   = 0;
static int tests_pass  = 0;
static int tests_fail  = 0;

#define TEST(name)  do { tests_run++; printf("  %-52s", name); } while(0)
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

/*==========================================================================
 *  测试组 1: 颜色分类
 *==========================================================================*/
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
    TEST("color: WHITE via exclusion (FG_AREA_AVAILABLE=0 path)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    cfg.white_luma_ratio = 0.60f;
    cfg.black_luma_ratio = 0.05f;
    /* 降低单色阈值，让 RGB 都低于阈值但总和相对 bbox 不低 */
    cfg.min_red_area  = 200;
    cfg.min_blue_area = 200;
    cfg.min_yel_area  = 200;

    /* bbox 25×25=625, sum_rgb=150+150+180=480, fr=0.768 > 0.60 → WHITE */
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

    /* 大面积 bbox，几乎无色 → fr≈0 < 0.05 → BLACK */
    feature_snapshot_t s = make_snap(5, 3, 2, 50,50, 200,200, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.color_id, COLOR_BLACK, "color_id");
    PASS();
}

static void test_color_unknown_mid_fill(void)
{
    TEST("color: UNKNOWN (mid fill, no dominant color)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    cfg.white_luma_ratio = 0.60f;
    cfg.black_luma_ratio = 0.05f;
    cfg.min_red_area  = 200;
    cfg.min_blue_area = 200;
    cfg.min_yel_area  = 200;

    /* bbox 100×100=10000, sum=540, fr=0.054 ∈ [0.05, 0.60] → UNKNOWN */
    feature_snapshot_t s = make_snap(180, 170, 190, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.color_id, COLOR_UNKNOWN, "color_id");
    PASS();
}

/*==========================================================================
 *  测试组 2: 形状分类 — Cam0 俯视（方 vs 圆）
 *==========================================================================*/
static void test_shape_cam0_cube(void)
{
    TEST("shape Cam0: high fill (>=0.85) → CUBE");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    /* 100×100 bbox, fill=1.0 */
    feature_snapshot_t s = make_snap(8000, 1000, 1000, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CUBE, "shape_id");
    PASS();
}

static void test_shape_cam0_cylinder(void)
{
    TEST("shape Cam0: mid fill (0.70~0.85) → CYLINDER (circle)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    /* 100×100 bbox, fill=0.75 (circle ≈ π/4 = 78%) */
    feature_snapshot_t s = make_snap(6000, 750, 750, 50,50, 150,150, 0, 1);
    /* area=10000, sum=7500, fr=0.75 ∈ [0.70, 0.85) */
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CYLINDER, "shape_id");
    PASS();
}

static void test_shape_cam0_unknown_low_fill(void)
{
    TEST("shape Cam0: low fill (<0.70) → UNKNOWN");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    /* 100×100 bbox, fill=0.23 — Cam0 不能单独判 cone */
    feature_snapshot_t s = make_snap(2000, 200, 100, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_UNKNOWN, "shape_id");
    PASS();
}

static void test_shape_cam0_unknown_tiny(void)
{
    TEST("shape Cam0: bbox too small → UNKNOWN");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    /* area = 1×1 = 1 → triggers area ≤ 1 guard → UNKNOWN */
    feature_snapshot_t s = make_snap(20, 10, 5, 50,50, 51,51, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_UNKNOWN, "shape_id");
    PASS();
}

/*==========================================================================
 *  测试组 3: 形状分类 — Cam1 侧面（方 vs 三角）
 *==========================================================================*/
static void test_shape_cam1_cube(void)
{
    TEST("shape Cam1: high fill (>=0.85) → CUBE (square side)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    /* 侧面看方形 → 填充率很高 */
    feature_snapshot_t s = make_snap(8000, 500, 500, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 1, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CUBE, "shape_id");
    PASS();
}

static void test_shape_cam1_cone(void)
{
    TEST("shape Cam1: mid fill (0.25~0.85) → CONE (triangle side)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    /* 侧面看三角 → 填充率中等偏低 */
    feature_snapshot_t s = make_snap(2000, 200, 100, 50,50, 150,150, 0, 1);
    /* area=10000, sum=2300, fr=0.23 → 低于 0.25? Hmm.
       0.23 < 0.25 → would be UNKNOWN.
       Need fill ≥ 0.25. Let me adjust: area=10000, need sum ≥ 2500. */
    (void)s;
    s = make_snap(2000, 300, 300, 50,50, 150,150, 0, 1);
    /* area=10000, sum=2600, fr=0.26 ∈ [0.25, 0.85) → CONE */
    vision_result_t r = classify_frame(&s, 1, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CONE, "shape_id");
    PASS();
}

static void test_shape_cam1_unknown_low_fill(void)
{
    TEST("shape Cam1: very low fill (<0.25) → UNKNOWN");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    feature_snapshot_t s = make_snap(100, 50, 30, 50,50, 200,200, 0, 1);
    /* area=22500, sum=180, fr=0.008 → UNKNOWN */
    vision_result_t r = classify_frame(&s, 1, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_UNKNOWN, "shape_id");
    PASS();
}

/*==========================================================================
 *  测试组 4: 尺寸分类
 *==========================================================================*/
static void test_size_cam0_no_size(void)
{
    TEST("size: Cam0 always returns size=0");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    feature_snapshot_t s = make_snap(2000, 100, 50, 50,50, 100,100, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.size_cm_x10, 0, "size_cm_x10 (Cam0 n/a)");
    PASS();
}

static void test_size_cam1_height(void)
{
    TEST("size: Cam1 height_px lookup → 2.5cm");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    cfg.height_px_20mm = 60;
    cfg.height_px_25mm = 75;
    cfg.height_px_30mm = 90;

    /* height_px=78, closest to 75 → 25 (2.5cm) */
    feature_snapshot_t s = make_snap(2000, 100, 50, 50,50, 100,100, 78, 1);
    vision_result_t r = classify_frame(&s, 1, &cfg);
    ASSERT_EQ(r.size_cm_x10, 25, "size_cm_x10");
    PASS();
}

static void test_size_cam1_height_zero(void)
{
    TEST("size: Cam1 height_px==0 → size=0 (no fallback)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    /* height_px==0 → _classify_size 直接返回 0 */
    feature_snapshot_t s = make_snap(8000, 200, 100, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 1, &cfg);
    ASSERT_EQ(r.size_cm_x10, 0, "size_cm_x10");
    PASS();
}

/*==========================================================================
 *  测试组 5: 多帧滤波
 *==========================================================================*/
static void test_filter_stabilize(void)
{
    TEST("filter: stabilizes after confirm frames");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    cfg.filter_window  = 5;
    cfg.filter_confirm = 3;

    mf_filter_t f;
    mf_filter_reset(&f);

    /* fill = 10000/10000 = 1.0 → CUBE on Cam0 */
    feature_snapshot_t s = make_snap(8000, 1000, 1000, 50,50, 150,150, 0, 1);
    vision_result_t raw = classify_frame(&s, 0, &cfg);

    vision_result_t r;
    r = mf_filter_update(&f, &raw, &cfg);
    r = mf_filter_update(&f, &raw, &cfg);
    r = mf_filter_update(&f, &raw, &cfg);
    /* 3rd frame: votes=3 ≥ confirm=3 → confirmed */
    ASSERT_EQ(r.color_id, COLOR_RED, "color_id");
    ASSERT_EQ(r.shape_id, SHAPE_CUBE, "shape_id");
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

    vision_result_t r;
    r = mf_filter_update(&f, &raw, &cfg);  /* 1/5 */
    r = mf_filter_update(&f, &raw, &cfg);  /* 2/5, votes=2 < 3 */
    ASSERT_EQ(r.color_id, COLOR_UNKNOWN, "color_id unstabilized");
    ASSERT_EQ(r.shape_id, SHAPE_UNKNOWN, "shape_id unstabilized");
    PASS();
}

static void test_filter_noise_causes_unknown(void)
{
    TEST("filter: noise → UNKNOWN (3 colors break majority)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    cfg.filter_window  = 5;
    cfg.filter_confirm = 3;

    mf_filter_t f;
    mf_filter_reset(&f);

    /* 三种颜色的快照，确保没有单一 (color,shape,size) 能凑够 3 票。
     * 所有 snap fill=0.87 → shape=CUBE on Cam0，仅颜色不同。 */
    feature_snapshot_t red_snap  = make_snap(8600, 50,  50,  50,50, 150,150, 0, 1);
    feature_snapshot_t blue_snap = make_snap(50,  8600, 50,  50,50, 150,150, 0, 2);
    feature_snapshot_t yel_snap  = make_snap(50,  50,  8600, 50,50, 150,150, 0, 3);

    vision_result_t red  = classify_frame(&red_snap, 0, &cfg);
    vision_result_t blue = classify_frame(&blue_snap, 0, &cfg);
    vision_result_t yel  = classify_frame(&yel_snap, 0, &cfg);
    /* red:  (RED,CUBE,0), blue: (BLUE,CUBE,0), yel:  (YELLOW,CUBE,0) */

    int i;
    for (i = 0; i < 5; i++) mf_filter_update(&f, &red, &cfg);
    /* buffer: [R,R,R,R,R]  →  stable RED */

    /* Inject B, B, Y:
     *   B → [B,R,R,R,R]  R=4 B=1          → RED (stable)
     *   B → [B,B,R,R,R]  R=3 B=2          → RED (stable)
     *   Y → [B,B,Y,R,R]  R=2 B=2 Y=1      → no key ≥ 3 → UNKNOWN */
    mf_filter_update(&f, &blue, &cfg);
    mf_filter_update(&f, &blue, &cfg);
    vision_result_t r = mf_filter_update(&f, &yel, &cfg);
    ASSERT_EQ(r.color_id, COLOR_UNKNOWN, "color_id — must be UNKNOWN, not coasting");
    PASS();
}

/*==========================================================================
 *  测试组 6: 双路融合
 *==========================================================================*/
static void test_fuse_cube(void)
{
    TEST("fuse: Cam0=CUBE + Cam1=CUBE → CUBE");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    vision_result_t c0;
    memset(&c0, 0, sizeof(c0));
    c0.color_id    = COLOR_RED;
    c0.shape_id    = SHAPE_CUBE;
    c0.confidence  = 200;

    vision_result_t c1;
    memset(&c1, 0, sizeof(c1));
    c1.color_id    = COLOR_RED;
    c1.shape_id    = SHAPE_CUBE;
    c1.size_cm_x10 = 25;
    c1.confidence  = 180;

    vision_result_t f = fuse_results(&c0, &c1, &cfg);
    ASSERT_EQ(f.shape_id,    SHAPE_CUBE, "shape → CUBE");
    ASSERT_EQ(f.color_id,    COLOR_RED,  "color (Cam0 wins)");
    ASSERT_EQ(f.size_cm_x10, 25,         "size  (Cam1 wins)");
    PASS();
}

static void test_fuse_cylinder(void)
{
    TEST("fuse: Cam0=CYLINDER + Cam1=CUBE → CYLINDER");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    vision_result_t c0;
    memset(&c0, 0, sizeof(c0));
    c0.color_id    = COLOR_BLUE;
    c0.shape_id    = SHAPE_CYLINDER;
    c0.confidence  = 210;

    vision_result_t c1;
    memset(&c1, 0, sizeof(c1));
    c1.color_id    = COLOR_BLUE;
    c1.shape_id    = SHAPE_CUBE;       /* 侧面看圆柱是方形 */
    c1.size_cm_x10 = 30;
    c1.confidence  = 190;

    vision_result_t f = fuse_results(&c0, &c1, &cfg);
    ASSERT_EQ(f.shape_id,    SHAPE_CYLINDER, "shape → CYLINDER");
    ASSERT_EQ(f.color_id,    COLOR_BLUE,     "color");
    ASSERT_EQ(f.size_cm_x10, 30,             "size");
    PASS();
}

static void test_fuse_cone(void)
{
    TEST("fuse: Cam0=CYLINDER + Cam1=CONE → CONE");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    vision_result_t c0;
    memset(&c0, 0, sizeof(c0));
    c0.color_id    = COLOR_YELLOW;
    c0.shape_id    = SHAPE_CYLINDER;   /* 俯视圆锥顶面也是圆形 */
    c0.confidence  = 170;

    vision_result_t c1;
    memset(&c1, 0, sizeof(c1));
    c1.color_id    = COLOR_YELLOW;
    c1.shape_id    = SHAPE_CONE;       /* 侧面看圆锥是三角形 */
    c1.size_cm_x10 = 20;
    c1.confidence  = 160;

    vision_result_t f = fuse_results(&c0, &c1, &cfg);
    ASSERT_EQ(f.shape_id,    SHAPE_CONE,    "shape → CONE");
    ASSERT_EQ(f.color_id,    COLOR_YELLOW,  "color");
    ASSERT_EQ(f.size_cm_x10, 20,            "size");
    PASS();
}

static void test_fuse_unknown_combo(void)
{
    TEST("fuse: Cam0=CYLINDER + Cam1=CYLINDER → UNKNOWN");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    vision_result_t c0, c1;
    memset(&c0, 0, sizeof(c0));
    memset(&c1, 0, sizeof(c1));
    c0.shape_id = SHAPE_CYLINDER;
    c1.shape_id = SHAPE_CYLINDER;  /* 无此组合的融合规则 */

    vision_result_t f = fuse_results(&c0, &c1, &cfg);
    ASSERT_EQ(f.shape_id, SHAPE_UNKNOWN, "shape → UNKNOWN");
    PASS();
}

/*--------------------------------------------------------------------------
 *  融合: NULL 退化
 *--------------------------------------------------------------------------*/
static void test_fuse_cam1_null(void)
{
    TEST("fuse: cam1==NULL → Cam0 only, no crash");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    vision_result_t c0;
    memset(&c0, 0, sizeof(c0));
    c0.color_id    = COLOR_BLUE;
    c0.shape_id    = SHAPE_CYLINDER;
    c0.size_cm_x10 = 25;
    c0.confidence  = 150;

    vision_result_t f = fuse_results(&c0, 0, &cfg);
    ASSERT_EQ(f.color_id,    COLOR_BLUE,     "color");
    ASSERT_EQ(f.shape_id,    SHAPE_CYLINDER, "shape");
    ASSERT_EQ(f.size_cm_x10, 0,              "size (n/a from Cam0)");
    ASSERT_EQ(f.confidence,  150,            "confidence");
    PASS();
}

static void test_fuse_cam0_null(void)
{
    TEST("fuse: cam0==NULL → Cam1 only, no crash");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    vision_result_t c1;
    memset(&c1, 0, sizeof(c1));
    c1.color_id    = COLOR_YELLOW;
    c1.shape_id    = SHAPE_CUBE;
    c1.size_cm_x10 = 30;
    c1.confidence  = 220;

    vision_result_t f = fuse_results(0, &c1, &cfg);
    ASSERT_EQ(f.color_id,    COLOR_YELLOW, "color");
    ASSERT_EQ(f.shape_id,    SHAPE_CUBE,   "shape");
    ASSERT_EQ(f.size_cm_x10, 30,           "size");
    ASSERT_EQ(f.confidence,  220,          "confidence");
    PASS();
}

static void test_fuse_both_null(void)
{
    TEST("fuse: both NULL → all-zero, no crash");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    vision_result_t f = fuse_results(0, 0, &cfg);
    ASSERT_EQ(f.color_id,    COLOR_UNKNOWN, "color");
    ASSERT_EQ(f.shape_id,    SHAPE_UNKNOWN, "shape");
    ASSERT_EQ(f.size_cm_x10, 0,             "size");
    PASS();
}

static void test_filter_size_jitter(void)
{
    TEST("filter: size participates in voting key (size jitter picks majority)");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);
    cfg.filter_window  = 5;
    cfg.filter_confirm = 3;
    cfg.height_px_20mm = 50;
    cfg.height_px_25mm = 70;
    cfg.height_px_30mm = 90;

    mf_filter_t f;
    mf_filter_reset(&f);

    /* Same color+shape (RED,CUBE) but different sizes from Cam1 height_px.
     * Need fill ≥ 0.85 on Cam1 → sum_rgb ≥ 8500 for area=10000 */
    feature_snapshot_t s20 = make_snap(8500, 50,  50, 50,50, 150,150, 55, 1);
    feature_snapshot_t s25 = make_snap(8500, 50,  50, 50,50, 150,150, 72, 2);
    /* s20 → height_px=55 → closest to 50 → size=20
     * s25 → height_px=72 → closest to 70 → size=25 */

    vision_result_t r20 = classify_frame(&s20, 1, &cfg);
    vision_result_t r25 = classify_frame(&s25, 1, &cfg);
    /* r20: (RED,CUBE,20), r25: (RED,CUBE,25) — different keys because size differs */

    /* Feed: 20, 25, 20, 25, 20.
     * (RED,CUBE,20): 3 votes, (RED,CUBE,25): 2 votes → 3 ≥ confirm=3 → confirmed */
    mf_filter_update(&f, &r20, &cfg);
    mf_filter_update(&f, &r25, &cfg);
    mf_filter_update(&f, &r20, &cfg);
    mf_filter_update(&f, &r25, &cfg);
    vision_result_t r = mf_filter_update(&f, &r20, &cfg);
    ASSERT_EQ(r.color_id,    COLOR_RED,  "color confirmed");
    ASSERT_EQ(r.size_cm_x10, 20,         "size picks majority (20 has 3 votes)");
    PASS();
}

/*==========================================================================
 *  测试组 7: 边界条件
 *==========================================================================*/
static void test_fill_ratio_clamped(void)
{
    TEST("boundary: fill_ratio clamped to 1.0");
    classifier_cfg_t cfg;
    classifier_cfg_default(&cfg);

    /* sum (100k) > area (10k) → clamp */
    feature_snapshot_t s = make_snap(50000, 30000, 20000, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CUBE, "shape_id (clamped, not crashed)");
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
    for (i = 0; i < 5; i++) {
        mf_filter_update(&f, &raw, &cfg);
    }
    vision_result_t r = mf_filter_update(&f, &raw, &cfg);
    ASSERT_EQ(r.color_id, COLOR_RED, "not crashed");
    PASS();
}

/*==========================================================================
 *  main
 *==========================================================================*/
int main(void)
{
    printf("=== vision_classifier unit tests ===\n\n");

    printf("[1] Color classification\n");
    test_color_red();
    test_color_blue();
    test_color_yellow();
    test_color_white_exclusion();
    test_color_black_exclusion();
    test_color_unknown_mid_fill();

    printf("\n[2] Shape — Cam0 (top view: cube vs circle)\n");
    test_shape_cam0_cube();
    test_shape_cam0_cylinder();
    test_shape_cam0_unknown_low_fill();
    test_shape_cam0_unknown_tiny();

    printf("\n[3] Shape — Cam1 (side view: cube vs triangle)\n");
    test_shape_cam1_cube();
    test_shape_cam1_cone();
    test_shape_cam1_unknown_low_fill();

    printf("\n[4] Size classification\n");
    test_size_cam0_no_size();
    test_size_cam1_height();
    test_size_cam1_height_zero();

    printf("\n[5] Multi-frame filter\n");
    test_filter_stabilize();
    test_filter_not_stabilized();
    test_filter_noise_causes_unknown();
    test_filter_size_jitter();

    printf("\n[6] Two-camera fusion\n");
    test_fuse_cube();
    test_fuse_cylinder();
    test_fuse_cone();
    test_fuse_unknown_combo();
    test_fuse_cam1_null();
    test_fuse_cam0_null();
    test_fuse_both_null();

    printf("\n[7] Boundary conditions\n");
    test_fill_ratio_clamped();
    test_config_window_bounds();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_pass, tests_run, tests_fail);

    return (tests_fail > 0) ? 1 : 0;
}
