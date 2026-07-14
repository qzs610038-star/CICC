/*==========================================================================
 *  main_loop_adapter.c  —  ARM_DISABLED 主循环适配器实现
 *
 *  从 main.c 提取的可测试单步函数。双方（main.c + Host test）链接同一份代码。
 *
 *  三个关键修复（相对首版 main.c 内联代码）：
 *    P1-1: 抽出为独立函数，Host test 可调用同一实现
 *    P1-2: 只在 event==REMOVE_CONFIRM && ACK_ACCEPTED 时返回 1（ABANDON 不冒充）
 *    P1-3: observation 使用调用方传入的 task_match_result_t（含真实 color/shape/size reason）
 *==========================================================================*/

#include "main_loop_adapter.h"
#include "cpu_result_semantics_adapters.h"
#include <string.h>

int main_loop_arm_disabled_step(
    round_controller_t *rc,
    uint16_t *uart_seq,
    uint32_t now_ms,
    const task_match_result_t *match,
    round_event_t uart_event,
    int uart_event_valid,
    cpu_display_result_t *display_out,
    round_controller_output_t *rc_out)
{
    round_controller_input_t  in;
    round_controller_output_t out;
    round_event_t             attempted_event = ROUND_EVENT_NONE;

    memset(&in, 0, sizeof(in));
    in.now_ms = now_ms;

    /* ---- UART 事件注入 ---- */
    if (uart_event_valid && uart_event != ROUND_EVENT_NONE) {
        in.event_valid = 1;
        in.event_seq   = (*uart_seq)++;
        in.event       = uart_event;
        attempted_event = uart_event;
    }

    /* ---- 观测注入（使用调用方传入的 match，含真实 reason） ---- */
    if (match != 0 &&
        (match->action == MATCH_ACTION_GRAB || match->action == MATCH_ACTION_SKIP)) {
        in.observation_valid = 1;
        in.match = *match;
    }

    /* ---- ARM_DISABLED 安全门 ---- */
    in.arm_enabled = 0;
    in.arm_busy    = 0;
    in.arm_done    = 0;
    in.arm_fault   = 0;

    /* ---- 推进状态机 ---- */
    round_controller_tick(rc, &in, &out);

    /* ---- 统一语义投影 ---- */
    if (display_out) {
        memset(display_out, 0, sizeof(*display_out));
    }
    cpu_display_from_round_output(&out, display_out);

    /* ---- 输出原始 RC 结果 ---- */
    if (rc_out) *rc_out = out;

    /* ---- P1-2: 判断解锁/复位条件，精确到事件类型 ---- */
    if (out.event_ack_valid &&
        out.event_ack_status == ROUND_EVENT_ACK_ACCEPTED) {

        /* SESSION_RESET → 全部重新初始化 */
        if (attempted_event == ROUND_EVENT_SESSION_RESET)
            return -1;

        /* 只有 REMOVE_CONFIRM 在 ROUND_DONE 被接受才触发解锁。
         * ABANDON/SOFT_RESET/其他事件即使 ACCEPTED 也不冒充 REMOVE。 */
        if (attempted_event == ROUND_EVENT_REMOVE_CONFIRM)
            return 1;
    }

    return 0;
}
