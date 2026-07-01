/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    afe_adc.c
 * @brief   AFE 真机后端（afe_adc）—— 占位。
 * @ingroup AFE
 *
 * @details bms_afe_backend_read() 的真机实现：经 devicetree 取 ADC / 专用 AFE 芯片设备
 *          （DEVICE_DT_GET + adc_read / SPI），走 Zephyr 设备 API，业务层零改动。
 *          见 docs/design/concept-architecture.md「数据源后端可切换（afe）」与「分层铁律」。
 *
 *          当前为占位：待 boards/enervenue/bms_f405 的 dts（ADC 通道、AFE 芯片节点）就绪后实现。
 *          仅在 CONFIG_BMS_AFE_BACKEND_ADC=y（真机板）时编译，QEMU/native_sim 不选此后端。
 */

#include <errno.h>
#include <zephyr/sys/util.h>

#include "bms/afe.h"
#include "bms/types.h"

int bms_afe_backend_read(struct bms_cell_meas *out)
{
	ARG_UNUSED(out);
	/* TODO(bms_f405)：DEVICE_DT_GET(ADC/AFE) → 采样 → 填充 out。 */
	return -ENOSYS;
}
