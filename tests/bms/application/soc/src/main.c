/*
 * SOC 估算模块单元测试（ztest, native_sim / mps2-an386）
 *
 * 覆盖两组纯函数：
 *  - bms_soc_estimate            —— 电压映射初值器（既有 5 用例保留，T-EST）
 *  - bms_soc_coulomb_state_reset —— 状态复位（T-RESET）
 *  - bms_soc_coulomb_step        —— 库仑积分步进核心（T-STEP，新增）
 *
 * 每个用例注释回链需求ID（REQ-SOC-025..036）/设计项（03-design.md §x）。
 * 测试桩与 soc.c 的回退默认一致（设计 §5.1）：
 *   CONFIG_BMS_SOC_PACK_CAPACITY_MAH = 100000 (mAh)
 *   CONFIG_BMS_AFE_SAMPLE_PERIOD_MS  = 100   (ms)
 *   CONFIG_BMS_SOC_GAP_FACTOR_N      = 10
 *   CONFIG_BMS_SOC_MAX_CURRENT_MA    = 200000 (mA)
 * 换算：DEN = 容量(mAh) × 3600 = 360,000,000 mA·ms / ‰；dt_cap = N×period = 1000ms。
 */

/*========== Includes ========================================================*/
#include <errno.h>
#include <stdint.h>
#include <zephyr/ztest.h>

#include "bms/application/soc.h"

/*========== Macros and Definitions ==========================================*/
/* 与 soc.c 回退默认一致的本地常量（设计 §5.1），用于解析期望值断言 */
#define TEST_CAP_MAH     100000
#define TEST_PERIOD_MS   100
#define TEST_GAP_N       10
#define TEST_DT_CAP_MS   (TEST_GAP_N * TEST_PERIOD_MS) /* 1000 ms */
#define TEST_MAX_CURR_MA 200000
#define TEST_DEN         ((int64_t)TEST_CAP_MAH * 3600) /* 360,000,000 mA·ms/‰ */

/*========== Static Constant and Variable Definitions ========================*/

/*========== Extern Constant and Variable Definitions ========================*/

/*========== Static Function Prototypes ======================================*/
static void fill_cells(struct bms_cell_meas *m, int32_t mv);
static void make_meas(struct bms_cell_meas *m, int32_t mv, int32_t cur, uint32_t ts);

/*========== Static Function Implementations =================================*/
static void fill_cells(struct bms_cell_meas *m, int32_t mv)
{
	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		m->cell_mv[i] = mv;
	}
}

/* 构造一帧测量：恒定电压 mv、电流 cur、时间戳 ts */
static void make_meas(struct bms_cell_meas *m, int32_t mv, int32_t cur, uint32_t ts)
{
	*m = (struct bms_cell_meas){0};
	fill_cells(m, mv);
	m->pack_current_ma = cur;
	m->timestamp_ms = ts;
}

ZTEST_SUITE(bms_soc, NULL, NULL, NULL, NULL, NULL);

/* ============================================================
 * T-EST：bms_soc_estimate 电压映射初值器（既有 5 用例，全部保留）
 * 回链：REQ-SOC-028（初值=电压映射，0‰ 漂移）、REQ-SOC-027（夹紧）
 * ============================================================ */

ZTEST(bms_soc, test_full_charge)
{
	struct bms_cell_meas m = {0};
	struct bms_soc soc;

	fill_cells(&m, 4200);
	zassert_ok(bms_soc_estimate(&m, &soc));
	zassert_equal(soc.soc_permille, 1000, "4.2V should map to 100%%");
}

ZTEST(bms_soc, test_empty)
{
	struct bms_cell_meas m = {0};
	struct bms_soc soc;

	fill_cells(&m, 3000);
	zassert_ok(bms_soc_estimate(&m, &soc));
	zassert_equal(soc.soc_permille, 0, "3.0V should map to 0%%");
}

