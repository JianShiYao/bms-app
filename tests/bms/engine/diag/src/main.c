/*
 * bms_diag 每条目诊断生命周期纯状态机单元测试（ztest, native_sim / mps2-an386）。
 *
 * 覆盖 bms_diag_entry_step —— 去抖 confirm/clear + 锁存 latch 的纯步进函数
 * （无副作用、输入注入时间）。设计契约：
 *  - docs/concept/diagnostics-fault-model.md §4（生命周期状态机）/§5（锁存与复位）
 *  - docs/concept/runtime-model.md §2（时间比较必须用有符号差以回绕安全）
 *
 * TDD 红灯说明：本增量**不实现** bms_diag_entry_step（不改 diag.c / task.c，
 * 不改现有 bms_diag_report 签名，additive）。链接期应报
 * `undefined reference to bms_diag_entry_step` —— 这是**预期红灯**
 * （链接期未定义符号，非断言失败）。coder 阶段补齐实现并重连后转绿。
 *
 * 每个用例注释回链设计契约。
 */

/*========== Includes ========================================================*/
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/ztest.h>

#include "bms/engine/diag.h"

/*========== Macros and Definitions ==========================================*/

/*========== Static Constant and Variable Definitions ========================*/

/*========== Extern Constant and Variable Definitions ========================*/

/*========== Static Function Prototypes ======================================*/

/*========== Static Function Implementations =================================*/
ZTEST_SUITE(bms_diag, NULL, NULL, NULL, NULL, NULL);

/* ============================================================
 * 1. confirm 去抖：原始持续激活满 confirm_time 才 ACTIVE
 * 回链：diagnostics-fault-model §4（CONFIRMING→ACTIVE 去抖）
 * ============================================================ */
ZTEST(bms_diag, test_confirm_debounce_to_active)
{
	const struct bms_diag_entry_cfg cfg = {
		.severity = BMS_DIAG_ERROR,
		.confirm_time_ms = 30,
		.clear_time_ms = 0,
		.latch = false,
	};
	struct bms_diag_entry_rt rt = {.state = BMS_DIAG_LIFE_INACTIVE, .since_ms = 0};

	/* raw=true now=0 → 进 CONFIRMING（尚未 ACTIVE） */
	bms_diag_entry_step(&cfg, &rt, true, 0);
	zassert_equal(rt.state, BMS_DIAG_LIFE_CONFIRMING,
		      "raw active must first enter CONFIRMING for debounce");

	/* now=20 未满 confirm_time → 仍 CONFIRMING */
	bms_diag_entry_step(&cfg, &rt, true, 20);
	zassert_equal(rt.state, BMS_DIAG_LIFE_CONFIRMING,
		      "before confirm_time elapses must stay CONFIRMING");

	/* now=30 满 confirm_time → ACTIVE */
	bms_diag_entry_step(&cfg, &rt, true, 30);
	zassert_equal(rt.state, BMS_DIAG_LIFE_ACTIVE, "at confirm_time must transition to ACTIVE");
}

/* ============================================================
 * 2. confirm 中止：CONFIRMING 中途 raw=false → 回 INACTIVE
 * 回链：diagnostics-fault-model §4（去抖中止）
 * ============================================================ */
ZTEST(bms_diag, test_confirm_aborts_on_raw_clear)
{
	const struct bms_diag_entry_cfg cfg = {
		.severity = BMS_DIAG_ERROR,
		.confirm_time_ms = 30,
		.clear_time_ms = 0,
		.latch = false,
	};
	struct bms_diag_entry_rt rt = {.state = BMS_DIAG_LIFE_INACTIVE, .since_ms = 0};

	bms_diag_entry_step(&cfg, &rt, true, 0);
	zassert_equal(rt.state, BMS_DIAG_LIFE_CONFIRMING, "enter CONFIRMING");

	/* 去抖窗内 raw 撤销 → 中止，回 INACTIVE */
	bms_diag_entry_step(&cfg, &rt, false, 10);
	zassert_equal(rt.state, BMS_DIAG_LIFE_INACTIVE,
		      "raw clear during CONFIRMING must abort back to INACTIVE");
}

