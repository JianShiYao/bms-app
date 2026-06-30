/*
 * CAN 通信模块接口
 */
#ifndef BMS_COMM_H_
#define BMS_COMM_H_

#include <stdint.h>

#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化通信模块。
 * 调度由 bms_task 统一负责；native_sim 下无真实 CAN，走日志桩。
 * native_sim 下无真实 CAN，走日志桩；真实硬件接 can1。
 * @return 0 成功，负值为 errno。
 */
int bms_comm_init(void);

int32_t bms_comm_effective_period_ms(void);

void bms_comm_tx_snapshot(const struct bms_cell_meas *meas, const struct bms_soc *soc,
			  const struct bms_prot_evt *prot);

/**
 * @brief 纯函数：把请求的上报周期(ms)钳制到合法闭区间 [lo, hi]（供线程与单测复用）。
 *
 * 无副作用、不依赖全局状态/硬件/Kconfig 宏；边界以入参注入，便于 host 单测
 * （对齐 bms_afe_validate(m, limits) 的「边界入参注入」范式）。
 *
 * 钳制规则（确定且唯一）：
 *   requested <  lo  -> 返回 lo   （含 requested <= 0 的全部情形，因 lo > 0）
 *   requested >  hi  -> 返回 hi
 *   否则             -> 返回 requested 原值
 *
 * @param requested 请求周期，单位 ms（允许任意 int32_t，含 0/负数/越界）
 * @param lo        合法下界，单位 ms；调用方须保证 lo > 0 且 lo <= hi
 * @param hi        合法上界，单位 ms
 * @return 钳制后的生效周期，单位 ms。
 *
 * 不变式（前置条件成立时）：lo <= 返回值 <= hi 且 返回值 >= lo > 0（即恒 > 0）。
 * 前置条件：0 < lo <= hi（由编译期 range 与单一调用点共同保证）。
 *
 * (DES-COMM-002; REQ-COMM-004/005/007)
 */
int32_t bms_comm_clamp_period_ms(int32_t requested, int32_t lo, int32_t hi);

#ifdef __cplusplus
}
#endif

#endif /* BMS_COMM_H_ */
