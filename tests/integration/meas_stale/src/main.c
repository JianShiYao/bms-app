/*
 * Integration (TDD 红灯): 测量过期/源失联 fail-safe（M3b-1）。
 *
 * 目标行为（coder 将实现）：当 DB_CELL_MEAS 虽 valid 但**时间戳过期**
 * （now - meas.timestamp_ms > 容忍，默认 300ms）时，bms_task_safety_step 应：
 *   - 置诊断 BMS_DIAG_MEAS_STALE(ERROR)；
 *   - **不据过期数据闭合** —— 保护保持失效安全默认 {FAULT, OPEN}，
 *     经 bms_next_state → bms_state 进 FAULT、接触器 OPEN。
 *
 * 设计依据：
 *  - docs/concept/diagnostics-fault-model.md §5（过期/源失联的诊断上报与严重度）。
 *  - docs/concept/data-model.md（bms_db entry 的 stale/时间戳纪律：值 + 时间戳 + 有效性）。
 *  - docs/concept/runtime-model.md §2（全系统唯一可注入时间源；时间比较回绕安全）。
 *  - CLAUDE.md §3 失效安全：接触器默认 OPEN，仅判定 NORMAL 才 CLOSED。
 *
 * AFE-off 植入手法（为何能确定性造 stale）：
 *   本测试**不注入 CONFIG_BMS_AFE**（见 CMakeLists / prj.conf）。task.c 的
 *   run_measurement() 采样体整段包在 `#if defined(CONFIG_BMS_AFE)` 内，AFE 关时
 *   safety_step 不采样、也不覆盖 DB_CELL_MEAS。于是测试可直接 bms_db_write_cell_meas()
 *   植入一帧带**指定 timestamp_ms** 的合法（validity=ALL、量程内）测量，且该帧在后续
 *   safety_step 中不会被刷新 —— 从而「valid 但时间戳过期」这一状态完全由测试掌控。
 *
 * 红灯性质：coder 尚未实现 stale 检测/上报与 run_protection_and_bms 重构，
 * 故 test_stale_meas_flags_and_faults 应**断言失败**（MEAS_STALE 位未置 /
 * 接触器未按失效安全 OPEN），属预期红灯（断言失败，非链接错 —— enum 值已加故可编译）。
 */
#include <stdint.h>
#include <zephyr/ztest.h>

#include "bms/db.h"
#include "bms/diag.h"
#include "bms/protection.h"
#include "bms/task.h"
#include "bms/time.h"
#include "bms/types.h"

/* 过期容忍默认 300ms（coder 实现时的判定阈值）；用例 2 用 400ms > 容忍确保过期。 */
#define STALE_TOLERANCE_MS 300U

/* 注入时间源：固定的单调毫秒，脱离内核时钟以确定驱动 stale 判定。 */
static uint32_t test_now_ms;

static uint32_t injected_time_source(void)
{
	return test_now_ms;
}

/*
 * 造一帧合法（validity=ALL、电压/温度/电流在合理量程内使保护判 NORMAL）测量并植入，
 * 采样时刻由参数 ts_ms 指定。量程参照 protection 默认阈值：
 *   OV=4250mV / UV=2800mV / OC=50000mA / OT=600(0.1℃) —— 取中间安全值。
 */
static void inject_valid_meas_at(uint32_t ts_ms)
{
	struct bms_cell_meas m = {
		.timestamp_ms = ts_ms,
		.pack_current_ma = 0,           /* |0| < 50000mA，非过流 */
		.validity = BMS_MEAS_VALID_ALL, /* 全有效 */
	};

	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		m.cell_mv[i] = 3700; /* 2800 < 3700 < 4250，非欠压/过压 */
	}
	for (int i = 0; i < BMS_TEMP_SENSOR_COUNT; i++) {
		m.temp_dci[i] = 250; /* 25.0℃ < 60.0℃，非过温 */
	}

	zassert_ok(bms_db_write_cell_meas(&m), "seed cell_meas write failed");
}

static void *meas_stale_setup(void)
{
	/* 全系统唯一时间源注入（runtime-model §2）：让 bms_time_now_ms() 返回受控值。 */
	test_now_ms = 0U;
	bms_time_set_source(injected_time_source);
	return NULL;
}

