/*
 * AFE 仿真数据源（afe_sim） —— 临时演示版
 *
 * 用途：在 QEMU / native_sim（无真实 ADC/AFE 芯片）下为 afe 模块提供
 *       "会动"的测量数据，使 soc / protection / balancing 拿到真实变化的输入。
 *
 * 模型（全整数运算，单位与 struct bms_cell_meas 一致）：
 *   1) 电流：以 uptime 驱动一个三角形充放电循环（+充 / -放）。
 *   2) SOC ：库仑积分 ΔQ = I·Δt，累加到容量百分比。
 *   3) 电压：SOC 线性映射到 OCV（3000mV 空 ~ 4200mV 满），叠加内阻压降。
 *   4) 不均衡：每串加一个固定小偏移，模拟单体差异（给 balancing 用）。
 *   5) 温度：基础 25.0℃ + 随 |电流| 升温，叠加轻微噪声。
 *   6) 噪声：自带 LCG 伪随机（不依赖 CONFIG_ENTROPY），幅度很小。
 *
 * 这是 afe.h 中 bms_afe_sample() 的一个实现，可直接替换 afe.c 里的内联桩
 * （接法见文件末注释）。真机版本应另写 afe_adc.c，走 devicetree 的 ADC 设备。
 */
#include <errno.h>
#include <stdint.h>
#include <zephyr/kernel.h>

#include "bms/afe.h"
#include "bms/types.h"

/* ---- 可调仿真参数 ---- */
#define SIM_OCV_EMPTY_MV  3000  /* SOC=0% 时的开路电压 */
#define SIM_OCV_FULL_MV   4200  /* SOC=100% 时的开路电压 */
#define SIM_CELL_R_MOHM   2     /* 单体等效内阻 (mΩ)，用于充放电压降 */
#define SIM_CHARGE_MA     2000  /* 充电电流幅值（正） */
#define SIM_CYCLE_MS      60000 /* 一个充→放循环周期 (ms) */
#define SIM_BASE_TEMP_DCI 250   /* 基础温度 25.0℃ (单位 0.1℃) */

/* 仿真内部状态：SOC 用千分比保存以保留精度 */
static int32_t s_soc_permille = 500; /* 起始 50% */
static uint32_t s_last_ms;           /* 上次采样时刻，用于积分 Δt */
static uint32_t s_lcg = 0x12345678u; /* 伪随机种子 */

/* 极简 LCG，返回 [-range, +range] 的小噪声 */
static int32_t sim_noise(int32_t range)
{
	s_lcg = s_lcg * 1103515245u + 12345u;
	return (int32_t)((s_lcg >> 16) % (uint32_t)(2 * range + 1)) - range;
}

/* 由 uptime 生成三角波电流：前半周期充电(+)，后半周期放电(-) */
static int32_t sim_current_ma(uint32_t now_ms)
{
	uint32_t phase = now_ms % SIM_CYCLE_MS;

	if (phase < SIM_CYCLE_MS / 2) {
		return SIM_CHARGE_MA; /* 充电 */
	}
	return -SIM_CHARGE_MA; /* 放电 */
}

int bms_afe_sample(struct bms_cell_meas *out)
{
	if (out == NULL) {
		return -EINVAL;
	}

	uint32_t now = k_uptime_get_32();
	uint32_t dt_ms = (s_last_ms == 0) ? 0 : (now - s_last_ms);

	s_last_ms = now;
	out->timestamp_ms = now;

	/* 1) 当前电流 */
	int32_t current_ma = sim_current_ma(now);

	out->pack_current_ma = current_ma + sim_noise(10);

	/* 2) 库仑积分推 SOC：ΔSOC(‰) = I(mA)·Δt(ms) / (容量(mAh)·3600)。
	 *    用 CONFIG_BMS_SOC_PACK_CAPACITY_MAH 作为容量基准（无则回退）。 */
#ifndef CONFIG_BMS_SOC_PACK_CAPACITY_MAH
#define CONFIG_BMS_SOC_PACK_CAPACITY_MAH 100000
#endif
	int64_t dq = (int64_t)current_ma * (int64_t)dt_ms; /* mA·ms */
	int32_t dsoc = (int32_t)(dq / ((int64_t)CONFIG_BMS_SOC_PACK_CAPACITY_MAH * 3600));

	s_soc_permille += dsoc;
	if (s_soc_permille > 1000) {
		s_soc_permille = 1000;
	}
	if (s_soc_permille < 0) {
		s_soc_permille = 0;
	}

	/* 3) SOC → OCV 线性映射，再叠加内阻压降 I·R */
	int32_t ocv_mv =
		SIM_OCV_EMPTY_MV + (SIM_OCV_FULL_MV - SIM_OCV_EMPTY_MV) * s_soc_permille / 1000;
	int32_t ir_drop_mv = current_ma * SIM_CELL_R_MOHM / 1000; /* mA·mΩ→mV */

	/* 4) 逐串填充：基准 + 每串固定不均衡偏移 + 小噪声 */
	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		int32_t imbalance = (i % 5) * 3; /* 0~12mV 的串间差异 */

		out->cell_mv[i] = ocv_mv + ir_drop_mv + imbalance + sim_noise(2);
	}

	/* 5) 温度：基础 + 随 |电流| 升温（每 1A 约 +1.0℃）+ 噪声 */
	int32_t abs_ma = current_ma < 0 ? -current_ma : current_ma;
	int32_t temp_rise_dci = abs_ma / 100; /* 2000mA → +20 → +2.0℃ */

	for (int i = 0; i < BMS_TEMP_SENSOR_COUNT; i++) {
		out->temp_dci[i] = SIM_BASE_TEMP_DCI + temp_rise_dci + sim_noise(3);
	}

	return 0;
}

/*
 * 如何启用（临时验证用，二选一）：
 *
 * 方案①（最快，先看效果）：把 afe.c 里现有的 bms_afe_sample() 整个函数删掉/注释，
 *   再在 CMakeLists.txt 的 afe 源里把本文件加进去：
 *     target_sources_ifdef(CONFIG_BMS_AFE app PRIVATE
 *         src/bms/afe/afe.c
 *         src/bms/afe/afe_sim.c)   # 注意：两边不能同时定义 bms_afe_sample
 *
 * 方案②（推荐的"方式B"骨架）：在 afe.h 增加后端接口 afe_backend_read()，
 *   afe.c 只调后端；用 Kconfig 选 afe_sim.c / afe_adc.c。这样真机/仿真共用业务逻辑。
 */
