/*==========================================================================
 *  task_matcher.c  —  目标匹配与动作决策 实现
 *
 *  比对 vision_result_t 与 task_target_t，输出抓取/跳过/报错，
 *  并保存成功匹配时的抓取坐标。
 *
 *  四任务决赛规则（2026-07-12）：
 *    MODE_1: 颜色+形状精确匹配，尺寸通配
 *    MODE_2: 混合形状池中的颜色+形状匹配，尺寸通配
 *    MODE_3: 颜色+形状匹配，|obs.size - reference_size| == 10mm
 *    MODE_4: 颜色+形状匹配，|obs.size - target_size| <= 5mm
 *
 *  一轮一事务锁：GRAB 后自动进入 ROUND_GRAB_REQUESTED，防止连续帧重复抓取。
 *==========================================================================*/

#include "task_matcher.h"
#include "board_io.h"
#include <stdlib.h>    /* abs() */
#include <string.h>    /* memset() */

/*==========================================================================
 *  内部状态
 *==========================================================================*/

static task_target_t       g_target;
static task_match_result_t g_last_match;   /* 最近一次 evaluate() 的完整结果 */
static uint16_t            g_grab_cx;      /* 最近一次 GRAB 匹配的 x 坐标 */
static uint16_t            g_grab_cy;
static int                 g_grab_valid;   /* 抓取坐标是否有效 */

/*==========================================================================
 *  内部 helper — 绝对值差值
 *==========================================================================*/

static uint8_t _abs_diff_u8(uint8_t a, uint8_t b)
{
    return (a > b) ? (uint8_t)(a - b) : (uint8_t)(b - a);
}

/*==========================================================================
 *  公共 API
 *==========================================================================*/

void task_matcher_init(void)
{
    g_target.target_color       = COLOR_UNKNOWN;
    g_target.target_shape       = SHAPE_UNKNOWN;
    g_target.target_size_cm_x10 = 0;
    g_target.reference_size_cm_x10 = 0;
    g_target.task_mode          = TASK_MODE_NONE;
    g_target.round_state        = ROUND_IDLE;
    g_grab_valid = 0;
    memset(&g_last_match, 0, sizeof(g_last_match));
}

/*--------------------------------------------------------------------------
 *  旧接口 — 兼容历史测试
 *--------------------------------------------------------------------------*/
void task_matcher_set_target(const task_target_t *t)
{
    if (t == 0) {
        task_matcher_set_target_ex(0);
        return;
    }

    /* 包装为扩展版：旧接口保留历史颜色+形状+尺寸精确匹配语义。 */
    task_target_t ex;
    ex.target_color       = t->target_color;
    ex.target_shape       = t->target_shape;
    ex.target_size_cm_x10 = t->target_size_cm_x10;
    ex.reference_size_cm_x10 = 0;
    ex.task_mode          = TASK_MODE_LEGACY_EXACT;
    ex.round_state        = ROUND_TARGET_LOCKED;
    task_matcher_set_target_ex(&ex);
}

/*--------------------------------------------------------------------------
 *  扩展接口 — 支持四任务决赛
 *--------------------------------------------------------------------------*/
void task_matcher_set_target_ex(const task_target_t *t)
{
    if (t == 0) {
        g_target.target_color       = COLOR_UNKNOWN;
        g_target.target_shape       = SHAPE_UNKNOWN;
        g_target.target_size_cm_x10 = 0;
        g_target.reference_size_cm_x10 = 0;
        g_target.task_mode          = TASK_MODE_NONE;
        g_target.round_state        = ROUND_IDLE;
        g_grab_valid = 0;
        return;
    }

    /* 同一轮反复读到同一目标时必须保持事务锁；否则 main() 每帧重读
     * TARGET_SEL 会把 GRAB_REQUESTED 重新解锁，造成同一物体重复抓取。
     * next_round() 会先把状态置为 IDLE，因此下一轮仍可用相同目标。 */
    if (g_target.round_state != ROUND_IDLE &&
        g_target.target_color == t->target_color &&
        g_target.target_shape == t->target_shape &&
        g_target.target_size_cm_x10 == t->target_size_cm_x10 &&
        g_target.reference_size_cm_x10 == t->reference_size_cm_x10 &&
        g_target.task_mode == t->task_mode)
        return;

    g_target.target_color       = t->target_color;
    g_target.target_shape       = t->target_shape;
    g_target.target_size_cm_x10 = t->target_size_cm_x10;
    g_target.reference_size_cm_x10 = t->reference_size_cm_x10;
    g_target.task_mode          = t->task_mode;

    /* API contract: set_target_ex() auto-locks to TARGET_LOCKED.
     * Never trust the caller's round_state — a memset-to-zero struct
     * would leave ROUND_IDLE and silently block all evaluate() calls. */
    g_target.round_state        = ROUND_TARGET_LOCKED;

    g_grab_valid = 0;     /* new target → old grab coordinates are stale */
}

