/*
 * Integration (TDD 红灯): 把 M5 喂狗门控（bms_sys_mon_wdt_feed_allowed）接到硬
 * watchdog（hal/wdt），激活 bms_task_wdt_step 喂狗接线。
 *
 * 目标：把「仅当所有安全关键任务健康才喂硬 watchdog；任一失联/超时 → 停喂 →
 * 复位（软先于硬）」这条契约钉在行为层。本增量只写红灯集成测试；接线
 * （task.c 的 bms_task_wdt_step 调 bms_sys_mon_wdt_feed_allowed + bms_wdt_feed、
 * wdt_stub 真实记数）归 coder。
 *
 * 设计依据（权威契约）：
 *  - docs/concept/runtime-model.md §7：仅当所有安全关键任务健康才喂硬 watchdog；
 *    任一失联/超时 → 停喂 → 复位（软先于硬）。
 *  - bms/engine/sys_mon.h：bms_sys_mon_wdt_feed_allowed(now) —— 仅安全关键任务
 *    (SAFETY) 已 seen 且健康才 true；SAFETY 心跳阈值 30ms。
 *
 * coder 将把 task.c 改成：
 *  - bms_task_wdt_step(now) = if (bms_sys_mon_wdt_feed_allowed(now)) bms_wdt_feed();
 *  - bms_task_init() 调 bms_wdt_init()；
 *  并让 wdt_stub 的 bms_wdt_feed 递增静态计数、bms_wdt_stub_feed_count 返回之。
 *
 * 红灯性质：存根 bms_task_wdt_step 为空 + wdt_stub 的 bms_wdt_feed 不记数 +
 * bms_wdt_stub_feed_count 恒 0 → 期望「喂一次（+1）」的用例断言失败（断言失败型红灯）；
 * 期望「不喂（计数不变）」的用例因恒 0 恰好通过。故至少用例 1 为断言失败型红灯。
 *
 * 时间判定用极端超时值（1e6 ms）触发 SAFETY 心跳超时，不写死 SYS_MON_CFG 阈值；
 * 有符号差回绕由 bms_time_after 保证安全。计数用「调用前后差」判定，避免依赖
 * stub 静态计数在用例间的绝对值残留。
 */

/*========== Includes ========================================================*/
#include <stdint.h>
#include <zephyr/ztest.h>

#include "bms/hal/afe.h"
#include "bms/engine/db.h"
#include "bms/engine/diag.h"
#include "bms/measurement-control/protection.h"
#include "bms/engine/sys_mon.h"
#include "bms/engine/task.h"
#include "bms/engine/time.h"
#include "bms/hal/wdt.h"

/*========== Macros and Definitions ==========================================*/

/*========== Static Constant and Variable Definitions ========================*/
/* 注入时间源：受控单调毫秒，脱离内核时钟以确定驱动心跳判定。 */
static uint32_t test_now_ms;

/*========== Extern Constant and Variable Definitions ========================*/

/*========== Static Function Prototypes ======================================*/
static uint32_t injected_time_source(void);
static void *wdt_feed_setup(void);
static void wdt_feed_before(void *fixture);
static void wdt_feed_teardown(void *fixture);

/*========== Static Function Implementations =================================*/
static uint32_t injected_time_source(void)
{
	return test_now_ms;
}

static void *wdt_feed_setup(void)
{
	/* 全系统唯一时间源注入（runtime-model §2）：让 bms_time_now_ms() 返回受控值。 */
	test_now_ms = 0U;
	bms_time_set_source(injected_time_source);
	return NULL;
}

static void wdt_feed_before(void *fixture)
{
	ARG_UNUSED(fixture);

	/* 每个用例前重置注入时间与整条 engine 链，保证用例间无残留状态。 */
	test_now_ms = 0U;

	/* 初始化链：db → diag → afe → protection → task（用 init，不起线程）。 */
	zassert_ok(bms_db_init(), "db init failed");
	zassert_ok(bms_diag_init(), "diag init failed");
	zassert_ok(bms_afe_init(), "afe init failed");
	zassert_ok(bms_protection_init(), "protection init failed");
	zassert_ok(bms_task_init(), "task init (no-thread) failed");

	/* 显式再初始化 sys_mon 聚合层保险（清零每任务 rt[]，seen=false 不误报）。
	 * 契约上 bms_task_init 已应调用 bms_sys_mon_init（coder 接线），此处二次调用
	 * 幂等，确保用例间 rt[] 归零，不残留上一用例的 last_enter_ms / seen。 */
	zassert_ok(bms_sys_mon_init(), "sys_mon init failed");
}

