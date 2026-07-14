/*==========================================================================
 *  test_main_loop_arm_disabled.c  —  ARM_DISABLED Host/Mock 测试
 *
 *  链接 main_loop_adapter.c（与 main.c 同一份实现），保证测试真实覆盖
 *  所有 main.c 适配代码（P1-1）。
 *
 *  覆盖：
 *    - 非目标 SKIP（color/shape/size 三类真实 reason）                (P1-3)
 *    - 目标 ARM_NOT_READY（arm_enabled=0）
 *    - ABANDON（不冒充 REMOVE 解锁 matcher）                         (P1-2)
 *    - Acquire 超时
 *    - 重复/倒退/65535→0回绕/delta=32768 歧义 event_seq
 *    - REMOVE_CONFIRM 唯一触发 matcher 解锁（return 1）              (P1-2)
 *    - SESSION_RESET 触发重新初始化（return -1）                      (P1-2)
 *    - 5 轮序列 + NULL 安全路径
 *
 *  不调用 arm_controller、UART2、myCobot transport。不连接/驱动机械臂。
 *  不改 round_controller.c/.h 行为。
 *==========================================================================*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "round_controller.h"
#include "cpu_result_semantics.h"
#include "cpu_result_semantics_adapters.h"
#include "main_loop_adapter.h"
#include "task_matcher.h"
#include "vision_classifier.h"

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
#define CHECK_NEQ(a, b) _check(__FILE__, __LINE__, ((a) != (b)), #a " != " #b)
#define TEST(name) \
    printf("  %-64s", name " "); fflush(stdout); _test_start = _test_failures
#define PASS() \
    do { int d = _test_failures - _test_start; \
         if (d == 0) printf("PASS\n"); else printf("%d FAILED\n", d); } while (0)

/*==========================================================================
 *  测试夹具 — 调用 main_loop_arm_disabled_step()（与 main.c 同一实现）
 *==========================================================================*/

/* 快捷：初始化 RC + 发送 APPLY_CONFIG */
static round_controller_output_t adapter_init_apply(round_controller_t *rc,
                                                     uint16_t *seq,
                                                     uint32_t now_ms)
{
    round_controller_config_t cfg;
    round_controller_output_t out;
    cpu_display_result_t d;
    memset(&cfg, 0, sizeof(cfg));
    cfg.acquire_timeout_ms = 3000u;
    cfg.arm_timeout_ms     = 15000u;
    round_controller_init(rc, &cfg, now_ms);
    *seq = 1;  /* step() 做 (*seq)++ → seq 1 被 APPLY_CONFIG 消费，下一可用 seq=2 */
    main_loop_arm_disabled_step(rc, seq, now_ms,
                                0, ROUND_EVENT_APPLY_CONFIG, 1, &d, &out);
    return out;
}

/* 快捷：发送 PLACE_CONFIRM */
static round_controller_output_t adapter_place(round_controller_t *rc,
                                                uint16_t *seq,
                                                uint32_t now_ms, uint16_t ev_seq)
{
    cpu_display_result_t d;
    round_controller_output_t out;
    uint16_t saved = *seq;
    *seq = ev_seq;
    main_loop_arm_disabled_step(rc, seq, now_ms,
                                0, ROUND_EVENT_PLACE_CONFIRM, 1, &d, &out);
    *seq = saved > ev_seq ? saved : ev_seq + 1;
    return out;
}

/* 快捷：喂观测 + tick（无事件） */
static int adapter_observe(round_controller_t *rc, uint16_t *seq,
                            uint32_t now_ms,
                            const task_match_result_t *match,
                            cpu_display_result_t *d, round_controller_output_t *out)
{
    return main_loop_arm_disabled_step(rc, seq, now_ms,
                match, ROUND_EVENT_NONE, 0, d, out);
}

/* 快捷：空 tick（无事件、无观测），推进状态机 */
static int adapter_idle_tick(round_controller_t *rc, uint16_t *seq,
                              uint32_t now_ms,
                              cpu_display_result_t *d, round_controller_output_t *out)
{
    return main_loop_arm_disabled_step(rc, seq, now_ms,
                0, ROUND_EVENT_NONE, 0, d, out);
}

/* 快捷：发送事件 + tick */
static int adapter_event(round_controller_t *rc, uint16_t *seq,
                          uint32_t now_ms, round_event_t ev,
                          cpu_display_result_t *d, round_controller_output_t *out)
{
    return main_loop_arm_disabled_step(rc, seq, now_ms,
                0, ev, 1, d, out);
}

