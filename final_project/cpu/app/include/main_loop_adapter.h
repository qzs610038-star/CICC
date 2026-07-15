/*==========================================================================
 *  main_loop_adapter.h  —  ARM_DISABLED 主循环适配器（可测试函数）
 *
 *  从 wsc 旧版 main.c 提取的 round_controller + cpu_result_semantics
 *  单步逻辑。当前正式 main.c 保留 arm_runtime structural bridge，在 G4
 *  取得受审事件源/单调时基前不调用本适配器；Host 测试直接覆盖本实现。
 *
 *  ARM_DISABLED 约束：
 *    - arm_enabled 固定为 0
 *    - 目标轮 → ARM_NOT_READY
 *    - 不调用 arm_controller、UART2、myCobot transport
 *    - 不连接/驱动机械臂
 *==========================================================================*/

#ifndef MAIN_LOOP_ADAPTER_H
#define MAIN_LOOP_ADAPTER_H

#include <stdint.h>
#include "round_controller.h"
#include "cpu_result_semantics.h"
#include "task_matcher.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 *  单步推进 ARM_DISABLED 主循环适配器。
 *
 *  参数：
 *    rc          — 持久 round_controller 状态（调用方分配，只由本函数推进）
 *    uart_seq    — 持久事件序号计数器（调用方分配，本函数递增）
 *    now_ms      — 当前时间戳（毫秒）
 *    match       — NULL = 无新观测；非 NULL = 从 task_matcher_get_last_match()
 *                  获得的本帧 match result（含 action/is_target/reason/mode）
 *    uart_event  — UART 注入的事件（ROUND_EVENT_NONE = 无事件）
 *    uart_event_valid — 1 = uart_event 有效
 *    display_out — 输出：统一语义投影结果（总是写入）
 *    rc_out      — 输出：round_controller 原始输出（总是写入，除非 NULL）
 *
 *  返回值：
 *     1 = REMOVE_CONFIRM 刚刚被 ACCEPTED → 调用方应调 task_matcher_next_round()
 *    -1 = SESSION_RESET 刚刚被 ACCEPTED → 调用方应重新初始化
 *     0 = 正常步进（无解锁或复位事件）
 *
 *  注意：
 *    - ABANDON 不会触发 matcher 解锁（只有 REMOVE 触发）。
 *    - 本函数不调 task_matcher_next_round()；解锁由返回值通知调用方执行。
 *    - match 的内容直接复制到 round_controller_input_t.match（含真实 reason）。
 *--------------------------------------------------------------------------*/
int main_loop_arm_disabled_step(
    round_controller_t *rc,
    uint16_t *uart_seq,
    uint32_t now_ms,
    const task_match_result_t *match,
    round_event_t uart_event,
    int uart_event_valid,
    cpu_display_result_t *display_out,
    round_controller_output_t *rc_out);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_LOOP_ADAPTER_H */
