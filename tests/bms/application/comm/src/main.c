/*
 * comm 模块 CAN 上报周期合法化单元测试（ztest, native_sim / mps2-an386）
 *
 * 被测：纯函数核心 bms_comm_clamp_period_ms(requested, lo, hi)
 *   （源 app/src/bms/comm/comm_period.c；声明 app/include/bms/comm.h）
 *
 * 该函数为「编译期 Kconfig range + 运行期 clamp」双保险中运行期那一侧：把请求
 * 周期钳制到合法闭区间 [lo, hi]。无副作用、无全局/硬件/Kconfig 依赖、边界以入参
 * 注入，故 host 可直接调用断言（对齐 bms_meas_validate(m, limits) 范式）。
 *
 * 测试侧本地常量镜像 app/Kconfig 中 BMS_COMM_REPORT_PERIOD_MS 的 `range 10 60000`
 * 端点（与 comm.c 内 BMS_COMM_PERIOD_MIN/MAX_MS 一致），供边界断言。
 *
 * 设计：03-design.md §7.2（套件名 bms_comm）。每用例回链 REQ-COMM-NNN。
 */
#include <stdint.h>
#include <zephyr/ztest.h>

#include "bms/comm.h"

/* 镜像 Kconfig `range 10 60000` 端点（= comm.c 的 BMS_COMM_PERIOD_MIN/MAX_MS）。 */
#define TEST_P_MIN 10
#define TEST_P_MAX 60000

ZTEST_SUITE(bms_comm, NULL, NULL, NULL, NULL, NULL);

/* ============================================================
 * 正常路径：合法值原样生效
 * ============================================================ */

/* Verifies REQ-COMM-001: 上报周期由配置值决定（合法值原样生效）。
 * Verifies REQ-COMM-002: 默认 200ms 落在合法区间内，原样返回 200。 */
ZTEST(bms_comm, test_default_period_is_200)
{
	zassert_equal(bms_comm_clamp_period_ms(200, TEST_P_MIN, TEST_P_MAX), 200,
		      "默认 200ms 落在 [%d, %d] 内应原样生效", TEST_P_MIN, TEST_P_MAX);

	/* 配置可观察地改变周期：两个不同合法值分别原样返回（REQ-COMM-001）。 */
	zassert_equal(bms_comm_clamp_period_ms(100, TEST_P_MIN, TEST_P_MAX), 100,
		      "合法值 100 应原样生效");
	zassert_equal(bms_comm_clamp_period_ms(1000, TEST_P_MIN, TEST_P_MAX), 1000,
		      "合法值 1000 应原样生效");
	zassert_not_equal(bms_comm_clamp_period_ms(100, TEST_P_MIN, TEST_P_MAX),
			  bms_comm_clamp_period_ms(1000, TEST_P_MIN, TEST_P_MAX),
			  "不同合法配置应得到不同生效周期");
}

/* ============================================================
 * 边界：取值范围端点 + range 约束自洽
 * ============================================================ */

/* Verifies REQ-COMM-004: 取值范围 [P_min, P_max]，端点合法且原样返回；
 * 断言 P_min>0、P_max>=200、P_min<=200<=P_max（与 default 200 不冲突）。 */
ZTEST(bms_comm, test_period_range_bounds)
{
	/* 端点（含端点）合法，原样返回 */
	zassert_equal(bms_comm_clamp_period_ms(TEST_P_MIN, TEST_P_MIN, TEST_P_MAX), TEST_P_MIN,
		      "下界端点 %d 应原样返回", TEST_P_MIN);
	zassert_equal(bms_comm_clamp_period_ms(TEST_P_MAX, TEST_P_MIN, TEST_P_MAX), TEST_P_MAX,
		      "上界端点 %d 应原样返回", TEST_P_MAX);

	/* range 自洽性约束（REQ-COMM-004 / REQ-COMM-002 不冲突）：
	 * P_min 严格 > 0、P_max >= 200、200 落在 [P_min, P_max] 内。 */
	zassert_true(TEST_P_MIN > 0, "P_min 必须严格 > 0");
	zassert_true(TEST_P_MAX >= 200, "P_max 必须 >= 200");
	zassert_true(TEST_P_MIN <= 200 && 200 <= TEST_P_MAX, "默认 200 必须落在 [P_min, P_max] 内");
}

/* ============================================================
 * 失效安全：低于下界钳制（含 0<x<P_min 与 <=0）
 * ============================================================ */

/* Verifies REQ-COMM-005: 0<x<P_min 的请求钳制到 P_min（睡眠恒 > 0，不忙等）。
 * Verifies REQ-COMM-007: 钳制结果确定且唯一（可被单测断言）。 */
