/**
 * @file MainWindowUi.cpp
 * @brief MainWindow UI construction helpers.
 */

#include "MainWindow.h"

#include "DeviceWorkspaceWidget.h"
#include "DownloadDockWidget.h"
#include "MonitorWidget.h"
#include "ParameterTuningPanel.h"
#include "ParameterTuningWindow.h"
#include "ProgramBlocksWidget.h"
#include "ProjectController.h"
#include "ProjectExplorerWidget.h"
#include "ui/GlobalStatusBar.h"
#include "ui/InspectorPanel.h"
#include "ui/ProblemsPanel.h"
#include "OutputPaneController.h"
#include "Common.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDockWidget>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMenuBar>
#include <QProgressBar>
#include <QSize>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

void MainWindow::createActions()
{
    auto makeSharedAction = [this](const QString& objectName, const QString& iconPath,
                                   const QString& text, const QString& tooltip,
                                   const QKeySequence& shortcut = QKeySequence()) {
        auto* action = new QAction(this);
        action->setObjectName(objectName);
        action->setText(text);
        action->setToolTip(tooltip);
        action->setShortcutContext(Qt::WindowShortcut);
        if (!iconPath.isEmpty()) {
            action->setIcon(QIcon(iconPath));
        }
        if (!shortcut.isEmpty()) {
            action->setShortcut(shortcut);
        }
        return action;
    };

    // 文件操作
    m_actNew = makeSharedAction("actNew", ":/icons/new.svg", "新建项目(&N)", "新建项目 (Ctrl+N)", QKeySequence("Ctrl+N"));
    connect(m_actNew, &QAction::triggered, this, &MainWindow::onNewProject);

    m_actOpen = makeSharedAction("actOpen", ":/icons/open.svg", "打开项目(&O)...", "打开项目 (Ctrl+O)", QKeySequence("Ctrl+O"));
    connect(m_actOpen, &QAction::triggered, this, &MainWindow::onOpenProject);

    m_actSave = makeSharedAction("actSave", ":/icons/save.svg", "保存(&S)", "保存项目 (Ctrl+S)", QKeySequence("Ctrl+S"));
    m_actSave->setIconText("保存");
    connect(m_actSave, &QAction::triggered, this, &MainWindow::onSaveProject);

    m_actSaveAll = makeSharedAction("actSaveAll", "", "全部保存(&L)", "保存所有修改 (Ctrl+Shift+S)", QKeySequence("Ctrl+Shift+S"));
    connect(m_actSaveAll, &QAction::triggered, this, &MainWindow::onSaveAll);

    m_actCloseActiveTab = makeSharedAction("actCloseActiveTab", "", "关闭当前标签(&W)", "关闭当前标签页 (Ctrl+W)", QKeySequence("Ctrl+W"));
    connect(m_actCloseActiveTab, &QAction::triggered, this, &MainWindow::closeCurrentActiveTab);

    m_actCloseProject = makeSharedAction("actCloseProject", "", "关闭项目", "关闭当前项目");
    connect(m_actCloseProject, &QAction::triggered, this, &MainWindow::onCloseProject);

    m_actExit = makeSharedAction("actExit", "", "退出(&Q)", "退出程序");
    connect(m_actExit, &QAction::triggered, this, &QWidget::close);

    // 编辑操作
    m_actUndo = makeSharedAction("actUndo", "", "撤销(&U)", "撤销 (Ctrl+Z)", QKeySequence("Ctrl+Z"));
    connect(m_actUndo, &QAction::triggered, this, &MainWindow::onUndo);

    m_actRedo = makeSharedAction("actRedo", "", "重做(&R)", "重做 (Ctrl+Y)", QKeySequence("Ctrl+Y"));
    connect(m_actRedo, &QAction::triggered, this, &MainWindow::onRedo);

    m_actCut = makeSharedAction("actCut", "", "剪切(&T)", "剪切 (Ctrl+X)", QKeySequence("Ctrl+X"));
    connect(m_actCut, &QAction::triggered, this, &MainWindow::onCut);

    m_actCopy = makeSharedAction("actCopy", "", "复制(&C)", "复制 (Ctrl+C)", QKeySequence("Ctrl+C"));
    connect(m_actCopy, &QAction::triggered, this, &MainWindow::onCopy);

    m_actPaste = makeSharedAction("actPaste", "", "粘贴(&P)", "粘贴 (Ctrl+V)", QKeySequence("Ctrl+V"));
    connect(m_actPaste, &QAction::triggered, this, &MainWindow::onPaste);

    m_actSelectAll = makeSharedAction("actSelectAll", "", "全选(&A)", "全选 (Ctrl+A)", QKeySequence("Ctrl+A"));
    connect(m_actSelectAll, &QAction::triggered, this, &MainWindow::onSelectAll);

    m_actFind = makeSharedAction("actFind", "", "查找(&F)...", "查找 (Ctrl+F)", QKeySequence("Ctrl+F"));
    connect(m_actFind, &QAction::triggered, this, &MainWindow::onFind);

    m_actGotoLine = makeSharedAction("actGotoLine", "", "转到行(&G)...", "转到指定行 (Ctrl+G)", QKeySequence("Ctrl+G"));
    connect(m_actGotoLine, &QAction::triggered, this, &MainWindow::openGotoLine);

    // 视图操作
    m_actCommandPalette = makeSharedAction("actCommandPalette", "", "命令面板...", "显示所有命令 (Ctrl+Shift+P / F1)");
    m_actCommandPalette->setShortcuts({QKeySequence("Ctrl+Shift+P"), QKeySequence(Qt::Key_F1)});
    connect(m_actCommandPalette, &QAction::triggered, this, &MainWindow::openCommandPalette);

    m_actQuickOpen = makeSharedAction("actQuickOpen", "", "快速打开...", "按文件名快速打开 (Ctrl+P)", QKeySequence("Ctrl+P"));
    connect(m_actQuickOpen, &QAction::triggered, this, &MainWindow::openQuickOpen);

    m_actToggleSidebar = makeSharedAction("actToggleSidebar", "", "切换侧边栏(&B)", "显示或隐藏项目浏览器侧边栏 (Ctrl+B)", QKeySequence("Ctrl+B"));
    connect(m_actToggleSidebar, &QAction::triggered, this, [this]() {
        if (m_actToggleExplorerDock) {
            m_actToggleExplorerDock->toggle();
        }
    });

    m_actToggleBottomPanel = makeSharedAction("actToggleBottomPanel", "", "切换底部面板(&J)", "显示或隐藏输出与问题面板 (Ctrl+J)", QKeySequence("Ctrl+J"));
    connect(m_actToggleBottomPanel, &QAction::triggered, this, [this]() {
        if (m_actToggleOutputDock) {
            m_actToggleOutputDock->toggle();
        }
    });

    m_actToggleDslEditor = makeSharedAction("actToggleDslEditor", ":/icons/output.svg", "LH编辑器(&D)", "显示或隐藏 LH 编辑器窗口 (Ctrl+D)", QKeySequence("Ctrl+D"));
    m_actToggleDslEditor->setCheckable(true);
    m_actToggleDslEditor->setChecked(true);
    connect(m_actToggleDslEditor, &QAction::toggled, this, &MainWindow::onToggleDslEditor);
    m_actOpenDslEditorToolBar = m_actToggleDslEditor;

    m_actToggleExplorerDock = makeSharedAction("actToggleExplorerDock", "", "项目浏览器(&E)", "显示或隐藏项目浏览器");
    m_actToggleExplorerDock->setCheckable(true);
    m_actToggleExplorerDock->setChecked(true);
    connect(m_actToggleExplorerDock, &QAction::toggled, this, &MainWindow::onToggleExplorerDock);

    m_actToggleInspectorDock = makeSharedAction("actToggleInspectorDock", "", "检查面板(&I)", "显示或隐藏检查面板");
    m_actToggleInspectorDock->setCheckable(true);
    m_actToggleInspectorDock->setChecked(false);
    connect(m_actToggleInspectorDock, &QAction::toggled, this, &MainWindow::onToggleInspectorDock);

    m_actToggleFunctionList = makeSharedAction("actToggleFunctionList", "", "函数列表(&L)", "显示或隐藏函数列表");
    m_actToggleFunctionList->setCheckable(true);
    m_actToggleFunctionList->setChecked(false);
    connect(m_actToggleFunctionList, &QAction::toggled, this, &MainWindow::onToggleFunctionList);

    m_actToggleOutputDock = makeSharedAction("actToggleOutputDock", "", "输出面板(&O)", "显示或隐藏输出与问题面板");
    m_actToggleOutputDock->setCheckable(true);
    m_actToggleOutputDock->setChecked(false);
    connect(m_actToggleOutputDock, &QAction::toggled, this, &MainWindow::onToggleOutputDock);

    m_actToggleMonitorDock = makeSharedAction("actToggleMonitorDock", "", "监控工作区", "切换到监控工作区");
    m_actToggleMonitorDock->setCheckable(true);
    m_actToggleMonitorDock->setChecked(false);
    connect(m_actToggleMonitorDock, &QAction::toggled, this, &MainWindow::onToggleMonitorDock);

    m_actToggleDownloadDock = makeSharedAction("actToggleDownloadDock", "", "构建/下载工作区", "切换到构建/下载工作区");
    m_actToggleDownloadDock->setCheckable(true);
    m_actToggleDownloadDock->setChecked(false);
    connect(m_actToggleDownloadDock, &QAction::toggled, this, &MainWindow::onToggleDownloadDock);

    m_actClearOutput = makeSharedAction("actClearOutput", "", "清空输出(&C)", "清空输出 (Ctrl+Shift+C)", QKeySequence("Ctrl+Shift+C"));
    connect(m_actClearOutput, &QAction::triggered, this, &MainWindow::onClearOutput);

    m_actOpenDisplayWorkspace = makeSharedAction("actOpenDisplayWorkspace", "", "显示屏函数块(&S)", "打开显示类函数块 (Ctrl+Shift+D)", QKeySequence("Ctrl+Shift+D"));
    connect(m_actOpenDisplayWorkspace, &QAction::triggered, this, &MainWindow::onOpenDisplayBlocksWindow);

    m_actResetCurrentLayout = makeSharedAction("actResetLayout", "", "重置当前工作区布局", "重置当前工作区的界面布局到默认状态");
    connect(m_actResetCurrentLayout, &QAction::triggered, this, &MainWindow::onResetCurrentWorkspaceLayout);
    m_actResetLayout = m_actResetCurrentLayout;

    m_actResetAllLayouts = makeSharedAction("actResetAllLayouts", "", "重置全部工作区布局", "重置所有工作区的界面布局到默认状态");
    connect(m_actResetAllLayouts, &QAction::triggered, this, &MainWindow::onResetAllWorkspaceLayouts);

    // 构建操作
    m_actCompile = makeSharedAction("actCompile", ":/icons/compile.svg", "编译LH (F7)", "编译 LH (F7)", QKeySequence(Qt::Key_F7));
    m_actCompile->setIconText("编译LH");
    connect(m_actCompile, &QAction::triggered, this, &MainWindow::onCompileConfiguration);
    m_actCompileConfig = m_actCompile;

    m_actCompileParameters = makeSharedAction("actCompileParameters", "", "编译参数", "编译参数配置");
    connect(m_actCompileParameters, &QAction::triggered, this, &MainWindow::onCompileParameters);

    m_actCompileCommunication = makeSharedAction("actCompileCommunication", "", "编译通信", "编译通信配置");
    connect(m_actCompileCommunication, &QAction::triggered, this, &MainWindow::onCompileCommunication);

    m_actCompileAndRunProject = makeSharedAction("actCompileAndRun", ":/icons/compile.svg", "编译并运行(&R)", "先编译当前工程，再在成功后立即运行 (F8)", QKeySequence("F8"));
    connect(m_actCompileAndRunProject, &QAction::triggered, this, &MainWindow::onCompileAndRunProject);

    // 控制器运行操作（按规范严格命名为“运行控制器”与“停止控制器”）
    m_actRunProject = makeSharedAction("actRunProject", ":/icons/run.svg", "运行控制器", "运行控制器 (F9)", QKeySequence(Qt::Key_F9));
    m_actRunProject->setIconText("运行控制器");
    connect(m_actRunProject, &QAction::triggered, this, &MainWindow::onRunProject);

    m_actStopProject = makeSharedAction("actStopProject", ":/icons/stop.svg", "停止控制器", "停止控制器 (Shift+F9)", QKeySequence("Shift+F9"));
    m_actStopProject->setIconText("停止控制器");
    connect(m_actStopProject, &QAction::triggered, this, &MainWindow::onStopProject);

    m_actTestControllerConnection = makeSharedAction("actTestConnection", ":/icons/monitor.svg", "测试连接", "按当前运行配置测试控制器连接");
    connect(m_actTestControllerConnection, &QAction::triggered, this, &MainWindow::onTestControllerConnection);

    m_actPauseController = makeSharedAction("actPauseController", "", "暂停控制器", "暂停控制器执行");
    m_actPauseController->setEnabled(false);
    connect(m_actPauseController, &QAction::triggered, this, &MainWindow::onPauseController);

    m_actResumeController = makeSharedAction("actResumeController", "", "继续控制器", "继续控制器执行");
    m_actResumeController->setEnabled(false);
    connect(m_actResumeController, &QAction::triggered, this, &MainWindow::onResumeController);

    m_actStepController = makeSharedAction("actStepController", "", "单步执行", "单步执行 (F10)", QKeySequence("F10"));
    m_actStepController->setEnabled(false);
    connect(m_actStepController, &QAction::triggered, this, &MainWindow::onStepController);

    m_actRunToCursor = makeSharedAction("actRunToCursor", "", "运行到光标", "运行到光标 (Ctrl+F10)", QKeySequence("Ctrl+F10"));
    connect(m_actRunToCursor, &QAction::triggered, this, &MainWindow::onRunControllerToCursor);

    // 监控操作（按规范严格命名为“开始监控”与“停止监控”）
    m_actOpenMonitor = makeSharedAction("actOpenMonitor", ":/icons/monitor.svg", "监控工作区", "打开监控工作区 (Ctrl+M)", QKeySequence("Ctrl+M"));
    connect(m_actOpenMonitor, &QAction::triggered, this, &MainWindow::onOpenMonitor);

    m_actStartMonitor = makeSharedAction("actStartMonitor", ":/icons/run.svg", "开始监控", "开始监控 (F5)", QKeySequence("F5"));
    connect(m_actStartMonitor, &QAction::triggered, this, &MainWindow::onStartMonitoring);

    m_actStopMonitor = makeSharedAction("actStopMonitor", ":/icons/stop.svg", "停止监控", "停止监控 (Shift+F5)", QKeySequence("Shift+F5"));
    connect(m_actStopMonitor, &QAction::triggered, this, &MainWindow::onStopMonitoring);

    m_actExportMonitorData = makeSharedAction("actExportMonitorData", "", "导出监控数据", "导出监控数据");
    connect(m_actExportMonitorData, &QAction::triggered, this, &MainWindow::onExportMonitorData);

    m_actExportMonitorImage = makeSharedAction("actExportMonitorImage", "", "导出监控图像", "将当前监控图导出为图像");
    connect(m_actExportMonitorImage, &QAction::triggered, this, &MainWindow::onExportMonitorImage);

    m_actParameterTuning = makeSharedAction("actParameterTuning", ":/icons/settings.svg", "调参窗口", "打开独立调参窗口 (Ctrl+Shift+M)", QKeySequence("Ctrl+Shift+M"));
    connect(m_actParameterTuning, &QAction::triggered, this, &MainWindow::onOpenParameterTuningWindow);

    // 工具操作
    m_actOpenDownload = makeSharedAction("actOpenDownload", ":/icons/download.svg", "专家诊断下载", "打开构建与诊断下载（专家模式）");
    connect(m_actOpenDownload, &QAction::triggered, this, &MainWindow::onOpenDownloadWindow);

    m_actOpenLogDir = makeSharedAction("actOpenLogDir", "", "打开日志目录", "打开应用程序日志目录");
    connect(m_actOpenLogDir, &QAction::triggered, this, &MainWindow::onOpenLogDirectory);

    m_actDiagnosis = makeSharedAction("actDiagnosis", "", "诊断向导", "打开问题面板并查看快速诊断摘要");
    connect(m_actDiagnosis, &QAction::triggered, this, &MainWindow::onOpenDiagnosisWizard);

    m_actSettings = makeSharedAction("actSettings", ":/icons/settings.svg", "选项(&O)...", "打开选项设置 (Ctrl+,)", QKeySequence("Ctrl+,"));
    connect(m_actSettings, &QAction::triggered, this, &MainWindow::onOpenSettings);

    m_actOpcServerSettings = makeSharedAction("actOpcServerSettings", "", "OPC 服务设置...", "配置 OPC DA 服务参数");
    connect(m_actOpcServerSettings, &QAction::triggered, this, &MainWindow::onOpenOpcServerSettings);

    m_actAbout = makeSharedAction("actAbout", "", "关于...", "关于本软件");
    connect(m_actAbout, &QAction::triggered, this, &MainWindow::onAbout);

    addAction(m_actSaveAll);
    addAction(m_actCloseActiveTab);
    addAction(m_actCommandPalette);
    addAction(m_actQuickOpen);
    addAction(m_actGotoLine);
    addAction(m_actToggleSidebar);
    addAction(m_actToggleBottomPanel);
}

