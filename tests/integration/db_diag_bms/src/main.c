/*
 * Engine core integration tests: bms_db -> bms_diag -> bms_bms.
 *
 * These tests pin the foxBMS 2 inspired engine contracts without starting the
 * long-running Zephyr task threads.
 */
#include <zephyr/ztest.h>

#include "bms/bms.h"
#include "bms/db.h"
#include "bms/diag.h"

ZTEST_SUITE(bms_integration, NULL, NULL, NULL, NULL, NULL);

static void reset_engine_core(void)
{
	zassert_ok(bms_db_init());
	zassert_ok(bms_diag_init());
}

static struct bms_prot_evt normal_protection(void)
{
	return (struct bms_prot_evt){
		.state = BMS_PROT_NORMAL,
		.contactor = BMS_CONTACTOR_CLOSED,
	};
}

static struct bms_state_inputs normal_inputs(void)
{
	struct bms_diag_state diag;

	zassert_ok(bms_diag_get_state(&diag));
	return (struct bms_state_inputs){
		.close_allowed = true,
		.open_request = false,
		.hw_fault_latched = false,
		.diag = diag,
		.prot = normal_protection(),
	};
}

/* Verifies REQ-ENG-001: database stores typed snapshots with validity and sequence metadata. */
ZTEST(bms_integration, test_db_write_read_snapshot)
{
	struct bms_cell_meas wr = {
		.timestamp_ms = 123U,
		.pack_current_ma = 1500,
		.validity = BMS_MEAS_VALID_ALL,
	};
	struct bms_cell_meas rd;
	struct bms_db_meta meta;

	reset_engine_core();
	wr.cell_mv[0] = 3701;
	wr.temp_dci[0] = 251;

	zassert_ok(bms_db_read_cell_meas(&rd, &meta));
	zassert_false(meta.valid, "fresh database slot must be invalid");
	zassert_equal(meta.sequence, 0U, "fresh database sequence must be zero");

	zassert_ok(bms_db_write_cell_meas(&wr));
	zassert_ok(bms_db_read_cell_meas(&rd, &meta));
	zassert_true(meta.valid, "written snapshot must be valid");
	zassert_equal(meta.sequence, 1U, "first write must increment sequence");
	zassert_equal(rd.timestamp_ms, wr.timestamp_ms);
	zassert_equal(rd.cell_mv[0], wr.cell_mv[0]);
	zassert_equal(rd.pack_current_ma, wr.pack_current_ma);
	zassert_equal(rd.temp_dci[0], wr.temp_dci[0]);
	zassert_equal(rd.validity, wr.validity);

	wr.timestamp_ms = 124U;
	zassert_ok(bms_db_write_cell_meas(&wr));
	zassert_ok(bms_db_read_cell_meas(&rd, &meta));
	zassert_equal(meta.sequence, 2U, "second write must increment sequence again");
	zassert_equal(rd.timestamp_ms, 124U);
}

/* Verifies REQ-ENG-003/004: diagnosis error prevents transition into NORMAL. */
ZTEST(bms_integration, test_diag_error_blocks_normal)
{
	struct bms_state_inputs inputs;

	reset_engine_core();
	zassert_ok(bms_diag_report(BMS_DIAG_INVALID_MEAS, BMS_DIAG_ERROR, true, false));

	inputs = normal_inputs();

	zassert_equal(bms_next_state(BMS_STATE_STANDBY, &inputs), BMS_STATE_FAULT,
		      "active diagnosis error must block NORMAL");
	zassert_equal(bms_contactor_for_state(BMS_STATE_FAULT), BMS_CONTACTOR_OPEN,
		      "fault state must keep contactor open");
}

/* Verifies REQ-ENG-005: every non-NORMAL BMS state maps to contactor OPEN. */
ZTEST(bms_integration, test_bms_default_open)
{
	zassert_equal(bms_contactor_for_state(BMS_STATE_INIT), BMS_CONTACTOR_OPEN);
	zassert_equal(bms_contactor_for_state(BMS_STATE_STANDBY), BMS_CONTACTOR_OPEN);
	zassert_equal(bms_contactor_for_state(BMS_STATE_PRECHARGE), BMS_CONTACTOR_OPEN);
	zassert_equal(bms_contactor_for_state(BMS_STATE_FAULT), BMS_CONTACTOR_OPEN);
	zassert_equal(bms_contactor_for_state(BMS_STATE_LOCKED), BMS_CONTACTOR_OPEN);
	zassert_equal(bms_contactor_for_state(BMS_STATE_NORMAL), BMS_CONTACTOR_CLOSED);
	zassert_equal(bms_next_state(BMS_STATE_NORMAL, NULL), BMS_STATE_FAULT,
		      "NULL inputs must fail safe");
}

/* Verifies REQ-ENG-004/005: protection fault flows through DB into BMS fault/open state. */
ZTEST(bms_integration, test_bms_fault_opens_contactor)
{
	struct bms_prot_evt prot = {
		.state = BMS_PROT_OV,
		.contactor = BMS_CONTACTOR_OPEN,
	};
	struct bms_prot_evt prot_rd;
	struct bms_db_meta meta;
	struct bms_diag_state diag;
	struct bms_state_inputs inputs;
	struct bms_state_snapshot snapshot;
	enum bms_state next;

	reset_engine_core();
	zassert_ok(bms_db_write_prot(&prot));
	zassert_ok(bms_db_read_prot(&prot_rd, &meta));
	zassert_true(meta.valid);
	zassert_equal(prot_rd.state, BMS_PROT_OV);

	zassert_ok(bms_diag_get_state(&diag));
	inputs = (struct bms_state_inputs){
		.close_allowed = true,
		.open_request = false,
		.hw_fault_latched = false,
		.diag = diag,
		.prot = prot_rd,
	};

	next = bms_next_state(BMS_STATE_NORMAL, &inputs);
	snapshot = (struct bms_state_snapshot){
		.state = next,
		.contactor = bms_contactor_for_state(next),
	};
	zassert_ok(bms_db_write_bms_state(&snapshot));
	zassert_ok(bms_db_read_bms_state(&snapshot, &meta));

	zassert_equal(snapshot.state, BMS_STATE_FAULT);
	zassert_equal(snapshot.contactor, BMS_CONTACTOR_OPEN,
		      "protection fault must result in contactor OPEN");
}