ZTEST(bms_soc, test_mid_in_range)
{
	struct bms_cell_meas m = {0};
	struct bms_soc soc;

	fill_cells(&m, 3600);
	zassert_ok(bms_soc_estimate(&m, &soc));
	zassert_true(soc.soc_permille > 0 && soc.soc_permille < 1000,
		     "mid voltage should be within (0,1000)");
}

ZTEST(bms_soc, test_clamp_over_full)
{
	struct bms_cell_meas m = {0};
	struct bms_soc soc;

	fill_cells(&m, 4500);
	zassert_ok(bms_soc_estimate(&m, &soc));
	zassert_equal(soc.soc_permille, 1000, "above-full must clamp to 100%%");
}

ZTEST(bms_soc, test_null_args)
{
	zassert_equal(bms_soc_estimate(NULL, NULL), -EINVAL);
}

/* ============================================================
 * T-RESET：bms_soc_coulomb_state_reset（设计 §2.3，REQ-SOC-028 验收 2）
 * ============================================================ */

/* 复位后字段归零、initialized==false（设计 §2.3 契约） */
ZTEST(bms_soc, test_reset_clears_state)
{
	struct bms_soc_coulomb_state st = {
		.acc_charge_ma_ms = 12345,
		.last_ts_ms = 678,
		.soc_permille = 555,
		.initialized = true,
	};

	bms_soc_coulomb_state_reset(&st);
	zassert_equal(st.acc_charge_ma_ms, 0, "acc must be cleared");
	zassert_equal(st.last_ts_ms, 0, "last_ts must be cleared");
	zassert_equal(st.soc_permille, 0, "soc must be cleared");
	zassert_false(st.initialized, "initialized must be false after reset");
}

/* NULL 安全返回，无操作、不崩溃（设计 §2.3 契约：state 为 NULL 时安全返回） */
ZTEST(bms_soc, test_reset_null_safe)
{
	bms_soc_coulomb_state_reset(NULL);
	/* 不崩溃即通过 */
}

/* ============================================================
 * T-STEP：bms_soc_coulomb_step 库仑积分步进核心
 * ============================================================ */

/* 分支 A：空指针 → -EINVAL，不触 state、不写 out（设计 §3-A，REQ-SOC-030 验收 1） */
ZTEST(bms_soc, test_step_null_returns_einval)
{
	struct bms_soc_coulomb_state st;
	struct bms_cell_meas m;
	struct bms_soc out;

	bms_soc_coulomb_state_reset(&st);
	make_meas(&m, 3600, 0, 0);

	zassert_equal(bms_soc_coulomb_step(NULL, &m, &out), -EINVAL, "state NULL");
	zassert_equal(bms_soc_coulomb_step(&st, NULL, &out), -EINVAL, "meas NULL");
	zassert_equal(bms_soc_coulomb_step(&st, &m, NULL), -EINVAL, "out NULL");
	/* state 未被污染：仍处于复位后的未初始化态 */
	zassert_false(st.initialized, "state must not be touched on -EINVAL");
}

/* 分支 B：首帧初始化 —— 初值=电压映射且不积分（设计 §3-B，REQ-SOC-028 验收 1，REQ-SOC-029
 * 含初始化帧） */
ZTEST(bms_soc, test_step_first_frame_init)
{
	struct bms_soc_coulomb_state st;
	struct bms_cell_meas m;
	struct bms_soc out;
	struct bms_soc map;

	bms_soc_coulomb_state_reset(&st);
	/* 3600mV → (3600-3000)*1000/1200 = 500‰ */
	make_meas(&m, 3600, 100000, 1000); /* 即使带大电流，首帧也不积分 */

	zassert_ok(bms_soc_estimate(&m, &map)); /* 解析期望初值 */
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));

	zassert_equal(out.soc_permille, map.soc_permille,
		      "first frame SOC must equal voltage-map value (0permille drift)");
	zassert_equal(out.timestamp_ms, m.timestamp_ms, "out ts == meas ts (REQ-SOC-029)");
	zassert_equal(out.soh_permille, 1000, "soh fixed 1000 (REQ-SOC-029)");
	zassert_true(st.initialized, "state initialized after first frame");
	zassert_equal(st.last_ts_ms, m.timestamp_ms, "last_ts set to first frame ts");
	zassert_equal(st.soc_permille, out.soc_permille, "state.soc == out.soc");
}

