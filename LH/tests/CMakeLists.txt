# tests/CMakeLists.txt

set(LH_TEST_DEFAULT_INCLUDE_DIRS
    ${LX_PUBLIC_INCLUDE_DIRS}
)

function(configure_lh_test target)
    target_include_directories(${target} PRIVATE
        ${LH_TEST_DEFAULT_INCLUDE_DIRS}
        ${ARGN}
    )
endfunction()

function(register_lh_test name target)
    add_test(NAME ${name} COMMAND ${target})
endfunction()

# 仅在显式测试配置中启用 BuildController 发布失败注入；生产默认关闭。
option(LH_ENABLE_TEST_FAILURE_INJECTION
       "Enable deterministic test-only artifact publish failure injection"
       OFF)
if (LH_ENABLE_TEST_FAILURE_INJECTION AND TARGET designer_module)
    target_compile_definitions(designer_module PRIVATE LH_BUILD_CONTROLLER_TESTING)
endif()

# Basic environment test
add_executable(dummy_test dummy_test.cpp)
target_link_libraries(dummy_test
    Qt5::Core
)
configure_lh_test(dummy_test)

# Monitor module tests
add_executable(monitor_test monitor_test.cpp)
target_link_libraries(monitor_test
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Charts
    Qt5::Sql
    monitor
    core_module
)
configure_lh_test(monitor_test)

# SnippetRepository + completion tests
add_executable(snippet_repository_test
    snippet_repository_test.cpp
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)
target_link_libraries(snippet_repository_test
    Qt5::Core
    Qt5::Test
    designer_module
)
configure_lh_test(snippet_repository_test
    ${CMAKE_SOURCE_DIR}/src/designer
)

# MonitorDataProcessor tests
add_executable(monitor_data_processor_test monitor_data_processor_test.cpp)
target_link_libraries(monitor_data_processor_test
    Qt5::Core
    Qt5::Test
    monitor
)
configure_lh_test(monitor_data_processor_test
    ${CMAKE_SOURCE_DIR}/src/monitor
)

# MonitorDataLogger + SQLite persistence tests
add_executable(monitor_data_logger_test monitor_data_logger_test.cpp)
target_link_libraries(monitor_data_logger_test
    Qt5::Core
    Qt5::Test
    Qt5::Sql
    monitor
    core_module
)
configure_lh_test(monitor_data_logger_test
    ${CMAKE_SOURCE_DIR}/src/monitor
)

# MonitorManager backend switch regression test
add_executable(monitor_manager_backend_test monitor_manager_backend_test.cpp)
target_link_libraries(monitor_manager_backend_test
    Qt5::Core
    Qt5::Test
    monitor
    ServoValveComm
    core_module
)
configure_lh_test(monitor_manager_backend_test
    ${CMAKE_SOURCE_DIR}/src/monitor
)

# MainWindow integration tests
add_executable(main_window_integration_test
    main_window_integration_test.cpp
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)
target_link_libraries(main_window_integration_test
    Qt5::Core
    Qt5::Test
    Qt5::Widgets
    Qt5::Charts
    Qt5::Svg
    designer_module
    compiler_module
    ServoValveComm
    monitor
    core_module
)
if (LH_ENABLE_TEST_FAILURE_INJECTION)
    target_compile_definitions(main_window_integration_test PRIVATE LH_BUILD_CONTROLLER_TESTING)
endif()
configure_lh_test(main_window_integration_test
    ${CMAKE_SOURCE_DIR}/src/designer
)

# DownloadManager worker 生命周期测试
add_executable(download_manager_lifecycle_test download_manager_lifecycle_test.cpp)
target_link_libraries(download_manager_lifecycle_test
    Qt5::Core
    Qt5::Test
    ServoValveComm
)
configure_lh_test(download_manager_lifecycle_test
    ${CMAKE_SOURCE_DIR}/src/communication
)

# TaskScheduler tests
add_executable(task_scheduler_test task_scheduler_test.cpp)
target_link_libraries(task_scheduler_test
    Qt5::Core
    core_module
)
configure_lh_test(task_scheduler_test)

# DataManager tests
add_executable(data_manager_test data_manager_test.cpp)
target_link_libraries(data_manager_test
    Qt5::Core
    Qt5::Sql
    core_module
)
configure_lh_test(data_manager_test)

# Core test implementations kept as independent QTest executables
add_executable(core_task_scheduler_test
    ${CMAKE_SOURCE_DIR}/src/core/tests/TaskSchedulerTest.cpp
)
target_link_libraries(core_task_scheduler_test
    Qt5::Core
    Qt5::Test
    core_module
)
configure_lh_test(core_task_scheduler_test)

add_executable(core_data_manager_test
    ${CMAKE_SOURCE_DIR}/src/core/tests/DataManagerTest.cpp
)
target_link_libraries(core_data_manager_test
    Qt5::Core
    Qt5::Test
    Qt5::Sql
    core_module
)
configure_lh_test(core_data_manager_test)

