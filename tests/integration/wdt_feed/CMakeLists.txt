cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_wdt_feed_integration_test LANGUAGES C)

# 把 M5 喂狗门控（bms_sys_mon_wdt_feed_allowed）接到硬 watchdog（hal/wdt）的
# 集成型红灯套件。被测入口 bms_task_wdt_step / bms_task_init 都在 engine/task.c，
# 链 task.c 会拉入其全部依赖，故源集完全照 tests/integration/sys_mon_task，
# 额外再加 hal/wdt/wdt_stub.c（提供 bms_wdt_init/feed/stub_feed_count）：
#   - engine/task.c    —— 被测编排（wdt_step / safety_step / init）
#   - engine/time.c    —— bms_time_now_ms / due / set_source（注入时间）
#   - engine/db.c      —— bms_db_* 快照读写
#   - engine/diag.c    —— bms_diag_* 诊断聚合
#   - engine/sys_mon.c —— 任务健康聚合层（enter/exit/step/init + wdt_feed_allowed）
#   - application/bms.c —— bms_next_state / bms_contactor_for_state 状态机
#   - protection/protection.c —— CONFIG_BMS_PROTECTION=y 时的保护判定
#   - channels.c       —— zbus channel 定义（task.c publish_* 需要）
#   - afe/afe.c + afe/afe_sim.c —— afe 采样边缘与仿真后端
#   - meas/meas.c + meas/meas_validate.c —— 采样有效性
#   - contactor/contactor.c + hal/contactor_io/contactor_io_fake.c —— 接触器输出
#   - hal/wdt/wdt_stub.c —— 看门狗 stub 后端（feed 记数供测试观测）
set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../app)

# app/Kconfig 不参与隔离测试构建；task.c 用 `#if defined(CONFIG_BMS_AFE)` /
# `#if defined(CONFIG_BMS_PROTECTION)` 选择编排路径。为让被测编排走
# 「AFE 采样 + PROTECTION 判定」这条安全链，注入等价编译宏（照 sys_mon_task）。
zephyr_compile_definitions(
  CONFIG_BMS_AFE=1
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
  ${APP_DIR}/src/bms/hal/afe/afe.c
  ${APP_DIR}/src/bms/hal/afe/afe_sim.c
  ${APP_DIR}/src/bms/measurement-control/meas/meas.c
  ${APP_DIR}/src/bms/measurement-control/meas/meas_validate.c
  ${APP_DIR}/src/bms/hal/wdt/wdt_stub.c
)
