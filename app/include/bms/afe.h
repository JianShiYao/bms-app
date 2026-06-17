/*
 * AFE（电芯采样）模块接口
 */
#ifndef BMS_AFE_H_
#define BMS_AFE_H_

#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 AFE 模块。
 * 启动周期采样线程，向 chan_cell_meas 发布测量数据。
 * @return 0 成功，负值为 errno。
 */
int bms_afe_init(void);

/**
 * @brief 执行一次采样并填充测量结构（供线程与单测复用）。
 * native_sim 下产生桩数据；真实硬件读取 ADC/AFE 芯片。
 * @param out 输出测量数据
 * @return 0 成功，负值为 errno。
 */
int bms_afe_sample(struct bms_cell_meas *out);

#ifdef __cplusplus
}
#endif

#endif /* BMS_AFE_H_ */
