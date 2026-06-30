/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    diag.h
 * @brief   BMS 诊断中心接口。
 * @ingroup SYS
 */

#ifndef BMS_DIAG_H_
#define BMS_DIAG_H_

#include <stdbool.h>
#include <stdint.h>

#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 诊断条目 id（同时用作位掩码的 bit 序号）。 */
enum bms_diag_id {
	BMS_DIAG_INVALID_MEAS = 0,  /**< 测量无效/超界 */
	BMS_DIAG_PROTECTION_ACTIVE, /**< 保护判定为非 NORMAL */
	BMS_DIAG_TASK_OVERRUN,      /**< 任务超期/超时 */
	BMS_DIAG_COUNT,             /**< 诊断条目数量（哨兵，非具体条目） */
};

/** 诊断严重度（递增）。 */
enum bms_diag_severity {
	BMS_DIAG_INFO = 0, /**< 信息，无需动作 */
	BMS_DIAG_WARNING,  /**< 警告 */
	BMS_DIAG_ERROR,    /**< 错误 */
	BMS_DIAG_CRITICAL, /**< 严重，安全相关 */
};

/** 诊断中心状态快照。 */
struct bms_diag_state {
	uint32_t timestamp_ms;               /**< 最近一次更新时刻（ms） */
	uint32_t active_mask;                /**< 当前激活诊断位掩码（bit = bms_diag_id） */
	uint32_t latched_mask;               /**< 已锁存诊断位掩码 */
	enum bms_diag_severity max_severity; /**< 当前激活项中的最高严重度 */
};

/**
 * @brief 初始化诊断中心（清空激活/锁存状态）。
 * @return 0 成功。
 */
int bms_diag_init(void);

/**
 * @brief 上报一条诊断条目，更新激活/锁存掩码与最高严重度（线程安全）。
 * @param id       诊断条目 id
 * @param severity 严重度
 * @param active   true=该条目当前激活，false=清除其激活位
 * @param latch    激活时是否锁存（仅 @p active 为 true 时有效）
 * @return 0 成功；@p id 越界返回 -EINVAL。
 */
int bms_diag_report(enum bms_diag_id id, enum bms_diag_severity severity, bool active, bool latch);

/**
 * @brief 读取诊断状态快照（线程安全）。
 * @param[out] out 输出诊断状态（非空）。
 * @return 0 成功；@p out 为 NULL 返回 -EINVAL。
 */
int bms_diag_get_state(struct bms_diag_state *out);

/**
 * @brief 是否存在已锁存故障，或当前最高严重度达到 ERROR 及以上。
 * @return true 表示存在错误。
 */
bool bms_diag_has_error(void);

#ifdef __cplusplus
}
#endif

#endif /* BMS_DIAG_H_ */