# DataManager keyset/cursor history paging tests
add_executable(data_manager_history_paging_test data_manager_history_paging_test.cpp)
target_link_libraries(data_manager_history_paging_test
    Qt5::Core
    Qt5::Test
    Qt5::Sql
    core_module
)
configure_lh_test(data_manager_history_paging_test)

# DslCompletionEngine integration test
add_executable(dsl_completion_engine_test
    dsl_completion_engine_test.cpp
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)
target_link_libraries(dsl_completion_engine_test
    Qt5::Core
    Qt5::Test
    designer_module
)
configure_lh_test(dsl_completion_engine_test
    ${CMAKE_SOURCE_DIR}/src/designer
)

add_executable(dsl_script_editor_save_test
    dsl_script_editor_save_test.cpp
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)
target_link_libraries(dsl_script_editor_save_test
    Qt5::Core
    Qt5::Test
    Qt5::Widgets
    designer_module
)
configure_lh_test(dsl_script_editor_save_test
    ${CMAKE_SOURCE_DIR}/src/designer
)

add_executable(project_save_close_test
    project_save_close_test.cpp
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)
target_link_libraries(project_save_close_test
    Qt5::Core
    Qt5::Test
    Qt5::Widgets
    Qt5::Charts
    Qt5::Svg
    designer_module
    compiler_module
    ServoValveComm
    monitor
    core_module
)
configure_lh_test(project_save_close_test
    ${CMAKE_SOURCE_DIR}/src/designer
)

# Legacy DSL compatibility probe
add_executable(dsl_legacy_compile_probe dsl_legacy_compile_probe.cpp)
target_link_libraries(dsl_legacy_compile_probe
    Qt5::Core
    compiler_module
)
configure_lh_test(dsl_legacy_compile_probe)

# Python DSL compiler semantic contract test
add_executable(dsl_compiler_semantics_test dsl_compiler_semantics_test.cpp)
target_link_libraries(dsl_compiler_semantics_test
    Qt5::Core
    compiler_module
)
configure_lh_test(dsl_compiler_semantics_test)

# Monitor export tests
add_executable(monitor_export_test monitor_export_test.cpp)
target_link_libraries(monitor_export_test
    Qt5::Core
    Qt5::Test
    Qt5::Svg
    monitor
)
configure_lh_test(monitor_export_test
    ${CMAKE_SOURCE_DIR}/src/monitor
)

# Monitor history paging + export integration contract test
add_executable(monitor_history_export_integration_test
    monitor_history_export_integration_test.cpp
)
target_link_libraries(monitor_history_export_integration_test
    Qt5::Core
    Qt5::Test
    Qt5::Sql
    monitor
    core_module
)
configure_lh_test(monitor_history_export_integration_test
    ${CMAKE_SOURCE_DIR}/src/monitor
)

# ProtocolDetector offline tests
add_executable(protocol_detector_test protocol_detector_test.cpp)
target_link_libraries(protocol_detector_test
    Qt5::Core
    Qt5::Test
    ServoValveComm
)
configure_lh_test(protocol_detector_test)

# Communication configuration routing tests
add_executable(communication_routing_test communication_routing_test.cpp)
target_link_libraries(communication_routing_test
    Qt5::Core
    Qt5::Test
    ServoValveComm
)
configure_lh_test(communication_routing_test)

# Serial receive buffer and reentrancy tests
add_executable(serial_interface_test serial_interface_test.cpp)
target_link_libraries(serial_interface_test
    Qt5::Core
    Qt5::Test
    ServoValveComm
)
configure_lh_test(serial_interface_test)

# Controller debug protocol tests
add_executable(controller_debug_protocol_test controller_debug_protocol_test.cpp)
target_link_libraries(controller_debug_protocol_test
    Qt5::Core
    Qt5::Test
    ServoValveComm
)
configure_lh_test(controller_debug_protocol_test)

# Controller device backend tests
add_executable(controller_device_backend_test controller_device_backend_test.cpp)
target_link_libraries(controller_device_backend_test
    Qt5::Core
    Qt5::Test
    ServoValveComm
)
configure_lh_test(controller_device_backend_test)

# RuntimePointTable tests
add_executable(runtime_point_table_test runtime_point_table_test.cpp)
target_link_libraries(runtime_point_table_test
    Qt5::Core
    Qt5::Test
)
configure_lh_test(runtime_point_table_test)

# VirtualDeviceBackend tests
add_executable(virtual_device_backend_test virtual_device_backend_test.cpp)
target_link_libraries(virtual_device_backend_test
    Qt5::Core
    Qt5::Test
    ServoValveComm
)
configure_lh_test(virtual_device_backend_test)

