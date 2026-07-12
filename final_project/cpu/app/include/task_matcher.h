/*==========================================================================
 *  task_matcher.h  —  目标匹配与动作决策
 *
 *  接收 vision_classifier 的稳定分类结果，与任务目标比对，输出抓取/跳过/报错。
 *
 *  使用方式：
 *    1. task_matcher_set_target_ex(&t) 设定本轮目标（含 task_mode）
 *    2. 每帧调用 task_matcher_evaluate(&obs, cx, cy) 评估
 *    3. 若返回 MATCH_ACTION_GRAB → 表示授权固定 P_pick 抓取序列。
 *       随后自动进入 GRAB_REQUESTED 锁定状态，同轮不再重复 GRAB。
 *    4. 本轮结束后调用 task_matcher_next_round() 释放锁定，进入下一轮。
 *
 *  grab_center 仅用于 OSD/日志/偏差检查，不作为机械臂实时坐标接口。
 *  （正式主线采用固定抓取点，不做视觉伺服闭环。）
 *
 *  四任务决赛规则（2026-07-12）：
 *    TASK_MODE_1 — 颜色+形状精确匹配（无视尺寸）
 *    TASK_MODE_2 — 混合形状池中的指定颜色正方体（尺寸通配）
 *    TASK_MODE_3 — 颜色+形状匹配，尺寸与 reference_size 差 = 10mm(1cm)
 *    TASK_MODE_4 — 颜色+形状匹配，尺寸与 target_size 差 ≤ 5mm(0.5cm)
 *==========================================================================*/

#ifndef TASK_MATCHER_H
#define TASK_MATCHER_H

#include <stdint.h>
#include "vision_classifier.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 *  编译开关
 *--------------------------------------------------------------------------*/
#ifndef TASK_MATCHER_DEBUG_MODE
#  define TASK_MATCHER_DEBUG_MODE  0   /* 正式主线默认 0；调试时设为 1 */
#endif

/*--------------------------------------------------------------------------
 *  动作码（回写到 FPGA OSD / match_action 寄存器）
 *--------------------------------------------------------------------------*/
#define MATCH_ACTION_NONE    0   /* 空闲，无目标或未评估 */
#define MATCH_ACTION_GRAB    1   /* 全部匹配 → 抓取 */
#define MATCH_ACTION_SKIP    2   /* 存在不匹配 → 跳过 */
#define MATCH_ACTION_ERROR   3   /* 观测无效（仅 DEBUG_MODE=1 时输出） */

/*--------------------------------------------------------------------------
 *  任务模式（四任务决赛）
 *--------------------------------------------------------------------------*/
typedef enum {
    TASK_MODE_NONE = 0,
    TASK_MODE_1    = 1,   /* 颜色+形状精确匹配，尺寸通配 */
    TASK_MODE_2    = 2,   /* 混合形状池：颜色+形状匹配，尺寸通配 */
    TASK_MODE_3    = 3,   /* 颜色+形状匹配，尺寸=|obs-ref|=10mm */
    TASK_MODE_4    = 4    /* 颜色+形状匹配，尺寸=|obs-target|≤5mm */
} task_mode_t;

/*--------------------------------------------------------------------------
 *  一轮一事务状态机
 *--------------------------------------------------------------------------*/
typedef enum {
    ROUND_IDLE          = 0,   /* 未设定目标，等待新轮 */
    ROUND_TARGET_LOCKED = 1,   /* 目标已设定，等待匹配 */
    ROUND_GRAB_REQUESTED = 2,  /* 本轮已发出 GRAB，锁定防重复 */
    ROUND_DONE          = 3    /* 本轮完成，等待 next_round 复位 */
} round_state_t;

/*--------------------------------------------------------------------------
 *  尺寸差值常量（cm_x10 单位，即 0.1cm = 1mm）
 *--------------------------------------------------------------------------*/
#define TASK3_SIZE_DELTA_CM_X10      10   /* 任务三：边长差 = 10mm (1cm) */
#define TASK4_SIZE_DELTA_MAX_CM_X10   5   /* 任务四：边长差 ≤ 5mm (0.5cm) */
#define TASK_MODE_LEGACY_EXACT       255u /* 旧 API 专用：颜色+形状+尺寸精确匹配 */

/*--------------------------------------------------------------------------
 *  决策理由码（供 round_controller / OSD 语义输出使用）
 *--------------------------------------------------------------------------*/
typedef enum {
    REASON_TARGET_MATCH = 0,
    REASON_COLOR_MISMATCH,
    REASON_SHAPE_MISMATCH,
    REASON_SIZE_NOT_EQ_10MM,
    REASON_SIZE_OUTSIDE_5MM,
    REASON_OBSERVATION_UNKNOWN,
    REASON_TARGET_INVALID,
    REASON_STABILITY_TIMEOUT,
    REASON_OPERATOR_ABANDON,
    REASON_ARM_NOT_READY,
    REASON_ARM_FAULT
} reason_code_t;

typedef struct {
    uint8_t action;             /* MATCH_ACTION_* */
    uint8_t is_target;          /* 1=目标, 0=非目标或不可判定 */
    reason_code_t reason;
    task_mode_t mode;
} task_match_result_t;

#define TASK_MODE_COLOR_CUBE          TASK_MODE_1
#define TASK_MODE_SHAPE_COLOR_CUBE    TASK_MODE_2
#define TASK_MODE_SIZE_DELTA_EQ_10MM  TASK_MODE_3
#define TASK_MODE_SIZE_DELTA_LE_5MM   TASK_MODE_4

/*--------------------------------------------------------------------------
 *  任务目标（扩展版，支持四任务决赛）
 *--------------------------------------------------------------------------*/
