/*
 * 保护状态机单元测试（ztest, native_sim）
 *
 * 重点验证失效安全：异常/无效数据时接触器必须 OPEN；仅 NORMAL 才 CLOSED。
 * 含红线不变量扫描（CLOSED ⟺ NORMAL）与无效测量隔离（REQ-PROT-033）。
 */
#include <errno.h>
#include <string.h>
#include <zephyr/ztest.h>

#include "bms/protection.h"

ZTEST_SUITE(bms_protection, NULL, NULL, NULL, NULL, NULL);

/* 构造一帧全部正常的测量 */
static void make_normal(struct bms_cell_meas *m)
{
	memset(m, 0, sizeof(*m));
	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		m->cell_mv[i] = 3700;
	}
	m->pack_current_ma = 1000;
	for (int i = 0; i < BMS_TEMP_SENSOR_COUNT; i++) {
		m->temp_dci[i] = 250;
	}
	m->validity = BMS_MEAS_VALID_ALL; /* 正常帧：测量全有效，否则失效安全强制 OPEN */
}

ZTEST(bms_protection, test_normal_closes_contactor)
{
	struct bms_cell_meas m;
	struct bms_prot_limits lim;
	struct bms_prot_evt evt;

	make_normal(&m);
	bms_protection_default_limits(&lim);

	zassert_ok(bms_protection_evaluate(&m, &lim, &evt));
	zassert_equal(evt.state, BMS_PROT_NORMAL);
	zassert_equal(evt.contactor, BMS_CONTACTOR_CLOSED, "normal must close contactor");
}

ZTEST(bms_protection, test_overvoltage_opens)
{
	struct bms_cell_meas m;
	struct bms_prot_limits lim;
	struct bms_prot_evt evt;

	make_normal(&m);
	bms_protection_default_limits(&lim);
	m.cell_mv[3] = lim.cell_ov_mv + 10;

	zassert_ok(bms_protection_evaluate(&m, &lim, &evt));
	zassert_equal(evt.state, BMS_PROT_OV);
	zassert_equal(evt.cell_index, 3);
	zassert_equal(evt.contactor, BMS_CONTACTOR_OPEN, "OV must open");
}

ZTEST(bms_protection, test_undervoltage_opens)
{
	struct bms_cell_meas m;
	struct bms_prot_limits lim;
	struct bms_prot_evt evt;

	make_normal(&m);
	bms_protection_default_limits(&lim);
	m.cell_mv[0] = lim.cell_uv_mv - 10;

	zassert_ok(bms_protection_evaluate(&m, &lim, &evt));
	zassert_equal(evt.state, BMS_PROT_UV);
	zassert_equal(evt.contactor, BMS_CONTACTOR_OPEN, "UV must open");
}

ZTEST(bms_protection, test_overcurrent_opens)
{
	struct bms_cell_meas m;
	struct bms_prot_limits lim;
	struct bms_prot_evt evt;

	make_normal(&m);
	bms_protection_default_limits(&lim);
	m.pack_current_ma = -(lim.over_current_ma + 100); /* 放电过流 */

	zassert_ok(bms_protection_evaluate(&m, &lim, &evt));
	zassert_equal(evt.state, BMS_PROT_OC);
	zassert_equal(evt.contactor, BMS_CONTACTOR_OPEN, "OC must open");
}

ZTEST(bms_protection, test_overtemp_opens)
{
	struct bms_cell_meas m;
	struct bms_prot_limits lim;
	struct bms_prot_evt evt;

	make_normal(&m);
	bms_protection_default_limits(&lim);
	m.temp_dci[1] = lim.over_temp_dci + 10;

	zassert_ok(bms_protection_evaluate(&m, &lim, &evt));
	zassert_equal(evt.state, BMS_PROT_OT);
	zassert_equal(evt.contactor, BMS_CONTACTOR_OPEN, "OT must open");
}

ZTEST(bms_protection, test_null_args)
{
	zassert_equal(bms_protection_evaluate(NULL, NULL, NULL), -EINVAL);
}

/* 失效安全：测量全无效（AFE 故障）即便阈值正常也必须 FAULT→OPEN（Verifies REQ-PROT-033） */
ZTEST(bms_protection, test_invalid_validity_opens)
{
	struct bms_cell_meas m;
	struct bms_prot_limits lim;
	struct bms_prot_evt evt;

	make_normal(&m); /* 阈值全正常 */
	m.validity = 0;  /* 但测量全无效 */
	bms_protection_default_limits(&lim);

	zassert_ok(bms_protection_evaluate(&m, &lim, &evt));
	zassert_equal(evt.state, BMS_PROT_FAULT, "invalid measurement must fault");
	zassert_equal(evt.contactor, BMS_CONTACTOR_OPEN,
		      "invalid data must NOT close (REQ-PROT-033)");
}

