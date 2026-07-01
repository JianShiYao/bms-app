/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    afe.c
 * @brief   AFE（电芯采样）模块 —— 测量服务。
 * @ingroup AFE
 *
 * @details 职责：采集电压/电流/温度，并执行测量可信化。
 *          周期调度由 bms_task 统一负责，本模块不再自启动线程。
 *          采样实现按 Kconfig 选后端（afe_stub / afe_sim / afe_adc），业务逻辑不变，
 *          见 docs/concept/architecture.md「数据源后端可切换（afe）」。
 */

#include <errno.h>
#include <zephyr/logging/log.h>

#include "bms/afe.h"

LOG_MODULE_REGISTER(bms_afe, LOG_LEVEL_INF);

/* 合理性校验阈值（来自 Kconfig）。语义为"读数是否物理可信"，非保护阈值。 */
static const struct bms_afe_limits AFE_LIMITS = {
	.cell_mv_min = CONFIG_BMS_AFE_PLAUSIBLE_CELL_MV_MIN,
	.cell_mv_max = CONFIG_BMS_AFE_PLAUSIBLE_CELL_MV_MAX,
	.current_abs_max_ma = CONFIG_BMS_AFE_PLAUSIBLE_CURRENT_ABS_MAX_MA,
	.temp_dci_min = CONFIG_BMS_AFE_PLAUSIBLE_TEMP_DCI_MIN,
	.temp_dci_max = CONFIG_BMS_AFE_PLAUSIBLE_TEMP_DCI_MAX,
};

int bms_afe_sample(struct bms_cell_meas *out)
{
	if (out == NULL) {
		return -EINVAL;
	}

	/* 数据源边缘：acquire（所选后端）→ validate（纯函数置 validity）→ 交业务层。
	 * 见 docs/concept/architecture.md「测量数据纪律」：业务层只看到带有效位的可信帧。 */
	int ret = bms_afe_backend_read(out);

	if (ret != 0) {
		return ret;
	}
	return bms_afe_validate(out, &AFE_LIMITS);
}

int bms_afe_init(void)
{
	LOG_INF("AFE init: period=%d ms, cells=%d, temps=%d", CONFIG_BMS_AFE_SAMPLE_PERIOD_MS,
		BMS_CELL_COUNT, BMS_TEMP_SENSOR_COUNT);
	return 0;
}
