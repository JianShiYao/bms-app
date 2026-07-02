/*
 * 测量可信化纯函数单元测试（ztest, native_sim / mps2-an386）。
 *
 * 被测：bms_meas_validate —— 合理性校验（测量数据纪律，安全相关）。
 * 回链：docs/concept/architecture.md「测量数据纪律」、docs/concept/data-model.md
 *       （raw → meas 可信化 → DB）。校验为纯函数（无后端/线程/zbus），直接 ztest。
 */

/*========== Includes ========================================================*/
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/ztest.h>

#include "bms/measurement-control/meas.h"
#include "bms/types.h"

/*========== Macros and Definitions ==========================================*/

/*========== Static Constant and Variable Definitions ========================*/
/* 与 meas.c 经 Kconfig 注入的回退默认一致的本地镜像 */
static const struct bms_meas_limits TEST_LIMITS = {
	.cell_mv_min = 0,
	.cell_mv_max = 6000,
	.current_abs_max_ma = 300000,
	.temp_dci_min = -400,
	.temp_dci_max = 1250,
};

/*========== Extern Constant and Variable Definitions ========================*/

/*========== Static Function Prototypes ======================================*/
static void make_good_frame(struct bms_cell_meas *m);

/*========== Static Function Implementations =================================*/
/* 构造一帧"全部在合理范围内"的测量 */
static void make_good_frame(struct bms_cell_meas *m)
{
	*m = (struct bms_cell_meas){0};
	for (int i = 0; i < BMS_CELL_COUNT; i++) {
		m->cell_mv[i] = 3700;
	}
	m->pack_current_ma = 1000;
	for (int i = 0; i < BMS_TEMP_SENSOR_COUNT; i++) {
		m->temp_dci[i] = 250;
	}
}

ZTEST_SUITE(bms_meas, NULL, NULL, NULL, NULL, NULL);

ZTEST(bms_meas, test_validate_null_returns_einval)
{
	struct bms_cell_meas m;

	make_good_frame(&m);
	zassert_equal(bms_meas_validate(NULL, &TEST_LIMITS), -EINVAL, "m=NULL 应 -EINVAL");
	zassert_equal(bms_meas_validate(&m, NULL), -EINVAL, "lim=NULL 应 -EINVAL");
}

ZTEST(bms_meas, test_good_frame_is_all_valid)
{
	struct bms_cell_meas m;

	make_good_frame(&m);
	zassert_ok(bms_meas_validate(&m, &TEST_LIMITS), NULL);
	zassert_equal(m.validity, BMS_MEAS_VALID_ALL, "合理帧应全部有效，实测 0x%02x", m.validity);
}

ZTEST(bms_meas, test_overvoltage_clears_only_voltage_bit)
{
	struct bms_cell_meas m;

	make_good_frame(&m);
	m.cell_mv[3] = 6001; /* 超上限 */
	zassert_ok(bms_meas_validate(&m, &TEST_LIMITS), NULL);
	zassert_equal(m.validity & BMS_MEAS_VALID_VOLTAGE, 0, "越界电压应清电压有效位");
	zassert_not_equal(m.validity & BMS_MEAS_VALID_CURRENT, 0, "电流位不应受影响");
	zassert_not_equal(m.validity & BMS_MEAS_VALID_TEMP, 0, "温度位不应受影响");
}

ZTEST(bms_meas, test_undervoltage_clears_voltage_bit)
{
	struct bms_cell_meas m;

	make_good_frame(&m);
	m.cell_mv[0] = -1; /* 低于下限 0 */
	zassert_ok(bms_meas_validate(&m, &TEST_LIMITS), NULL);
	zassert_equal(m.validity & BMS_MEAS_VALID_VOLTAGE, 0, "欠压坏值应清电压有效位");
}

ZTEST(bms_meas, test_overcurrent_clears_only_current_bit)
{
	struct bms_cell_meas m;

	make_good_frame(&m);
	m.pack_current_ma = -300001; /* 绝对值超限 */
	zassert_ok(bms_meas_validate(&m, &TEST_LIMITS), NULL);
	zassert_equal(m.validity & BMS_MEAS_VALID_CURRENT, 0, "越界电流应清电流有效位");
	zassert_not_equal(m.validity & BMS_MEAS_VALID_VOLTAGE, 0, "电压位不应受影响");
}

ZTEST(bms_meas, test_over_temp_clears_only_temp_bit)
{
	struct bms_cell_meas m;

	make_good_frame(&m);
	m.temp_dci[2] = 1251; /* 超上限 */
	zassert_ok(bms_meas_validate(&m, &TEST_LIMITS), NULL);
	zassert_equal(m.validity & BMS_MEAS_VALID_TEMP, 0, "越界温度应清温度有效位");
	zassert_not_equal(m.validity & BMS_MEAS_VALID_VOLTAGE, 0, "电压位不应受影响");
}

ZTEST(bms_meas, test_validate_does_not_modify_values)
{
	struct bms_cell_meas m, before;

	make_good_frame(&m);
	m.cell_mv[5] = 9999; /* 故意坏值 */
	before = m;
	zassert_ok(bms_meas_validate(&m, &TEST_LIMITS), NULL);
	/* 仅 validity 可变；其余字段逐位不变 */
	before.validity = m.validity;
	zassert_equal(memcmp(&m, &before, sizeof(m)), 0, "校验不得修改测量值，仅写 validity");
}

/*========== Extern Function Implementations =================================*/

/*========== Externalized Static Function Implementations (Unit Test) ========*/