/* 失效安全：缺任一有效位（电压/电流/温度）都必须 OPEN（Verifies REQ-PROT-033） */
ZTEST(bms_protection, test_partial_validity_opens)
{
	const uint8_t bits[] = {BMS_MEAS_VALID_VOLTAGE, BMS_MEAS_VALID_CURRENT,
				BMS_MEAS_VALID_TEMP};
	struct bms_prot_limits lim;

	bms_protection_default_limits(&lim);

	for (size_t k = 0; k < ARRAY_SIZE(bits); k++) {
		struct bms_cell_meas m;
		struct bms_prot_evt evt;

		make_normal(&m);
		m.validity = (uint8_t)(BMS_MEAS_VALID_ALL & ~bits[k]); /* 清掉一位 */

		zassert_ok(bms_protection_evaluate(&m, &lim, &evt));
		zassert_equal(evt.contactor, BMS_CONTACTOR_OPEN,
			      "missing validity bit 0x%x must open", (unsigned int)bits[k]);
	}
}

/*
 * 失效安全红线不变量（Verifies REQ-PROT-033）：扫描电压/电流/温度/有效位组合，
 * 断言「接触器 CLOSED 当且仅当 NORMAL」——非 NORMAL 绝不 CLOSED。
 */
ZTEST(bms_protection, test_invariant_closed_iff_normal)
{
	const int32_t volts[] = {2700, 3700, 4300};    /* 欠压 / 正常 / 过压 */
	const int32_t currs[] = {-60000, 1000, 60000}; /* 放电过流 / 正常 / 充电过流 */
	const int32_t temps[] = {250, 700};            /* 正常 / 过温 */
	const uint8_t vals[] = {BMS_MEAS_VALID_ALL, 0,
				(uint8_t)(BMS_MEAS_VALID_ALL & ~BMS_MEAS_VALID_TEMP)};
	struct bms_prot_limits lim;

	bms_protection_default_limits(&lim);

	for (size_t a = 0; a < ARRAY_SIZE(volts); a++) {
		for (size_t b = 0; b < ARRAY_SIZE(currs); b++) {
			for (size_t c = 0; c < ARRAY_SIZE(temps); c++) {
				for (size_t d = 0; d < ARRAY_SIZE(vals); d++) {
					struct bms_cell_meas m;
					struct bms_prot_evt evt;

					make_normal(&m);
					for (int i = 0; i < BMS_CELL_COUNT; i++) {
						m.cell_mv[i] = volts[a];
					}
					m.pack_current_ma = currs[b];
					for (int i = 0; i < BMS_TEMP_SENSOR_COUNT; i++) {
						m.temp_dci[i] = temps[c];
					}
					m.validity = vals[d];

					zassert_ok(bms_protection_evaluate(&m, &lim, &evt));
					int closed = (evt.contactor == BMS_CONTACTOR_CLOSED);
					int normal = (evt.state == BMS_PROT_NORMAL);
					zassert_equal(
						closed, normal,
						"CLOSED iff NORMAL violated: state=%d contactor=%d",
						evt.state, evt.contactor);
				}
			}
		}
	}
}

/* 边界：cell_mv == 过压阈值即触发（`>=`）；阈值-1 不触发。 */
ZTEST(bms_protection, test_ov_threshold_boundary)
{
	struct bms_cell_meas m;
	struct bms_prot_limits lim;
	struct bms_prot_evt evt;

	bms_protection_default_limits(&lim);

	make_normal(&m);
	m.cell_mv[0] = lim.cell_ov_mv; /* 恰好等于阈值 */
	zassert_ok(bms_protection_evaluate(&m, &lim, &evt));
	zassert_equal(evt.state, BMS_PROT_OV, "cell_mv == ov threshold must trip (>=)");
	zassert_equal(evt.contactor, BMS_CONTACTOR_OPEN);

	make_normal(&m);
	m.cell_mv[0] = lim.cell_ov_mv - 1; /* 阈值下方 */
	zassert_ok(bms_protection_evaluate(&m, &lim, &evt));
	zassert_equal(evt.state, BMS_PROT_NORMAL, "just below ov must be normal");
	zassert_equal(evt.contactor, BMS_CONTACTOR_CLOSED);
}
