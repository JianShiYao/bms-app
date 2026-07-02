/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    afe.c
 * @brief   AFE（电芯采样）模块 —— hal wrapper 边缘（初始化）。
 * @ingroup AFE
 *
 * @details hal/afe 职责：只出**原始帧**（@ref bms_afe_backend_read，由所选后端实现），
 *          不做有效性判定。测量可信化（validity / 时间戳）在 measurement-control/meas
 *          （见 bms/meas.h、architecture.md「测量数据纪律」）。周期调度由 bms_task 统一
 *          负责，本模块不自启动线程。采样后端按 Kconfig 三选一（afe_stub / afe_sim /
 *          afe_adc），见 architecture.md「数据源后端可切换（afe）」。
 */

#include <zephyr/logging/log.h>

#include "bms/afe.h"

LOG_MODULE_REGISTER(bms_afe, LOG_LEVEL_INF);

/* 正常构建由 app/Kconfig 提供；无 app Kconfig 的隔离测试场景回退默认值
 * （与 app/Kconfig default 一致；同 afe_sim.c / task.c 的回退惯例）。 */
#ifndef CONFIG_BMS_AFE_SAMPLE_PERIOD_MS
#define CONFIG_BMS_AFE_SAMPLE_PERIOD_MS 100
#endif

int bms_afe_init(void)
{
	LOG_INF("AFE init: period=%d ms, cells=%d, temps=%d", CONFIG_BMS_AFE_SAMPLE_PERIOD_MS,
		BMS_CELL_COUNT, BMS_TEMP_SENSOR_COUNT);
	return 0;
}
