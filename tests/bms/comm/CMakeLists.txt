cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_comm_test LANGUAGES C)

# 只链接 comm 模块的纯函数核心（comm_period.c），不链 comm.c
# （后者含 K_THREAD_DEFINE / 桩 TX / LOG_MODULE_REGISTER，不应拖进 host 单测）。
# 对齐 tests/bms/afe 只链 afe_sim.c + afe_validate.c 的范式（DES-COMM-006 / ADR-COMM-08）。
set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../app)

target_include_directories(app PRIVATE ${APP_DIR}/include)
target_sources(app PRIVATE
  src/main.c
  ${APP_DIR}/src/bms/comm/comm_period.c
)
