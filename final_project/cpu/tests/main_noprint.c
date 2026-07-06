/*==========================================================================
 *  main_noprint.c  —  vision_classifier 裸机测试（无 printf，结果写内存）
 *
 *  用法:
 *    1. 编译: build_qemu_noprint.bat
 *    2. 启动 QEMU GDB server:
 *       qemu-system-riscv32 -M spike -S -gdb tcp:1234 -bios none -kernel test.elf
 *    3. GDB 连接:
 *       riscv-none-embed-gdb test.elf
 *       (gdb) target remote localhost:1234
 *       (gdb) continue
 *       (gdb) Ctrl+C
 *       (gdb) x/4xw &g_results
 *
 *  结果布局 (uint32_t g_results[4]):
 *    [0] = 0xBEEF0000 | tests_run      (magic + 运行数)
 *    [1] = tests_pass                   (通过数)
 *    [2] = tests_fail                   (失败数, 0 = 全部通过)
 *    [3] = 0x00000000 (PASS) | 非零 (第一个失败的测试编号)
 *==========================================================================*/

#include <stdint.h>
#include <string.h>

#ifndef IO_APB_SLAVE_0_BASE
#define IO_APB_SLAVE_0_BASE  0x00000000u
#endif

#include "board_io.h"
#include "vision_classifier.h"

/* === 测试结果存储（已知地址，GDB 可读）=== */
volatile uint32_t g_results[4] __attribute__((section(".data")));

static int tests_run   = 0;
static int tests_pass  = 0;
static int tests_fail  = 0;

#define RECORD_PASS()  do { tests_pass++; } while(0)
#define RECORD_FAIL()  do { if (tests_fail == 0) g_results[3] = tests_run + 1; tests_fail++; } while(0)

#define TEST(name)  do { tests_run++; } while(0)

#define ASSERT_EQ(a, b)  do { \
    if ((a) != (b)) { RECORD_FAIL(); return; } \
} while(0)

/*--------------------------------------------------------------------------
 *  辅助函数
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
 *  测试函数（同 test_classifier.c，输出改为内存记录）
 *--------------------------------------------------------------------------*/
