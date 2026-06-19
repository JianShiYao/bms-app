/*
 * SOC/SOH 估算模块 —— 库仑计数（安时积分）实现
 *
 * 职责：订阅 chan_cell_meas，估算荷电/健康状态，发布到 chan_soc。
 * SOC 用库仑积分（对 pack_current_ma 按帧间 Δt 积分）；首帧用电压线性映射初始化。
 * 设计来源：docs/features/soc-coulomb/03-design.md。
 */
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bms/soc.h"
#include "bms/channels.h"

LOG_MODULE_REGISTER(bms_soc, LOG_LEVEL_INF);

#define SOC_THREAD_STACK 1024
#define SOC_THREAD_PRIO  7

/* 线性映射端点：3.0V→0%，4.2V→100%（电压映射初值器，REQ-SOC-C04） */
#define SOC_EMPTY_MV 3000
#define SOC_FULL_MV  4200

/*
 * 单测构建回退默认（设计 §5.1，编码必做）：
 * tests/bms/soc/CMakeLists.txt 直接把 soc.c 编入 host 测试，而其 prj.conf 不提供
 * CONFIG_BMS_SOC_*，故须按 types.h 对 CONFIG_BMS_CELL_COUNT 的范式补 #ifndef 回退，
 * 否则纯函数在测试构建下无法取值/编译。正常 app 构建由 app/Kconfig 提供这些符号。
 */
#ifndef CONFIG_BMS_SOC_PACK_CAPACITY_MAH
#define CONFIG_BMS_SOC_PACK_CAPACITY_MAH 100000
#endif
#ifndef CONFIG_BMS_SOC_GAP_FACTOR_N
#define CONFIG_BMS_SOC_GAP_FACTOR_N 10
#endif
#ifndef CONFIG_BMS_SOC_MAX_CURRENT_MA
#define CONFIG_BMS_SOC_MAX_CURRENT_MA 200000
#endif
#ifndef CONFIG_BMS_AFE_SAMPLE_PERIOD_MS /* 测试 prj.conf 也未开 AFE */
#define CONFIG_BMS_AFE_SAMPLE_PERIOD_MS 100
#endif
/*
 * CONFIG_BMS_SOC_INIT_FROM_VOLTAGE 为 bool（默认 y）；本设计取「首帧初始化恒走电压映射」，
 * 该 Kconfig 仅作显式关闭开关，故算法不直接依赖该宏（设计 §5.1）。
 */

/* 该模块作为订阅者，自行向 chan_cell_meas 注册观察 */
ZBUS_SUBSCRIBER_DEFINE(soc_sub, 4);
ZBUS_CHAN_ADD_OBS(chan_cell_meas, soc_sub, 3);

/*
 * 库仑积分跨帧状态实例（模块私有，设计 §1.1 / ADR-SOC-C02）。
 * BSS 零初始化 → initialized=false，天然首帧安全态。
 */
static struct bms_soc_coulomb_state soc_state;

int bms_soc_estimate(const struct bms_cell_meas *meas, struct bms_soc *out)
{
	if (meas == NULL || out == NULL) {
		return -EINVAL;
	}

	int64_t sum_mv = 0;

	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		sum_mv += meas->cell_mv[i];
	}
	int32_t avg_mv = (int32_t)(sum_mv / BMS_CELL_COUNT);

	/* 线性映射到 0..1000 ‰ 并夹紧 */
	int32_t permille = (avg_mv - SOC_EMPTY_MV) * 1000 / (SOC_FULL_MV - SOC_EMPTY_MV);
	if (permille < 0) {
		permille = 0;
	} else if (permille > 1000) {
		permille = 1000;
	}

	out->timestamp_ms = meas->timestamp_ms;
	out->soc_permille = (uint16_t)permille;
	out->soh_permille = 1000; /* TODO: 真实 SOH 估算 */

	return 0;
}

/*
 * 内部辅助（纯函数，设计 §2.4）：量程合理性检查。
 * |pack_current_ma| 是否在 [0, CONFIG_BMS_SOC_MAX_CURRENT_MA]。
 * 关联需求：REQ-SOC-C06（坏数据隔离）。
 */
static bool soc_current_in_range(int32_t pack_current_ma)
{
	int64_t mag = (pack_current_ma < 0) ? -(int64_t)pack_current_ma : (int64_t)pack_current_ma;
	return mag <= (int64_t)CONFIG_BMS_SOC_MAX_CURRENT_MA;
}

/*
 * 内部辅助（纯函数，设计 §2.4、§4.4）：由累计电荷 acc(mA·ms) 换算并夹紧为 ‰。
 * 含对称四舍五入（正负一致），避免小增量被整数除截断而单调累积偏移（REQ-SOC-C11）。
 * 关联需求：REQ-SOC-C03（夹紧 [0,1000]）、REQ-SOC-C11（精度/舍入）。
 */
static uint16_t soc_charge_to_permille(int64_t acc_charge_ma_ms)
{
	const int64_t DEN = (int64_t)CONFIG_BMS_SOC_PACK_CAPACITY_MAH * 3600; /* mA·ms / ‰ */
	int64_t acc = acc_charge_ma_ms;
	int64_t pm = (acc + (acc >= 0 ? DEN / 2 : -DEN / 2)) / DEN;

	if (pm < 0) {
		pm = 0;
	} else if (pm > 1000) {
		pm = 1000;
	}
	return (uint16_t)pm;
}

/*
 * 库仑积分状态复位（设计 §2.3，REQ-SOC-C04 验收 2）。
 */
void bms_soc_coulomb_state_reset(struct bms_soc_coulomb_state *state)
{
	if (state == NULL) {
		return; /* NULL 安全返回（设计 §2.3 契约） */
	}
	state->acc_charge_ma_ms = 0;
	state->last_ts_ms = 0;
	state->soc_permille = 0;
	state->initialized = false;
}

