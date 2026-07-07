/*==========================================================================
 *  board_io.h  —  FPGA↔CPU APB3 寄存器读写接口
 *
 *  基于 vision_register_handbook_draft v2 (2026-07-05 双路版)
 *  基地址以 Efinity 生成 soc.h 为准，本文件只用偏移量。
 *==========================================================================*/

#ifndef BOARD_IO_H
#define BOARD_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 *  编译期校验：正式构建必须由 soc.h 提供基地址；测试/独立编译可用占位值
 *--------------------------------------------------------------------------*/
#if !defined(IO_APB_SLAVE_0_BASE) && !defined(IO_APB_SLAVE_x_BASE)
#  ifdef APB_VISION_BASE_PLACEHOLDER
#    define IO_APB_SLAVE_0_BASE  APB_VISION_BASE_PLACEHOLDER
#    warning "APB3 base address not provided by soc.h — using placeholder. Real build MUST include soc.h."
#  else
#    error "APB3 base address not defined. Include the Efinity-generated soc.h, or define APB_VISION_BASE_PLACEHOLDER for test builds."
#  endif
#endif

/* 如果 soc.h 定义的宏名不同，在此统一映射 */
#ifndef APB_VISION_BASE
#  ifdef IO_APB_SLAVE_0_BASE
#    define APB_VISION_BASE   IO_APB_SLAVE_0_BASE
#  else
#    define APB_VISION_BASE   IO_APB_SLAVE_x_BASE
#  endif
#endif

/*--------------------------------------------------------------------------
 *  FPGA 变更适配开关 —— 改这里，不改业务逻辑
 *--------------------------------------------------------------------------*/
#define HANDSHAKE_INDEPENDENT  0   /* 每路独立 valid/ack/frame_id（默认）*/
#define HANDSHAKE_MERGED       1   /* 两路合并握手（FPGA 可能要求）   */

#ifndef HANDSHAKE_MODE
#  define HANDSHAKE_MODE  HANDSHAKE_INDEPENDENT
#endif

/* FG_AREA_AVAILABLE: FPGA 实现了 LIVE_FG_AREA 寄存器时改为 1。
   默认 0 → CPU 降级用 R+G+B 面积近似填充率。
   改为 1 → CPU 直接读 LIVE_FG_AREA 计算填充率，对白/黑物体更准。 */
#ifndef FG_AREA_AVAILABLE
#  define FG_AREA_AVAILABLE  0
#endif

/* 合并握手模式尚未定义具体协议；若 FPGA 方要求改为 MERGED，
   必须先补充 STATUS/ACK 合并规则再解除此 error。 */
#if HANDSHAKE_MODE == HANDSHAKE_MERGED
#  error "HANDSHAKE_MERGED protocol not yet defined. Define merged STATUS/ACK rules first."
#endif

/* 摄像头启用控制：CAM_ENABLED(0)=俯视, CAM_ENABLED(1)=侧面 */
#ifndef CAM_ENABLED
#  define CAM_ENABLED(n)  (1)     /* 默认两路都启用 */
#endif

/* 全局寄存器跟哪个摄像头的 commit 通道：CAM0(0) 或 CAM1(1) */
#ifndef CAM_COMMIT_GLOBAL
#  define CAM_COMMIT_GLOBAL  0    /* 默认跟 Cam0 */
#endif

/*--------------------------------------------------------------------------
 *  内存映射 I/O 原子操作
 *--------------------------------------------------------------------------*/
static inline void mmio_write32(uint32_t addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
}

static inline uint32_t mmio_read32(uint32_t addr)
{
    return *(volatile uint32_t *)addr;
}

/*--------------------------------------------------------------------------
 *  Register Map 偏移量（与手册 §2 完全对齐）
 *--------------------------------------------------------------------------*/

/* === 2.1 全局寄存器 === */
#define OFF_REG_MAGIC           0x000u   /* R   Magic 0x375A */
#define OFF_SYS_CTRL            0x004u   /* R/W [4]=HDMI sel(r), [3:2]=Cam1[OSD,en], [1:0]=Cam0[OSD,en] */
#define OFF_CPU_HEARTBEAT       0x010u   /* R/W 32-bit 递增心跳 */
#define OFF_CPU_ARM_STATE       0x064u   /* R/W 机械臂状态机阶段，跟 CAM_COMMIT_GLOBAL */
#define OFF_CPU_ERROR_CODE      0x068u   /* R/W 错误码，跟 CAM_COMMIT_GLOBAL */

