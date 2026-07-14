/*==========================================================================
 *  cpu_result_semantics.h  —  统一 CPU 结果/理由展示语义（纯类型层）
 *
 *  目的：
 *    仓库当前并存两套内部理由枚举：
 *      1. task_matcher.h / round_controller 使用 reason_code_t / REASON_*
 *      2. competition_tasks.h 使用 competition_reason_t / COMP_REASON_*
 *    官方细则要求每轮明确输出「识别 / 判断 / 执行或不执行 / 理由」。后续
 *    OSD 与 APB 打包不应直接理解两套内部枚举，因此本模块提供一个寄存器无关
 *    的统一展示语义层：唯一的理由枚举、唯一的判断码、唯一的执行状态。
 *
 *  头文件拆分（本轮）：
 *    - 本文件只放「纯语义类型 + 文本接口」，仅依赖 <stdint.h>，不引入
 *      task_matcher.h / competition_tasks.h / round_controller.h，
 *      因此也不会传递引入 board_io.h 的 APB 占位 #warning；可独立编译。
 *    - 上游枚举/结构 → 统一语义的转换接口放在
 *      cpu_result_semantics_adapters.h（需要包含上游头）。
 *
 *  ABI 约定（重要）：
 *    - 以下 enum / struct 仅用于 CPU 内部语义传递，**不是** APB / OSD 的
 *      wire ABI。数值可能随内部演进调整，禁止把这些类型的原始字节直接
 *      序列化进寄存器、帧缓存或对外协议；对外编码必须经过后续独立的
 *      OSD/APB 适配层显式映射。
 *    - 所有公开枚举显式赋值；NONE 恒为 0，INVALID_INTERNAL 恒为 255，
 *      便于兜底判定与调试观察，未知/越界一律安全落到 INVALID_INTERNAL。
 *==========================================================================*/

#ifndef CPU_RESULT_SEMANTICS_H
#define CPU_RESULT_SEMANTICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 *  统一展示理由码（寄存器无关，CPU 内部语义，非 wire ABI）
 *  细分尺寸/观测/超时类理由，保留 OSD 需要的区分度。
 *  未知/越界/矛盾组合一律 → CPU_REASON_INVALID_INTERNAL(255)。
 *--------------------------------------------------------------------------*/
typedef enum {
    CPU_REASON_NONE                      = 0,   /* 无理由 / 尚无结果 */
    CPU_REASON_TARGET_MATCH              = 1,   /* 命中目标 */
    CPU_REASON_COLOR_MISMATCH            = 2,   /* 颜色不匹配 */
    CPU_REASON_SHAPE_MISMATCH            = 3,   /* 形状不匹配 */
    CPU_REASON_SIZE_DIFF_NOT_10MM        = 4,   /* 任务三：|obs-ref| != 10mm */
    CPU_REASON_SIZE_DIFF_OVER_5MM        = 5,   /* 任务四：|obs-target| > 5mm */
    CPU_REASON_OBSERVATION_UNKNOWN       = 6,   /* 分类未知/观测不可判定 */
    CPU_REASON_OBSERVATION_UNSTABLE      = 7,   /* 观测不稳定（竞赛评估侧）*/
    CPU_REASON_SIZE_UNAVAILABLE          = 8,   /* 尺寸不可用（竞赛评估侧）*/
    CPU_REASON_INVALID_TARGET            = 9,   /* 目标配置非法 */
    CPU_REASON_OPERATOR_ABANDON          = 10,  /* 操作员放弃本轮 */
    CPU_REASON_ARM_NOT_READY             = 11,  /* 机械臂未就绪（未使能/正忙）*/
    CPU_REASON_ARM_FAULT                 = 12,  /* 机械臂故障 / 等待超时故障 */
    CPU_REASON_ACQUIRE_STABILITY_TIMEOUT = 13,  /* 取样阶段稳定超时 */
    CPU_REASON_ROUND_TIMEOUT             = 14,  /* 轮级超时（竞赛评估侧）*/
    CPU_REASON_INVALID_INTERNAL          = 255  /* 未知/越界/矛盾组合，安全兜底 */
} cpu_reason_t;

/*--------------------------------------------------------------------------
 *  统一判断/动作码（识别—判断层的稳定表达，CPU 内部语义，非 wire ABI）
 *--------------------------------------------------------------------------*/
typedef enum {
    CPU_DECISION_NONE    = 0,   /* 无动作 / 未判定 / 动作被降级 */
    CPU_DECISION_EXECUTE = 1,   /* 目标 → 授权固定抓取 */
    CPU_DECISION_SKIP    = 2,   /* 非目标 → 跳过 */
    CPU_DECISION_ERROR   = 3    /* 观测/内部异常或矛盾组合，不作为目标 */
} cpu_decision_t;

/*--------------------------------------------------------------------------
 *  统一执行状态（“执行或不执行及理由”中的执行侧，CPU 内部语义，非 wire ABI）
 *  区分：尚无结果 / 请求执行 / 正常非目标跳过 / 被阻止 / 执行故障。
 *--------------------------------------------------------------------------*/
typedef enum {
    CPU_EXEC_NONE               = 0,   /* 尚无结果 或 有结果但无可执行动作 */
    CPU_EXEC_REQUESTED          = 1,   /* 已请求执行（抓取已授权/进行中/完成）*/
    CPU_EXEC_SKIPPED_NON_TARGET = 2,   /* 正常非目标跳过 */
    CPU_EXEC_BLOCKED            = 3,   /* 因安全门 / 放弃 / 超时被阻止 */
    CPU_EXEC_FAULT              = 4    /* 执行故障 / 矛盾组合安全兜底 */
} cpu_execution_t;

/*--------------------------------------------------------------------------
 *  统一展示结果（供 OSD / 后续 APB 打包唯一消费；非 wire ABI，禁止直接序列化）
 *--------------------------------------------------------------------------*/
typedef struct {
    uint8_t         valid;      /* 1 = 本轮结果已锁存；0 = 尚无结果 */
    uint8_t         is_target;  /* 1 = 判定为目标；0 = 非目标/不可判定 */
    cpu_decision_t  decision;   /* 判断/动作码 */
    cpu_execution_t execution;  /* 执行状态 */
    cpu_reason_t    reason;     /* 统一理由 */
} cpu_display_result_t;

/*--------------------------------------------------------------------------
 *  文本接口（OSD/日志用，寄存器无关，纯类型依赖）
 *  非法/越界值返回 "INVALID_INTERNAL"（decision/execution 同样兜底为
 *  "INVALID_INTERNAL"），绝不返回 "NONE" 掩盖非法输入。
 *--------------------------------------------------------------------------*/
const char *cpu_reason_text(cpu_reason_t reason);
const char *cpu_decision_text(cpu_decision_t decision);
const char *cpu_execution_text(cpu_execution_t execution);

#ifdef __cplusplus
}
#endif

#endif /* CPU_RESULT_SEMANTICS_H */
