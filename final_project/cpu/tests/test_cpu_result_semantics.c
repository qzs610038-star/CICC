/*==========================================================================
 *  test_cpu_result_semantics.c  —  统一 CPU 结果/理由语义层 Host 测试
 *
 *  覆盖（Codex Gate 复审版）：
 *    - reason_code_t 每个合法值 → 统一理由（含尺寸/观测/超时细分）
 *    - competition_reason_t 每个合法值 → 统一理由（含任务模式相关的
 *      SIZE_RELATION_MISMATCH：Task3/Task4/无上下文）
 *    - 两套枚举的非法/越界值 → CPU_REASON_INVALID_INTERNAL
 *    - (action + reason + is_target) 完整合法组合矩阵：A/B/C/D
 *    - 阻止/故障理由的 is_target 极性：每个理由同时覆盖合法与相反 is_target，
 *      合法侧判为 BLOCKED/FAULT，相反侧必须安全失败（绝不 REQUESTED/正常 SKIP）
 *    - 矛盾/非法组合矩阵：一律 ERROR/FAULT/INVALID_INTERNAL，
 *      且断言绝不为 REQUESTED / 正常 SKIPPED
 *    - result_valid=0 且其余字段为垃圾值 → 完整安全空结果
 *    - NULL 输入 / NULL 输出
 *    - 文本接口（含非法值 → INVALID_INTERNAL，不返回 NONE）
 *    - 真实 round_controller 端到端投影
 *
 *  纯 Host/Mock，不连接或驱动任何机械臂，不读写 MMIO。
 *==========================================================================*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cpu_result_semantics.h"
#include "cpu_result_semantics_adapters.h"
#include "round_controller.h"

static int _test_failures = 0;
static int _test_count = 0;
static int _test_start = 0;

static void _check(const char *file, int line, int cond, const char *msg)
{
    _test_count++;
    if (!cond) {
        _test_failures++;
        printf("  FAIL [%s:%d] %s\n", file, line, msg);
    }
}

#define CHECK(cond) _check(__FILE__, __LINE__, (cond), #cond)
#define CHECK_EQ(a, b) _check(__FILE__, __LINE__, ((a) == (b)), #a " == " #b)
#define TEST(name) \
    printf("  %-64s", name " "); fflush(stdout); _test_start = _test_failures
#define PASS() \
    do { int d = _test_failures - _test_start; \
         if (d == 0) printf("PASS\n"); else printf("%d FAILED\n", d); } while (0)

/*==========================================================================
 *  reason_code_t → 统一理由映射（每个合法值 + 非法值）
 *==========================================================================*/
static void test_reason_code_mapping_legal(void)
{
    TEST("reason_code: every legal value maps to expected unified reason");
    CHECK_EQ(cpu_reason_from_reason_code(REASON_TARGET_MATCH),
             CPU_REASON_TARGET_MATCH);
    CHECK_EQ(cpu_reason_from_reason_code(REASON_COLOR_MISMATCH),
             CPU_REASON_COLOR_MISMATCH);
    CHECK_EQ(cpu_reason_from_reason_code(REASON_SHAPE_MISMATCH),
             CPU_REASON_SHAPE_MISMATCH);
    CHECK_EQ(cpu_reason_from_reason_code(REASON_SIZE_NOT_EQ_10MM),
             CPU_REASON_SIZE_DIFF_NOT_10MM);
    CHECK_EQ(cpu_reason_from_reason_code(REASON_SIZE_OUTSIDE_5MM),
             CPU_REASON_SIZE_DIFF_OVER_5MM);
    CHECK_EQ(cpu_reason_from_reason_code(REASON_OBSERVATION_UNKNOWN),
             CPU_REASON_OBSERVATION_UNKNOWN);
    CHECK_EQ(cpu_reason_from_reason_code(REASON_TARGET_INVALID),
             CPU_REASON_INVALID_TARGET);
    CHECK_EQ(cpu_reason_from_reason_code(REASON_STABILITY_TIMEOUT),
             CPU_REASON_ACQUIRE_STABILITY_TIMEOUT);
    CHECK_EQ(cpu_reason_from_reason_code(REASON_OPERATOR_ABANDON),
             CPU_REASON_OPERATOR_ABANDON);
    CHECK_EQ(cpu_reason_from_reason_code(REASON_ARM_NOT_READY),
             CPU_REASON_ARM_NOT_READY);
    CHECK_EQ(cpu_reason_from_reason_code(REASON_ARM_FAULT),
             CPU_REASON_ARM_FAULT);
    PASS();
}