void MainWindow::createMenus()
{
    QMenu* fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction(m_actNew);
    fileMenu->addAction(m_actOpen);
    m_recentProjectsMenu = fileMenu->addMenu("最近项目(&R)");
    fileMenu->addSeparator();
    fileMenu->addAction(m_actSave);
    fileMenu->addAction(m_actSaveAll);
    fileMenu->addAction(m_actCloseActiveTab);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actCloseProject);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actExit);

    QMenu* editMenu = menuBar()->addMenu("编辑(&E)");
    editMenu->addAction(m_actUndo);
    editMenu->addAction(m_actRedo);
    editMenu->addSeparator();
    editMenu->addAction(m_actCut);
    editMenu->addAction(m_actCopy);
    editMenu->addAction(m_actPaste);
    editMenu->addSeparator();
    editMenu->addAction(m_actSelectAll);
    editMenu->addSeparator();
    editMenu->addAction(m_actFind);
    editMenu->addAction(m_actGotoLine);
    updateEditActions();

    QMenu* viewMenu = menuBar()->addMenu("视图(&V)");
    viewMenu->addAction(m_actCommandPalette);
    viewMenu->addAction(m_actQuickOpen);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actToggleDslEditor);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actToggleSidebar);
    viewMenu->addAction(m_actToggleBottomPanel);
    viewMenu->addAction(m_actToggleExplorerDock);
    viewMenu->addAction(m_actToggleInspectorDock);
    viewMenu->addAction(m_actToggleFunctionList);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actToggleOutputDock);
    viewMenu->addAction(m_actToggleMonitorDock);
    viewMenu->addAction(m_actToggleDownloadDock);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actClearOutput);
    viewMenu->addAction(m_actOpenDisplayWorkspace);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actResetCurrentLayout);
    viewMenu->addAction(m_actResetAllLayouts);

    QMenu* buildMenu = menuBar()->addMenu("构建(&B)");
    buildMenu->addAction(m_actCompile);
    buildMenu->addAction(m_actCompileParameters);
    buildMenu->addAction(m_actCompileCommunication);
    buildMenu->addAction(m_actCompileAndRunProject);

    QMenu* runMenu = menuBar()->addMenu("运行(&R)");
    runMenu->addAction(m_actRunProject);
    runMenu->addAction(m_actStopProject);
    runMenu->addAction(m_actTestControllerConnection);
    runMenu->addSeparator();
    runMenu->addAction(m_actPauseController);
    runMenu->addAction(m_actResumeController);
    runMenu->addAction(m_actStepController);
    runMenu->addAction(m_actRunToCursor);

    QMenu* monitorMenu = menuBar()->addMenu("监控(&M)");
    monitorMenu->addAction(m_actOpenMonitor);
    monitorMenu->addAction(m_actParameterTuning);
    monitorMenu->addSeparator();
    monitorMenu->addAction(m_actStartMonitor);
    monitorMenu->addAction(m_actStopMonitor);
    monitorMenu->addSeparator();
    monitorMenu->addAction(m_actExportMonitorData);
    monitorMenu->addAction(m_actExportMonitorImage);

    QMenu* toolsMenu = menuBar()->addMenu("工具(&T)");
    toolsMenu->addAction(m_actOpenLogDir);
    toolsMenu->addSeparator();
    toolsMenu->addAction(m_actDiagnosis);
    toolsMenu->addSeparator();
    toolsMenu->addAction(m_actSettings);
    toolsMenu->addAction(m_actOpcServerSettings);

    QMenu* helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction(m_actAbout);
}

