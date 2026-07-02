/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    protection.h
 * @brief   保护状态机模块接口。
 * @ingroup PROT
 */

#ifndef BMS_PROTECTION_H_
#define BMS_PROTECTION_H_

/*========== Includes ========================================================*/
#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*========== Macros and Definitions ==========================================*/
/** 保护阈值（桩默认值，后续按电芯规格表配置/Kconfig 化） */
struct bms_prot_limits {
	int32_t cell_ov_mv;      /**< 单体过压阈值 mV */
	int32_t cell_uv_mv;      /**< 单体欠压阈值 mV */
	int32_t over_current_ma; /**< 过流阈值 mA（绝对值） */
	int32_t over_temp_dci;   /**< 过温阈值 0.1℃ */
};

/*========== Extern Constant and Variable Declarations =======================*/

/*========== Extern Function Prototypes ======================================*/
/**
 * @brief 初始化保护模块。失效安全：默认接触器断开。
 * 调度与诊断登记由 bms_task/bms_diag 统一负责。
 * @return 0 成功，负值为 errno。
 */
int bms_protection_init(void);

/**
 * @brief 纯函数：根据测量与阈值评估保护状态（供线程与单测复用）。
 * @param[in]  meas   输入测量
 * @param[in]  limits 阈值
 * @param[out] out    输出保护事件（含期望接触器状态）
 * @return 0 成功，负值为 errno。
 */
int bms_protection_evaluate(const struct bms_cell_meas *meas, const struct bms_prot_limits *limits,
			    struct bms_prot_evt *out);

/**
 * @brief 获取默认保护阈值（桩值）。
 * @param[out] limits 输出默认阈值（非空）。
 */
void bms_protection_default_limits(struct bms_prot_limits *limits);

/*========== Externalized Static Function Prototypes (Unit Test) =============*/

#ifdef __cplusplus
}
#endif

#endif /* BMS_PROTECTION_H_ */
