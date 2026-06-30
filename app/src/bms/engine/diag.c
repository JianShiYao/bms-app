/*
 * BMS diagnosis center.
 */
#include <errno.h>
#include <zephyr/kernel.h>

#include "bms/diag.h"

static K_MUTEX_DEFINE(diag_lock);

static struct bms_diag_state diag_state;

int bms_diag_init(void)
{
	k_mutex_lock(&diag_lock, K_FOREVER);
	diag_state = (struct bms_diag_state){0};
	diag_state.timestamp_ms = k_uptime_get_32();
	k_mutex_unlock(&diag_lock);
	return 0;
}

int bms_diag_report(enum bms_diag_id id, enum bms_diag_severity severity, bool active, bool latch)
{
	if (id >= BMS_DIAG_COUNT) {
		return -EINVAL;
	}

	uint32_t bit = BIT(id);

	k_mutex_lock(&diag_lock, K_FOREVER);
	diag_state.timestamp_ms = k_uptime_get_32();
	if (active) {
		diag_state.active_mask |= bit;
		if (latch) {
			diag_state.latched_mask |= bit;
		}
		if (severity > diag_state.max_severity) {
			diag_state.max_severity = severity;
		}
	} else {
		diag_state.active_mask &= ~bit;
		if (diag_state.active_mask == 0U) {
			diag_state.max_severity = BMS_DIAG_INFO;
		}
	}
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