void MainWindow::createToolBars()
{
    m_mainToolBar = addToolBar("主工具栏");
    m_mainToolBar->setMovable(false);
    m_mainToolBar->setObjectName("MainToolBar");
    m_mainToolBar->setIconSize(QSize(18, 18));
    m_mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    updateToolBarForWorkspace(WorkspaceId::Programming);
}

void MainWindow::updateToolBarForWorkspace(const QString& workspaceId)
{
    if (!m_mainToolBar) {
        return;
    }

    m_mainToolBar->clear();

    if (workspaceId == WorkspaceId::Monitor) {
        // 监控工作区：开始监控、停止监控、调参窗口、运行控制器、停止控制器 (5 个常驻)
        if (m_actStartMonitor) m_mainToolBar->addAction(m_actStartMonitor);
        if (m_actStopMonitor) m_mainToolBar->addAction(m_actStopMonitor);
        if (m_actParameterTuning) m_mainToolBar->addAction(m_actParameterTuning);
        if (m_actRunProject) m_mainToolBar->addAction(m_actRunProject);
        if (m_actStopProject) m_mainToolBar->addAction(m_actStopProject);
    } else if (workspaceId == WorkspaceId::Device) {
        // 设备工作区：测试连接、运行控制器、停止控制器、更多控制下拉 (4 个常驻)
        if (m_actTestControllerConnection) m_mainToolBar->addAction(m_actTestControllerConnection);
        if (m_actRunProject) m_mainToolBar->addAction(m_actRunProject);
        if (m_actStopProject) m_mainToolBar->addAction(m_actStopProject);

        auto* deviceMoreBtn = new QToolButton(m_mainToolBar);
        deviceMoreBtn->setObjectName(QStringLiteral("btnDeviceMoreOpsDropdown"));
        deviceMoreBtn->setText(tr("更多操作"));
        deviceMoreBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        deviceMoreBtn->setPopupMode(QToolButton::InstantPopup);
        auto* deviceMenu = new QMenu(deviceMoreBtn);
        if (m_actPauseController) deviceMenu->addAction(m_actPauseController);
        if (m_actResumeController) deviceMenu->addAction(m_actResumeController);
        if (m_actStepController) deviceMenu->addAction(m_actStepController);
        deviceMenu->addSeparator();
        if (m_actDiagnosis) deviceMenu->addAction(m_actDiagnosis);
        deviceMoreBtn->setMenu(deviceMenu);
        m_mainToolBar->addWidget(deviceMoreBtn);
    } else {
        // 编程工作区（默认）：保存、编译下拉、运行控制器、停止控制器 (4 个常驻)
        if (m_actSave) m_mainToolBar->addAction(m_actSave);

        auto* compileBtn = new QToolButton(m_mainToolBar);
        compileBtn->setObjectName(QStringLiteral("btnCompileDropdown"));
        compileBtn->setDefaultAction(m_actCompile);
        compileBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        compileBtn->setPopupMode(QToolButton::MenuButtonPopup);
        auto* compileMenu = new QMenu(compileBtn);
        if (m_actCompile) compileMenu->addAction(m_actCompile);
        if (m_actCompileParameters) compileMenu->addAction(m_actCompileParameters);
        if (m_actCompileCommunication) compileMenu->addAction(m_actCompileCommunication);
        if (m_actCompileAndRunProject) compileMenu->addAction(m_actCompileAndRunProject);
        compileBtn->setMenu(compileMenu);
        m_mainToolBar->addWidget(compileBtn);

        if (m_actRunProject) m_mainToolBar->addAction(m_actRunProject);
        if (m_actStopProject) m_mainToolBar->addAction(m_actStopProject);
    }
}

