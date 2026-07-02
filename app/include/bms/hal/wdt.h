/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    wdt.h
 * @brief   硬件看门狗（watchdog）薄封装接口。
 * @ingroup WDT
 *
 * @details 落实运行时模型契约（docs/concept/runtime-model.md §7）：
 *  仅当所有安全关键任务健康时才喂硬 watchdog；任一失联/超时 → 停喂 →
 *  watchdog 复位（软先于硬）。本头声明与后端无关的封装接口：
 *   - @ref bms_wdt_init  初始化硬件看门狗（sim 板为 stub 后端，no-op）。
 *   - @ref bms_wdt_feed  喂狗一次（sim stub 后端记数供测试观测）。
 *   - @ref bms_wdt_stub_feed_count 仅 stub 后端提供，返回累计 feed 次数。
 *
 *  真实后端：sim 板（native_sim / mps2）用 wdt stub（无硬件狗）；bms_f405 用
 *  STM32 IWDG（coder 另做，不在本增量内）。门控策略由
 *  @ref bms_sys_mon_wdt_feed_allowed 提供，接线由 @ref bms_task_wdt_step 完成。
 *
 *  本增量只声明契约（additive）+ 最小存根；真实记数/硬件逻辑由 coder 补齐。
 */

#ifndef BMS_WDT_H_
#define BMS_WDT_H_

/*========== Includes ========================================================*/
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*========== Macros and Definitions ==========================================*/

/*========== Extern Constant and Variable Declarations =======================*/

/*========== Extern Function Prototypes ======================================*/
/**
 * @brief 初始化硬件看门狗。
 * @details sim 板为 stub 后端时为 no-op；bms_f405 配置并启动 STM32 IWDG。
 * @return 0 成功。
 */
int bms_wdt_init(void);

/**
 * @brief 喂狗一次（复位看门狗计时器）。
 * @details sim stub 后端累计 feed 次数供测试观测（无硬件狗）；真实后端刷新 IWDG。
 */
void bms_wdt_feed(void);

/**
 * @brief 【仅 stub 后端】返回累计 feed 次数，供测试观测喂狗行为。
 * @return 自进程启动以来 @ref bms_wdt_feed 被调用的累计次数。
 */
uint32_t bms_wdt_stub_feed_count(void);

/*========== Externalized Static Function Prototypes (Unit Test) =============*/

#ifdef __cplusplus
}
#endif

#endif /* BMS_WDT_H_ */
