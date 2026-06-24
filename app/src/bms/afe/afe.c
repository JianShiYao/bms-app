/*
 * AFE（电芯采样）模块 —— 线程编排
 *
 * 职责：周期采集电压/电流/温度，发布到 chan_cell_meas。
 * 采样实现按 Kconfig 选后端（afe_stub / afe_sim / afe_adc），业务逻辑不变，
 * 见 docs/architecture.md「数据源后端可切换（afe）」。
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bms/afe.h"
#include "bms/channels.h"

LOG_MODULE_REGISTER(bms_afe, LOG_LEVEL_INF);

#define AFE_THREAD_STACK 1024
#define AFE_THREAD_PRIO  6

/* 合理性校验阈值（来自 Kconfig）。语义为"读数是否物理可信"，非保护阈值。 */
static const struct bms_afe_limits afe_limits = {
	.cell_mv_min = CONFIG_BMS_AFE_PLAUSIBLE_CELL_MV_MIN,
	.cell_mv_max = CONFIG_BMS_AFE_PLAUSIBLE_CELL_MV_MAX,
	.current_abs_max_ma = CONFIG_BMS_AFE_PLAUSIBLE_CURRENT_ABS_MAX_MA,
	.temp_dci_min = CONFIG_BMS_AFE_PLAUSIBLE_TEMP_DCI_MIN,
	.temp_dci_max = CONFIG_BMS_AFE_PLAUSIBLE_TEMP_DCI_MAX,
};

int bms_afe_sample(struct bms_cell_meas *out)
{
	/* 数据源边缘：acquire（所选后端）→ validate（纯函数置 validity）→ 交业务层。
	 * 见 docs/architecture.md「测量数据纪律」：业务层只看到带有效位的可信帧。 */
	int ret = bms_afe_backend_read(out);

	if (ret != 0) {
		return ret;
	}
	return bms_afe_validate(out, &afe_limits);
}

static void afe_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct bms_cell_meas meas;

	while (1) {
		if (bms_afe_sample(&meas) == 0) {
			int ret = zbus_chan_pub(&chan_cell_meas, &meas, K_MSEC(50));

			if (ret != 0) {
				LOG_WRN("publish chan_cell_meas failed: %d", ret);
			}
		}
		k_msleep(CONFIG_BMS_AFE_SAMPLE_PERIOD_MS);
	}
}

K_THREAD_DEFINE(bms_afe_tid, AFE_THREAD_STACK, afe_thread, NULL, NULL, NULL, AFE_THREAD_PRIO, 0, 0);

int bms_afe_init(void)
{
	LOG_INF("AFE init: period=%d ms, cells=%d, temps=%d", CONFIG_BMS_AFE_SAMPLE_PERIOD_MS,
		BMS_CELL_COUNT, BMS_TEMP_SENSOR_COUNT);
	return 0;
}