void MainWindow::createStatusBar()
{
    // U04: 将 GlobalStatusBar 调整为底部摘要承载控件，消除顶部与底部的重复连接状态
    m_globalStatusBar = new GlobalStatusBar(this);
    m_globalStatusBar->setObjectName("GlobalStatusBar");
    statusBar()->addWidget(m_globalStatusBar, 1);

    m_editorPositionLabel = new QLabel(this);
    m_editorPositionLabel->setObjectName("EditorPositionLabel");
    m_editorPositionLabel->setText(QStringLiteral("行 1, 列 1"));
    m_editorPositionLabel->setMinimumWidth(70);
    m_editorPositionLabel->setAlignment(Qt::AlignCenter);
    statusBar()->addPermanentWidget(m_editorPositionLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName("StatusBarProgressBar");
    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(false);
    m_progressBar->setFixedWidth(120);
    statusBar()->addPermanentWidget(m_progressBar);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("StatusMessageLabel");
    m_statusLabel->setVisible(false);
    m_connectionStatusLabel = nullptr;

    // 点击分别定位问题列表或监控告警
    connect(m_globalStatusBar, &GlobalStatusBar::problemClicked, this, [this]() {
        if (m_bottomPanels && m_problemsPanel) {
            m_bottomPanels->setCurrentWidget(m_problemsPanel);
        }
        if (m_logDock) {
            m_logDock->setVisible(true);
            if (m_actToggleOutputDock) {
                m_actToggleOutputDock->setChecked(true);
            }
        }
        if (m_problemsPanel) {
            m_problemsPanel->selectFirstError();
        }
    });

    connect(m_globalStatusBar, &GlobalStatusBar::alarmClicked, this, [this]() {
        switchToWorkspace(WorkspaceId::Monitor);
        if (m_monitorWidget) {
            m_monitorWidget->showAlarmTab();
        }
    });
}

namespace {
class WorkspaceTabWidget : public QTabWidget {
public:
    explicit WorkspaceTabWidget(QWidget* parent = nullptr) : QTabWidget(parent) {}
    QSize minimumSizeHint() const override {
        QSize barHint = tabBar() ? tabBar()->minimumSizeHint() : QSize(0, 0);
        QWidget* cur = currentWidget();
        if (!cur) {
            return barHint;
        }
        // Retain the active page's reasonable minimum size constraint while preventing
        // inactive tabs from propagating oversized dimensions.
        const QSize curMin = cur->minimumSizeHint();
        return QSize(qMax(barHint.width(), curMin.width()),
                     barHint.height() + curMin.height());
    }
};
} // namespace

void MainWindow::createDockWidgets()
{
    m_workspaceTabs = new WorkspaceTabWidget(this);
    m_workspaceTabs->setObjectName("WorkspaceTabs");
    // Pages must be allowed to follow a narrow logical viewport. Their child
    // controls remain reachable through the page scroll areas and docks.
    m_workspaceTabs->setMinimumSize(0, 0);
    setCentralWidget(m_workspaceTabs);

    // 1. 编程工作区
    m_workspaceDslPage = new QWidget(m_workspaceTabs);
    m_workspaceDslPage->setMinimumWidth(0);
    auto* dslLayout = new QVBoxLayout(m_workspaceDslPage);
    dslLayout->setContentsMargins(0, 0, 0, 0);
    dslLayout->setSpacing(0);
    m_mdiArea = new QMdiArea(m_workspaceDslPage);
    m_mdiArea->setMinimumSize(0, 0);
    m_mdiArea->setViewMode(QMdiArea::TabbedView);
    m_mdiArea->setTabsClosable(true);
    m_mdiArea->setTabsMovable(true);
    dslLayout->addWidget(m_mdiArea);
    m_workspaceTabs->addTab(m_workspaceDslPage, "编程");

    createDslEditorSubWindow();

    // 2. 监控工作区
    m_workspaceMonitorPage = new QWidget(m_workspaceTabs);
    m_workspaceMonitorPage->setMinimumWidth(0);
    auto* monitorLayout = new QVBoxLayout(m_workspaceMonitorPage);
    monitorLayout->setContentsMargins(6, 4, 6, 4);
    monitorLayout->setSpacing(4);
    m_monitorWidget = new MonitorWidget(m_workspaceMonitorPage);
    monitorLayout->addWidget(m_monitorWidget);
    m_workspaceTabs->addTab(m_workspaceMonitorPage, "监控");

    // 3. 设备工作区
    m_deviceWorkspaceWidget = new DeviceWorkspaceWidget(m_workspaceTabs);
    m_deviceWorkspaceWidget->setMinimumWidth(0);
    m_downloadWidget = m_deviceWorkspaceWidget->downloadWidget();
    m_workspaceBuildPage = m_deviceWorkspaceWidget;
    m_workspaceTabs->addTab(m_workspaceBuildPage, "设备");

    m_deviceWorkspaceWidget->bindActions(
        m_actTestControllerConnection,
        m_actRunProject,
        m_actStopProject,
        m_actPauseController,
        m_actResumeController,
        m_actStepController,
        m_actDiagnosis
    );

    if (m_deviceWorkspaceWidget->toggleExpertButton()) {
        connect(m_deviceWorkspaceWidget->toggleExpertButton(), &QPushButton::toggled,
                this, &MainWindow::updateDeviceWorkspaceInfo);
    }
    if (m_deviceWorkspaceWidget->downloadWidget()) {
        connect(m_deviceWorkspaceWidget->downloadWidget(), &DownloadDockWidget::targetConfigChanged,
                this, &MainWindow::updateDeviceWorkspaceInfo);
    }

    m_explorerDock = new QDockWidget("项目与函数库", this);
    m_explorerDock->setObjectName("ExplorerDock");
    m_explorerDock->setMinimumWidth(0);
    m_explorerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_explorerDock->setFeatures(QDockWidget::DockWidgetMovable |
                                QDockWidget::DockWidgetClosable);

    m_leftTabs = new QTabWidget(m_explorerDock);
    m_leftTabs->setMinimumWidth(0);
    m_leftTabs->setObjectName(QStringLiteral("LeftTabsWidget"));
    m_leftTabs->setDocumentMode(true);

    m_projectExplorerWidget = new ProjectExplorerWidget(m_leftTabs);
    m_projectExplorerWidget->setMinimumWidth(0);
    m_leftTabs->addTab(m_projectExplorerWidget, QStringLiteral("项目"));

    m_programBlocksWidget = new ProgramBlocksWidget(m_leftTabs);
    m_programBlocksWidget->setMinimumWidth(0);
    m_programBlocksWidget->setObjectName(QStringLiteral("ProgramBlocksWidget"));
    m_leftTabs->addTab(m_programBlocksWidget, QStringLiteral("函数库"));

    bindFunctionLibraryDataSource();

    m_explorerDock->setWidget(m_leftTabs);
    addDockWidget(Qt::LeftDockWidgetArea, m_explorerDock);
    resizeDocks({m_explorerDock}, {260}, Qt::Horizontal);

    refreshExplorerRoot();

    connect(m_projectExplorerWidget, &ProjectExplorerWidget::fileOpenRequested,
            this, &MainWindow::onExplorerFileOpenRequested);
    connect(m_projectExplorerWidget, &ProjectExplorerWidget::mainScriptRequested,
            this, [this](const QString& path) {
        if (m_projectController) m_projectController->setMainScriptFile(path);
    });
    connect(m_projectExplorerWidget, &ProjectExplorerWidget::deleteRequested,
            this, &MainWindow::deleteProjectDocumentPath);
    connect(m_projectExplorerWidget, &ProjectExplorerWidget::fileSelected,
            this, &MainWindow::onFileSelectedInExplorer);
    connect(m_projectExplorerWidget, &ProjectExplorerWidget::locateCurrentFileRequested,
            this, &MainWindow::onLocateCurrentFileInExplorer);

    connect(m_programBlocksWidget, &ProgramBlocksWidget::snippetDoubleClicked,
            this, [this](const FunctionSnippet& snippet) {
        if (m_dslEditor) {
            if (snippet.canInsert()) m_dslEditor->insertSnippet(snippet.templateCode);
        }
    });
    connect(m_programBlocksWidget, &ProgramBlocksWidget::snippetSelected,
            this, &MainWindow::onSnippetSelectedInBlocks);

    if (m_actToggleExplorerDock) {
        m_explorerDock->setVisible(m_actToggleExplorerDock->isChecked());
        connect(m_explorerDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            if (m_actToggleExplorerDock) {
                m_actToggleExplorerDock->blockSignals(true);
                m_actToggleExplorerDock->setChecked(visible);
                m_actToggleExplorerDock->blockSignals(false);
            }
        });
    }

    m_logDock = new QDockWidget("输出与问题", this);
    m_logDock->setObjectName("LogDock");
    m_logDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

    m_bottomPanels = new QTabWidget(m_logDock);
    m_bottomPanels->setObjectName("BottomPanels");

    m_problemsPanel = new ProblemsPanel(m_bottomPanels);
    m_bottomPanels->addTab(m_problemsPanel, "问题");

    m_outputViewer = new QTextEdit(m_bottomPanels);
    m_outputViewer->setObjectName(QStringLiteral("outputViewer"));
    m_outputViewer->setReadOnly(true);
    m_outputViewer->setLineWrapMode(QTextEdit::NoWrap);
    m_outputViewer->document()->setMaximumBlockCount(OutputPaneConfig::DEFAULT_MAX_BLOCK_COUNT);
    m_outputViewer->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_outputViewer, &QTextEdit::customContextMenuRequested,
            this, &MainWindow::onOutputContextMenu);
    m_bottomPanels->addTab(m_outputViewer, "输出");

    connect(m_problemsPanel, &ProblemsPanel::problemCountChanged, this, [this](int count) {
        if (m_globalStatusBar) {
            m_globalStatusBar->setProblemCount(count);
        }
    });

    connect(m_problemsPanel, &ProblemsPanel::diagnosticActivated,
            this, &MainWindow::onDiagnosticActivated);

    if (m_monitorWidget) {
        connect(m_monitorWidget, &MonitorWidget::alarmCountChanged, this, [this](int count) {
            m_alarmCount = count;
            if (m_globalStatusBar) {
                m_globalStatusBar->setAlarmCount(count);
            }
        });
        connect(m_monitorWidget, &MonitorWidget::requestOpenParameterTuning,
                this, &MainWindow::onOpenParameterTuningWindow);
    }

    m_logDock->setWidget(m_bottomPanels);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    m_logDock->setMinimumHeight(0);
    if (m_workspaceTabs) {
        resizeDocks({m_logDock}, {220}, Qt::Vertical);
    }
    m_logDock->setVisible(false);

    if (m_actToggleOutputDock) {
        m_logDock->setVisible(m_actToggleOutputDock->isChecked());
        connect(m_logDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            if (m_actToggleOutputDock) {
                m_actToggleOutputDock->blockSignals(true);
                m_actToggleOutputDock->setChecked(visible);
                m_actToggleOutputDock->blockSignals(false);
            }
        });
    }

    m_monitorDock = nullptr;
    m_downloadDock = nullptr;

    connect(m_workspaceTabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (m_switchingWorkspace) {
            return;
        }
        const QString targetId = workspaceIdForTabIndex(index);
        if (!targetId.isEmpty() && targetId != m_currentWorkspaceId) {
            switchToWorkspace(targetId);
        }
        if (m_workspaceTabs && m_inspectorPanel) {
            m_inspectorPanel->setWorkspaceName(m_workspaceTabs->tabText(index));
        }
    });
}

