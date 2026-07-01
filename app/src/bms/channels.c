/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    channels.c
 * @brief   BMS zbus channel 定义。
 * @ingroup SYS
 *
 * @details 观察者（订阅者/监听者）由各模块用 ZBUS_CHAN_ADD_OBS 自行注册，
 *          因此这里以 ZBUS_OBSERVERS_EMPTY 定义，保持 channel 与模块解耦。
 */

#include "bms/channels.h"

ZBUS_CHAN_DEFINE(chan_cell_meas,       /* name */
		 struct bms_cell_meas, /* type */
		 NULL,                 /* validator */
		 NULL,                 /* user data */
		 ZBUS_OBSERVERS_EMPTY, /* observers（由模块动态添加） */
		 ZBUS_MSG_INIT(0)      /* 初始值 */
);

ZBUS_CHAN_DEFINE(chan_soc, struct bms_soc, NULL, NULL, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));

ZBUS_CHAN_DEFINE(chan_prot_state, struct bms_prot_evt, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(0));
