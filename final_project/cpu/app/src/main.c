/*==========================================================================
 *  main.c  —  识别主循环集成（模块 5 + ARM_DISABLED round_controller 接入）
 *
 *  集成 board_io + vision_classifier + param_table + task_matcher +
 *  round_controller + cpu_result_semantics。
 *
 *  识别侧边界（严格）：
 *    - grab_center 仅用于 UART 日志和 OSD 偏差检查。
 *    - 机械臂控制 (arm_controller) 不在本人范围。
 *    - ARM_DISABLED=1：arm_enabled 固定为 0，目标轮直接 ARM_NOT_READY，
 *      绝对不发生 request_arm_grab 或任何机械臂动作请求。
 *
 *  主循环流程：
 *    1. 心跳
 *    2. Cam0/1 轮询：读特征 → 分类 → 滤波 → ack → 回写分类结果
 *    3. 融合 + 任务匹配 → action (task_matcher)
 *    4. 两路 match_action 重写（OSD 一致性）
 *    5. UART 事件注入 + round_controller_tick + cpu_result_semantics 投影
 *    6. 全局状态提交
 *==========================================================================*/

#include "bsp.h"
#include "board_io.h"
#include "vision_classifier.h"
#include "param_table.h"
#include "task_matcher.h"
#include "round_controller.h"
#include "cpu_result_semantics.h"
#include "cpu_result_semantics_adapters.h"
#include "main_loop_adapter.h"

#include <stdint.h>
#include <string.h>

/*--------------------------------------------------------------------------
 *  ARM 状态码（本人侧只输出，队友 arm_controller 负责执行）
 *--------------------------------------------------------------------------*/
#define ARM_STATE_UNKNOWN   0
#define ARM_STATE_IDLE      1
#define ARM_STATE_GRABBING  2
#define ARM_STATE_ERROR     9

/*--------------------------------------------------------------------------
 *  错误码
 *--------------------------------------------------------------------------*/
#define ERR_NO_FPGA           1   /* APB 握手失败 — FPGA 未就绪或基地址错误 */
#define ERR_COMMIT_TIMEOUT     2   /* CFG_COMMIT 超时 — staging 未进入 active */

/*--------------------------------------------------------------------------
 *  ARM_DISABLED：编译期全局机械臂安全开关。
 *  置 1 时 round_controller arm_enabled 固定为 0，目标轮直接 ARM_NOT_READY，
 *  绝对不发生 request_arm_grab 或任何机械臂动作请求。
 *  仅当正式 soc.h / APB / UART2 D2 / 机械臂 T0 全部关闭后才允许置 0。
 *--------------------------------------------------------------------------*/
#ifndef ARM_DISABLED
#define ARM_DISABLED 1
#endif

/*--------------------------------------------------------------------------
 *  配置写入占位值
 *
 *  FPGA 颜色分割阈值、ROI 等需要现场标定。当前使用保守全帧默认值，
 *  确保两路摄像头都能产生特征数据，后续由 param_table 标定流程覆盖。
 *--------------------------------------------------------------------------*/
#define INIT_ROI_TL_Y    0
#define INIT_ROI_TL_X    0
#define INIT_ROI_BR_Y  1079
#define INIT_ROI_BR_X  1919
#define INIT_RED_TH_0   0x00000010u
#define INIT_RED_TH_1   0x00FFFF00u
#define INIT_BLUE_TH_0  0x00000010u
#define INIT_BLUE_TH_1  0x0000FF00u
#define INIT_YEL_TH_0   0x00000010u
#define INIT_YEL_TH_1   0x00FFFF00u
#define INIT_LUMA_MIN   0x0010u
#define INIT_LUMA_MAX   0x00F0u

/*==========================================================================
 *  UART 日志辅助
 *==========================================================================*/

static void uart_puts(const char *s)
{
    while (*s) bsp_putChar(*s++);
}

static void uart_put_dec(uint32_t v)
{
    char buf[12];
    int i = 0;
    if (v == 0) { bsp_putChar('0'); return; }
    while (v > 0) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i > 0) bsp_putChar(buf[--i]);
}

