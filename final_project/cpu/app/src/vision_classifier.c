/*==========================================================================
 *  vision_classifier.c  —  颜色 / 形状 / 尺寸分类 实现
 *
 *  输入：feature_snapshot_t（FPGA LIVE_* 寄存器快照）
 *  输出：vision_result_t（颜色 + 形状 + 尺寸 + 置信度）
 *
 *  算法按 CPU_MODULE_PLAN §2：
 *    1. 颜色 — 三色面积最大值 + 白/黑排除法
 *    2. 形状 — 分摄像头角色判填充率 + 双路融合
 *    3. 尺寸 — Cam1 height_px 查表
 *    4. 滤波 — 滑动窗口多数投票
 *==========================================================================*/

#include "vision_classifier.h"
#include <string.h>

/*==========================================================================
 *  内部 helper — bbox 解包与面积
 *==========================================================================*/

/* bbox_min 和 bbox_max 寄存器使用相同的位域编码：
 *   [15:0]  = X 坐标（低 16-bit）
 *   [31:16] = Y 坐标（高 16-bit）
 * 因此 _x_min 和 _x_max 函数体相同、_y_min 和 _y_max 函数体相同，
 * 不是复制粘贴错误 — 两者只是从不同寄存器解出各自的 X/Y 分量。 */
static inline uint16_t _bbox_x_min(uint32_t v) { return v & 0xFFFFu; }
static inline uint16_t _bbox_y_min(uint32_t v) { return (v >> 16) & 0xFFFFu; }
static inline uint16_t _bbox_x_max(uint32_t v) { return v & 0xFFFFu; }
static inline uint16_t _bbox_y_max(uint32_t v) { return (v >> 16) & 0xFFFFu; }

static uint32_t _bbox_area(const feature_snapshot_t *snap)
{
    uint32_t w = _bbox_x_max(snap->bbox_max) - _bbox_x_min(snap->bbox_min);
    uint32_t h = _bbox_y_max(snap->bbox_max) - _bbox_y_min(snap->bbox_min);
    uint32_t a = w * h;
    return a ? a : 1u;   /* guard div-by-zero */
}

/*==========================================================================
 *  内部 helper — 前景填充率
 *  FG_AREA_AVAILABLE=1 用硬件前景计数；否则用 R+G+B 近似
 *==========================================================================*/

static float _fill_rate(const feature_snapshot_t *snap)
{
    uint32_t area = _bbox_area(snap);
#if FG_AREA_AVAILABLE
    if (snap->fg_area > 0) {
        float fr = (snap->fg_area > area)
                 ? 1.0f
                 : (float)snap->fg_area / (float)area;
        return fr;
    }
#endif
    uint32_t total = snap->red_area + snap->blue_area + snap->yel_area;
    if (total > area) total = area;   /* clamp 防止计数溢出 */
    return (float)total / (float)area;
}

/*==========================================================================
 *  内部 helper — 颜色分类
 *
 *  策略：
 *    1. 找 R/G/B 面积最大者
 *    2. 若 ≥ 最低阈值 且 显著领先次大 → 该颜色
 *    3. 若 ≥ 最低阈值 但优势不明显 → 仍为该颜色，降置信度
 *    4. 全低于阈值 → 用排除法区分为白/黑
 *
 *  白/黑排除法已知限制（需 FG_AREA_AVAILABLE=1 或 FPGA 升级解决）：
 *    - 当前 FG_AREA_AVAILABLE=0 时，用 R+G+B 面积之和近似前景面积。
 *      这不是亮度/明度，而是 FPGA 颜色分割后"被归为红/蓝/黄的像素总数"。
 *    - 白色物块几乎不含饱和色 → R/G/B 各自都低 → 总和也低 →
 *      排除法中的 fr 值低 → 容易误判为 BLACK 或 UNKNOWN。
 *    - 黑色物块同理：R/G/B 各自都低 → 总和低 → 排除法正常判为 BLACK。
 *    - 结论：FG_AREA_AVAILABLE=0 时，白色检测不可靠。
 *      需 FPGA 提供 LIVE_FG_AREA（bbox 内前景像素总数，不限颜色）才能
 *      准确区分"大面积白色前景"和"大面积无前景区域"。
 *
 *  现场标定建议：
 *    - 优先推动 FPGA 队友实现 LIVE_FG_AREA 寄存器
 *    - 若短期内无法实现，可临时调高 white_luma_ratio、调低 min_*_area，
 *      让白色物块的微弱色彩分量也能进入排除法路径
 *==========================================================================*/

