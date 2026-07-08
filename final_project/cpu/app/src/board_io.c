/*==========================================================================
 *  board_io.c  —  FPGA↔CPU APB3 寄存器读写 实现
 *
 *  基于 vision_register_handbook_draft v2 (2026-07-05 双路版)
 *
 *  协议：
 *   - 写 staging 寄存器不立即生效；需显式 commit 后在 VSYNC 边界生效。
 *   - CamN 结果/配置跟随 CFG_COMMIT_N 提交。
 *   - 全局寄存器 (ARM_STATE/ERROR_CODE) 跟随 CAM_COMMIT_GLOBAL 提交。
 *   - CFG_COMMIT 是 16-bit 全值比较，不是截位。
 *==========================================================================*/

#include <stdint.h>
#include "board_io.h"

/*==========================================================================
 *  内部常量
 *==========================================================================*/

/* commit / valid 轮询超时（迭代次数；QCRV32 主频待确认后调整） */
#define COMMIT_POLL_MAX   100000u
#define VALID_POLL_MAX    100000u

/* cam 参数合法性 — 必须是 0（俯视）或 1（侧面） */
#define VALID_CAM(c)  ((c) == 0 || (c) == 1)

/*==========================================================================
 *  公共 API
 *==========================================================================*/

/*--------------------------------------------------------------------------
 *  上电握手：读 REG_MAGIC 高 16-bit，验证 APB 窗口存活
 *--------------------------------------------------------------------------*/
int board_io_validate(void)
{
    uint32_t magic = mmio_read32(REG_ADDR(OFF_REG_MAGIC));
    uint16_t id    = (uint16_t)((magic >> 16) & 0xFFFFu);

    if (id == 0x375Au)
        return 0;

    return -1;
}

/*--------------------------------------------------------------------------
 *  写 CamN 全量 CFG 配置到 staging（不提交）
 *--------------------------------------------------------------------------*/
void board_io_write_config(int cam,
                           uint16_t roi_tl_y, uint16_t roi_tl_x,
                           uint16_t roi_br_y, uint16_t roi_br_x,
                           uint32_t red_th_0, uint32_t red_th_1,
                           uint32_t blue_th_0, uint32_t blue_th_1,
                           uint32_t yel_th_0, uint32_t yel_th_1,
                           uint16_t luma_min, uint16_t luma_max)
{
    if (!VALID_CAM(cam) || !CAM_ENABLED(cam))
        return;

    /* ROI */
    mmio_write32(REG_ADDR(OFF_CFG_ROI_TL(cam)),
                 ((uint32_t)roi_tl_y << 16) | roi_tl_x);
    mmio_write32(REG_ADDR(OFF_CFG_ROI_BR(cam)),
                 ((uint32_t)roi_br_y << 16) | roi_br_x);

    /* 颜色阈值 — 三对 */
    mmio_write32(REG_ADDR(OFF_CFG_RED_TH_0(cam)),  red_th_0);
    mmio_write32(REG_ADDR(OFF_CFG_RED_TH_1(cam)),  red_th_1);
    mmio_write32(REG_ADDR(OFF_CFG_BLUE_TH_0(cam)), blue_th_0);
    mmio_write32(REG_ADDR(OFF_CFG_BLUE_TH_1(cam)), blue_th_1);
    mmio_write32(REG_ADDR(OFF_CFG_YEL_TH_0(cam)),  yel_th_0);
    mmio_write32(REG_ADDR(OFF_CFG_YEL_TH_1(cam)),  yel_th_1);

    /* 亮度阈值 [31:16]=MAX, [15:0]=MIN */
    mmio_write32(REG_ADDR(OFF_CFG_LUMA_TH(cam)),
                 ((uint32_t)luma_max << 16) | luma_min);
}