static void wdt_feed_teardown(void *fixture)
{
	ARG_UNUSED(fixture);

	/* 复位为默认内核时间源，避免污染后续测试。 */
	bms_time_set_source(NULL);
}

ZTEST_SUITE(wdt_feed, NULL, wdt_feed_setup, wdt_feed_before, NULL, wdt_feed_teardown);

/*
 * 用例 1（核心，断言失败型红灯）：安全关键任务健康时，喂狗一次。
 * 依据 runtime-model §7 + sys_mon.h（SAFETY 已 seen 且健康 → wdt_feed_allowed=true）。
 * 时序：bms_task_safety_step(0) 内 SAFETY enter@0（健康心跳）；随后 bms_task_wdt_step(0)
 * 应调 bms_wdt_feed 恰一次。以「调用前后计数差 == 1」判定，避免依赖 stub 绝对计数。
 * 红：存根 wdt_step 为空 + feed 不记数 + stub_feed_count 恒 0 → 差为 0 ≠ 1，断言失败。
 */
ZTEST(wdt_feed, test_wdt_step_feeds_when_healthy)
{
	uint32_t c0;

	/* SAFETY enter@0（健康）。 */
	bms_task_safety_step(0U);

	c0 = bms_wdt_stub_feed_count();
	bms_task_wdt_step(0U);

	zassert_equal(bms_wdt_stub_feed_count(), c0 + 1U,
		      "healthy safety task must feed the watchdog exactly once");
}

/*
 * 用例 2：安全关键任务心跳超时（失联）时，停喂。
 * 依据 runtime-model §7：任一安全关键任务失联/超时 → 停喂（失效安全）。
 * 时序：bms_task_safety_step(0) 内 SAFETY enter@0；随后 bms_task_wdt_step(1e6ms)：
 * 距 SAFETY 上次 enter 已 1e6ms，必超心跳阈值 → wdt_feed_allowed=false → 不喂。
 * 红灯说明：存根恒 0 → 计数不变，此断言恰好通过（非本套件红灯来源）。
 */
ZTEST(wdt_feed, test_wdt_step_blocks_when_safety_unhealthy)
{
	uint32_t c0;

	/* SAFETY enter@0。 */
	bms_task_safety_step(0U);

	c0 = bms_wdt_stub_feed_count();

	/* 跳到 1e6ms：距 SAFETY 上次 enter 1e6ms → 心跳超时 → 门控 false。 */
	test_now_ms = 1000000U;
	bms_task_wdt_step(1000000U);

	zassert_equal(bms_wdt_stub_feed_count(), c0,
		      "unhealthy (heartbeat-timeout) safety task must NOT feed the watchdog");
}

/*
 * 用例 3：安全关键任务从未 seen（开机未运行）时，失效安全不喂。
 * 依据 sys_mon.h：未运行过的安全关键任务（seen=false）按失效安全**不允许**喂。
 * 时序：仅初始化（before 已 bms_sys_mon_init），**不** safety_step（SAFETY 从未 enter）；
 * bms_task_wdt_step(5e5ms) 应因门控 false 而不喂。
 * 红灯说明：存根恒 0 → 计数不变，此断言恰好通过（非本套件红灯来源）。
 */
ZTEST(wdt_feed, test_wdt_step_blocks_before_safety_seen)
{
	uint32_t c0;

	/* 不调 safety_step：SAFETY 从未 seen。 */
	c0 = bms_wdt_stub_feed_count();

	test_now_ms = 500000U;
	bms_task_wdt_step(500000U);

	zassert_equal(bms_wdt_stub_feed_count(), c0,
		      "never-seen safety task must NOT feed the watchdog (fail-safe)");
}

/*========== Extern Function Implementations =================================*/

/*========== Externalized Static Function Implementations (Unit Test) ========*/