static void test_reason_code_mapping_illegal(void)
{
    TEST("reason_code: illegal / out-of-range values map to INVALID_INTERNAL");
    /* 恰好越过最大合法枚举 (REASON_ARM_FAULT==10) 的下一个值 */
    CHECK_EQ(cpu_reason_from_reason_code((reason_code_t)11),
             CPU_REASON_INVALID_INTERNAL);
    CHECK_EQ(cpu_reason_from_reason_code((reason_code_t)99),
             CPU_REASON_INVALID_INTERNAL);
    CHECK_EQ(cpu_reason_from_reason_code((reason_code_t)255),
             CPU_REASON_INVALID_INTERNAL);
    CHECK_EQ(cpu_reason_from_reason_code((reason_code_t)0x7fffffff),
             CPU_REASON_INVALID_INTERNAL);
    PASS();
}

/*==========================================================================
 *  competition_reason_t → 统一理由映射（含任务模式细分 + 非法值）
 *==========================================================================*/
static void test_competition_reason_mapping_legal(void)
{
    TEST("competition_reason: every legal value maps to expected unified");
    /* 与 mode 无关的理由：任取一个非尺寸模式验证稳定 */
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_NONE, COMP_TASK_COLOR_CUBE),
             CPU_REASON_NONE);
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_TARGET_MATCH,
             COMP_TASK_COLOR_CUBE), CPU_REASON_TARGET_MATCH);
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_COLOR_MISMATCH,
             COMP_TASK_COLOR_CUBE), CPU_REASON_COLOR_MISMATCH);
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_SHAPE_MISMATCH,
             COMP_TASK_SHAPE_COLOR_CUBE), CPU_REASON_SHAPE_MISMATCH);
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_OBSERVATION_UNSTABLE,
             COMP_TASK_COLOR_CUBE), CPU_REASON_OBSERVATION_UNSTABLE);
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_INVALID_TARGET,
             COMP_TASK_COLOR_CUBE), CPU_REASON_INVALID_TARGET);
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_SIZE_UNAVAILABLE,
             COMP_TASK_SIZE_DELTA_1CM_CUBE), CPU_REASON_SIZE_UNAVAILABLE);
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_OPERATOR_ABANDONED,
             COMP_TASK_COLOR_CUBE), CPU_REASON_OPERATOR_ABANDON);
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_ROUND_TIMEOUT,
             COMP_TASK_COLOR_CUBE), CPU_REASON_ROUND_TIMEOUT);
    PASS();
}

static void test_competition_size_relation_mode_split(void)
{
    TEST("competition_reason: SIZE_RELATION_MISMATCH splits by task mode");
    /* Task 3 → 差值须为 10mm；Task 4 → 差值须 ≤ 5mm */
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_SIZE_RELATION_MISMATCH,
             COMP_TASK_SIZE_DELTA_1CM_CUBE), CPU_REASON_SIZE_DIFF_NOT_10MM);
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_SIZE_RELATION_MISMATCH,
             COMP_TASK_SIZE_WITHIN_0P5CM_CUBE), CPU_REASON_SIZE_DIFF_OVER_5MM);
    /* 无尺寸上下文（Task1/Task2/非法 mode）→ 不臆断，安全兜底 */
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_SIZE_RELATION_MISMATCH,
             COMP_TASK_COLOR_CUBE), CPU_REASON_INVALID_INTERNAL);
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_SIZE_RELATION_MISMATCH,
             COMP_TASK_SHAPE_COLOR_CUBE), CPU_REASON_INVALID_INTERNAL);
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_SIZE_RELATION_MISMATCH,
             (competition_task_mode_t)0), CPU_REASON_INVALID_INTERNAL);
    CHECK_EQ(cpu_reason_from_competition(COMP_REASON_SIZE_RELATION_MISMATCH,
             (competition_task_mode_t)99), CPU_REASON_INVALID_INTERNAL);
    PASS();
}

static void test_competition_reason_mapping_illegal(void)
{
    TEST("competition_reason: illegal values map to INVALID_INTERNAL");
    /* 恰好越过最大合法枚举 (COMP_REASON_ROUND_TIMEOUT==9) 的下一个值 */
    CHECK_EQ(cpu_reason_from_competition((competition_reason_t)10,
             COMP_TASK_COLOR_CUBE), CPU_REASON_INVALID_INTERNAL);
    CHECK_EQ(cpu_reason_from_competition((competition_reason_t)200,
             COMP_TASK_SIZE_DELTA_1CM_CUBE), CPU_REASON_INVALID_INTERNAL);
    CHECK_EQ(cpu_reason_from_competition((competition_reason_t)0x7fffffff,
             COMP_TASK_COLOR_CUBE), CPU_REASON_INVALID_INTERNAL);
    PASS();
}

