/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    wdt_stub.c
 * @brief   看门狗 stub 后端（sim 板：无硬件狗）。
 * @ingroup WDT
 *
 * @details 详见 bms/wdt.h。sim 板（native_sim / mps2 / qmxx）无硬件看门狗，本后端以
 *  软件计数记录喂狗行为，供测试观测门控是否放行喂狗（runtime-model §7）。桩不驱动
 *  真实复位，但**不放宽安全默认**——喂/不喂完全由上层 bms_sys_mon_wdt_feed_allowed 决定。
 */

/*========== Includes ========================================================*/
#include <stdint.h>

#include "bms/hal/wdt.h"

/*========== Macros and Definitions ==========================================*/

/*========== Static Constant and Variable Definitions ========================*/
static uint32_t feed_count;

/*========== Extern Constant and Variable Definitions ========================*/

/*========== Static Function Prototypes ======================================*/

/*========== Static Function Implementations =================================*/

/*========== Extern Function Implementations =================================*/
int bms_wdt_init(void)
{
	/* stub 后端：无硬件狗，初始化无副作用。 */
	feed_count = 0U;
	return 0;
}

void bms_wdt_feed(void)
{
	feed_count++;
}

uint32_t bms_wdt_stub_feed_count(void)
{
	return feed_count;
}

/*========== Externalized Static Function Implementations (Unit Test) ========*/
