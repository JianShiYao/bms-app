/*
 * CAN 通信模块 —— 桩实现
 *
 * 职责：订阅各 channel，对外 CAN 上报（电压/SOC/保护状态）；接收外部命令。
 * native_sim 下无真实 CAN 控制器，走日志桩；真实硬件接 can1（见第二步 overlay）。
 */
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bms/comm.h"
#include "bms/channels.h"

LOG_MODULE_REGISTER(bms_comm, LOG_LEVEL_INF);

#define COMM_THREAD_STACK 1024
#define COMM_THREAD_PRIO  8

ZBUS_SUBSCRIBER_DEFINE(comm_sub, 8);
ZBUS_CHAN_ADD_OBS(chan_cell_meas, comm_sub, 4);
ZBUS_CHAN_ADD_OBS(chan_soc, comm_sub, 4);
ZBUS_CHAN_ADD_OBS(chan_prot_state, comm_sub, 4);

static void comm_report_meas(const struct bms_cell_meas *m)
{
	/* TODO: 打包为 CAN 帧并 can_send()。当前仅日志桩。 */
	LOG_DBG("CAN TX meas: cell0=%dmV I=%dmA T0=%d.%d C",
		m->cell_mv[0], m->pack_current_ma,
		m->temp_dci[0] / 10, m->temp_dci[0] % 10);
}

static void comm_report_soc(const struct bms_soc *s)
{
	LOG_INF("CAN TX soc=%u.%u%% soh=%u.%u%%",
		s->soc_permille / 10, s->soc_permille % 10,
		s->soh_permille / 10, s->soh_permille % 10);
}

static void comm_report_prot(const struct bms_prot_evt *e)
{
	LOG_INF("CAN TX prot state=%d contactor=%s",
		e->state, e->contactor == BMS_CONTACTOR_CLOSED ? "CLOSED" : "OPEN");
}

static void comm_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;

	while (zbus_sub_wait(&comm_sub, &chan, K_FOREVER) == 0) {
		if (chan == &chan_cell_meas) {
			struct bms_cell_meas m;

			if (zbus_chan_read(&chan_cell_meas, &m, K_MSEC(50)) == 0) {
				comm_report_meas(&m);
			}
		} else if (chan == &chan_soc) {
			struct bms_soc s;

			if (zbus_chan_read(&chan_soc, &s, K_MSEC(50)) == 0) {
				comm_report_soc(&s);
			}
		} else if (chan == &chan_prot_state) {
			struct bms_prot_evt e;

			if (zbus_chan_read(&chan_prot_state, &e, K_MSEC(50)) == 0) {
				comm_report_prot(&e);
			}
		}
	}
}

K_THREAD_DEFINE(bms_comm_tid, COMM_THREAD_STACK, comm_thread,
		NULL, NULL, NULL, COMM_THREAD_PRIO, 0, 0);

int bms_comm_init(void)
{
	/* TODO: 取 can1 device、配置位速率、can_start()，注册 RX filter 收命令 */
	LOG_INF("Comm init: CAN report stub (native_sim has no CAN HW)");
	return 0;
}
