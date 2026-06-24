# BMS 应用 CMake —— Zephyr application
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_app LANGUAGES C)

# 模块公共头（对外接口）
target_include_directories(app PRIVATE include)

# 健壮性：app 自有源码的编译告警视为错误（仅限 app 目标，不波及 Zephyr/modules）。
# 与 clang-tidy(WarningsAsErrors)/sca-gcc 形成互补，挡住编译器层告警。
target_compile_options(app PRIVATE -Werror)

# 核心源：main + zbus channel 定义
target_sources(app PRIVATE
  src/main.c
  src/bms/channels.c
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
