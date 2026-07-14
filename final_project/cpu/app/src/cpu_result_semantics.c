/*==========================================================================
 *  cpu_result_semantics.c  —  统一 CPU 结果/理由语义：纯文本接口实现
 *
 *  只依赖纯语义头（<stdint.h>），不引入任何上游头，因此本编译单元不含
 *  board_io.h 的 APB 占位 #warning，可在无占位宏定义时干净编译。
 *
 *  文本兜底约定（Codex Gate 要求）：
 *    - 非法/越界 reason  → "INVALID_INTERNAL"
 *    - 非法/越界 decision → "INVALID_INTERNAL"（不返回 "NONE" 掩盖）
 *    - 非法/越界 execution→ "INVALID_INTERNAL"（不返回 "NONE" 掩盖）
 *==========================================================================*/

#include "cpu_result_semantics.h"

const char *cpu_reason_text(cpu_reason_t reason)
{
    switch (reason) {
    case CPU_REASON_NONE:                      return "NONE";
    case CPU_REASON_TARGET_MATCH:              return "TARGET_MATCH";
    case CPU_REASON_COLOR_MISMATCH:            return "COLOR_MISMATCH";
    case CPU_REASON_SHAPE_MISMATCH:            return "SHAPE_MISMATCH";
    case CPU_REASON_SIZE_DIFF_NOT_10MM:        return "SIZE_DIFF_NOT_10MM";
    case CPU_REASON_SIZE_DIFF_OVER_5MM:        return "SIZE_DIFF_OVER_5MM";
    case CPU_REASON_OBSERVATION_UNKNOWN:       return "OBSERVATION_UNKNOWN";
    case CPU_REASON_OBSERVATION_UNSTABLE:      return "OBSERVATION_UNSTABLE";
    case CPU_REASON_SIZE_UNAVAILABLE:          return "SIZE_UNAVAILABLE";
    case CPU_REASON_INVALID_TARGET:            return "INVALID_TARGET";
    case CPU_REASON_OPERATOR_ABANDON:          return "OPERATOR_ABANDON";
    case CPU_REASON_ARM_NOT_READY:             return "ARM_NOT_READY";
    case CPU_REASON_ARM_FAULT:                 return "ARM_FAULT";
    case CPU_REASON_ACQUIRE_STABILITY_TIMEOUT: return "ACQUIRE_STABILITY_TIMEOUT";
    case CPU_REASON_ROUND_TIMEOUT:             return "ROUND_TIMEOUT";
    case CPU_REASON_INVALID_INTERNAL:          return "INVALID_INTERNAL";
    default:                                   return "INVALID_INTERNAL";
    }
}

const char *cpu_decision_text(cpu_decision_t decision)
{
    switch (decision) {
    case CPU_DECISION_NONE:    return "NONE";
    case CPU_DECISION_EXECUTE: return "EXECUTE";
    case CPU_DECISION_SKIP:    return "SKIP";
    case CPU_DECISION_ERROR:   return "ERROR";
    default:                   return "INVALID_INTERNAL";
    }
}

const char *cpu_execution_text(cpu_execution_t execution)
{
    switch (execution) {
    case CPU_EXEC_NONE:               return "NONE";
    case CPU_EXEC_REQUESTED:          return "REQUESTED";
    case CPU_EXEC_SKIPPED_NON_TARGET: return "SKIPPED_NON_TARGET";
    case CPU_EXEC_BLOCKED:            return "BLOCKED";
    case CPU_EXEC_FAULT:              return "FAULT";
    default:                          return "INVALID_INTERNAL";
    }
}