# ParameterController tests
# 只链接需要的源文件，避免拉入 designer_module 全部依赖
add_executable(parameter_controller_test
    parameter_controller_test.cpp
    ${CMAKE_SOURCE_DIR}/src/designer/ParameterController.cpp
)
target_link_libraries(parameter_controller_test
    Qt5::Core
    Qt5::Test
    ServoValveComm
    monitor
    core_module
)
configure_lh_test(parameter_controller_test
    ${CMAKE_SOURCE_DIR}/src/designer
)

# RuntimeSessionController tests
# 链接 designer_module 整体，测试只调用不依赖 UI 的状态机方法
add_executable(runtime_session_controller_test
    runtime_session_controller_test.cpp
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)
target_link_libraries(runtime_session_controller_test
    Qt5::Core
    Qt5::Test
    Qt5::Widgets
    Qt5::Charts
    Qt5::Svg
    designer_module
    compiler_module
    ServoValveComm
    monitor
    core_module
)
configure_lh_test(runtime_session_controller_test
    ${CMAKE_SOURCE_DIR}/src/designer
)

# OPC server config tests
add_executable(opc_server_config_test opc_server_config_test.cpp)
target_link_libraries(opc_server_config_test
    Qt5::Core
    Qt5::Test
    ServoValveComm
)
configure_lh_test(opc_server_config_test)

# Diagnostic snapshot tests
add_executable(diagnostic_snapshot_test
    diagnostic_snapshot_test.cpp
    ${CMAKE_SOURCE_DIR}/src/diagnostics/DiagnosticSnapshotService.cpp
)
target_link_libraries(diagnostic_snapshot_test
    Qt5::Core
    Qt5::Test
)
configure_lh_test(diagnostic_snapshot_test
    ${CMAKE_SOURCE_DIR}/src/diagnostics
)

# Register tests
register_lh_test(DummyTest dummy_test)
register_lh_test(MonitorTest monitor_test)
register_lh_test(TaskSchedulerTest task_scheduler_test)
register_lh_test(DataManagerTest data_manager_test)
register_lh_test(CoreTaskSchedulerTest core_task_scheduler_test)
register_lh_test(CoreDataManagerTest core_data_manager_test)
register_lh_test(DataManagerHistoryPagingTest data_manager_history_paging_test)
register_lh_test(SnippetRepositoryTest snippet_repository_test)
register_lh_test(MonitorDataProcessorTest monitor_data_processor_test)
register_lh_test(MonitorDataLoggerTest monitor_data_logger_test)
register_lh_test(MonitorManagerBackendTest monitor_manager_backend_test)
register_lh_test(MainWindowIntegrationTest main_window_integration_test)
register_lh_test(DownloadManagerLifecycleTest download_manager_lifecycle_test)
register_lh_test(DslCompletionEngineTest dsl_completion_engine_test)
register_lh_test(DslScriptEditorSaveTest dsl_script_editor_save_test)
register_lh_test(ProjectSaveCloseTest project_save_close_test)
register_lh_test(LegacyDslCompileProbe dsl_legacy_compile_probe)
register_lh_test(DslCompilerSemanticsTest dsl_compiler_semantics_test)
set_tests_properties(DslCompilerSemanticsTest PROPERTIES SKIP_RETURN_CODE 77)
register_lh_test(MonitorExportTest monitor_export_test)
register_lh_test(MonitorHistoryExportIntegrationTest monitor_history_export_integration_test)
register_lh_test(ProtocolDetectorTest protocol_detector_test)
register_lh_test(CommunicationRoutingTest communication_routing_test)
register_lh_test(SerialInterfaceTest serial_interface_test)
set_tests_properties(SerialInterfaceTest PROPERTIES TIMEOUT 10)
register_lh_test(ControllerDebugProtocolTest controller_debug_protocol_test)
register_lh_test(ControllerDeviceBackendTest controller_device_backend_test)
register_lh_test(RuntimePointTableTest runtime_point_table_test)
register_lh_test(VirtualDeviceBackendTest virtual_device_backend_test)
register_lh_test(ParameterControllerTest parameter_controller_test)
register_lh_test(RuntimeSessionControllerTest runtime_session_controller_test)
register_lh_test(OpcServerConfigTest opc_server_config_test)
register_lh_test(DiagnosticSnapshotTest diagnostic_snapshot_test)

# 安装布局回归测试：前缀位于构建目录，不污染源码目录；兼容单配置和多配置生成器。
add_test(NAME InstallLayoutTest
    COMMAND ${CMAKE_COMMAND}
        "-DLH_INSTALL_BINARY_DIR=${CMAKE_BINARY_DIR}"
        "-DLH_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/install_layout_test_prefix"
        "-DLH_INSTALL_EXECUTABLE_NAME=$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        "-DLH_INSTALL_CONFIG=$<CONFIG>"
        "-DLH_ENABLE_QT_DEPLOYMENT=${LH_ENABLE_QT_DEPLOYMENT}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/install_layout_test.cmake"
)
