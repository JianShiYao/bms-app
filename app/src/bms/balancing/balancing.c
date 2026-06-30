/*
 * 单体均衡模块 —— 策略服务
 *
 * 职责：计算需均衡的单体位掩码。
 * 调度与执行由 bms_task 统一负责。
 * 当前为被动均衡策略桩：高于最低单体电压 + delta 的单体置位。
 */
#include <errno.h>
#include <string.h>
#include <zephyr/logging/log.h>

#include "bms/balancing.h"

LOG_MODULE_REGISTER(bms_balancing, LOG_LEVEL_INF);

#define BAL_DELTA_MV 20 /* 压差阈值，桩 */

#define BAL_MASK_BYTES ((BMS_CELL_COUNT + 7) / 8)

int bms_balancing_compute(const struct bms_cell_meas *meas, int32_t delta_mv, uint8_t *mask_out,
			  size_t mask_len)
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

int bms_balancing_init(void)
{
	LOG_INF("Balancing init: passive-stub delta=%d mV", BAL_DELTA_MV);
	return 0;
}
