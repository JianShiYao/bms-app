/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    task.c
 * @brief   foxBMS 2 inspired 任务框架（Zephyr 静态线程实现）。
 * @ingroup SYS
 */

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bms/afe.h"
#include "bms/balancing.h"
#include "bms/bms.h"
#include "bms/channels.h"
#include "bms/comm.h"
#include "bms/db.h"
#include "bms/diag.h"
#include "bms/protection.h"
#include "bms/soc.h"
#include "bms/task.h"
#include "bms/time.h"

LOG_MODULE_REGISTER(bms_task, LOG_LEVEL_INF);

#ifndef CONFIG_BMS_TASK_SAFETY_PERIOD_MS
#define CONFIG_BMS_TASK_SAFETY_PERIOD_MS 10
#endif
#ifndef CONFIG_BMS_TASK_APP_PERIOD_MS
#define CONFIG_BMS_TASK_APP_PERIOD_MS 100
#endif
#ifndef CONFIG_BMS_AFE_SAMPLE_PERIOD_MS
#define CONFIG_BMS_AFE_SAMPLE_PERIOD_MS 100
#endif

#define TASK_SAFETY_STACK 1536
#define TASK_APP_STACK    1536
#define TASK_BG_STACK     768

#define TASK_SAFETY_PRIO 3
#define TASK_APP_PRIO    5
#define TASK_BG_PRIO     8

#define BAL_DELTA_MV   20
#define BAL_MASK_BYTES ((BMS_CELL_COUNT + 7) / 8)

#if defined(CONFIG_BMS_SOC)
static struct bms_soc_coulomb_state task_soc_state;
#endif
static struct bms_prot_limits task_prot_limits;
static enum bms_state task_bms_state = BMS_STATE_INIT;

/* 采样到期状态（safety_step 消化，bms_task_init 复位为 0）。 */
static uint32_t next_sample;

#if defined(CONFIG_BMS_COMM)
/* comm 周期上报到期状态（app_step 消化，bms_task_init 复位为 0）。 */
static uint32_t next_comm;
#endif
#if defined(CONFIG_BMS_SOC)
/* 上次消化的采样序号，去重 SOC 步进（bms_task_init 复位为 0）。 */
static uint32_t last_soc_meas_seq;
#endif

static void publish_cell_compat(const struct bms_cell_meas *meas)
{
	(void)zbus_chan_pub(&chan_cell_meas, meas, K_NO_WAIT);
}

#if defined(CONFIG_BMS_SOC)
static void publish_soc_compat(const struct bms_soc *soc)
{
	(void)zbus_chan_pub(&chan_soc, soc, K_NO_WAIT);
}
#endif

static void publish_prot_compat(const struct bms_prot_evt *prot)
{
	(void)zbus_chan_pub(&chan_prot_state, prot, K_NO_WAIT);
}

static void run_measurement(uint32_t now)
{
#if defined(CONFIG_BMS_AFE)
	struct bms_cell_meas meas;

	if (bms_afe_sample(&meas) != 0) {
		(void)bms_diag_report(BMS_DIAG_INVALID_MEAS, true, now);
		return;
	}

	(void)bms_db_write_cell_meas(&meas);
	publish_cell_compat(&meas);

	(void)bms_diag_report(BMS_DIAG_INVALID_MEAS,
			      (meas.validity & BMS_MEAS_VALID_ALL) != BMS_MEAS_VALID_ALL, now);
#else
	ARG_UNUSED(now);
#endif
}

static void run_protection_and_bms(uint32_t now_ms)
{
#if defined(CONFIG_BMS_PROTECTION)
	struct bms_cell_meas meas;
	struct bms_db_meta meas_meta;
	struct bms_prot_evt prot = {
		.state = BMS_PROT_FAULT,
		.contactor = BMS_CONTACTOR_OPEN,
	};

	if (bms_db_read_cell_meas(&meas, &meas_meta) != 0 || !meas_meta.valid) {
		return;
	}

	if (bms_protection_evaluate(&meas, &task_prot_limits, &prot) == 0) {
		(void)bms_db_write_prot(&prot);
		publish_prot_compat(&prot);
		(void)bms_diag_report(BMS_DIAG_PROTECTION_ACTIVE, prot.state != BMS_PROT_NORMAL,
				      now_ms);
	}
#else
	ARG_UNUSED(now_ms);
	struct bms_prot_evt prot = {
		.timestamp_ms = bms_time_now_ms(),
		.state = BMS_PROT_NORMAL,
		.contactor = BMS_CONTACTOR_CLOSED,
	};
#endif

	struct bms_diag_state diag;
	struct bms_state_inputs inputs;

	(void)bms_diag_get_state(&diag);
	inputs = (struct bms_state_inputs){
		.close_allowed = true,
		.open_request = false,
		.hw_fault_latched = false,
		.diag = diag,
		.prot = prot,
	};

	task_bms_state = bms_next_state(task_bms_state, &inputs);

	struct bms_state_snapshot state = {
		.timestamp_ms = bms_time_now_ms(),
		.state = task_bms_state,
		.contactor = bms_contactor_for_state(task_bms_state),
	};

	(void)bms_db_write_bms_state(&state);
}

void bms_task_safety_step(uint32_t now_ms)
{
	if (bms_time_due(now_ms, &next_sample, CONFIG_BMS_AFE_SAMPLE_PERIOD_MS)) {
		run_measurement(now_ms);
	}
	run_protection_and_bms(now_ms);
}

