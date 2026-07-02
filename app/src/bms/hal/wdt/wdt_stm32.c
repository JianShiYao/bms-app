/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    wdt_stm32.c
 * @brief   看门狗 STM32 IWDG 后端（真板 bms_f405）。
 * @ingroup WDT
 *
 * @details bms/wdt.h 的真板后端：经 Zephyr wdt 驱动操作 STM32 内部独立看门狗（IWDG）。
 *  init 安装超时通道并启动；feed 喂狗。超时 CONFIG_BMS_WDT_TIMEOUT_MS，须比喂狗周期
 *  （safety 10ms）大得多。真实复位行为须真板/HIL 验证（runtime-model §7）。
 *  仅在 CONFIG_BMS_WDT_BACKEND_STM32 下**编译并链接**（见 app/CMakeLists / app/Kconfig）。
 *
 *  DT guard：仅当 devicetree 存在 okay 的 iwdg 节点（真板）才走真实实现；无 iwdg 时
 *  （如 clang-tidy 以 native_sim 编译库分析本文件）编译占位分支，避免对不存在节点
 *  DEVICE_DT_GET。真板 bms_f405 链接的是真实分支。
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/watchdog.h>

#include "bms/hal/wdt.h"

#ifndef CONFIG_BMS_WDT_TIMEOUT_MS
#define CONFIG_BMS_WDT_TIMEOUT_MS 1000
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(iwdg), okay)

static const struct device *wdt_dev = DEVICE_DT_GET(DT_NODELABEL(iwdg));
static int wdt_channel = -1;

int bms_wdt_init(void)
{
	if (!device_is_ready(wdt_dev)) {
		return -ENODEV;
	}

	const struct wdt_timeout_cfg cfg = {
		.window = {.min = 0U, .max = CONFIG_BMS_WDT_TIMEOUT_MS},
		.callback = NULL,
		.flags = WDT_FLAG_RESET_SOC,
	};

	wdt_channel = wdt_install_timeout(wdt_dev, &cfg);
	if (wdt_channel < 0) {
		return wdt_channel;
	}

	return wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
}

void bms_wdt_feed(void)
{
	if (wdt_channel >= 0) {
		(void)wdt_feed(wdt_dev, wdt_channel);
	}
}

#else /* 无 okay 的 iwdg 节点（非真板 DT）：占位，真板 bms_f405 不走此分支。 */

int bms_wdt_init(void)
{
	return -ENODEV;
}

void bms_wdt_feed(void)
{
}

#endif