static void meas_stale_before(void *fixture)
{
	ARG_UNUSED(fixture);

	/* 每个用例前重置注入时间与 engine 链（AFE 关：初始化链不含 afe）。 */
	test_now_ms = 0U;

	zassert_ok(bms_db_init(), "db init failed");
	zassert_ok(bms_diag_init(), "diag init failed");
	zassert_ok(bms_protection_init(), "protection init failed");
	zassert_ok(bms_task_init(), "task init (no-thread) failed");
}

static void meas_stale_teardown(void *fixture)
{
	ARG_UNUSED(fixture);

	/* 复位为默认内核时间源，避免污染后续测试。 */
	bms_time_set_source(NULL);
}

ZTEST_SUITE(meas_stale, NULL, meas_stale_setup, meas_stale_before, NULL, meas_stale_teardown);

/*
 * 用例 1：新鲜测量不应判过期。
 * 植入 timestamp_ms = now（T），safety_step(T)；now - ts = 0 <= 容忍，
 * 故 MEAS_STALE 位不应在 active_mask。
 * 依据 diagnostics-fault-model §5（未过期不上报）/ data-model stale。
 */
ZTEST(meas_stale, test_fresh_meas_not_stale)
{
	const uint32_t T = 1000U;
	struct bms_diag_state diag;

	test_now_ms = T;
	inject_valid_meas_at(T); /* 采样时刻 == now，未过期 */

	bms_task_safety_step(T);

	zassert_ok(bms_diag_get_state(&diag), "diag get_state failed");
	zassert_true((diag.active_mask & BIT(BMS_DIAG_MEAS_STALE)) == 0U,
		     "fresh measurement must not raise MEAS_STALE");
}

/*
 * 用例 2（核心红灯）：过期测量应置 MEAS_STALE(ERROR) 且失效安全 OPEN。
 * 植入 timestamp_ms = T，注入 now = T + 400（> 300ms 容忍），safety_step(T+400)。
 * 期望（coder 实现后）：
 *   - 诊断：MEAS_STALE 位在 active_mask，且 max_severity >= BMS_DIAG_ERROR；
 *   - 整机：contactor == OPEN（不据过期数据闭合，失效安全默认 {FAULT, OPEN}）。
 * 当前 coder 未实现 stale 检测/上报与 run_protection_and_bms 重构，故本用例应**断言失败**
 * （MEAS_STALE 未置位 / 接触器未按失效安全）—— 预期红灯。
 * 依据 diagnostics-fault-model §5、data-model stale、runtime-model §2、CLAUDE.md §3 失效安全。
 */
ZTEST(meas_stale, test_stale_meas_flags_and_faults)
{
	const uint32_t T = 1000U;
	const uint32_t now = T + STALE_TOLERANCE_MS + 100U; /* T + 400 > 容忍 */
	struct bms_diag_state diag;
	struct bms_state_snapshot state;
	struct bms_db_meta meta;

	inject_valid_meas_at(T); /* 采样时刻停在 T */
	test_now_ms = now;       /* 现在已到 T+400，帧过期且不被 AFE 刷新 */

	bms_task_safety_step(now);

	/* 诊断：MEAS_STALE 置位且严重度达 ERROR 及以上。 */
	zassert_ok(bms_diag_get_state(&diag), "diag get_state failed");
	zassert_true((diag.active_mask & BIT(BMS_DIAG_MEAS_STALE)) != 0U,
		     "stale measurement must raise BMS_DIAG_MEAS_STALE");
	zassert_true(diag.max_severity >= BMS_DIAG_ERROR,
		     "stale measurement severity must be ERROR or above");

	/* 失效安全：不据过期数据闭合，接触器保持 OPEN。 */
	zassert_ok(bms_db_read_bms_state(&state, &meta), "bms_state read failed");
	zassert_true(meta.valid, "bms_state snapshot must be valid after a safety step");
	zassert_equal(state.contactor, BMS_CONTACTOR_OPEN,
		      "fail-safe: contactor must stay OPEN on stale measurement");
}
