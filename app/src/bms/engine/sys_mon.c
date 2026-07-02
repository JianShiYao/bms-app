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

#include <string.h>

#include "bms/engine/db.h"
#include "bms/engine/diag.h"
#include "bms/engine/sys_mon.h"
#include "bms/engine/time.h"

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

/*
 * ---------------------------------------------------------------------------
 * 有状态聚合层：内部维护每任务 rt[]，enter/exit 委托上方纯核；step 评估全部任务、
 * 聚合掩码写 DB_TASK_HEALTH、并把「任一超时/超限」上报 BMS_DIAG_TASK_OVERRUN。
 * 契约见 docs/concept/runtime-model.md §6。
 *
 * 每任务静态配置为**内联默认值**（暂不依赖 Kconfig，避免脱离 app 构建的纯单测取不到
 * CONFIG 而误判；按板精调留后续片）：wcet 为周期任务体单轮预算上限，心跳超时取数倍周期。
 * ---------------------------------------------------------------------------
 */
static const struct bms_sys_mon_cfg SYS_MON_CFG[BMS_SYS_MON_COUNT] = {
	[BMS_SYS_MON_SAFETY] = {.wcet_ms = 5, .heartbeat_timeout_ms = 30, .safety_critical = true},
	[BMS_SYS_MON_APP] = {.wcet_ms = 20, .heartbeat_timeout_ms = 300, .safety_critical = false},
};

static struct bms_sys_mon_rt sys_mon_rt[BMS_SYS_MON_COUNT];

int bms_sys_mon_init(void)
{
	memset(sys_mon_rt, 0, sizeof(sys_mon_rt));
	return 0;
}

void bms_sys_mon_task_enter(enum bms_sys_mon_task id, uint32_t now_ms)
{
	if ((unsigned int)id >= BMS_SYS_MON_COUNT) {
		return;
	}
	bms_sys_mon_enter(&sys_mon_rt[id], now_ms);
}

void bms_sys_mon_task_exit(enum bms_sys_mon_task id, uint32_t now_ms)
{
	if ((unsigned int)id >= BMS_SYS_MON_COUNT) {
		return;
	}
	bms_sys_mon_exit(&sys_mon_rt[id], now_ms);
}

void bms_sys_mon_step(uint32_t now_ms)
{
	struct bms_task_health health = {.timestamp_ms = now_ms};

	for (unsigned int i = 0; i < BMS_SYS_MON_COUNT; i++) {
		struct bms_sys_mon_health h =
			bms_sys_mon_eval(&SYS_MON_CFG[i], &sys_mon_rt[i], now_ms);

		if (h.heartbeat_timeout) {
			health.heartbeat_timeout_mask |= (1U << i);
		}
		if (h.runtime_overrun) {
			health.runtime_overrun_mask |= (1U << i);
		}
	}

	bms_db_write_task_health(&health);

	/* 任一任务超时/超限 → 上报任务超期诊断（失效安全链：sys_mon→diag→bms）。 */
	bool any_fault = (health.heartbeat_timeout_mask | health.runtime_overrun_mask) != 0U;

	bms_diag_report(BMS_DIAG_TASK_OVERRUN, any_fault, now_ms);
}

bool bms_sys_mon_wdt_feed_allowed(uint32_t now_ms)
{
	/*
	 * 门控（runtime-model §7）：仅当**每个安全关键任务**都已 seen 且健康（无心跳超时、
	 * 无运行超限）时才允许喂硬 watchdog；任一安全关键任务从未运行/失联/超限 → 停喂，
	 * 让 watchdog 复位进上电安全态（软先于硬）。非安全关键任务不参与硬狗门控。
	 */
	for (unsigned int i = 0; i < BMS_SYS_MON_COUNT; i++) {
		if (!SYS_MON_CFG[i].safety_critical) {
			continue;
		}

		const struct bms_sys_mon_rt *rt = &sys_mon_rt[i];

		/* 从未 seen → 安全任务未证明活着，失效安全不喂（闭合"从未启动"缺口）。 */
		if (!rt->seen) {
			return false;
		}

		struct bms_sys_mon_health h = bms_sys_mon_eval(&SYS_MON_CFG[i], rt, now_ms);

		if (h.heartbeat_timeout || h.runtime_overrun) {
			return false;
		}
	}

	return true;
}