static uint8_t _classify_color(const feature_snapshot_t *snap,
                               const classifier_cfg_t *cfg,
                               uint8_t *conf)
{
    uint32_t r = snap->red_area;
    uint32_t b = snap->blue_area;
    uint32_t y = snap->yel_area;

    /* 找最大 & 次大 */
    uint32_t max_v = r, sec_v = 0;
    uint8_t  max_c = COLOR_RED;

    if (b > max_v) { sec_v = max_v; max_v = b; max_c = COLOR_BLUE; }
    else           { sec_v = b; }

    if (y > max_v) { sec_v = max_v; max_v = y; max_c = COLOR_YELLOW; }
    else if (y > sec_v) { sec_v = y; }

    /* 判定阈值 */
    uint32_t th = cfg->min_red_area;
    if (max_c == COLOR_BLUE)   th = cfg->min_blue_area;
    if (max_c == COLOR_YELLOW) th = cfg->min_yel_area;

    if (max_v >= th) {
        /* 有颜色候选 — 检查是否真正占主导 */
        if (sec_v == 0 || max_v >= sec_v * 2u) {
            *conf = 220;                   /* 明确主导色 */
        } else {
            *conf = 140;                   /* 多色混杂，可能白背景干扰 */
        }
        return max_c;
    }

    /* ---- 全部低于阈值 → 白/黑排除法 ---- */
    /* 复用 _fill_rate()：当 FG_AREA_AVAILABLE=1 时优先用硬件前景计数
     * （fg_area/bbox_area，含 clamp），否则降级为 R+G+B 近似。
     * 白色物块：R/G/B 各自低但 fg_area 高 → ratio 高 → WHITE
     * 黑色物块：R/G/B 各自低且 fg_area 低 → ratio 低 → BLACK
     * FG_AREA_AVAILABLE=0 时白色检测不可靠（见文件头注释）。*/
    float ratio = _fill_rate(snap);

    if (ratio > cfg->white_luma_ratio) {
        *conf = 130;
        return COLOR_WHITE;
    }
    if (ratio < cfg->black_luma_ratio) {
        *conf = 130;
        return COLOR_BLACK;
    }

    *conf = 30;
    return COLOR_UNKNOWN;
}

/*==========================================================================
 *  内部 helper — 形状分类（单摄）
 *
 *  Cam0 俯视：填充率高→方(SHAPE_CUBE)，填充率 ≈78%→圆(SHAPE_CYLINDER)
 *              （圆柱和圆锥从顶部看都是圆的）
 *  Cam1 侧面：填充率高→方(SHAPE_CUBE)，填充率低→三角(SHAPE_CONE)
 *
 *  返回的 shape_id 是单摄视角下的形状；双路融合在 fuse_results() 完成。
 *==========================================================================*/

