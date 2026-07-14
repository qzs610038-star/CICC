/*==========================================================================
 *  cpu_result_semantics_adapters.c  —  上游枚举/结构 → 统一语义的转换实现
 *
 *  只做寄存器无关的枚举映射与结构投影：
 *    - 两套内部理由码 → 统一 cpu_reason_t（competition 侧结合任务模式细分）
 *    - round_controller_output_t → cpu_display_result_t（含完整合法组合校验）
 *  不涉及 APB 地址、MMIO、机械臂动作或 round_controller 语义变更。
 *
 *  合法 (action + reason + is_target) 组合（result_valid==1 时）：
 *    A. GRAB + TARGET_MATCH + is_target==1
 *         → EXECUTE / REQUESTED / TARGET_MATCH
 *    B. SKIP + {COLOR|SHAPE|SIZE_NOT_EQ_10MM|SIZE_OUTSIDE_5MM} + is_target==0
 *         → SKIP / SKIPPED_NON_TARGET / 细分理由
 *    C. NONE + 阻止理由，且 is_target 必须与该理由的真实语义一致：
 *         - ARM_NOT_READY               + is_target==1
 *         - OPERATOR_ABANDON            + is_target==0
 *         - STABILITY_TIMEOUT           + is_target==0
 *         - OBSERVATION_UNKNOWN         + is_target==0
 *         - TARGET_INVALID              + is_target==0
 *         → NONE / BLOCKED / 细分理由
 *    D. NONE + ARM_FAULT + is_target==1
 *         → NONE / FAULT / ARM_FAULT
 *  以上 is_target 取值精确匹配 round_controller 已知输出集合；任何理由配错误
 *  is_target（如 ARM_FAULT+is_target==0、OPERATOR_ABANDON+is_target==1）都视为
 *  矛盾组合。其余矛盾/非法组合统一安全兜底：
 *    valid=1, is_target=0, decision=ERROR, execution=FAULT,
 *    reason=INVALID_INTERNAL —— 绝不输出 REQUESTED 或正常 SKIP。
 *==========================================================================*/

#include "cpu_result_semantics_adapters.h"

cpu_reason_t cpu_reason_from_reason_code(reason_code_t reason)
{
    switch (reason) {
    case REASON_TARGET_MATCH:        return CPU_REASON_TARGET_MATCH;
    case REASON_COLOR_MISMATCH:      return CPU_REASON_COLOR_MISMATCH;
    case REASON_SHAPE_MISMATCH:      return CPU_REASON_SHAPE_MISMATCH;
    case REASON_SIZE_NOT_EQ_10MM:    return CPU_REASON_SIZE_DIFF_NOT_10MM;
    case REASON_SIZE_OUTSIDE_5MM:    return CPU_REASON_SIZE_DIFF_OVER_5MM;
    case REASON_OBSERVATION_UNKNOWN: return CPU_REASON_OBSERVATION_UNKNOWN;
    case REASON_TARGET_INVALID:      return CPU_REASON_INVALID_TARGET;
    case REASON_STABILITY_TIMEOUT:   return CPU_REASON_ACQUIRE_STABILITY_TIMEOUT;
    case REASON_OPERATOR_ABANDON:    return CPU_REASON_OPERATOR_ABANDON;
    case REASON_ARM_NOT_READY:       return CPU_REASON_ARM_NOT_READY;
    case REASON_ARM_FAULT:           return CPU_REASON_ARM_FAULT;
    default:                         return CPU_REASON_INVALID_INTERNAL;
    }
}

cpu_reason_t cpu_reason_from_competition(competition_reason_t reason,
                                         competition_task_mode_t mode)
{
    switch (reason) {
    case COMP_REASON_NONE:            return CPU_REASON_NONE;
    case COMP_REASON_TARGET_MATCH:    return CPU_REASON_TARGET_MATCH;
    case COMP_REASON_COLOR_MISMATCH:  return CPU_REASON_COLOR_MISMATCH;
    case COMP_REASON_SHAPE_MISMATCH:  return CPU_REASON_SHAPE_MISMATCH;
    case COMP_REASON_SIZE_RELATION_MISMATCH:
        /* 尺寸关系不匹配的语义完全依赖任务模式，无上下文不得臆断。 */
        switch (mode) {
        case COMP_TASK_SIZE_DELTA_1CM_CUBE:    return CPU_REASON_SIZE_DIFF_NOT_10MM;
        case COMP_TASK_SIZE_WITHIN_0P5CM_CUBE: return CPU_REASON_SIZE_DIFF_OVER_5MM;
        case COMP_TASK_COLOR_CUBE:
        case COMP_TASK_SHAPE_COLOR_CUBE:
        default:                               return CPU_REASON_INVALID_INTERNAL;
        }
    case COMP_REASON_OBSERVATION_UNSTABLE: return CPU_REASON_OBSERVATION_UNSTABLE;
    case COMP_REASON_INVALID_TARGET:       return CPU_REASON_INVALID_TARGET;
    case COMP_REASON_SIZE_UNAVAILABLE:     return CPU_REASON_SIZE_UNAVAILABLE;
    case COMP_REASON_OPERATOR_ABANDONED:   return CPU_REASON_OPERATOR_ABANDON;
    case COMP_REASON_ROUND_TIMEOUT:        return CPU_REASON_ROUND_TIMEOUT;
    default:                               return CPU_REASON_INVALID_INTERNAL;
    }
}

