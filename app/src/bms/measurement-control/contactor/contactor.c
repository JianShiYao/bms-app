/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    contactor.c
 * @brief   接触器抽象实现：执行期望态→读反馈→写 DB_CONTACTOR_FB→诊断不一致。
 * @ingroup CONT
 *
 * @details 落实 architecture.md §7（执行 bms_bms 期望态并反馈）/§8（反馈不一致诊断）。
 *          eval 为纯判定；step 为一步编排（经 hal/contactor_io seam 执行/回读，写
 *          DB_CONTACTOR_FB，按 eval 上报 BMS_DIAG_CONTACTOR_MISMATCH，去抖/锁存由
 *          diag 生命周期负责）。本片不接 task.c（Phase 1-③b 再接）。
 */

/*========== Includes ========================================================*/
#include "bms/measurement-control/contactor.h"
#include "bms/hal/contactor_io.h"
#include "bms/engine/db.h"
#include "bms/engine/diag.h"

/*========== Macros and Definitions ==========================================*/

/*========== Static Constant and Variable Definitions ========================*/

/*========== Extern Constant and Variable Definitions ========================*/

/*========== Static Function Prototypes ======================================*/

/*========== Static Function Implementations =================================*/

/*========== Extern Function Implementations =================================*/
bool bms_contactor_eval(enum bms_contactor desired, enum bms_contactor actual)
{
	return actual != desired;
}

void bms_contactor_step(enum bms_contactor desired, uint32_t now_ms)
{
	struct bms_contactor_fb fb = {0};

	/* 执行期望态 → 回读反馈 → 盖时间戳 → 发布 DB_CONTACTOR_FB。 */
	bms_contactor_io_apply(desired);
	(void)bms_contactor_io_read(&fb);
	fb.timestamp_ms = now_ms;
	(void)bms_db_write_contactor_fb(&fb);

	/* 反馈与期望不一致 → 上报（CRITICAL + 去抖 + 锁存，粘连/拒动失效安全）。 */
	(void)bms_diag_report(BMS_DIAG_CONTACTOR_MISMATCH, bms_contactor_eval(desired, fb.actual),
			      now_ms);
}

/*========== Externalized Static Function Implementations (Unit Test) ========*/
