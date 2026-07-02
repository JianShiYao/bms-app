/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    afe.h
 * @brief   AFE（电芯采样）模块接口。
 * @ingroup AFE
 */

#ifndef BMS_AFE_H_
#define BMS_AFE_H_

/*========== Includes ========================================================*/
#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*========== Macros and Definitions ==========================================*/

/*========== Extern Constant and Variable Declarations =======================*/

/*========== Extern Function Prototypes ======================================*/
/**
 * @brief 初始化 AFE 模块。
 * 采样周期由 bms_task 统一调度，本函数只初始化模块服务。
 * @return 0 成功，负值为 errno。
 */
int bms_afe_init(void);

/**
 * @brief AFE 后端读取接口（后端无关的采样 seam）。
 *
 * 设计来源：docs/concept/architecture.md「数据源后端可切换（afe）」。由所选后端按 Kconfig
 * 提供唯一实现：afe_stub.c（恒定桩）/ afe_sim.c（充放电仿真）/ afe_adc.c（真机 ADC）。
 * 硬件/仿真差异收敛到本函数实现，afe 线程与业务逻辑不变。
 *
 * @param[out] out 输出一帧测量（非空）。
 * @return 0 成功，负值为 errno。
 */
int bms_afe_backend_read(struct bms_cell_meas *out);

/*========== Externalized Static Function Prototypes (Unit Test) =============*/

#ifdef __cplusplus
}
#endif

#endif /* BMS_AFE_H_ */
