/*
 * BMS database facade.
 *
 * foxBMS 2 inspired data exchange center: modules write/read typed snapshots
 * through this API instead of depending on each other's threads.
 */
#ifndef BMS_DB_H_
#define BMS_DB_H_

#include <stdbool.h>
#include <stdint.h>

#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bms_db_meta {
	uint32_t sequence;
	bool valid;
};

int bms_db_init(void);

int bms_db_write_cell_meas(const struct bms_cell_meas *meas);
int bms_db_read_cell_meas(struct bms_cell_meas *meas, struct bms_db_meta *meta);

int bms_db_write_soc(const struct bms_soc *soc);
int bms_db_read_soc(struct bms_soc *soc, struct bms_db_meta *meta);

int bms_db_write_prot(const struct bms_prot_evt *prot);
int bms_db_read_prot(struct bms_prot_evt *prot, struct bms_db_meta *meta);

int bms_db_write_bms_state(const struct bms_state_snapshot *state);
int bms_db_read_bms_state(struct bms_state_snapshot *state, struct bms_db_meta *meta);

#ifdef __cplusplus
}
#endif

#endif /* BMS_DB_H_ */