/*--------------------------------------------------------------------------
 *  每帧日志（精简版，不刷屏）
 *
 *  输出格式：
 *    [C color shape size_mm] [action] [cx,cy]
 *  例：
 *    [0 R CU ----] [1 B CU  25] | fuse=CU action=GRAB cx=0320 cy=0240
 *--------------------------------------------------------------------------*/
static void log_frame(int cam, const vision_result_t *s,
                       const feature_snapshot_t *snap)
{
    static const char color_ch[] = { '?','W','K','R','B','Y' };
    static const char shape_ch[] = { '?','C','Y','N' };  /* Cube, cYlinder, coNe */
    (void)snap;  /* 未来可打印 bbox / fg_area */

    bsp_putChar('[');
    bsp_putChar(cam == 0 ? '0' : '1');
    bsp_putChar(' ');

    /* color */
    uint8_t cid = s->color_id;
    bsp_putChar(cid < 6 ? color_ch[cid] : '?');
    bsp_putChar(' ');

    /* shape */
    uint8_t sid = s->shape_id;
    bsp_putChar(sid < 4 ? shape_ch[sid] : '?');
    bsp_putChar(' ');

    /* size (mm) — size_cm_x10 is 0.1cm units, which is mm.
     *  e.g. size_cm_x10=20 → 2.0cm → 20mm. Print directly, no conversion. */
    if (s->size_cm_x10 > 0) {
        if (s->size_cm_x10 < 100) bsp_putChar(' ');
        uart_put_dec(s->size_cm_x10);
    } else {
        uart_puts("---");
    }
    bsp_putChar(']');
}

static void log_action_fused(uint8_t action, const vision_result_t *fused,
                              uint16_t cx, uint16_t cy)
{
    static const char *act_str[] = {
        "NONE", "GRAB", "SKIP", "ERROR"
    };
    bsp_putChar(' ');
    bsp_putChar('|');
    bsp_putChar(' ');

    /* fuse result */
    bsp_putChar('f');
    bsp_putChar('=');
    static const char shape_ch[] = { '?','C','Y','N' };
    uint8_t sid = fused->shape_id;
    bsp_putChar(sid < 4 ? shape_ch[sid] : '?');
    bsp_putChar(' ');

    /* action */
    uart_puts("act=");
    uart_puts(action < 4 ? act_str[action] : "???");

    /* grab center (log only — NOT sent to arm_controller) */
    bsp_putChar(' ');
    uart_puts("cx=");
    uart_put_dec(cx);
    uart_puts(" cy=");
    uart_put_dec(cy);

    bsp_putChar('\r');
    bsp_putChar('\n');
}

/*==========================================================================
 *  配置初始化 — 写两路初始 ROI + 颜色阈值
 *  返回 0 成功，-1 表示 commit 超时。
 *==========================================================================*/

static int init_camera_configs(uint16_t *seq0, uint16_t *seq1)
{
    int rc;

    /* Cam0 (俯视) */
    board_io_write_config(0,
                          INIT_ROI_TL_Y, INIT_ROI_TL_X,
                          INIT_ROI_BR_Y, INIT_ROI_BR_X,
                          INIT_RED_TH_0,  INIT_RED_TH_1,
                          INIT_BLUE_TH_0, INIT_BLUE_TH_1,
                          INIT_YEL_TH_0,  INIT_YEL_TH_1,
                          INIT_LUMA_MIN,  INIT_LUMA_MAX);
    rc = board_io_commit_config(0, seq0);
    if (rc != 0) {
        uart_puts("[FATAL] Cam0 config commit timeout.\r\n");
        return -1;
    }

    /* Cam1 (侧面) */
    board_io_write_config(1,
                          INIT_ROI_TL_Y, INIT_ROI_TL_X,
                          INIT_ROI_BR_Y, INIT_ROI_BR_X,
                          INIT_RED_TH_0,  INIT_RED_TH_1,
                          INIT_BLUE_TH_0, INIT_BLUE_TH_1,
                          INIT_YEL_TH_0,  INIT_YEL_TH_1,
                          INIT_LUMA_MIN,  INIT_LUMA_MAX);
    rc = board_io_commit_config(1, seq1);
    if (rc != 0) {
        uart_puts("[FATAL] Cam1 config commit timeout.\r\n");
        return -1;
    }

    return 0;
}