static void t_color_red(void) {
    TEST("color RED");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(8000, 100, 50, 50,50, 200,200, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.color_id, COLOR_RED);
    RECORD_PASS();
}
static void t_color_blue(void) {
    TEST("color BLUE");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(50, 9000, 30, 50,50, 200,200, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.color_id, COLOR_BLUE);
    RECORD_PASS();
}
static void t_color_yellow(void) {
    TEST("color YELLOW");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(30, 20, 7000, 50,50, 200,200, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.color_id, COLOR_YELLOW);
    RECORD_PASS();
}
static void t_color_white(void) {
    TEST("color WHITE exclusion");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(150, 150, 180, 50,50, 75,75, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.color_id, COLOR_WHITE);
    RECORD_PASS();
}
static void t_color_black(void) {
    TEST("color BLACK exclusion");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    cfg.black_luma_ratio = 0.05f;
    feature_snapshot_t s = make_snap(5, 3, 2, 50,50, 200,200, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.color_id, COLOR_BLACK);
    RECORD_PASS();
}
static void t_color_unknown(void) {
    TEST("color small→UNKNOWN");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(5, 3, 2, 50,50, 60,60, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.color_id, COLOR_UNKNOWN);
    RECORD_PASS();
}
static void t_shape_cube(void) {
    TEST("shape CUBE");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(8000, 1000, 1000, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CUBE);
    RECORD_PASS();
}
static void t_shape_cylinder(void) {
    TEST("shape CYLINDER");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(5500, 300, 200, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CYLINDER);
    RECORD_PASS();
}
static void t_shape_cone_top(void) {
    TEST("shape CONE topview");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(2000, 200, 100, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CONE);
    RECORD_PASS();
}
static void t_shape_cyl_side(void) {
    TEST("shape CYL side");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(2000, 100, 50, 50,50, 90,106, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CYLINDER);
    RECORD_PASS();
}
static void t_shape_cone_side(void) {
    TEST("shape CONE side");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(500, 100, 50, 50,50, 80,120, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CONE);
    RECORD_PASS();
}
static void t_shape_tiny(void) {
    TEST("shape tiny→UNKNOWN");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(20, 10, 5, 50,50, 60,60, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_UNKNOWN);
    RECORD_PASS();
}
static void t_size_cam0(void) {
    TEST("size Cam0 area");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(2000, 100, 50, 50,50, 100,100, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.size_cm_x10, 20);
    RECORD_PASS();
}
static void t_size_cam1(void) {
    TEST("size Cam1 height");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    cfg.height_px_20mm = 60; cfg.height_px_25mm = 75; cfg.height_px_30mm = 90;
    feature_snapshot_t s = make_snap(2000, 100, 50, 50,50, 100,100, 78, 1);
    vision_result_t r = classify_frame(&s, 1, &cfg);
    ASSERT_EQ(r.size_cm_x10, 25);
    RECORD_PASS();
}
static void t_size_cam1_fallback(void) {
    TEST("size Cam1 hp==0 fallback");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(8000, 200, 100, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 1, &cfg);
    ASSERT_EQ(r.size_cm_x10, 25);
    RECORD_PASS();
}
static void t_filter_stable(void) {
    TEST("filter stabilize");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    cfg.filter_window = 5; cfg.filter_confirm = 3;
    mf_filter_t f; mf_filter_reset(&f);
    feature_snapshot_t s = make_snap(8000, 100, 50, 50,50, 150,150, 0, 1);
    vision_result_t raw = classify_frame(&s, 0, &cfg);
    mf_filter_update(&f, &raw, &cfg);
    mf_filter_update(&f, &raw, &cfg);
    vision_result_t r = mf_filter_update(&f, &raw, &cfg);
    ASSERT_EQ(r.color_id, COLOR_RED);
    ASSERT_EQ(r.shape_id, SHAPE_CUBE);
    RECORD_PASS();
}
static void t_filter_unstable(void) {
    TEST("filter unstabilized→UNKNOWN");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    cfg.filter_window = 5; cfg.filter_confirm = 3;
    mf_filter_t f; mf_filter_reset(&f);
    feature_snapshot_t s = make_snap(8000, 100, 50, 50,50, 150,150, 0, 1);
    vision_result_t raw = classify_frame(&s, 0, &cfg);
    mf_filter_update(&f, &raw, &cfg);
    vision_result_t r = mf_filter_update(&f, &raw, &cfg);
    ASSERT_EQ(r.color_id, COLOR_UNKNOWN);
    ASSERT_EQ(r.shape_id, SHAPE_UNKNOWN);
    RECORD_PASS();
}
static void t_filter_noise(void) {
    TEST("filter noise→UNKNOWN");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    cfg.filter_window = 5; cfg.filter_confirm = 3;
    mf_filter_t f; mf_filter_reset(&f);
    feature_snapshot_t rs = make_snap(8000, 100, 50, 50,50, 150,150, 0, 1);
    feature_snapshot_t bs = make_snap(100, 8000, 50, 50,50, 150,150, 0, 2);
    vision_result_t red = classify_frame(&rs, 0, &cfg);
    vision_result_t blue = classify_frame(&bs, 0, &cfg);
    int i; for (i = 0; i < 5; i++) mf_filter_update(&f, &red, &cfg);
    mf_filter_update(&f, &blue, &cfg);
    mf_filter_update(&f, &red,  &cfg);
    vision_result_t r = mf_filter_update(&f, &blue, &cfg);
    ASSERT_EQ(r.color_id, COLOR_UNKNOWN);
    RECORD_PASS();
}
static void t_fuse_both(void) {
    TEST("fuse both cams");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    vision_result_t c0, c1;
    memset(&c0, 0, sizeof(c0)); memset(&c1, 0, sizeof(c1));
    c0.color_id = COLOR_RED; c0.shape_id = SHAPE_CUBE;
    c0.size_cm_x10 = 20; c0.confidence = 200;
    c1.color_id = COLOR_RED; c1.shape_id = SHAPE_CYLINDER;
    c1.size_cm_x10 = 30; c1.confidence = 180;
    vision_result_t f = fuse_results(&c0, &c1, &cfg);
    ASSERT_EQ(f.color_id, COLOR_RED);
    ASSERT_EQ(f.shape_id, SHAPE_CUBE);
    ASSERT_EQ(f.size_cm_x10, 30);
    ASSERT_EQ(f.confidence, 200);
    RECORD_PASS();
}
static void t_fuse_cam1_null(void) {
    TEST("fuse cam1=NULL");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    vision_result_t c0;
    memset(&c0, 0, sizeof(c0));
    c0.color_id = COLOR_BLUE; c0.shape_id = SHAPE_CONE;
    c0.size_cm_x10 = 25; c0.confidence = 150;
    vision_result_t f = fuse_results(&c0, 0, &cfg);
    ASSERT_EQ(f.color_id, COLOR_BLUE);
    ASSERT_EQ(f.shape_id, SHAPE_CONE);
    ASSERT_EQ(f.confidence, 150);
    RECORD_PASS();
}
static void t_fuse_cam0_null(void) {
    TEST("fuse cam0=NULL");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    vision_result_t c1;
    memset(&c1, 0, sizeof(c1));
    c1.color_id = COLOR_YELLOW; c1.shape_id = SHAPE_CYLINDER;
    c1.size_cm_x10 = 30; c1.confidence = 220;
    vision_result_t f = fuse_results(0, &c1, &cfg);
    ASSERT_EQ(f.color_id, COLOR_YELLOW);
    ASSERT_EQ(f.shape_id, SHAPE_CYLINDER);
    ASSERT_EQ(f.confidence, 220);
    RECORD_PASS();
}
static void t_boundary_clamp(void) {
    TEST("boundary fill_ratio clamp");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    feature_snapshot_t s = make_snap(50000, 30000, 20000, 50,50, 150,150, 0, 1);
    vision_result_t r = classify_frame(&s, 0, &cfg);
    ASSERT_EQ(r.shape_id, SHAPE_CUBE);
    RECORD_PASS();
}
static void t_boundary_window(void) {
    TEST("boundary window=0 protected");
    classifier_cfg_t cfg; classifier_cfg_default(&cfg);
    cfg.filter_window = 0; cfg.filter_confirm = 10;
    mf_filter_t f; mf_filter_reset(&f);
    feature_snapshot_t s = make_snap(8000, 100, 50, 50,50, 150,150, 0, 1);
    vision_result_t raw = classify_frame(&s, 0, &cfg);
    int i; for (i = 0; i < 5; i++) mf_filter_update(&f, &raw, &cfg);
    vision_result_t r = mf_filter_update(&f, &raw, &cfg);
    ASSERT_EQ(r.color_id, COLOR_RED);
    RECORD_PASS();
}

