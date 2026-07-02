/*
 * AFE 仿真后端单元测试（ztest, native_sim / mps2-an386）
 *
 * 被测：仿真后端纯函数核心（设计：docs/concept/architecture.md「数据源后端可切换（afe）」）
 *  - bms_afe_sim_state_reset —— 复位到确定起点（T-RESET）
 *  - bms_afe_sim_step        —— 三角充放电 + 库仑积分 + OCV 映射 + 温升（T-STEP）
 *
 * 模型为有状态纯函数：时钟由入参注入、状态由调用方持有、积分用干净电流
 * （噪声只叠加在输出上），故 SOC 演化对给定时间序列完全确定可测。
 *
 * 测试桩与 afe_sim.c 回退默认一致：
 *   CONFIG_BMS_SOC_PACK_CAPACITY_MAH = 100000 (mAh)
 * 换算：DEN = 容量(mAh) × 3600 = 360,000,000 mA·ms/‰；充放电幅值 = 2000 mA。
 */

/*========== Includes ========================================================*/
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/ztest.h>

#include "bms/hal/afe.h"
#include "bms/hal/afe_sim.h"
#include "bms/types.h"

/*========== Macros and Definitions ==========================================*/
/* 与 afe_sim.c 内部常量/回退默认一致的本地镜像，用于解析期望值断言 */
#define TEST_CAP_MAH      100000
#define TEST_DEN          ((int64_t)TEST_CAP_MAH * 3600) /* 360,000,000 mA·ms/‰ */
#define TEST_CHARGE_MA    2000
#define TEST_CYCLE_MS     60000
#define TEST_OCV_EMPTY_MV 3000
#define TEST_OCV_FULL_MV  4200
#define TEST_BASE_TEMP    250 /* 25.0℃（单位 0.1℃） */

/* 充电相位内的某时刻（now % CYCLE < CYCLE/2）。基准 +1 避开首帧 last_ms==0。 */
#define CHARGE_PHASE_MS(dt)    (1u + (dt))
/* 放电相位内的某时刻（now % CYCLE >= CYCLE/2）。 */
#define DISCHARGE_PHASE_MS(dt) (1u + (TEST_CYCLE_MS / 2) + (dt))

/*========== Static Constant and Variable Definitions ========================*/

/*========== Extern Constant and Variable Definitions ========================*/

/*========== Static Function Prototypes ======================================*/
static int32_t avg_cell_mv(const struct bms_cell_meas *m);

/*========== Static Function Implementations =================================*/
static int32_t avg_cell_mv(const struct bms_cell_meas *m)
{
	int64_t sum = 0;

	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		sum += m->cell_mv[i];
	}
	return (int32_t)(sum / BMS_CELL_COUNT);
}

ZTEST_SUITE(bms_afe_sim, NULL, NULL, NULL, NULL, NULL);

/* ============================================================
 * T-GUARD：参数校验（防御式，安全相关）
 * ============================================================ */

ZTEST(bms_afe_sim, test_null_args_return_einval)
{
	struct bms_afe_sim_state st;
	struct bms_cell_meas out;

	bms_afe_sim_state_reset(&st);
	zassert_equal(bms_afe_sim_step(NULL, 1000, &out), -EINVAL, "st=NULL 应 -EINVAL");
	zassert_equal(bms_afe_sim_step(&st, 1000, NULL), -EINVAL, "out=NULL 应 -EINVAL");
}

/* ============================================================
 * T-RESET：复位到确定起点
 * ============================================================ */

ZTEST(bms_afe_sim, test_reset_sets_known_start)
{
	struct bms_afe_sim_state st = {.soc_permille = 123, .last_ms = 456, .lcg = 0};

	bms_afe_sim_state_reset(&st);
	zassert_equal(st.soc_permille, 500, "复位后 SOC 应为 500‰");
	zassert_equal(st.last_ms, 0, "复位后 last_ms 应为 0（下次为首帧）");
	zassert_not_equal(st.lcg, 0, "复位后噪声种子应为非零固定值");
}

ZTEST(bms_afe_sim, test_reset_null_is_safe)
{
	bms_afe_sim_state_reset(NULL); /* 不应崩溃 */
}

/* ============================================================
 * T-STEP：步进核心
 * ============================================================ */

ZTEST(bms_afe_sim, test_first_frame_no_integration)
{
	struct bms_afe_sim_state st;
	struct bms_cell_meas out = {0};

	bms_afe_sim_state_reset(&st); /* last_ms=0 → 首帧 */
	zassert_ok(bms_afe_sim_step(&st, 1000, &out), "首帧应成功");
	zassert_equal(st.soc_permille, 500, "首帧 Δt=0 不积分，SOC 不变");
	zassert_equal(out.timestamp_ms, 1000, "timestamp 应透传 now_ms");
}

ZTEST(bms_afe_sim, test_charging_raises_soc)
{
	struct bms_afe_sim_state st;
	struct bms_cell_meas out = {0};

	bms_afe_sim_state_reset(&st);
	bms_afe_sim_step(&st, 1, &out); /* 首帧建立 last_ms */
	int32_t soc0 = st.soc_permille;

	/* dt = 1,800,000 ms 全程充电：ΔSOC = 2000×1.8e6/3.6e8 = +10‰ */
	zassert_ok(bms_afe_sim_step(&st, CHARGE_PHASE_MS(1800000), &out), NULL);
	zassert_true(st.soc_permille > soc0, "充电应使 SOC 上升");
	zassert_equal(st.soc_permille, soc0 + 10, "ΔSOC 应为 +10‰");
}

