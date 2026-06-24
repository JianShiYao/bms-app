/*
 * BMS 应用入口
 *
 * main 仅做初始化与健康监测；各 BMS 模块的工作线程由模块内 K_THREAD_DEFINE 自启。
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bms/afe.h"
#include "bms/soc.h"
#include "bms/protection.h"
#include "bms/balancing.h"
#include "bms/comm.h"

LOG_MODULE_REGISTER(bms_main, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("==== BMS firmware starting on %s ====", CONFIG_BOARD);

	if (IS_ENABLED(CONFIG_BMS_AFE)) {
		bms_afe_init();
	}
	if (IS_ENABLED(CONFIG_BMS_SOC)) {
		bms_soc_init();
	}
	if (IS_ENABLED(CONFIG_BMS_PROTECTION)) {
		bms_protection_init();
	}
	if (IS_ENABLED(CONFIG_BMS_BALANCING)) {
		bms_balancing_init();
	}
	if (IS_ENABLED(CONFIG_BMS_COMM)) {
		bms_comm_init();
	}

	LOG_INF("BMS modules initialized");

	/* 健康监测：周期打印存活信息（后续可接看门狗喂狗） */
	while (1) {
		k_sleep(K_SECONDS(5));
	}

	return 0;
}
