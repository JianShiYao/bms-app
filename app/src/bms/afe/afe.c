/*
 * AFE（电芯采样）模块 —— 桩实现
 *
 * 职责：周期采集电压/电流/温度，发布到 chan_cell_meas。
 * native_sim 下产生桩数据；真实硬件接 ADC 或专用 AFE 芯片（驱动留 drivers/）。
 */
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bms/afe.h"
#include "bms/channels.h"

LOG_MODULE_REGISTER(bms_afe, LOG_LEVEL_INF);

#define AFE_THREAD_STACK 1024
#define AFE_THREAD_PRIO  6

int bms_afe_sample(struct bms_cell_meas *out)
{
	if (out == NULL) {
		return -EINVAL;
	}

	out->timestamp_ms = k_uptime_get_32();

	/* TODO: 替换为真实 ADC / AFE 芯片读取。以下为 native_sim 桩数据。 */
	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		out->cell_mv[i] = 3700 + (i % 5) * 5; /* ~3.70V 附近 */
	}
	out->pack_current_ma = 1000; /* 1A 充电（正） */
	for (int i = 0; i < BMS_TEMP_SENSOR_COUNT; i++) {
		out->temp_dci[i] = 250; /* 25.0℃ */
	}

	return 0;
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
