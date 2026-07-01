/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    sys_mon.h
 * @brief   BMS 系统监控（任务健康核）：心跳与运行时间监控纯函数。
 * @ingroup SYS
 *
 * @details 落实运行时模型契约（docs/concept/runtime-model.md §6 / §8）与
 *  架构基线 ADR-ARCH-003（docs/concept/architecture.md §6）：
 *  - 每个被监控 cyclic 任务进入/退出调 @ref bms_sys_mon_enter / @ref bms_sys_mon_exit，
 *    记录 last_enter_ms（心跳）、本次运行时间、峰值运行时间。
 *  - 判据：**心跳超时**（超过阈值未再 enter）、**运行超时**（峰值运行时间 > 声明 WCET）。
 *  - 一切时间判定以**注入 now_ms** 纯测（runtime-model §9），心跳超时判定复用
 *    @ref bms_time_after 保证有符号差回绕安全（runtime-model §2）。
 *
 * 本增量只声明契约（additive），实现由后续 coder 阶段补齐。
 */

#ifndef BMS_SYS_MON_H_
#define BMS_SYS_MON_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 每任务静态配置（登记表项）。 */
struct bms_sys_mon_cfg {
	uint32_t wcet_ms;              /**< 声明的最大运行时间上限（WCET） */
	uint32_t heartbeat_timeout_ms; /**< 心跳超时阈值：距上次 enter 超过此值判超时 */
};

/** 每任务运行态。 */
struct bms_sys_mon_rt {
	uint32_t last_enter_ms;   /**< 最近一次进入时刻（心跳） */
	uint32_t last_runtime_ms; /**< 最近一次运行时间（exit - enter） */
	uint32_t peak_runtime_ms; /**< 历史峰值运行时间（取最大） */
	bool seen;                /**< 是否已至少 enter 过一次（false=开机未运行，兜底不误报） */
};

/** 纯评估结果。 */
struct bms_sys_mon_health {
	bool heartbeat_timeout; /**< 心跳超时（超过阈值未再 enter） */
	bool runtime_overrun;   /**< 运行超时（峰值运行时间 > 声明 WCET） */
};

/**
 * @brief 记录任务进入时刻（心跳打点）。
 * @details 置 rt->last_enter_ms = now_ms、rt->seen = true。
 * @param rt     任务运行态（就地更新，非空）。
 * @param now_ms 注入的单调毫秒时间。
 */
void bms_sys_mon_enter(struct bms_sys_mon_rt *rt, uint32_t now_ms);

/**
 * @brief 记录任务退出时刻并更新运行时间与峰值。
 * @details 本次运行时间 = now_ms - last_enter_ms（有符号差保护，runtime-model §2）；
 *          更新 rt->last_runtime_ms，并把 rt->peak_runtime_ms 取历史最大。
 * @param rt     任务运行态（就地更新，非空）。
 * @param now_ms 注入的单调毫秒时间。
 */
void bms_sys_mon_exit(struct bms_sys_mon_rt *rt, uint32_t now_ms);

/**
 * @brief 纯评估任务健康（无副作用；输入注入时间）。落 runtime-model §6。
 * @details
 *  - heartbeat_timeout = rt->seen &&
 *    @ref bms_time_after (now_ms, rt->last_enter_ms + cfg->heartbeat_timeout_ms)；
 *    未 enter 过（seen=false）不判超时，避免开机即误报。
 *  - runtime_overrun = rt->peak_runtime_ms > cfg->wcet_ms。
 * @param cfg    任务静态配置（非空）。
 * @param rt     任务运行态（非空）。
 * @param now_ms 注入的单调毫秒时间。
 * @return 评估结果快照。
 */
struct bms_sys_mon_health bms_sys_mon_eval(const struct bms_sys_mon_cfg *cfg,
					   const struct bms_sys_mon_rt *rt, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* BMS_SYS_MON_H_ */
