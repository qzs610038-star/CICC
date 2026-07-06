/*==========================================================================
 *  vision_classifier.h  —  颜色 / 形状 / 尺寸分类
 *
 *  输入：feature_snapshot_t（来自 board_io）
 *  输出：vision_result_t（颜色 ID + 形状 ID + 尺寸 + 置信度）
 *
 *  算法概要：
 *   - 颜色：取 red/blue/yel 面积最大者为候选色；全低于阈值 → 用排除法
 *          判定白/黑
 *   - 形状：用 bbox 宽高比 + 填充率区分 cube / cylinder / cone
 *   - 尺寸：Cam1 侧面高度像素 → 查表得 cm；Cam0 俯视图面积近似
 *   - 滤波：滑动窗口多数投票，连续 N 帧一致才输出稳定结果
 *==========================================================================*/

#ifndef VISION_CLASSIFIER_H
#define VISION_CLASSIFIER_H

#include <stdint.h>
#include "board_io.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 *  颜色 ID（与 shape_detect.h / OSD 一致）
 *--------------------------------------------------------------------------*/
#define COLOR_UNKNOWN  0
#define COLOR_WHITE    1
#define COLOR_BLACK    2
#define COLOR_RED      3
#define COLOR_BLUE     4
#define COLOR_YELLOW   5

/*--------------------------------------------------------------------------
 *  形状 ID
 *--------------------------------------------------------------------------*/
#define SHAPE_UNKNOWN   0
#define SHAPE_CUBE      1
#define SHAPE_CYLINDER  2
#define SHAPE_CONE      3

/*--------------------------------------------------------------------------
 *  分类结果
 *--------------------------------------------------------------------------*/
typedef struct {
    uint8_t color_id;       /* COLOR_* */
    uint8_t shape_id;       /* SHAPE_* */
    uint8_t size_cm_x10;    /* 0.1 cm：20=2.0cm, 25=2.5cm, 30=3.0cm */
    uint8_t confidence;     /* 0-255，越低越不确定 */
} vision_result_t;

/*--------------------------------------------------------------------------
 *  分类器配置（可从 param_table 加载，也可用默认值）
 *--------------------------------------------------------------------------*/
typedef struct {
    /* 颜色：最小面积阈值（低于此值视为无该颜色） */
    uint32_t min_red_area;
    uint32_t min_blue_area;
    uint32_t min_yel_area;

    /* 排除法判定白/黑的门限 */
    float    white_luma_ratio;   /* (sum_rgb / bbox_area) 高于此视为白 */
    float    black_luma_ratio;   /* (sum_rgb / bbox_area) 低于此视为黑 */

    /* 形状：宽高比范围（x1000 定点） */
    uint16_t cube_ratio_lo;      /* 典型 850 */
    uint16_t cube_ratio_hi;      /* 典型 1150 */
    uint16_t cyl_ratio_lo;       /* 典型 700 */
    uint16_t cyl_ratio_hi;       /* 典型 1400（cone 在余下区间） */

    /* 形状：填充率下限（有色像素 / bbox 总面积），低于此视为 cone */
    float    cube_fill_lo;
    float    cyl_fill_lo;

    /* 尺寸：Cam1 高度→cm 查表（需标定，见 size_calibration.c） */
    uint16_t height_px_20mm;     /* 2.0cm 物块在侧面图里的像素高度 */
    uint16_t height_px_25mm;
    uint16_t height_px_30mm;

    /* 多帧滤波：稳定窗口大小 */
    uint8_t  filter_window;
    uint8_t  filter_confirm;     /* 窗口内需≥此数一致才输出 */
} classifier_cfg_t;

/*--------------------------------------------------------------------------
 *  多帧滤波器
 *--------------------------------------------------------------------------*/
typedef struct {
    vision_result_t history[8];
    vision_result_t stable;      /* 当前稳定输出 */
    uint8_t         write_idx;
    uint8_t         stable_age;  /* 稳定结果持续帧数 */
} mf_filter_t;

/*--------------------------------------------------------------------------
 *  API
 *--------------------------------------------------------------------------*/

/* 用保守默认值初始化分类器配置（现场标定前可用） */
void classifier_cfg_default(classifier_cfg_t *cfg);

/* 单帧分类：根据 FPGA 特征快照输出颜色/形状/尺寸。
 * cam: 0=俯视(颜色+形状), 1=侧面(颜色+形状+尺寸,需 height_px) */
vision_result_t classify_frame(const feature_snapshot_t *snap, int cam,
                               const classifier_cfg_t *cfg);

/* 多帧滤波器：喂入单帧结果，判定当前窗口是否可靠。
 *
 * 行为：
 *  - 窗口内最高票数 ≥ filter_confirm → 返回确认结果，更新内部 stable
 *  - 窗口内最高票数 < filter_confirm → 返回全零（color_id=COLOR_UNKNOWN,
 *    shape_id=SHAPE_UNKNOWN），表示"当前帧不可靠"
 *
 * 调用侧据此判断：
 *  - color_id == COLOR_UNKNOWN → 当前帧未稳定，自行滑行或保持上一帧决策
 *  - color_id != COLOR_UNKNOWN → 当前帧结果已通过窗口确认 */
vision_result_t mf_filter_update(mf_filter_t *f, const vision_result_t *raw,
                                 const classifier_cfg_t *cfg);

/* 重置滤波器（切换目标 / 模式时调用） */
void mf_filter_reset(mf_filter_t *f);

/* 融合两路结果：cam0 颜色+形状, cam1 尺寸 → 最终决策 */
vision_result_t fuse_results(const vision_result_t *cam0,
                             const vision_result_t *cam1,
                             const classifier_cfg_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* VISION_CLASSIFIER_H */