/* SYS_CTRL bit 定义 */
#define SYS_CTRL_CAM0_EN        0x01u
#define SYS_CTRL_CAM0_OSD       0x02u
#define SYS_CTRL_CAM1_EN        0x04u
#define SYS_CTRL_CAM1_OSD       0x08u
#define SYS_CTRL_HDMI_SEL       0x10u   /* R: 0=Cam0, 1=Cam1 */

/* === 2.2 Camera 0 寄存器（俯视）=== */
#define OFF_CAM0_BASE           0x000u   /* Cam0 区域基偏移 */
#define OFF_SYS_STATUS0         0x008u   /* R   [0]=valid, [31:16]=frame_id */
#define OFF_SYS_ACK0            0x00Cu   /* W   [15:0]=frame_id 应答 */
#define OFF_CFG_CAM0_ROI_TL     0x014u   /* R/W [31:16]Y_MIN, [15:0]X_MIN */
#define OFF_CFG_CAM0_ROI_BR     0x018u   /* R/W [31:16]Y_MAX, [15:0]X_MAX */
#define OFF_CFG_CAM0_RED_TH_0   0x020u   /* R/W 红色阈值下限 */
#define OFF_CFG_CAM0_RED_TH_1   0x024u   /* R/W 红色阈值上限 */
#define OFF_CFG_CAM0_BLUE_TH_0  0x028u   /* R/W 蓝色阈值下限 */
#define OFF_CFG_CAM0_BLUE_TH_1  0x02Cu   /* R/W 蓝色阈值上限 */
#define OFF_CFG_CAM0_YEL_TH_0   0x030u   /* R/W 黄色阈值下限 */
#define OFF_CFG_CAM0_YEL_TH_1   0x034u   /* R/W 黄色阈值上限 */
#define OFF_CFG_CAM0_LUMA_TH    0x03Cu   /* R/W [31:16]MAX, [15:0]MIN */
#define OFF_CPU_OSD_CTRL0       0x040u   /* R/W OSD 控制字 */
#define OFF_CPU_OSD_BBOX_MIN0   0x044u   /* R/W 红框 min */
#define OFF_CPU_OSD_BBOX_MAX0   0x048u   /* R/W 红框 max */
#define OFF_CFG_COMMIT0         0x04Cu   /* W   [15:0]=config_seq */
#define OFF_CFG_STATUS0         0x050u   /* R   [31:16]active_seq, [15:0]pending_seq */
#define OFF_CPU_RESULT_COLOR0   0x054u   /* R/W 颜色结果 */
#define OFF_CPU_RESULT_SHAPE0   0x058u   /* R/W 形状结果 */
#define OFF_CPU_RESULT_SIZE0    0x05Cu   /* R/W 尺寸 0.1cm */
#define OFF_CPU_MATCH_ACTION0   0x060u   /* R/W 匹配/动作 */
#define OFF_LIVE_RED_AREA0      0x080u   /* R   红色像素数 */
#define OFF_LIVE_BLUE_AREA0     0x084u   /* R   蓝色像素数 */
#define OFF_LIVE_YEL_AREA0      0x088u   /* R   黄色像素数 */
#define OFF_LIVE_FG_AREA0       0x0B0u   /* R   bbox 内前景像素总数（手册 v2.1 新增）*/
#define OFF_LIVE_BBOX_MIN0      0x0A0u   /* R   [31:16]Y_MIN,[15:0]X_MIN */
#define OFF_LIVE_BBOX_MAX0      0x0A4u   /* R   [31:16]Y_MAX,[15:0]X_MAX */
#define OFF_LIVE_CENTER0        0x0A8u   /* R   [31:16]Y_CEN,[15:0]X_CEN */