/* ============================================================
 * 3. confirm=0 立即置位（安全要点：故障即刻生效）
 * 回链：diagnostics-fault-model §4（confirm_time_ms==0 一步到 ACTIVE）
 * ============================================================ */
ZTEST(bms_diag, test_confirm_zero_activates_immediately)
{
	const struct bms_diag_entry_cfg cfg = {
		.severity = BMS_DIAG_CRITICAL,
		.confirm_time_ms = 0,
		.clear_time_ms = 0,
		.latch = false,
	};
	struct bms_diag_entry_rt rt = {.state = BMS_DIAG_LIFE_INACTIVE, .since_ms = 0};

	/* confirm=0：raw=true 同一步即 ACTIVE（不经 CONFIRMING 停留） */
	bms_diag_entry_step(&cfg, &rt, true, 0);
	zassert_equal(rt.state, BMS_DIAG_LIFE_ACTIVE,
		      "confirm_time==0 must activate immediately in one step");
}

/* ============================================================
 * 4. clear 去抖：ACTIVE 后 raw=false 满 clear_time 才清除
 * 回链：diagnostics-fault-model §4（ACTIVE→CLEARING→INACTIVE 去抖）
 * ============================================================ */
ZTEST(bms_diag, test_clear_debounce_to_inactive)
{
	const struct bms_diag_entry_cfg cfg = {
		.severity = BMS_DIAG_ERROR,
		.confirm_time_ms = 0,
		.clear_time_ms = 50,
		.latch = false,
	};
	struct bms_diag_entry_rt rt = {.state = BMS_DIAG_LIFE_INACTIVE, .since_ms = 0};

	/* 先到 ACTIVE（confirm=0 立即） */
	bms_diag_entry_step(&cfg, &rt, true, 100);
	zassert_equal(rt.state, BMS_DIAG_LIFE_ACTIVE, "reach ACTIVE first");

	/* raw=false now=200 → 进 CLEARING */
	bms_diag_entry_step(&cfg, &rt, false, 200);
	zassert_equal(rt.state, BMS_DIAG_LIFE_CLEARING,
		      "raw clear must first enter CLEARING for debounce");

	/* now=240 未满 clear_time（起于 200，需 250）→ 仍 CLEARING */
	bms_diag_entry_step(&cfg, &rt, false, 240);
	zassert_equal(rt.state, BMS_DIAG_LIFE_CLEARING,
		      "before clear_time elapses must stay CLEARING");

	/* now=250 满 clear_time → INACTIVE（latch=false） */
	bms_diag_entry_step(&cfg, &rt, false, 250);
	zassert_equal(rt.state, BMS_DIAG_LIFE_INACTIVE,
		      "at clear_time with latch=false must return INACTIVE");
}

/* ============================================================
 * 5. clear 中止：CLEARING 中途 raw=true → 回 ACTIVE
 * 回链：diagnostics-fault-model §4（清除去抖中止）
 * ============================================================ */
ZTEST(bms_diag, test_clear_aborts_on_raw_reassert)
{
	const struct bms_diag_entry_cfg cfg = {
		.severity = BMS_DIAG_ERROR,
		.confirm_time_ms = 0,
		.clear_time_ms = 50,
		.latch = false,
	};
	struct bms_diag_entry_rt rt = {.state = BMS_DIAG_LIFE_INACTIVE, .since_ms = 0};

	bms_diag_entry_step(&cfg, &rt, true, 100);
	zassert_equal(rt.state, BMS_DIAG_LIFE_ACTIVE, "reach ACTIVE");

	bms_diag_entry_step(&cfg, &rt, false, 200);
	zassert_equal(rt.state, BMS_DIAG_LIFE_CLEARING, "enter CLEARING");

	/* 清除去抖窗内 raw 重新触发 → 回 ACTIVE */
	bms_diag_entry_step(&cfg, &rt, true, 220);
	zassert_equal(rt.state, BMS_DIAG_LIFE_ACTIVE,
		      "raw reassert during CLEARING must abort back to ACTIVE");
}

