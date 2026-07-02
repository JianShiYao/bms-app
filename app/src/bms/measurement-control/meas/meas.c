/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    meas.c
 * @brief   测量采集边缘（bms_meas_acquire）—— 原始帧 → 可信帧。
 * @ingroup MEAS
 *
 * @details 数据源边缘（architecture.md「测量数据纪律」）：委托 hal/afe 后端取原始帧，
 *          经 bms_meas_validate 置 validity、盖时间戳，产出可信帧。合理性阈值来自
 *          Kconfig（CONFIG_BMS_MEAS_PLAUSIBLE_*）；纯校验逻辑见 meas_validate.c。
 */

#include <errno.h>
#include <stddef.h>

#include "bms/afe.h"
#include "bms/meas.h"

/* 正常构建由 app/Kconfig 提供；无 app Kconfig 的隔离测试场景回退默认值
 * （与 app/Kconfig 各 default 一致）。 */
#ifndef CONFIG_BMS_MEAS_PLAUSIBLE_CELL_MV_MIN
#define CONFIG_BMS_MEAS_PLAUSIBLE_CELL_MV_MIN 0
#endif
#ifndef CONFIG_BMS_MEAS_PLAUSIBLE_CELL_MV_MAX
#define CONFIG_BMS_MEAS_PLAUSIBLE_CELL_MV_MAX 6000
#endif
#ifndef CONFIG_BMS_MEAS_PLAUSIBLE_CURRENT_ABS_MAX_MA
#define CONFIG_BMS_MEAS_PLAUSIBLE_CURRENT_ABS_MAX_MA 300000
#endif
#ifndef CONFIG_BMS_MEAS_PLAUSIBLE_TEMP_DCI_MIN
#define CONFIG_BMS_MEAS_PLAUSIBLE_TEMP_DCI_MIN (-400)
#endif
#ifndef CONFIG_BMS_MEAS_PLAUSIBLE_TEMP_DCI_MAX
#define CONFIG_BMS_MEAS_PLAUSIBLE_TEMP_DCI_MAX 1250
#endif

/* 合理性校验阈值（来自 Kconfig）。语义为"读数是否物理可信"，非保护阈值。 */
static const struct bms_meas_limits MEAS_LIMITS = {
	.cell_mv_min = CONFIG_BMS_MEAS_PLAUSIBLE_CELL_MV_MIN,
	.cell_mv_max = CONFIG_BMS_MEAS_PLAUSIBLE_CELL_MV_MAX,
	.current_abs_max_ma = CONFIG_BMS_MEAS_PLAUSIBLE_CURRENT_ABS_MAX_MA,
	.temp_dci_min = CONFIG_BMS_MEAS_PLAUSIBLE_TEMP_DCI_MIN,
	.temp_dci_max = CONFIG_BMS_MEAS_PLAUSIBLE_TEMP_DCI_MAX,
};

int bms_meas_acquire(struct bms_cell_meas *out, uint32_t now_ms)
{
	if (out == NULL) {
		return -EINVAL;
	}

	/* 数据源边缘：acquire（hal/afe 后端）→ 盖时间戳 → validate（纯函数置 validity）→ 交业务层。
	 * 见 architecture.md「测量数据纪律」：业务层只看到带时间戳与有效位的可信帧。 */
	int ret = bms_afe_backend_read(out);

	if (ret != 0) {
		return ret;
	}
	out->timestamp_ms = now_ms;
	return bms_meas_validate(out, &MEAS_LIMITS);
}
