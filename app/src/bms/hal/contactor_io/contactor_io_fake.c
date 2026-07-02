/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    contactor_io_fake.c
 * @brief   接触器 IO 的 fake backend（单测/仿真用）。
 * @ingroup CONT
 *
 * @details 提供 bms/contactor_io.h seam 的 fake 实现（native_sim/QEMU）：apply 默认**回显**
 *          期望态（模拟执行成功），read 返回当前实测。测试可用 @ref bms_contactor_io_fake_set
 *          **强制注入**实测状态以造反馈不一致（注入后 apply 不再回显）。真实 backend
 *          （bms_f405 经 AFE(SH3673520)）属 Phase 3。契约见 hardware-abstraction.md §2。
 */

/*========== Includes ========================================================*/
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>

#include "bms/hal/contactor_io.h"

/*========== Macros and Definitions ==========================================*/

/*========== Static Constant and Variable Definitions ========================*/
/* 失效安全默认 OPEN；被 fake_set 强制后 apply 不再回显（用于造粘连/拒动）。 */
static enum bms_contactor fake_actual = BMS_CONTACTOR_OPEN;
static bool fake_forced;

/*========== Extern Constant and Variable Definitions ========================*/

/*========== Static Function Prototypes ======================================*/

/*========== Static Function Implementations =================================*/

/*========== Extern Function Implementations =================================*/
void bms_contactor_io_apply(enum bms_contactor desired)
{
	if (!fake_forced) {
		fake_actual = desired; /* 未强制注入时回显期望态（模拟执行到位） */
	}
}

int bms_contactor_io_read(struct bms_contactor_fb *out)
{
	if (out == NULL) {
		return -EINVAL;
	}
	*out = (struct bms_contactor_fb){0};
	out->actual = fake_actual;
	return 0;
}

void bms_contactor_io_fake_set(enum bms_contactor actual)
{
	fake_actual = actual;
	fake_forced = true;
}

/*========== Externalized Static Function Implementations (Unit Test) ========*/
