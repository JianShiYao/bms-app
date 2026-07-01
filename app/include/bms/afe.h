/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    afe.h
 * @brief   AFE（电芯采样）模块接口。
 * @ingroup AFE
 */

#ifndef BMS_AFE_H_
#define BMS_AFE_H_

#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 AFE 模块。
 * 采样周期由 bms_task 统一调度，本函数只初始化模块服务。
 * @return 0 成功，负值为 errno。
 */
int bms_afe_init(void);

/**
 * @brief 执行一次采样并填充测量结构（供线程与单测复用）。
 * 业务层入口：委托给所选后端的 bms_afe_backend_read()，不感知后端种类。
 * @param[out] out 输出测量数据
 * @return 0 成功，负值为 errno。
 */
int bms_afe_sample(struct bms_cell_meas *out);

/**
 * @brief AFE 后端读取接口（后端无关的采样 seam）。
 *
 * 设计来源：docs/design/concept-architecture.md「数据源后端可切换（afe）」。由所选后端按 Kconfig
 * 提供唯一实现：afe_stub.c（恒定桩）/ afe_sim.c（充放电仿真）/ afe_adc.c（真机 ADC）。
 * 硬件/仿真差异收敛到本函数实现，afe 线程与业务逻辑不变。
 *
 * @param[out] out 输出一帧测量（非空）。
 * @return 0 成功，负值为 errno。
 */
int bms_afe_backend_read(struct bms_cell_meas *out);

/**
 * @brief AFE 数据合理性校验阈值。
 *
 * 语义为「传感器读数是否物理可信」（量程/合理性），明确区别于 protection 的
 * 保护阈值（OV/UV/OC/OT）——后者判定"电池是否处于危险工况"。本结构很宽，只用于
 * 剔除明显坏帧（断线、量程溢出、ADC 异常）。
 */
struct bms_afe_limits {
	int32_t cell_mv_min;        /**< 单体电压合理下限 (mV) */
	int32_t cell_mv_max;        /**< 单体电压合理上限 (mV) */
	int32_t current_abs_max_ma; /**< 电流绝对值合理上限 (mA) */
	int32_t temp_dci_min;       /**< 温度合理下限 (0.1℃) */
	int32_t temp_dci_max;       /**< 温度合理上限 (0.1℃) */
};

/**
 * @brief 对一帧测量做合理性校验，按结果设置 m->validity 位（有状态纯函数）。
 *
 * 设计来源：docs/design/concept-architecture.md「测量数据纪律」——校验与采集分离、校验段为纯函数，
 * 由数据源「边缘」（bms_afe_sample）在 backend_read 之后调用：任一后端（sim/stub/adc）
 * 产出的原始帧都过同一校验，业务层只看到带 validity 的可信帧。
 *
 * 规则（任一量越界则清除其对应有效位，全部在界内则置 BMS_MEAS_VALID_ALL）：
 *  - VOLTAGE：所有 cell_mv[i] ∈ [cell_mv_min, cell_mv_max]
 *  - CURRENT：|pack_current_ma| ≤ current_abs_max_ma
 *  - TEMP   ：所有 temp_dci[i] ∈ [temp_dci_min, temp_dci_max]
 * 仅写 m->validity，不修改任何测量值。
 *
 * @param[in,out] m   输入/输出测量（非空）；读取各量、写入 validity。
 * @param[in]     lim 合理性阈值（非空）。
 * @return 0 成功；-EINVAL（m 或 lim 为 NULL，不写 m）。
 */
int bms_afe_validate(struct bms_cell_meas *m, const struct bms_afe_limits *lim);

#ifdef __cplusplus
}
#endif

#endif /* BMS_AFE_H_ */
