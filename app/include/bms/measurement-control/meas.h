/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    meas.h
 * @brief   测量可信化（measurement-control/meas）接口。
 * @ingroup MEAS
 *
 * @details 数据流 raw → meas 可信化 → DB_CELL_MEAS（docs/concept/data-model.md、
 *  architecture.md「测量数据纪律」）：`hal/afe` 后端只出**原始帧**（@ref bms_afe_backend_read），
 *  本层负责合理性校验（写 validity 位）+ 时间戳戳记，产出可信帧交业务层写入 DB。
 *  校验为纯函数（无线程/无 zbus/无副作用），可脱后端与线程单测。
 */

#ifndef BMS_MEAS_H_
#define BMS_MEAS_H_

/*========== Includes ========================================================*/
#include <stdint.h>

#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*========== Macros and Definitions ==========================================*/
/**
 * @brief 测量合理性校验阈值。
 *
 * 语义为「传感器读数是否物理可信」（量程/合理性），明确区别于 protection 的
 * 保护阈值（OV/UV/OC/OT）——后者判定「电池是否处于危险工况」。本结构很宽，只用于
 * 剔除明显坏帧（断线、量程溢出、ADC 异常）。
 */
struct bms_meas_limits {
	int32_t cell_mv_min;        /**< 单体电压合理下限 (mV) */
	int32_t cell_mv_max;        /**< 单体电压合理上限 (mV) */
	int32_t current_abs_max_ma; /**< 电流绝对值合理上限 (mA) */
	int32_t temp_dci_min;       /**< 温度合理下限 (0.1℃) */
	int32_t temp_dci_max;       /**< 温度合理上限 (0.1℃) */
};

/*========== Extern Constant and Variable Declarations =======================*/

/*========== Extern Function Prototypes ======================================*/
/**
 * @brief 对一帧测量做合理性校验，按结果设置 m->validity 位（纯函数）。
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
int bms_meas_validate(struct bms_cell_meas *m, const struct bms_meas_limits *lim);

/**
 * @brief 采集一帧可信测量：后端读原始帧 → 校验置 validity → 盖时间戳。
 *
 * 数据源边缘（architecture.md「测量数据纪律」）：委托 `hal/afe` 的 @ref bms_afe_backend_read
 * 取原始帧，经 @ref bms_meas_validate 置 validity，并把 out->timestamp_ms 戳为 @p now_ms，
 * 交业务层（写 DB / 上报诊断）。业务层只看到带时间戳与有效位的可信帧。
 *
 * @param[out] out    输出可信测量（非空）。
 * @param      now_ms 注入的采样时刻（单调毫秒），戳入 out->timestamp_ms。
 * @return 0 成功；负值为 errno（out 为 NULL 或后端读取失败）。
 */
int bms_meas_acquire(struct bms_cell_meas *out, uint32_t now_ms);

/*========== Externalized Static Function Prototypes (Unit Test) =============*/

#ifdef __cplusplus
}
#endif

#endif /* BMS_MEAS_H_ */