/* 非目标跳过的合法上游理由集合（配合 action==SKIP && is_target==0）。 */
static int is_skip_reason(reason_code_t reason)
{
    switch (reason) {
    case REASON_COLOR_MISMATCH:
    case REASON_SHAPE_MISMATCH:
    case REASON_SIZE_NOT_EQ_10MM:
    case REASON_SIZE_OUTSIDE_5MM:
        return 1;
    default:
        return 0;
    }
}

/* 被安全门/放弃/超时/观测阻止的合法上游理由，及其唯一合法 is_target。
 * 精确匹配 round_controller 真实输出（不是超集）：
 *   - ARM_NOT_READY：识别到目标但机械臂不可用，保留 is_target=1；
 *   - OPERATOR_ABANDON / STABILITY_TIMEOUT / OBSERVATION_UNKNOWN /
 *     TARGET_INVALID：本轮无有效目标，is_target 必须为 0。
 * 命中则返回 1 并写出 *expected；非阻止理由返回 0。 */
static int blocked_reason_expected_target(reason_code_t reason, uint8_t *expected)
{
    switch (reason) {
    case REASON_ARM_NOT_READY:
        *expected = 1u;
        return 1;
    case REASON_OPERATOR_ABANDON:
    case REASON_STABILITY_TIMEOUT:
    case REASON_OBSERVATION_UNKNOWN:
    case REASON_TARGET_INVALID:
        *expected = 0u;
        return 1;
    default:
        return 0;
    }
}

static void set_no_result(cpu_display_result_t *display)
{
    display->valid = 0u;
    display->is_target = 0u;
    display->decision = CPU_DECISION_NONE;
    display->execution = CPU_EXEC_NONE;
    display->reason = CPU_REASON_NONE;
}

/* 矛盾/非法组合的安全兜底：明确暴露为内部错误，绝不解释成目标或执行请求。 */
static void set_error_result(cpu_display_result_t *display)
{
    display->valid = 1u;
    display->is_target = 0u;
    display->decision = CPU_DECISION_ERROR;
    display->execution = CPU_EXEC_FAULT;
    display->reason = CPU_REASON_INVALID_INTERNAL;
}

void cpu_display_from_round_output(const round_controller_output_t *out,
                                   cpu_display_result_t *display)
{
    uint8_t action;
    uint8_t is_target;
    reason_code_t reason;

    if (display == 0) {
        return;
    }
    if (out == 0 || !out->result_valid) {
        /* 无结果或空输入：安全空结果，绝不遗留陈旧的目标解释。 */
        set_no_result(display);
        return;
    }

    action = out->decision_action;
    is_target = out->is_target ? 1u : 0u;
    reason = out->reason;

    /* A. 目标 → 授权抓取（三要素必须一致）。 */
    if (action == MATCH_ACTION_GRAB &&
        reason == REASON_TARGET_MATCH && is_target == 1u) {
        display->valid = 1u;
        display->is_target = 1u;
        display->decision = CPU_DECISION_EXECUTE;
        display->execution = CPU_EXEC_REQUESTED;
        display->reason = CPU_REASON_TARGET_MATCH;
        return;
    }

    /* B. 正常非目标跳过（动作、理由、is_target 必须自洽）。 */
    if (action == MATCH_ACTION_SKIP && is_target == 0u && is_skip_reason(reason)) {
        display->valid = 1u;
        display->is_target = 0u;
        display->decision = CPU_DECISION_SKIP;
        display->execution = CPU_EXEC_SKIPPED_NON_TARGET;
        display->reason = cpu_reason_from_reason_code(reason);
        return;
    }

    /* C. 被安全门/放弃/超时/观测阻止：动作必须为 NONE，且 is_target 必须与该
     *    理由的真实语义一致（精确匹配 round_controller 输出）。不一致即视为
     *    矛盾组合，安全兜底，绝不解释成 BLOCKED/目标。 */
    if (action == MATCH_ACTION_NONE) {
        uint8_t expected_target;
        if (blocked_reason_expected_target(reason, &expected_target)) {
            if (is_target != expected_target) {
                set_error_result(display);
                return;
            }
            display->valid = 1u;
            display->is_target = expected_target;
            display->decision = CPU_DECISION_NONE;
            display->execution = CPU_EXEC_BLOCKED;
            display->reason = cpu_reason_from_reason_code(reason);
            return;
        }

        /* D. 机械臂故障：动作必须为 NONE，且必须保留 is_target==1（只能经由
         *    GRAB→WAIT_ARM_DONE 抵达）；is_target==0 与该理由矛盾，安全兜底。 */
        if (reason == REASON_ARM_FAULT) {
            if (is_target != 1u) {
                set_error_result(display);
                return;
            }
            display->valid = 1u;
            display->is_target = 1u;
            display->decision = CPU_DECISION_NONE;
            display->execution = CPU_EXEC_FAULT;
            display->reason = CPU_REASON_ARM_FAULT;
            return;
        }
    }

    /* 其余：矛盾组合（如 GRAB 配非目标理由、SKIP 配目标理由、阻止/故障理由配错误
     * is_target、非法动作/理由等）一律安全兜底为内部错误，绝不输出 REQUESTED
     * 或正常 SKIP。 */
    set_error_result(display);
}