/*--------------------------------------------------------------------------
 *  提交 CamN 的 CFG staging → active
 *
 *  写入 config_seq 到 CFG_COMMIT，轮询 CFG_STATUS 直到
 *  active_seq (bits [31:16]) == config_seq，或超时。
 *
 *  *seq 在本函数内递增；调用方传入持久变量即可。
 *  返回 0 成功，-1 超时。
 *--------------------------------------------------------------------------*/
int board_io_commit_config(int cam, uint16_t *seq)
{
    uint32_t i;
    uint32_t status;
    uint16_t active;

    if (!VALID_CAM(cam) || !CAM_ENABLED(cam) || seq == 0)
        return -1;

    uint16_t config_seq = ++(*seq);

    /* 写 CFG_COMMIT 触发 VSYNC 边界生效 */
    mmio_write32(REG_ADDR(OFF_CFG_COMMIT(cam)), (uint32_t)config_seq);

    /* 轮询等待 active_seq == config_seq */
    for (i = 0; i < COMMIT_POLL_MAX; i++) {
        status = mmio_read32(REG_ADDR(OFF_CFG_STATUS(cam)));
        active = (uint16_t)((status >> 16) & 0xFFFFu);

        if (active == config_seq)
            return 0;
    }

    return -1;  /* 超时 */
}

/*--------------------------------------------------------------------------
 *  等待 CamN 的 feature_valid，读取完整 LIVE_* 特征快照
 *
 *  返回 0 成功，-1 超时或 CAM_ENABLED(n)==0。
 *--------------------------------------------------------------------------*/
int board_io_read_features(int cam, feature_snapshot_t *snap)
{
    uint32_t i;
    uint32_t status;

    if (!VALID_CAM(cam) || !CAM_ENABLED(cam) || snap == 0)
        return -1;

    /* 等待 valid 标志（bit [0]） */
    for (i = 0; i < VALID_POLL_MAX; i++) {
        status = mmio_read32(REG_ADDR(OFF_SYS_STATUS(cam)));
        if (status & 0x1u)
            break;
    }

    if (!(status & 0x1u))
        return -1;  /* 超时，无有效帧 */

    /* 提取 frame_id（bits [31:16]） */
    snap->frame_id = (uint16_t)((status >> 16) & 0xFFFFu);

    /* 读取全部 LIVE 特征寄存器 */
    snap->red_area  = mmio_read32(REG_ADDR(OFF_LIVE_RED_AREA(cam)));
    snap->blue_area = mmio_read32(REG_ADDR(OFF_LIVE_BLUE_AREA(cam)));
    snap->yel_area  = mmio_read32(REG_ADDR(OFF_LIVE_YEL_AREA(cam)));
    snap->bbox_min  = mmio_read32(REG_ADDR(OFF_LIVE_BBOX_MIN(cam)));
    snap->bbox_max  = mmio_read32(REG_ADDR(OFF_LIVE_BBOX_MAX(cam)));
    snap->center    = mmio_read32(REG_ADDR(OFF_LIVE_CENTER(cam)));

#if FG_AREA_AVAILABLE
    snap->fg_area   = mmio_read32(REG_ADDR(OFF_LIVE_FG_AREA(cam)));
#else
    snap->fg_area   = 0;
#endif

    /* Cam1 专属 height_px；Cam0 读返回 0 */
    if (cam == 1)
        snap->height_px = mmio_read32(REG_ADDR(OFF_LIVE_HEIGHT_PX));
    else
        snap->height_px = 0;

    return 0;
}

/*--------------------------------------------------------------------------
 *  应答 CamN 的 frame_id，释放 FPGA 快照供下一帧
 *--------------------------------------------------------------------------*/
void board_io_ack_frame(int cam, uint16_t frame_id)
{
    if (!VALID_CAM(cam) || !CAM_ENABLED(cam))
        return;

    mmio_write32(REG_ADDR(OFF_SYS_ACK(cam)), (uint32_t)frame_id);
}

/*--------------------------------------------------------------------------
 *  回写 CamN 的分类结果 + OSD 坐标到 staging（不提交）
 *--------------------------------------------------------------------------*/