/* === 2.3 Camera 1 寄存器（侧面）=== (offset = Cam0 + 0x100) */
#define CAM_OFFSET              0x100u
#define OFF_SYS_STATUS1         0x108u
#define OFF_SYS_ACK1            0x10Cu
#define OFF_CFG_CAM1_ROI_TL     0x114u
#define OFF_CFG_CAM1_ROI_BR     0x118u
#define OFF_CFG_CAM1_RED_TH_0   0x120u
#define OFF_CFG_CAM1_RED_TH_1   0x124u
#define OFF_CFG_CAM1_BLUE_TH_0  0x128u
#define OFF_CFG_CAM1_BLUE_TH_1  0x12Cu
#define OFF_CFG_CAM1_YEL_TH_0   0x130u
#define OFF_CFG_CAM1_YEL_TH_1   0x134u
#define OFF_CFG_CAM1_LUMA_TH    0x13Cu
#define OFF_CPU_OSD_CTRL1       0x140u
#define OFF_CPU_OSD_BBOX_MIN1   0x144u
#define OFF_CPU_OSD_BBOX_MAX1   0x148u
#define OFF_CFG_COMMIT1         0x14Cu
#define OFF_CFG_STATUS1         0x150u
#define OFF_CPU_RESULT_COLOR1   0x154u
#define OFF_CPU_RESULT_SHAPE1   0x158u
#define OFF_CPU_RESULT_SIZE1    0x15Cu
#define OFF_CPU_MATCH_ACTION1   0x160u
#define OFF_LIVE_RED_AREA1      0x180u
#define OFF_LIVE_BLUE_AREA1     0x184u
#define OFF_LIVE_YEL_AREA1      0x188u
#define OFF_LIVE_FG_AREA1       0x1B0u   /* R   bbox 内前景像素总数 */
#define OFF_LIVE_BBOX_MIN1      0x1A0u
#define OFF_LIVE_BBOX_MAX1      0x1A4u
#define OFF_LIVE_CENTER1        0x1A8u
#define OFF_LIVE_HEIGHT_PX      0x1ACu   /* Cam1 专属：侧面物体像素高度 */

/* 地址宏：基址 + 偏移量（OFF_* 已是绝对偏移，不再额外加 cam * CAM_OFFSET）*/
#define REG_ADDR(off)           (APB_VISION_BASE + (off))

/*--------------------------------------------------------------------------
 *  便捷宏 — 按摄像头编号取偏移量
 *--------------------------------------------------------------------------*/
