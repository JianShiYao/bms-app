/*
 * bms_sys_mon 纯 watchdog 喂狗门控判定单元测试
 * （ztest, native_sim / mps2-an386）。
 *
 * 覆盖 bms_sys_mon_wdt_feed_allowed 的失效安全门控语义：
 *  仅当每个安全关键任务都已 seen 且健康（无心跳超时、无运行超限）时才允许喂硬
 *  watchdog；任一安全关键任务失联/超时/从未运行 → 停喂，让 watchdog 复位进上电
 *  安全态。非安全关键任务的健康不参与硬 watchdog 门控（其失联由软 FAULT 处理）。
 *
 * 设计契约：
 *  - docs/concept/runtime-model.md §7（watchdog 门控：仅安全关键任务全部心跳健康才喂狗；
 *    任一失联/超时 → 停喂让狗复位进上电安全态；软先于硬）
 *  - docs/concept/hardware-abstraction.md §2/§7（WDT wrapper 桩化不得放宽安全默认）
 *  - 失效安全细化：安全关键任务必须已 seen 且健康才允许喂；seen=false 亦不喂
 *    （闭合"从未启动"缺口）
 *
 * TDD 红灯说明：本增量下 bms_sys_mon_wdt_feed_allowed 为**乐观存根**（恒返回 true），
 * 真实门控逻辑归 coder。因此期望 false 的用例（2/3/5）断言失败 —— 这是**预期红灯
 * （断言失败型）**，非链接期未定义符号；期望 true 的用例（1/4）恰好通过，coder 补齐
 * 后仍应通过。
 *
 * 时间判定全部以 task_enter/task_exit/wdt_feed_allowed 的 now_ms 入参直接注入，
 * 不依赖内核时钟；超时以极端值（1e6 ms）触发，健康以极小差保持，不依赖具体阈值。
 *
 * 每用例前 bms_sys_mon_init() 清零内部每任务 rt[]（before fixture）。
 *
 * 每个用例注释回链设计契约。
 */
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/ztest.h>

#include "bms/sys_mon.h"

/* 极端超时增量：远超任何 SAFETY/APP 的 WCET 与心跳阈值，稳定触发超时/超限。 */
#define HUGE_MS 1000000u /* 1e6 ms */

/* 每用例前清零内部 rt[]（seen=false、时间归零），保证用例相互隔离。 */
static void wdt_before(void *fixture)
{
	ARG_UNUSED(fixture);
	bms_sys_mon_init();
}

ZTEST_SUITE(bms_sys_mon_wdt, NULL, NULL, wdt_before, NULL, NULL);

/* ============================================================
 * 1. 安全关键任务已 seen 且健康 → 允许喂狗
 * 回链：runtime-model §7（仅安全关键任务全部心跳健康才喂硬 watchdog）
 * ============================================================ */
ZTEST(bms_sys_mon_wdt, test_feed_allowed_when_safety_healthy)
{
	/* SAFETY 刚进出，运行 1ms（远小于 WCET）、心跳新鲜 → 健康。 */
	bms_sys_mon_task_enter(BMS_SYS_MON_SAFETY, 1000);
	bms_sys_mon_task_exit(BMS_SYS_MON_SAFETY, 1001);

	zassert_true(bms_sys_mon_wdt_feed_allowed(1002),
		     "safety task seen and healthy must allow feeding hard watchdog");
}

/* ============================================================
 * 2. 安全关键任务心跳超时 → 停喂
 * 回链：runtime-model §7（安全关键任务失联/超时 → 停止喂狗让狗复位）
 * ============================================================ */
ZTEST(bms_sys_mon_wdt, test_feed_blocked_when_safety_heartbeat_timeout)
{
	/* SAFETY enter 后不再 exit/enter，now 距上次 enter 达 1e6ms → 心跳超时。 */
	bms_sys_mon_task_enter(BMS_SYS_MON_SAFETY, 1000);

	zassert_false(
		bms_sys_mon_wdt_feed_allowed(1000 + HUGE_MS),
		"safety heartbeat timeout must block feeding (fail-safe: let watchdog reset)");
}

/* ============================================================
 * 3. 安全关键任务运行超限 → 停喂
 * 回链：runtime-model §7（运行超限亦属不健康 → 停喂）
 * ============================================================ */
ZTEST(bms_sys_mon_wdt, test_feed_blocked_when_safety_runtime_overrun)
{
	/* SAFETY 单轮运行 1e6ms 远超其 WCET → 运行超限（peak > wcet）。 */
	bms_sys_mon_task_enter(BMS_SYS_MON_SAFETY, 100);
	bms_sys_mon_task_exit(BMS_SYS_MON_SAFETY, 100 + HUGE_MS);

	zassert_false(bms_sys_mon_wdt_feed_allowed(100 + HUGE_MS + 1),
		      "safety runtime overrun must block feeding (fail-safe)");
}

/* ============================================================
 * 4. 仅非安全关键任务(APP)不健康 → 仍允许喂狗
 * 回链：runtime-model §7（硬 watchdog 门控只看安全关键任务；
 *       APP 失联由软 FAULT 处理，不停硬狗）
 * ============================================================ */
ZTEST(bms_sys_mon_wdt, test_feed_allowed_when_only_app_unhealthy)
{
	/* APP 在 t=0 进入后再不打点，到 now=1e6ms 已心跳超时（但 APP 非安全关键）。 */
	bms_sys_mon_task_enter(BMS_SYS_MON_APP, 0);
	/* SAFETY 在 now 时刻刚进出，健康。 */
	bms_sys_mon_task_enter(BMS_SYS_MON_SAFETY, HUGE_MS);
	bms_sys_mon_task_exit(BMS_SYS_MON_SAFETY, HUGE_MS);

	zassert_true(bms_sys_mon_wdt_feed_allowed(HUGE_MS),
		     "only non-safety-critical APP unhealthy must still allow feeding "
		     "(hard watchdog gates on safety-critical tasks only)");
}

/* ============================================================
 * 5. 安全关键任务从未 seen → 停喂（闭合"从未启动"缺口）
 * 回链：失效安全细化（seen=false 亦不喂）/ hardware-abstraction §7（桩不放宽安全默认）
 * ============================================================ */
ZTEST(bms_sys_mon_wdt, test_feed_blocked_before_safety_seen)
{
	/* 仅 init（before fixture 已做），未 enter 任何任务：SAFETY seen=false。 */
	zassert_false(bms_sys_mon_wdt_feed_allowed(500000),
		      "safety task never seen must block feeding (fail-safe: close "
		      "never-started gap)");
}
