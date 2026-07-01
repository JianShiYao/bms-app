/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    afe_validate.c
 * @brief   AFE 测量合理性校验（afe_validate）—— 纯函数。
 * @ingroup AFE
 *
 * @details 详见 bms/afe.h 的 bms_afe_validate()。校验与采集分离（架构「测量数据纪律」）：
 *          afe 边缘在 backend_read 之后调用本函数，剔除明显坏帧并写 validity 位。
 *          无线程、无 zbus、无副作用，供 afe 线程与 ztest 直接复用。
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>

#include "bms/afe.h"
#include "bms/types.h"

int bms_afe_validate(struct bms_cell_meas *m, const struct bms_afe_limits *lim)
{
	if ((m == NULL) || (lim == NULL)) {
		return -EINVAL;
	}

	uint8_t valid = 0;

	/* 电压：所有串都在合理量程内才置位 */
	bool voltage_ok = true;

	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		if ((m->cell_mv[i] < lim->cell_mv_min) || (m->cell_mv[i] > lim->cell_mv_max)) {
			voltage_ok = false;
			break;
		}
	}
	if (voltage_ok) {
		valid |= BMS_MEAS_VALID_VOLTAGE;
	}

	/* 电流：绝对值不超合理上限 */
	int32_t abs_ma = (m->pack_current_ma < 0) ? -m->pack_current_ma : m->pack_current_ma;

	if (abs_ma <= lim->current_abs_max_ma) {
		valid |= BMS_MEAS_VALID_CURRENT;
	}

	/* 温度：所有通道都在合理量程内才置位 */
	bool temp_ok = true;

	for (int i = 0; i < BMS_TEMP_SENSOR_COUNT; i++) {
		if ((m->temp_dci[i] < lim->temp_dci_min) || (m->temp_dci[i] > lim->temp_dci_max)) {
			temp_ok = false;
			break;
		}
	}
	if (temp_ok) {
		valid |= BMS_MEAS_VALID_TEMP;
	}

	m->validity = valid; /* 仅写 validity，不改任何测量值 */
	return 0;
}
