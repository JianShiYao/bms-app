/* SPDX-License-Identifier: Apache-2.0 */

/*
 * bms_sys_mon 有状态聚合层红灯测试（M5 第 2 片）。
 *
 * 契约来源：
 *  - docs/concept/runtime-model.md §6：sys_mon 聚合评估全部任务健康，写
 *    DB_TASK_HEALTH（owner=sys_mon），判据（心跳超时/运行超时）生成对应 diag 条目。
 *  - docs/concept/data-model.md：DB_TASK_HEALTH owner=bms_sys_mon，内容=任务心跳/
 *    运行时间/超时标志；stale/未写入=健康未知。
 *  - BMS_DIAG_TASK_OVERRUN 已登记 confirm=0 → 一步即 ACTIVE，
 *    st.active_mask & BIT(BMS_DIAG_TASK_OVERRUN) 判是否激活。
 *
 * 为不耦合 coder 的 cfg 默认值（WCET / 心跳阈值），超时/超限一律用**极端值**触发、
 * 健康一律用**极小值**：只要 cfg 取任何合理值，极端输入都必然越阈值、极小输入都必然在阈内。
 *
 * 红灯性质：sys_mon.c 的 step/enter/exit 为空存根、db.c 的 task_health 存根不持久化，
 * 故写库不产生 valid/timestamp/mask、diag 也不会被上报——用例 1/2/3 断言失败；
 * 用例 4（期望无故障）可能因存根全 0 恰好"通过"，coder 补齐后仍应通过。
 */
#include <zephyr/ztest.h>

#include "bms/engine/db.h"
#include "bms/engine/diag.h"
#include "bms/engine/sys_mon.h"

/* 每用例前重置全部有状态层：db / diag / sys_mon。 */
static void reset_before(void *fixture)
{
	ARG_UNUSED(fixture);
	zassert_ok(bms_db_init());
	zassert_ok(bms_diag_init());
	zassert_ok(bms_sys_mon_init());
}

ZTEST_SUITE(bms_sys_mon_agg, NULL, NULL, reset_before, NULL, NULL);

/* BMS_DIAG_TASK_OVERRUN 当前是否激活（读诊断聚合）。 */
static bool task_overrun_active(void)
{
	struct bms_diag_state st;

	zassert_ok(bms_diag_get_state(&st));
	return (st.active_mask & BIT(BMS_DIAG_TASK_OVERRUN)) != 0U;
}

/*
 * 用例 1（runtime-model §6 + data-model DB_TASK_HEALTH）：
 * init 后未 enter 任何任务，step 应发布"健康"快照——meta.valid、timestamp 命中、
 * 两 mask 均 0（无任务运行→不误报）。
 */
ZTEST(bms_sys_mon_agg, test_step_publishes_healthy_when_idle)
{
	const uint32_t t = 5000U;
	struct bms_task_health health;
	struct bms_db_meta meta;

	bms_sys_mon_step(t);

	zassert_ok(bms_db_read_task_health(&health, &meta));
	zassert_true(meta.valid, "step 后 DB_TASK_HEALTH 应已写入（valid）");
	zassert_equal(health.timestamp_ms, t, "快照时刻应为本轮 monitor 时刻");
	zassert_equal(health.heartbeat_timeout_mask, 0U, "无任务运行不得误报心跳超时");
	zassert_equal(health.runtime_overrun_mask, 0U, "无任务运行不得误报运行超时");
}

/*
 * 用例 2（runtime-model §6：运行超时判据）：
 * SAFETY 运行极长时间（1e7 ms，必超任何合理 WCET）→ step 后 runtime_overrun_mask
 * 命中 SAFETY 位，且上报 BMS_DIAG_TASK_OVERRUN 激活。
 */
ZTEST(bms_sys_mon_agg, test_runtime_overrun_sets_mask_and_reports_diag)
{
	const uint32_t t_enter = 100U;
	const uint32_t t_exit = t_enter + 10000000U; /* 运行 1e7 ms，必超合理 WCET */
	const uint32_t t_step = t_exit + 1U;
	struct bms_task_health health;
	struct bms_db_meta meta;

	bms_sys_mon_task_enter(BMS_SYS_MON_SAFETY, t_enter);
	bms_sys_mon_task_exit(BMS_SYS_MON_SAFETY, t_exit);
	bms_sys_mon_step(t_step);

	zassert_ok(bms_db_read_task_health(&health, &meta));
	zassert_true((health.runtime_overrun_mask & BIT(BMS_SYS_MON_SAFETY)) != 0U,
		     "运行超时应置 runtime_overrun_mask 的 SAFETY 位");
	zassert_true(task_overrun_active(), "运行超时应上报 BMS_DIAG_TASK_OVERRUN 激活");
}

/*
 * 用例 3（runtime-model §6：心跳超时判据）：
 * SAFETY enter 后不 exit，step 距上次 enter 极久（1e7 ms，必超任何合理心跳阈值）→
 * heartbeat_timeout_mask 命中 SAFETY 位，且 BMS_DIAG_TASK_OVERRUN 激活。
 */
ZTEST(bms_sys_mon_agg, test_heartbeat_timeout_sets_mask_and_reports_diag)
{
	const uint32_t t_enter = 1000U;
	const uint32_t t_step = t_enter + 10000000U; /* 距上次 enter 1e7 ms，必超心跳阈值 */
	struct bms_task_health health;
	struct bms_db_meta meta;

	bms_sys_mon_task_enter(BMS_SYS_MON_SAFETY, t_enter);
	/* 故意不 exit：持续未再心跳。 */
	bms_sys_mon_step(t_step);

	zassert_ok(bms_db_read_task_health(&health, &meta));
	zassert_true((health.heartbeat_timeout_mask & BIT(BMS_SYS_MON_SAFETY)) != 0U,
		     "心跳超时应置 heartbeat_timeout_mask 的 SAFETY 位");
	zassert_true(task_overrun_active(), "心跳超时应上报 BMS_DIAG_TASK_OVERRUN 激活");
}

/*
 * 用例 4（runtime-model §6：健康任务不误报）：
 * SAFETY 极短运行（1 ms）、step 距 enter 极短（2 ms）→ 两 mask 对 SAFETY 位均 0，
 * 且 BMS_DIAG_TASK_OVERRUN 未激活。
 */
ZTEST(bms_sys_mon_agg, test_healthy_tasks_no_fault)
{
	const uint32_t t_enter = 1000U;
	const uint32_t t_exit = 1001U; /* 运行 1 ms，必在任何合理 WCET 内 */
	const uint32_t t_step = 1002U; /* 距 enter 2 ms，必在任何合理心跳阈值内 */
	struct bms_task_health health;
	struct bms_db_meta meta;

	bms_sys_mon_task_enter(BMS_SYS_MON_SAFETY, t_enter);
	bms_sys_mon_task_exit(BMS_SYS_MON_SAFETY, t_exit);
	bms_sys_mon_step(t_step);

	zassert_ok(bms_db_read_task_health(&health, &meta));
	zassert_equal(health.heartbeat_timeout_mask & BIT(BMS_SYS_MON_SAFETY), 0U,
		      "健康任务不得置心跳超时位");
	zassert_equal(health.runtime_overrun_mask & BIT(BMS_SYS_MON_SAFETY), 0U,
		      "健康任务不得置运行超时位");
	zassert_false(task_overrun_active(), "健康任务不得上报 BMS_DIAG_TASK_OVERRUN");
}
