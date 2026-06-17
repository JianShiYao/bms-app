/*
 * SOC/SOH 估算模块 —— 桩实现
 *
 * 职责：订阅 chan_cell_meas，估算荷电/健康状态，发布到 chan_soc。
 * 当前 SOC 用平均电压线性映射（桩）；后续替换为库仑积分/卡尔曼滤波。
 */
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bms/soc.h"
#include "bms/channels.h"

LOG_MODULE_REGISTER(bms_soc, LOG_LEVEL_INF);

#define SOC_THREAD_STACK 1024
#define SOC_THREAD_PRIO  7

/* 线性映射端点（桩）：3.0V→0%，4.2V→100% */
#define SOC_EMPTY_MV 3000
#define SOC_FULL_MV  4200

/* 该模块作为订阅者，自行向 chan_cell_meas 注册观察 */
ZBUS_SUBSCRIBER_DEFINE(soc_sub, 4);
ZBUS_CHAN_ADD_OBS(chan_cell_meas, soc_sub, 3);

int bms_soc_estimate(const struct bms_cell_meas *meas, struct bms_soc *out)
{
	if (meas == NULL || out == NULL) {
		return -EINVAL;
	}

	int64_t sum_mv = 0;

	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		sum_mv += meas->cell_mv[i];
	}
	int32_t avg_mv = (int32_t)(sum_mv / BMS_CELL_COUNT);

	/* 线性映射到 0..1000 ‰ 并夹紧 */
	int32_t permille = (avg_mv - SOC_EMPTY_MV) * 1000 /
			   (SOC_FULL_MV - SOC_EMPTY_MV);
	if (permille < 0) {
		permille = 0;
	} else if (permille > 1000) {
		permille = 1000;
	}

	out->timestamp_ms = meas->timestamp_ms;
	out->soc_permille = (uint16_t)permille;
	out->soh_permille = 1000;   /* TODO: 真实 SOH 估算 */

	return 0;
}

static void soc_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;
	struct bms_cell_meas meas;
	struct bms_soc soc;

	while (zbus_sub_wait(&soc_sub, &chan, K_FOREVER) == 0) {
		if (chan != &chan_cell_meas) {
			continue;
		}
		if (zbus_chan_read(&chan_cell_meas, &meas, K_MSEC(50)) != 0) {
			continue;
		}
		if (bms_soc_estimate(&meas, &soc) == 0) {
			zbus_chan_pub(&chan_soc, &soc, K_MSEC(50));
			LOG_DBG("SOC=%u.%u%%", soc.soc_permille / 10,
				soc.soc_permille % 10);
		}
	}
}

K_THREAD_DEFINE(bms_soc_tid, SOC_THREAD_STACK, soc_thread,
		NULL, NULL, NULL, SOC_THREAD_PRIO, 0, 0);

int bms_soc_init(void)
{
	LOG_INF("SOC init: linear-stub %d..%d mV", SOC_EMPTY_MV, SOC_FULL_MV);
	return 0;
}
