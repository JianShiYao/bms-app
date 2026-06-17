/*
 * SOC/SOH 估算模块接口
 */
#ifndef BMS_SOC_H_
#define BMS_SOC_H_

#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 SOC 模块（启动线程，订阅 chan_cell_meas）。
 * @return 0 成功，负值为 errno。
 */
int bms_soc_init(void);

/**
 * @brief 由一帧测量估算 SOC/SOH（供线程与单测复用）。
 * 当前为桩实现（电压线性映射）；后续替换为库仑积分/卡尔曼。
 * @param meas 输入测量
 * @param out  输出 SOC/SOH
 * @return 0 成功，负值为 errno。
 */
int bms_soc_estimate(const struct bms_cell_meas *meas, struct bms_soc *out);

#ifdef __cplusplus
}
#endif

#endif /* BMS_SOC_H_ */