/*==========================================================================
 *  round_controller_output_t → 统一展示结果（手工构造）
 *==========================================================================*/
static round_controller_output_t make_output(uint8_t result_valid,
                                             uint8_t decision_action,
                                             uint8_t is_target,
                                             reason_code_t reason)
{
    round_controller_output_t out;
    memset(&out, 0, sizeof(out));
    out.state = ROUND_STATE_ROUND_DONE;
    out.result_valid = result_valid;
    out.decision_action = decision_action;
    out.is_target = is_target;
    out.reason = reason;
    return out;
}

/* 断言一个投影结果是「安全空结果」（尚无结果）。 */
static void expect_empty(const cpu_display_result_t *d)
{
    CHECK_EQ(d->valid, 0u);
    CHECK_EQ(d->is_target, 0u);
    CHECK_EQ(d->decision, CPU_DECISION_NONE);
    CHECK_EQ(d->execution, CPU_EXEC_NONE);
    CHECK_EQ(d->reason, CPU_REASON_NONE);
}

/* 断言一个投影结果是矛盾/非法组合的「安全错误结果」，
 * 并显式确认绝不是 REQUESTED / 正常 SKIPPED。 */
static void expect_safe_error(const cpu_display_result_t *d)
{
    CHECK_EQ(d->valid, 1u);
    CHECK_EQ(d->is_target, 0u);
    CHECK_EQ(d->decision, CPU_DECISION_ERROR);
    CHECK_EQ(d->execution, CPU_EXEC_FAULT);
    CHECK_EQ(d->reason, CPU_REASON_INVALID_INTERNAL);
    CHECK(d->execution != CPU_EXEC_REQUESTED);
    CHECK(d->execution != CPU_EXEC_SKIPPED_NON_TARGET);
    CHECK(d->decision != CPU_DECISION_EXECUTE);
    CHECK(d->decision != CPU_DECISION_SKIP);
}

/*----- A: 合法目标执行 -----------------------------------------------------*/
static void test_combo_legal_target_execute(void)
{
    TEST("combo A: GRAB + TARGET_MATCH + is_target=1 -> EXECUTE/REQUESTED");
    round_controller_output_t out =
        make_output(1u, MATCH_ACTION_GRAB, 1u, REASON_TARGET_MATCH);
    cpu_display_result_t d;
    cpu_display_from_round_output(&out, &d);
    CHECK_EQ(d.valid, 1u);
    CHECK_EQ(d.is_target, 1u);
    CHECK_EQ(d.decision, CPU_DECISION_EXECUTE);
    CHECK_EQ(d.execution, CPU_EXEC_REQUESTED);
    CHECK_EQ(d.reason, CPU_REASON_TARGET_MATCH);
    PASS();
}

/*----- B: 合法非目标跳过（四种理由）--------------------------------------*/
static void check_skip(reason_code_t reason, cpu_reason_t expect, const char *msg)
{
    round_controller_output_t out = make_output(1u, MATCH_ACTION_SKIP, 0u, reason);
    cpu_display_result_t d;
    cpu_display_from_round_output(&out, &d);
    _check(__FILE__, __LINE__, d.valid == 1u, msg);
    _check(__FILE__, __LINE__, d.is_target == 0u, msg);
    _check(__FILE__, __LINE__, d.decision == CPU_DECISION_SKIP, msg);
    _check(__FILE__, __LINE__, d.execution == CPU_EXEC_SKIPPED_NON_TARGET, msg);
    _check(__FILE__, __LINE__, d.reason == expect, msg);
}

static void test_combo_legal_skip_matrix(void)
{
    TEST("combo B: SKIP non-target for all 4 mismatch reasons");
    check_skip(REASON_COLOR_MISMATCH, CPU_REASON_COLOR_MISMATCH, "skip color");
    check_skip(REASON_SHAPE_MISMATCH, CPU_REASON_SHAPE_MISMATCH, "skip shape");
    check_skip(REASON_SIZE_NOT_EQ_10MM, CPU_REASON_SIZE_DIFF_NOT_10MM, "skip size10");
    check_skip(REASON_SIZE_OUTSIDE_5MM, CPU_REASON_SIZE_DIFF_OVER_5MM, "skip size5");
    PASS();
}

