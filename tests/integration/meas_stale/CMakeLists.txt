cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_meas_stale_integration_test LANGUAGES C)

# M3b-1（测量过期/源失联 fail-safe）红灯：验证 bms_task_safety_step 在 DB_CELL_MEAS
# 虽 valid 但时间戳过期（now - meas.timestamp_ms > 容忍）时，置 BMS_DIAG_MEAS_STALE(ERROR)
# 且不据过期数据闭合接触器（保持失效安全默认 {FAULT, OPEN}）。
#
# 关键手法（AFE-off 植入）：仿 task_pipeline，但**不注入 CONFIG_BMS_AFE**，只注入
# CONFIG_BMS_PROTECTION=1。这样 task.c run_measurement() 的 `#if defined(CONFIG_BMS_AFE)`
# 段被整段排除 —— safety_step 不会采样、也不会覆盖/刷新 DB_CELL_MEAS。于是测试可
# 直接用 bms_db_write_cell_meas() 植入一帧带指定 timestamp_ms 的合法（valid）测量，
# 且该帧在后续 safety_step 中不被刷新，从而**确定性地制造「过期但 valid」**的槽位。
#
# 源集（照 task_pipeline，但去掉 afe/*，因 AFE 段被排除、不产生引用）：
#   - engine/task.c   —— 被测编排（safety_step / init）
#   - engine/time.c   —— bms_time_now_ms / after / set_source（注入时间）
#   - engine/db.c     —— bms_db_* 快照读写（植入 + 读回）
#   - engine/diag.c   —— bms_diag_* 诊断聚合（读 MEAS_STALE 位）
#   - application/bms.c —— bms_next_state / bms_contactor_for_state 状态机
#   - protection/protection.c —— CONFIG_BMS_PROTECTION=1 时的保护判定
#   - channels.c      —— zbus channel 定义（task.c publish_* 需要）
set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../app)

# 故意只注入 PROTECTION，不注入 AFE/SOC/BALANCING/COMM：
# 对应 #if 段全部排除，AFE 采样路径被排除 → 测试植入的帧不会被刷新（stale 可控）。
zephyr_compile_definitions(
  CONFIG_BMS_PROTECTION=1
)

target_include_directories(app PRIVATE ${APP_DIR}/include)
target_sources(app PRIVATE
  src/main.c
  ${APP_DIR}/src/bms/engine/task.c
  ${APP_DIR}/src/bms/engine/time.c
  ${APP_DIR}/src/bms/engine/db.c
  ${APP_DIR}/src/bms/engine/diag.c
  ${APP_DIR}/src/bms/engine/sys_mon.c
  ${APP_DIR}/src/bms/application/bms.c
  ${APP_DIR}/src/bms/measurement-control/protection/protection.c
  ${APP_DIR}/src/bms/engine/channels.c
  ${APP_DIR}/src/bms/measurement-control/contactor/contactor.c
  ${APP_DIR}/src/bms/hal/contactor_io/contactor_io_fake.c
)
