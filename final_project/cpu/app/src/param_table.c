/*==========================================================================
 *  param_table.c  —  分类器参数表管理 实现
 *
 *  双槽 RAM 存储 + 可选 NVM 持久化 + 参数合法性校验。
 *==========================================================================*/

#include "param_table.h"
#include <string.h>

/*==========================================================================
 *  内部状态
 *==========================================================================*/

static classifier_cfg_t  g_slots[PARAM_SLOT_COUNT];
static int               g_active_slot    = PARAM_SLOT_DEFAULT;
static int               g_calibrated     = 0;  /* slot 1 是否有有效数据 */
static int               g_calibrating    = 0;  /* 是否处于标定模式中 */

/*==========================================================================
 *  NVM 弱引用 —— 用户如需持久化，链接时提供同名强符号覆盖
 *==========================================================================*/

#if PARAM_TABLE_NVM_AVAILABLE
/* 用户必须提供这两个函数 */
extern int param_table_nvm_read (uint32_t offset, void *buf, uint32_t len);
extern int param_table_nvm_write(uint32_t offset, const void *buf, uint32_t len);

/* NVM 中存储的标定块头部 */
#define NVM_MAGIC           0x504D5450u   /* "PTMP" */
#define NVM_BLOB_OFFSET     0x00001000u   /* NVM 基地址偏移（留给其他数据） */

typedef struct {
    uint32_t magic;
    uint32_t crc32;
    uint32_t version;
    uint32_t _reserved;
    classifier_cfg_t cfg;
} nvm_calib_blob_t;

static uint32_t _crc32(const void *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t *p = (const uint8_t *)data;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            uint32_t mask = (crc & 1u) ? 0xFFFFFFFFu : 0u;
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}
#endif /* PARAM_TABLE_NVM_AVAILABLE */

/*==========================================================================
 *  参数合法性校验
 *==========================================================================*/

int param_table_validate(const classifier_cfg_t *cfg)
{
    /* ---- 颜色阈值 ---- */
    /* min_*_area 是 uint32_t，只需确保不会溢出到不合理范围。
     * 上限设 10M 像素（远超 1080p 全帧）作为防御。 */
    if (cfg->min_red_area  > 10000000u) return PARAM_ERR_THRESHOLD;
    if (cfg->min_blue_area > 10000000u) return PARAM_ERR_THRESHOLD;
    if (cfg->min_yel_area  > 10000000u) return PARAM_ERR_THRESHOLD;

    /* ---- 白/黑排除法亮度比 ---- */
    if (cfg->white_luma_ratio < 0.0f || cfg->white_luma_ratio > 1.0f)
        return PARAM_ERR_LUMA_RATIO;
    if (cfg->black_luma_ratio < 0.0f || cfg->black_luma_ratio > 1.0f)
        return PARAM_ERR_LUMA_RATIO;
    if (cfg->white_luma_ratio <= cfg->black_luma_ratio)
        return PARAM_ERR_LUMA_RATIO;   /* 白阈值必须高于黑阈值 */

    /* ---- 宽高比范围 ---- */
    if (cfg->cube_ratio_lo == 0 || cfg->cube_ratio_hi == 0)
        return PARAM_ERR_RATIO_RANGE;
    if (cfg->cube_ratio_lo >= cfg->cube_ratio_hi)
        return PARAM_ERR_RATIO_RANGE;
    if (cfg->cyl_ratio_lo == 0 || cfg->cyl_ratio_hi == 0)
        return PARAM_ERR_RATIO_RANGE;
    if (cfg->cyl_ratio_lo >= cfg->cyl_ratio_hi)
        return PARAM_ERR_RATIO_RANGE;
    /* 范围合理性：x1000 定点，典型 500-2000 */
    if (cfg->cube_ratio_lo < 500  || cfg->cube_ratio_hi > 2000)
        return PARAM_ERR_RATIO_RANGE;
    if (cfg->cyl_ratio_lo  < 400  || cfg->cyl_ratio_hi  > 2000)
        return PARAM_ERR_RATIO_RANGE;

    /* ---- 填充率 ---- */
    if (cfg->cube_fill_lo < 0.50f || cfg->cube_fill_lo > 0.99f)
        return PARAM_ERR_FILL_RANGE;
    if (cfg->cyl_fill_lo  < 0.30f || cfg->cyl_fill_lo  > 0.85f)
        return PARAM_ERR_FILL_RANGE;
    if (cfg->cyl_fill_lo >= cfg->cube_fill_lo)
        return PARAM_ERR_FILL_RANGE;   /* 圆填充率必须低于方填充率 */

    /* ---- 尺寸查表 ---- */
    if (cfg->height_px_20mm < 5 || cfg->height_px_20mm > 4095)
        return PARAM_ERR_SIZE_TABLE;
    if (cfg->height_px_25mm < 5 || cfg->height_px_25mm > 4095)
        return PARAM_ERR_SIZE_TABLE;
    if (cfg->height_px_30mm < 5 || cfg->height_px_30mm > 4095)
        return PARAM_ERR_SIZE_TABLE;
    /* 必须单调递增：20mm < 25mm < 30mm */
    if (cfg->height_px_20mm >= cfg->height_px_25mm)
        return PARAM_ERR_SIZE_TABLE;
    if (cfg->height_px_25mm >= cfg->height_px_30mm)
        return PARAM_ERR_SIZE_TABLE;

    /* ---- 滤波窗口 ---- */
    if (cfg->filter_window < 1 || cfg->filter_window > 8)
        return PARAM_ERR_WINDOW;
    if (cfg->filter_confirm < 1 || cfg->filter_confirm > 8)
        return PARAM_ERR_WINDOW;
    if (cfg->filter_confirm > cfg->filter_window)
        return PARAM_ERR_WINDOW;

    return PARAM_OK;
}

