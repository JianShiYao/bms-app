/*
 * BMS main state machine pure logic.
 */
#include <stddef.h>

#include "bms/bms.h"

enum bms_contactor bms_contactor_for_state(enum bms_state state)
{
	return state == BMS_STATE_NORMAL ? BMS_CONTACTOR_CLOSED : BMS_CONTACTOR_OPEN;
}

enum bms_state bms_next_state(enum bms_state cur, const struct bms_state_inputs *in)
{
	if (in == NULL) {
		return BMS_STATE_FAULT;
	}

	if (in->hw_fault_latched || in->diag.latched_mask != 0U ||
	    in->diag.max_severity >= BMS_DIAG_CRITICAL) {
		return BMS_STATE_LOCKED;
	}

	if (in->diag.max_severity >= BMS_DIAG_ERROR || in->prot.state != BMS_PROT_NORMAL ||
	    in->open_request) {
		return BMS_STATE_FAULT;
	}

	switch (cur) {
	case BMS_STATE_INIT:
		return BMS_STATE_STANDBY;
	case BMS_STATE_STANDBY:
		return in->close_allowed ? BMS_STATE_NORMAL : BMS_STATE_STANDBY;
	case BMS_STATE_PRECHARGE:
		return in->close_allowed ? BMS_STATE_NORMAL : BMS_STATE_FAULT;
	case BMS_STATE_NORMAL:
		return in->close_allowed ? BMS_STATE_NORMAL : BMS_STATE_STANDBY;
	case BMS_STATE_FAULT:
		return in->close_allowed ? BMS_STATE_STANDBY : BMS_STATE_FAULT;
	case BMS_STATE_LOCKED:
	default:
		return BMS_STATE_LOCKED;
	}
}
