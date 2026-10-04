cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_sensors)

set(SENSORHUB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_include_directories(app PRIVATE ${SENSORHUB_DIR}/include)
target_sources(app PRIVATE
  src/main.c
  ${SENSORHUB_DIR}/drivers/vs_temp.c
  ${SENSORHUB_DIR}/drivers/vs_imu.c
)
