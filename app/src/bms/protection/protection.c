/*
 * 保护状态机模块 —— 判定服务
 *
 * 职责：评估过压/欠压/过流/过温，并给出保护判定。
 * 调度、诊断登记与接触器控制由 bms_task/bms_diag/bms_bms 负责。
 * 失效安全原则：默认接触器 OPEN，仅当判定 NORMAL 时才 CLOSED。
 */
#include <errno.h>
#include <stdlib.h>
#include <zephyr/logging/log.h>

#include "bms/protection.h"

LOG_MODULE_REGISTER(bms_protection, LOG_LEVEL_INF);

void bms_protection_default_limits(struct bms_prot_limits *limits)
{
	if (limits == NULL) {
		return;
	}
	/* TODO: 按电芯规格表 / Kconfig 配置真实阈值 */
	limits->cell_ov_mv = 4250;       /* 4.25V 过压 */
	limits->cell_uv_mv = 2800;       /* 2.80V 欠压 */
	limits->over_current_ma = 50000; /* 50A 过流（绝对值） */
	limits->over_temp_dci = 600;     /* 60.0℃ 过温 */
}

int bms_protection_evaluate(const struct bms_cell_meas *meas, const struct bms_prot_limits *limits,
			    struct bms_prot_evt *out)
{
	if (meas == NULL || limits == NULL || out == NULL) {
		return -EINVAL;
	}

	out->timestamp_ms = meas->timestamp_ms;
	out->cell_index = 0;

	/*
	 * 失效安全（REQ-PROT-033）：要闭合接触器必须电压/电流/温度测量全部有效。
	 * 任一有效位缺失（AFE 故障 / 坏数据）即无法排除越限风险，强制 FAULT → OPEN，
	 * 绝不据无效数据闭合（对齐 concept-architecture.md「测量数据纪律」）。
	 */
	if ((meas->validity & BMS_MEAS_VALID_ALL) != BMS_MEAS_VALID_ALL) {
		out->state = BMS_PROT_FAULT;
		out->contactor = BMS_CONTACTOR_OPEN;
		return 0;
	}

	out->state = BMS_PROT_NORMAL;

	/* 电压检查（任一单体越限即触发） */
	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		if (meas->cell_mv[i] >= limits->cell_ov_mv) {
			out->state = BMS_PROT_OV;
			out->cell_index = (uint8_t)i;
			goto decide;
		}
		if (meas->cell_mv[i] <= limits->cell_uv_mv) {
			out->state = BMS_PROT_UV;
			out->cell_index = (uint8_t)i;
			goto decide;
		}
	}

	/* 过流检查（充放电绝对值） */
	if (abs(meas->pack_current_ma) >= limits->over_current_ma) {
		out->state = BMS_PROT_OC;
		goto decide;
	}

	/* 过温检查 */
	for (int i = 0; i < BMS_TEMP_SENSOR_COUNT; i++) {
		if (meas->temp_dci[i] >= limits->over_temp_dci) {
			out->state = BMS_PROT_OT;
			goto decide;
		}
	}

decide:
	/* 失效安全：仅 NORMAL 才闭合接触器 */
	out->contactor =
		(out->state == BMS_PROT_NORMAL) ? BMS_CONTACTOR_CLOSED : BMS_CONTACTOR_OPEN;
	return 0;
}

int bms_protection_init(void)
{
	LOG_INF("Protection init: fail-safe default contactor OPEN");
	return 0;
}
