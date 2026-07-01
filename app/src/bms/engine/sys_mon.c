/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    sys_mon.c
 * @brief   BMS 系统监控（任务健康核）实现——TDD 红灯阶段**空存根**。
 * @ingroup SYS
 *
 * @details 心跳与运行时间监控纯核：enter/exit 记录进入时刻、本次/峰值运行时间；
 *  eval 纯评估心跳超时与运行超时。契约见 bms/sys_mon.h 与
 *  docs/concept/runtime-model.md §6。心跳超时判定复用 @ref bms_time_after
 *  以有符号差回绕安全（runtime-model §2）。
 */

#include "bms/sys_mon.h"
#include "bms/time.h"

void bms_sys_mon_enter(struct bms_sys_mon_rt *rt, uint32_t now_ms)
{
	rt->last_enter_ms = now_ms;
	rt->seen = true;
}

void bms_sys_mon_exit(struct bms_sys_mon_rt *rt, uint32_t now_ms)
{
	/* 运行时间为时间差（无符号差即正确 elapsed，模 2^32）。 */
	uint32_t runtime_ms = now_ms - rt->last_enter_ms;

	rt->last_runtime_ms = runtime_ms;
	if (runtime_ms > rt->peak_runtime_ms) {
		rt->peak_runtime_ms = runtime_ms;
	}
}

struct bms_sys_mon_health bms_sys_mon_eval(const struct bms_sys_mon_cfg *cfg,
					   const struct bms_sys_mon_rt *rt, uint32_t now_ms)
{
	return (struct bms_sys_mon_health){
		/* 心跳超时：已 enter 过且距上次 enter 超过阈值（有符号差回绕安全）。 */
		.heartbeat_timeout =
			rt->seen &&
			bms_time_after(now_ms, rt->last_enter_ms + cfg->heartbeat_timeout_ms),
		/* 运行超时：峰值运行时间超过声明 WCET。 */
		.runtime_overrun = rt->peak_runtime_ms > cfg->wcet_ms,
	};
}
