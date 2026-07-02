/*
 * bms 主状态机纯函数单元测试（ztest, native_sim / mps2-an386）。
 *
 * 覆盖两个纯函数（无内核依赖，仅链 app/src/bms/application/bms.c）：
 *  - bms_next_state          —— 由当前状态与输入计算下一 BMS 状态
 *  - bms_contactor_for_state —— 由 BMS 状态给出期望接触器状态
 *
 * 安全链回链（architecture.md「安全链优先级」）：
 *   采样有效性 → 诊断聚合 → BMS 状态机 → 接触器输出，不得绕过 fail-safe 默认态。
 *
 * M4（预充时序骨架）TDD 红灯说明：
 *   本增量补齐 STANDBY→PRECHARGE→NORMAL。data-model/architecture §7 规定：
 *   STANDBY 收到合法闭合命令先进 PRECHARGE（执行预充、检查电压爬升与超时），
 *   预充完成方进 NORMAL，超时转 FAULT，命令撤销优雅回退 STANDBY。
 *   bms.h 已 additive 增 struct bms_state_inputs.precharge_complete / precharge_timeout
 *   两位（故本套件可编译），但 coder 尚未在 bms_next_state 加 PRECHARGE 路由。因此：
 *     - test_initialized_clean_reaches_normal 改断言 STANDBY→PRECHARGE：当前仍
 *       STANDBY→NORMAL → 断言失败（断言失败型红灯）。
 *     - 新增 4 个 PRECHARGE 用例（complete→NORMAL、timeout→FAULT、close 撤销→STANDBY、
 *       未完成留 PRECHARGE）：当前 bms.c PRECHARGE 分支尚未按契约路由 → 断言失败型红灯。
 *     - 其余 characterization 用例（latched/CRITICAL/ERROR/prot/INIT/未初始化/接触器映射）
 *       描述既有不变式，pre-coder 应已通过（绿）。
 *   coder 在 bms_next_state 补齐 PRECHARGE 路由后，红灯用例转绿。
 *   tester 不实现路由、不改 bms.c/diag.c 实现、不改 task.c。
 */
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/ztest.h>

#include "bms/application/bms.h"
#include "bms/engine/diag.h"
#include "bms/types.h"

ZTEST_SUITE(bms_state, NULL, NULL, NULL, NULL, NULL);

/*
 * 构造一组"干净且允许闭合、且预充就绪"的输入：无硬件故障、无断开请求、允许闭合，
 * 预充完成且未超时，诊断已初始化且无任何激活/锁存项、严重度 INFO，
 * 保护为 NORMAL 且接触器 CLOSED。
 * 该基线下：STANDBY 应进 PRECHARGE；处于 PRECHARGE 时应进 NORMAL。
 */
static struct bms_state_inputs clean_inputs(void)
{
	return (struct bms_state_inputs){
		.close_allowed = true,
		.precharge_complete = true,
		.precharge_timeout = false,
		.open_request = false,
		.hw_fault_latched = false,
		.diag =
			{
				.active_mask = 0,
				.latched_mask = 0,
				.max_severity = BMS_DIAG_INFO,
				.initialized = true,
			},
		.prot =
			{
				.state = BMS_PROT_NORMAL,
				.contactor = BMS_CONTACTOR_CLOSED,
			},
	};
}

/* ============================================================
 * M3b 核心红灯：诊断未初始化不得进 NORMAL
 * 回链：data-model「未初始化或 stale 不得进 NORMAL」；architecture 安全链失效安全默认态
 * ============================================================ */

/*
 * 干净输入但 diag.initialized=false（诊断中心尚未就绪）。
 * 失效安全语义：下游不得据未就绪诊断闭合接触器，STANDBY 不得进 NORMAL。
 * 断言：结果 != BMS_STATE_NORMAL。
 * 红灯性质：coder 未加门控前，该输入照常 STANDBY→NORMAL → 断言失败（断言失败型红灯）。
 */