/*==========================================================================
 *  公共 API
 *==========================================================================*/

void param_table_init(void)
{
    /* slot 0: 始终用保守默认值 */
    classifier_cfg_default(&g_slots[PARAM_SLOT_DEFAULT]);

    /* slot 1: 清零，等待标定 */
    memset(&g_slots[PARAM_SLOT_CALIBRATED], 0,
           sizeof(g_slots[PARAM_SLOT_CALIBRATED]));

    g_active_slot = PARAM_SLOT_DEFAULT;
    g_calibrated  = 0;
    g_calibrating = 0;

    /* 尝试加载 NVM 标定值 */
    if (PARAM_TABLE_NVM_AVAILABLE) {
        int rc = param_table_load_calibrated();
        if (rc == 0) {
            g_active_slot = PARAM_SLOT_CALIBRATED;
        }
    }
}

const classifier_cfg_t *param_table_get(void)
{
    return &g_slots[g_active_slot];
}

int param_table_set(int slot, const classifier_cfg_t *cfg)
{
    if (slot < 0 || slot >= PARAM_SLOT_COUNT)
        return PARAM_ERR_WINDOW;   /* 任何非零错误码均可 */

    int err = param_table_validate(cfg);
    if (err != PARAM_OK)
        return err;

    memcpy(&g_slots[slot], cfg, sizeof(classifier_cfg_t));

    if (slot == PARAM_SLOT_CALIBRATED) {
        g_calibrated   = 1;
        g_active_slot  = PARAM_SLOT_CALIBRATED;
    } else {
        /* slot 0 写入不切换活跃槽（已有标定值时优先保留标定值） */
        if (!g_calibrated) {
            g_active_slot = PARAM_SLOT_DEFAULT;
        }
    }

    return PARAM_OK;
}

int param_table_load_calibrated(void)
{
    if (!PARAM_TABLE_NVM_AVAILABLE)
        return -1;

#if PARAM_TABLE_NVM_AVAILABLE
    nvm_calib_blob_t blob;
    int rc = param_table_nvm_read(NVM_BLOB_OFFSET, &blob, sizeof(blob));
    if (rc != 0)
        return -1;

    /* 校验魔数 */
    if (blob.magic != NVM_MAGIC)
        return -1;

    /* 校验 CRC（覆盖 cfg 字段，不包括 header） */
    uint32_t calc_crc = _crc32(&blob.cfg, sizeof(blob.cfg));
    if (calc_crc != blob.crc32)
        return -1;

    /* 校验参数合法性 */
    int err = param_table_validate(&blob.cfg);
    if (err != PARAM_OK)
        return -1;

    /* 写入 slot 1 */
    memcpy(&g_slots[PARAM_SLOT_CALIBRATED], &blob.cfg,
           sizeof(classifier_cfg_t));
    g_calibrated  = 1;
    g_active_slot = PARAM_SLOT_CALIBRATED;

    return 0;
#else
    return -1;
#endif
}

int param_table_save_calibrated(void)
{
    if (!PARAM_TABLE_NVM_AVAILABLE)
        return -1;

    if (!g_calibrated)
        return -1;   /* slot 1 没数据，拒绝写入垃圾 */

#if PARAM_TABLE_NVM_AVAILABLE
    nvm_calib_blob_t blob;
    memset(&blob, 0, sizeof(blob));

    blob.magic   = NVM_MAGIC;
    blob.version = 1;
    memcpy(&blob.cfg, &g_slots[PARAM_SLOT_CALIBRATED],
           sizeof(classifier_cfg_t));
    blob.crc32 = _crc32(&blob.cfg, sizeof(blob.cfg));

    int rc = param_table_nvm_write(NVM_BLOB_OFFSET, &blob, sizeof(blob));
    return (rc == 0) ? 0 : -1;
#else
    return -1;
#endif
}

void param_table_enter_calibration(void)
{
    /* 复制当前活跃配置到 slot 1 作为标定起点 */
    memcpy(&g_slots[PARAM_SLOT_CALIBRATED],
           &g_slots[g_active_slot],
           sizeof(classifier_cfg_t));

    g_calibrated  = 1;
    g_calibrating = 1;
    g_active_slot = PARAM_SLOT_CALIBRATED;
}

void param_table_exit_calibration(void)
{
    g_calibrating = 0;
}

int param_table_is_calibrated(void)
{
    return g_calibrated;
}

const char *param_table_strerror(int err)
{
    switch (err) {
    case PARAM_OK:              return "OK";
    case PARAM_ERR_RATIO_RANGE: return "aspect ratio out of range";
    case PARAM_ERR_FILL_RANGE:  return "fill rate threshold out of range";
    case PARAM_ERR_SIZE_TABLE:  return "size lookup table invalid";
    case PARAM_ERR_THRESHOLD:   return "color threshold out of range";
    case PARAM_ERR_WINDOW:      return "filter window/confirm invalid";
    case PARAM_ERR_LUMA_RATIO:  return "white/black luma ratio invalid";
    default:                    return "unknown error";
    }
}