ZTEST(bms_comm, test_clamp_below_lower_bound)
{
	zassert_equal(bms_comm_clamp_period_ms(1, TEST_P_MIN, TEST_P_MAX), TEST_P_MIN,
		      "1ms (< P_min) 应钳制到 %d", TEST_P_MIN);
	zassert_equal(bms_comm_clamp_period_ms(9, TEST_P_MIN, TEST_P_MAX), TEST_P_MIN,
		      "9ms (< P_min) 应钳制到 %d", TEST_P_MIN);

	/* 确定性：同一输入多次求值结果恒一致且唯一（REQ-COMM-007 可观测前提）。 */
	int32_t r1 = bms_comm_clamp_period_ms(5, TEST_P_MIN, TEST_P_MAX);
	int32_t r2 = bms_comm_clamp_period_ms(5, TEST_P_MIN, TEST_P_MAX);
	zassert_equal(r1, r2, "钳制结果应确定（同输入同输出）");
	zassert_equal(r1, TEST_P_MIN, "结果应唯一确定为 P_min");
	zassert_true(r1 > 0, "钳制结果恒 > 0");
}

/* Verifies REQ-COMM-005: 请求 <=0（0/负数）钳制到 P_min 且结果恒 > 0（不忙等）。 */
ZTEST(bms_comm, test_clamp_zero_or_negative)
{
	zassert_equal(bms_comm_clamp_period_ms(0, TEST_P_MIN, TEST_P_MAX), TEST_P_MIN,
		      "0ms 应钳制到 %d", TEST_P_MIN);
	zassert_equal(bms_comm_clamp_period_ms(-1, TEST_P_MIN, TEST_P_MAX), TEST_P_MIN,
		      "-1ms 应钳制到 %d", TEST_P_MIN);
	zassert_equal(bms_comm_clamp_period_ms(-1000, TEST_P_MIN, TEST_P_MAX), TEST_P_MIN,
		      "-1000ms 应钳制到 %d", TEST_P_MIN);
	zassert_equal(bms_comm_clamp_period_ms(INT32_MIN, TEST_P_MIN, TEST_P_MAX), TEST_P_MIN,
		      "INT32_MIN 应钳制到 %d", TEST_P_MIN);

	/* 失效安全核心不变式：结果恒 > 0，comm 线程睡眠周期不退化为忙等。 */
	zassert_true(bms_comm_clamp_period_ms(0, TEST_P_MIN, TEST_P_MAX) > 0, "0 钳制结果必 > 0");
	zassert_true(bms_comm_clamp_period_ms(-1000, TEST_P_MIN, TEST_P_MAX) > 0,
		     "负值钳制结果必 > 0");
}

/* ============================================================
 * 失效安全：高于上界钳制（远超限）
 * ============================================================ */

/* Verifies REQ-COMM-004: 请求 > P_max 钳制到 P_max。
 * Verifies REQ-COMM-005: 越上界仍落在 [P_min, P_max]，结果 > 0。 */
ZTEST(bms_comm, test_clamp_above_upper_bound)
{
	zassert_equal(bms_comm_clamp_period_ms(60001, TEST_P_MIN, TEST_P_MAX), TEST_P_MAX,
		      "60001ms (> P_max) 应钳制到 %d", TEST_P_MAX);
	zassert_equal(bms_comm_clamp_period_ms(INT32_MAX, TEST_P_MIN, TEST_P_MAX), TEST_P_MAX,
		      "INT32_MAX 应钳制到 %d", TEST_P_MAX);
}

/* ============================================================
 * 失效安全不变式：覆盖样本上 P_min <= ret <= P_max 且 ret > 0
 * ============================================================ */

/* Verifies REQ-COMM-005: 对任意输入，合法化结果恒满足
 * P_min <= ret <= P_max 且 ret > 0（失效安全不变式）。 */
ZTEST(bms_comm, test_clamp_invariant_holds)
{
	const int32_t samples[] = {INT32_MIN,  -1000, -1,  0,          1,     9,
				   TEST_P_MIN, 11,    200, TEST_P_MAX, 60001, INT32_MAX};

	for (size_t i = 0; i < ARRAY_SIZE(samples); i++) {
		int32_t ret = bms_comm_clamp_period_ms(samples[i], TEST_P_MIN, TEST_P_MAX);

		zassert_true(ret >= TEST_P_MIN, "样本 %d: ret(%d) 应 >= P_min(%d)", samples[i], ret,
			     TEST_P_MIN);
		zassert_true(ret <= TEST_P_MAX, "样本 %d: ret(%d) 应 <= P_max(%d)", samples[i], ret,
			     TEST_P_MAX);
		zassert_true(ret > 0, "样本 %d: ret(%d) 应恒 > 0", samples[i], ret);
	}
}