/*==========================================================================
 *  T1: 非目标 SKIP（COLOR_MISMATCH reason，P1-3）
 *==========================================================================*/
static void test_skip_color_mismatch(void)
{
    TEST("T1: SKIP + COLOR_MISMATCH reason (not hardcoded)");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    task_match_result_t m;
    uint32_t t = 1000;

    adapter_init_apply(&rc, &seq, t);
    t += 10; adapter_place(&rc, &seq, t, 2);

    /* 观察：SKIP + COLOR_MISMATCH */
    memset(&m, 0, sizeof(m));
    m.action = MATCH_ACTION_SKIP; m.is_target = 0;
    m.reason = REASON_COLOR_MISMATCH; m.mode = TASK_MODE_1;
    t += 10; adapter_observe(&rc, &seq, t, &m, &d, &out);

    /* 推进到 DONE */
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);

    CHECK_EQ(d.valid, 1u);
    CHECK_EQ(d.decision, CPU_DECISION_SKIP);
    CHECK_EQ(d.execution, CPU_EXEC_SKIPPED_NON_TARGET);
    CHECK_EQ(d.reason, CPU_REASON_COLOR_MISMATCH);  /* 真实 reason，非硬编码 */
    PASS();
}

/*==========================================================================
 *  T2: 非目标 SKIP（SHAPE_MISMATCH reason，P1-3）
 *==========================================================================*/