typedef struct {
    uint8_t target_color;            /* COLOR_WHITE/BLACK/RED/BLUE/YELLOW/UNKNOWN */
    uint8_t target_shape;            /* SHAPE_* — SHAPE_UNKNOWN 时匹配任意形状 */
    uint8_t target_size_cm_x10;      /* 20/25/30; 0=通配 — 正式任务仅 MODE_4 使用 */
    uint8_t reference_size_cm_x10;   /* 任务三参照物尺寸; 0=未锁定 */
    uint8_t task_mode;               /* TASK_MODE_1..4 */
    uint8_t round_state;             /* ROUND_* — 防重复触发的事务锁状态 */
} task_target_t;

/*--------------------------------------------------------------------------
 *  API
 *--------------------------------------------------------------------------*/

/* 初始化：清空目标和内部状态 */
void task_matcher_init(void);

/* 设定当前要寻找的物块目标（旧接口，兼容历史测试）。
 * 等价于 task_matcher_set_target_ex() 且 task_mode=TASK_MODE_LEGACY_EXACT。
 * 传入 NULL 则清空目标。 */
void task_matcher_set_target(const task_target_t *t);

/* 设定当前目标（扩展版，支持 task_mode / reference_size / round_state）。
 * 传入 NULL 则清空目标并重置为 ROUND_IDLE。
 * 调用此函数自动推进到 ROUND_TARGET_LOCKED（若目标有效）。
 * 同一轮重复写入完全相同的目标是幂等操作，不会解除 GRAB_REQUESTED；
 * 下一轮必须先调用 task_matcher_next_round()。 */
void task_matcher_set_target_ex(const task_target_t *t);

/* 将一帧观测结果与当前目标比对。
 *
 * center_x / center_y: bbox 中心的像素坐标（来自 feature_snapshot_t.center）。
 * 若本次返回 MATCH_ACTION_GRAB，该坐标被内部保存，
 * 后续 task_matcher_get_grab_coord() 可取出，且 round_state 自动推进到
 * ROUND_GRAB_REQUESTED（锁定，防止同轮重复 GRAB）。
 *
 * 决策规则（按优先级）：
 *   1. round_state != ROUND_TARGET_LOCKED → NONE
 *      （未设定目标 / 已抓过 / 本轮已完成 → 不抓）
 *   2. color_id 或 shape_id 为 UNKNOWN：
 *        DEBUG_MODE=1（调试）→ ERROR
 *        DEBUG_MODE=0（正式）→ NONE
 *   3. target_color != UNKNOWN 且 color 不匹配 → SKIP
 *   4. target_shape != UNKNOWN 且 shape 不匹配 → SKIP
 *   5. 尺寸判定（按 task_mode 分派）：
 *        MODE_1: 跳过尺寸检查（通配）
 *        MODE_2: 跳过尺寸检查（混合形状池本身为相同尺寸规格）
 *        MODE_3: obs.size==0 → NONE；
 *                |obs.size - reference_size| != 10 → SKIP
 *        MODE_4: obs.size==0 → NONE；
 *                |obs.size - target_size| > 5 → SKIP
 *        LEGACY_EXACT: 仅旧 API 使用，保留颜色+形状+尺寸精确匹配
 *   6. 全部匹配 → GRAB（自动推进到 ROUND_GRAB_REQUESTED）
 *
 * 注意：正式主线中 UNKNOWN 只输出 NONE，不输出 ERROR。 */
uint8_t task_matcher_evaluate(const vision_result_t *obs,
                               uint16_t center_x, uint16_t center_y);

/* 获取最近一次 GRAB 匹配时的抓取坐标。
 * 返回 0 成功，-1 表示尚无 GRAB 匹配。 */
int task_matcher_get_grab_coord(uint16_t *cx, uint16_t *cy);

/* 获取当前目标（NULL=无目标），用于调试 / OSD 显示 */
const task_target_t *task_matcher_get_target(void);

/* 推进到下一轮：重置事务锁，允许新的 GRAB。
 * 调用后 round_state 从 ROUND_GRAB_REQUESTED/ROUND_DONE → ROUND_IDLE。
 * 调用方必须在下一轮开始前重新 set_target。 */
void task_matcher_next_round(void);

/* 完全重置：清空目标 + round_state → ROUND_IDLE。
 * 等价于 set_target_ex(NULL)。 */
void task_matcher_round_reset(void);

/* 获取当前 round_state，供 main 循环 / 调试使用 */
uint8_t task_matcher_get_round_state(void);

/* 从 FPGA TARGET_SEL 寄存器读取并构建 cube 目标。
 * target_shape 固定为 SHAPE_CUBE（识别侧只抓正方体）。
 * 内部调用 board_io_read_target_sel_raw() 解码。
 * 返回 0=目标有效已设定, -1=target_valid=0 或 TARGET_SEL 未实现。
 *
 * 依赖 board_io.h 的 TARGET_SEL_AVAILABLE 编译开关：
 *   0（默认）→ DEBUG_MODE=1 时保持当前目标不动（调试期手动 set_target）；
 *              DEBUG_MODE=0 时强制清空目标（正式主线安全策略），返回 -1。
 *   1        → 真正读 TARGET_SEL 寄存器
 * FPGA 队友确认地址/位定义后改为 1。
 *
 * 注意：旧 2-bit color_sel 不支持白/黑五色；四任务正式接入前
 * 必须使用 set_target_ex() + Host/mock API 注入五色目标，
 * 不得依赖 TARGET_SEL_AVAILABLE=1 读取旧字段。 */
int task_matcher_read_target_from_fpga(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_MATCHER_H */
