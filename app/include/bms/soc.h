/*
 * SOC/SOH 估算模块接口
 */
#ifndef BMS_SOC_H_
#define BMS_SOC_H_

#include <stdint.h>
#include <stdbool.h>

#include "bms/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SOC 库仑积分跨帧状态（模块私有；不进 types.h，非 zbus 载荷）。
 *
 * 设计来源：03-design.md §1.1（ADR-SOC-C02 跨帧状态模块私有 / ADR-SOC-C06 int64 承载）。
 *
 * 不变式（invariant，任意时刻成立）：
 *  - initialized==false 时，acc_charge_ma_ms 与 last_ts_ms 内容无意义（未使用）。
 *  - initialized==true  时，soc_permille ∈ [0,1000]，last_ts_ms 为最近一次有效更新所用帧的时间戳。
 *  - acc_charge_ma_ms 是「相对初值起点」的累计净转移电荷（充电为正），量纲 mA·ms。
 *
 * 字段顺序：8 字节对齐量在前，避免填充浪费。
 * soc.c 中以 static 实例化（BSS 零初始化 → initialized=false，天然首帧安全态）。
 */
struct bms_soc_coulomb_state {
	int64_t acc_charge_ma_ms; /**< 累计净转移电荷，量纲 mA·ms，充电为正。int64
				     防溢出（REQ-SOC-C10）。 */
	uint32_t last_ts_ms;      /**< 上一帧已积分的 timestamp_ms（k_uptime, ms）。 */
	uint16_t soc_permille;    /**< 当前 SOC 估值 ‰，恒夹紧于 [0,1000]（REQ-SOC-C03）。 */
	bool initialized;         /**< 是否已完成上电一次性初始化（电压映射，REQ-SOC-C04）。 */
};

/**
 * @brief 初始化 SOC 模块。
 * 调度与 database 写入由 bms_task 统一负责。
 * @return 0 成功，负值为 errno。
 */
int bms_soc_init(void);

/**
 * @brief 由一帧测量的平均电压线性映射出 SOC 初值（电压映射初始化器）。
 *
 * 设计来源：03-design.md §2.1（ADR-SOC-C04）。语义收敛为「上电一次性初值来源」，
 * 行为与原桩实现逐位一致（3000mV→0‰、4200mV→1000‰，端点外夹紧）。无跨帧状态、无副作用。
 *
 * @param meas 输入测量（非空）
 * @param out  输出 SOC/SOH（非空）
 * @return 0 成功；-EINVAL（meas/out 任一为 NULL）。
 */
int bms_soc_estimate(const struct bms_cell_meas *meas, struct bms_soc *out);

/**
 * @brief 库仑积分步进核心（有状态纯函数，供线程与单测复用）。
 *
 * 设计来源：03-design.md §2.2、§3、§7（REQ-SOC-C01/C02/C03/C06/C07/C10/C11）。
 * 有状态库仑积分一步：取 Δt、对 pack_current_ma 积分、更新 state 与 out->soc_permille（夹紧）；
 * 首帧执行电压映射初始化（不积分）；异常输入安全降级。输出进入即先置安全态。
 *
 * @param state 跨帧积分状态（非空，调用方持有，单测可栈上构造）。本函数为其唯一读写入口。
 * @param meas  本帧测量（非空）。读取 pack_current_ma、timestamp_ms、cell_mv[]（仅初始化时）。
 * @param out   发布载荷（非空）。
 * @return 0   已产生有效输出（含初始化帧、含降级后仍更新的帧），调用方应发布；
 *         -EINVAL 任一指针 NULL（不触 state、不写 out）；
 *         -EAGAIN 坏数据被跳过，本帧不应发布（§3 分支 F）。
 */
int bms_soc_coulomb_step(struct bms_soc_coulomb_state *state, const struct bms_cell_meas *meas,
			 struct bms_soc *out);

/**
 * @brief 库仑积分状态复位（便于单测/上电）。
 *
 * 设计来源：03-design.md §2.3。把 state 置为「未初始化安全态」：全部字段归零、initialized=false。
 *
 * @param state 为 NULL 时安全返回（无操作）。
 */
void bms_soc_coulomb_state_reset(struct bms_soc_coulomb_state *state);

#ifdef __cplusplus
}
#endif

#endif /* BMS_SOC_H_ */
