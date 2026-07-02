/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    time.h
 * @brief   engine 时间基准：全系统唯一单调毫秒时间源与回绕安全的到期/比较纯函数。
 * @ingroup SYS
 *
 * @details 落实运行时模型契约（docs/concept/runtime-model.md §2）：
 *  - @ref bms_time_now_ms 是全系统**唯一**单调递增毫秒时间源；业务模块与纯函数
 *    不得直接调用内核时间 API，一切时间访问经本模块，以便单测注入。
 *  - 所有时间比较**必须**用有符号差值以回绕安全（`(int32_t)(now - deadline) >= 0`），
 *    不得对时间戳做无符号大小直接比较。
 *  - 时钟源可注入（@ref bms_time_set_source），使到期/超时/stale/心跳判定脱离内核单测。
 */

#ifndef BMS_TIME_H_
#define BMS_TIME_H_

/*========== Includes ========================================================*/
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*========== Macros and Definitions ==========================================*/

/*========== Extern Constant and Variable Declarations =======================*/

/*========== Extern Function Prototypes ======================================*/
/**
 * @brief 返回全系统唯一的单调递增毫秒时间。
 * @details 默认由内核 `k_uptime_get_32()` 提供；可经 @ref bms_time_set_source 注入替换。
 * @return 当前单调毫秒时间戳（32 位，允许回绕）。
 */
uint32_t bms_time_now_ms(void);

/**
 * @brief 回绕安全地判定 now 是否已达到/越过 deadline。
 * @details 语义为有符号差值 `(int32_t)(now - deadline) >= 0`（运行时模型 §2）。
 *          now == deadline 视为已到；跨 32 位回绕仍正确（差值落在半周期内时）。
 * @param now       当前时间戳。
 * @param deadline  目标时间戳。
 * @return now 达到或越过 deadline 时返回 true，否则 false。
 */
bool bms_time_after(uint32_t now, uint32_t deadline);

/**
 * @brief 纯周期到期判定（无副作用输入注入时间；到期则推进下次触发点）。
 * @details 参考既有 `due_u32`（本增量将其提升为本函数）：
 *          - 未到期（`(int32_t)(now - *next) < 0`）返回 false，*next 不变；
 *          - 到期返回 true 并 `*next += period_ms`；
 *          - 若推进后仍已过期（回绕/滞后），把 `*next` 重置为 `now + period_ms`，
 *            以防回绕后疯狂追赶（运行时模型 §4）。
 * @param now        当前时间戳。
 * @param next       下次触发时间戳（就地更新）。
 * @param period_ms  周期（毫秒）。
 * @return 本次到期返回 true，否则 false。
 */
bool bms_time_due(uint32_t now, uint32_t *next, uint32_t period_ms);

/**
 * @brief 注入时钟源，用于测试脱离内核或将来切换更高精度基准。
 * @param source 返回单调毫秒的函数指针；传 NULL 复位为默认内核源（k_uptime_get_32）。
 */
void bms_time_set_source(uint32_t (*source)(void));

/*========== Externalized Static Function Prototypes (Unit Test) =============*/

#ifdef __cplusplus
}
#endif

#endif /* BMS_TIME_H_ */
