/*
 * bms_sys_mon 任务健康核（心跳与运行时间）纯函数单元测试
 * （ztest, native_sim / mps2-an386）。
 *
 * 覆盖 enter/exit 运行时间记录、峰值保持、运行超时、心跳超时（含回绕安全）
 * 与「未 enter 不误报」兜底。设计契约：
 *  - docs/concept/runtime-model.md §6（bms_sys_mon：心跳与运行时间）
 *  - docs/concept/runtime-model.md §2（时间比较必须用有符号差以回绕安全）
 *  - docs/concept/runtime-model.md §9（一切时间判定以注入 now_ms 纯测）
 *  - docs/concept/architecture.md §6（ADR-ARCH-003 任务健康监控）
 *
 * TDD 红灯说明：本增量下 sys_mon.c 为**空存根**（enter/exit 空壳、eval 返回全 0），
 * **不实现**任何真实逻辑（真实逻辑归 coder）。因此测试可编译链接，但断言失败
 * —— 这是**预期红灯（断言失败型）**，非链接期未定义符号。coder 阶段补齐实现后转绿。
 *
 * 时间判定全部以 eval/enter/exit 的 now_ms 入参直接注入，不依赖内核时钟。
 *
 * 每个用例注释回链设计契约。
 */
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/ztest.h>

#include "bms/engine/sys_mon.h"

ZTEST_SUITE(bms_sys_mon, NULL, NULL, NULL, NULL, NULL);

/* ============================================================
 * 1. enter/exit 记录本次运行时间
 * 回链：runtime-model §6（进入/退出记录本次运行时间）
 * ============================================================ */
ZTEST(bms_sys_mon, test_enter_exit_records_runtime)
{
	struct bms_sys_mon_rt rt = {0};

	bms_sys_mon_enter(&rt, 100);
	bms_sys_mon_exit(&rt, 130);

	/* 本次运行时间 = exit - enter = 130 - 100 = 30 */
	zassert_equal(rt.last_runtime_ms, 30u,
		      "last_runtime_ms must equal exit-enter (130-100=30)");
	/* enter 必须置 seen，供心跳判定的开机兜底 */
	zassert_true(rt.seen, "enter must mark rt as seen");
	/* enter 记录心跳时刻 */
	zassert_equal(rt.last_enter_ms, 100u, "enter must record last_enter_ms");
}

/* ============================================================
 * 2. 峰值保持：峰值取历史最大，不被较小值覆盖
 * 回链：runtime-model §6（记录峰值运行时间）
 * ============================================================ */
ZTEST(bms_sys_mon, test_peak_runtime_holds_max)
{
	struct bms_sys_mon_rt rt = {0};

	/* 第一轮：运行时间 30 */
	bms_sys_mon_enter(&rt, 100);
	bms_sys_mon_exit(&rt, 130);
	zassert_equal(rt.peak_runtime_ms, 30u, "peak after first round must be 30");

	/* 第二轮：运行时间 10（更小）→ 峰值仍保持 30 */
	bms_sys_mon_enter(&rt, 200);
	bms_sys_mon_exit(&rt, 210);
	zassert_equal(rt.last_runtime_ms, 10u, "last_runtime_ms must track latest (10)");
	zassert_equal(rt.peak_runtime_ms, 30u,
		      "peak must hold historical max (30), not be overwritten by smaller 10");
}

/* ============================================================
 * 3. 运行超时：峰值运行时间 > 声明 WCET
 * 回链：runtime-model §6（运行超时判据：> 声明 WCET）
 * ============================================================ */
ZTEST(bms_sys_mon, test_runtime_overrun_on_peak_gt_wcet)
{
	const struct bms_sys_mon_cfg cfg = {.wcet_ms = 20, .heartbeat_timeout_ms = 0};

	/* peak=30 > wcet=20 → 运行超时 */
	struct bms_sys_mon_rt over = {.peak_runtime_ms = 30, .seen = true};
	struct bms_sys_mon_health h_over = bms_sys_mon_eval(&cfg, &over, 0);
	zassert_true(h_over.runtime_overrun, "peak(30) > wcet(20) must set runtime_overrun");

	/* peak=15 <= wcet=20 → 不超时 */
	struct bms_sys_mon_rt ok = {.peak_runtime_ms = 15, .seen = true};
	struct bms_sys_mon_health h_ok = bms_sys_mon_eval(&cfg, &ok, 0);
	zassert_false(h_ok.runtime_overrun, "peak(15) <= wcet(20) must not set runtime_overrun");
}

