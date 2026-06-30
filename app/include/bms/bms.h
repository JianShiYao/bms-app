/*
 * BMS main state machine.
 */
#ifndef BMS_BMS_H_
#define BMS_BMS_H_

#include <stdbool.h>

#include "bms/diag.h"
#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bms_state_inputs {
	bool close_allowed;
	bool open_request;
	bool hw_fault_latched;
	struct bms_diag_state diag;
	struct bms_prot_evt prot;
};

enum bms_state bms_next_state(enum bms_state cur, const struct bms_state_inputs *in);
enum bms_contactor bms_contactor_for_state(enum bms_state state);

#ifdef __cplusplus
}
#endif

#endif /* BMS_BMS_H_ */
