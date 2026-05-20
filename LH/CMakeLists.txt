cmake_minimum_required(VERSION 3.15)
project(LH VERSION 1.0.0 LANGUAGES CXX)

# ============ 编译器设置 ============
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============ Qt 配置 ============
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Sql
    SerialBus
    SerialPort
    Charts
    Svg
    Test
)

# ============ 输出目录 ============
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============ 包含目录 ============
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# ============ 编译选项 ============
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# ============ 子模块 ============
add_subdirectory(src/core)
add_subdirectory(src/communication)
add_subdirectory(src/compiler)
# add_subdirectory(src/components)  # 图形化逻辑组件模块已移除
add_subdirectory(src/designer)
add_subdirectory(src/monitor)

# ============ 主程序 ============
add_executable(${PROJECT_NAME} WIN32
    src/main.cpp
    resources/resources.qrc
)

target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
    Qt5::Sql
    Qt5::SerialBus
    Qt5::Charts
    core_module
    ServoValveComm
    compiler_module
    # components_module  # 图形化逻辑组件模块已移除
    designer_module
    monitor
)

target_compile_definitions(compiler_module PRIVATE
    LX_PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ============ 测试 ============
enable_testing()
add_subdirectory(tests)

# ============ 安装配置 ============
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

message(STATUS "========================================")
message(STATUS "  项目配置完成")
message(STATUS "  Qt版本: ${Qt5_VERSION}")
message(STATUS "  编译器: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "========================================")