/*----- C: 合法阻止（五种理由）--------------------------------------------*/
static void check_blocked(uint8_t is_target, reason_code_t reason,
                          cpu_reason_t expect, const char *msg)
{
    round_controller_output_t out =
        make_output(1u, MATCH_ACTION_NONE, is_target, reason);
    cpu_display_result_t d;
    cpu_display_from_round_output(&out, &d);
    _check(__FILE__, __LINE__, d.valid == 1u, msg);
    _check(__FILE__, __LINE__, d.is_target == is_target, msg);
    _check(__FILE__, __LINE__, d.decision == CPU_DECISION_NONE, msg);
    _check(__FILE__, __LINE__, d.execution == CPU_EXEC_BLOCKED, msg);
    _check(__FILE__, __LINE__, d.reason == expect, msg);
}

static void test_combo_legal_blocked_matrix(void)
{
    TEST("combo C: NONE blocked for all 5 gate/abandon/timeout/observe reasons");
    /* ARM_NOT_READY 保留 is_target=1 的识别证据 */
    check_blocked(1u, REASON_ARM_NOT_READY, CPU_REASON_ARM_NOT_READY, "arm_not_ready");
    check_blocked(0u, REASON_OPERATOR_ABANDON, CPU_REASON_OPERATOR_ABANDON, "abandon");
    check_blocked(0u, REASON_STABILITY_TIMEOUT,
                  CPU_REASON_ACQUIRE_STABILITY_TIMEOUT, "stability_timeout");
    check_blocked(0u, REASON_OBSERVATION_UNKNOWN,
                  CPU_REASON_OBSERVATION_UNKNOWN, "observation_unknown");
    check_blocked(0u, REASON_TARGET_INVALID, CPU_REASON_INVALID_TARGET, "target_invalid");
    PASS();
}

/*----- D: 合法故障 ---------------------------------------------------------*/
static void test_combo_legal_fault(void)
{
    TEST("combo D: NONE + ARM_FAULT -> FAULT, keeps is_target evidence");
    round_controller_output_t out =
        make_output(1u, MATCH_ACTION_NONE, 1u, REASON_ARM_FAULT);
    cpu_display_result_t d;
    cpu_display_from_round_output(&out, &d);
    CHECK_EQ(d.valid, 1u);
    CHECK_EQ(d.is_target, 1u);
    CHECK_EQ(d.decision, CPU_DECISION_NONE);
    CHECK_EQ(d.execution, CPU_EXEC_FAULT);
    CHECK_EQ(d.reason, CPU_REASON_ARM_FAULT);
    PASS();
}

/*----- C/D 极性校验：每个阻止/故障理由只接受其真实 is_target ------------*/
/* 合法侧（good_target）→ BLOCKED；相反 is_target → 安全错误，绝不 BLOCKED。 */
static void check_blocked_polarity(reason_code_t reason, uint8_t good_target,
                                   cpu_reason_t expect, const char *msg)
{
    round_controller_output_t ok =
        make_output(1u, MATCH_ACTION_NONE, good_target, reason);
    round_controller_output_t bad =
        make_output(1u, MATCH_ACTION_NONE,
                    (uint8_t)(good_target ? 0u : 1u), reason);
    cpu_display_result_t d;

    cpu_display_from_round_output(&ok, &d);
    _check(__FILE__, __LINE__, d.valid == 1u, msg);
    _check(__FILE__, __LINE__, d.is_target == good_target, msg);
    _check(__FILE__, __LINE__, d.decision == CPU_DECISION_NONE, msg);
    _check(__FILE__, __LINE__, d.execution == CPU_EXEC_BLOCKED, msg);
    _check(__FILE__, __LINE__, d.reason == expect, msg);

    /* 预填成“像目标执行”的脏值，确认相反极性被强制清成安全错误 */
    d.valid = 1u; d.is_target = 1u; d.decision = CPU_DECISION_EXECUTE;
    d.execution = CPU_EXEC_REQUESTED; d.reason = CPU_REASON_TARGET_MATCH;
    cpu_display_from_round_output(&bad, &d);
    expect_safe_error(&d);
}

static void test_combo_blocked_polarity(void)
{
    TEST("combo C: each blocked reason accepts only its real is_target");
    check_blocked_polarity(REASON_ARM_NOT_READY, 1u,
                           CPU_REASON_ARM_NOT_READY, "arm_not_ready polarity");
    check_blocked_polarity(REASON_OPERATOR_ABANDON, 0u,
                           CPU_REASON_OPERATOR_ABANDON, "abandon polarity");
    check_blocked_polarity(REASON_STABILITY_TIMEOUT, 0u,
                           CPU_REASON_ACQUIRE_STABILITY_TIMEOUT, "stability polarity");
    check_blocked_polarity(REASON_OBSERVATION_UNKNOWN, 0u,
                           CPU_REASON_OBSERVATION_UNKNOWN, "observation polarity");
    check_blocked_polarity(REASON_TARGET_INVALID, 0u,
                           CPU_REASON_INVALID_TARGET, "target_invalid polarity");
    PASS();
}

