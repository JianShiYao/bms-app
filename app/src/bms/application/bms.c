/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    bms.c
 * @brief   BMS 主状态机纯逻辑。
 * @ingroup SYS
 */

#include <stddef.h>

#include "bms/bms.h"

enum bms_contactor bms_contactor_for_state(enum bms_state state)
{
	return (state == BMS_STATE_NORMAL) ? BMS_CONTACTOR_CLOSED : BMS_CONTACTOR_OPEN;
}

enum bms_state bms_next_state(enum bms_state cur, const struct bms_state_inputs *in)
{
	if (in == NULL) {
		return BMS_STATE_FAULT;
	}

	if (in->hw_fault_latched || (in->diag.latched_mask != 0U) ||
	    (in->diag.max_severity >= BMS_DIAG_CRITICAL)) {
		return BMS_STATE_LOCKED;
	}

	if (!in->diag.initialized || (in->diag.max_severity >= BMS_DIAG_ERROR) ||
	    (in->prot.state != BMS_PROT_NORMAL) || in->open_request) {
		return BMS_STATE_FAULT;
	}

	switch (cur) {
	case BMS_STATE_INIT:
		return BMS_STATE_STANDBY;
	case BMS_STATE_STANDBY:
		/* 收到合法闭合命令先执行预充（不直闭主接触器），architecture §7。 */
		return in->close_allowed ? BMS_STATE_PRECHARGE : BMS_STATE_STANDBY;
	case BMS_STATE_PRECHARGE:
		if (!in->close_allowed) {
			return BMS_STATE_STANDBY; /* 命令撤销 → 优雅回退（非故障） */
		}
		if (in->precharge_timeout) {
			return BMS_STATE_FAULT; /* 预充超时 → 失效安全 */
		}
		if (in->precharge_complete) {
			return BMS_STATE_NORMAL; /* 电压爬升达标 → 闭合主接触器 */
		}
		return BMS_STATE_PRECHARGE; /* 未完成未超时 → 继续预充 */
	case BMS_STATE_NORMAL:
		return in->close_allowed ? BMS_STATE_NORMAL : BMS_STATE_STANDBY;
	case BMS_STATE_FAULT:
		return in->close_allowed ? BMS_STATE_STANDBY : BMS_STATE_FAULT;
	case BMS_STATE_LOCKED:
	default:
		return BMS_STATE_LOCKED;
	}
}
