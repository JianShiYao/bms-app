/*
 * Integration (TDD 红灯): 把 bms_sys_mon 接入 task.c，激活任务健康聚合层。
 *
 * 目标：把「被监控任务进入/退出打点 → 周期聚合评估 → 写 DB_TASK_HEALTH →
 * 判据生成 diag → 软先于硬进 FAULT/接触器 OPEN」这条契约钉在行为层。
 * 本增量只写红灯集成测试；接线（task.c 调 bms_sys_mon_init/enter/exit/step）归 coder。
 *
 * 设计依据（权威契约）：
 *  - docs/concept/runtime-model.md §6：被监控任务进入/退出调 enter/exit；
 *    周期聚合评估 → 写 DB_TASK_HEALTH → 判据生成 diag。
 *  - docs/concept/runtime-model.md §7：软先于硬（sys_mon → diag → bms 进 FAULT，
 *    先于停喂狗）。
 *  - docs/concept/architecture.md §6、M5：任务健康核 + 聚合层。
 *
 * coder 将把 task.c 改成：
 *  - bms_task_init() 调 bms_sys_mon_init()；
 *  - bms_task_safety_step(now) = task_enter(SAFETY,now) → 采样 →
 *    bms_sys_mon_step(now) → run_protection_and_bms → task_exit(SAFETY,now)
 *    （step 在状态机之前 → 本拍 TASK_OVERRUN 即驱动状态机）；
 *  - bms_task_app_step(now) 起始/末尾 task_enter/exit(APP,now)。
 *
 * 红灯性质：coder 未接线 → 从不 enter/step → 从不写 DB_TASK_HEALTH、
 * 无 TASK_OVERRUN 掩码/诊断 → 相关断言失败（编译链接通过、断言失败型红灯）。
 *
 * 时间判定用极端超时值（1e6 ms）触发心跳超时，不依赖 SYS_MON_CFG 具体阈值
 * （默认 APP 300ms / SAFETY 30ms）；有符号差回绕由 bms_time_after 保证安全。
 */

/*========== Includes ========================================================*/
#include <stdint.h>
#include <zephyr/sys/util.h> /* BIT() */
#include <zephyr/ztest.h>

#include "bms/hal/afe.h"
#include "bms/engine/db.h"
#include "bms/engine/diag.h"
#include "bms/measurement-control/protection.h"
#include "bms/engine/sys_mon.h"
#include "bms/engine/task.h"
#include "bms/engine/time.h"
#include "bms/types.h"

/*========== Macros and Definitions ==========================================*/

/*========== Static Constant and Variable Definitions ========================*/
/* 注入时间源：受控单调毫秒，脱离内核时钟以确定驱动到期/心跳判定。 */
static uint32_t test_now_ms;

/*========== Extern Constant and Variable Definitions ========================*/

/*========== Static Function Prototypes ======================================*/
static uint32_t injected_time_source(void);
static void *sys_mon_task_setup(void);
static void sys_mon_task_before(void *fixture);
static void sys_mon_task_teardown(void *fixture);

/*========== Static Function Implementations =================================*/
static uint32_t injected_time_source(void)
{
	return test_now_ms;
}

static void *sys_mon_task_setup(void)
{
	/* 全系统唯一时间源注入（runtime-model §2）：让 bms_time_now_ms() 返回受控值。 */
	test_now_ms = 0U;
	bms_time_set_source(injected_time_source);
	return NULL;
}

static void sys_mon_task_before(void *fixture)
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

static void sys_mon_task_teardown(void *fixture)
{
	ARG_UNUSED(fixture);

	/* 复位为默认内核时间源，避免污染后续测试。 */
	bms_time_set_source(NULL);
}

ZTEST_SUITE(sys_mon_task, NULL, sys_mon_task_setup, sys_mon_task_before, NULL,
	    sys_mon_task_teardown);

/*
 * 用例 1：每轮 safety 编排都应把任务健康发布到 DB_TASK_HEALTH。
 * 依据 runtime-model §6：周期聚合评估 → 写 DB_TASK_HEALTH。
 * coder 接线后 safety_step 内会调 bms_sys_mon_step(now)，写入本轮快照。
 * 红：coder 未接线 → 从不 bms_sys_mon_step → 从不写 DB_TASK_HEALTH → valid=false，断言失败。
 */
ZTEST(sys_mon_task, test_task_health_published_each_safety_cycle)
{
	struct bms_task_health h;
	struct bms_db_meta meta;

	bms_task_safety_step(0U);

	/* 最可能失败的 valid/时间戳断言放最前，便于 mps2/an386 首个致命断言即暴露信息。 */
	zassert_ok(bms_db_read_task_health(&h, &meta), "task_health read failed");
	zassert_true(meta.valid, "safety step must publish a valid DB_TASK_HEALTH snapshot");
	zassert_equal(h.timestamp_ms, 0U, "task_health timestamp must equal injected now (0)");
}