/*
 * 库仑积分步进核心（设计 §2.2、§3、§7；伪代码采用方案 A：acc 为单一真值源）。
 * 分支 A 空指针 / B 首帧初始化 / C 时间戳非单调回退 / D 正常 / E 丢帧夹紧 / F 电流超量程。
 * 关联需求：REQ-SOC-C01/C02/C03/C06/C07/C10/C11。
 */
int bms_soc_coulomb_step(struct bms_soc_coulomb_state *state, const struct bms_cell_meas *meas,
			 struct bms_soc *out)
{
	/* 分支 A：空指针 —— 不触 state、不写 out（REQ-SOC-C06 验收 1） */
	if (state == NULL || meas == NULL || out == NULL) {
		return -EINVAL;
	}

	const int64_t DEN = (int64_t)CONFIG_BMS_SOC_PACK_CAPACITY_MAH * 3600; /* mA·ms / ‰ */

	/* 输出先置安全态：用上一稳定 SOC（未初始化则 0），ts/soh 先填（REQ-SOC-C05） */
	out->timestamp_ms = meas->timestamp_ms;
	out->soh_permille = 1000;
	out->soc_permille = state->initialized ? state->soc_permille : 0;

	/* 分支 B：首帧初始化（电压映射 → 等效起始电荷），不积分（设计 §3-B，REQ-SOC-C04） */
	if (!state->initialized) {
		struct bms_soc init;

		(void)bms_soc_estimate(meas, &init); /* meas 已非空，返回 0 */
		/* 由初值 ‰ 反算等效起始电荷 → 此后 ‰ 恒由 acc 推出（单一真值源，设计 §4.4） */
		state->acc_charge_ma_ms = (int64_t)init.soc_permille * DEN;
		state->soc_permille = init.soc_permille;
		state->last_ts_ms = meas->timestamp_ms;
		state->initialized = true;
		out->soc_permille = init.soc_permille;
		return 0; /* 发布初始化帧（REQ-SOC-C05 含初始化帧） */
	}

	/* 分支 F：电流超量程 —— 跳过积分，acc 不污染，ts 推进，不发布（设计 §3-F，REQ-SOC-C06） */
	if (!soc_current_in_range(meas->pack_current_ma)) {
		state->last_ts_ms = meas->timestamp_ms; /* 防下帧误判丢帧（C06 验收 3） */
		return -EAGAIN; /* out 保持上一稳定值，不发布（C05 验收 2） */
	}

	/* Δt 解析：分支 C(非单调回退) / D(正常) / E(丢帧夹紧)（设计 §3、§7，REQ-SOC-C02） */
	uint32_t dt_ms;
	const uint32_t period = CONFIG_BMS_AFE_SAMPLE_PERIOD_MS;
	const uint32_t dt_cap = (uint32_t)CONFIG_BMS_SOC_GAP_FACTOR_N * period;

	if (meas->timestamp_ms <= state->last_ts_ms) {
		/* 分支 C：时间戳非单调/回绕 → 回退缺省周期，仍正向积分（不反向跳变） */
		dt_ms = period;
	} else {
		uint32_t diff = meas->timestamp_ms - state->last_ts_ms;

		/* 分支 E：丢帧夹紧到上限 / 分支 D：正常取真实差值 */
		dt_ms = (diff > dt_cap) ? dt_cap : diff;
	}

	/* 积分（先提升 int64，防中间溢出，设计 §4.2）；方向随电流符号（REQ-SOC-C07） */
	int64_t dQ = (int64_t)meas->pack_current_ma * (int64_t)dt_ms; /* mA·ms */
	state->acc_charge_ma_ms += dQ;

	/* 换算 + 对称舍入 + 夹紧（设计 §4.4，REQ-SOC-C03/C11） */
	uint16_t pm = soc_charge_to_permille(state->acc_charge_ma_ms);

	state->soc_permille = pm;
	state->last_ts_ms = meas->timestamp_ms;
	out->soc_permille = pm;
	return 0;
}

static void soc_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;
	struct bms_cell_meas meas;
	struct bms_soc soc;

	/* 跨帧积分状态：复位为未初始化安全态（亦可依赖 BSS 零初始化，设计 §6） */
	bms_soc_coulomb_state_reset(&soc_state);

	while (zbus_sub_wait(&soc_sub, &chan, K_FOREVER) == 0) {
		if (chan != &chan_cell_meas) {
			continue;
		}
		if (zbus_chan_read(&chan_cell_meas, &meas, K_MSEC(50)) != 0) {
			continue;
		}
		/* 库仑积分步进；据返回码决定是否发布（设计 §6，REQ-SOC-C05） */
		int rc = bms_soc_coulomb_step(&soc_state, &meas, &soc);

		if (rc == 0) {
			/* 发布超时即丢弃，不重试（REQ-SOC-C09 不阻塞安全链） */
			zbus_chan_pub(&chan_soc, &soc, K_MSEC(50));
			LOG_DBG("SOC=%u.%u%%", soc.soc_permille / 10, soc.soc_permille % 10);
		}
		/* rc == -EAGAIN（跳过帧）/ -EINVAL：不发布，继续下一帧 */
	}
}

K_THREAD_DEFINE(bms_soc_tid, SOC_THREAD_STACK, soc_thread, NULL, NULL, NULL, SOC_THREAD_PRIO, 0, 0);

int bms_soc_init(void)
{
	LOG_INF("SOC init: coulomb-counting, cap=%d mAh, init-map %d..%d mV",
		CONFIG_BMS_SOC_PACK_CAPACITY_MAH, SOC_EMPTY_MV, SOC_FULL_MV);
	return 0;
}
