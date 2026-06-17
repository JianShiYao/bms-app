# BMS 应用 CMake —— Zephyr application
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_app LANGUAGES C)

# 模块公共头（对外接口）
target_include_directories(app PRIVATE include)

# 核心源：main + zbus channel 定义
target_sources(app PRIVATE
  src/main.c
  src/bms/channels.c
)

# 各 BMS 模块按 Kconfig 开关裁剪编译
target_sources_ifdef(CONFIG_BMS_AFE        app PRIVATE src/bms/afe/afe.c)
target_sources_ifdef(CONFIG_BMS_SOC        app PRIVATE src/bms/soc/soc.c)
target_sources_ifdef(CONFIG_BMS_PROTECTION app PRIVATE src/bms/protection/protection.c)
target_sources_ifdef(CONFIG_BMS_BALANCING  app PRIVATE src/bms/balancing/balancing.c)
target_sources_ifdef(CONFIG_BMS_COMM       app PRIVATE src/bms/comm/comm.c)
