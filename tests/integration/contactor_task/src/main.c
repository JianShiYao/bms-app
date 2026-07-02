/*
 * Integration (TDD 红灯): 把 bms_contactor 接入 task.c，激活接触器闭环。
 *
 * 目标：把「每 safety 拍执行期望接触器态 → 读反馈 → 写 DB_CONTACTOR_FB →
 * 反馈不一致（粘连/拒动）经 200ms 去抖 → CONTACTOR_MISMATCH(CRITICAL+latch) →
 * diag → 下一拍状态机进 LOCKED、接触器 OPEN（失效安全）」这条契约钉在行为层。
 * 本增量只写红灯集成测试；接线（run_protection_and_bms 内调 bms_contactor_step）归 coder。
 *
 * 设计依据（权威契约）：
 *  - docs/concept/architecture.md §7：bms_contactor 执行 bms_bms 期望接触器态并反馈
 *    实测；安全链末端「接触器输出」。
 *  - docs/concept/architecture.md §8：接触器反馈不一致（粘连/拒动）→ 诊断。
 *  - 失效安全（CLAUDE.md §3）：接触器默认 OPEN，仅 NORMAL 才 CLOSED。
 *
 * coder 将把 task.c 的 run_protection_and_bms 改成：在 bms_db_write_bms_state(&state)
 * 之后调 bms_contactor_step(state.contactor, now_ms)（执行期望态→读反馈→写
 * DB_CONTACTOR_FB→上报 BMS_DIAG_CONTACTOR_MISMATCH）。
 *
 * sim fake 语义：bms_contactor_io_fake_set(actual) 强制实测态（此后 apply 不回显）；
 * 未强制时 apply 回显期望态（无不一致）。
 *
 * 红灯性质：coder 未接线 → 从不 contactor_step → 从不写 DB_CONTACTOR_FB、
 * 无 CONTACTOR_MISMATCH、不进 LOCKED → 相关断言失败（编译链接通过、断言失败型红灯）。
 * 符号（bms_contactor_step / contactor_io_fake / db_read_contactor_fb）均已存在，非链接错。
 */
#include <stdint.h>
#include <zephyr/sys/util.h> /* BIT() */
#include <zephyr/ztest.h>

#include "bms/afe.h"
#include "bms/contactor_io.h"
#include "bms/db.h"
#include "bms/diag.h"
#include "bms/protection.h"
#include "bms/sys_mon.h"
#include "bms/task.h"
#include "bms/time.h"
#include "bms/types.h"

/* 注入时间源：受控单调毫秒，脱离内核时钟以确定驱动去抖/锁存与反馈时间戳。 */
static uint32_t test_now_ms;

static uint32_t injected_time_source(void)
{
	return test_now_ms;
}

static void *contactor_task_setup(void)
{
	/* 全系统唯一时间源注入（runtime-model §2）：让 bms_time_now_ms() 返回受控值。 */
	test_now_ms = 0U;
	bms_time_set_source(injected_time_source);
	return NULL;
}

static void contactor_task_before(void *fixture)
{
	ARG_UNUSED(fixture);

	/* 每个用例前重置注入时间与整条 engine 链，保证用例间无残留状态。 */
	test_now_ms = 0U;

	/* 初始化链：db → diag → afe → protection → task（用 init，不起线程）。
	 * task_init 契约上应初始化 sys_mon（已接入）；下方二次 sys_mon_init 幂等保险。 */
	zassert_ok(bms_db_init(), "db init failed");
	zassert_ok(bms_diag_init(), "diag init failed");
	zassert_ok(bms_afe_init(), "afe init failed");
	zassert_ok(bms_protection_init(), "protection init failed");
	zassert_ok(bms_task_init(), "task init (no-thread) failed");
	zassert_ok(bms_sys_mon_init(), "sys_mon init failed");
}

static void contactor_task_teardown(void *fixture)
{
	ARG_UNUSED(fixture);

	/* 复位为默认内核时间源，避免污染后续测试。 */
	bms_time_set_source(NULL);
}

ZTEST_SUITE(contactor_task, NULL, contactor_task_setup, contactor_task_before, NULL,
	    contactor_task_teardown);

/*
 * 用例 1：每轮 safety 编排都应把接触器反馈发布到 DB_CONTACTOR_FB。
 * 依据 architecture §7：bms_contactor 执行期望态并反馈实测；安全链末端接触器输出。
 * coder 接线后 safety_step 内（bms_db_write_bms_state 之后）会调
 * bms_contactor_step(state.contactor, now_ms)，写入本拍反馈快照。
 *
 * 未强制 fake actual → apply 回显期望态 → fb.actual 应等于本拍 bms_state.contactor
 * （desired）。读 bms_state 取本拍 desired，与反馈 actual 比对。
 *
 * 红：coder 未接线 → 从不 contactor_step → 从不写 DB_CONTACTOR_FB → valid=false，断言失败。
 */
