/*
 * Integration smoke: bms_task 周期编排单步（safety_step）。
 *
 * 目标：把 M2「周期任务改绝对节拍 + 提取可测编排 step」的编排契约钉在链接期与行为层。
 * 被测编排 bms_task_safety_step() 由 coder 实现（本文件为 TDD 红灯：coder 未实现前
 * 应在链接期报 `undefined reference to bms_task_safety_step` —— 预期红灯）。
 *
 * 设计依据：
 *  - docs/concept/runtime-model.md §4（周期到期/绝对节拍）、§9（编排 step 可注入时间、可测）。
 *  - docs/concept/architecture.md 安全链：采样有效性 → 诊断聚合 → BMS 状态机 → 接触器输出，
 *    不得绕过 fail-safe 默认态（接触器默认 OPEN，仅 NORMAL 才 CLOSED）。
 *
 * 说明：初始化链用 init（不起线程）——bms_task_init 语义将改为「仅初始化，不启动线程」，
 * bms_task_start 才起线程，测试从不调用 start，以便单线程 ztest 中同步驱动 safety_step。
 */

/*========== Includes ========================================================*/
#include <stdint.h>
#include <zephyr/ztest.h>

#include "bms/hal/afe.h"
#include "bms/engine/db.h"
#include "bms/engine/diag.h"
#include "bms/measurement-control/protection.h"
#include "bms/engine/task.h"
#include "bms/engine/time.h"
#include "bms/types.h"

/*========== Macros and Definitions ==========================================*/

/*========== Static Constant and Variable Definitions ========================*/
/* 注入时间源：固定/可递增的单调毫秒，脱离内核时钟以确定驱动到期判定。 */
static uint32_t test_now_ms;

/*========== Extern Constant and Variable Definitions ========================*/

/*========== Static Function Prototypes ======================================*/
static uint32_t injected_time_source(void);
static void *task_pipeline_setup(void);
static void task_pipeline_before(void *fixture);
static void task_pipeline_teardown(void *fixture);

/*========== Static Function Implementations =================================*/
static uint32_t injected_time_source(void)
{
	return test_now_ms;
}

static void *task_pipeline_setup(void)
{
	/* 全系统唯一时间源注入（runtime-model §2）：让 bms_time_now_ms() 返回受控值。 */
	test_now_ms = 0U;
	bms_time_set_source(injected_time_source);
	return NULL;
}

static void task_pipeline_before(void *fixture)
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
}

static void task_pipeline_teardown(void *fixture)
{
	ARG_UNUSED(fixture);

	/* 复位为默认内核时间源，避免污染后续测试。 */
	bms_time_set_source(NULL);
}

ZTEST_SUITE(task_pipeline, NULL, task_pipeline_setup, task_pipeline_before, NULL,
	    task_pipeline_teardown);

/*
 * 一轮 safety 编排应贯通整条采样→保护→状态机→写库链路：
 * 采样入库（cell_meas 有效）、保护判定入库（prot）、BMS 状态入库（bms_state）。
 * 依据 runtime-model §9 / architecture 安全链。
 */
ZTEST(task_pipeline, test_safety_step_populates_pipeline)
{
	struct bms_cell_meas meas;
	struct bms_prot_evt prot;
	struct bms_state_snapshot state;
	struct bms_db_meta meta;

	/* 采样周期首帧应到期（next_sample 从 0 起，now=0 视为已到）。 */
	bms_task_safety_step(bms_time_now_ms());

	/* 采样已入库且经校验为有效帧（sim 后端产出合理量程读数）。 */
	zassert_ok(bms_db_read_cell_meas(&meas, &meta), "cell_meas read failed");
	zassert_true(meta.valid, "safety step must populate a valid cell measurement");

	/* 保护判定已入库。 */
	zassert_ok(bms_db_read_prot(&prot, &meta), "prot read failed");

	/* BMS 状态已入库。 */
	zassert_ok(bms_db_read_bms_state(&state, &meta), "bms_state read failed");
}

/*
 * 失效安全不变式：接触器只有在 state==NORMAL 时才允许 CLOSED，否则必须 OPEN。
 * 一轮 step 从 INIT 出发不会立即到 NORMAL，故此处接触器应为 OPEN；
 * 用不变式断言（state==NORMAL || contactor==OPEN），不写死具体枚举以免脆。
 * 依据 CLAUDE.md §3 失效安全 / architecture 安全链末端「接触器输出」。
 */
ZTEST(task_pipeline, test_pipeline_failsafe_contactor)
{
	struct bms_state_snapshot state;
	struct bms_db_meta meta;

	bms_task_safety_step(bms_time_now_ms());

	zassert_ok(bms_db_read_bms_state(&state, &meta), "bms_state read failed");
	zassert_true(meta.valid, "bms_state snapshot must be valid after a safety step");

	zassert_true(state.state == BMS_STATE_NORMAL || state.contactor == BMS_CONTACTOR_OPEN,
		     "fail-safe: contactor must be OPEN unless state is NORMAL");
}

/*
 * 多轮 safety step 应不崩、且状态快照序号随每轮写入前进（编排稳定推进）。
 * 递增注入时间以让后续采样周期继续到期（runtime-model §4 绝对节拍）。
 */
ZTEST(task_pipeline, test_safety_step_advances_sequence)
{
	struct bms_state_snapshot state;
	struct bms_db_meta meta_first;
	struct bms_db_meta meta_later;

	bms_task_safety_step(bms_time_now_ms());
	zassert_ok(bms_db_read_bms_state(&state, &meta_first), "first bms_state read failed");

	for (int i = 0; i < 5; i++) {
		test_now_ms += 100U; /* 越过 AFE 采样周期，驱动后续到期 */
		bms_task_safety_step(bms_time_now_ms());
	}

	zassert_ok(bms_db_read_bms_state(&state, &meta_later), "later bms_state read failed");
	zassert_true(meta_later.sequence > meta_first.sequence,
		     "repeated safety steps must advance the bms_state write sequence");
}

/*========== Extern Function Implementations =================================*/

/*========== Externalized Static Function Implementations (Unit Test) ========*/
