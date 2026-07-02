/*
 * bms_contactor 接触器抽象单元测试（ztest, native_sim / mps2-an386）。
 *
 * 覆盖：
 *  - bms_contactor_eval —— 纯判定：反馈 actual 是否与期望 desired 不一致。
 *  - bms_contactor_step —— 一步：执行期望态→读反馈→写 DB_CONTACTOR_FB→按 eval
 *    上报 BMS_DIAG_CONTACTOR_MISMATCH（去抖 confirm 200ms + 锁存 latch + CRITICAL）。
 *
 * 设计契约（design-first，测试向契约对齐，不从代码反推）：
 *  - docs/concept/architecture.md §7：bms_contactor 执行 bms_bms 期望接触器态并反馈实测。
 *  - docs/concept/architecture.md §8：bms_contactor 诊断＝反馈不一致 / 预充超时 / 粘连失败。
 *  - docs/concept/data-model.md：DB_CONTACTOR_FB（owner=bms_contactor，消费者 bms/diag）；
 *    stale 视为诊断故障。
 *  - 失效安全（CLAUDE.md §3）：接触器默认 OPEN。
 *
 * TDD 红灯说明（本增量只声明 + 最小存根，不实现真实逻辑）：
 *  - contactor.c 存根 eval 恒 false、step 空；contactor_io_fake.c 不回显/不注入；
 *    db.c 接触器槽位不持久化；diag.c 的 DIAG_CFG 尚未登记 CONTACTOR_MISMATCH。
 *  - 所有符号已声明并链接 → 预期为**断言失败型红灯**（编译链接通过、断言失败），
 *    非链接未定义符号错。coder 补齐真实逻辑与 DIAG_CFG 登记后转绿。
 *
 * 每个用例注释回链设计契约。
 */
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/ztest.h>

#include "bms/contactor.h"
#include "bms/contactor_io.h"
#include "bms/db.h"
#include "bms/diag.h"
#include "bms/types.h"

/* 每例前重置引擎核心（db + diag），保证用例独立、无残留锁存。 */
static void contactor_before(void *fixture)
{
	ARG_UNUSED(fixture);
	zassert_ok(bms_db_init());
	zassert_ok(bms_diag_init());
}

ZTEST_SUITE(bms_contactor, NULL, NULL, contactor_before, NULL, NULL);

/* ============================================================
 * 1. eval 纯函数：一致 → 不上报
 * 回链：architecture.md §8（反馈不一致才诊断）
 * ============================================================ */
ZTEST(bms_contactor, test_eval_match_no_mismatch)
{
	zassert_false(bms_contactor_eval(BMS_CONTACTOR_OPEN, BMS_CONTACTOR_OPEN),
		      "OPEN 期望 + OPEN 实测 应无不一致");
	zassert_false(bms_contactor_eval(BMS_CONTACTOR_CLOSED, BMS_CONTACTOR_CLOSED),
		      "CLOSED 期望 + CLOSED 实测 应无不一致");
}

/* ============================================================
 * 2. eval 纯函数：不一致 → 判定 true（含拒动/粘连两向）
 * 回链：architecture.md §8（反馈不一致 = 诊断触发）
 * ============================================================ */
ZTEST(bms_contactor, test_eval_mismatch)
{
	zassert_true(bms_contactor_eval(BMS_CONTACTOR_CLOSED, BMS_CONTACTOR_OPEN),
		     "期望 CLOSED 实测 OPEN（拒动）应不一致");
	zassert_true(bms_contactor_eval(BMS_CONTACTOR_OPEN, BMS_CONTACTOR_CLOSED),
		     "期望 OPEN 实测 CLOSED（粘连）应不一致");
}

/* ============================================================
 * 3. step 健康路径：反馈一致 → 发布 DB_CONTACTOR_FB、不报诊断
 * 回链：architecture.md §7（执行期望态并反馈）、data-model.md（DB_CONTACTOR_FB owner）
 * ============================================================ */
