/*==========================================================================
 *  task_matcher.h  —  目标匹配与动作决策
 *
 *  接收 vision_classifier 的稳定分类结果，与任务目标比对，输出抓取/跳过/报错。
 *
 *  使用方式：
 *    1. task_matcher_set_target(&t) 或 task_matcher_read_target_from_fpga()
 *       设定本轮目标
 *    2. 每帧调用 task_matcher_evaluate(&obs, cx, cy) 评估
 *    3. 若返回 MATCH_ACTION_GRAB → 表示授权固定 P_pick 抓取序列。
 *       task_matcher_get_grab_coord() 可取出抓取中心坐标，
 *       但该坐标仅用于 OSD/日志/偏差检查，不作为机械臂实时坐标接口。
 *       （正式主线采用固定抓取点，不做视觉伺服闭环。）
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
#define MATCH_ACTION_GRAB    1   /* 三属性全部匹配 → 抓取 */
#define MATCH_ACTION_SKIP    2   /* 存在不匹配 → 跳过 */
#define MATCH_ACTION_ERROR   3   /* 观测无效（仅 DEBUG_MODE=1 时输出） */

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
 *   2. color_id 或 shape_id 为 UNKNOWN：
 *        DEBUG_MODE=1（调试）→ ERROR（便于 OSD 排查）
 *        DEBUG_MODE=0（正式）→ NONE（不抓、不报错，等待下一帧有效观测）
 *   3. target_color != UNKNOWN 且 color 不匹配 → SKIP
 *   4. target_shape != UNKNOWN 且 shape 不匹配 → SKIP
 *   5. target_size != 0 且 obs.size==0（Cam1 尺寸不可用）→ NONE
 *      （必须等 Cam1 侧面稳定；不能仅靠 Cam0 判断尺寸）
 *   6. target_size != 0 且 size 不匹配 → SKIP
 *   7. 全部匹配 → GRAB
 *
 * 注意：正式主线中 UNKNOWN 只输出 NONE，不输出 ERROR。
 * 这与 2026-07-09 用户决策一致：避免因偶然噪声帧触发误报错。 */
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
 *   0（默认）→ DEBUG_MODE=1 时保持当前目标不动（调试期手动 set_target）；
 *              DEBUG_MODE=0 时强制清空目标（正式主线安全策略），返回 -1。
 *   1        → 真正读 TARGET_SEL 寄存器
 * FPGA 队友确认地址/位定义后改为 1。
 *
 * 正式主线行为（2026-07-09 用户决策）：
 *   TARGET_SEL_AVAILABLE=0 或 target_valid=0 时，必须清空目标并返回 NONE，
 *   不允许沿用旧目标继续抓取。手动设定目标仅限显式调试模式。
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
