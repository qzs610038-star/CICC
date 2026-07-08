/*==========================================================================
 *  task_matcher.c  —  目标匹配与动作决策 实现
 *
 *  比对 vision_result_t 与 task_target_t，输出抓取/跳过/报错，
 *  并保存成功匹配时的抓取坐标。
 *==========================================================================*/

#include "task_matcher.h"
#include "board_io.h"

/*==========================================================================
 *  内部状态
 *==========================================================================*/

static task_target_t  g_target;
static int            g_target_set;      /* 是否已设定有效目标 */
static uint16_t       g_grab_cx;         /* 最近一次 GRAB 匹配的 x 坐标 */
static uint16_t       g_grab_cy;
static int            g_grab_valid;      /* 抓取坐标是否有效 */

/*==========================================================================
 *  公共 API
 *==========================================================================*/

void task_matcher_init(void)
{
    g_target.target_color     = COLOR_UNKNOWN;
    g_target.target_shape     = SHAPE_UNKNOWN;
    g_target.target_size_cm_x10 = 0;
    g_target_set = 0;
    g_grab_valid = 0;
}

void task_matcher_set_target(const task_target_t *t)
{
    if (t == 0) {
        g_target_set = 0;
        g_grab_valid = 0;    /* stale grab coords are meaningless without target */
        return;
    }

    g_target.target_color     = t->target_color;
    g_target.target_shape     = t->target_shape;
    g_target.target_size_cm_x10 = t->target_size_cm_x10;
    g_target_set = 1;
    g_grab_valid = 0;     /* new target → old grab coordinates are stale */
}

uint8_t task_matcher_evaluate(const vision_result_t *obs,
                               uint16_t center_x, uint16_t center_y)
{
    /* 每次 evaluate 都作废旧坐标 — 只本轮 GRAB 才能使坐标有效 */
    g_grab_valid = 0;

    /* 1. 无目标 → NONE */
    if (!g_target_set)
        return MATCH_ACTION_NONE;

    /* 2. 空指针 → ERROR（防御裸机环境下调用方传错 NULL） */
    if (obs == 0)
        return MATCH_ACTION_ERROR;

    /* 3. 观测无效 → ERROR */
    if (obs->color_id == COLOR_UNKNOWN || obs->shape_id == SHAPE_UNKNOWN)
        return MATCH_ACTION_ERROR;

    /* 4. 颜色不匹配 → SKIP（target_color=UNKNOWN 时跳过颜色检查） */
    if (g_target.target_color != COLOR_UNKNOWN &&
        obs->color_id != g_target.target_color)
        return MATCH_ACTION_SKIP;

    /* 5. 形状不匹配 → SKIP（target_shape=UNKNOWN 时跳过形状检查） */
    if (g_target.target_shape != SHAPE_UNKNOWN &&
        obs->shape_id != g_target.target_shape)
        return MATCH_ACTION_SKIP;

    /* 6. 尺寸不匹配 → SKIP（target_size=0 时跳过尺寸检查） */
    if (g_target.target_size_cm_x10 != 0 &&
        obs->size_cm_x10 != g_target.target_size_cm_x10)
        return MATCH_ACTION_SKIP;

    /* 7. 全部匹配 → GRAB，保存坐标 */
    g_grab_cx   = center_x;
    g_grab_cy   = center_y;
    g_grab_valid = 1;

    return MATCH_ACTION_GRAB;
}

int task_matcher_get_grab_coord(uint16_t *cx, uint16_t *cy)
{
    if (!g_grab_valid)
        return -1;

    if (cx == 0 || cy == 0)
        return -1;

    *cx = g_grab_cx;
    *cy = g_grab_cy;
    return 0;
}

const task_target_t *task_matcher_get_target(void)
{
    return g_target_set ? &g_target : 0;
}

int task_matcher_read_target_from_fpga(void)
{
#if TARGET_SEL_AVAILABLE
    uint32_t raw = board_io_read_target_sel_raw();

    /* target_valid=0 → 清空目标，不抓取 */
    if (!(raw & TARGET_SEL_VALID)) {
        task_matcher_set_target(0);
        return -1;
    }

    task_target_t t;
    t.target_shape = SHAPE_CUBE;  /* 固定：识别侧只抓正方体 */

    /* color_sel[1:0] → COLOR_* */
    switch (raw & TARGET_SEL_COLOR_MASK) {
    case TARGET_SEL_COLOR_RED:  t.target_color = COLOR_RED;    break;
    case TARGET_SEL_COLOR_BLUE: t.target_color = COLOR_BLUE;   break;
    case TARGET_SEL_COLOR_YEL:  t.target_color = COLOR_YELLOW; break;
    default:                    t.target_color = COLOR_UNKNOWN; break;  /* 任意 */
    }

    /* size_sel[3:2] → 0.1cm 单位 */
    switch (raw & TARGET_SEL_SIZE_MASK) {
    case TARGET_SEL_SIZE_SMALL: t.target_size_cm_x10 = 20; break;
    case TARGET_SEL_SIZE_MED:   t.target_size_cm_x10 = 25; break;
    case TARGET_SEL_SIZE_LARGE: t.target_size_cm_x10 = 30; break;
    default:                    t.target_size_cm_x10 = 0;  break;  /* 任意 */
    }

    task_matcher_set_target(&t);
    return 0;
#else
    /* TARGET_SEL 寄存器未实现 → 不读硬件，保持当前目标不动。
     * 调试期可通过 task_matcher_set_target() 手动设定目标。
     * FPGA 队友确认地址/位定义后，把 board_io.h 的 TARGET_SEL_AVAILABLE 改为 1。 */
    return -1;
#endif
}