void bms_task_app_step(uint32_t now_ms)
{
	struct bms_cell_meas meas;
	struct bms_db_meta meas_meta = {0};

	if (bms_db_read_cell_meas(&meas, &meas_meta) == 0 && meas_meta.valid) {
#if defined(CONFIG_BMS_SOC)
		if (meas_meta.sequence != last_soc_meas_seq) {
			struct bms_soc soc;
			int rc = bms_soc_coulomb_step(&task_soc_state, &meas, &soc);

			last_soc_meas_seq = meas_meta.sequence;
			if (rc == 0) {
				(void)bms_db_write_soc(&soc);
				publish_soc_compat(&soc);
			}
		}
#endif

#if defined(CONFIG_BMS_BALANCING)
		uint8_t mask[BAL_MASK_BYTES];

		if (bms_balancing_compute(&meas, BAL_DELTA_MV, mask, sizeof(mask)) == 0) {
			LOG_DBG("balancing mask[0]=0x%02x", mask[0]);
		}
#endif
	}

#if defined(CONFIG_BMS_COMM)
	if (bms_time_due(now_ms, &next_comm, (uint32_t)bms_comm_effective_period_ms())) {
		struct bms_soc soc;
		struct bms_prot_evt prot;
		struct bms_db_meta soc_meta = {0};
		struct bms_db_meta prot_meta = {0};
		const struct bms_cell_meas *meas_ptr = meas_meta.valid ? &meas : NULL;
		const struct bms_soc *soc_ptr = NULL;
		const struct bms_prot_evt *prot_ptr = NULL;

		if (bms_db_read_soc(&soc, &soc_meta) == 0 && soc_meta.valid) {
			soc_ptr = &soc;
		}
		if (bms_db_read_prot(&prot, &prot_meta) == 0 && prot_meta.valid) {
			prot_ptr = &prot;
		}
		bms_comm_tx_snapshot(meas_ptr, soc_ptr, prot_ptr);
	}
#else
	ARG_UNUSED(now_ms);
#endif
}

static void tsk_safety_10ms(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uint32_t next_wake = bms_time_now_ms();

	while (1) {
		bms_task_safety_step(bms_time_now_ms());

		next_wake += CONFIG_BMS_TASK_SAFETY_PERIOD_MS;
		int32_t delay = (int32_t)(next_wake - bms_time_now_ms());

		if (delay > 0) {
			k_msleep((uint32_t)delay);
		} else {
			next_wake = bms_time_now_ms(); /* 落后则重置，不追赶 */
		}
	}
}

static void tsk_app_100ms(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uint32_t next_wake = bms_time_now_ms();

	while (1) {
		bms_task_app_step(bms_time_now_ms());

		next_wake += CONFIG_BMS_TASK_APP_PERIOD_MS;
		int32_t delay = (int32_t)(next_wake - bms_time_now_ms());

		if (delay > 0) {
			k_msleep((uint32_t)delay);
		} else {
			next_wake = bms_time_now_ms(); /* 落后则重置，不追赶 */
		}
	}
}

static void tsk_background(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		struct bms_state_snapshot state;
		struct bms_db_meta meta;

		if (bms_db_read_bms_state(&state, &meta) == 0 && meta.valid) {
			LOG_INF("health: bms_state=%d contactor=%s", state.state,
				state.contactor == BMS_CONTACTOR_CLOSED ? "CLOSED" : "OPEN");
		}
		k_sleep(K_SECONDS(5));
	}
}

K_THREAD_DEFINE(bms_tsk_safety_tid, TASK_SAFETY_STACK, tsk_safety_10ms, NULL, NULL, NULL,
		TASK_SAFETY_PRIO, 0, SYS_FOREVER_MS);
K_THREAD_DEFINE(bms_tsk_app_tid, TASK_APP_STACK, tsk_app_100ms, NULL, NULL, NULL, TASK_APP_PRIO, 0,
		SYS_FOREVER_MS);
K_THREAD_DEFINE(bms_tsk_bg_tid, TASK_BG_STACK, tsk_background, NULL, NULL, NULL, TASK_BG_PRIO, 0,
		SYS_FOREVER_MS);

int bms_task_init(void)
{
#if defined(CONFIG_BMS_SOC)
	bms_soc_coulomb_state_reset(&task_soc_state);
	last_soc_meas_seq = 0;
#endif
#if defined(CONFIG_BMS_PROTECTION)
	bms_protection_default_limits(&task_prot_limits);
#endif
	task_bms_state = BMS_STATE_INIT;

	next_sample = 0;
#if defined(CONFIG_BMS_COMM)
	next_comm = 0;
#endif

	struct bms_state_snapshot initial = {
		.timestamp_ms = bms_time_now_ms(),
		.state = BMS_STATE_INIT,
		.contactor = BMS_CONTACTOR_OPEN,
	};

	(void)bms_db_write_bms_state(&initial);

	LOG_INF("Task framework init: safety=%d ms app=%d ms", CONFIG_BMS_TASK_SAFETY_PERIOD_MS,
		CONFIG_BMS_TASK_APP_PERIOD_MS);
	return 0;
}

void bms_task_start(void)
{
	k_thread_start(bms_tsk_safety_tid);
	k_thread_start(bms_tsk_app_tid);
	k_thread_start(bms_tsk_bg_tid);
}
