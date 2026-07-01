/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    bms.h
 * @brief   BMS 主状态机接口。
 * @ingroup SYS
 */

#ifndef BMS_BMS_H_
#define BMS_BMS_H_

#include <stdbool.h>

#include "bms/diag.h"
#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** BMS 主状态机的决策输入。 */
struct bms_state_inputs {
	bool close_allowed;         /**< 是否允许闭合接触器 */
	bool precharge_complete;    /**< 预充完成（电压爬升达标）；PRECHARGE→NORMAL 门控 */
	bool precharge_timeout;     /**< 预充超时；PRECHARGE→FAULT 门控 */
	bool open_request;          /**< 外部断开请求 */
	bool hw_fault_latched;      /**< 硬件故障已锁存 */
	struct bms_diag_state diag; /**< 诊断状态快照 */
	struct bms_prot_evt prot;   /**< 保护事件快照 */
};

/**
 * @brief 纯函数：由当前状态与输入计算下一 BMS 状态（供线程与单测复用）。
 * @details 失效安全：诊断达 ERROR、保护非 NORMAL 或有断开请求时一律转 FAULT。
 * @param      cur 当前状态
 * @param[in]  in  状态机输入（非空）
 * @return 下一 BMS 状态。
 */
enum bms_state bms_next_state(enum bms_state cur, const struct bms_state_inputs *in);

/**
 * @brief 纯函数：给出某 BMS 状态下期望的接触器状态。
 * @param state BMS 状态
 * @return 期望接触器状态（仅 NORMAL 闭合，其余一律断开）。
 */
enum bms_contactor bms_contactor_for_state(enum bms_state state);

#ifdef __cplusplus
}
#endif

#endif /* BMS_BMS_H_ */
