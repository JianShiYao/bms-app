/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    task.h
 * @brief   foxBMS 2 inspired 任务框架（映射到 Zephyr 线程）接口。
 * @ingroup SYS
 */

#ifndef BMS_TASK_H_
#define BMS_TASK_H_

/*========== Includes ========================================================*/
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*========== Macros and Definitions ==========================================*/

/*========== Extern Constant and Variable Declarations =======================*/

/*========== Extern Function Prototypes ======================================*/
/**
 * @brief 初始化任务框架状态并写入初始安全态（不启动线程）。
 * @details 按 Kconfig 复位 SOC 与保护状态，写入初始 BMS 状态（失效安全：接触器 OPEN）。
 *          语义将改为「仅初始化，不起线程」——线程启动交由 @ref bms_task_start。
 * @return 0 成功。
 */
int bms_task_init(void);

/**
 * @brief 启动 安全/应用/后台 三个静态线程（供 main 调用；测试不调用它）。
 * @details 前置条件：已调用 @ref bms_task_init。
 */
void bms_task_start(void);

/**
 * @brief 执行一轮 safety 周期编排（采样到期消化→保护判定→BMS 状态机→写 DB）。
 * @details 供 main 的 safety 线程与集成测试复用，把周期线程体拆成可注入时间的单步编排。
 *          安全链顺序遵守 docs/concept/architecture.md：采样有效性→诊断聚合→BMS 状态机→接触器输出。
 * @param now_ms 注入的当前单调毫秒时间（由调用方提供，便于测试脱离内核时钟）。
 */
void bms_task_safety_step(uint32_t now_ms);

/**
 * @brief 执行一轮 app 周期编排（SOC/均衡/周期上报）。
 * @details 供 main 的 app 线程与集成测试复用；now_ms 为注入时间。
 * @param now_ms 注入的当前单调毫秒时间。
 */
void bms_task_app_step(uint32_t now_ms);

/**
 * @brief 按 sys_mon 门控喂硬 watchdog（仅安全关键任务健康时才喂）。
 * @details 落实 docs/concept/runtime-model.md §7：当且仅当
 *          @ref bms_sys_mon_wdt_feed_allowed 返回 true（所有安全关键任务已 seen
 *          且健康）时调用 @ref bms_wdt_feed；否则停喂（失效安全 → watchdog 复位）。
 *          供 main 的 safety 线程与集成测试复用；now_ms 为注入时间。
 * @param now_ms 注入的当前单调毫秒时间。
 */
void bms_task_wdt_step(uint32_t now_ms);

/*========== Externalized Static Function Prototypes (Unit Test) =============*/

#ifdef __cplusplus
}
#endif

#endif /* BMS_TASK_H_ */