/* 初始化仅发生一次：其后帧由积分更新，不再被电压映射覆盖（REQ-SOC-028 验收 2） */
ZTEST(bms_soc, test_step_init_only_once)
{
	struct bms_soc_coulomb_state st;
	struct bms_cell_meas m;
	struct bms_soc out;

	bms_soc_coulomb_state_reset(&st);
	/* 首帧 3600mV → 500‰ 初值 */
	make_meas(&m, 3600, 0, 1000);
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));
	zassert_equal(out.soc_permille, 500, "init 500permille");

	/* 第二帧电压跳到 4200mV（映射为 1000‰），但电流为 0：SOC 不应被电压映射覆盖为 1000 */
	make_meas(&m, 4200, 0, 1100);
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));
	zassert_equal(out.soc_permille, 500,
		      "post-init frame must NOT be overwritten by voltage map");
}

/*
 * 分支 D：正常充电积分，精度 ≤ ±1‰（设计 §3-D/§4，REQ-SOC-025/031/035）。
 * 初值 500‰，恒流 +100000mA(100A，在量程内)，每帧 dt=1000ms(=dt_cap，仍属正常)。
 * 每帧 dQ = 100000 × 1000 = 1e8 mA·ms；ΔSOC/帧 = 1e8/3.6e8 ≈ 0.2778‰。
 * 跑 36 帧：累计 acc=3.6e9，ΔSOC = 3.6e9/3.6e8 = 10‰ → 期望 510‰（解析精确）。
 */
ZTEST(bms_soc, test_step_charge_integration_accuracy)
{
	struct bms_soc_coulomb_state st;
	struct bms_cell_meas m;
	struct bms_soc out;

	bms_soc_coulomb_state_reset(&st);
	make_meas(&m, 3600, 0, 0); /* 初值 500‰ */
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));
	zassert_equal(out.soc_permille, 500);

	uint16_t prev = out.soc_permille;
	uint32_t ts = 0;
	for (int i = 0; i < 36; i++) {
		ts += 1000;
		make_meas(&m, 3600, 100000, ts);
		zassert_ok(bms_soc_coulomb_step(&st, &m, &out));
		/* 方向正确性：充电时 SOC 单调不减（REQ-SOC-031 验收 1） */
		zassert_true(out.soc_permille >= prev, "charging must be monotonic non-decreasing");
		prev = out.soc_permille;
	}
	/* 解析期望 510‰，容差 ±1‰（REQ-SOC-025 验收 2、REQ-SOC-035） */
	zassert_within(out.soc_permille, 510, 1,
		       "30s @100A on 100Ah pack should add ~10permille (got %u)", out.soc_permille);
}

/* 分支 D：放电方向 —— 电流为负 SOC 单调不增（REQ-SOC-031 验收 2） */
ZTEST(bms_soc, test_step_discharge_direction)
{
	struct bms_soc_coulomb_state st;
	struct bms_cell_meas m;
	struct bms_soc out;

	bms_soc_coulomb_state_reset(&st);
	make_meas(&m, 3600, 0, 0); /* 初值 500‰ */
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));

	uint16_t prev = out.soc_permille;
	uint32_t ts = 0;
	for (int i = 0; i < 36; i++) {
		ts += 1000;
		make_meas(&m, 3600, -100000, ts); /* 放电 */
		zassert_ok(bms_soc_coulomb_step(&st, &m, &out));
		zassert_true(out.soc_permille <= prev,
			     "discharging must be monotonic non-increasing");
		prev = out.soc_permille;
	}
	zassert_within(out.soc_permille, 490, 1,
		       "30s @-100A on 100Ah pack should drop ~10permille (got %u)",
		       out.soc_permille);
}

