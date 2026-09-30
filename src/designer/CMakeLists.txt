# src/designer/CMakeLists.txt
# 设计器模块构建配置

add_library(designer_module STATIC
    MainWindow.cpp
    MainWindowMonitor.cpp
    MainWindowOutput.cpp
    MainWindowUi.cpp
    MainWindowInspector.cpp
    MainWindowExplorer.cpp
    MainWindowWorkspace.cpp
    DownloadDockWidget.cpp
    DeviceWorkspaceWidget.cpp
    ProjectExplorerWidget.cpp
    ProgramBlocksWidget.cpp
    DslScriptEditor.cpp
    DslSyntaxHighlighter.cpp
    DslCompletionEngine.cpp
    DslDragDropHandler.cpp
    SnippetRepository.cpp
    SettingsDialog.cpp
    OpcServerSettingsDialog.cpp
    OutputPaneController.cpp
    MonitorController.cpp
    ProjectController.cpp
    ProjectConfigValidation.cpp
    BuildController.cpp
    SettingsController.cpp
    ../diagnostics/DiagnosticSnapshotService.cpp
    ui/ThemeManager.cpp
    ui/GlobalStatusBar.cpp
    ui/InspectorPanel.cpp
    ParameterTuningPanel.cpp
    ParameterTuningWindow.cpp
    RuntimeSessionController.cpp
    RuntimeMonitorAdapter.cpp
    RuntimeSessionDownload.cpp
    ProfileResolution.cpp
    RuntimeSessionDebug.cpp
    RuntimeSessionOpc.cpp
    ui/ProblemsPanel.cpp
    ui/DiagnosticParser.cpp
    ui/CommandPaletteDialog.cpp
)

target_link_libraries(designer_module
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    Qt5::Charts
    compiler_module
    monitor
    ServoValveComm
    project_command_coordinator
    runtime_session_service
)

target_include_directories(designer_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LX_PUBLIC_INCLUDE_DIRS}
)

target_include_directories(designer_module PRIVATE ${LX_PRIVATE_INCLUDE_DIRS})