ZTEST(bms_state, test_uninitialized_diag_blocks_normal)
{
	/* Verifies data-model: diag 未初始化不得进 NORMAL（失效安全） */
	struct bms_state_inputs in = clean_inputs();

	in.diag.initialized = false;

	zassert_not_equal(bms_next_state(BMS_STATE_STANDBY, &in), BMS_STATE_NORMAL,
			  "uninitialized diag must NOT reach NORMAL (fail-safe)");
}

/* ============================================================
 * 基线：诊断已初始化 + 干净输入，STANDBY 先进 PRECHARGE（不再直达 NORMAL）
 * 回链：architecture §7 状态表——STANDBY 收到合法闭合命令先执行预充
 * ============================================================ */

/* 干净输入（initialized=true，close_allowed=true）：STANDBY 应进 PRECHARGE（不直闭） */
ZTEST(bms_state, test_initialized_clean_reaches_normal)
{
	/* Verifies architecture §7: 合法闭合命令下 STANDBY→PRECHARGE（先预充，不直达 NORMAL） */
	struct bms_state_inputs in = clean_inputs();

	zassert_equal(
		bms_next_state(BMS_STATE_STANDBY, &in), BMS_STATE_PRECHARGE,
		"clean initialized inputs must route STANDBY -> PRECHARGE (not direct NORMAL)");
}

/* ============================================================
 * M4 PRECHARGE 时序：complete→NORMAL / timeout→FAULT / 撤销→STANDBY / 未完成→留 PRECHARGE
 * 回链：architecture §7 状态表——PRECHARGE 执行预充、检查电压爬升与超时
 * ============================================================ */

/* cur=PRECHARGE + 干净（precharge_complete=true）→ NORMAL（预充完成方允许闭合） */
ZTEST(bms_state, test_precharge_complete_reaches_normal)
{
	/* Verifies architecture §7: 预充完成（电压爬升达标）时 PRECHARGE→NORMAL */
	struct bms_state_inputs in = clean_inputs();

	zassert_equal(bms_next_state(BMS_STATE_PRECHARGE, &in), BMS_STATE_NORMAL,
		      "completed precharge must reach NORMAL");
}

/* cur=PRECHARGE + 预充超时（precharge_complete 置 false 隔离）→ FAULT（失效安全） */
ZTEST(bms_state, test_precharge_timeout_forces_fault)
{
	/* Verifies architecture §7: 预充超时转 FAULT（不得闭合） */
	struct bms_state_inputs in = clean_inputs();

	in.precharge_complete = false;
	in.precharge_timeout = true;

	zassert_equal(bms_next_state(BMS_STATE_PRECHARGE, &in), BMS_STATE_FAULT,
		      "precharge timeout must force FAULT");
}

/* cur=PRECHARGE + close_allowed=false（其余 clean）→ STANDBY（命令撤销优雅回退） */
ZTEST(bms_state, test_precharge_abort_on_close_withdrawn)
{
	/* Verifies architecture §7: 预充中撤销闭合命令优雅回退 STANDBY（非故障） */
	struct bms_state_inputs in = clean_inputs();

	in.close_allowed = false;

	zassert_equal(bms_next_state(BMS_STATE_PRECHARGE, &in), BMS_STATE_STANDBY,
		      "withdrawn close command during precharge must fall back to STANDBY");
}

/* cur=PRECHARGE + 未完成未超时（close_allowed=true）→ 留 PRECHARGE（等待电压爬升） */
ZTEST(bms_state, test_precharge_stays_until_complete)
{
	/* Verifies architecture §7: 预充未完成且未超时时留在 PRECHARGE 等待 */
	struct bms_state_inputs in = clean_inputs();

	in.precharge_complete = false;
	in.precharge_timeout = false;

	zassert_equal(bms_next_state(BMS_STATE_PRECHARGE, &in), BMS_STATE_PRECHARGE,
		      "incomplete precharge (no timeout) must stay in PRECHARGE");
}

