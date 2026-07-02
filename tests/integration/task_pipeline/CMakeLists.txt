cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_task_pipeline_integration_test LANGUAGES C)

# 链接 task.c 的编排单步（bms_task_safety_step）所需的最小 engine 源集合。
# 只列 task.c 在 CONFIG_BMS_AFE=y + CONFIG_BMS_PROTECTION=y、其余模块关 下真正引用到的编译单元：
#   - engine/task.c   —— 被测编排（safety_step / app_step / init / start）
#   - engine/time.c   —— bms_time_now_ms / due / set_source（注入时间）
#   - engine/db.c     —— bms_db_* 快照读写
#   - engine/diag.c   —— bms_diag_* 诊断聚合
#   - application/bms.c —— bms_next_state / bms_contactor_for_state 状态机
#   - protection/protection.c —— CONFIG_BMS_PROTECTION=y 时的保护判定
#   - channels.c      —— zbus channel 定义（task.c publish_* 需要）
#   - afe/afe.c + afe/afe_validate.c + afe/afe_sim.c —— afe 采样边缘与仿真后端
#     （照 tests/bms/afe：直接列 sim 后端源，不经 app Kconfig 的 target_sources_ifdef）
# 不列 soc/balancing/comm：对应 CONFIG 关闭，task.c 里被 #if defined(...) 排除，不产生引用。
set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../app)

# app/Kconfig 不参与隔离测试构建（测试 autoconf.h 里没有 CONFIG_BMS_* 符号），
# 而 task.c 用 `#if defined(CONFIG_BMS_AFE)` / `#if defined(CONFIG_BMS_PROTECTION)`
# 选择编排路径。为让被测编排走「AFE 采样 + PROTECTION 判定」这条安全链，
# 在此按被测配置注入等价编译宏（等同 app 构建 CONFIG_BMS_AFE=y / CONFIG_BMS_PROTECTION=y 的效果；
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
  ${APP_DIR}/src/bms/hal/afe/afe.c
  ${APP_DIR}/src/bms/hal/afe/afe_validate.c
  ${APP_DIR}/src/bms/hal/afe/afe_sim.c
)
