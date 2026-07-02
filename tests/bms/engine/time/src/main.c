/*
 * bms_time engine 时间基准模块单元测试（ztest, native_sim / mps2-an386）。
 *
 * 覆盖三组纯函数 / 注入点（设计契约：docs/concept/runtime-model.md §2、§4）：
 *  - bms_time_after      —— 回绕安全的有符号差值时间比较
 *  - bms_time_due        —— 纯周期到期判定（推进 *next；落后过多重置防疯狂追赶）
 *  - bms_time_set_source —— 时钟源注入 / 复位，使 bms_time_now_ms 可脱离内核单测
 *
 * TDD 红灯说明：本增量**不实现** app/src/bms/engine/time.c，故 CMakeLists.txt
 * 未列入 time.c。链接期应报 `undefined reference to bms_time_*` —— 这是**预期红灯**
 * （链接期未定义符号，非断言失败）。coder 阶段补齐实现后转绿。
 *
 * 每个用例注释回链设计契约（runtime-model §2 有符号差回绕安全 / §4 到期判定纯函数）。
 */
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/ztest.h>

#include "bms/engine/time.h"

ZTEST_SUITE(bms_time, NULL, NULL, NULL, NULL, NULL);

/* ============================================================
 * bms_time_after：回绕安全的有符号差值比较
 * 回链：runtime-model §2「所有时间比较必须用有符号差值以回绕安全」
 * ============================================================ */

/* now > deadline → 已过（真）；now == deadline → 已到（真）；now < deadline → 未到（假） */
ZTEST(bms_time, test_after_basic_ordering)
{
	/* Verifies runtime-model §2: (int32_t)(now-deadline)>=0 的基本序 */
	zassert_true(bms_time_after(1000, 500), "now>deadline must be after");
	zassert_true(bms_time_after(500, 500), "now==deadline must count as after");
	zassert_false(bms_time_after(499, 500), "now<deadline must NOT be after");
}

/*
 * 回绕：deadline 接近 32 位上限、now 已回绕到小值，二者有符号差 >=0 应判为「已过」。
 * deadline=0xFFFFFFF0, now=0x00000005 → (int32_t)(now-deadline)=+21 >=0 → 真。
 */
ZTEST(bms_time, test_after_wraparound_is_safe)
{
	/* Verifies runtime-model §2: 有符号差回绕安全（禁止无符号大小比较） */
	zassert_true(bms_time_after(0x00000005u, 0xFFFFFFF0u),
		     "wrapped now must be treated as after a pre-wrap deadline");
	/* 反向：now 尚未到达（仍在 deadline 之前，差值为负）→ 未过 */
	zassert_false(bms_time_after(0xFFFFFFF0u, 0x00000005u),
		      "pre-wrap now must NOT be after a post-wrap deadline");
}

/* ============================================================
 * bms_time_due：纯周期到期判定
 * 回链：runtime-model §4「到期判定为纯函数；落后过多重置 next=now+period」
 * ============================================================ */

/* 未到期：返回 false 且 *next 不变 */
ZTEST(bms_time, test_due_not_yet)
{
	/* Verifies runtime-model §4: 未到期无副作用 */
	uint32_t next = 1000;

	zassert_false(bms_time_due(999, &next, 100), "before deadline must be not due");
	zassert_equal(next, 1000, "*next must be unchanged when not due");
}

/* 到期：返回 true 且 *next 前进一个 period（恰好到点，正常节拍） */
ZTEST(bms_time, test_due_advances_by_period)
{
	/* Verifies runtime-model §4: 到期推进 *next += period（绝对节拍无漂移） */
	uint32_t next = 1000;

	zassert_true(bms_time_due(1000, &next, 100), "at deadline must be due");
	zassert_equal(next, 1100, "*next must advance by exactly one period");
}

/*
 * 落后追赶：now 远超 next（多期滞后）→ 到期返回 true，但 *next 被重置为 ~now+period，
 * 而非仅 +period 后仍落后而在后续帧疯狂累加追赶。
 */
ZTEST(bms_time, test_due_catchup_resets_next)
{
	/* Verifies runtime-model §4: 落后过多把 next 重置为 now+period 防疯狂追赶 */
	uint32_t next = 1000;
	uint32_t now = 5000; /* 落后 40 个 period */

	zassert_true(bms_time_due(now, &next, 100), "far-behind must be due");
	zassert_equal(next, now + 100,
		      "*next must reset to now+period (not chase with tiny increments)");
}

/*
 * 回绕安全：next 接近 32 位上限，now 已回绕到小值，应判为到期并正确推进，
 * 不因无符号比较把「已到」误判为「远未到」。
 */
ZTEST(bms_time, test_due_wraparound_is_safe)
{
	/* Verifies runtime-model §2/§4: 到期判定同样用有符号差回绕安全 */
	uint32_t next = 0xFFFFFFF0u;
	uint32_t now = 0x00000005u; /* 已越过 next（有符号差 +21） */

	zassert_true(bms_time_due(now, &next, 100),
		     "wrapped now past next must be due (signed diff)");
	/* 推进后 next 应领先于 now（+period 或重置为 now+period），不得回退到 now 之前 */
	zassert_true(bms_time_after(next, now), "advanced *next must be at/after now");
}

/* ============================================================
 * bms_time_set_source：时钟源注入 / 复位
 * 回链：runtime-model §2「now_ms 必须可注入」、§9 可测性
 * ============================================================ */

static uint32_t fake_clock_value;

static uint32_t fake_clock(void)
{
	return fake_clock_value;
}

/* 注入返回固定值的假时钟 → bms_time_now_ms() 返回该值 */
ZTEST(bms_time, test_set_source_injects_clock)
{
	/* Verifies runtime-model §2: now_ms 可注入以脱离内核单测 */
	fake_clock_value = 0xDEADBEEFu;
	bms_time_set_source(fake_clock);
	zassert_equal(bms_time_now_ms(), 0xDEADBEEFu, "now must return injected source value");

	fake_clock_value = 42u;
	zassert_equal(bms_time_now_ms(), 42u, "now must track injected source updates");

	/* 复位，避免污染其他用例 */
	bms_time_set_source(NULL);
}

/* set_source(NULL) 复位为默认内核源：不再返回上次注入的固定值 */
ZTEST(bms_time, test_set_source_null_resets_to_default)
{
	/* Verifies runtime-model §2: NULL 复位为默认(k_uptime_get_32) */
	fake_clock_value = 777u;
	bms_time_set_source(fake_clock);
	zassert_equal(bms_time_now_ms(), 777u, "injected before reset");

	bms_time_set_source(NULL);
	/* 复位后不应仍锁定在注入的固定值 777（默认源为内核单调时间） */
	zassert_not_equal(bms_time_now_ms(), 777u,
			  "after NULL reset, now must no longer return the injected constant");
}
