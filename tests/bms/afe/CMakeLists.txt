cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_afe_test LANGUAGES C)

# 复用被测后端源码与公共头（只链接仿真后端的纯函数核心）
set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../app)

target_include_directories(app PRIVATE ${APP_DIR}/include)
target_sources(app PRIVATE
  src/main.c
  ${APP_DIR}/src/bms/afe/afe_sim.c
  ${APP_DIR}/src/bms/afe/afe_validate.c
)
