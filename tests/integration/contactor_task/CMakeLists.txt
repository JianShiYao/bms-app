cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_contactor_task_integration_test LANGUAGES C)

# 链接 task.c 接入 bms_contactor 后（M6 Phase 1-③b：激活接触器闭环）所需的最小
# engine 源集合。完全照 tests/integration/sys_mon_task/CMakeLists.txt 的 target_sources，
# 额外再加两行 contactor 源 —— coder 接线后 task.c 的 run_protection_and_bms 会引用
# bms_contactor_step：
#   - engine/task.c   —— 被测编排（safety_step / app_step / init）
#   - engine/time.c   —— bms_time_now_ms / due / set_source（注入时间）
#   - engine/db.c     —— bms_db_* 快照读写（含 DB_CONTACTOR_FB / DB_BMS_STATE）
#   - engine/diag.c   —— bms_diag_* 诊断聚合（CONTACTOR_MISMATCH 去抖/锁存）
#   - engine/sys_mon.c —— 任务健康聚合层（task.c 已接入，需一并链接）
#   - application/bms.c —— bms_next_state / bms_contactor_for_state 状态机
#   - protection/protection.c —— CONFIG_BMS_PROTECTION=y 时的保护判定
#   - channels.c      —— zbus channel 定义（task.c publish_* 需要）
#   - afe/afe.c + afe/afe_sim.c —— afe 采样边缘与仿真后端
#   - meas/meas.c + meas/meas_validate.c —— 采样归一与有效性
#   - contactor/contactor.c —— 接触器一步编排（本片接入：执行期望→读反馈→写
#     DB_CONTACTOR_FB→上报 CONTACTOR_MISMATCH）
#   - hal/contactor_io/contactor_io_fake.c —— 仿真 backend（fake apply/read/set）
set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../app)

# app/Kconfig 不参与隔离测试构建；task.c 用 `#if defined(CONFIG_BMS_AFE)` /
# `#if defined(CONFIG_BMS_PROTECTION)` 选择编排路径。为让被测编排走
# 「AFE 采样 + PROTECTION 判定」这条安全链，按被测配置注入等价编译宏
# （等同 app 构建 CONFIG_BMS_AFE=y / CONFIG_BMS_PROTECTION=y 的效果；
# 故意不注入 SOC/BALANCING/COMM —— 对应 #if 段保持排除，其源也不链接）。
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
  ${APP_DIR}/src/bms/hal/wdt/wdt_stub.c
  ${APP_DIR}/src/bms/hal/afe/afe.c
  ${APP_DIR}/src/bms/hal/afe/afe_sim.c
  ${APP_DIR}/src/bms/measurement-control/meas/meas.c
  ${APP_DIR}/src/bms/measurement-control/meas/meas_validate.c
  ${APP_DIR}/src/bms/measurement-control/contactor/contactor.c
  ${APP_DIR}/src/bms/hal/contactor_io/contactor_io_fake.c
)