static void test_combo_fault_polarity(void)
{
    TEST("combo D: ARM_FAULT accepts only is_target=1, else safe error");
    round_controller_output_t ok =
        make_output(1u, MATCH_ACTION_NONE, 1u, REASON_ARM_FAULT);
    round_controller_output_t bad =
        make_output(1u, MATCH_ACTION_NONE, 0u, REASON_ARM_FAULT);
    cpu_display_result_t d;

    /* 合法侧：ARM_FAULT + is_target=1 → FAULT */
    cpu_display_from_round_output(&ok, &d);
    CHECK_EQ(d.valid, 1u);
    CHECK_EQ(d.is_target, 1u);
    CHECK_EQ(d.decision, CPU_DECISION_NONE);
    CHECK_EQ(d.execution, CPU_EXEC_FAULT);
    CHECK_EQ(d.reason, CPU_REASON_ARM_FAULT);

    /* 非法侧：ARM_FAULT + is_target=0（与“只能经 GRAB 抵达”矛盾）→ 安全错误 */
    d.valid = 1u; d.is_target = 1u; d.decision = CPU_DECISION_EXECUTE;
    d.execution = CPU_EXEC_REQUESTED; d.reason = CPU_REASON_TARGET_MATCH;
    cpu_display_from_round_output(&bad, &d);
    expect_safe_error(&d);
    PASS();
}

/*----- 矛盾/非法组合矩阵 --------------------------------------------------*/
struct bad_combo {
    uint8_t action;
    uint8_t is_target;
    reason_code_t reason;
    const char *label;
};

static void test_combo_contradictions(void)
{
    static const struct bad_combo bad[] = {
        { MATCH_ACTION_GRAB, 0u, REASON_TARGET_MATCH,   "GRAB+match but is_target=0" },
        { MATCH_ACTION_GRAB, 1u, REASON_COLOR_MISMATCH, "GRAB+non-target reason" },
        { MATCH_ACTION_GRAB, 0u, REASON_COLOR_MISMATCH, "GRAB+non-target reason+t0" },
        { MATCH_ACTION_GRAB, 1u, REASON_ARM_FAULT,      "GRAB+ARM_FAULT" },
        { MATCH_ACTION_SKIP, 1u, REASON_TARGET_MATCH,   "SKIP+target reason+t1" },
        { MATCH_ACTION_SKIP, 0u, REASON_TARGET_MATCH,   "SKIP+target reason+t0" },
        { MATCH_ACTION_SKIP, 1u, REASON_COLOR_MISMATCH, "SKIP+mismatch but is_target=1" },
        { MATCH_ACTION_SKIP, 0u, REASON_ARM_NOT_READY,  "SKIP+gate reason" },
        { MATCH_ACTION_NONE, 1u, REASON_TARGET_MATCH,   "NONE+target reason" },
        { MATCH_ACTION_NONE, 0u, REASON_COLOR_MISMATCH, "NONE+skip reason" },
        { MATCH_ACTION_ERROR, 1u, REASON_TARGET_MATCH,  "ERROR action+target" },
        { (uint8_t)77, 1u, REASON_TARGET_MATCH,         "illegal action+target" },
        { MATCH_ACTION_NONE, 0u, (reason_code_t)77,     "NONE+illegal reason" },
        { MATCH_ACTION_GRAB, 1u, (reason_code_t)200,    "GRAB+illegal reason" },
        { MATCH_ACTION_SKIP, 0u, (reason_code_t)200,    "SKIP+illegal reason" },
    };
    size_t i;
    TEST("combo X: contradictory/illegal combos -> safe ERROR/FAULT, never exec/skip");
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        round_controller_output_t out =
            make_output(1u, bad[i].action, bad[i].is_target, bad[i].reason);
        cpu_display_result_t d;
        /* 预填成“像目标执行”的脏值，确认被强制清成安全错误 */
        d.valid = 1u; d.is_target = 1u; d.decision = CPU_DECISION_EXECUTE;
        d.execution = CPU_EXEC_REQUESTED; d.reason = CPU_REASON_TARGET_MATCH;
        cpu_display_from_round_output(&out, &d);
        expect_safe_error(&d);
    }
    PASS();
}

