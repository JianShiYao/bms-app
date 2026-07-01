/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    main.c
 * @brief   BMS 应用入口。
 * @ingroup SYS
 *
 * @details main 只做 engine/module 初始化；长期运行逻辑由 bms_task 统一调度。
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bms/afe.h"
#include "bms/balancing.h"
#include "bms/comm.h"
#include "bms/db.h"
#include "bms/diag.h"
#include "bms/protection.h"
#include "bms/soc.h"
#include "bms/task.h"

LOG_MODULE_REGISTER(bms_main, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("==== BMS firmware starting on %s ====", CONFIG_BOARD);

	bms_db_init();
	bms_diag_init();

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

	bms_task_init();

	LOG_INF("BMS engine initialized");

	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
