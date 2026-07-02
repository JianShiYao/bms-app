/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    contactor_io.h
 * @brief   接触器/MOS 输出与反馈的硬件抽象 seam（contactor_io wrapper）。
 * @ingroup CONT
 *
 * @details 落实 docs/concept/hardware-abstraction.md §2：contactor_io 收敛接触器/MOS
 *          的输出（执行期望态）与反馈（读实测状态），后端可切换——GPIO 或经 AFE。
 *          bms_f405 真实 backend 经 AFE(SH3673520) 属 Phase 3；本片仅提供 fake backend。
 */

#ifndef BMS_CONTACTOR_IO_H_
#define BMS_CONTACTOR_IO_H_

/*========== Includes ========================================================*/
#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*========== Macros and Definitions ==========================================*/

/*========== Extern Constant and Variable Declarations =======================*/

/*========== Extern Function Prototypes ======================================*/
/**
 * @brief 执行期望接触器态（由所选 backend——fake/真实——提供）。
 * @param desired 期望接触器状态（失效安全默认 OPEN）。
 */
void bms_contactor_io_apply(enum bms_contactor desired);

/**
 * @brief 读取接触器反馈（实测状态 + 预充状态）。
 * @param[out] out 输出接触器反馈快照（非空）。
 * @return 0 成功；@p out 为 NULL 返回 -EINVAL。
 */
int bms_contactor_io_read(struct bms_contactor_fb *out);

/**
 * @brief 【仅 fake backend】强制下次 @ref bms_contactor_io_read 返回的 actual，用于造不一致。
 * @param actual 注入的实测接触器状态。
 */
void bms_contactor_io_fake_set(enum bms_contactor actual);

/*========== Externalized Static Function Prototypes (Unit Test) =============*/

#ifdef __cplusplus
}
#endif

#endif /* BMS_CONTACTOR_IO_H_ */
