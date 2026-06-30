/*
 * CAN 上报周期合法化 —— 纯函数（comm_period）
 *
 * 详见 bms/comm.h 的 bms_comm_clamp_period_ms()。把请求周期钳制到合法闭区间
 * [lo, hi]，是「编译期 Kconfig range + 运行期 clamp」双保险中运行期那一侧。
 * 无线程、无 zbus、无 Kconfig 宏、无副作用，边界以入参注入，供 comm 线程与
 * ztest 直接复用（对齐 afe_validate 纯函数与线程分离的范式）。
 *
 * (DES-COMM-002; REQ-COMM-004/005/007)
 */
#include <stdint.h>

#include "bms/comm.h"

int32_t bms_comm_clamp_period_ms(int32_t requested, int32_t lo, int32_t hi)
{
	if (requested < lo) {
		/* 覆盖 requested<=0 与 0<requested<lo 两类；因 lo>0 故结果恒>0。 */
		return lo;
	}
	if (requested > hi) {
		return hi;
	}
	return requested;
}
