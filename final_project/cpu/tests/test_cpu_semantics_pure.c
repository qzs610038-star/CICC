/*==========================================================================
 *  test_cpu_semantics_pure.c  —  纯语义头独立编译证明（compile-only）
 *
 *  仅包含 cpu_result_semantics.h（纯语义头），不包含任何上游头。
 *  由 run 脚本以 -Wall -Wextra -Werror（不加 -Wno-error=cpp、不定义 APB
 *  占位宏）单独编译为目标文件：
 *    - 若纯语义头意外传递引入 board_io.h，其 APB 占位 #warning 会在 -Werror
 *      下升级为错误导致编译失败；
 *    - 编译成功即证明纯语义头独立、无 APB 依赖、无占位告警。
 *
 *  同时用 _Static_assert 固定关键枚举数值约定（NONE=0，INVALID_INTERNAL=255）。
 *==========================================================================*/

#include "cpu_result_semantics.h"

/* 数值约定（Codex Gate 要求：显式赋值，NONE=0，INVALID_INTERNAL=255）。 */
_Static_assert(CPU_REASON_NONE == 0,
               "CPU_REASON_NONE must be 0");
_Static_assert(CPU_REASON_INVALID_INTERNAL == 255,
               "CPU_REASON_INVALID_INTERNAL must be 255");
_Static_assert(CPU_DECISION_NONE == 0,
               "CPU_DECISION_NONE must be 0");
_Static_assert(CPU_EXEC_NONE == 0,
               "CPU_EXEC_NONE must be 0");

/* 引用纯类型与文本接口，确保头文件在纯依赖下可用（compile-only）。 */
const char *cpu_semantics_pure_probe(void);
const char *cpu_semantics_pure_probe(void)
{
    cpu_display_result_t d;
    d.valid = 0u;
    d.is_target = 0u;
    d.decision = CPU_DECISION_NONE;
    d.execution = CPU_EXEC_NONE;
    d.reason = CPU_REASON_NONE;
    if (d.valid) {
        return cpu_decision_text(d.decision);
    }
    return cpu_reason_text(d.reason);
}