/*
 * 用例 2：APP 与 SAFETY 都被步进（健康）时，两 mask 均为 0 且 TASK_OVERRUN 未激活。
 * 依据 runtime-model §6：健康任务不产生超时/超限掩码，判据不生成 diag。
 * app_step / safety_step 各在 now=0 完成一次 enter/exit（运行时间 0，无超限；
 * 距上次 enter 0ms，无心跳超时）。
 * 红：coder 未接线 → 未写 DB_TASK_HEALTH（valid=false）→ 断言失败。
 */
ZTEST(sys_mon_task, test_healthy_when_app_and_safety_stepped)
{
	struct bms_task_health h;
	struct bms_db_meta meta;
	struct bms_diag_state diag;

	/* APP enter/exit@0（健康心跳）；SAFETY enter/exit@0 并触发聚合写库。 */
	bms_task_app_step(0U);
	bms_task_safety_step(0U);

	/* valid 最可能失败，放最前。 */
	zassert_ok(bms_db_read_task_health(&h, &meta), "task_health read failed");
	zassert_true(meta.valid, "task_health must be valid after both tasks stepped");

	/* 健康：无心跳超时、无运行超限。 */
	zassert_equal(h.heartbeat_timeout_mask, 0U,
		      "no heartbeat timeout expected when both tasks freshly stepped");
	zassert_equal(h.runtime_overrun_mask, 0U,
		      "no runtime overrun expected for zero-duration steps");

	/* 判据不生成 TASK_OVERRUN 诊断。 */
	zassert_ok(bms_diag_get_state(&diag), "diag get_state failed");
	zassert_equal(diag.active_mask & BIT(BMS_DIAG_TASK_OVERRUN), 0U,
		      "TASK_OVERRUN must not be active when all tasks are healthy");
}

/*
 * 用例 3（核心）：APP 心跳超时 → TASK_OVERRUN(ERROR) → diag → 状态机 → FAULT/接触器 OPEN。
 * 依据 runtime-model §6（心跳超时判据）+ §7（软先于硬：sys_mon → diag → bms 进 FAULT）
 * 与 architecture 安全链末端接触器输出（失效安全默认 OPEN）。
 *
 * 时序：APP enter@0（bms_task_app_step(0)）；SAFETY 健康步进@0；随后**不再调 app_step**，
 * 仅 safety_step(1e6ms)。距 APP 上次 enter 1e6ms，必超任何合理心跳阈值 → APP 心跳超时。
 * safety_step 内 step 在状态机之前 → 本拍即以 TASK_OVERRUN 驱动 bms_next_state 进 FAULT。
 *
 * 红：coder 未接线 → 无 bms_sys_mon_step → 无 APP 心跳掩码、无 TASK_OVERRUN 诊断
 * → 掩码/诊断两条断言必失败。接触器 OPEN 一条即便未接线也可能恰好成立（初始失效安全态），
 * 故把掩码/诊断断言放最前，确保红灯为断言失败型。
 */
ZTEST(sys_mon_task, test_app_heartbeat_timeout_drives_failsafe)
{
	struct bms_task_health h;
	struct bms_db_meta meta;
	struct bms_diag_state diag;
	struct bms_state_snapshot state;

	/* APP 打一次心跳@0；SAFETY 健康步进@0。 */
	bms_task_app_step(0U);
	bms_task_safety_step(0U);

	/* 跳到 1e6ms 只步进 SAFETY：APP 从此失联，距上次 enter 已达 1e6ms。 */
	test_now_ms = 1000000U;
	bms_task_safety_step(1000000U);

	/* 核心断言（最可能失败）放最前：APP 位心跳超时掩码非 0。 */
	zassert_ok(bms_db_read_task_health(&h, &meta), "task_health read failed");
	zassert_true(meta.valid, "task_health must be valid after safety step");
	zassert_not_equal(h.heartbeat_timeout_mask & BIT(BMS_SYS_MON_APP), 0U,
			  "APP heartbeat timeout must be latched in DB_TASK_HEALTH mask");

	/* 判据生成 TASK_OVERRUN 诊断（软路径）。 */
	zassert_ok(bms_diag_get_state(&diag), "diag get_state failed");
	zassert_not_equal(diag.active_mask & BIT(BMS_DIAG_TASK_OVERRUN), 0U,
			  "APP heartbeat timeout must activate BMS_DIAG_TASK_OVERRUN");

	/* 失效安全（软先于硬）：状态机据此打开接触器。 */
	zassert_ok(bms_db_read_bms_state(&state, &meta), "bms_state read failed");
	zassert_equal(state.contactor, BMS_CONTACTOR_OPEN,
		      "fail-safe: contactor must OPEN on TASK_OVERRUN");
}

/*========== Extern Function Implementations =================================*/

/*========== Externalized Static Function Implementations (Unit Test) ========*/
