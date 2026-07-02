cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_contactor_test LANGUAGES C)

# 链接被测接触器抽象 + fake backend + db/diag/time 依赖（纯函数 eval + 一步 step）。
# APP_DIR 深度＝tests/bms/measurement-control/contactor → ../../../../app
set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../app)

target_include_directories(app PRIVATE ${APP_DIR}/include)
target_sources(app PRIVATE
  src/main.c
  ${APP_DIR}/src/bms/measurement-control/contactor/contactor.c
  ${APP_DIR}/src/bms/hal/contactor_io/contactor_io_fake.c
  ${APP_DIR}/src/bms/engine/db.c
  ${APP_DIR}/src/bms/engine/diag.c
  ${APP_DIR}/src/bms/engine/time.c
)
