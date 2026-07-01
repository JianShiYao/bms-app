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
 * M3b（initialized 门控）TDD 红灯说明：
 *   本增量为 data-model「未初始化或 stale 不得进 NORMAL」新增字段 + 红灯测试。
 *   diag.h 已增 struct bms_diag_state.initialized 字段（故本套件可编译），
 *   但 coder 尚未在 bms_next_state 加门控。因此：
 *     - test_uninitialized_diag_blocks_normal 为**断言失败型红灯**：
 *       diag.initialized=false 的干净输入当前仍会 STANDBY→NORMAL，断言 != NORMAL 失败。
 *     - test_initialized_clean_reaches_normal 与其余 characterization 用例
 *       描述既有不变式，pre-coder 应已通过（绿）。
 *   coder 在 bms_next_state 补齐「diag 未初始化按失效安全处理（返回 FAULT，不得进 NORMAL）」
 *   门控后，红灯用例转绿。tester 不实现门控、不改 bms.c/diag.c 实现。
 */
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/ztest.h>

#include "bms/bms.h"
#include "bms/diag.h"
#include "bms/types.h"

ZTEST_SUITE(bms_state, NULL, NULL, NULL, NULL, NULL);

/*
 * 构造一组"干净且允许闭合"的输入：无硬件故障、无断开请求、允许闭合，
 * 诊断已初始化且无任何激活/锁存项、严重度 INFO，保护为 NORMAL 且接触器 CLOSED。
 * 该输入在无额外门控时应允许 STANDBY → NORMAL。
 */
static struct bms_state_inputs clean_inputs(void)
{
	return (struct bms_state_inputs){
		.close_allowed = true,
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
 * 基线：诊断已初始化 + 干净输入应可达 NORMAL
 * 回链：architecture 安全链——所有前置条件满足方可闭合
 * ============================================================ */

/* 干净输入（initialized=true）：STANDBY 应进 NORMAL（此为门控放行的正路径基线） */
ZTEST(bms_state, test_initialized_clean_reaches_normal)
{
	/* Verifies architecture: 前置全满足（含 diag 已初始化）时 STANDBY→NORMAL */
	struct bms_state_inputs in = clean_inputs();

	zassert_equal(bms_next_state(BMS_STATE_STANDBY, &in), BMS_STATE_NORMAL,
		      "clean initialized inputs must allow STANDBY -> NORMAL");
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