/* ============================================================
 * 6. latch：清除条件满足后转 LATCHED，此后恒 LATCHED
 * 回链：diagnostics-fault-model §5（锁存：清除后保持直至复位）
 * ============================================================ */
ZTEST(bms_diag, test_latch_holds_after_clear)
{
	const struct bms_diag_entry_cfg cfg = {
		.severity = BMS_DIAG_CRITICAL,
		.confirm_time_ms = 0,
		.clear_time_ms = 50,
		.latch = true,
	};
	struct bms_diag_entry_rt rt = {.state = BMS_DIAG_LIFE_INACTIVE, .since_ms = 0};

	bms_diag_entry_step(&cfg, &rt, true, 100);
	zassert_equal(rt.state, BMS_DIAG_LIFE_ACTIVE, "reach ACTIVE");

	/* raw=false 满 clear_time → LATCHED（而非 INACTIVE） */
	bms_diag_entry_step(&cfg, &rt, false, 200);
	zassert_equal(rt.state, BMS_DIAG_LIFE_CLEARING, "enter CLEARING");
	bms_diag_entry_step(&cfg, &rt, false, 250);
	zassert_equal(rt.state, BMS_DIAG_LIFE_LATCHED,
		      "at clear_time with latch=true must transition to LATCHED");

	/* 此后 raw 无论真假恒 LATCHED（复位属后续增量，不自动清） */
	bms_diag_entry_step(&cfg, &rt, false, 300);
	zassert_equal(rt.state, BMS_DIAG_LIFE_LATCHED, "LATCHED holds under raw=false");
	bms_diag_entry_step(&cfg, &rt, true, 400);
	zassert_equal(rt.state, BMS_DIAG_LIFE_LATCHED, "LATCHED holds under raw=true");
}

/* ============================================================
 * 7. 回绕安全：since 接近 32 位上限、now 已回绕，去抖判定仍正确
 * 回链：runtime-model §2（有符号差回绕安全）/ diagnostics-fault-model §4
 * ============================================================ */
ZTEST(bms_diag, test_debounce_wraparound_is_safe)
{
	const struct bms_diag_entry_cfg cfg = {
		.severity = BMS_DIAG_ERROR,
		.confirm_time_ms = 30,
		.clear_time_ms = 0,
		.latch = false,
	};
	/* since 起于回绕前的大值；now 已回绕到小值，有符号差 = confirm_time */
	uint32_t since = 0xFFFFFFF0u;
	struct bms_diag_entry_rt rt = {.state = BMS_DIAG_LIFE_INACTIVE, .since_ms = 0};

	/* now=since → 进 CONFIRMING（since 记为 now） */
	bms_diag_entry_step(&cfg, &rt, true, since);
	zassert_equal(rt.state, BMS_DIAG_LIFE_CONFIRMING, "enter CONFIRMING at pre-wrap since");

	/* now 已回绕：(int32_t)(now - since) = 20 < 30 → 仍 CONFIRMING */
	bms_diag_entry_step(&cfg, &rt, true, since + 20u);
	zassert_equal(rt.state, BMS_DIAG_LIFE_CONFIRMING,
		      "wrapped now with signed diff < confirm_time must stay CONFIRMING");

	/* now 已回绕：(int32_t)(now - since) = 30 >= 30 → ACTIVE（禁止无符号误判） */
	bms_diag_entry_step(&cfg, &rt, true, since + 30u);
	zassert_equal(rt.state, BMS_DIAG_LIFE_ACTIVE,
		      "wrapped now with signed diff >= confirm_time must reach ACTIVE");
}

/*========== Extern Function Implementations =================================*/

/*========== Externalized Static Function Implementations (Unit Test) ========*/
