/*
 * 保护状态机模块 —— 桩实现
 *
 * 职责：订阅 chan_cell_meas / chan_soc，评估过压/欠压/过流/过温，
 * 发布 chan_prot_state 并给出期望接触器状态。
 * 失效安全原则：默认接触器 OPEN，仅当判定 NORMAL 时才 CLOSED。
 */
#include <errno.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bms/protection.h"
#include "bms/channels.h"

LOG_MODULE_REGISTER(bms_protection, LOG_LEVEL_INF);

#define PROT_THREAD_STACK 1024
#define PROT_THREAD_PRIO  4 /* 安全相关，优先级高于其它模块 */

ZBUS_SUBSCRIBER_DEFINE(prot_sub, 8);
ZBUS_CHAN_ADD_OBS(chan_cell_meas, prot_sub, 2);
ZBUS_CHAN_ADD_OBS(chan_soc, prot_sub, 2);

void bms_protection_default_limits(struct bms_prot_limits *limits)
{
	if (limits == NULL) {
		return;
	}
	/* TODO: 按电芯规格表 / Kconfig 配置真实阈值 */
	limits->cell_ov_mv = 4250;       /* 4.25V 过压 */
	limits->cell_uv_mv = 2800;       /* 2.80V 欠压 */
	limits->over_current_ma = 50000; /* 50A 过流（绝对值） */
	limits->over_temp_dci = 600;     /* 60.0℃ 过温 */
}

int bms_protection_evaluate(const struct bms_cell_meas *meas, const struct bms_prot_limits *limits,
			    struct bms_prot_evt *out)
{
	if (meas == NULL || limits == NULL || out == NULL) {
		return -EINVAL;
	}

	out->timestamp_ms = meas->timestamp_ms;
	out->state = BMS_PROT_NORMAL;
	out->cell_index = 0;

	/* 电压检查（任一单体越限即触发） */
	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		if (meas->cell_mv[i] >= limits->cell_ov_mv) {
			out->state = BMS_PROT_OV;
			out->cell_index = (uint8_t)i;
			goto decide;
		}
		if (meas->cell_mv[i] <= limits->cell_uv_mv) {
			out->state = BMS_PROT_UV;
			out->cell_index = (uint8_t)i;
			goto decide;
		}
	}

	/* 过流检查（充放电绝对值） */
	if (abs(meas->pack_current_ma) >= limits->over_current_ma) {
		out->state = BMS_PROT_OC;
		goto decide;
	}

	/* 过温检查 */
	for (int i = 0; i < BMS_TEMP_SENSOR_COUNT; i++) {
		if (meas->temp_dci[i] >= limits->over_temp_dci) {
			out->state = BMS_PROT_OT;
			goto decide;
		}
	}

decide:
	/* 失效安全：仅 NORMAL 才闭合接触器 */
	out->contactor =
		(out->state == BMS_PROT_NORMAL) ? BMS_CONTACTOR_CLOSED : BMS_CONTACTOR_OPEN;
	return 0;
}

static void prot_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;
	struct bms_cell_meas meas;
	struct bms_prot_limits limits;
	struct bms_prot_evt evt;

	bms_protection_default_limits(&limits);

	while (zbus_sub_wait(&prot_sub, &chan, K_FOREVER) == 0) {
		if (chan != &chan_cell_meas) {
			continue; /* chan_soc 暂未参与判定，预留 */
		}
		if (zbus_chan_read(&chan_cell_meas, &meas, K_MSEC(50)) != 0) {
			continue;
		}
		if (bms_protection_evaluate(&meas, &limits, &evt) == 0) {
			/* TODO: 此处驱动接触器/MOS GPIO（按 evt.contactor） */
			if (evt.state != BMS_PROT_NORMAL) {
				LOG_WRN("protection state=%d cell=%d -> contactor OPEN", evt.state,
					evt.cell_index);
			}
			zbus_chan_pub(&chan_prot_state, &evt, K_MSEC(50));
		}
	}
}

K_THREAD_DEFINE(bms_prot_tid, PROT_THREAD_STACK, prot_thread, NULL, NULL, NULL, PROT_THREAD_PRIO, 0,
		0);

int bms_protection_init(void)
{
	LOG_INF("Protection init: fail-safe default contactor OPEN");
	return 0;
}