/*--------------------------------------------------------------------------
 *  main
 *--------------------------------------------------------------------------*/
int main(void)
{
    g_results[0] = 0;
    g_results[1] = 0;
    g_results[2] = 0;
    g_results[3] = 0;

    t_color_red();
    t_color_blue();
    t_color_yellow();
    t_color_white();
    t_color_black();
    t_color_unknown();

    t_shape_cube();
    t_shape_cylinder();
    t_shape_cone_top();
    t_shape_cyl_side();
    t_shape_cone_side();
    t_shape_tiny();

    t_size_cam0();
    t_size_cam1();
    t_size_cam1_fallback();

    t_filter_stable();
    t_filter_unstable();
    t_filter_noise();

    t_fuse_both();
    t_fuse_cam1_null();
    t_fuse_cam0_null();

    t_boundary_clamp();
    t_boundary_window();

    /* 写入 magic + 结果 */
    g_results[0] = 0xBEEF0000u | (uint32_t)tests_run;
    g_results[1] = (uint32_t)tests_pass;
    g_results[2] = (uint32_t)tests_fail;

    /* ebreak 触发 GDB 断点，允许 GDB 在此处读取 g_results */
    __asm__ __volatile__("ebreak");

    /* 进入死循环，GDB 在此断点查看 g_results */
    while (1) {}
    return 0;
}