ZTEST(bms_contactor, test_step_healthy_publishes_fb_no_diag)
{
	const uint32_t T = 5000U;
	struct bms_contactor_fb fb;
	struct bms_db_meta meta;
	struct bms_diag_state diag;

	/* fake 令实测回 CLOSED，期望也 CLOSED → 一致 */
	bms_contactor_io_fake_set(BMS_CONTACTOR_CLOSED);
	bms_contactor_step(BMS_CONTACTOR_CLOSED, T);

	/* DB_CONTACTOR_FB 应被发布：valid、actual==CLOSED、时间戳==T */
	zassert_ok(bms_db_read_contactor_fb(&fb, &meta));
	zassert_true(meta.valid, "step 后 DB_CONTACTOR_FB 应有效");
	zassert_equal(fb.actual, BMS_CONTACTOR_CLOSED, "反馈 actual 应为 CLOSED，实测 %d",
		      fb.actual);
	zassert_equal(fb.timestamp_ms, T, "反馈时间戳应为注入时间 %u，实测 %u", T, fb.timestamp_ms);

	/* 一致 → CONTACTOR_MISMATCH 不应激活 */
	zassert_ok(bms_diag_get_state(&diag));
	zassert_false(diag.active_mask & BIT(BMS_DIAG_CONTACTOR_MISMATCH),
		      "一致反馈不应激活 CONTACTOR_MISMATCH，active_mask=0x%08x", diag.active_mask);
}

/* ============================================================
 * 4. step 不一致（核心）：去抖 confirm(200ms) → 越过后 ACTIVE + LATCHED + CRITICAL
 * 回链：architecture.md §8（粘连检测失败/反馈不一致，安全相关）、
 *       data-model.md（DB_CONTACTOR_FB 反映实测 actual）、
 *       diagnostics-fault-model §4/§5（去抖 + 锁存）。
 * 说明：coder 将把 DIAG_CFG[CONTACTOR_MISMATCH] 设为
 *       {severity=CRITICAL, confirm_time_ms=200, clear_time_ms=0, latch=true}。
 * ============================================================ */
ZTEST(bms_contactor, test_step_mismatch_confirms_critical_latched)
{
	const uint32_t T0 = 1000U;
	const uint32_t CONFIRM_MS = 200U; /* 与 coder 拟登记的 confirm_time_ms 对齐 */
	struct bms_contactor_fb fb;
	struct bms_diag_state diag;

	/* fake 令实测回 OPEN，期望 CLOSED → 不一致（拒动） */
	bms_contactor_io_fake_set(BMS_CONTACTOR_OPEN);

	/* 第一步：进入 CONFIRMING，去抖尚未越过 → 此刻不应 active */
	bms_contactor_step(BMS_CONTACTOR_CLOSED, T0);
	zassert_ok(bms_diag_get_state(&diag));
	zassert_false(diag.active_mask & BIT(BMS_DIAG_CONTACTOR_MISMATCH),
		      "去抖窗内 CONTACTOR_MISMATCH 不应激活，active_mask=0x%08x", diag.active_mask);

	/* 第二步：越过 confirm_time（+250ms > 200ms）→ 应 ACTIVE、CRITICAL。
	 * 注：raw 仍为真时条目处于 ACTIVE（进 active_mask、贡献 max_severity）；
	 * latched_mask 仅含 LATCHED 态（须 raw 清除后才转），故此处不断 latched（见第三步）。 */
	bms_contactor_step(BMS_CONTACTOR_CLOSED, T0 + CONFIRM_MS + 50U);
	zassert_ok(bms_diag_get_state(&diag));
	zassert_true(diag.active_mask & BIT(BMS_DIAG_CONTACTOR_MISMATCH),
		     "越过去抖后 CONTACTOR_MISMATCH 应激活，active_mask=0x%08x", diag.active_mask);
	zassert_equal(diag.max_severity, BMS_DIAG_CRITICAL,
		      "接触器不一致应达 CRITICAL 严重度，实测 %d", diag.max_severity);

	/* DB_CONTACTOR_FB.actual 应反映实测 OPEN */
	zassert_ok(bms_db_read_contactor_fb(&fb, NULL));
	zassert_equal(fb.actual, BMS_CONTACTOR_OPEN, "反馈 actual 应为实测 OPEN，实测 %d",
		      fb.actual);

	/* 第三步：不一致消失（实测恢复 CLOSED）后，latch=true 应转 LATCHED 并保持
	 * （clear_time=0 一步即锁存）；锁存后即便一致仍在 latched_mask，驱动 LOCKED。 */
	bms_contactor_io_fake_set(BMS_CONTACTOR_CLOSED);
	bms_contactor_step(BMS_CONTACTOR_CLOSED, T0 + CONFIRM_MS + 100U);
	zassert_ok(bms_diag_get_state(&diag));
	zassert_true(diag.latched_mask & BIT(BMS_DIAG_CONTACTOR_MISMATCH),
		     "清除后 CONTACTOR_MISMATCH 应锁存保持，latched_mask=0x%08x",
		     diag.latched_mask);
}
