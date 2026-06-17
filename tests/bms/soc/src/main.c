/*
 * SOC 估算模块单元测试（ztest, native_sim）
 */
#include <errno.h>
#include <zephyr/ztest.h>

#include "bms/soc.h"

ZTEST_SUITE(bms_soc, NULL, NULL, NULL, NULL, NULL);

static void fill_cells(struct bms_cell_meas *m, int32_t mv)
{
	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		m->cell_mv[i] = mv;
	}
}

ZTEST(bms_soc, test_full_charge)
{
	struct bms_cell_meas m = {0};
	struct bms_soc soc;

	fill_cells(&m, 4200);
	zassert_ok(bms_soc_estimate(&m, &soc));
	zassert_equal(soc.soc_permille, 1000, "4.2V should map to 100%%");
}

ZTEST(bms_soc, test_empty)
{
	struct bms_cell_meas m = {0};
	struct bms_soc soc;

	fill_cells(&m, 3000);
	zassert_ok(bms_soc_estimate(&m, &soc));
	zassert_equal(soc.soc_permille, 0, "3.0V should map to 0%%");
}

ZTEST(bms_soc, test_mid_in_range)
{
	struct bms_cell_meas m = {0};
	struct bms_soc soc;

	fill_cells(&m, 3600);
	zassert_ok(bms_soc_estimate(&m, &soc));
	zassert_true(soc.soc_permille > 0 && soc.soc_permille < 1000,
		     "mid voltage should be within (0,1000)");
}

ZTEST(bms_soc, test_clamp_over_full)
{
	struct bms_cell_meas m = {0};
	struct bms_soc soc;

	fill_cells(&m, 4500);
	zassert_ok(bms_soc_estimate(&m, &soc));
	zassert_equal(soc.soc_permille, 1000, "above-full must clamp to 100%%");
}

ZTEST(bms_soc, test_null_args)
{
	zassert_equal(bms_soc_estimate(NULL, NULL), -EINVAL);
}