static uint8_t _classify_shape(const feature_snapshot_t *snap, int cam,
                               const classifier_cfg_t *cfg,
                               uint8_t *conf)
{
    float fill = _fill_rate(snap);

    if (fill <= 0.0f || _bbox_area(snap) <= 1u) {
        *conf = 0;
        return SHAPE_UNKNOWN;
    }

    if (cam == 0) {
        /* ---- Cam0 俯视：方 vs 圆 ---- */
        if (fill >= cfg->cube_fill_lo) {
            /* 填充率高 → 方形 */
            float t = (fill - cfg->cube_fill_lo) / (1.0f - cfg->cube_fill_lo + 0.001f);
            if (t > 1.0f) t = 1.0f;
            *conf = (uint8_t)(55u + (uint8_t)(200.0f * t));
            return SHAPE_CUBE;       /* 俯视方形 → 可能是 cube */
        } else if (fill >= cfg->cyl_fill_lo) {
            /* 填充率中 → 圆形 */
            float t = (fill - cfg->cyl_fill_lo) / (cfg->cube_fill_lo - cfg->cyl_fill_lo + 0.001f);
            if (t > 1.0f) t = 1.0f;
            *conf = (uint8_t)(55u + (uint8_t)(200.0f * t));
            return SHAPE_CYLINDER;   /* 俯视圆形 → 可能是 cylinder 或 cone */
        } else {
            *conf = 30;
            return SHAPE_UNKNOWN;
        }
    } else {
        /* ---- Cam1 侧面：方 vs 三角 ---- */
        if (fill >= cfg->cube_fill_lo) {
            /* 填充率高 → 方形 */
            float t = (fill - cfg->cube_fill_lo) / (1.0f - cfg->cube_fill_lo + 0.001f);
            if (t > 1.0f) t = 1.0f;
            *conf = (uint8_t)(55u + (uint8_t)(200.0f * t));
            return SHAPE_CUBE;       /* 侧面方形 → 可能是 cube 或 cylinder */
        } else if (fill >= cfg->cone_fill_lo) {
            /* 填充率偏低 → 三角形（锥形侧面） */
            /* 离 cube_fill_lo 越远，越像三角 */
            float dist = cfg->cube_fill_lo - fill;
            float t = dist / (cfg->cube_fill_lo - cfg->cone_fill_lo + 0.001f);
            if (t > 1.0f) t = 1.0f;
            *conf = (uint8_t)(55u + (uint8_t)(200.0f * t));
            return SHAPE_CONE;       /* 侧面三角 → 可能是 cone */
        } else {
            *conf = 30;
            return SHAPE_UNKNOWN;
        }
    }
}

/*==========================================================================
 *  内部 helper — 尺寸分类（Cam1 侧面高度 → 0.1cm 查表）
 *==========================================================================*/

static uint8_t _classify_size(uint16_t height_px, const classifier_cfg_t *cfg)
{
    if (height_px == 0) return 0;

    /* 找最近的标准高度 */
    uint32_t d20 = (height_px > cfg->height_px_20mm)
                 ? (uint32_t)(height_px - cfg->height_px_20mm)
                 : (uint32_t)(cfg->height_px_20mm - height_px);
    uint32_t d25 = (height_px > cfg->height_px_25mm)
                 ? (uint32_t)(height_px - cfg->height_px_25mm)
                 : (uint32_t)(cfg->height_px_25mm - height_px);
    uint32_t d30 = (height_px > cfg->height_px_30mm)
                 ? (uint32_t)(height_px - cfg->height_px_30mm)
                 : (uint32_t)(cfg->height_px_30mm - height_px);

    if (d20 <= d25 && d20 <= d30) return 20;   /* 2.0 cm */
    if (d25 <= d20 && d25 <= d30) return 25;   /* 2.5 cm */
    return 30;                                   /* 3.0 cm */
}

/*==========================================================================
 *  Public API
 *==========================================================================*/

/*--------------------------------------------------------------------------
 *  默认分类器配置（保守值，现场标定前可用）
 *--------------------------------------------------------------------------*/
void classifier_cfg_default(classifier_cfg_t *cfg)
{
    if (cfg == 0)
        return;

    /* 颜色面积阈值 — 全帧像素数，需现场标定 */
    cfg->min_red_area  = 500;
    cfg->min_blue_area = 500;
    cfg->min_yel_area  = 500;

    /* 白/黑排除法比率 */
    cfg->white_luma_ratio = 0.55f;
    cfg->black_luma_ratio = 0.10f;

    /* 宽高比范围（定点 x1000）— 暂未启用，保留给现场标定 */
    cfg->cube_ratio_lo = 850;
    cfg->cube_ratio_hi = 1150;
    cfg->cyl_ratio_lo  = 700;
    cfg->cyl_ratio_hi  = 1400;

    /* 填充率门限 */
    cfg->cube_fill_lo = 0.85f;    /* 方形 bbox 内 ≥85% 有色像素 */
    /* 圆形填充率理论值 π/4≈0.785。默认 0.70 比理论值低约 8%，
     * 作为余量吸收 bbox 不精确、镜头畸变、边缘像素噪声等误差。
     * 现场标定时可回推到接近 0.78，或根据实测误判率微调。 */
    cfg->cyl_fill_lo  = 0.70f;
    cfg->cone_fill_lo = 0.25f;    /* Cam1 triangle lower bound */

    /* 侧面高度 → 尺寸查表（像素值，需现场标定） */
    cfg->height_px_20mm = 100;
    cfg->height_px_25mm = 125;
    cfg->height_px_30mm = 150;

    /* 多帧滤波 */
    cfg->filter_window  = 5;
    cfg->filter_confirm = 3;
}

