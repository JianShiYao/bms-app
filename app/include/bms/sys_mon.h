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

/*
 * ---------------------------------------------------------------------------
 * 有状态聚合层（M5 第 2 片）：镜像 diag 的「纯核 + 有状态 registry」双层结构。
 * 内部维护每任务 rt[]，周期性评估全部任务→写 DB_TASK_HEALTH→上报 diag。
 * 契约见 docs/concept/runtime-model.md §6 与 data-model.md（DB_TASK_HEALTH，
 * owner=bms_sys_mon）。本增量只声明契约（additive），聚合/写库/上报由 coder 补齐。
 * ---------------------------------------------------------------------------
 */

/** 被监控任务 id（同时用作 DB_TASK_HEALTH 掩码的 bit 序号）。 */
enum bms_sys_mon_task {
	BMS_SYS_MON_SAFETY = 0, /**< safety cyclic 任务 */
	BMS_SYS_MON_APP,        /**< app cyclic 任务 */
	BMS_SYS_MON_COUNT,      /**< 被监控任务数量（哨兵，非具体任务） */
};

/**
 * @brief 初始化系统监控有状态层（清零内部每任务 rt[]）。
 * @details 每任务运行态归零（seen=false，开机未运行不误报，runtime-model §6）。
 * @return 0 成功。
 */
int bms_sys_mon_init(void);

/**
 * @brief 记录指定任务进入时刻（心跳打点），委托纯核 @ref bms_sys_mon_enter。
 * @param id     被监控任务 id（越界忽略）。
 * @param now_ms 注入的单调毫秒时间。
 */
void bms_sys_mon_task_enter(enum bms_sys_mon_task id, uint32_t now_ms);

/**
 * @brief 记录指定任务退出时刻并更新运行/峰值时间，委托纯核 @ref bms_sys_mon_exit。
 * @param id     被监控任务 id（越界忽略）。
 * @param now_ms 注入的单调毫秒时间。
 */
void bms_sys_mon_task_exit(enum bms_sys_mon_task id, uint32_t now_ms);

/**
 * @brief 周期评估全部任务健康→写 DB_TASK_HEALTH→上报诊断（runtime-model §6）。
 * @details 逐任务委托纯核 @ref bms_sys_mon_eval 评估心跳超时/运行超时，聚合为
 *          heartbeat_timeout_mask / runtime_overrun_mask 写入 DB_TASK_HEALTH；
 *          任一超时/超限则以 @ref bms_diag_report 上报 BMS_DIAG_TASK_OVERRUN。
 * @param now_ms 注入的单调毫秒时间（写入快照 timestamp_ms 与超时判定）。
 */
void bms_sys_mon_step(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* BMS_SYS_MON_H_ */
