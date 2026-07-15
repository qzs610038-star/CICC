/*==========================================================================
 *  cpu_result_semantics_adapters.h  —  上游枚举/结构 → 统一语义的转换接口
 *
 *  本文件承接 cpu_result_semantics.h（纯语义类型），额外包含上游头文件，
 *  声明从两套内部理由码及 round_controller 输出到统一展示语义的转换函数。
 *
 *  与纯语义头的区别：
 *    - 纯语义头只依赖 <stdint.h>，可独立编译，不含 APB 占位 #warning。
 *    - 本适配头包含 task_matcher.h / competition_tasks.h / round_controller.h，
 *      会传递引入 vision_classifier.h → board_io.h 的 APB 占位 #warning，
 *      属既有约定放行项；需要该编译单元定义 APB 基址占位宏。
 *
 *  安全约定：
 *    - 所有转换以 switch 实现（非数组索引），未知/越界/矛盾组合一律安全
 *      映射为 CPU_REASON_INVALID_INTERNAL / CPU_EXEC_FAULT，绝不默认解释
 *      成目标或执行请求。
 *    - 不定义任何 APB 地址 / 寄存器位布局；不修改上游理由语义。
 *==========================================================================*/

#ifndef CPU_RESULT_SEMANTICS_ADAPTERS_H
#define CPU_RESULT_SEMANTICS_ADAPTERS_H

#include "cpu_result_semantics.h"  /* 纯语义类型 */
#include "task_matcher.h"          /* reason_code_t / REASON_* / MATCH_ACTION_* */
#include "competition_tasks.h"     /* competition_reason_t / competition_task_mode_t */
#include "round_controller.h"      /* round_controller_output_t */

#ifdef __cplusplus
extern "C" {
#endif

/* 将 task_matcher / round_controller 的 reason_code_t 映射为统一理由码。
 * 未知/越界值安全映射为 CPU_REASON_INVALID_INTERNAL。 */
cpu_reason_t cpu_reason_from_reason_code(reason_code_t reason);

/* 将 competition_tasks 的 competition_reason_t 映射为统一理由码。
 *   COMP_REASON_SIZE_RELATION_MISMATCH 必须结合任务模式细分：
 *     Task 3 (SIZE_DELTA_1CM)     → CPU_REASON_SIZE_DIFF_NOT_10MM
 *     Task 4 (SIZE_WITHIN_0P5CM)  → CPU_REASON_SIZE_DIFF_OVER_5MM
 *     其他模式 / 无上下文          → CPU_REASON_INVALID_INTERNAL
 * 其余理由与 mode 无关；未知/越界值安全映射为 CPU_REASON_INVALID_INTERNAL。 */
cpu_reason_t cpu_reason_from_competition(competition_reason_t reason,
                                         competition_task_mode_t mode);

/* 将 round_controller 输出投影为统一展示结果，并校验
 * (action + reason + is_target) 的完整合法组合：
 *   out == NULL     → *display 填为安全空结果（valid=0）。
 *   display == NULL → 直接返回（无输出可写）。
 *   result_valid==0 → 安全空结果（忽略其余字段的陈旧/垃圾值）。
 *   合法组合        → 映射为对应 decision/execution/reason。
 *   矛盾/非法组合    → valid=1, is_target=0, decision=ERROR,
 *                     execution=FAULT, reason=INVALID_INTERNAL；
 *                     绝不输出 REQUESTED 或正常 SKIP。 */
void cpu_display_from_round_output(const round_controller_output_t *out,
                                   cpu_display_result_t *display);

#ifdef __cplusplus
}
#endif

#endif /* CPU_RESULT_SEMANTICS_ADAPTERS_H */