/* ============================================================
 * 既有不变式（characterization）：pre/post-coder 均应通过
 * 回链：bms.h/bms.c 现状失效安全语义、architecture 安全链
 * ============================================================ */

/* latched_mask != 0 → LOCKED（有锁存故障，最高优先级失效安全） */
ZTEST(bms_state, test_latched_mask_forces_locked)
{
	/* Verifies bms.c: latched_mask 非零一律 LOCKED */
	struct bms_state_inputs in = clean_inputs();

	in.diag.latched_mask = 1U;

	zassert_equal(bms_next_state(BMS_STATE_STANDBY, &in), BMS_STATE_LOCKED,
		      "latched diagnostics must force LOCKED");
}

/* max_severity >= CRITICAL → LOCKED */
ZTEST(bms_state, test_critical_severity_forces_locked)
{
	/* Verifies bms.c: 最高严重度达 CRITICAL 一律 LOCKED */
	struct bms_state_inputs in = clean_inputs();

	in.diag.max_severity = BMS_DIAG_CRITICAL;

	zassert_equal(bms_next_state(BMS_STATE_STANDBY, &in), BMS_STATE_LOCKED,
		      "critical severity must force LOCKED");
}

/* max_severity >= ERROR（非 critical、无 latched）→ FAULT */
ZTEST(bms_state, test_error_severity_forces_fault)
{
	/* Verifies bms.c: 达 ERROR（未到 CRITICAL、无锁存）转 FAULT */
	struct bms_state_inputs in = clean_inputs();

	in.diag.max_severity = BMS_DIAG_ERROR;

	zassert_equal(bms_next_state(BMS_STATE_STANDBY, &in), BMS_STATE_FAULT,
		      "error severity must force FAULT");
}

/* prot.state != NORMAL → FAULT */
ZTEST(bms_state, test_protection_not_normal_forces_fault)
{
	/* Verifies bms.c: 保护非 NORMAL 转 FAULT */
	struct bms_state_inputs in = clean_inputs();

	in.prot.state = BMS_PROT_OV;

	zassert_equal(bms_next_state(BMS_STATE_STANDBY, &in), BMS_STATE_FAULT,
		      "non-normal protection must force FAULT");
}

/* cur=INIT + 干净输入 → STANDBY */
ZTEST(bms_state, test_init_clean_reaches_standby)
{
	/* Verifies bms.c: INIT 干净输入前进到 STANDBY */
	struct bms_state_inputs in = clean_inputs();

	zassert_equal(bms_next_state(BMS_STATE_INIT, &in), BMS_STATE_STANDBY,
		      "INIT with clean inputs must advance to STANDBY");
}

/* 接触器映射：仅 NORMAL 闭合，其余（INIT/STANDBY/FAULT/LOCKED）一律断开（失效安全默认态） */
ZTEST(bms_state, test_contactor_only_closed_in_normal)
{
	/* Verifies architecture 安全链：仅 NORMAL 闭合，其余默认 OPEN */
	zassert_equal(bms_contactor_for_state(BMS_STATE_NORMAL), BMS_CONTACTOR_CLOSED,
		      "NORMAL must map to CLOSED");
	zassert_equal(bms_contactor_for_state(BMS_STATE_INIT), BMS_CONTACTOR_OPEN,
		      "INIT must map to OPEN");
	zassert_equal(bms_contactor_for_state(BMS_STATE_STANDBY), BMS_CONTACTOR_OPEN,
		      "STANDBY must map to OPEN");
	zassert_equal(bms_contactor_for_state(BMS_STATE_FAULT), BMS_CONTACTOR_OPEN,
		      "FAULT must map to OPEN");
	zassert_equal(bms_contactor_for_state(BMS_STATE_LOCKED), BMS_CONTACTOR_OPEN,
		      "LOCKED must map to OPEN");
}