void MainWindow::createWorkspaceTabs()
{
    // Workspace tabs are initialized in createDockWidgets.
}

void MainWindow::createInspectorDock()
{
    m_inspectorDock = new QDockWidget("检查面板", this);
    m_inspectorDock->setObjectName("InspectorDock");
    m_inspectorDock->setStyleSheet(R"(
QDockWidget#InspectorDock {
    border: 1px solid #d0d7de;
}
QDockWidget#InspectorDock::title {
    border: 1px solid #d0d7de;
    border-bottom: none;
}
)");
    m_inspectorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_inspectorDock->setFeatures(QDockWidget::DockWidgetMovable |
                                 QDockWidget::DockWidgetClosable);
    m_inspectorDock->setMinimumWidth(200);

    m_inspectorPanel = new InspectorPanel(m_inspectorDock);
    m_inspectorPanel->setMinimumWidth(200);
    m_inspectorPanel->setPanelMode(InspectorPanel::PanelMode::Inspection);
    m_inspectorDock->setWidget(m_inspectorPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_inspectorDock);
    resizeDocks({m_inspectorDock}, {300}, Qt::Horizontal);
    m_inspectorDock->setVisible(false);

    if (m_actToggleInspectorDock) {
        m_inspectorDock->setVisible(m_actToggleInspectorDock->isChecked());
        connect(m_inspectorDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            if (m_actToggleInspectorDock) {
                m_actToggleInspectorDock->blockSignals(true);
                m_actToggleInspectorDock->setChecked(visible);
                m_actToggleInspectorDock->blockSignals(false);
            }
        });
    }

    connect(m_inspectorPanel, &InspectorPanel::requestCompile,
            this, &MainWindow::onCompileConfiguration);
    connect(m_inspectorPanel, &InspectorPanel::requestRun,
            this, &MainWindow::onRunProject);
    connect(m_inspectorPanel, &InspectorPanel::requestOpenMonitor,
            this, &MainWindow::onOpenMonitor);
}