static void test_skip_shape_mismatch(void)
{
    TEST("T2: SKIP + SHAPE_MISMATCH reason");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    task_match_result_t m;
    uint32_t t = 2000;

    adapter_init_apply(&rc, &seq, t);
    t += 10; adapter_place(&rc, &seq, t, 2);

    memset(&m, 0, sizeof(m));
    m.action = MATCH_ACTION_SKIP; m.is_target = 0;
    m.reason = REASON_SHAPE_MISMATCH; m.mode = TASK_MODE_2;
    t += 10; adapter_observe(&rc, &seq, t, &m, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(d.reason, CPU_REASON_SHAPE_MISMATCH);
    PASS();
}

/*==========================================================================
 *  T3: 非目标 SKIP（SIZE_DIFF_NOT_10MM reason，P1-3）
 *==========================================================================*/
static void test_skip_size_not_10mm(void)
{
    TEST("T3: SKIP + SIZE_DIFF_NOT_10MM reason (Task3)");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    task_match_result_t m;
    uint32_t t = 3000;

    adapter_init_apply(&rc, &seq, t);
    t += 10; adapter_place(&rc, &seq, t, 2);

    memset(&m, 0, sizeof(m));
    m.action = MATCH_ACTION_SKIP; m.is_target = 0;
    m.reason = REASON_SIZE_NOT_EQ_10MM; m.mode = TASK_MODE_3;
    t += 10; adapter_observe(&rc, &seq, t, &m, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(d.reason, CPU_REASON_SIZE_DIFF_NOT_10MM);
    PASS();
}

/*==========================================================================
 *  T4: 非目标 SKIP（SIZE_DIFF_OVER_5MM reason，P1-3）
 *==========================================================================*/
static void test_skip_size_over_5mm(void)
{
    TEST("T4: SKIP + SIZE_DIFF_OVER_5MM reason (Task4)");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    task_match_result_t m;
    uint32_t t = 4000;

    adapter_init_apply(&rc, &seq, t);
    t += 10; adapter_place(&rc, &seq, t, 2);

    memset(&m, 0, sizeof(m));
    m.action = MATCH_ACTION_SKIP; m.is_target = 0;
    m.reason = REASON_SIZE_OUTSIDE_5MM; m.mode = TASK_MODE_4;
    t += 10; adapter_observe(&rc, &seq, t, &m, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(d.reason, CPU_REASON_SIZE_DIFF_OVER_5MM);
    PASS();
}

/*==========================================================================
 *  T5: task_matcher 实际产出真实 reason（验证 _return_match）
 *==========================================================================*/
static void test_task_matcher_last_match_reasons(void)
{
    TEST("T5: task_matcher_get_last_match() returns real reasons");
    task_target_t tgt;
    vision_result_t obs;
    task_match_result_t m;

    task_matcher_init();

    /* color mismatch */
    memset(&tgt, 0, sizeof(tgt));
    tgt.target_color = COLOR_RED; tgt.target_shape = SHAPE_CUBE;
    tgt.task_mode = TASK_MODE_1;
    task_matcher_set_target_ex(&tgt);
    memset(&obs, 0, sizeof(obs));
    obs.color_id = COLOR_BLUE; obs.shape_id = SHAPE_CUBE;
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    task_matcher_get_last_match(&m);
    CHECK_EQ(m.reason, REASON_COLOR_MISMATCH);

    /* shape mismatch */
    task_matcher_next_round();
    memset(&tgt, 0, sizeof(tgt));
    tgt.target_color = COLOR_RED; tgt.target_shape = SHAPE_CUBE;
    tgt.task_mode = TASK_MODE_2;
    task_matcher_set_target_ex(&tgt);
    obs.color_id = COLOR_RED; obs.shape_id = SHAPE_CYLINDER;
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    task_matcher_get_last_match(&m);
    CHECK_EQ(m.reason, REASON_SHAPE_MISMATCH);

    /* size mismatch (Task3) */
    task_matcher_next_round();
    memset(&tgt, 0, sizeof(tgt));
    tgt.target_color = COLOR_RED; tgt.target_shape = SHAPE_CUBE;
    tgt.reference_size_cm_x10 = 20; tgt.task_mode = TASK_MODE_3;
    task_matcher_set_target_ex(&tgt);
    obs.color_id = COLOR_RED; obs.shape_id = SHAPE_CUBE;
    obs.size_cm_x10 = 15;  /* |15-20|=5, !=10 → SKIP */
    CHECK_EQ(task_matcher_evaluate(&obs, 0, 0), MATCH_ACTION_SKIP);
    task_matcher_get_last_match(&m);
    CHECK_EQ(m.reason, REASON_SIZE_NOT_EQ_10MM);

    /* GRAB → TARGET_MATCH + is_target=1 */
    task_matcher_next_round();
    memset(&tgt, 0, sizeof(tgt));
    tgt.target_color = COLOR_RED; tgt.target_shape = SHAPE_CUBE;
    tgt.task_mode = TASK_MODE_1;
    task_matcher_set_target_ex(&tgt);
    obs.color_id = COLOR_RED; obs.shape_id = SHAPE_CUBE;
    CHECK_EQ(task_matcher_evaluate(&obs, 10, 20), MATCH_ACTION_GRAB);
    task_matcher_get_last_match(&m);
    CHECK_EQ(m.reason, REASON_TARGET_MATCH);
    CHECK_EQ(m.is_target, 1u);
    CHECK_EQ(m.action, MATCH_ACTION_GRAB);
    PASS();
}

/*==========================================================================
 *  T6: 目标轮 ARM_DISABLED → BLOCKED/ARM_NOT_READY
 *==========================================================================*/
static void test_target_arm_not_ready(void)
{
    TEST("T6: target + arm_enabled=0 -> BLOCKED/ARM_NOT_READY");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    task_match_result_t m;
    int ret; uint32_t t = 5000;

    adapter_init_apply(&rc, &seq, t);
    t += 10; adapter_place(&rc, &seq, t, 2);

    memset(&m, 0, sizeof(m));
    m.action = MATCH_ACTION_GRAB; m.is_target = 1;
    m.reason = REASON_TARGET_MATCH; m.mode = TASK_MODE_1;
    t += 10; adapter_observe(&rc, &seq, t, &m, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    ret = adapter_idle_tick(&rc, &seq, t, &d, &out);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(ret, 0);  /* 非 REMOVE → 不触发解锁 */

    CHECK_EQ(out.result_valid, 1u);
    CHECK_EQ(out.decision_action, MATCH_ACTION_NONE);
    CHECK_EQ(out.is_target, 1u);
    CHECK_EQ(out.reason, REASON_ARM_NOT_READY);
    CHECK_EQ(out.request_arm_grab, 0u);

    CHECK_EQ(d.valid, 1u);
    CHECK_EQ(d.is_target, 1u);
    CHECK_EQ(d.decision, CPU_DECISION_NONE);
    CHECK_EQ(d.execution, CPU_EXEC_BLOCKED);
    CHECK_EQ(d.reason, CPU_REASON_ARM_NOT_READY);
    CHECK_NEQ(d.execution, CPU_EXEC_REQUESTED);
    PASS();
}

/*==========================================================================
 *  T7: ABANDON 不触发 matcher 解锁（P1-2）
 *==========================================================================*/
static void test_abandon_no_unlock(void)
{
    TEST("T7: ABANDON does NOT trigger matcher unlock (P1-2)");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    int ret; uint32_t t = 6000;

    adapter_init_apply(&rc, &seq, t);
    t += 10; adapter_place(&rc, &seq, t, 2);
    /* ABANDON — 不喂观测 */
    t += 10; ret = adapter_event(&rc, &seq, t, ROUND_EVENT_ABANDON_ROUND, &d, &out);

    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(out.reason, REASON_OPERATOR_ABANDON);
    CHECK_EQ(d.execution, CPU_EXEC_BLOCKED);
    CHECK_EQ(d.reason, CPU_REASON_OPERATOR_ABANDON);

    /* 关键断言：ABANDON 返回 0，不触发 task_matcher_next_round() */
    CHECK_EQ(ret, 0);
    CHECK_NEQ(ret, 1);   /* 绝不能假装成 REMOVE */
    PASS();
}

/*==========================================================================
 *  T8: 只有 REMOVE_CONFIRM 返回 1 触发 matcher 解锁（P1-2）
 *==========================================================================*/
static void test_remove_triggers_unlock(void)
{
    TEST("T8: REMOVE_CONFIRM -> accepted returns 1 (matcher unlock)");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    task_match_result_t m;
    int ret; uint32_t t = 7000;

    /* 完成一轮（SKIP）→ ROUND_DONE */
    adapter_init_apply(&rc, &seq, t);
    t += 10; adapter_place(&rc, &seq, t, 2);
    memset(&m, 0, sizeof(m));
    m.action = MATCH_ACTION_SKIP; m.is_target = 0;
    m.reason = REASON_COLOR_MISMATCH; m.mode = TASK_MODE_1;
    t += 10; adapter_observe(&rc, &seq, t, &m, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);

    /* REMOVE_CONFIRM → 应返回 1 */
    t += 10; ret = adapter_event(&rc, &seq, t, ROUND_EVENT_REMOVE_CONFIRM, &d, &out);
    CHECK_EQ(ret, 1);  /* ← P1-2: 唯一触发解锁的事件 */
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);
    PASS();
}

/*==========================================================================
 *  T9: SESSION_RESET 返回 -1 触发重新初始化（P1-2）
 *==========================================================================*/
static void test_session_reset_reinit(void)
{
    TEST("T9: SESSION_RESET returns -1 (re-init signal)");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    int ret; uint32_t t = 8000;

    adapter_init_apply(&rc, &seq, t);
    /* 在 WAIT_PLACE_CONFIRM 状态发 SESSION_RESET */
    t += 10; ret = adapter_event(&rc, &seq, t, ROUND_EVENT_SESSION_RESET, &d, &out);
    CHECK_EQ(ret, -1);  /* ← P1-2: SESSION_RESET 返回 -1 */

    /* 确认 RC 被复位到 CONFIG，round_seq=0，result_valid=0 */
    CHECK_EQ(out.state, ROUND_STATE_CONFIG);
    CHECK_EQ(out.round_seq, 0u);
    CHECK_EQ(out.result_valid, 0u);
    PASS();
}

/*==========================================================================
 *  T10: Acquire 超时
 *==========================================================================*/
static void test_acquire_timeout(void)
{
    TEST("T10: acquire timeout -> BLOCKED / ACQUIRE_STABILITY_TIMEOUT");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    uint32_t t = 9000;

    adapter_init_apply(&rc, &seq, t);
    t += 10; adapter_place(&rc, &seq, t, 2);
    /* deadline = 9010 + 3000 = 12010。跳到 12020 触发超时 */
    t = 12020;
    adapter_idle_tick(&rc, &seq, t, &d, &out);

    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    CHECK_EQ(out.reason, REASON_STABILITY_TIMEOUT);
    CHECK_EQ(d.execution, CPU_EXEC_BLOCKED);
    CHECK_EQ(d.reason, CPU_REASON_ACQUIRE_STABILITY_TIMEOUT);
    PASS();
}

/*==========================================================================
 *  T11–T14: 16-bit event_seq 契约
 *==========================================================================*/
static void test_event_seq_duplicate(void)
{
    TEST("T11: duplicate event_seq (delta=0) rejected");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    uint32_t t = 10000;

    out = adapter_init_apply(&rc, &seq, t);  /* seq 1 consumed */
    CHECK_EQ(out.event_ack_status, ROUND_EVENT_ACK_ACCEPTED);

    /* 重复 APPLY_CONFIG(seq=1) — delta=0 */
    t += 10;
    uint16_t saved = seq; seq = 1;
    main_loop_arm_disabled_step(&rc, &seq, t,
                0, ROUND_EVENT_APPLY_CONFIG, 1, &d, &out);
    seq = saved;
    CHECK_EQ(out.event_ack_valid, 0u);
    CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);
    PASS();
}

static void test_event_seq_backward(void)
{
    TEST("T12: backward event_seq (delta>32767) rejected");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    uint32_t t = 11000;

    adapter_init_apply(&rc, &seq, t);  /* last=1 */
    /* PLACE(seq=10) → accepted, last=10 */
    t += 10; out = adapter_place(&rc, &seq, t, 10);
    CHECK_EQ(out.event_ack_status, ROUND_EVENT_ACK_ACCEPTED);

    /* PLACE(seq=5), delta=65531 → rejected */
    t += 10;
    uint16_t saved = seq; seq = 5;
    main_loop_arm_disabled_step(&rc, &seq, t,
                0, ROUND_EVENT_PLACE_CONFIRM, 1, &d, &out);
    seq = saved;
    CHECK_EQ(out.event_ack_valid, 0u);
    CHECK_EQ(out.state, ROUND_STATE_ACQUIRE_STABLE);  /* unchanged */
    PASS();
}

static void test_event_seq_rollover(void)
{
    TEST("T13: 65535->0 rollover (delta=1) accepted");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    round_controller_config_t cfg;
    uint32_t t = 12000;

    /* 手工 init：APPLY_CONFIG(seq=65534) */
    memset(&cfg, 0, sizeof(cfg));
    round_controller_init(&rc, &cfg, t);
    seq = 65534;
    main_loop_arm_disabled_step(&rc, &seq, t,
                0, ROUND_EVENT_APPLY_CONFIG, 1, &d, &out);
    CHECK_EQ(out.event_ack_status, ROUND_EVENT_ACK_ACCEPTED);

    /* PLACE(seq=65535), delta=1 → accepted */
    t += 10; seq = 65535;
    main_loop_arm_disabled_step(&rc, &seq, t,
                0, ROUND_EVENT_PLACE_CONFIRM, 1, &d, &out);
    CHECK_EQ(out.event_ack_status, ROUND_EVENT_ACK_ACCEPTED);
    CHECK_EQ(out.state, ROUND_STATE_ACQUIRE_STABLE);

    /* ABANDON(seq=0), delta=1（回绕）→ accepted */
    t += 10; seq = 0;
    main_loop_arm_disabled_step(&rc, &seq, t,
                0, ROUND_EVENT_ABANDON_ROUND, 1, &d, &out);
    CHECK_EQ(out.event_ack_status, ROUND_EVENT_ACK_ACCEPTED);
    CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
    PASS();
}

static void test_event_seq_delta_32768(void)
{
    TEST("T14: delta=32768 (half-range ambiguity) rejected");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    uint32_t t = 13000;

    adapter_init_apply(&rc, &seq, t);  /* last=1 */
    /* PLACE(seq=2) → last=2 */
    t += 10; out = adapter_place(&rc, &seq, t, 2);
    CHECK_EQ(out.event_ack_status, ROUND_EVENT_ACK_ACCEPTED);

    /* ABANDON(seq=2+32768=32770), delta=32768 → rejected */
    t += 10; seq = (uint16_t)(2 + 32768);
    main_loop_arm_disabled_step(&rc, &seq, t,
                0, ROUND_EVENT_ABANDON_ROUND, 1, &d, &out);
    CHECK_EQ(out.event_ack_valid, 0u);
    CHECK_EQ(out.state, ROUND_STATE_ACQUIRE_STABLE);  /* unchanged */
    PASS();
}

/*==========================================================================
 *  T15: 5 轮序列
 *==========================================================================*/
static void test_5_round_sequence(void)
{
    TEST("T15: 5-round ARM_DISABLED sequence (seq + independent)");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    task_match_result_t m;
    uint32_t t = 14000;
    int r, ret;

    adapter_init_apply(&rc, &seq, t);

    for (r = 0; r < 5; r++) {
        t += 10; adapter_place(&rc, &seq, t, (uint16_t)((r+1)*10));

        if (r % 2 == 0) {
            memset(&m, 0, sizeof(m));
            m.action = MATCH_ACTION_SKIP; m.is_target = 0;
            m.reason = REASON_SHAPE_MISMATCH; m.mode = TASK_MODE_2;
        } else {
            memset(&m, 0, sizeof(m));
            m.action = MATCH_ACTION_GRAB; m.is_target = 1;
            m.reason = REASON_TARGET_MATCH; m.mode = TASK_MODE_1;
        }
        t += 10; adapter_observe(&rc, &seq, t, &m, &d, &out);
        t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
        t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
        t += 10; adapter_idle_tick(&rc, &seq, t, &d, &out);
        CHECK_EQ(out.state, ROUND_STATE_ROUND_DONE);
        CHECK_EQ(out.result_valid, 1u);
        CHECK_EQ(out.round_seq, (uint16_t)r);

        CHECK_EQ(d.valid, 1u);
        if (r % 2 == 0) {
            CHECK_EQ(d.execution, CPU_EXEC_SKIPPED_NON_TARGET);
        } else {
            CHECK_EQ(d.execution, CPU_EXEC_BLOCKED);
            CHECK_EQ(d.reason, CPU_REASON_ARM_NOT_READY);
        }

        /* REMOVE → 返回 1，解锁 matcher */
        t += 10;
        ret = adapter_event(&rc, &seq, t, ROUND_EVENT_REMOVE_CONFIRM, &d, &out);
        CHECK_EQ(ret, 1);
        CHECK_EQ(out.state, ROUND_STATE_WAIT_PLACE_CONFIRM);
        CHECK_EQ(out.round_seq, (uint16_t)(r + 1));
        CHECK_EQ(out.result_valid, 0u);
    }
    PASS();
}

/*==========================================================================
 *  T16: NULL + result_valid=0 安全路径
 *==========================================================================*/
static void test_null_safety(void)
{
    TEST("T16: NULL safety + result_valid=0 -> empty projection");
    round_controller_t rc; uint16_t seq;
    round_controller_output_t out; cpu_display_result_t d;
    uint32_t t = 15000;

    /* adapter 接受 NULL rc_out */
    adapter_init_apply(&rc, &seq, t);
    t += 10;
    main_loop_arm_disabled_step(&rc, &seq, t,
                0, ROUND_EVENT_ABANDON_ROUND, 1, &d, 0);
    CHECK(1);  /* 不崩溃 */

    /* result_valid=0 → 安全空结果 */
    memset(&out, 0, sizeof(out));
    out.state = ROUND_STATE_WAIT_PLACE_CONFIRM;
    out.result_valid = 0;
    out.decision_action = 0xAB; out.is_target = 0xFF;
    out.reason = (reason_code_t)0xDEAD;
    cpu_display_from_round_output(&out, &d);
    CHECK_EQ(d.valid, 0u);
    CHECK_EQ(d.decision, CPU_DECISION_NONE);
    CHECK_EQ(d.execution, CPU_EXEC_NONE);

    /* NULL display → 不崩溃 */
    {
        task_match_result_t m; memset(&m, 0, sizeof(m));
        m.action = MATCH_ACTION_GRAB; m.is_target = 1;
        m.reason = REASON_TARGET_MATCH; m.mode = TASK_MODE_1;
        main_loop_arm_disabled_step(&rc, &seq, t,
                    &m, ROUND_EVENT_NONE, 0, 0, &out);
        CHECK(1);
    }
    PASS();
}

int main(void)
{
    printf("\n=== main_loop ARM_DISABLED Host/Mock tests (via main_loop_adapter.c) ===\n\n");

    printf("[SKIP reasons — P1-3: real reason, not hardcoded]\n");
    test_skip_color_mismatch();
    test_skip_shape_mismatch();
    test_skip_size_not_10mm();
    test_skip_size_over_5mm();
    test_task_matcher_last_match_reasons();

    printf("\n[round types]\n");
    test_target_arm_not_ready();

    printf("\n[P1-2: matcher unlock — only REMOVE triggers]\n");
    test_abandon_no_unlock();
    test_remove_triggers_unlock();
    test_session_reset_reinit();

    printf("\n[timeout]\n");
    test_acquire_timeout();

    printf("\n[event_seq: 16-bit half-range]\n");
    test_event_seq_duplicate();
    test_event_seq_backward();
    test_event_seq_rollover();
    test_event_seq_delta_32768();

    printf("\n[sequence + safety]\n");
    test_5_round_sequence();
    test_null_safety();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           _test_count - _test_failures, _test_count, _test_failures);
    return _test_failures ? 1 : 0;
}
