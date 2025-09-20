cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensorhub)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_sources(app PRIVATE
  app/main.c
  app/shell.c
  app/power.c
  drivers/vs_temp.c
  drivers/vs_imu.c
  subsys/mqtt_bridge/mqtt_bridge.c
)
