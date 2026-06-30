/*
 * 单体均衡模块接口
 */
#ifndef BMS_BALANCING_H_
#define BMS_BALANCING_H_

#include <stddef.h>
#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化均衡模块。
 * 调度与执行由 bms_task 统一负责。
 * @return 0 成功，负值为 errno。
 */
int bms_balancing_init(void);

/**
 * @brief 纯函数：根据测量计算需均衡的单体位掩码（供线程与单测复用）。
 * 当前为桩实现（高于最低电压 + 阈值的单体置位）。
 * @param meas       输入测量
 * @param delta_mv   触发均衡的压差阈值 mV
 * @param mask_out   输出位掩码（bit i = 第 i 串需均衡），长度需 >= ceil(CELL_COUNT/8)
 * @param mask_len   mask_out 字节数
 * @return 0 成功，负值为 errno。
 */
int bms_balancing_compute(const struct bms_cell_meas *meas, int32_t delta_mv, uint8_t *mask_out,
			  size_t mask_len);

#ifdef __cplusplus
}
#endif

#endif /* BMS_BALANCING_H_ */
