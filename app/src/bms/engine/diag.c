/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    diag.c
 * @brief   BMS 诊断中心实现。
 * @ingroup SYS
 */

#include <errno.h>
#include <zephyr/kernel.h>

#include "bms/diag.h"
#include "bms/time.h"

static K_MUTEX_DEFINE(diag_lock);

static struct bms_diag_state diag_state;

/* 每条目静态登记表（severity/confirm/clear/latch）。默认值经验证与旧行为等价：
 * confirm=0 保证安全故障即时置位、不延迟；PROTECTION latch=true 保持锁存。 */
static const struct bms_diag_entry_cfg DIAG_CFG[BMS_DIAG_COUNT] = {
	[BMS_DIAG_INVALID_MEAS] =
		{
			.severity = BMS_DIAG_ERROR,
			.confirm_time_ms = 0,
			.clear_time_ms = 0,
			.latch = false,
		},
	[BMS_DIAG_PROTECTION_ACTIVE] =
		{
			.severity = BMS_DIAG_CRITICAL,
			.confirm_time_ms = 0,
			.clear_time_ms = 0,
			.latch = true,
		},
	[BMS_DIAG_TASK_OVERRUN] =
		{
			.severity = BMS_DIAG_ERROR,
			.confirm_time_ms = 0,
			.clear_time_ms = 0,
			.latch = false,
		},
	[BMS_DIAG_MEAS_STALE] =
		{
			.severity = BMS_DIAG_ERROR,
			.confirm_time_ms = 0,
			.clear_time_ms = 0,
			.latch = false,
		},
};

static struct bms_diag_entry_rt diag_rt[BMS_DIAG_COUNT];

void bms_diag_entry_step(const struct bms_diag_entry_cfg *cfg, struct bms_diag_entry_rt *rt,
			 bool raw_active, uint32_t now_ms)
{
	switch (rt->state) {
	case BMS_DIAG_LIFE_INACTIVE:
		if (raw_active) {
			rt->state = BMS_DIAG_LIFE_CONFIRMING;
			rt->since_ms = now_ms;
			if (bms_time_after(now_ms, rt->since_ms + cfg->confirm_time_ms)) {
				rt->state = BMS_DIAG_LIFE_ACTIVE;
			}
		}
		break;

	case BMS_DIAG_LIFE_CONFIRMING:
		if (raw_active) {
			if (bms_time_after(now_ms, rt->since_ms + cfg->confirm_time_ms)) {
				rt->state = BMS_DIAG_LIFE_ACTIVE;
			}
		} else {
			rt->state = BMS_DIAG_LIFE_INACTIVE;
		}
		break;

	case BMS_DIAG_LIFE_ACTIVE:
		if (!raw_active) {
			rt->state = BMS_DIAG_LIFE_CLEARING;
			rt->since_ms = now_ms;
			if (bms_time_after(now_ms, rt->since_ms + cfg->clear_time_ms)) {
				rt->state =
					cfg->latch ? BMS_DIAG_LIFE_LATCHED : BMS_DIAG_LIFE_INACTIVE;
			}
		}
		break;

	case BMS_DIAG_LIFE_CLEARING:
		if (raw_active) {
			rt->state = BMS_DIAG_LIFE_ACTIVE;
		} else if (bms_time_after(now_ms, rt->since_ms + cfg->clear_time_ms)) {
			rt->state = cfg->latch ? BMS_DIAG_LIFE_LATCHED : BMS_DIAG_LIFE_INACTIVE;
		}
		break;

	case BMS_DIAG_LIFE_LATCHED:
	default:
		rt->state = BMS_DIAG_LIFE_LATCHED;
		break;
	}
}

int bms_diag_init(void)
{
	k_mutex_lock(&diag_lock, K_FOREVER);
	for (int i = 0; i < BMS_DIAG_COUNT; i++) {
		diag_rt[i] = (struct bms_diag_entry_rt){
			.state = BMS_DIAG_LIFE_INACTIVE,
			.since_ms = 0,
		};
	}
	diag_state = (struct bms_diag_state){0};
	diag_state.timestamp_ms = bms_time_now_ms();
	k_mutex_unlock(&diag_lock);
	return 0;
}

/* 重算聚合状态（调用者须持锁）。 */
static void diag_recompute_locked(uint32_t now_ms)
{
	uint32_t active_mask = 0U;
	uint32_t latched_mask = 0U;
	enum bms_diag_severity max_severity = BMS_DIAG_INFO;

	for (int i = 0; i < BMS_DIAG_COUNT; i++) {
		if (diag_rt[i].state == BMS_DIAG_LIFE_ACTIVE) {
			active_mask |= BIT(i);
			if (DIAG_CFG[i].severity > max_severity) {
				max_severity = DIAG_CFG[i].severity;
			}
		}
		if (diag_rt[i].state == BMS_DIAG_LIFE_LATCHED) {
			latched_mask |= BIT(i);
		}
	}

	diag_state.active_mask = active_mask;
	diag_state.latched_mask = latched_mask;
	diag_state.max_severity = max_severity;
	diag_state.timestamp_ms = now_ms;
}

int bms_diag_report(enum bms_diag_id id, bool raw_active, uint32_t now_ms)
{
	if (id >= BMS_DIAG_COUNT) {
		return -EINVAL;
	}

	k_mutex_lock(&diag_lock, K_FOREVER);
	bms_diag_entry_step(&DIAG_CFG[id], &diag_rt[id], raw_active, now_ms);
	diag_recompute_locked(now_ms);
	k_mutex_unlock(&diag_lock);
	return 0;
}

int bms_diag_get_state(struct bms_diag_state *out)
{
	if (out == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&diag_lock, K_FOREVER);
	*out = diag_state;
	k_mutex_unlock(&diag_lock);
	return 0;
}

bool bms_diag_has_error(void)
{
	struct bms_diag_state state;

	(void)bms_diag_get_state(&state);
	return state.latched_mask != 0U || state.max_severity >= BMS_DIAG_ERROR;
}
