/*
 * BMS zbus channel 声明
 *
 * channel 在 src/bms/channels.c 中用 ZBUS_CHAN_DEFINE 定义，
 * 各模块通过 ZBUS_CHAN_ADD_OBS 订阅、zbus_chan_pub 发布。
 */
#ifndef BMS_CHANNELS_H_
#define BMS_CHANNELS_H_

#include <zephyr/zbus/zbus.h>
#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

ZBUS_CHAN_DECLARE(chan_cell_meas);   /**< struct bms_cell_meas，afe 发布 */
ZBUS_CHAN_DECLARE(chan_soc);         /**< struct bms_soc，soc 发布 */
ZBUS_CHAN_DECLARE(chan_prot_state);  /**< struct bms_prot_evt，protection 发布 */

#ifdef __cplusplus
}
#endif

#endif /* BMS_CHANNELS_H_ */