/* 零电流帧 SOC 不变，时间戳照常推进（REQ-SOC-031 验收 3） */
ZTEST(bms_soc, test_step_zero_current_no_change)
{
	struct bms_soc_coulomb_state st;
	struct bms_cell_meas m;
	struct bms_soc out;

	bms_soc_coulomb_state_reset(&st);
	make_meas(&m, 3600, 0, 0);
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out)); /* 初值 500 */

	make_meas(&m, 3600, 0, 500); /* 正常 dt=500ms，零电流 */
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));
	zassert_equal(out.soc_permille, 500, "zero current keeps SOC unchanged");
	zassert_equal(st.last_ts_ms, 500, "timestamp still advances on zero current");
}

/*
 * 分支 C：时间戳非单调/回退 —— 回退缺省周期 period 作为 Δt，正向积分不反向跳变
 *（设计 §3-C，REQ-SOC-026 验收 2、REQ-SOC-030 验收 2）。
 */
ZTEST(bms_soc, test_step_nonmonotonic_ts_fallback)
{
	struct bms_soc_coulomb_state st;
	struct bms_cell_meas m;
	struct bms_soc out;

	bms_soc_coulomb_state_reset(&st);
	make_meas(&m, 3600, 0, 5000); /* 初值 500‰，last_ts=5000 */
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));

	int64_t acc_before = st.acc_charge_ma_ms;

	/* 第二帧时间戳回退（ts <= last_ts），充电电流：应回退 period=100ms，仍正向积分 */
	make_meas(&m, 3600, 100000, 4000); /* ts < last_ts */
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));

	/* 回退 Δt=period=100ms：dQ = 100000×100 = 1e7（正），SOC 不反向下降 */
	zassert_true(out.soc_permille >= 500, "fallback dt must not cause reverse jump");
	zassert_equal(st.acc_charge_ma_ms, acc_before + (int64_t)100000 * TEST_PERIOD_MS,
		      "fallback dt must equal default period (100ms)");
	/* 采纳新基准，防卡死（设计 §3-C：last_ts 更新为本帧 ts） */
	zassert_equal(st.last_ts_ms, 4000, "last_ts adopts new (smaller) ts to avoid deadlock");
}

/*
 * 分支 E：丢帧（大间隔）—— Δt 夹紧到上限 dt_cap，单帧 |ΔSOC| ≤ 设计上限
 *（设计 §3-E/§3.1，REQ-SOC-026 验收 3、REQ-SOC-030 验收 2）。
 * 设计上限：|ΔSOC|_max = MAX_CURRENT × dt_cap / DEN = 200000×1000/3.6e8 ≈ 0.56‰。
 * 用真实差值 1,000,000ms（远超 dt_cap=1000ms），若不夹紧会产生巨大跳变。
 */
ZTEST(bms_soc, test_step_frame_drop_clamped)
{
	struct bms_soc_coulomb_state st;
	struct bms_cell_meas m;
	struct bms_soc out;

	bms_soc_coulomb_state_reset(&st);
	make_meas(&m, 3600, 0, 0); /* 初值 500‰ */
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));
	int64_t acc_before = st.acc_charge_ma_ms;

	/* 巨大帧间隔（丢帧）：ts 跳到 1,000,000ms，电流 100000mA */
	make_meas(&m, 3600, 100000, 1000000);
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));

	/* Δt 应被夹紧到 dt_cap=1000ms：dQ = 100000×1000 = 1e8，而非 100000×1e6 */
	zassert_equal(st.acc_charge_ma_ms, acc_before + (int64_t)100000 * TEST_DT_CAP_MS,
		      "gap dt must be clamped to dt_cap (1000ms)");
	/* 单帧 |ΔSOC| 不超过设计上限（此处 1e8/3.6e8≈0.28‰，远小于 SOC 越界） */
	zassert_true(out.soc_permille <= 501,
		     "single-frame delta must stay within design cap (got %u)", out.soc_permille);
	zassert_true(out.soc_permille >= 0 && out.soc_permille <= 1000, "SOC stays in [0,1000]");
	zassert_equal(st.last_ts_ms, 1000000, "last_ts advances to current frame");
}