ZTEST(bms_afe_sim, test_discharging_lowers_soc)
{
	struct bms_afe_sim_state st;
	struct bms_cell_meas out = {0};

	bms_afe_sim_state_reset(&st);
	bms_afe_sim_step(&st, 1, &out);
	int32_t soc0 = st.soc_permille;

	/* dt = 1,800,000 ms 全程放电：ΔSOC ≈ -10‰ */
	zassert_ok(bms_afe_sim_step(&st, DISCHARGE_PHASE_MS(1800000), &out), NULL);
	zassert_true(st.soc_permille < soc0, "放电应使 SOC 下降");
}

ZTEST(bms_afe_sim, test_soc_clamped_to_full)
{
	struct bms_afe_sim_state st;
	struct bms_cell_meas out = {0};

	bms_afe_sim_state_reset(&st);
	bms_afe_sim_step(&st, 1, &out);
	/* 注入巨量充电时长，ΔSOC 远超满量程 → 夹紧 1000 */
	zassert_ok(bms_afe_sim_step(&st, CHARGE_PHASE_MS(300000000u), &out), NULL);
	zassert_equal(st.soc_permille, 1000, "过充应夹紧至 1000‰");
}

ZTEST(bms_afe_sim, test_soc_clamped_to_empty)
{
	struct bms_afe_sim_state st;
	struct bms_cell_meas out = {0};

	bms_afe_sim_state_reset(&st);
	bms_afe_sim_step(&st, 1, &out);
	zassert_ok(bms_afe_sim_step(&st, DISCHARGE_PHASE_MS(300000000u), &out), NULL);
	zassert_equal(st.soc_permille, 0, "过放应夹紧至 0‰");
}

ZTEST(bms_afe_sim, test_current_positive_in_charge_phase)
{
	struct bms_afe_sim_state st;
	struct bms_cell_meas out = {0};

	bms_afe_sim_state_reset(&st);
	zassert_ok(bms_afe_sim_step(&st, CHARGE_PHASE_MS(0), &out), NULL);
	zassert_true(out.pack_current_ma > 0, "充电相位电流应为正，实测 %d", out.pack_current_ma);
}

ZTEST(bms_afe_sim, test_current_negative_in_discharge_phase)
{
	struct bms_afe_sim_state st;
	struct bms_cell_meas out = {0};

	bms_afe_sim_state_reset(&st);
	zassert_ok(bms_afe_sim_step(&st, DISCHARGE_PHASE_MS(0), &out), NULL);
	zassert_true(out.pack_current_ma < 0, "放电相位电流应为负，实测 %d", out.pack_current_ma);
}

ZTEST(bms_afe_sim, test_voltage_tracks_soc_within_ocv_band)
{
	struct bms_afe_sim_state lo, hi;
	struct bms_cell_meas out_lo = {0}, out_hi = {0};

	/* 两个状态复位后种子一致 → 噪声序列相同，可直接比较电压高低 */
	bms_afe_sim_state_reset(&lo);
	bms_afe_sim_state_reset(&hi);
	lo.soc_permille = 0;    /* 空 */
	hi.soc_permille = 1000; /* 满 */

	/* last_ms 仍为 0 → 首帧不积分，SOC 维持注入值 */
	zassert_ok(bms_afe_sim_step(&lo, 0, &out_lo), NULL);
	zassert_ok(bms_afe_sim_step(&hi, 0, &out_hi), NULL);

	int32_t v_lo = avg_cell_mv(&out_lo);
	int32_t v_hi = avg_cell_mv(&out_hi);

	zassert_true(v_hi > v_lo, "高 SOC 电压应高于低 SOC（%d vs %d）", v_hi, v_lo);
	/* 落在 OCV 带内（允许 IR 压降/串间偏移/噪声的小裕量） */
	zassert_within(v_lo, TEST_OCV_EMPTY_MV, 50, "空电压应近 3000mV，实测 %d", v_lo);
	zassert_within(v_hi, TEST_OCV_FULL_MV, 50, "满电压应近 4200mV，实测 %d", v_hi);
}

ZTEST(bms_afe_sim, test_temp_rises_with_current)
{
	struct bms_afe_sim_state st;
	struct bms_cell_meas out = {0};

	bms_afe_sim_state_reset(&st);
	zassert_ok(bms_afe_sim_step(&st, CHARGE_PHASE_MS(0), &out), NULL);
	for (int i = 0; i < BMS_TEMP_SENSOR_COUNT; i++) {
		zassert_true(out.temp_dci[i] > TEST_BASE_TEMP,
			     "带载温度应高于基础 25.0℃，温感[%d]=%d", i, out.temp_dci[i]);
	}
}

ZTEST(bms_afe_sim, test_deterministic_for_same_inputs)
{
	struct bms_afe_sim_state a, b;
	struct bms_cell_meas oa = {0}, ob = {0};

	bms_afe_sim_state_reset(&a);
	bms_afe_sim_state_reset(&b);

	const uint32_t seq[] = {1, 7000, 33000, 95000, 1800001};

	for (size_t i = 0; i < ARRAY_SIZE(seq); i++) {
		zassert_ok(bms_afe_sim_step(&a, seq[i], &oa), NULL);
		zassert_ok(bms_afe_sim_step(&b, seq[i], &ob), NULL);
	}
	zassert_equal(memcmp(&oa, &ob, sizeof(oa)), 0, "相同输入序列应产生逐位相同输出");
	zassert_equal(a.soc_permille, b.soc_permille, "SOC 演化应确定");
}

/*
 * 注：合理性校验（bms_meas_validate）用例已随 meas 模块迁至
 * tests/bms/measurement-control/meas（Phase 1-①b）。本套件只覆盖 afe_sim 后端。
 */

/*========== Extern Function Implementations =================================*/

/*========== Externalized Static Function Implementations (Unit Test) ========*/