/*--------------------------------------------------------------------------
 *  单帧分类
 *--------------------------------------------------------------------------*/
vision_result_t classify_frame(const feature_snapshot_t *snap, int cam,
                               const classifier_cfg_t *cfg)
{
    vision_result_t r;
    memset(&r, 0, sizeof(r));

    if (snap == 0 || cfg == 0 || !VALID_CAM(cam))
        return r;

    uint8_t col_conf = 0;
    uint8_t shp_conf = 0;

    r.color_id = _classify_color(snap, cfg, &col_conf);
    r.shape_id = _classify_shape(snap, cam, cfg, &shp_conf);

    if (cam == 1) {
        r.size_cm_x10 = _classify_size((uint16_t)snap->height_px, cfg);
    }

    /* 综合置信度：颜色 & 形状取几何平均 */
    r.confidence = (uint8_t)(((uint16_t)col_conf * (uint16_t)shp_conf) / 255u);

    return r;
}

/*==========================================================================
 *  多帧滤波器
 *
 *  环形缓冲 + 多数投票：
 *   - 窗口内有 ≥ filter_confirm 票的颜色/形状组合 → 确认输出
 *   - 票数不足 → 返回全零（COLOR_UNKNOWN），调用方自行滑行
 *==========================================================================*/

void mf_filter_reset(mf_filter_t *f)
{
    if (f == 0)
        return;

    memset(f, 0, sizeof(*f));
}

vision_result_t mf_filter_update(mf_filter_t *f, const vision_result_t *raw,
                                 const classifier_cfg_t *cfg)
{
    static const vision_result_t zero_result;   /* all-zero */

    if (f == 0 || raw == 0 || cfg == 0)
        return zero_result;

    uint8_t w = cfg->filter_window;
    if (w > 8)  w = 8;    /* history[] 容量上限 */
    if (w == 0) w = 5;    /* 防御 */

    /* 写入环形缓冲 */
    f->history[f->write_idx] = *raw;
    f->write_idx = (f->write_idx + 1u) % w;

    /* 多数投票：遍历缓冲，统计每种 (color, shape, size) 出现次数。
     * 尺寸参与投票，防止同色同形但尺寸抖动时输出随机尺寸。 */
    uint8_t  best_votes = 0;
    uint8_t  best_color = COLOR_UNKNOWN;
    uint8_t  best_shape = SHAPE_UNKNOWN;
    uint8_t  best_size  = 0;

    for (uint8_t i = 0; i < w; i++) {
        vision_result_t *cand = &f->history[i];
        if (cand->color_id == COLOR_UNKNOWN) continue;

        uint8_t votes = 0;
        for (uint8_t j = 0; j < w; j++) {
            if (f->history[j].color_id == cand->color_id &&
                f->history[j].shape_id == cand->shape_id &&
                f->history[j].size_cm_x10 == cand->size_cm_x10) {
                votes++;
            }
        }
        if (votes > best_votes) {
            best_votes = votes;
            best_color = cand->color_id;
            best_shape = cand->shape_id;
            best_size  = cand->size_cm_x10;
        }
    }

    uint8_t confirm = cfg->filter_confirm;
    if (confirm == 0) confirm = 3;     /* default first, then clamp to window */
    if (confirm > w) confirm = w;

    if (best_votes >= confirm) {
        f->stable.color_id  = best_color;
        f->stable.shape_id  = best_shape;
        f->stable.size_cm_x10 = best_size;
        f->stable.confidence = (uint8_t)((uint16_t)best_votes * 255u / (uint16_t)w);
        f->stable_age++;
        return f->stable;
    }

    /* 票数不足 → 返回全零，不更新 stable。
     * 调用方检查 color_id==0 自行决定：滑行上一帧 / 保持旧决策 */
    return zero_result;
}