/*
 * 分支 F：电流超量程 —— 跳过本帧积分，返回 -EAGAIN，acc 不污染，ts 推进
 *（设计 §3-F，REQ-SOC-030 验收 2/3、REQ-SOC-029 验收 2 不发布）。
 */
ZTEST(bms_soc, test_step_over_range_current_skipped)
{
	struct bms_soc_coulomb_state st;
	struct bms_cell_meas m;
	struct bms_soc out;

	bms_soc_coulomb_state_reset(&st);
	make_meas(&m, 3600, 0, 0); /* 初值 500‰ */
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));
	int64_t acc_before = st.acc_charge_ma_ms;

	/* 超量程电流：MAX+1 = 200001mA（恰好越限） */
	make_meas(&m, 3600, TEST_MAX_CURR_MA + 1, 1000);
	int rc = bms_soc_coulomb_step(&st, &m, &out);
	zassert_equal(rc, -EAGAIN, "over-range current must be skipped (-EAGAIN)");
	/* acc 未被污染（REQ-SOC-030 验收 3） */
	zassert_equal(st.acc_charge_ma_ms, acc_before, "acc must NOT be polluted by skipped frame");
	/* out 保持上一稳定值 */
	zassert_equal(out.soc_permille, 500, "out keeps last stable SOC");
	/* ts 仍推进，防下帧误判丢帧（设计 §3-F） */
	zassert_equal(st.last_ts_ms, 1000, "last_ts advances even when skipped");
}

/* 超量程负电流（远超限）同样跳过（失效安全「远超限」类，REQ-SOC-030） */
ZTEST(bms_soc, test_step_over_range_negative_current_skipped)
{
	struct bms_soc_coulomb_state st;
	struct bms_cell_meas m;
	struct bms_soc out;

	bms_soc_coulomb_state_reset(&st);
	make_meas(&m, 3600, 0, 0);
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));

	make_meas(&m, 3600, -2000000, 1000); /* -2000A，远超 ±200A 量程 */
	zassert_equal(bms_soc_coulomb_step(&st, &m, &out), -EAGAIN,
		      "far-over-range negative current must be skipped");
	zassert_equal(out.soc_permille, 500, "SOC unchanged on skipped frame");
}

/*
 * 异常帧后正常帧可恢复正确积分（REQ-SOC-030 验收 3：状态不被污染到不可恢复）。
 * 序列：初始化 → 超量程帧(跳过) → 正常充电帧 应能正常积分。
 */
ZTEST(bms_soc, test_step_recovers_after_bad_frame)
{
	struct bms_soc_coulomb_state st;
	struct bms_cell_meas m;
	struct bms_soc out;

	bms_soc_coulomb_state_reset(&st);
	make_meas(&m, 3600, 0, 0); /* 初值 500‰ */
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));

	/* 坏帧：超量程，跳过 */
	make_meas(&m, 3600, TEST_MAX_CURR_MA + 1, 1000);
	zassert_equal(bms_soc_coulomb_step(&st, &m, &out), -EAGAIN);

	/* 正常帧：dt = 2000-1000 = 1000ms（last_ts 已推进到 1000），充电应正常积分 */
	make_meas(&m, 3600, 100000, 2000);
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));
	/* dQ = 100000×1000 = 1e8，acc 应从初值正确累加（未被坏帧污染） */
	zassert_equal(st.acc_charge_ma_ms, (int64_t)500 * TEST_DEN + (int64_t)100000 * 1000,
		      "normal frame after bad frame must integrate from clean acc");
	zassert_true(out.soc_permille >= 500, "recovery frame integrates correctly");
}