void MainWindow::createTuningDock()
{
    if (m_tuningDock) {
        return;
    }

    m_parameterTuningPanel = new ParameterTuningPanel(this);
    m_parameterTuningPanel->setMinimumSize(0, 0);

    m_tuningDock = new QDockWidget(QStringLiteral("PID 调参"), this);
    m_tuningDock->setObjectName(QStringLiteral("ParameterTuningDock"));
    m_tuningDock->setStyleSheet(R"(
QDockWidget#ParameterTuningDock {
    border: 1px solid #d0d7de;
}
QDockWidget#ParameterTuningDock::title {
    border: 1px solid #d0d7de;
    border-bottom: none;
}
)");
    m_tuningDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    m_tuningDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    m_tuningDock->setMinimumSize(0, 0);
    m_tuningDock->setWidget(m_parameterTuningPanel);
    // Start at the bottom so a hidden tuning dock never contributes a large
    // horizontal minimum size. Wide windows are moved beside the workspace
    // when the dock is opened.
    addDockWidget(Qt::BottomDockWidgetArea, m_tuningDock);
    resizeDocks({m_tuningDock}, {340}, Qt::Vertical);
    m_tuningDock->setVisible(false);

    connect(m_parameterTuningPanel, &ParameterTuningPanel::requestCompile,
            this, &MainWindow::onCompileConfiguration);
    connect(m_parameterTuningPanel, &ParameterTuningPanel::requestRun,
            this, &MainWindow::onRunProject);
    connect(m_parameterTuningPanel, &ParameterTuningPanel::requestOpenMonitor,
            this, &MainWindow::onOpenMonitor);
    connect(m_parameterTuningPanel, &ParameterTuningPanel::requestEditParameter,
            this, &MainWindow::onEditParameterRequested);
    connect(m_parameterTuningPanel, &ParameterTuningPanel::requestApplyParameters,
            this, &MainWindow::onApplyParametersRequested);
    connect(m_parameterTuningPanel, &ParameterTuningPanel::requestPopOutWindow,
            this, &MainWindow::onPopOutParameterTuning);
    connect(m_parameterTuningPanel, &ParameterTuningPanel::requestDockBack,
            this, &MainWindow::onDockBackParameterTuning);
}

