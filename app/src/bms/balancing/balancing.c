/*
 * 单体均衡模块 —— 桩实现
 *
 * 职责：订阅 chan_cell_meas，计算需均衡的单体位掩码。
 * 当前为被动均衡策略桩：高于最低单体电压 + delta 的单体置位。
 */
#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bms/balancing.h"
#include "bms/channels.h"

LOG_MODULE_REGISTER(bms_balancing, LOG_LEVEL_INF);

#define BAL_THREAD_STACK 1024
#define BAL_THREAD_PRIO  7
#define BAL_DELTA_MV     20   /* 压差阈值，桩 */

#define BAL_MASK_BYTES ((BMS_CELL_COUNT + 7) / 8)

ZBUS_SUBSCRIBER_DEFINE(bal_sub, 4);
ZBUS_CHAN_ADD_OBS(chan_cell_meas, bal_sub, 3);

int bms_balancing_compute(const struct bms_cell_meas *meas, int32_t delta_mv,
			  uint8_t *mask_out, size_t mask_len)
{
	if (meas == NULL || mask_out == NULL || mask_len < BAL_MASK_BYTES) {
		return -EINVAL;
	}

	/* 找最低单体电压 */
	int32_t min_mv = meas->cell_mv[0];

	for (int i = 1; i < BMS_CELL_COUNT; i++) {
		if (meas->cell_mv[i] < min_mv) {
			min_mv = meas->cell_mv[i];
		}
	}

	memset(mask_out, 0, mask_len);
	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		if (meas->cell_mv[i] - min_mv >= delta_mv) {
			mask_out[i / 8] |= (uint8_t)(1U << (i % 8));
		}
	}

	return 0;
}

static void bal_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;
	struct bms_cell_meas meas;
	uint8_t mask[BAL_MASK_BYTES];

	while (zbus_sub_wait(&bal_sub, &chan, K_FOREVER) == 0) {
		if (chan != &chan_cell_meas) {
			continue;
		}
		if (zbus_chan_read(&chan_cell_meas, &meas, K_MSEC(50)) != 0) {
			continue;
		}
		if (bms_balancing_compute(&meas, BAL_DELTA_MV, mask,
					  sizeof(mask)) == 0) {
			/* TODO: 按 mask 驱动均衡开关 GPIO */
			LOG_DBG("balancing mask[0]=0x%02x", mask[0]);
		}
	}
}

K_THREAD_DEFINE(bms_bal_tid, BAL_THREAD_STACK, bal_thread,
		NULL, NULL, NULL, BAL_THREAD_PRIO, 0, 0);

int bms_balancing_init(void)
{
	LOG_INF("Balancing init: passive-stub delta=%d mV", BAL_DELTA_MV);
	return 0;
}