/*----- result_valid=0 + 垃圾字段 ------------------------------------------*/
static void test_result_invalid_ignores_garbage(void)
{
    TEST("display: result_valid=0 with garbage fields -> empty safe result");
    /* 携带陈旧的 GRAB/is_target/TARGET_MATCH 以及越界脏值，均须清成空结果 */
    round_controller_output_t a =
        make_output(0u, MATCH_ACTION_GRAB, 1u, REASON_TARGET_MATCH);
    round_controller_output_t b =
        make_output(0u, (uint8_t)0xAB, 0xFFu, (reason_code_t)0xDEAD);
    cpu_display_result_t d;
    cpu_display_from_round_output(&a, &d);
    expect_empty(&d);
    cpu_display_from_round_output(&b, &d);
    expect_empty(&d);
    PASS();
}

static void test_null_input_is_empty(void)
{
    TEST("display: NULL input -> empty safe result, never a target");
    cpu_display_result_t d;
    d.valid = 1u; d.is_target = 1u; d.decision = CPU_DECISION_EXECUTE;
    d.execution = CPU_EXEC_REQUESTED; d.reason = CPU_REASON_TARGET_MATCH;
    cpu_display_from_round_output(0, &d);
    expect_empty(&d);
    PASS();
}

static void test_null_output_does_not_crash(void)
{
    TEST("display: NULL output pointer is a safe no-op");
    round_controller_output_t out =
        make_output(1u, MATCH_ACTION_GRAB, 1u, REASON_TARGET_MATCH);
    cpu_display_from_round_output(&out, 0); /* 不得崩溃 */
    cpu_display_from_round_output(0, 0);    /* 双 NULL 也不得崩溃 */
    CHECK(1);
    PASS();
}

/*==========================================================================
 *  文本助手（含非法值兜底 → INVALID_INTERNAL，不返回 NONE）
 *==========================================================================*/
static void test_text_helpers(void)
{
    TEST("text: reason/decision/execution helpers incl. illegal fallback");
    CHECK(strcmp(cpu_reason_text(CPU_REASON_TARGET_MATCH), "TARGET_MATCH") == 0);
    CHECK(strcmp(cpu_reason_text(CPU_REASON_SIZE_DIFF_NOT_10MM),
                 "SIZE_DIFF_NOT_10MM") == 0);
    CHECK(strcmp(cpu_reason_text(CPU_REASON_SIZE_DIFF_OVER_5MM),
                 "SIZE_DIFF_OVER_5MM") == 0);
    CHECK(strcmp(cpu_reason_text(CPU_REASON_OBSERVATION_UNSTABLE),
                 "OBSERVATION_UNSTABLE") == 0);
    CHECK(strcmp(cpu_reason_text(CPU_REASON_ACQUIRE_STABILITY_TIMEOUT),
                 "ACQUIRE_STABILITY_TIMEOUT") == 0);
    CHECK(strcmp(cpu_reason_text(CPU_REASON_INVALID_INTERNAL),
                 "INVALID_INTERNAL") == 0);
    /* 非法值绝不返回 NONE */
    CHECK(strcmp(cpu_reason_text((cpu_reason_t)123), "INVALID_INTERNAL") == 0);
    CHECK(strcmp(cpu_decision_text(CPU_DECISION_EXECUTE), "EXECUTE") == 0);
    CHECK(strcmp(cpu_decision_text((cpu_decision_t)123), "INVALID_INTERNAL") == 0);
    CHECK(strcmp(cpu_decision_text((cpu_decision_t)123), "NONE") != 0);
    CHECK(strcmp(cpu_execution_text(CPU_EXEC_BLOCKED), "BLOCKED") == 0);
    CHECK(strcmp(cpu_execution_text((cpu_execution_t)123), "INVALID_INTERNAL") == 0);
    CHECK(strcmp(cpu_execution_text((cpu_execution_t)123), "NONE") != 0);
    PASS();
}

/*==========================================================================
 *  端到端：用真实 round_controller 驱动，确认与手工构造一致
 *==========================================================================*/
static round_controller_input_t base_input(uint32_t now_ms)
{
    round_controller_input_t in;
    memset(&in, 0, sizeof(in));
    in.now_ms = now_ms;
    return in;
}

static void send_event(round_controller_t *rc, round_controller_output_t *out,
                       uint32_t now_ms, uint16_t seq, round_event_t event)
{
    round_controller_input_t in = base_input(now_ms);
    in.event_valid = 1;
    in.event_seq = seq;
    in.event = event;
    round_controller_tick(rc, &in, out);
}