ZTEST(contactor_task, test_safety_step_publishes_contactor_fb)
{
	struct bms_contactor_fb fb;
	struct bms_state_snapshot state;
	struct bms_db_meta fb_meta;
	struct bms_db_meta state_meta;

	bms_task_safety_step(0U);

	/* 最可能失败的 valid/时间戳断言放最前，便于 mps2/an386 首个致命断言即暴露信息。 */
	zassert_ok(bms_db_read_contactor_fb(&fb, &fb_meta), "contactor_fb read failed");
	zassert_true(fb_meta.valid, "safety step must publish a valid DB_CONTACTOR_FB snapshot");
	zassert_equal(fb.timestamp_ms, 0U, "contactor_fb timestamp must equal injected now (0)");

	/* 本拍 bms_state.contactor 为期望态；fake 未强制 → apply 回显 → actual==desired。 */
	zassert_ok(bms_db_read_bms_state(&state, &state_meta), "bms_state read failed");
	zassert_true(state_meta.valid, "safety step must publish a valid DB_BMS_STATE snapshot");
	zassert_equal(
		fb.actual, state.contactor,
		"unforced fake echoes desired: contactor_fb.actual must equal bms_state.contactor");
}

/*
 * 用例 2（核心）：接触器粘连 → CONTACTOR_MISMATCH(CRITICAL+latch) → 下一拍状态机 → LOCKED、接触器
 * OPEN。 依据 architecture §7/§8（反馈不一致诊断）+ CLAUDE.md §3（失效安全默认 OPEN）
 * + diag 登记表（CONTACTOR_MISMATCH: confirm_time_ms=200, CRITICAL, latch=true）。
 *
 * 粘连造法：bms_contactor_io_fake_set(BMS_CONTACTOR_CLOSED) 强制实测 CLOSED；
 * 而 INIT/STANDBY 期望 OPEN → 每拍 eval 得不一致（actual CLOSED != desired OPEN）。
 *
 * 时序：
 *  - safety_step(1000)：首个不一致 raw_active → 起 200ms 去抖计时（尚未确认 ACTIVE）。
 *  - safety_step(1000+250)：距首报 250ms > 200ms 去抖 → CONTACTOR_MISMATCH 确认 ACTIVE(CRITICAL)。
 *  - safety_step(1000+300)：再步一拍让状态机消化——CRITICAL≥CRITICAL，bms_next_state 顶层判定
 * LOCKED。
 *
 * 断言：CONTACTOR_MISMATCH 在 active_mask 或 latched_mask（latch=true 故锁存必置）；
 * bms_state.state == LOCKED；contactor == OPEN（失效安全）。
 *
 * 红：coder 未接线 → 无 contactor_step → 无 CONTACTOR_MISMATCH → 不进 LOCKED，断言失败。
 * 掩码断言最可能失败（接触器 OPEN 即便未接线也可能恰好成立于初始失效安全态），故放最前。
 */
ZTEST(contactor_task, test_welded_contactor_drives_locked)
{
	struct bms_diag_state diag;
	struct bms_state_snapshot state;
	struct bms_db_meta meta;

	/* 强制实测 CLOSED（粘连）：与 INIT/STANDBY 期望 OPEN 恒不一致。 */
	bms_contactor_io_fake_set(BMS_CONTACTOR_CLOSED);

	/* 首拍：不一致 raw_active 起 200ms 去抖。 */
	test_now_ms = 1000U;
	bms_task_safety_step(1000U);

	/* 越 200ms 去抖：CONTACTOR_MISMATCH 确认为 ACTIVE(CRITICAL)。 */
	test_now_ms = 1000U + 250U;
	bms_task_safety_step(1000U + 250U);

	/* 再步一拍让状态机消化：CRITICAL → bms_next_state 顶层判定 LOCKED。 */
	test_now_ms = 1000U + 300U;
	bms_task_safety_step(1000U + 300U);

	/* 核心断言（最可能失败）放最前：CONTACTOR_MISMATCH 已激活/锁存。 */
	zassert_ok(bms_diag_get_state(&diag), "diag get_state failed");
	zassert_not_equal(
		(diag.active_mask | diag.latched_mask) & BIT(BMS_DIAG_CONTACTOR_MISMATCH), 0U,
		"welded contactor must activate/latch BMS_DIAG_CONTACTOR_MISMATCH after debounce");

	/* CRITICAL 锁存 → 下一拍状态机进 LOCKED。 */
	zassert_ok(bms_db_read_bms_state(&state, &meta), "bms_state read failed");
	zassert_true(meta.valid, "bms_state must be valid after safety steps");
	zassert_equal(state.state, BMS_STATE_LOCKED,
		      "CRITICAL CONTACTOR_MISMATCH must drive state machine to LOCKED");

	/* 失效安全：LOCKED 下接触器期望 OPEN。 */
	zassert_equal(state.contactor, BMS_CONTACTOR_OPEN,
		      "fail-safe: contactor must be OPEN in LOCKED");
}
