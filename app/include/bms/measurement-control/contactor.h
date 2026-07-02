/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    contactor.h
 * @brief   接触器抽象：执行 bms_bms 期望态并反馈实测状态、诊断反馈不一致。
 * @ingroup CONT
 *
 * @details 落实 docs/concept/architecture.md §7（bms_contactor 执行期望接触器态并反馈
 *          实际状态）与 §8（诊断＝反馈不一致 / 预充超时 / 粘连检测失败）。反馈快照写入
 *          DB_CONTACTOR_FB（owner=bms_contactor，见 data-model.md）。本片不接 task.c。
 */

#ifndef BMS_CONTACTOR_H_
#define BMS_CONTACTOR_H_

#include <stdbool.h>

#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 纯判定：反馈是否与期望不一致（actual != desired）。
 * @param desired 期望接触器状态。
 * @param actual  实测接触器状态（反馈）。
 * @return true 表示反馈与期望不一致（含粘连/拒动）。
 */
bool bms_contactor_eval(enum bms_contactor desired, enum bms_contactor actual);

/**
 * @brief 一步：执行期望态→读反馈→写 DB_CONTACTOR_FB→按 @ref bms_contactor_eval
 *        上报 BMS_DIAG_CONTACTOR_MISMATCH。
 * @param desired 本周期期望接触器状态（失效安全默认 OPEN）。
 * @param now_ms  注入的单调毫秒时间（用于诊断去抖/锁存与反馈时间戳）。
 */
void bms_contactor_step(enum bms_contactor desired, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* BMS_CONTACTOR_H_ */
