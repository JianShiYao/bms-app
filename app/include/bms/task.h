/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    task.h
 * @brief   foxBMS 2 inspired 任务框架（映射到 Zephyr 线程）接口。
 * @ingroup SYS
 */

#ifndef BMS_TASK_H_
#define BMS_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化任务框架并启动 安全/应用/后台 三个静态线程。
 * @details 按 Kconfig 复位 SOC 与保护状态，写入初始 BMS 状态（失效安全：接触器 OPEN）。
 * @return 0 成功。
 */
int bms_task_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BMS_TASK_H_ */
