/*
 * CAN 通信模块 —— 桩实现
 *
 * 职责：按固定周期对外 CAN 上报（电压/SOC/保护状态）；接收外部命令。
 * 仿真阶段（QEMU/native_sim）无真实 CAN 控制器，走日志桩；
 * 真实硬件接 can1（见第二步 overlay）。
 *
 * 日志策略：CAN 帧按 CONFIG_BMS_COMM_REPORT_PERIOD_MS 周期发送（真实硬件上即
 * can_send）；但控制台 INF 仅在 SOC/保护状态“变化时”打印，避免在 QEMU 仿真
 * 时钟快进时刷屏。高频的逐帧测量数据走 DBG，需要时再开。
 */
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bms/comm.h"
#include "bms/channels.h"

LOG_MODULE_REGISTER(bms_comm, LOG_LEVEL_INF);

#define COMM_THREAD_STACK 1024
#define COMM_THREAD_PRIO  8

static void comm_tx_meas(const struct bms_cell_meas *m)
{
	/* TODO: 打包为 CAN 帧并 can_send()。当前仅 DBG 桩。 */
	LOG_DBG("CAN TX meas: cell0=%dmV I=%dmA T0=%d.%d C", m->cell_mv[0], m->pack_current_ma,
		m->temp_dci[0] / 10, m->temp_dci[0] % 10);
}

static void comm_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct bms_cell_meas meas;
	struct bms_soc soc;
	struct bms_prot_evt prot;

	/* 上一次已上报值，用于“变化时才 INF”（-1 为不可能的初值，确保首帧打印）。 */
	int32_t last_soc = -1;
	int32_t last_soh = -1;
	int32_t last_state = -1;
	int32_t last_contactor = -1;

	while (1) {
		/*
		 * 周期性快照上报：直接读取各 channel 最近一次发布的值，
		 * 与内部各模块的更新节奏解耦，CAN 帧按固定周期对外广播。
		 */
		if (zbus_chan_read(&chan_cell_meas, &meas, K_MSEC(50)) == 0) {
			comm_tx_meas(&meas);
		}

		if (zbus_chan_read(&chan_soc, &soc, K_MSEC(50)) == 0) {
			/* TODO: can_send() SOC/SOH 帧。 */
			if (soc.soc_permille != last_soc || soc.soh_permille != last_soh) {
				LOG_INF("CAN TX soc=%u.%u%% soh=%u.%u%%", soc.soc_permille / 10,
					soc.soc_permille % 10, soc.soh_permille / 10,
					soc.soh_permille % 10);
				last_soc = soc.soc_permille;
				last_soh = soc.soh_permille;
			}
		}

		if (zbus_chan_read(&chan_prot_state, &prot, K_MSEC(50)) == 0) {
			/* TODO: can_send() 保护状态帧。保护状态变化是安全事件，务必可见。 */
			if (prot.state != last_state || prot.contactor != last_contactor) {
				LOG_INF("CAN TX prot state=%d contactor=%s", prot.state,
					prot.contactor == BMS_CONTACTOR_CLOSED ? "CLOSED" : "OPEN");
				last_state = prot.state;
				last_contactor = prot.contactor;
			}
		}

		k_msleep(CONFIG_BMS_COMM_REPORT_PERIOD_MS);
	}
}

K_THREAD_DEFINE(bms_comm_tid, COMM_THREAD_STACK, comm_thread, NULL, NULL, NULL, COMM_THREAD_PRIO, 0,
		0);

int bms_comm_init(void)
{
	/* TODO: 取 can1 device、配置位速率、can_start()，注册 RX filter 收命令 */
	LOG_INF("Comm init: CAN report stub, period=%d ms (no CAN HW in sim)",
		CONFIG_BMS_COMM_REPORT_PERIOD_MS);
	return 0;
}