#define OFF_SYS_STATUS(n)       ((n) == 0 ? OFF_SYS_STATUS0   : OFF_SYS_STATUS1)
#define OFF_SYS_ACK(n)          ((n) == 0 ? OFF_SYS_ACK0      : OFF_SYS_ACK1)
#define OFF_CFG_ROI_TL(n)       ((n) == 0 ? OFF_CFG_CAM0_ROI_TL : OFF_CFG_CAM1_ROI_TL)
#define OFF_CFG_ROI_BR(n)       ((n) == 0 ? OFF_CFG_CAM0_ROI_BR : OFF_CFG_CAM1_ROI_BR)
#define OFF_CFG_RED_TH_0(n)     ((n) == 0 ? OFF_CFG_CAM0_RED_TH_0 : OFF_CFG_CAM1_RED_TH_0)
#define OFF_CFG_RED_TH_1(n)     ((n) == 0 ? OFF_CFG_CAM0_RED_TH_1 : OFF_CFG_CAM1_RED_TH_1)
#define OFF_CFG_BLUE_TH_0(n)    ((n) == 0 ? OFF_CFG_CAM0_BLUE_TH_0 : OFF_CFG_CAM1_BLUE_TH_0)
#define OFF_CFG_BLUE_TH_1(n)    ((n) == 0 ? OFF_CFG_CAM0_BLUE_TH_1 : OFF_CFG_CAM1_BLUE_TH_1)
#define OFF_CFG_YEL_TH_0(n)     ((n) == 0 ? OFF_CFG_CAM0_YEL_TH_0 : OFF_CFG_CAM1_YEL_TH_0)
#define OFF_CFG_YEL_TH_1(n)     ((n) == 0 ? OFF_CFG_CAM0_YEL_TH_1 : OFF_CFG_CAM1_YEL_TH_1)
#define OFF_CFG_LUMA_TH(n)      ((n) == 0 ? OFF_CFG_CAM0_LUMA_TH : OFF_CFG_CAM1_LUMA_TH)
#define OFF_CFG_COMMIT(n)       ((n) == 0 ? OFF_CFG_COMMIT0   : OFF_CFG_COMMIT1)
#define OFF_CFG_STATUS(n)       ((n) == 0 ? OFF_CFG_STATUS0   : OFF_CFG_STATUS1)
#define OFF_CPU_OSD_CTRL(n)     ((n) == 0 ? OFF_CPU_OSD_CTRL0  : OFF_CPU_OSD_CTRL1)
#define OFF_CPU_OSD_BBOX_MIN(n) ((n) == 0 ? OFF_CPU_OSD_BBOX_MIN0 : OFF_CPU_OSD_BBOX_MIN1)
#define OFF_CPU_OSD_BBOX_MAX(n) ((n) == 0 ? OFF_CPU_OSD_BBOX_MAX0 : OFF_CPU_OSD_BBOX_MAX1)
#define OFF_CPU_RESULT_COLOR(n) ((n) == 0 ? OFF_CPU_RESULT_COLOR0 : OFF_CPU_RESULT_COLOR1)
#define OFF_CPU_RESULT_SHAPE(n) ((n) == 0 ? OFF_CPU_RESULT_SHAPE0 : OFF_CPU_RESULT_SHAPE1)
#define OFF_CPU_RESULT_SIZE(n)  ((n) == 0 ? OFF_CPU_RESULT_SIZE0  : OFF_CPU_RESULT_SIZE1)
#define OFF_CPU_MATCH_ACTION(n) ((n) == 0 ? OFF_CPU_MATCH_ACTION0 : OFF_CPU_MATCH_ACTION1)
#define OFF_LIVE_RED_AREA(n)    ((n) == 0 ? OFF_LIVE_RED_AREA0  : OFF_LIVE_RED_AREA1)
#define OFF_LIVE_BLUE_AREA(n)   ((n) == 0 ? OFF_LIVE_BLUE_AREA0 : OFF_LIVE_BLUE_AREA1)
#define OFF_LIVE_YEL_AREA(n)    ((n) == 0 ? OFF_LIVE_YEL_AREA0  : OFF_LIVE_YEL_AREA1)
#define OFF_LIVE_FG_AREA(n)     ((n) == 0 ? OFF_LIVE_FG_AREA0   : OFF_LIVE_FG_AREA1)
#define OFF_LIVE_BBOX_MIN(n)    ((n) == 0 ? OFF_LIVE_BBOX_MIN0  : OFF_LIVE_BBOX_MIN1)
#define OFF_LIVE_BBOX_MAX(n)    ((n) == 0 ? OFF_LIVE_BBOX_MAX0  : OFF_LIVE_BBOX_MAX1)
#define OFF_LIVE_CENTER(n)      ((n) == 0 ? OFF_LIVE_CENTER0    : OFF_LIVE_CENTER1)

/* Cam1 专属 */
#define REG_LIVE_HEIGHT_PX      REG_ADDR(OFF_LIVE_HEIGHT_PX)

/*--------------------------------------------------------------------------
 *  Feature snapshot（一帧的特征数据，CPU 端暂存）
 *--------------------------------------------------------------------------*/
typedef struct {
    uint32_t red_area;
    uint32_t blue_area;
    uint32_t yel_area;
    uint32_t bbox_min;          /* [31:16]Y_MIN, [15:0]X_MIN */
    uint32_t bbox_max;          /* [31:16]Y_MAX, [15:0]X_MAX */
    uint32_t center;            /* [31:16]Y_CEN, [15:0]X_CEN */
    uint32_t fg_area;           /* bbox 内前景像素总数（FG_AREA_AVAILABLE=1 时有效；否则 0）*/
    uint32_t height_px;         /* Cam1 only; 0 for Cam0 */
    uint16_t frame_id;
} feature_snapshot_t;

