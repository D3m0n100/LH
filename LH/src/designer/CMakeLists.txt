# src/designer/CMakeLists.txt
# 设计器模块构建配置

add_library(designer_module STATIC
    MainWindow.cpp
    DownloadDockWidget.cpp
    ProjectExplorerWidget.cpp
    ProgramBlocksWidget.cpp
    DslScriptEditor.cpp
    DslSyntaxHighlighter.cpp
    DslCompletionEngine.cpp
    DslDragDropHandler.cpp
    SnippetRepository.cpp
    SettingsDialog.cpp
    OutputPaneController.cpp
    MonitorController.cpp
    ProjectController.cpp
    BuildController.cpp
    RunController.cpp
    SettingsController.cpp
    ui/ThemeManager.cpp
    ui/GlobalStatusBar.cpp
    ui/InspectorPanel.cpp
    ParameterTuningWindow.cpp
    ui/ProblemsPanel.cpp
)

target_link_libraries(designer_module
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    Qt5::Charts
    compiler_module
    monitor
    ServoValveComm
)

target_include_directories(designer_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/monitor
    ${CMAKE_SOURCE_DIR}/include
)
