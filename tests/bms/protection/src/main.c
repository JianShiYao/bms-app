/*
 * 保护状态机单元测试（ztest, native_sim）
 *
 * 重点验证失效安全：异常时接触器必须 OPEN；正常时才 CLOSED。
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
	zassert_equal(evt.contactor, BMS_CONTACTOR_CLOSED,
		      "normal must close contactor");
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
