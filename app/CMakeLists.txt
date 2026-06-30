# BMS 应用 CMake —— Zephyr application
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_app LANGUAGES C)

# 模块公共头（对外接口）
target_include_directories(app PRIVATE include)

# 把 git 版本编进固件，供现场/日志追溯（呼应 foxBMS VER_VERSION_s）。git describe
# 在 configure 时取值：--always 无 tag 退化为 hash、--dirty 标记工作树未提交改动、
# --tags 用最近 tag。非 git 环境回退 "unknown"。应用版本（语义化）由 Zephyr 从
# app/VERSION 自动生成的 <app_version.h> 提供。
execute_process(
  COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR} describe --always --dirty --tags --abbrev=12
  OUTPUT_VARIABLE BMS_GIT_DESCRIBE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
if(NOT BMS_GIT_DESCRIBE)
  set(BMS_GIT_DESCRIBE "unknown")
endif()
set(BMS_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/generated)
configure_file(include/bms/build_info.h.in ${BMS_GENERATED_DIR}/bms/build_info.h @ONLY)
target_include_directories(app PRIVATE ${BMS_GENERATED_DIR})

# 健壮性：app 自有源码的编译告警视为错误（仅限 app 目标，不波及 Zephyr/modules）。
# 与 clang-tidy(WarningsAsErrors)/sca-gcc 形成互补，挡住编译器层告警。
target_compile_options(app PRIVATE -Werror)

# 核心源：main + compatibility zbus channel + foxBMS-style engine
target_sources(app PRIVATE
  src/main.c
  src/bms/channels.c
  src/bms/engine/db.c
  src/bms/engine/diag.c
  src/bms/engine/task.c
  src/bms/application/bms.c
)

# 各 BMS 模块按 Kconfig 开关裁剪编译
target_sources_ifdef(CONFIG_BMS_AFE        app PRIVATE src/bms/afe/afe.c)
# afe 测量合理性校验（纯函数，测量数据纪律）
target_sources_ifdef(CONFIG_BMS_AFE        app PRIVATE src/bms/afe/afe_validate.c)
# afe 采样后端：三选一（choice 保证恰好一个 backend 提供 bms_afe_backend_read）
target_sources_ifdef(CONFIG_BMS_AFE_BACKEND_STUB app PRIVATE src/bms/afe/afe_stub.c)
target_sources_ifdef(CONFIG_BMS_AFE_BACKEND_SIM  app PRIVATE src/bms/afe/afe_sim.c)
target_sources_ifdef(CONFIG_BMS_AFE_BACKEND_ADC  app PRIVATE src/bms/afe/afe_adc.c)
target_sources_ifdef(CONFIG_BMS_SOC        app PRIVATE src/bms/soc/soc.c)
target_sources_ifdef(CONFIG_BMS_PROTECTION app PRIVATE src/bms/protection/protection.c)
target_sources_ifdef(CONFIG_BMS_BALANCING  app PRIVATE src/bms/balancing/balancing.c)
target_sources_ifdef(CONFIG_BMS_COMM       app PRIVATE src/bms/comm/comm.c)
# comm 周期合法化（纯函数，运行期防御性钳制；单测只链此源）
target_sources_ifdef(CONFIG_BMS_COMM       app PRIVATE src/bms/comm/comm_period.c)
