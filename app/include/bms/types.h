/*
 * BMS 公共数据类型
 */
#ifndef BMS_TYPES_H_
#define BMS_TYPES_H_

#include <stdint.h>
#include <zephyr/autoconf.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 正常构建由 app/Kconfig 提供；单测等无 app Kconfig 的场景回退默认值。 */
#ifndef CONFIG_BMS_CELL_COUNT
#define CONFIG_BMS_CELL_COUNT 16
#endif
#ifndef CONFIG_BMS_TEMP_SENSOR_COUNT
#define CONFIG_BMS_TEMP_SENSOR_COUNT 4
#endif

#define BMS_CELL_COUNT        CONFIG_BMS_CELL_COUNT
#define BMS_TEMP_SENSOR_COUNT CONFIG_BMS_TEMP_SENSOR_COUNT

/** 一帧电芯测量数据（由 afe 模块发布到 chan_cell_meas） */
struct bms_cell_meas {
	uint32_t timestamp_ms;                   /**< 采样时刻（k_uptime） */
	int32_t cell_mv[BMS_CELL_COUNT];         /**< 每串电压，单位 mV */
	int32_t pack_current_ma;                 /**< 总电流，充电为正，单位 mA */
	int32_t temp_dci[BMS_TEMP_SENSOR_COUNT]; /**< 温度，单位 0.1℃ */
};

/** SOC/SOH 状态（由 soc 模块发布到 chan_soc） */
struct bms_soc {
	uint32_t timestamp_ms;
	uint16_t soc_permille; /**< 荷电状态 0..1000 (‰) */
	uint16_t soh_permille; /**< 健康状态 0..1000 (‰) */
};

/** 保护状态枚举 */
enum bms_prot_state {
	BMS_PROT_NORMAL = 0, /**< 正常，接触器可闭合 */
	BMS_PROT_OV,         /**< 过压 */
	BMS_PROT_UV,         /**< 欠压 */
	BMS_PROT_OC,         /**< 过流 */
	BMS_PROT_OT,         /**< 过温 */
	BMS_PROT_FAULT,      /**< 严重故障，强制安全态 */
};

/** 接触器/MOS 期望状态（失效安全：默认 OPEN） */
enum bms_contactor {
	BMS_CONTACTOR_OPEN = 0,
	BMS_CONTACTOR_CLOSED,
};

/** 保护事件（由 protection 模块发布到 chan_prot_state） */
struct bms_prot_evt {
	uint32_t timestamp_ms;
	enum bms_prot_state state;
	enum bms_contactor contactor; /**< 期望的接触器状态 */
	uint8_t cell_index;           /**< 触发保护的单体索引（如适用） */
};

#ifdef __cplusplus
}
#endif

#endif /* BMS_TYPES_H_ */
