/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    db.c
 * @brief   BMS 数据库（data-exchange）门面实现。
 * @ingroup SYS
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
static struct bms_task_health db_task_health;
static struct bms_contactor_fb db_contactor_fb;

static struct bms_db_slot db_cell_meta;
static struct bms_db_slot db_soc_meta;
static struct bms_db_slot db_prot_meta;
static struct bms_db_slot db_bms_state_meta;
static struct bms_db_slot db_task_health_meta;
static struct bms_db_slot db_contactor_fb_meta;

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
	memset(&db_task_health, 0, sizeof(db_task_health));
	memset(&db_contactor_fb, 0, sizeof(db_contactor_fb));
	memset(&db_cell_meta, 0, sizeof(db_cell_meta));
	memset(&db_soc_meta, 0, sizeof(db_soc_meta));
	memset(&db_prot_meta, 0, sizeof(db_prot_meta));
	memset(&db_bms_state_meta, 0, sizeof(db_bms_state_meta));
	memset(&db_task_health_meta, 0, sizeof(db_task_health_meta));
	memset(&db_contactor_fb_meta, 0, sizeof(db_contactor_fb_meta));
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

int bms_db_write_task_health(const struct bms_task_health *health)
{
	if (health == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&db_lock, K_FOREVER);
	db_task_health = *health;
	db_task_health_meta.sequence++;
	db_task_health_meta.valid = true;
	k_mutex_unlock(&db_lock);
	return 0;
}

int bms_db_read_task_health(struct bms_task_health *health, struct bms_db_meta *meta)
{
	if (health == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&db_lock, K_FOREVER);
	*health = db_task_health;
	copy_meta(&db_task_health_meta, meta);
	k_mutex_unlock(&db_lock);
	return 0;
}

int bms_db_write_contactor_fb(const struct bms_contactor_fb *fb)
{
	if (fb == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&db_lock, K_FOREVER);
	db_contactor_fb = *fb;
	db_contactor_fb_meta.sequence++;
	db_contactor_fb_meta.valid = true;
	k_mutex_unlock(&db_lock);
	return 0;
}

int bms_db_read_contactor_fb(struct bms_contactor_fb *fb, struct bms_db_meta *meta)
{
	if (fb == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&db_lock, K_FOREVER);
	*fb = db_contactor_fb;
	copy_meta(&db_contactor_fb_meta, meta);
	k_mutex_unlock(&db_lock);
	return 0;
}