/*
 * REQ-SOC-027：持续充电直至饱和，SOC 稳定于 1000‰，不溢出/不回绕。
 * 恒流 +200000mA(满量程)，dt=1000ms/帧。每帧 dQ=2e8；从 500‰ 起到 1000‰
 * 需 acc 增量 = 500×DEN = 1.8e11 → 900 帧，跑 2000 帧确保饱和。
 */
ZTEST(bms_soc, test_step_clamp_to_full)
{
	struct bms_soc_coulomb_state st;
	struct bms_cell_meas m;
	struct bms_soc out;

	bms_soc_coulomb_state_reset(&st);
	make_meas(&m, 3600, 0, 0); /* 初值 500‰ */
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));

	uint32_t ts = 0;
	for (int i = 0; i < 2000; i++) {
		ts += 1000;
		make_meas(&m, 3600, 200000, ts);
		zassert_ok(bms_soc_coulomb_step(&st, &m, &out));
		zassert_true(out.soc_permille <= 1000, "must never exceed 1000permille");
	}
	zassert_equal(out.soc_permille, 1000, "saturated charge clamps at 1000permille");
}

/*
 * REQ-SOC-027：持续放电直至耗尽，SOC 稳定于 0‰，不下溢/不出现负值。
 */
ZTEST(bms_soc, test_step_clamp_to_empty)
{
	struct bms_soc_coulomb_state st;
	struct bms_cell_meas m;
	struct bms_soc out;

	bms_soc_coulomb_state_reset(&st);
	make_meas(&m, 3600, 0, 0); /* 初值 500‰ */
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));

	uint32_t ts = 0;
	for (int i = 0; i < 2000; i++) {
		ts += 1000;
		make_meas(&m, 3600, -200000, ts);
		zassert_ok(bms_soc_coulomb_step(&st, &m, &out));
		zassert_true(out.soc_permille <= 1000, "uint16 must not underflow/wrap");
	}
	zassert_equal(out.soc_permille, 0, "saturated discharge clamps at 0permille");
}

/*
 * REQ-SOC-034：长时间(≥24h 等效)大电流连续积分不溢出，SOC 计算正确且夹紧正常。
 * 以满量程 ±200000mA、dt_cap=1000ms/帧，24h = 86400 帧。int64 承载 acc 不溢。
 * 这里充电饱和后保持 1000‰（验证 acc 持续累加不破坏夹紧）。
 */
ZTEST(bms_soc, test_step_no_overflow_24h)
{
	struct bms_soc_coulomb_state st;
	struct bms_cell_meas m;
	struct bms_soc out;

	bms_soc_coulomb_state_reset(&st);
	make_meas(&m, 3600, 0, 0); /* 初值 500‰ */
	zassert_ok(bms_soc_coulomb_step(&st, &m, &out));

	uint32_t ts = 0;
	/* 24h @ 1帧/s 等效 = 86400 帧，恒满量程充电 */
	for (int i = 0; i < 86400; i++) {
		ts += 1000;
		make_meas(&m, 3600, 200000, ts);
		zassert_ok(bms_soc_coulomb_step(&st, &m, &out));
		zassert_true(out.soc_permille <= 1000,
			     "SOC must stay clamped, no overflow corruption");
	}
	/* 24h 大电流后仍稳定夹紧于 1000‰，acc 为正且未回绕成负（int64 充裕） */
	zassert_equal(out.soc_permille, 1000, "still clamped at full after 24h");
	zassert_true(st.acc_charge_ma_ms > 0, "acc must remain positive (no int64 wrap)");
}

/*========== Extern Function Implementations =================================*/

/*========== Externalized Static Function Implementations (Unit Test) ========*/