/* ============================================================
 * 4. 心跳超时：距上次 enter 超过阈值判超时
 * 回链：runtime-model §6（心跳超时：超过阈值未再 enter）/ §2（有符号差）
 * ============================================================ */
ZTEST(bms_sys_mon, test_heartbeat_timeout_on_threshold)
{
	const struct bms_sys_mon_cfg cfg = {.wcet_ms = 0, .heartbeat_timeout_ms = 100};
	struct bms_sys_mon_rt rt = {0};

	bms_sys_mon_enter(&rt, 1000);

	/* now=1000+150 > 1000+100 阈值 → 心跳超时 */
	struct bms_sys_mon_health late = bms_sys_mon_eval(&cfg, &rt, 1000 + 150);
	zassert_true(late.heartbeat_timeout,
		     "elapsed(150) > heartbeat_timeout(100) must set heartbeat_timeout");

	/* now=1000+50 < 1000+100 阈值 → 未超时 */
	struct bms_sys_mon_health early = bms_sys_mon_eval(&cfg, &rt, 1000 + 50);
	zassert_false(early.heartbeat_timeout,
		      "elapsed(50) < heartbeat_timeout(100) must not set heartbeat_timeout");
}

/* ============================================================
 * 5. 未 enter 不判心跳超时（seen=false 兜底，避免开机即误报）
 * 回链：runtime-model §6（未 enter 过则不判超时）
 * ============================================================ */
ZTEST(bms_sys_mon, test_no_heartbeat_timeout_before_first_enter)
{
	const struct bms_sys_mon_cfg cfg = {.wcet_ms = 0, .heartbeat_timeout_ms = 100};
	/* rt 全 0：seen=false、last_enter_ms=0（从未 enter 过） */
	struct bms_sys_mon_rt rt = {0};

	/* 即使 now 为任意大值，seen=false 也不得判心跳超时（开机兜底） */
	struct bms_sys_mon_health h = bms_sys_mon_eval(&cfg, &rt, 0xFFFFFFFFu);
	zassert_false(h.heartbeat_timeout,
		      "seen=false (never entered) must never report heartbeat_timeout");
}

/* ============================================================
 * 6. 心跳超时回绕安全：last_enter 取回绕前大值、now 回绕到小值
 * 回链：runtime-model §2（有符号差回绕安全，禁止无符号误判）
 * ============================================================ */
ZTEST(bms_sys_mon, test_heartbeat_timeout_wraparound_is_safe)
{
	const struct bms_sys_mon_cfg cfg = {.wcet_ms = 0, .heartbeat_timeout_ms = 100};

	/* last_enter 起于回绕前的大值；deadline = last_enter + 100 会跨 32 位回绕 */
	uint32_t last_enter = 0xFFFFFFF0u;
	struct bms_sys_mon_rt rt = {.last_enter_ms = last_enter, .seen = true};

	/* now 已回绕：(int32_t)(now - (last_enter+100)) = 50 - 100 = -50 < 0 → 未超时 */
	struct bms_sys_mon_health early = bms_sys_mon_eval(&cfg, &rt, last_enter + 50u);
	zassert_false(early.heartbeat_timeout,
		      "wrapped now with signed elapsed(50) < timeout(100) must not report timeout");

	/* now 已回绕：(int32_t)(now - (last_enter+100)) = 0 >= 0 → 心跳超时（禁止无符号误判） */
	struct bms_sys_mon_health due = bms_sys_mon_eval(&cfg, &rt, last_enter + 100u);
	zassert_true(due.heartbeat_timeout,
		     "wrapped now with signed elapsed(100) >= timeout(100) must report timeout");
}
