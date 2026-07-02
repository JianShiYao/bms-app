/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    types.h
 * @brief   BMS 公共数据类型。
 * @ingroup SYS
 */

#ifndef BMS_TYPES_H_
#define BMS_TYPES_H_

#include <stdbool.h>
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

/**
 * bms_cell_meas.validity 测量有效位（测量数据纪律：值 + 时间戳 + 有效性）。
 *
 * 设计来源：docs/concept/architecture.md「测量数据纪律」。某位为 1 表示对应量本帧通过
 * 合理性校验、下游方可采用；为 0 表示无效，下游须失效安全处理（如电流无效则
 * SOC 不积分、保护不据此闭合接触器）。位由 meas 边缘的纯函数 bms_meas_validate() 置位。
 */
#define BMS_MEAS_VALID_VOLTAGE (1U << 0) /**< 电压量合理 */
#define BMS_MEAS_VALID_CURRENT (1U << 1) /**< 电流量合理 */
#define BMS_MEAS_VALID_TEMP    (1U << 2) /**< 温度量合理 */
#define BMS_MEAS_VALID_ALL (BMS_MEAS_VALID_VOLTAGE | BMS_MEAS_VALID_CURRENT | BMS_MEAS_VALID_TEMP)

/** 一帧电芯测量数据（由 afe 模块发布到 chan_cell_meas） */
struct bms_cell_meas {
	uint32_t timestamp_ms;                   /**< 采样时刻（k_uptime, ms），用于下游过期检测 */
	int32_t cell_mv[BMS_CELL_COUNT];         /**< 每串电压，单位 mV */
	int32_t pack_current_ma;                 /**< 总电流，充电为正，单位 mA */
	int32_t temp_dci[BMS_TEMP_SENSOR_COUNT]; /**< 温度，单位 0.1℃ */
	uint8_t validity; /**< 有效位掩码，BMS_MEAS_VALID_*；0=全无效（安全默认）*/
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

/** BMS 主状态机状态（foxBMS 2 inspired：集中管理接触器允许条件） */
enum bms_state {
	BMS_STATE_INIT = 0,
	BMS_STATE_STANDBY,
	BMS_STATE_PRECHARGE,
	BMS_STATE_NORMAL,
	BMS_STATE_FAULT,
	BMS_STATE_LOCKED,
};

/** BMS 主状态快照（由 bms_bms 写入 database） */
struct bms_state_snapshot {
	uint32_t timestamp_ms;
	enum bms_state state;
	enum bms_contactor contactor;
};

/** 接触器反馈快照（owner=bms_contactor，写入 DB_CONTACTOR_FB） */
struct bms_contactor_fb {
	uint32_t timestamp_ms;     /**< 本次反馈采集时刻 */
	enum bms_contactor actual; /**< 实测接触器状态（反馈） */
	bool precharge_active;     /**< 预充是否进行中（bms_f405 经 AFE，占位；fake 恒 false） */
};

/** 任务健康快照（owner=bms_sys_mon，写入 DB_TASK_HEALTH） */
struct bms_task_health {
	uint32_t timestamp_ms;           /**< 本轮 monitor 时刻 */
	uint32_t heartbeat_timeout_mask; /**< 心跳超时任务位掩码（bit = bms_sys_mon_task） */
	uint32_t runtime_overrun_mask;   /**< 运行超时任务位掩码 */
};

#ifdef __cplusplus
}
#endif

#endif /* BMS_TYPES_H_ */