void MainWindow::createParameterTuningWindow()
{
    if (m_parameterTuningWindow) {
        return;
    }

    m_parameterTuningWindow = new ParameterTuningWindow(this);
    m_parameterTuningWindow->setWindowFlag(Qt::Tool, true);
    m_parameterTuningWindow->setAttribute(Qt::WA_QuitOnClose, false);

    connect(m_parameterTuningWindow, &ParameterTuningWindow::requestCompile,
            this, &MainWindow::onCompileConfiguration);
    connect(m_parameterTuningWindow, &ParameterTuningWindow::requestRun,
            this, &MainWindow::onRunProject);
    connect(m_parameterTuningWindow, &ParameterTuningWindow::requestOpenMonitor,
            this, &MainWindow::onOpenMonitor);
    connect(m_parameterTuningWindow, &ParameterTuningWindow::requestEditParameter,
            this, &MainWindow::onEditParameterRequested);
    connect(m_parameterTuningWindow, &ParameterTuningWindow::requestApplyParameters,
            this, &MainWindow::onApplyParametersRequested);
    connect(m_parameterTuningWindow, &ParameterTuningWindow::requestDockBack,
            this, &MainWindow::onDockBackParameterTuning);
}

void MainWindow::onPopOutParameterTuning()
{
    if (!m_parameterTuningPanel) {
        return;
    }
    if (!m_parameterTuningWindow) {
        createParameterTuningWindow();
    }
    if (m_tuningDock) {
        m_tuningDock->setWidget(nullptr);
        m_tuningDock->hide();
    }
    m_parameterTuningWindow->setTuningPanel(m_parameterTuningPanel);
    m_parameterTuningWindow->setMinimumSize(0, 0);
    m_parameterTuningWindow->resize(qMin(m_parameterTuningWindow->width(), width()),
                                    qMin(m_parameterTuningWindow->height(), height()));
    m_parameterTuningWindow->show();
    m_parameterTuningWindow->raise();
    m_parameterTuningWindow->activateWindow();
}

