/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    time.c
 * @brief   engine 时间基准实现：唯一单调毫秒时间源（可注入）与回绕安全纯函数。
 * @ingroup SYS
 */

#include <zephyr/kernel.h>

#include "bms/time.h"

static uint32_t default_source(void)
{
	return k_uptime_get_32();
}

static uint32_t (*s_source)(void) = default_source;

uint32_t bms_time_now_ms(void)
{
	return s_source();
}

bool bms_time_after(uint32_t now, uint32_t deadline)
{
	return (int32_t)(now - deadline) >= 0;
}

bool bms_time_due(uint32_t now, uint32_t *next, uint32_t period_ms)
{
	if ((int32_t)(now - *next) < 0) {
		return false;
	}
	*next += period_ms;
	if ((int32_t)(now - *next) >= 0) {
		*next = now + period_ms;
	}
	return true;
}

void bms_time_set_source(uint32_t (*source)(void))
{
	if (source != NULL) {
		s_source = source;
	} else {
		s_source = default_source;
	}
}
