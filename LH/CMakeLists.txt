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

# ============ 安装时 Qt 部署配置（Windows 默认启用） ============
if(WIN32)
    set(LH_ENABLE_QT_DEPLOYMENT_DEFAULT ON)
else()
    set(LH_ENABLE_QT_DEPLOYMENT_DEFAULT OFF)
endif()
option(LH_ENABLE_QT_DEPLOYMENT
    "Deploy Qt runtime and required plugins with the Windows installation"
    ${LH_ENABLE_QT_DEPLOYMENT_DEFAULT}
)

if(WIN32 AND LH_ENABLE_QT_DEPLOYMENT)
    set(LH_QT_BIN_HINTS)
    if(DEFINED Qt5Core_DIR)
        get_filename_component(LH_QT_CORE_CMAKE_DIR "${Qt5Core_DIR}" ABSOLUTE)
        get_filename_component(LH_QT_PREFIX "${LH_QT_CORE_CMAKE_DIR}/../../.." ABSOLUTE)
        list(APPEND LH_QT_BIN_HINTS "${LH_QT_PREFIX}/bin")
    endif()
    if(DEFINED ENV{QTDIR} AND NOT "$ENV{QTDIR}" STREQUAL "")
        list(APPEND LH_QT_BIN_HINTS "$ENV{QTDIR}/bin")
    endif()

    find_program(LH_WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt.exe
        HINTS ${LH_QT_BIN_HINTS}
        PATH_SUFFIXES bin
    )
    if(NOT LH_WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR
            "LH_ENABLE_QT_DEPLOYMENT is ON, but the Qt-matched windeployqt was not found. "
            "Set PATH/QTDIR or configure with -DLH_ENABLE_QT_DEPLOYMENT=OFF only for "
            "a packaging environment that supplies Qt separately."
        )
    endif()

    file(TO_CMAKE_PATH
        "${LH_WINDEPLOYQT_EXECUTABLE}"
        LH_WINDEPLOYQT_EXECUTABLE_FOR_INSTALL
    )

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeployQt.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/DeployQt.cmake"
        @ONLY
    )
endif()

# ============ 输出目录 ============
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============ 包含目录 ============
set(LX_PUBLIC_INCLUDE_DIRS
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

target_include_directories(${PROJECT_NAME} PRIVATE
    ${LX_PUBLIC_INCLUDE_DIRS}
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

# ============ 安装 Python DSL 运行时（严格白名单） ============
set(LH_DSL_RUNTIME_SOURCE
    "${CMAKE_SOURCE_DIR}/third_party/custom_dsp_language/compile"
)
set(LH_DSL_RUNTIME_INSTALL
    "third_party/custom_dsp_language/compile"
)

install(FILES
    "${LH_DSL_RUNTIME_SOURCE}/lmc.py"
    "${LH_DSL_RUNTIME_SOURCE}/requirements.txt"
    DESTINATION "${LH_DSL_RUNTIME_INSTALL}"
)

install(FILES
    "${LH_DSL_RUNTIME_SOURCE}/grammar/LHLexer.py"
    "${LH_DSL_RUNTIME_SOURCE}/grammar/LHParser.py"
    "${LH_DSL_RUNTIME_SOURCE}/grammar/LHListener.py"
    "${LH_DSL_RUNTIME_SOURCE}/grammar/LHVisitor.py"
    DESTINATION "${LH_DSL_RUNTIME_INSTALL}/grammar"
)

install(FILES
    "${LH_DSL_RUNTIME_SOURCE}/src/__init__.py"
    DESTINATION "${LH_DSL_RUNTIME_INSTALL}/src"
)

install(FILES
    "${LH_DSL_RUNTIME_SOURCE}/src/lh_compiler/__init__.py"
    "${LH_DSL_RUNTIME_SOURCE}/src/lh_compiler/compiler.py"
    DESTINATION "${LH_DSL_RUNTIME_INSTALL}/src/lh_compiler"
)

install(DIRECTORY
    "${LH_DSL_RUNTIME_SOURCE}/src/lh_compiler/backend"
    "${LH_DSL_RUNTIME_SOURCE}/src/lh_compiler/frontend"
    "${LH_DSL_RUNTIME_SOURCE}/src/lh_compiler/function_blocks"
    DESTINATION "${LH_DSL_RUNTIME_INSTALL}/src/lh_compiler"
    FILES_MATCHING
    PATTERN "*.py"
    PATTERN "__pycache__" EXCLUDE
    PATTERN "*.pyc" EXCLUDE
    PATTERN "tests" EXCLUDE
    PATTERN "test_*" EXCLUDE
)

if(WIN32 AND LH_ENABLE_QT_DEPLOYMENT)
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/DeployQt.cmake")
endif()

message(STATUS "========================================")
message(STATUS "  项目配置完成")
message(STATUS "  Qt版本: ${Qt5_VERSION}")
message(STATUS "  编译器: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "========================================")
