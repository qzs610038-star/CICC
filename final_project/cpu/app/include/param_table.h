/*==========================================================================
 *  param_table.h  —  分类器参数表管理
 *
 *  管理 classifier_cfg_t 的双槽切换、标定存储和合法性校验。
 *
 *  双槽设计：
 *    slot 0 (PARAM_SLOT_DEFAULT)     — 保守默认值，始终存在
 *    slot 1 (PARAM_SLOT_CALIBRATED)  — 现场标定值，可存 NVM
 *
 *  运行时 param_table_get() 返回标定值（若有），否则返回默认值。
 *
 *  NVM 持久化：
 *    当 PARAM_TABLE_NVM_AVAILABLE=1 时，需要用户在链接时提供：
 *      int param_table_nvm_read(uint32_t offset, void *buf, uint32_t len);
 *      int param_table_nvm_write(uint32_t offset, const void *buf, uint32_t len);
 *    默认为 0，所有标定值仅存 RAM，掉电丢失。
 *==========================================================================*/

#ifndef PARAM_TABLE_H
#define PARAM_TABLE_H

#include <stdint.h>
#include "vision_classifier.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 *  配置槽
 *--------------------------------------------------------------------------*/
#define PARAM_SLOT_DEFAULT     0
#define PARAM_SLOT_CALIBRATED  1
#define PARAM_SLOT_COUNT       2

/*--------------------------------------------------------------------------
 *  校验返回值
 *--------------------------------------------------------------------------*/
#define PARAM_OK               0
#define PARAM_ERR_RATIO_RANGE  1   /* 宽高比范围不合理 */
#define PARAM_ERR_FILL_RANGE   2   /* 填充率门限不合理 */
#define PARAM_ERR_SIZE_TABLE   3   /* 尺寸查表值不合理 */
#define PARAM_ERR_THRESHOLD    4   /* 颜色阈值不合理 */
#define PARAM_ERR_WINDOW       5   /* 滤波窗口/确认数不合理 */
#define PARAM_ERR_LUMA_RATIO   6   /* 白/黑亮度比不合理 */

/*--------------------------------------------------------------------------
 *  NVM 编译开关
 *--------------------------------------------------------------------------*/
#ifndef PARAM_TABLE_NVM_AVAILABLE
#  define PARAM_TABLE_NVM_AVAILABLE  0
#endif

/*--------------------------------------------------------------------------
 *  API
 *--------------------------------------------------------------------------*/

/* 初始化：加载默认值到 slot 0；若 NVM 可用且有有效标定数据则加载 slot 1。
 * 必须在 param_table_get() 之前调用一次。 */
void param_table_init(void);

/* 返回当前活跃配置的只读指针（优先标定值，否则默认值）。
 * 调用方不可修改返回内容；用 param_table_set() 更新。 */
const classifier_cfg_t *param_table_get(void);

/* 将 cfg 写入指定 slot（仅 RAM），先校验再写入。
 * 返回 PARAM_OK 或错误码。slot 0（默认）也可被覆盖但不推荐。 */
int param_table_set(int slot, const classifier_cfg_t *cfg);

/* 从 NVM 加载标定参数到 slot 1。
 * PARAM_TABLE_NVM_AVAILABLE=0 时直接返回 -1。
 * 返回 0 成功，-1 失败（不存在/校验错误/NVM 不可用）。 */
int param_table_load_calibrated(void);

/* 将当前 slot 1 标定参数保存到 NVM。
 * PARAM_TABLE_NVM_AVAILABLE=0 时直接返回 -1。
 * 返回 0 成功，-1 失败。 */
int param_table_save_calibrated(void);

/* 进入标定模式：复制当前活跃配置到 slot 1，标记标定进行中。
 * 后续 param_table_get() 将返回 slot 1，param_table_set(1, ...) 可逐步调整。 */
void param_table_enter_calibration(void);

/* 退出标定模式：清除标定进行中标志。
 * 已写入 slot 1 的标定值保留，param_table_get() 继续返回 slot 1。
 * 调用时机：标定完成、切换回正常运行模式时。 */
void param_table_exit_calibration(void);

/* 独立校验 cfg 是否合法，不修改内部状态。
 * 返回 PARAM_OK 或错误码。 */
int param_table_validate(const classifier_cfg_t *cfg);

/* 是否已有有效标定值（slot 1 是否曾被 param_table_set(1,...) 或
 * param_table_load_calibrated() 成功写入）。 */
int param_table_is_calibrated(void);

/* 错误码 → 可读描述字符串 */
const char *param_table_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif /* PARAM_TABLE_H */
