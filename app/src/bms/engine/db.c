/*
 * BMS database facade.
 */
#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>

#include "bms/db.h"

struct bms_db_slot {
	uint32_t sequence;
	bool valid;
};

static K_MUTEX_DEFINE(db_lock);

static struct bms_cell_meas db_cell_meas;
static struct bms_soc db_soc;
static struct bms_prot_evt db_prot;
static struct bms_state_snapshot db_bms_state;

static struct bms_db_slot db_cell_meta;
static struct bms_db_slot db_soc_meta;
static struct bms_db_slot db_prot_meta;
static struct bms_db_slot db_bms_state_meta;

static void copy_meta(const struct bms_db_slot *slot, struct bms_db_meta *meta)
{
	if (meta == NULL) {
		return;
	}
	meta->sequence = slot->sequence;
	meta->valid = slot->valid;
}

int bms_db_init(void)
{
	k_mutex_lock(&db_lock, K_FOREVER);
	memset(&db_cell_meas, 0, sizeof(db_cell_meas));
	memset(&db_soc, 0, sizeof(db_soc));
	memset(&db_prot, 0, sizeof(db_prot));
	memset(&db_bms_state, 0, sizeof(db_bms_state));
	memset(&db_cell_meta, 0, sizeof(db_cell_meta));
	memset(&db_soc_meta, 0, sizeof(db_soc_meta));
	memset(&db_prot_meta, 0, sizeof(db_prot_meta));
	memset(&db_bms_state_meta, 0, sizeof(db_bms_state_meta));
	k_mutex_unlock(&db_lock);
	return 0;
}

int bms_db_write_cell_meas(const struct bms_cell_meas *meas)
{
	if (meas == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&db_lock, K_FOREVER);
	db_cell_meas = *meas;
	db_cell_meta.sequence++;
	db_cell_meta.valid = true;
	k_mutex_unlock(&db_lock);
	return 0;
}

int bms_db_read_cell_meas(struct bms_cell_meas *meas, struct bms_db_meta *meta)
{
	if (meas == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&db_lock, K_FOREVER);
	*meas = db_cell_meas;
	copy_meta(&db_cell_meta, meta);
	k_mutex_unlock(&db_lock);
	return 0;
}

int bms_db_write_soc(const struct bms_soc *soc)
{
	if (soc == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&db_lock, K_FOREVER);
	db_soc = *soc;
	db_soc_meta.sequence++;
	db_soc_meta.valid = true;
	k_mutex_unlock(&db_lock);
	return 0;
}

int bms_db_read_soc(struct bms_soc *soc, struct bms_db_meta *meta)
{
	if (soc == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&db_lock, K_FOREVER);
	*soc = db_soc;
	copy_meta(&db_soc_meta, meta);
	k_mutex_unlock(&db_lock);
	return 0;
}

int bms_db_write_prot(const struct bms_prot_evt *prot)
{
	if (prot == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&db_lock, K_FOREVER);
	db_prot = *prot;
	db_prot_meta.sequence++;
	db_prot_meta.valid = true;
	k_mutex_unlock(&db_lock);
	return 0;
}

int bms_db_read_prot(struct bms_prot_evt *prot, struct bms_db_meta *meta)
{
	if (prot == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&db_lock, K_FOREVER);
	*prot = db_prot;
	copy_meta(&db_prot_meta, meta);
	k_mutex_unlock(&db_lock);
	return 0;
}

int bms_db_write_bms_state(const struct bms_state_snapshot *state)
{
	if (state == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&db_lock, K_FOREVER);
	db_bms_state = *state;
	db_bms_state_meta.sequence++;
	db_bms_state_meta.valid = true;
	k_mutex_unlock(&db_lock);
	return 0;
}

int bms_db_read_bms_state(struct bms_state_snapshot *state, struct bms_db_meta *meta)
{
	if (state == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&db_lock, K_FOREVER);
	*state = db_bms_state;
	copy_meta(&db_bms_state_meta, meta);
	k_mutex_unlock(&db_lock);
	return 0;
}