/*==========================================================================
 *  单路帧处理：读 → 分类 → 滤波 → ack → 日志
 *  返回 1 表示该路 snapshot 有效（后续可用于回写），0 表示无新帧。
 *  obj_cx/obj_cy 仅在 cam==0 时输出 bbox 中心（供 evaluate 用）。
 *==========================================================================*/

static int process_camera(int cam, mf_filter_t *filt,
                           const classifier_cfg_t *cfg,
                           feature_snapshot_t *snap_out,
                           vision_result_t *stable_out)
{
    feature_snapshot_t snap;
    if (board_io_read_features(cam, &snap) != 0)
        return 0;   /* 无新帧或 CAM_ENABLED(n)==0 */

    vision_result_t raw  = classify_frame(&snap, cam, cfg);
    vision_result_t stable = mf_filter_update(filt, &raw, cfg);

    board_io_ack_frame(cam, snap.frame_id);

    /* 日志：输出该路当前帧分类 + 滤波后的稳定结果 */
    uart_puts("  ");
    log_frame(cam, &stable, &snap);

    /* 只有滤波稳定（color_id != UNKNOWN）才参与后续回写 */
    if (stable.color_id != COLOR_UNKNOWN && snap_out != 0)
        *snap_out = snap;
    if (stable_out != 0)
        *stable_out = stable;

    return 1;   /* 有新帧 */
}

/*==========================================================================
 *  round_controller 辅助 — 板载计时与 UART 事件注入
 *==========================================================================*/

/* 从 CLINT mtime 读取当前毫秒。Host mock 可替换为测试用计数器。 */
static uint32_t get_now_ms(void)
{
    volatile uint64_t *mtime = (volatile uint64_t *)(SYSTEM_CLINT_CTRL + 0xBFF8);
    return (uint32_t)(*mtime / (SYSTEM_CLINT_HZ / 1000u));
}

/* 非阻塞读一个 UART 字符；无数据返回 0。 */
static char uart_getchar_nb(void)
{
    uint32_t rx_occ = (read_u32(BSP_UART_TERMINAL + UART_STATUS) >> 24) & 0xFF;
    if (rx_occ == 0) return 0;
    return (char)(read_u32(BSP_UART_TERMINAL + UART_DATA) & 0xFF);
}

/* 从 UART 消费单字符命令 → round_event_t。
 *   'p' = PLACE, 'r' = REMOVE, 'a' = ABANDON,
 *   's' = SOFT_RESET, 'x' = SESSION_RESET
 * 返回 1=消费了事件，0=无事件或非法字符。 */
static int consume_uart_event(round_event_t *ev)
{
    char c = uart_getchar_nb();
    if (c == 0) return 0;
    switch (c) {
    case 'p': *ev = ROUND_EVENT_PLACE_CONFIRM;    return 1;
    case 'r': *ev = ROUND_EVENT_REMOVE_CONFIRM;   return 1;
    case 'a': *ev = ROUND_EVENT_ABANDON_ROUND;    return 1;
    case 's': *ev = ROUND_EVENT_SOFT_RESET_ROUND; return 1;
    case 'x': *ev = ROUND_EVENT_SESSION_RESET;    return 1;
    default:  return 0;
    }
}

/*==========================================================================
 *  main — 识别主循环（含 ARM_DISABLED round_controller 接入）
 *==========================================================================*/

