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
	BMS_DIAG_MEAS_STALE,        /**< 测量过期/源失联（valid 但时间戳超容忍） */
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
 * @brief 上报一条诊断条目的原始判定，步进其生命周期并重算聚合状态（线程安全）。
 * @details severity/confirm/clear/latch 归条目登记表；调用方仅提供本周期原始判定
 *          与注入时间。内部经 @ref bms_diag_entry_step 步进后重算 active/latched 掩码
 *          与最高严重度。
 * @param id         诊断条目 id
 * @param raw_active 源模块本周期原始判定（true=失败/触发）
 * @param now_ms     注入的单调毫秒时间
 * @return 0 成功；@p id 越界返回 -EINVAL。
 */
int bms_diag_report(enum bms_diag_id id, bool raw_active, uint32_t now_ms);

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

/** 诊断条目生命周期状态。 */
enum bms_diag_life {
	BMS_DIAG_LIFE_INACTIVE = 0,
	BMS_DIAG_LIFE_CONFIRMING, /**< 原始已激活、等待 confirm_time 去抖 */
	BMS_DIAG_LIFE_ACTIVE,
	BMS_DIAG_LIFE_CLEARING, /**< 原始已消失、等待 clear_time 去抖 */
	BMS_DIAG_LIFE_LATCHED,  /**< 锁存：清除条件满足后仍保持，直至复位 */
};

/** 每条目静态配置（登记表项）。 */
struct bms_diag_entry_cfg {
	enum bms_diag_severity severity;
	uint32_t confirm_time_ms; /**< 置位去抖：原始持续激活多久才 ACTIVE（0=立即） */
	uint32_t clear_time_ms;   /**< 恢复去抖：原始持续消失多久才允许清除（0=立即） */
	bool latch;               /**< 清除后是否转 LATCHED（保持） */
};

/** 每条目运行态。 */
struct bms_diag_entry_rt {
	enum bms_diag_life state;
	uint32_t since_ms; /**< 当前 CONFIRMING/CLEARING 阶段起始时间 */
};

/**
 * @brief 诊断条目生命周期纯步进（无副作用；输入注入时间）。落 diagnostics-fault-model §4。
 * @param cfg   条目配置（severity/confirm/clear/latch）
 * @param rt    条目运行态（就地更新）
 * @param raw_active 源模块本周期原始判定（true=失败/触发）
 * @param now_ms 注入的单调毫秒时间
 */
void bms_diag_entry_step(const struct bms_diag_entry_cfg *cfg, struct bms_diag_entry_rt *rt,
			 bool raw_active, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* BMS_DIAG_H_ */
