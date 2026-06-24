/*
 * AFE 桩后端（afe_stub）—— 恒定测量数据
 *
 * bms_afe_backend_read() 的最简实现：返回固定电压/电流/温度，确定性最强，
 * 用于最小化烟雾测试或 soc/protection 单独验证。需"会动"的数据请选 afe_sim 后端。
 * 见 docs/architecture.md「数据源后端可切换（afe）」。
 */
#include <errno.h>
#include <zephyr/kernel.h>

#include "bms/afe.h"
#include "bms/types.h"

int bms_afe_backend_read(struct bms_cell_meas *out)
{
	if (out == NULL) {
		return -EINVAL;
	}

	out->timestamp_ms = k_uptime_get_32();
	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		out->cell_mv[i] = 3700 + (i % 5) * 5; /* ~3.70V 附近 */
	}
	out->pack_current_ma = 1000; /* 1A 充电（正） */
	for (int i = 0; i < BMS_TEMP_SENSOR_COUNT; i++) {
		out->temp_dci[i] = 250; /* 25.0℃ */
	}

	return 0;
}
