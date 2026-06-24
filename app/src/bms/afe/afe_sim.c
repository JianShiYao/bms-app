/*
 * AFE 仿真后端（afe_sim）—— 实现
 *
 * 详见 bms/afe_sim.h。本文件提供：
 *  - bms_afe_sim_step()        纯函数仿真核心（确定可测；积分用干净电流，噪声只叠加在输出上）
 *  - bms_afe_sim_state_reset() 状态复位到确定起点
 *  - bms_afe_backend_read()    afe 后端接口的仿真实现（薄包装：取 k_uptime + static 状态）
 */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>

#include "bms/afe.h"
#include "bms/afe_sim.h"
#include "bms/types.h"

/* 正常构建由 app/Kconfig 提供；单测等无 app Kconfig 的场景回退默认值（与 soc 一致）。 */
#ifndef CONFIG_BMS_SOC_PACK_CAPACITY_MAH
#define CONFIG_BMS_SOC_PACK_CAPACITY_MAH 100000
#endif

/* ---- 仿真模型常量 ---- */
#define SIM_OCV_EMPTY_MV  3000        /* SOC=0% 时的开路电压 */
#define SIM_OCV_FULL_MV   4200        /* SOC=100% 时的开路电压 */
#define SIM_CELL_R_MOHM   2           /* 单体等效内阻 (mΩ)，用于充放电压降 */
#define SIM_CHARGE_MA     2000        /* 充放电电流幅值 */
#define SIM_CYCLE_MS      60000       /* 一个充→放循环周期 (ms) */
#define SIM_BASE_TEMP_DCI 250         /* 基础温度 25.0℃ (单位 0.1℃) */
#define SIM_SEED          0x12345678U /* 复位用固定噪声种子（确定性） */

/* ΔSOC(‰) 换算分母：容量(mAh) × 3600 = mA·ms/‰ */
#define SIM_SOC_DEN ((int64_t)CONFIG_BMS_SOC_PACK_CAPACITY_MAH * 3600)

/* 极简 LCG，推进种子并返回 [-range, +range] 的小噪声（噪声叠加在状态内的种子上，
 * 故同一初始种子 + 同一调用序列 → 逐位可复现）。 */
static int32_t sim_noise(uint32_t *lcg, int32_t range)
{
	*lcg = *lcg * 1103515245U + 12345U;
	return (int32_t)((*lcg >> 16) % (uint32_t)(2 * range + 1)) - range;
}

/* 由 now_ms 生成三角波电流：前半周期充电(+)，后半周期放电(-)。 */
static int32_t sim_current_ma(uint32_t now_ms)
{
	uint32_t phase = now_ms % SIM_CYCLE_MS;

	return (phase < SIM_CYCLE_MS / 2) ? SIM_CHARGE_MA : -SIM_CHARGE_MA;
}

void bms_afe_sim_state_reset(struct bms_afe_sim_state *st)
{
	if (st == NULL) {
		return;
	}
	st->soc_permille = 500; /* 起始 50% */
	st->last_ms = 0;        /* 下次为首帧 */
	st->lcg = SIM_SEED;     /* 固定种子 */
}

int bms_afe_sim_step(struct bms_afe_sim_state *st, uint32_t now_ms, struct bms_cell_meas *out)
{
	if (st == NULL || out == NULL) {
		return -EINVAL;
	}

	/* 首帧（last_ms==0）Δt 记 0，不积分，避免上电首帧 SOC 跳变 */
	uint32_t dt_ms = (st->last_ms == 0) ? 0U : (now_ms - st->last_ms);

	st->last_ms = now_ms;
	out->timestamp_ms = now_ms;

	/* 1) 干净电流用于积分；输出再叠加小噪声（保持 SOC 演化确定可测） */
	int32_t current_ma = sim_current_ma(now_ms);

	out->pack_current_ma = current_ma + sim_noise(&st->lcg, 10);

	/* 2) 库仑积分推 SOC，夹紧 [0,1000] */
	int64_t dq = (int64_t)current_ma * (int64_t)dt_ms; /* mA·ms */
	int32_t dsoc = (int32_t)(dq / SIM_SOC_DEN);

	st->soc_permille += dsoc;
	if (st->soc_permille > 1000) {
		st->soc_permille = 1000;
	}
	if (st->soc_permille < 0) {
		st->soc_permille = 0;
	}

	/* 3) SOC → OCV 线性映射，叠加内阻压降 I·R */
	int32_t ocv_mv =
		SIM_OCV_EMPTY_MV + (SIM_OCV_FULL_MV - SIM_OCV_EMPTY_MV) * st->soc_permille / 1000;
	int32_t ir_drop_mv = current_ma * SIM_CELL_R_MOHM / 1000; /* mA·mΩ→mV */

	/* 4) 逐串填充：基准 + 每串固定不均衡偏移 + 小噪声 */
	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		int32_t imbalance = (i % 5) * 3; /* 0~12mV 的串间差异 */

		out->cell_mv[i] = ocv_mv + ir_drop_mv + imbalance + sim_noise(&st->lcg, 2);
	}

	/* 5) 温度：基础 + 随 |电流| 升温（每 1A 约 +1.0℃）+ 小噪声 */
	int32_t abs_ma = current_ma < 0 ? -current_ma : current_ma;
	int32_t temp_rise_dci = abs_ma / 100; /* 2000mA → +20 → +2.0℃ */

	for (int i = 0; i < BMS_TEMP_SENSOR_COUNT; i++) {
		out->temp_dci[i] = SIM_BASE_TEMP_DCI + temp_rise_dci + sim_noise(&st->lcg, 3);
	}

	return 0;
}

int bms_afe_backend_read(struct bms_cell_meas *out)
{
	static struct bms_afe_sim_state s_state;
	static bool s_inited;

	if (out == NULL) {
		return -EINVAL;
	}
	if (!s_inited) {
		bms_afe_sim_state_reset(&s_state);
		s_inited = true;
	}
	return bms_afe_sim_step(&s_state, k_uptime_get_32(), out);
}