static task_match_result_t make_match(uint8_t action, uint8_t is_target,
                                      reason_code_t reason)
{
    task_match_result_t m;
    memset(&m, 0, sizeof(m));
    m.action = action;
    m.is_target = is_target;
    m.reason = reason;
    m.mode = TASK_MODE_1;
    return m;
}

/* 从 CONFIG 推进到 EXECUTE_OR_SKIP 之后的 ROUND_DONE/WAIT，喂入一帧观测。
 * arm_enabled 决定目标轮是否进入 WAIT_ARM_DONE。 */
static void drive_one_observation(round_controller_t *rc,
                                  round_controller_output_t *out,
                                  uint8_t arm_enabled,
                                  const task_match_result_t *match)
{
    round_controller_input_t in;

    round_controller_init(rc, 0, 0);
    send_event(rc, out, 1, 1, ROUND_EVENT_APPLY_CONFIG);
    send_event(rc, out, 2, 2, ROUND_EVENT_PLACE_CONFIRM);

    in = base_input(100);
    in.observation_valid = 1;
    in.arm_enabled = arm_enabled;
    in.match = *match;
    round_controller_tick(rc, &in, out);   /* ACQUIRE -> LATCH_RECOGNITION */
    in = base_input(110);
    in.arm_enabled = arm_enabled;
    round_controller_tick(rc, &in, out);   /* -> LATCH_DECISION */
    in.now_ms = 120;
    round_controller_tick(rc, &in, out);   /* -> EXECUTE_OR_SKIP */
    in.now_ms = 130;
    round_controller_tick(rc, &in, out);   /* -> WAIT_ARM_DONE or ROUND_DONE */
}

static void test_fsm_skip_maps_to_skipped(void)
{
    TEST("fsm: real SKIP round -> SKIPPED_NON_TARGET / SKIP");
    round_controller_t rc;
    round_controller_output_t out;
    task_match_result_t m = make_match(MATCH_ACTION_SKIP, 0, REASON_COLOR_MISMATCH);
    cpu_display_result_t d;

    drive_one_observation(&rc, &out, 0u, &m);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    cpu_display_from_round_output(&out, &d);
    CHECK_EQ(d.valid, 1u);
    CHECK_EQ(d.is_target, 0u);
    CHECK_EQ(d.decision, CPU_DECISION_SKIP);
    CHECK_EQ(d.execution, CPU_EXEC_SKIPPED_NON_TARGET);
    CHECK_EQ(d.reason, CPU_REASON_COLOR_MISMATCH);
    PASS();
}

static void test_fsm_arm_disabled_maps_to_blocked(void)
{
    TEST("fsm: real target + arm disabled -> BLOCKED / ARM_NOT_READY");
    round_controller_t rc;
    round_controller_output_t out;
    task_match_result_t m = make_match(MATCH_ACTION_GRAB, 1, REASON_TARGET_MATCH);
    cpu_display_result_t d;

    drive_one_observation(&rc, &out, 0u, &m);   /* arm disabled */
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    cpu_display_from_round_output(&out, &d);
    CHECK_EQ(d.valid, 1u);
    CHECK_EQ(d.is_target, 1u);
    CHECK_EQ(d.decision, CPU_DECISION_NONE);
    CHECK_EQ(d.execution, CPU_EXEC_BLOCKED);
    CHECK_EQ(d.reason, CPU_REASON_ARM_NOT_READY);
    PASS();
}

static void test_fsm_arm_enabled_maps_to_requested(void)
{
    TEST("fsm: real target + arm enabled -> REQUESTED / EXECUTE");
    round_controller_t rc;
    round_controller_output_t out;
    task_match_result_t m = make_match(MATCH_ACTION_GRAB, 1, REASON_TARGET_MATCH);
    cpu_display_result_t d;

    drive_one_observation(&rc, &out, 1u, &m);   /* arm enabled */
    CHECK_EQ(out.state, ROUND_STATE_WAIT_ARM_DONE);
    CHECK_EQ(out.request_arm_grab, 1u);
    cpu_display_from_round_output(&out, &d);
    CHECK_EQ(d.valid, 1u);
    CHECK_EQ(d.is_target, 1u);
    CHECK_EQ(d.decision, CPU_DECISION_EXECUTE);
    CHECK_EQ(d.execution, CPU_EXEC_REQUESTED);
    CHECK_EQ(d.reason, CPU_REASON_TARGET_MATCH);
    PASS();
}

