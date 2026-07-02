cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_sys_mon_test LANGUAGES C)

# 复用被测模块源码与公共头。
# 心跳超时判定复用 bms_time_after（回绕安全），故一并列入 time.c。
set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../app)

target_include_directories(app PRIVATE ${APP_DIR}/include)
target_sources(app PRIVATE
  src/main.c
  ${APP_DIR}/src/bms/engine/sys_mon.c
  ${APP_DIR}/src/bms/engine/time.c
)