/*==========================================================================
 *  双路融合
 *
 *  融合规则（来自 CPU_MODULE_PLAN §2 步骤 3）：
 *    俯视=方 + 侧面=方 → CUBE
 *    俯视=圆 + 侧面=方 → CYLINDER
 *    俯视=圆 + 侧面=三角 → CONE
 *
 *  颜色以 Cam0 俯视为准（向下看物体顶面更准）
 *  尺寸以 Cam1 侧面为准
 *==========================================================================*/

vision_result_t fuse_results(const vision_result_t *cam0,
                             const vision_result_t *cam1,
                             const classifier_cfg_t *cfg)
{
    (void)cfg;    /* 保留接口，未来可能用 cfg 做合法性检查 */

    vision_result_t fused;
    memset(&fused, 0, sizeof(fused));

    /* ---- 单路退化：一侧为 NULL 时直接返回另一侧 ---- */
    if (cam0 == 0 && cam1 == 0) {
        return fused;   /* 两路都空 → 全零 */
    }
    if (cam0 == 0) {
        /* 只有侧面：颜色/形状/尺寸均来自 Cam1 */
        fused.color_id   = cam1->color_id;
        fused.shape_id   = cam1->shape_id;
        fused.size_cm_x10 = cam1->size_cm_x10;
        fused.confidence = cam1->confidence;
        return fused;
    }
    if (cam1 == 0) {
        /* 只有俯视：颜色/形状来自 Cam0，尺寸无（Cam0 不算尺寸） */
        fused.color_id   = cam0->color_id;
        fused.shape_id   = cam0->shape_id;
        fused.size_cm_x10 = 0;
        fused.confidence = cam0->confidence;
        return fused;
    }

    /* ---- 双路融合 ---- */
    uint8_t s0 = cam0->shape_id;
    uint8_t s1 = cam1->shape_id;

    /* 形状融合（CPU_MODULE_PLAN §2 步骤 3）：
     *   俯视=方 + 侧面=方 → CUBE
     *   俯视=圆 + 侧面=方 → CYLINDER
     *   俯视=圆 + 侧面=三角 → CONE
     * Cam0 侧的 SHAPE_CYLINDER 含义是"俯视圆形，可能是圆柱也可能是圆锥"。
     *
     * 不在上述三种有效组合之列的情况（含任一 UNKNOWN / 其他形状 ID /
     * 无效组合如 CYLINDER+CYLINDER）→ 落入 else 分支，输出 SHAPE_UNKNOWN。
     * 这是有意为之：UNKNOWN 不单独分支处理，统一走 else fall-through。 */
    if (s0 == SHAPE_CUBE && s1 == SHAPE_CUBE) {
        fused.shape_id = SHAPE_CUBE;
    } else if (s0 == SHAPE_CYLINDER && s1 == SHAPE_CUBE) {
        fused.shape_id = SHAPE_CYLINDER;
    } else if (s0 == SHAPE_CYLINDER && s1 == SHAPE_CONE) {
        fused.shape_id = SHAPE_CONE;
    } else {
        fused.shape_id = SHAPE_UNKNOWN;
    }

    /* 颜色：Cam0 俯视优先（向下看顶面颜色更准） */
    if (cam0->color_id != COLOR_UNKNOWN) {
        fused.color_id = cam0->color_id;
    } else {
        fused.color_id = cam1->color_id;
    }

    /* 尺寸：Cam1 侧面为准 */
    fused.size_cm_x10 = cam1->size_cm_x10;

    /* 置信度：两路平均 */
    fused.confidence = (uint8_t)(((uint16_t)cam0->confidence +
                                  (uint16_t)cam1->confidence) / 2u);

    return fused;
}
