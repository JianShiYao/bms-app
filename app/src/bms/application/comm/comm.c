/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    comm.c
 * @brief   CAN 通信模块 —— 桩实现。
 * @ingroup COMM
 *
 * @details 职责：按固定周期对外 CAN 上报（电压/SOC/保护状态）；接收外部命令。
 *          仿真阶段（QEMU/native_sim）无真实 CAN 控制器，走日志桩；
 *          真实硬件接 can1（见第二步 overlay）。
 *
 *          日志策略：CAN 帧按 CONFIG_BMS_COMM_REPORT_PERIOD_MS 周期发送（真实硬件上即
 *          can_send）；但控制台 INF 仅在 SOC/保护状态“变化时”打印，避免在 QEMU 仿真
 *          时钟快进时刷屏。高频的逐帧测量数据走 DBG，需要时再开。
 */

/*========== Includes ========================================================*/
#include <errno.h>
#include <zephyr/logging/log.h>

#include "bms/application/comm.h"

/*========== Macros and Definitions ==========================================*/
LOG_MODULE_REGISTER(bms_comm, LOG_LEVEL_INF);

/* 运行期防御性钳制的边界源——数值必须与 app/Kconfig 中
 * BMS_COMM_REPORT_PERIOD_MS 的 `range 10 60000` 端点一致（改一处须同步另一处）。
 * (DES-COMM-003; REQ-COMM-004/005) */
#define BMS_COMM_PERIOD_MIN_MS 10
#define BMS_COMM_PERIOD_MAX_MS 60000

/*========== Static Constant and Variable Definitions ========================*/

/*========== Extern Constant and Variable Definitions ========================*/

/*========== Static Function Prototypes ======================================*/
static void comm_tx_meas(const struct bms_cell_meas *m);

/*========== Static Function Implementations =================================*/
static void comm_tx_meas(const struct bms_cell_meas *m)
{
	/* TODO: 打包为 CAN 帧并 can_send()。当前仅 DBG 桩。 */
	LOG_DBG("CAN TX meas: cell0=%dmV I=%dmA T0=%d.%d C", m->cell_mv[0], m->pack_current_ma,
		m->temp_dci[0] / 10, m->temp_dci[0] % 10);
}

/*========== Extern Function Implementations =================================*/
/* 生效上报周期 = 对编译期配置值施加运行期防御性钳制后的确定值。
 * 把 CONFIG_* 注入点收敛到唯一一处，线程与 init 共用，避免散落 k_msleep(CONFIG_*)。
 * (DES-COMM-003; REQ-COMM-004/005) */
int32_t bms_comm_effective_period_ms(void)
{
	return bms_comm_clamp_period_ms((int32_t)CONFIG_BMS_COMM_REPORT_PERIOD_MS,
					BMS_COMM_PERIOD_MIN_MS, BMS_COMM_PERIOD_MAX_MS);
}

void bms_comm_tx_snapshot(const struct bms_cell_meas *meas, const struct bms_soc *soc,
			  const struct bms_prot_evt *prot)
{
	/* 上一次已上报值，用于“变化时才 INF”（-1 为不可能的初值，确保首帧打印）。 */
	static int32_t last_soc = -1;
	static int32_t last_soh = -1;
	static int32_t last_state = -1;
	static int32_t last_contactor = -1;

	if (meas != NULL) {
		comm_tx_meas(meas);
	}

	if (soc != NULL) {
		/* TODO: can_send() SOC/SOH 帧。 */
		if (soc->soc_permille != last_soc || soc->soh_permille != last_soh) {
			LOG_INF("CAN TX soc=%u.%u%% soh=%u.%u%%", soc->soc_permille / 10,
				soc->soc_permille % 10, soc->soh_permille / 10,
				soc->soh_permille % 10);
			last_soc = soc->soc_permille;
			last_soh = soc->soh_permille;
		}
	}

	if (prot != NULL) {
		/* TODO: can_send() 保护状态帧。保护状态变化是安全事件，务必可见。 */
		if (prot->state != last_state || prot->contactor != last_contactor) {
			LOG_INF("CAN TX prot state=%d contactor=%s", prot->state,
				prot->contactor == BMS_CONTACTOR_CLOSED ? "CLOSED" : "OPEN");
			last_state = prot->state;
			last_contactor = prot->contactor;
		}
	}
}

int bms_comm_init(void)
{
	/* TODO: 取 can1 device、配置位速率、can_start()，注册 RX filter 收命令 */
	/* 打印合法化后生效周期，并列原始配置值，使「配了却被钳制」不再静默。
	 * (DES-COMM-005; REQ-COMM-007) */
	int32_t period = bms_comm_effective_period_ms();

	LOG_INF("Comm init: CAN report stub, period=%d ms (configured=%d, no CAN HW in sim)",
		period, CONFIG_BMS_COMM_REPORT_PERIOD_MS);
	return 0;
}

/*========== Externalized Static Function Implementations (Unit Test) ========*/