int main(void)
{
    bsp_init();
    uart_puts("\r\n========================================\r\n");
    uart_puts("  TJ375N529 Vision Recognition Main\r\n");
    uart_puts("  CPU_MODULE_PLAN v5 — 识别主循环集成\r\n");
    uart_puts("  Modules: board_io + classifier + params + matcher\r\n");
    uart_puts("========================================\r\n\r\n");

    /* ---- 1. 上电握手 ---- */
    uart_puts("[INIT] Validating APB window...\r\n");
    if (board_io_validate() != 0) {
        uart_puts("[FATAL] ERR_NO_FPGA — APB handshake failed.\r\n");
        uart_puts("        Check APB base address, FPGA config, power.\r\n");
        /* 心跳仍在跑，方便通过 heartbeat 寄存器确认 CPU 存活 */
        for (;;) { board_io_heartbeat(); }
    }
    uart_puts("[INIT] APB window OK (MAGIC high word matched).\r\n");

    /* ---- 2. 初始化 ---- */
    param_table_init();
    const classifier_cfg_t *cfg = param_table_get();

    mf_filter_t filt_cam0, filt_cam1;
    mf_filter_reset(&filt_cam0);
    mf_filter_reset(&filt_cam1);

    task_matcher_init();

    uint16_t seq_cam0 = 0, seq_cam1 = 0;

    /* ---- 3. 写入初始配置到两路 staging + commit ---- */
    uart_puts("[INIT] Writing camera configs...\r\n");
    if (init_camera_configs(&seq_cam0, &seq_cam1) != 0) {
        uart_puts("[FATAL] ERR_COMMIT_TIMEOUT — init config commit failed.\r\n");
        uart_puts("        Cameras may not be configured; FPGA may be unresponsive.\r\n");
        for (;;) { board_io_heartbeat(); }
    }
    uart_puts("[INIT] Both cameras configured.\r\n\r\n");

    /* ---- 3b. round_controller 初始化（ARM_DISABLED dry-run）---- */
    round_controller_t rc;
    round_controller_config_t rc_cfg;
    rc_cfg.acquire_timeout_ms = 3000u;
    rc_cfg.arm_timeout_ms     = 15000u;
    {
        round_controller_input_t  rc_in;
        round_controller_output_t rc_out;
        uint32_t t0 = get_now_ms();
        round_controller_init(&rc, &rc_cfg, t0);
        /* 首次 APPLY_CONFIG → WAIT_PLACE_CONFIRM */
        memset(&rc_in, 0, sizeof(rc_in));
        rc_in.now_ms     = t0;
        rc_in.event_valid = 1;
        rc_in.event_seq   = 1;
        rc_in.event       = ROUND_EVENT_APPLY_CONFIG;
        round_controller_tick(&rc, &rc_in, &rc_out);
        uart_puts("[INIT] round_controller: CONFIG -> WAIT_PLACE_CONFIRM\r\n");
    }

    uart_puts("[LOOP] Entering main recognition loop...\r\n");
    uart_puts("[LOOP] UART cmds: p=PLACE r=REMOVE a=ABANDON s=SOFT_RESET x=SESSION_RESET\r\n\r\n");

    /* ---- 4. 主循环 ---- */
    uint16_t uart_event_seq = 2;  /* seq 1 已被 APPLY_CONFIG 占用 */
    uint16_t latched_err = 0;    /* persists across iterations until commit_global succeeds */
    for (;;) {
        /* Carry forward any error that couldn't be committed to FPGA last iteration */
        uint16_t loop_err = latched_err;

        board_io_heartbeat();

        feature_snapshot_t snap0, snap1;
        vision_result_t   s0, s1;
        uint16_t obj_cx = 0, obj_cy = 0;
        int cam0_ok = 0, cam1_ok = 0;

        memset(&snap0, 0, sizeof(snap0));
        memset(&snap1, 0, sizeof(snap1));
        memset(&s0,    0, sizeof(s0));
        memset(&s1,    0, sizeof(s1));

        /* ---- 处理 Cam0 ---- */
        if (process_camera(0, &filt_cam0, cfg, &snap0, &s0)) {
            obj_cx = (uint16_t)(snap0.center & 0xFFFF);
            obj_cy = (uint16_t)((snap0.center >> 16) & 0xFFFF);
            cam0_ok = (s0.color_id != COLOR_UNKNOWN);

            /* 先回写分类结果（match_action 先填 0，等融合后更新） */
            result_writeback_t wb0;
            board_io_build_writeback(s0.color_id, s0.shape_id,
                                      s0.size_cm_x10, 0, &snap0, &wb0);
            board_io_write_results(0, &wb0);
            if (board_io_commit_results(0, &seq_cam0) != 0)
                loop_err = ERR_COMMIT_TIMEOUT;
        }

        /* ---- 处理 Cam1 ---- */
        if (process_camera(1, &filt_cam1, cfg, &snap1, &s1)) {
            cam1_ok = (s1.color_id != COLOR_UNKNOWN);

            /* 先回写分类结果（match_action 先填 0） */
            result_writeback_t wb1;
            board_io_build_writeback(s1.color_id, s1.shape_id,
                                      s1.size_cm_x10, 0, &snap1, &wb1);
            board_io_write_results(1, &wb1);
            if (board_io_commit_results(1, &seq_cam1) != 0)
                loop_err = ERR_COMMIT_TIMEOUT;
        }

        /* ---- 融合决策 ---- */
        vision_result_t fused = fuse_results(&filt_cam0.stable,
                                              &filt_cam1.stable, cfg);

        /* ---- 目标输入 ----
         * 当前路径：task_matcher_read_target_from_fpga() 只解红/蓝/黄
         * （旧 2-bit color_sel），不支持白/黑和 task_mode。
         *
         * 四任务正式接入点（待 FPGA 确认 3-bit color_sel 后启用）：
         *   task_target_t t;
         *   memset(&t, 0, sizeof(t));
         *   t.target_color  = <read from 3-bit TARGET_SEL or UART cmd>;
         *   t.target_shape  = SHAPE_CUBE;
         *   t.target_size_cm_x10 = <size_sel>;
         *   t.reference_size_cm_x10 = <ref_size, UART or fixed>;
         *   t.task_mode     = <task_mode from TARGET_SEL or UART>;
         *   task_matcher_set_target_ex(&t);
         *
         * 在硬件就绪前，可临时用 UART 调试命令注入五色目标：
         *   if (uart_cmd_target_ready) { task_matcher_set_target_ex(&uart_target); }
         */
        task_matcher_read_target_from_fpga();

        /* ---- 任务匹配 ---- */
        uint8_t action = task_matcher_evaluate(&fused, obj_cx, obj_cy);

        /* ---- 重写两路 match_action 为融合后 action ----
         * 两路都写同一个 action，保证 HDMI/OSD 切任一通道都看到一致的
         * 抓取/跳过/报错状态。commit 失败时 seq 不推进，下轮重试。 */
        if (cam0_ok) {
            result_writeback_t wb;
            board_io_build_writeback(s0.color_id, s0.shape_id,
                                      s0.size_cm_x10, action, &snap0, &wb);
            board_io_write_results(0, &wb);
            if (board_io_commit_results(0, &seq_cam0) != 0)
                loop_err = ERR_COMMIT_TIMEOUT;
        }
        if (cam1_ok) {
            result_writeback_t wb;
            board_io_build_writeback(s1.color_id, s1.shape_id,
                                      s1.size_cm_x10, action, &snap1, &wb);
            board_io_write_results(1, &wb);
            if (board_io_commit_results(1, &seq_cam1) != 0)
                loop_err = ERR_COMMIT_TIMEOUT;
        }

        /* ---- 日志：融合结果 + 动作 ---- */
        log_action_fused(action, &fused, obj_cx, obj_cy);

        /* ---- round_controller ARM_DISABLED dry-run 接入 ----
         * 调用可测试适配器 main_loop_arm_disabled_step()。
         * 事件源：UART 单字符命令。观测源：task_matcher_get_last_match()
         * （含真实 color/shape/size reason，不再硬编码 COLOR_MISMATCH）。
         * ARM_DISABLED: arm_enabled 固定为 0，目标轮直接 ARM_NOT_READY。 */
        {
            round_controller_output_t rc_out;
            cpu_display_result_t      display;
            round_event_t             ev = ROUND_EVENT_NONE;
            int                       has_ev = 0;
            task_match_result_t       match;
            const task_match_result_t *match_ptr = 0;

            /* UART 事件注入 */
            if (consume_uart_event(&ev)) {
                has_ev = 1;
            }

            /* 观测注入：从 matcher 读取最近一次真实 match result。
             * 仅 GRAB/SKIP 作为有效观测；NONE 不喂入。 */
            if (action == MATCH_ACTION_GRAB || action == MATCH_ACTION_SKIP) {
                task_matcher_get_last_match(&match);
                match_ptr = &match;
            }

            int rc_ret = main_loop_arm_disabled_step(
                &rc, &uart_event_seq, get_now_ms(),
                match_ptr, ev, has_ev,
                &display, &rc_out);

            /* 日志：轮次结果 */
            if (display.valid) {
                uart_puts("[RC] round=");
                uart_put_dec(rc_out.round_seq);
                uart_puts(" decision=");
                uart_puts(cpu_decision_text(display.decision));
                uart_puts(" exec=");
                uart_puts(cpu_execution_text(display.execution));
                uart_puts(" reason=");
                uart_puts(cpu_reason_text(display.reason));
                uart_puts(" is_target=");
                uart_put_dec(display.is_target);
                uart_puts(" req_grab=");
                uart_put_dec(rc_out.request_arm_grab);
                uart_puts("\r\n");
            }

            /* ACK 日志 */
            if (rc_out.event_ack_valid) {
                uart_puts("[RC] ACK seq=");
                uart_put_dec(rc_out.event_ack_seq);
                uart_puts(" status=");
                uart_puts(rc_out.event_ack_status == ROUND_EVENT_ACK_ACCEPTED
                          ? "ACCEPTED" : "REJECTED");
                uart_puts("\r\n");
            }

            /* P1-2: 按适配器返回值分派解锁/复位。
             * 1 = REMOVE_CONFIRM accepted → 解锁 matcher
             * -1 = SESSION_RESET accepted → 重新初始化 */
            if (rc_ret == 1) {
                task_matcher_next_round();
                uart_puts("[RC] REMOVE accepted, matcher unlocked\r\n");
            } else if (rc_ret == -1) {
                /* SESSION_RESET：重设 RC + matcher */
                uint32_t t0 = get_now_ms();
                round_controller_config_t cfg;
                cfg.acquire_timeout_ms = 3000u;
                cfg.arm_timeout_ms     = 15000u;
                round_controller_init(&rc, &cfg, t0);
                {
                    round_controller_input_t  rc_in;
                    round_controller_output_t ro;
                    memset(&rc_in, 0, sizeof(rc_in));
                    rc_in.now_ms = t0;
                    rc_in.event_valid = 1;
                    rc_in.event_seq   = 1;
                    rc_in.event       = ROUND_EVENT_APPLY_CONFIG;
                    round_controller_tick(&rc, &rc_in, &ro);
                }
                uart_event_seq = 2;
                task_matcher_round_reset();
                uart_puts("[RC] SESSION_RESET: RC + matcher re-initialized\r\n");
            }
        }

        /* ---- 全局状态提交 ----
         * ARM_STATE: ARM_DISABLED 期间固定输出 IDLE。队友 arm_controller
         * 负责在抓取/分拣时更新为 GRABBING 等实际状态。
         * ERROR_CODE: 本轮 + 历史未提交错误的累积。
         * commit_global 失败 → latched_err 锁存，下轮重试提交。
         * commit_global 成功 → latched_err 清零，错误已送达 FPGA。 */
        board_io_write_global_state(ARM_STATE_IDLE, loop_err);
        if (board_io_commit_global(&seq_cam0) != 0) {
            /* commit_global 失败：当前 loop_err（含可能的历史锁存 + 本轮新错误）
             * 未能写入 FPGA 寄存器。锁存到下一轮重试。
             * 若 loop_err==0 但 commit_global 本身是唯一的失败，
             * 用 ERR_COMMIT_TIMEOUT 标记。 */
            latched_err = (loop_err != 0) ? loop_err : ERR_COMMIT_TIMEOUT;
        } else {
            latched_err = 0;   /* 成功提交 → 错误已送达 FPGA，清除锁存 */
        }
    }

    return 0;
}
