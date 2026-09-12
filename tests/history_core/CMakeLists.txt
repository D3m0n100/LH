cmake_minimum_required(VERSION 3.16)
project(LHHistoryCoreTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
enable_testing()
find_package(Qt5 REQUIRED COMPONENTS Core Test)
get_filename_component(LH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# Build the production history boundary without configuring the GUI application.
add_library(monitor_history STATIC
    "${LH_ROOT}/src/monitor/MonitorHistoryService.cpp"
    "${LH_ROOT}/src/monitor/MonitorHistoryService.h"
)
target_include_directories(monitor_history PUBLIC "${LH_ROOT}/src")
target_link_libraries(monitor_history PUBLIC Qt5::Core)

add_executable(monitor_history_core_test "${LH_ROOT}/tests/monitor_history_core_test.cpp")
target_link_libraries(monitor_history_core_test PRIVATE monitor_history Qt5::Test)
add_test(NAME MonitorHistoryCoreTest COMMAND monitor_history_core_test)