void MainWindow::onDockBackParameterTuning()
{
    if (!m_parameterTuningPanel) {
        return;
    }
    if (m_parameterTuningWindow) {
        m_parameterTuningWindow->detachTuningPanel();
        m_parameterTuningWindow->hide();
    }
    if (m_tuningDock) {
        m_parameterTuningPanel->setParent(m_tuningDock);
        m_parameterTuningPanel->setStandaloneMode(false);
        m_parameterTuningPanel->show();
        m_tuningDock->setWidget(m_parameterTuningPanel);
        if (dockWidgetArea(m_tuningDock) == Qt::BottomDockWidgetArea) {
            adjustBottomTuningDockHeight();
        }
        m_tuningDock->show();
        m_tuningDock->raise();
    }
}

void MainWindow::createDslEditorSubWindow()
{
    m_dslEditor = new DslScriptEditor(this);
    bindFunctionLibraryDataSource();

    const bool functionListVisible = (m_actToggleFunctionList ? m_actToggleFunctionList->isChecked() : false);
    m_dslEditor->setFunctionListVisible(functionListVisible);

    m_projectController->setDslEditor(m_dslEditor);

    m_editorSubWindow = m_mdiArea->addSubWindow(m_dslEditor);
    m_editorSubWindow->setAttribute(Qt::WA_DeleteOnClose, false);
    m_editorSubWindow->installEventFilter(this);
    m_editorSubWindow->setWindowTitle("LH脚本编辑器");
    m_editorSubWindow->showMaximized();

    connect(m_editorSubWindow, &QObject::destroyed,
            this, &MainWindow::onDslEditorSubWindowDestroyed);

    connectDslEditorSignals();

    appendOutput(QString("[%1] LH 脚本编辑器已打开")
                 .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
}

void MainWindow::bindFunctionLibraryDataSource()
{
    DslCompletionEngine* engine = m_dslEditor ? m_dslEditor->completionEngine() : nullptr;
    if (m_programBlocksWidget) {
        m_programBlocksWidget->setCompletionEngine(engine);
    }
    if (m_displayBlocksWidget) {
        m_displayBlocksWidget->setCompletionEngine(engine);
    }
}

void MainWindow::connectDslEditorSignals()
{
    if (!m_dslEditor) {
        return;
    }

    connect(m_dslEditor, &DslScriptEditor::cursorPositionChanged,
            this, &MainWindow::onEditorCursorPositionChanged);
    connect(m_dslEditor->editor(), &QPlainTextEdit::textChanged, this, [this]() {
        if (m_dslEditor) onDocumentModified(m_dslEditor->currentFilePath());
    });
    connect(m_dslEditor, &DslScriptEditor::editorModified,
            this, &MainWindow::onEditorModified);

    connect(m_dslEditor, &DslScriptEditor::snippetInserted,
            this, &MainWindow::onSnippetInserted);
    connect(m_dslEditor, &DslScriptEditor::dropError,
            this, &MainWindow::onDropError);

    m_dslEditor->setStatusCallback([this](const QString& msg) {
        updateStatusBar(msg);
    });
}

void MainWindow::initConnections()
{
    connect(qApp, &QApplication::focusChanged,
            this, &MainWindow::onFocusChanged);
}