/*==========================================================================
 *  返回前写 last_match（供 main.c / adapter 读取真实 reason）
 *==========================================================================*/
static uint8_t _return_match(uint8_t action, uint8_t is_target,
                              reason_code_t reason)
{
    g_last_match.action    = action;
    g_last_match.is_target = is_target;
    g_last_match.reason    = reason;
    g_last_match.mode      = (task_mode_t)g_target.task_mode;
    return action;
}

/*==========================================================================
 *  核心判定 — 按 task_mode 分派
 *==========================================================================*/
uint8_t task_matcher_evaluate(const vision_result_t *obs,
                               uint16_t center_x, uint16_t center_y)
{
    /* ---- 1. 事务锁：只有 TARGET_LOCKED 状态才允许评估 ---- */
    if (g_target.round_state != ROUND_TARGET_LOCKED)
        return _return_match(MATCH_ACTION_NONE, 0, REASON_OBSERVATION_UNKNOWN);

    /* 每次进入实际评估都作废旧坐标 — 只本轮 GRAB 才能使坐标有效 */
    g_grab_valid = 0;

    /* ---- 2. 空指针 → 防御裸机崩溃 ---- */
    if (obs == 0) {
#if TASK_MATCHER_DEBUG_MODE
        return _return_match(MATCH_ACTION_ERROR, 0, REASON_OBSERVATION_UNKNOWN);
#else
        return _return_match(MATCH_ACTION_NONE, 0, REASON_OBSERVATION_UNKNOWN);
#endif
    }

    /* ---- 3. 观测无效 ---- */
    if (obs->color_id == COLOR_UNKNOWN || obs->shape_id == SHAPE_UNKNOWN) {
#if TASK_MATCHER_DEBUG_MODE
        return _return_match(MATCH_ACTION_ERROR, 0, REASON_OBSERVATION_UNKNOWN);
#else
        return _return_match(MATCH_ACTION_NONE, 0, REASON_OBSERVATION_UNKNOWN);
#endif
    }

    /* ---- 4. 颜色不匹配 ---- */
    if (g_target.target_color != COLOR_UNKNOWN &&
        obs->color_id != g_target.target_color)
        return _return_match(MATCH_ACTION_SKIP, 0, REASON_COLOR_MISMATCH);

    /* ---- 5. 形状不匹配 ---- */
    if (g_target.target_shape != SHAPE_UNKNOWN &&
        obs->shape_id != g_target.target_shape)
        return _return_match(MATCH_ACTION_SKIP, 0, REASON_SHAPE_MISMATCH);

    /* ---- 6. 尺寸判定（按 task_mode 分派） ---- */
    uint8_t mode = g_target.task_mode;

    if (mode == TASK_MODE_1) {
        /* MODE_1: 颜色+形状匹配即可，尺寸通配 — 不做尺寸检查 */
    } else if (mode == TASK_MODE_2) {
        /* MODE_2: 官方细则要求混合形状池中的指定颜色正方体；
         * 5 个物体本身为相同尺寸规格，尺寸不参与目标判断。 */
    } else if (mode == TASK_MODE_3) {
        /* MODE_3: 相对参照物边长差 = 10mm (1cm) */
        if (obs->size_cm_x10 == 0)
            return _return_match(MATCH_ACTION_NONE, 0, REASON_OBSERVATION_UNKNOWN);
        if (g_target.reference_size_cm_x10 == 0)
            return _return_match(MATCH_ACTION_NONE, 0, REASON_OBSERVATION_UNKNOWN);
        if (_abs_diff_u8(obs->size_cm_x10, g_target.reference_size_cm_x10)
            != TASK3_SIZE_DELTA_CM_X10)
            return _return_match(MATCH_ACTION_SKIP, 0, REASON_SIZE_NOT_EQ_10MM);
    } else if (mode == TASK_MODE_4) {
        /* MODE_4: 相对目标物边长差 ≤ 5mm (0.5cm) */
        if (obs->size_cm_x10 == 0)
            return _return_match(MATCH_ACTION_NONE, 0, REASON_OBSERVATION_UNKNOWN);
        if (_abs_diff_u8(obs->size_cm_x10, g_target.target_size_cm_x10)
            > TASK4_SIZE_DELTA_MAX_CM_X10)
            return _return_match(MATCH_ACTION_SKIP, 0, REASON_SIZE_OUTSIDE_5MM);
    } else if (mode == TASK_MODE_LEGACY_EXACT) {
        /* 旧 API 兼容：颜色+形状+尺寸精确匹配。 */
        if (g_target.target_size_cm_x10 != 0) {
            if (obs->size_cm_x10 == 0)
                return _return_match(MATCH_ACTION_NONE, 0, REASON_OBSERVATION_UNKNOWN);
            if (obs->size_cm_x10 != g_target.target_size_cm_x10)
                return _return_match(MATCH_ACTION_SKIP, 0, REASON_SIZE_OUTSIDE_5MM);
        }
    } else {
        /* 未识别的 task_mode → 安全侧：NONE */
        return _return_match(MATCH_ACTION_NONE, 0, REASON_TARGET_INVALID);
    }

    /* ---- 7. 全部匹配 → GRAB，保存坐标，锁定事务 ---- */
    g_grab_cx   = center_x;
    g_grab_cy   = center_y;
    g_grab_valid = 1;

    /* 自动推进事务锁：防止同轮连续帧重复 GRAB */
    g_target.round_state = ROUND_GRAB_REQUESTED;

    return _return_match(MATCH_ACTION_GRAB, 1, REASON_TARGET_MATCH);
}

