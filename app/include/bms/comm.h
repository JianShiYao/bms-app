/*
 * CAN 通信模块接口
 */
#ifndef BMS_COMM_H_
#define BMS_COMM_H_

#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化通信模块（启动线程，订阅各 channel，对外 CAN 上报）。
 * native_sim 下无真实 CAN，走日志桩；真实硬件接 can1。
 * @return 0 成功，负值为 errno。
 */
int bms_comm_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BMS_COMM_H_ */
