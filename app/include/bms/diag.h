/*
 * BMS diagnosis center.
 */
#ifndef BMS_DIAG_H_
#define BMS_DIAG_H_

#include <stdbool.h>
#include <stdint.h>

#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum bms_diag_id {
	BMS_DIAG_INVALID_MEAS = 0,
	BMS_DIAG_PROTECTION_ACTIVE,
	BMS_DIAG_TASK_OVERRUN,
	BMS_DIAG_COUNT,
};

enum bms_diag_severity {
	BMS_DIAG_INFO = 0,
	BMS_DIAG_WARNING,
	BMS_DIAG_ERROR,
	BMS_DIAG_CRITICAL,
};

struct bms_diag_state {
	uint32_t timestamp_ms;
	uint32_t active_mask;
	uint32_t latched_mask;
	enum bms_diag_severity max_severity;
};

int bms_diag_init(void);
int bms_diag_report(enum bms_diag_id id, enum bms_diag_severity severity, bool active, bool latch);
int bms_diag_get_state(struct bms_diag_state *out);
bool bms_diag_has_error(void);

#ifdef __cplusplus
}
#endif

#endif /* BMS_DIAG_H_ */