/*--------------------------------------------------------------------------
 *  抓取坐标
 *--------------------------------------------------------------------------*/
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
    return (g_target.round_state != ROUND_IDLE) ? &g_target : 0;
}

/*--------------------------------------------------------------------------
 *  最近一次 evaluate() 的完整 match result
 *--------------------------------------------------------------------------*/
void task_matcher_get_last_match(task_match_result_t *out)
{
    if (out) *out = g_last_match;
}

/*--------------------------------------------------------------------------
 *  一轮一事务状态机
 *--------------------------------------------------------------------------*/
void task_matcher_next_round(void)
{
    /* 从 GRAB_REQUESTED / DONE → IDLE，清空旧目标 */
    g_target.round_state = ROUND_IDLE;
    g_grab_valid = 0;
}

void task_matcher_round_reset(void)
{
    task_matcher_set_target_ex(0);
}

uint8_t task_matcher_get_round_state(void)
{
    return g_target.round_state;
}

/*--------------------------------------------------------------------------
 *  FPGA TARGET_SEL 读取（旧契约，四任务不可依赖）
 *--------------------------------------------------------------------------*/
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
    /* TARGET_SEL 寄存器未实现。
     *   正式主线 (DEBUG_MODE=0) → 强制清空目标，evaluate() 将返回 NONE，
     *     不允许沿用旧目标继续抓取（2026-07-09 用户决策）。
     *   调试模式 (DEBUG_MODE=1) → 保持当前目标不动，
     *     可通过 task_matcher_set_target() 手动设定目标进行测试。 */
#if TASK_MATCHER_DEBUG_MODE
    return -1;
#else
    task_matcher_set_target(0);
    return -1;
#endif
#endif
}