void board_io_write_results(int cam, const result_writeback_t *r)
{
    if (!VALID_CAM(cam) || !CAM_ENABLED(cam) || r == 0)
        return;

    mmio_write32(REG_ADDR(OFF_CPU_RESULT_COLOR(cam)),  (uint32_t)r->color);
    mmio_write32(REG_ADDR(OFF_CPU_RESULT_SHAPE(cam)),  (uint32_t)r->shape);
    mmio_write32(REG_ADDR(OFF_CPU_RESULT_SIZE(cam)),   (uint32_t)r->size_cm_x10);
    mmio_write32(REG_ADDR(OFF_CPU_MATCH_ACTION(cam)),  (uint32_t)r->match_action);

    mmio_write32(REG_ADDR(OFF_CPU_OSD_BBOX_MIN(cam)),  r->osd_bbox_min);
    mmio_write32(REG_ADDR(OFF_CPU_OSD_BBOX_MAX(cam)),  r->osd_bbox_max);
    mmio_write32(REG_ADDR(OFF_CPU_OSD_CTRL(cam)),      r->osd_ctrl);
}

/*--------------------------------------------------------------------------
 *  提交 CamN 的结果 staging → active（等价于 commit_config） */
/*--------------------------------------------------------------------------*/
int board_io_commit_results(int cam, uint16_t *seq)
{
    return board_io_commit_config(cam, seq);
}

/*--------------------------------------------------------------------------
 *  写全局 ARM_STATE + ERROR_CODE 到 staging（不提交）
 *--------------------------------------------------------------------------*/
void board_io_write_global_state(uint8_t arm_state, uint16_t error_code)
{
    mmio_write32(REG_ADDR(OFF_CPU_ARM_STATE),  (uint32_t)arm_state);
    mmio_write32(REG_ADDR(OFF_CPU_ERROR_CODE), (uint32_t)error_code);
}

/*--------------------------------------------------------------------------
 *  全局提交：对 CAM_COMMIT_GLOBAL 摄像头执行 commit
 *--------------------------------------------------------------------------*/
int board_io_commit_global(uint16_t *seq)
{
    return board_io_commit_config(CAM_COMMIT_GLOBAL, seq);
}

/*--------------------------------------------------------------------------
 *  周期递增 CPU_HEARTBEAT（直写，无 staging/commit）
 *--------------------------------------------------------------------------*/
void board_io_heartbeat(void)
{
    static uint32_t hb;
    hb++;
    mmio_write32(REG_ADDR(OFF_CPU_HEARTBEAT), hb);
}

/*--------------------------------------------------------------------------
 *  读全局 SYS_CTRL 寄存器
 *--------------------------------------------------------------------------*/
uint32_t board_io_read_sys_ctrl(void)
{
    return mmio_read32(REG_ADDR(OFF_SYS_CTRL));
}

/*--------------------------------------------------------------------------
 *  便捷打包：vision_result_t + feature_snapshot_t → result_writeback_t
 *
 *  调用方在主循环中填充 match_action 后再 write_results。
 *  首版约定：Cam0/Cam1 两路写同一个融合后 match_action，
 *  保证 HDMI/OSD 切任一通道都看到一致的抓取/跳过/报错状态。
 *--------------------------------------------------------------------------*/
void board_io_build_writeback(uint8_t color, uint8_t shape,
                              uint8_t size_cm_x10, uint8_t action,
                              const feature_snapshot_t *snap,
                              result_writeback_t *wb)
{
    if (wb == 0)
        return;

    wb->color        = color;
    wb->shape        = shape;
    wb->size_cm_x10  = size_cm_x10;
    wb->match_action = action;
    wb->osd_bbox_min = (snap != 0) ? snap->bbox_min : 0;
    wb->osd_bbox_max = (snap != 0) ? snap->bbox_max : 0;
    wb->osd_ctrl     = 0;
}