/*--------------------------------------------------------------------------
 *  Result writeback（CPU 分类结果，回写 FPGA）
 *--------------------------------------------------------------------------*/
typedef struct {
    uint8_t  color;             /* 0=unknown,1=white,2=black,3=red,4=blue,5=yellow */
    uint8_t  shape;             /* 0=unknown,1=cube,2=cylinder,3=cone */
    uint8_t  size_cm_x10;       /* 0.1cm: 20=2.0cm, 25=2.5cm, 30=3.0cm */
    uint8_t  match_action;      /* [0]match, [1]grab, [2]skip, [3]error */
    uint32_t osd_bbox_min;      /* OSD 红框 min */
    uint32_t osd_bbox_max;      /* OSD 红框 max */
    uint32_t osd_ctrl;          /* OSD 控制字 */
} result_writeback_t;

/*--------------------------------------------------------------------------
 *  API 声明
 *
 *  协议约定：
 *   - 所有写 CFG_* / CPU_RESULT_* / CPU_OSD_* / CPU_ARM_STATE / CPU_ERROR_CODE
 *     只写了 staging, 必须再显式调用 commit 才会在 VSYNC 边界生效。
 *   - CamN 的结果/配置跟随 CFG_COMMIT_N 提交; 全局寄存器跟随
 *     CAM_COMMIT_GLOBAL（默认 Cam0）的 CFG_COMMIT 提交。
 *--------------------------------------------------------------------------*/

/* 上电握手：读 REG_MAGIC 高 16-bit 验证 APB 窗口存活 */
int board_io_validate(void);

/* 写 CamN 的 CFG_* 配置到 staging（不触发 commit） */
void board_io_write_config(int cam,
                           uint16_t roi_tl_y, uint16_t roi_tl_x,
                           uint16_t roi_br_y, uint16_t roi_br_x,
                           uint32_t red_th_0, uint32_t red_th_1,
                           uint32_t blue_th_0, uint32_t blue_th_1,
                           uint32_t yel_th_0, uint32_t yel_th_1,
                           uint16_t luma_min, uint16_t luma_max);

/* 写 CamN 的 CFG_COMMIT，阻塞到 active_seq == config_seq 或超时。
 * *seq 会被更新为本次成功的 config_seq；调用方每次传入不同值。
 * CFG_COMMIT 是 16-bit 全值比较，不是截位。 */
int board_io_commit_config(int cam, uint16_t *seq);

/* 等待 CamN 的 feature_valid，读完整 LIVE_* 特征快照。
 * 若 CAM_ENABLED(n)==0 直接返回 -1。 */
int board_io_read_features(int cam, feature_snapshot_t *snap);

/* 应答 CamN 的 frame_id，释放 FPGA 快照 */
void board_io_ack_frame(int cam, uint16_t frame_id);

/* 回写 CamN 的分类结果 + OSD 坐标到 staging（不触发 commit）。
 * 需要随后调用 board_io_commit_config(cam, seq) 使其在 VSYNC 生效。 */
void board_io_write_results(int cam, const result_writeback_t *r);

/* 提交 CamN 的 staging 结果到 active（等价于对 CamN 调 commit_config）。
 * 通常在一帧的 write_results + write_global_state 之后调用。 */
int board_io_commit_results(int cam, uint16_t *seq);

/* 写全局 ARM_STATE + ERROR_CODE 到 staging（不触发 commit）。
 * 随后需要用 board_io_commit_config(CAM_COMMIT_GLOBAL, seq) 提交，
 * 因为手册规定全局寄存器跟 CAM_COMMIT_GLOBAL 的 commit 通道生效。 */
void board_io_write_global_state(uint8_t arm_state, uint16_t error_code);

/* 全局提交：对 CAM_COMMIT_GLOBAL 摄像头做 commit。
 * 在一帧 write_results + write_global_state 全部完成后调用一次即可。 */
int board_io_commit_global(uint16_t *seq);

/* 周期递增 CPU_HEARTBEAT（直接写，不走 staging/commit） */
void board_io_heartbeat(void);

/* 读全局 SYS_CTRL（HDMI 选择位、各通道使能/OSD） */
uint32_t board_io_read_sys_ctrl(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_IO_H */
