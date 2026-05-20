# tests/CMakeLists.txt

# Basic environment test
add_executable(dummy_test dummy_test.cpp)
target_link_libraries(dummy_test
    Qt5::Core
)
target_include_directories(dummy_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

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
target_include_directories(monitor_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

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
target_include_directories(snippet_repository_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/designer
)

# MonitorDataProcessor tests
add_executable(monitor_data_processor_test monitor_data_processor_test.cpp)
target_link_libraries(monitor_data_processor_test
    Qt5::Core
    Qt5::Test
    monitor
)
target_include_directories(monitor_data_processor_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/monitor
)

# TaskScheduler tests
add_executable(task_scheduler_test task_scheduler_test.cpp)
target_link_libraries(task_scheduler_test
    Qt5::Core
    core_module
)
target_include_directories(task_scheduler_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# DataManager tests
add_executable(data_manager_test data_manager_test.cpp)
target_link_libraries(data_manager_test
    Qt5::Core
    Qt5::Sql
    core_module
)
target_include_directories(data_manager_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

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
target_include_directories(dsl_completion_engine_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/designer
)

# Legacy DSL compatibility probe
add_executable(dsl_legacy_compile_probe dsl_legacy_compile_probe.cpp)
target_link_libraries(dsl_legacy_compile_probe
    Qt5::Core
    compiler_module
)
target_include_directories(dsl_legacy_compile_probe PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Monitor export tests
add_executable(monitor_export_test monitor_export_test.cpp)
target_link_libraries(monitor_export_test
    Qt5::Core
    Qt5::Test
    Qt5::Svg
    monitor
)
target_include_directories(monitor_export_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/monitor
)

# ProtocolDetector offline tests
add_executable(protocol_detector_test protocol_detector_test.cpp)
target_link_libraries(protocol_detector_test
    Qt5::Core
    Qt5::Test
    ServoValveComm
)
target_include_directories(protocol_detector_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Register tests
add_test(NAME DummyTest COMMAND dummy_test)
add_test(NAME MonitorTest COMMAND monitor_test)
add_test(NAME TaskSchedulerTest COMMAND task_scheduler_test)
add_test(NAME DataManagerTest COMMAND data_manager_test)
add_test(NAME SnippetRepositoryTest COMMAND snippet_repository_test)
add_test(NAME MonitorDataProcessorTest COMMAND monitor_data_processor_test)
add_test(NAME DslCompletionEngineTest COMMAND dsl_completion_engine_test)
add_test(NAME LegacyDslCompileProbe COMMAND dsl_legacy_compile_probe)
add_test(NAME MonitorExportTest COMMAND monitor_export_test)
add_test(NAME ProtocolDetectorTest COMMAND protocol_detector_test)
