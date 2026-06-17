cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_protection_test LANGUAGES C)

set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../app)

target_include_directories(app PRIVATE ${APP_DIR}/include)
target_sources(app PRIVATE
  src/main.c
  ${APP_DIR}/src/bms/channels.c
  ${APP_DIR}/src/bms/protection/protection.c
)
