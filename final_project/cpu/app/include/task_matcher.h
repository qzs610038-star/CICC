/*==========================================================================
 *  task_matcher.h  —  目标匹配与动作决策
 *
 *  接收 vision_classifier 的稳定分类结果，与任务目标比对，输出抓取/跳过/报错。
 *
 *  使用方式：
 *    1. task_matcher_set_target(&t) 设定本轮目标
 *    2. 每帧调用 task_matcher_evaluate(&obs, cx, cy) 评估
 *    3. 若返回 MATCH_ACTION_GRAB → task_matcher_get_grab_coord(&cx, &cy)
 *       取得抓取坐标 → 送入 arm_controller
 *==========================================================================*/

#ifndef TASK_MATCHER_H
#define TASK_MATCHER_H

#include <stdint.h>
#include "vision_classifier.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 *  动作码（回写到 FPGA OSD / match_action 寄存器）
 *--------------------------------------------------------------------------*/
#define MATCH_ACTION_NONE    0   /* 空闲，无目标或未评估 */
#define MATCH_ACTION_GRAB    1   /* 三属性全部匹配 → 抓取 */
#define MATCH_ACTION_SKIP    2   /* 存在不匹配 → 跳过 */
#define MATCH_ACTION_ERROR   3   /* 观测无效（UNKNOWN 颜色/形状）→ 报错 */

/*--------------------------------------------------------------------------
 *  任务目标
 *--------------------------------------------------------------------------*/
typedef struct {
    uint8_t target_color;       /* COLOR_* — COLOR_UNKNOWN 时匹配任意颜色 */
    uint8_t target_shape;       /* SHAPE_* — SHAPE_UNKNOWN 时匹配任意形状 */
    uint8_t target_size_cm_x10; /* 0=wildcard 匹配任意尺寸，非零则精确匹配 */
} task_target_t;

/*--------------------------------------------------------------------------
 *  API
 *--------------------------------------------------------------------------*/

/* 初始化：清空目标和内部状态 */
void task_matcher_init(void);

/* 设定当前要寻找的物块目标。传入 NULL 则清空目标。 */
void task_matcher_set_target(const task_target_t *t);

/* 将一帧观测结果与当前目标比对。
 *
 * center_x / center_y: bbox 中心的像素坐标（来自 feature_snapshot_t.center）。
 * 若本次返回 MATCH_ACTION_GRAB，该坐标被内部保存，
 * 后续 task_matcher_get_grab_coord() 可取出。
 *
 * 决策规则（按优先级）：
 *   1. 未设定目标 → NONE
 *   2. color_id 或 shape_id 为 UNKNOWN → ERROR
 *   3. target_color != UNKNOWN 且 color 不匹配 → SKIP
 *   4. target_shape != UNKNOWN 且 shape 不匹配 → SKIP
 *   5. target_size != 0 且 size 不匹配 → SKIP
 *   6. 全部匹配 → GRAB */
uint8_t task_matcher_evaluate(const vision_result_t *obs,
                               uint16_t center_x, uint16_t center_y);

/* 获取最近一次 GRAB 匹配时的抓取坐标。
 * 返回 0 成功，-1 表示尚无 GRAB 匹配。 */
int task_matcher_get_grab_coord(uint16_t *cx, uint16_t *cy);

/* 获取当前目标（NULL=无目标），用于调试 / OSD 显示 */
const task_target_t *task_matcher_get_target(void);

/* 从 FPGA TARGET_SEL 寄存器读取并构建 cube 目标。
 * target_shape 固定为 SHAPE_CUBE（识别侧只抓正方体）。
 * 内部调用 board_io_read_target_sel_raw() 解码。
 * 返回 0=目标有效已设定, -1=target_valid=0 或 TARGET_SEL 未实现。
 *
 * 依赖 board_io.h 的 TARGET_SEL_AVAILABLE 编译开关：
 *   0（默认）→ 直接返回 -1，不读硬件；调试期手动 set_target()
 *   1        → 真正读 TARGET_SEL 寄存器
 * FPGA 队友确认地址/位定义后改为 1。
 *
 * TARGET_SEL 位定义（由 FPGA 队友提供，CPU 只读）：
 *   [1:0] color_sel   00=任意, 01=红, 10=蓝, 11=黄
 *   [3:2] size_sel    00=任意, 01=小(20mm), 10=中(25mm), 11=大(30mm)
 *   [4]   target_valid  0=无效, 1=有效 */
int task_matcher_read_target_from_fpga(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_MATCHER_H */