static void test_fsm_arm_fault_maps_to_fault(void)
{
    TEST("fsm: real arm fault -> FAULT / ARM_FAULT");
    round_controller_t rc;
    round_controller_output_t out;
    round_controller_input_t in;
    task_match_result_t m = make_match(MATCH_ACTION_GRAB, 1, REASON_TARGET_MATCH);
    cpu_display_result_t d;

    drive_one_observation(&rc, &out, 1u, &m);   /* -> WAIT_ARM_DONE */
    CHECK_EQ(out.state, ROUND_STATE_WAIT_ARM_DONE);
    in = base_input(140);
    in.arm_fault = 1;
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_ARM_FAULT);
    cpu_display_from_round_output(&out, &d);
    CHECK_EQ(d.valid, 1u);
    CHECK_EQ(d.is_target, 1u);
    CHECK_EQ(d.execution, CPU_EXEC_FAULT);
    CHECK_EQ(d.reason, CPU_REASON_ARM_FAULT);
    PASS();
}

static void test_fsm_acquire_timeout_maps_to_blocked(void)
{
    TEST("fsm: real acquire timeout -> BLOCKED / ACQUIRE_STABILITY_TIMEOUT");
    round_controller_t rc;
    round_controller_output_t out;
    round_controller_input_t in;
    cpu_display_result_t d;

    round_controller_init(&rc, 0, 0);
    send_event(&rc, &out, 1, 1, ROUND_EVENT_APPLY_CONFIG);
    send_event(&rc, &out, 2, 2, ROUND_EVENT_PLACE_CONFIRM);
    /* 不喂观测，直接越过 acquire 截止时间（默认 3000ms） */
    in = base_input(100000);
    round_controller_tick(&rc, &in, &out);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(out.reason, REASON_STABILITY_TIMEOUT);
    cpu_display_from_round_output(&out, &d);
    CHECK_EQ(d.valid, 1u);
    CHECK_EQ(d.is_target, 0u);
    CHECK_EQ(d.decision, CPU_DECISION_NONE);
    CHECK_EQ(d.execution, CPU_EXEC_BLOCKED);
    CHECK_EQ(d.reason, CPU_REASON_ACQUIRE_STABILITY_TIMEOUT);
    PASS();
}

static void test_fsm_operator_abandon_maps_to_blocked(void)
{
    TEST("fsm: real operator abandon -> BLOCKED / OPERATOR_ABANDON");
    round_controller_t rc;
    round_controller_output_t out;
    cpu_display_result_t d;

    round_controller_init(&rc, 0, 0);
    send_event(&rc, &out, 1, 1, ROUND_EVENT_APPLY_CONFIG);
    send_event(&rc, &out, 2, 2, ROUND_EVENT_PLACE_CONFIRM);
    send_event(&rc, &out, 3, 3, ROUND_EVENT_ABANDON_ROUND);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(out.reason, REASON_OPERATOR_ABANDON);
    cpu_display_from_round_output(&out, &d);
    CHECK_EQ(d.valid, 1u);
    CHECK_EQ(d.decision, CPU_DECISION_NONE);
    CHECK_EQ(d.execution, CPU_EXEC_BLOCKED);
    CHECK_EQ(d.reason, CPU_REASON_OPERATOR_ABANDON);
    PASS();
}

int main(void)
{
    printf("\n=== cpu_result_semantics unit tests ===\n\n");

    test_reason_code_mapping_legal();
    test_reason_code_mapping_illegal();
    test_competition_reason_mapping_legal();
    test_competition_size_relation_mode_split();
    test_competition_reason_mapping_illegal();

    printf("\n[combo] (action + reason + is_target) validation matrix\n");
    test_combo_legal_target_execute();
    test_combo_legal_skip_matrix();
    test_combo_legal_blocked_matrix();
    test_combo_legal_fault();
    test_combo_blocked_polarity();
    test_combo_fault_polarity();
    test_combo_contradictions();
    test_result_invalid_ignores_garbage();
    test_null_input_is_empty();
    test_null_output_does_not_crash();
    test_text_helpers();

    printf("\n[fsm] end-to-end via real round_controller\n");
    test_fsm_skip_maps_to_skipped();
    test_fsm_arm_disabled_maps_to_blocked();
    test_fsm_arm_enabled_maps_to_requested();
    test_fsm_arm_fault_maps_to_fault();
    test_fsm_acquire_timeout_maps_to_blocked();
    test_fsm_operator_abandon_maps_to_blocked();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           _test_count - _test_failures, _test_count, _test_failures);
    return _test_failures ? 1 : 0;
}
