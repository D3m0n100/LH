/**
 * @file MainWindowUi.cpp
 * @brief MainWindow UI construction helpers.
 */

#include "MainWindow.h"

#include "DownloadDockWidget.h"
#include "MonitorWidget.h"
#include "ParameterTuningWindow.h"
#include "ProgramBlocksWidget.h"
#include "ProjectController.h"
#include "ProjectExplorerWidget.h"
#include "ui/GlobalStatusBar.h"
#include "ui/InspectorPanel.h"
#include "ui/ProblemsPanel.h"

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

void MainWindow::createMenus()
{
    QMenu* fileMenu = menuBar()->addMenu("文件(&F)");

    m_actNew = fileMenu->addAction("新建项目(&N)");
    m_actNew->setShortcut(QKeySequence("Ctrl+N"));
    connect(m_actNew, &QAction::triggered, this, &MainWindow::onNewProject);

    m_actOpen = fileMenu->addAction("打开项目(&O)...");
    m_actOpen->setShortcut(QKeySequence("Ctrl+O"));
    connect(m_actOpen, &QAction::triggered, this, &MainWindow::onOpenProject);

    m_recentProjectsMenu = fileMenu->addMenu("最近项目(&R)");

    fileMenu->addSeparator();

    m_actSave = fileMenu->addAction("保存(&S)");
    m_actSave->setShortcut(QKeySequence("Ctrl+S"));
    connect(m_actSave, &QAction::triggered, this, &MainWindow::onSaveProject);

    QAction* actSaveAll = fileMenu->addAction("全部保存");
    connect(actSaveAll, &QAction::triggered, this, &MainWindow::onSaveAll);

    fileMenu->addSeparator();

    QAction* actClose = fileMenu->addAction("关闭项目");
    connect(actClose, &QAction::triggered, this, &MainWindow::onCloseProject);

    fileMenu->addSeparator();

    QAction* actExit = fileMenu->addAction("退出(&Q)");
    connect(actExit, &QAction::triggered, this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu("编辑(&E)");

    m_actUndo = editMenu->addAction("撤销(&U)");
    m_actUndo->setShortcut(QKeySequence("Ctrl+Z"));
    connect(m_actUndo, &QAction::triggered, this, &MainWindow::onUndo);

    m_actRedo = editMenu->addAction("重做(&R)");
    m_actRedo->setShortcut(QKeySequence("Ctrl+Y"));
    connect(m_actRedo, &QAction::triggered, this, &MainWindow::onRedo);

    editMenu->addSeparator();

    m_actCut = editMenu->addAction("剪切(&T)");
    m_actCut->setShortcut(QKeySequence("Ctrl+X"));
    connect(m_actCut, &QAction::triggered, this, &MainWindow::onCut);

    m_actCopy = editMenu->addAction("复制(&C)");
    m_actCopy->setShortcut(QKeySequence("Ctrl+C"));
    connect(m_actCopy, &QAction::triggered, this, &MainWindow::onCopy);

    m_actPaste = editMenu->addAction("粘贴(&P)");
    m_actPaste->setShortcut(QKeySequence("Ctrl+V"));
    connect(m_actPaste, &QAction::triggered, this, &MainWindow::onPaste);

    editMenu->addSeparator();

    m_actSelectAll = editMenu->addAction("全选(&A)");
    m_actSelectAll->setShortcut(QKeySequence("Ctrl+A"));
    connect(m_actSelectAll, &QAction::triggered, this, &MainWindow::onSelectAll);

    editMenu->addSeparator();

    m_actFind = editMenu->addAction("查找(&F)...");
    m_actFind->setShortcut(QKeySequence("Ctrl+F"));
    connect(m_actFind, &QAction::triggered, this, &MainWindow::onFind);

    updateEditActions();

    QMenu* viewMenu = menuBar()->addMenu("视图(&V)");

    m_actToggleDslEditor = viewMenu->addAction("LH编辑器(&D)");
    m_actToggleDslEditor->setCheckable(true);
    m_actToggleDslEditor->setChecked(true);
    m_actToggleDslEditor->setShortcut(QKeySequence("Ctrl+D"));
    m_actToggleDslEditor->setToolTip("显示或隐藏 LH 编辑器窗口");
    connect(m_actToggleDslEditor, &QAction::toggled, this, &MainWindow::onToggleDslEditor);

    viewMenu->addSeparator();

    m_actToggleExplorerDock = viewMenu->addAction("项目浏览器(&E)");
    m_actToggleExplorerDock->setCheckable(true);
    m_actToggleExplorerDock->setChecked(true);
    m_actToggleExplorerDock->setToolTip("显示或隐藏项目浏览器");
    connect(m_actToggleExplorerDock, &QAction::toggled, this, &MainWindow::onToggleExplorerDock);

    m_actToggleFunctionList = viewMenu->addAction("函数列表(&L)");
    m_actToggleFunctionList->setCheckable(true);
    m_actToggleFunctionList->setChecked(false);
    connect(m_actToggleFunctionList, &QAction::toggled, this, &MainWindow::onToggleFunctionList);

    viewMenu->addSeparator();

    m_actToggleOutputDock = viewMenu->addAction("输出面板");
    m_actToggleOutputDock->setCheckable(true);
    m_actToggleOutputDock->setChecked(true);
    connect(m_actToggleOutputDock, &QAction::toggled, this, &MainWindow::onToggleOutputDock);

    m_actToggleMonitorDock = viewMenu->addAction("监控工作区");
    m_actToggleMonitorDock->setCheckable(true);
    m_actToggleMonitorDock->setChecked(false);
    connect(m_actToggleMonitorDock, &QAction::toggled, this, &MainWindow::onToggleMonitorDock);

    m_actToggleDownloadDock = viewMenu->addAction("构建/下载工作区");
    m_actToggleDownloadDock->setCheckable(true);
    m_actToggleDownloadDock->setChecked(false);
    connect(m_actToggleDownloadDock, &QAction::toggled, this, &MainWindow::onToggleDownloadDock);

    viewMenu->addSeparator();

    m_actClearOutput = viewMenu->addAction("清空输出(&C)");
    m_actClearOutput->setShortcut(QKeySequence("Ctrl+Shift+C"));
    connect(m_actClearOutput, &QAction::triggered, this, &MainWindow::onClearOutput);

    m_actOpenDisplayWorkspace = viewMenu->addAction("显示屏工作区(&S)");
    m_actOpenDisplayWorkspace->setShortcut(QKeySequence("Ctrl+Shift+D"));
    connect(m_actOpenDisplayWorkspace, &QAction::triggered, this, [this]() {
        if (m_workspaceTabs && m_workspaceDisplayPage) {
            m_workspaceTabs->setCurrentWidget(m_workspaceDisplayPage);
        }
    });

    viewMenu->addSeparator();

    m_actResetLayout = viewMenu->addAction("重置布局");
    connect(m_actResetLayout, &QAction::triggered, this, &MainWindow::onResetLayout);

    QMenu* buildMenu = menuBar()->addMenu("构建(&B)");

    m_actCompileConfig = buildMenu->addAction("编译LH (F7)");
    m_actCompileConfig->setShortcut(Qt::Key_F7);
    connect(m_actCompileConfig, &QAction::triggered, this, &MainWindow::onCompileConfiguration);

    m_actCompileParameters = buildMenu->addAction("编译参数");
    connect(m_actCompileParameters, &QAction::triggered, this, &MainWindow::onCompileParameters);

    m_actCompileCommunication = buildMenu->addAction("编译通信");
    connect(m_actCompileCommunication, &QAction::triggered, this, &MainWindow::onCompileCommunication);

    m_actCompileAndRunProject = buildMenu->addAction("编译并运行(&R)");
    m_actCompileAndRunProject->setShortcut(QKeySequence("F8"));
    m_actCompileAndRunProject->setToolTip("先编译当前工程，再在成功后立即运行 (F8)");
    connect(m_actCompileAndRunProject, &QAction::triggered, this, &MainWindow::onCompileAndRunProject);

    QMenu* runMenu = menuBar()->addMenu("运行(&R)");

    m_actRunProject = runMenu->addAction("运行项目 (F9)");
    m_actRunProject->setShortcut(Qt::Key_F9);
    connect(m_actRunProject, &QAction::triggered, this, &MainWindow::onRunProject);

    m_actStopProject = runMenu->addAction("停止项目 (Shift+F9)");
    m_actStopProject->setShortcut(QKeySequence("Shift+F9"));
    connect(m_actStopProject, &QAction::triggered, this, &MainWindow::onStopProject);

    m_actTestControllerConnection = runMenu->addAction("测试控制器连接");
    m_actTestControllerConnection->setToolTip("按当前运行配置测试控制器和目标侧连接");
    connect(m_actTestControllerConnection, &QAction::triggered, this, &MainWindow::onTestControllerConnection);

    runMenu->addSeparator();

    m_actPauseController = runMenu->addAction("暂停控制器");
    connect(m_actPauseController, &QAction::triggered, this, &MainWindow::onPauseController);

    m_actResumeController = runMenu->addAction("继续控制器");
    connect(m_actResumeController, &QAction::triggered, this, &MainWindow::onResumeController);

    m_actStepController = runMenu->addAction("单步执行");
    m_actStepController->setShortcut(QKeySequence("F10"));
    connect(m_actStepController, &QAction::triggered, this, &MainWindow::onStepController);

    m_actRunToCursor = runMenu->addAction("运行到光标");
    m_actRunToCursor->setShortcut(QKeySequence("Ctrl+F10"));
    connect(m_actRunToCursor, &QAction::triggered, this, &MainWindow::onRunControllerToCursor);

    QMenu* monitorMenu = menuBar()->addMenu("监控(&M)");

    m_actOpenMonitor = monitorMenu->addAction("打开监控");
    m_actOpenMonitor->setShortcut(QKeySequence("Ctrl+M"));
    connect(m_actOpenMonitor, &QAction::triggered, this, &MainWindow::onOpenMonitor);

    QAction* actParameterTuning = monitorMenu->addAction("调参窗口");
    actParameterTuning->setShortcut(QKeySequence("Ctrl+Shift+M"));
    actParameterTuning->setToolTip("打开独立调参窗口");
    connect(actParameterTuning, &QAction::triggered, this, &MainWindow::onOpenParameterTuningWindow);

    monitorMenu->addSeparator();

    m_actStartMonitor = monitorMenu->addAction("开始监控");
    m_actStartMonitor->setShortcut(QKeySequence("F5"));
    connect(m_actStartMonitor, &QAction::triggered, this, &MainWindow::onStartMonitoring);

    m_actStopMonitor = monitorMenu->addAction("停止监控");
    m_actStopMonitor->setShortcut(QKeySequence("Shift+F5"));
    connect(m_actStopMonitor, &QAction::triggered, this, &MainWindow::onStopMonitoring);

    monitorMenu->addSeparator();

    m_actExportMonitorData = monitorMenu->addAction("导出监控数据");
    connect(m_actExportMonitorData, &QAction::triggered, this, &MainWindow::onExportMonitorData);

    m_actExportMonitorImage = monitorMenu->addAction("导出监控图像");
    m_actExportMonitorImage->setToolTip("将当前监控图导出为 PNG");
    connect(m_actExportMonitorImage, &QAction::triggered, this, &MainWindow::onExportMonitorImage);

    QMenu* toolsMenu = menuBar()->addMenu("工具(&T)");

    QAction* actOpenLogDir = toolsMenu->addAction("打开日志目录");
    connect(actOpenLogDir, &QAction::triggered, this, &MainWindow::onOpenLogDirectory);

    toolsMenu->addSeparator();

    QAction* actDiagnosis = toolsMenu->addAction("诊断向导");
    actDiagnosis->setToolTip("打开问题面板并查看快速诊断摘要");
    connect(actDiagnosis, &QAction::triggered, this, &MainWindow::onOpenDiagnosisWizard);

    toolsMenu->addSeparator();

    m_actSettings = toolsMenu->addAction("选项(&O)...");
    m_actSettings->setShortcut(QKeySequence("Ctrl+,"));
    connect(m_actSettings, &QAction::triggered, this, &MainWindow::onOpenSettings);

    m_actOpcServerSettings = toolsMenu->addAction("OPC 服务设置...");
    connect(m_actOpcServerSettings, &QAction::triggered, this, &MainWindow::onOpenOpcServerSettings);

    QMenu* helpMenu = menuBar()->addMenu("帮助(&H)");
    QAction* actAbout = helpMenu->addAction("关于...");
    connect(actAbout, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::createToolBars()
{
    auto makeAction = [this](const QString& iconPath, const QString& text,
                             const QString& tooltip, const QKeySequence& shortcut = QKeySequence()) {
        auto* action = new QAction(QIcon(iconPath), text, this);
        action->setToolTip(tooltip);
        if (!shortcut.isEmpty()) {
            action->setShortcut(shortcut);
        }
        return action;
    };

    m_overviewToolBar = addToolBar("总览");
    m_overviewToolBar->setMovable(false);
    m_overviewToolBar->setObjectName("OverviewToolBar");
    m_overviewToolBar->setIconSize(QSize(16, 16));
    m_globalStatusBar = new GlobalStatusBar(this);
    m_overviewToolBar->addWidget(m_globalStatusBar);
    addToolBarBreak();

    m_fileToolBar = addToolBar("文件");
    m_fileToolBar->setMovable(false);
    m_fileToolBar->setObjectName("FileToolBar");
    m_fileToolBar->setIconSize(QSize(18, 18));
    m_fileToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    QAction* actNew = makeAction(":/icons/new.svg", "新建", "新建项目 (Ctrl+N)", QKeySequence("Ctrl+N"));
    connect(actNew, &QAction::triggered, this, &MainWindow::onNewProject);
    m_fileToolBar->addAction(actNew);

    QAction* actOpen = makeAction(":/icons/open.svg", "打开", "打开项目 (Ctrl+O)", QKeySequence("Ctrl+O"));
    connect(actOpen, &QAction::triggered, this, &MainWindow::onOpenProject);
    m_fileToolBar->addAction(actOpen);

    QAction* actSave = makeAction(":/icons/save.svg", "保存", "保存项目 (Ctrl+S)", QKeySequence("Ctrl+S"));
    connect(actSave, &QAction::triggered, this, &MainWindow::onSaveProject);
    m_fileToolBar->addAction(actSave);

    m_fileToolBar->addSeparator();

    m_actCompile = makeAction(":/icons/compile.svg", "编译", "编译 LH (F7)", QKeySequence(Qt::Key_F7));
    connect(m_actCompile, &QAction::triggered, this, &MainWindow::onCompileConfiguration);
    m_fileToolBar->addAction(m_actCompile);

    m_actOpenDownload = makeAction(":/icons/download.svg", "下载", "打开下载工作区");
    connect(m_actOpenDownload, &QAction::triggered, this, &MainWindow::onOpenDownloadWindow);
    m_fileToolBar->addAction(m_actOpenDownload);

    m_fileToolBar->addSeparator();

    m_actOpenDslEditorToolBar = makeAction(":/icons/output.svg", "LH", "打开 LH 编辑器 (Ctrl+D)", QKeySequence("Ctrl+D"));
    m_actOpenDslEditorToolBar->setToolTip("打开 LH 编辑器 (Ctrl+D)");
    m_actOpenDslEditorToolBar->setCheckable(true);
    m_actOpenDslEditorToolBar->setChecked(true);
    connect(m_actOpenDslEditorToolBar, &QAction::toggled, this, &MainWindow::onToggleDslEditor);
    m_fileToolBar->addAction(m_actOpenDslEditorToolBar);

    m_runToolBar = addToolBar("运行");
    m_runToolBar->setMovable(false);
    m_runToolBar->setObjectName("RunToolBar");
    m_runToolBar->setIconSize(QSize(18, 18));
    m_runToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    QAction* actRun = makeAction(":/icons/run.svg", "运行", "运行项目 (F9)", QKeySequence(Qt::Key_F9));
    connect(actRun, &QAction::triggered, this, &MainWindow::onRunProject);
    m_runToolBar->addAction(actRun);

    QAction* actStop = makeAction(":/icons/stop.svg", "停止", "停止项目 (Shift+F9)", QKeySequence("Shift+F9"));
    connect(actStop, &QAction::triggered, this, &MainWindow::onStopProject);
    m_runToolBar->addAction(actStop);

    QAction* actParameterTuning = makeAction(":/icons/settings.svg", "调参", "打开独立调参窗口 (Ctrl+Shift+M)", QKeySequence("Ctrl+Shift+M"));
    connect(actParameterTuning, &QAction::triggered, this, &MainWindow::onOpenParameterTuningWindow);
    m_runToolBar->addAction(actParameterTuning);

    QAction* actSettings = makeAction(":/icons/settings.svg", "选项", "打开选项 (Ctrl+,)", QKeySequence("Ctrl+,"));
    connect(actSettings, &QAction::triggered, this, &MainWindow::onOpenSettings);
    m_runToolBar->addAction(actSettings);

    m_runToolBar->addSeparator();

    QAction* actCompileAndRun = makeAction(":/icons/compile.svg", "编译并运行", "先编译再运行 (F8)", QKeySequence("F8"));
    connect(actCompileAndRun, &QAction::triggered, this, &MainWindow::onCompileAndRunProject);
    m_runToolBar->addAction(actCompileAndRun);

    QAction* actMonitor = makeAction(":/icons/monitor.svg", "监控", "打开监控工作区 (Ctrl+M)", QKeySequence("Ctrl+M"));
    connect(actMonitor, &QAction::triggered, this, &MainWindow::onOpenMonitor);
    m_runToolBar->addAction(actMonitor);

    QAction* actTestConnection = makeAction(":/icons/monitor.svg", "连接", "测试控制器连接");
    connect(actTestConnection, &QAction::triggered, this, &MainWindow::onTestControllerConnection);
    m_runToolBar->addAction(actTestConnection);

    auto* debugMenu = new QMenu(QStringLiteral("调试"), m_runToolBar);
    debugMenu->addAction(m_actPauseController);
    debugMenu->addAction(m_actStepController);
    debugMenu->addAction(m_actRunToCursor);

    QAction* actDebug = makeAction(":/icons/run.svg", "调试", "暂停、单步和运行到光标");
    actDebug->setMenu(debugMenu);
    m_runToolBar->addAction(actDebug);
    if (auto* debugButton = qobject_cast<QToolButton*>(m_runToolBar->widgetForAction(actDebug))) {
        debugButton->setPopupMode(QToolButton::InstantPopup);
    }
}

void MainWindow::createStatusBar()
{
    m_statusLabel = new QLabel(this);
    m_statusLabel->setMinimumWidth(220);

    m_editorPositionLabel = new QLabel(this);
    m_editorPositionLabel->setText("行 1, 列 1");
    m_editorPositionLabel->setMinimumWidth(180);
    m_editorPositionLabel->setAlignment(Qt::AlignCenter);

    m_connectionStatusLabel = new QLabel(this);
    m_connectionStatusLabel->setObjectName("ConnectionStatusLabel");
    m_connectionStatusLabel->setText("未连接");
    m_connectionStatusLabel->setProperty("connected", false);
    m_connectionStatusLabel->setMinimumWidth(100);
    m_connectionStatusLabel->setAlignment(Qt::AlignCenter);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(false);
    m_progressBar->setFixedWidth(150);

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_editorPositionLabel);
    statusBar()->addPermanentWidget(m_connectionStatusLabel);
    statusBar()->addPermanentWidget(m_progressBar);
}

void MainWindow::createDockWidgets()
{
    m_workspaceTabs = new QTabWidget(this);
    m_workspaceTabs->setObjectName("WorkspaceTabs");
    setCentralWidget(m_workspaceTabs);

    m_workspaceDslPage = new QWidget(m_workspaceTabs);
    auto* dslLayout = new QVBoxLayout(m_workspaceDslPage);
    dslLayout->setContentsMargins(0, 0, 0, 0);
    dslLayout->setSpacing(0);
    m_mdiArea = new QMdiArea(m_workspaceDslPage);
    m_mdiArea->setViewMode(QMdiArea::TabbedView);
    m_mdiArea->setTabsClosable(true);
    m_mdiArea->setTabsMovable(true);
    dslLayout->addWidget(m_mdiArea);
    m_workspaceTabs->addTab(m_workspaceDslPage, "LH工作区");

    createDslEditorSubWindow();

    m_workspaceBuildPage = new QWidget(m_workspaceTabs);
    auto* buildLayout = new QVBoxLayout(m_workspaceBuildPage);
    buildLayout->setContentsMargins(8, 8, 8, 8);
    buildLayout->setSpacing(8);
    m_downloadWidget = new DownloadDockWidget(m_workspaceBuildPage);
    buildLayout->addWidget(m_downloadWidget);
    m_workspaceTabs->addTab(m_workspaceBuildPage, "构建与下载");

    m_workspaceMonitorPage = new QWidget(m_workspaceTabs);
    auto* monitorLayout = new QVBoxLayout(m_workspaceMonitorPage);
    monitorLayout->setContentsMargins(8, 8, 8, 8);
    monitorLayout->setSpacing(8);
    m_monitorWidget = new MonitorWidget(m_workspaceMonitorPage);
    monitorLayout->addWidget(m_monitorWidget);
    m_workspaceTabs->addTab(m_workspaceMonitorPage, "监控");

    m_workspaceDisplayPage = new QWidget(m_workspaceTabs);
    auto* displayLayout = new QVBoxLayout(m_workspaceDisplayPage);
    displayLayout->setContentsMargins(8, 8, 8, 8);
    displayLayout->setSpacing(8);
    auto* displayHint = new QLabel("显示屏工作区：这里先复用显示类函数块列表，后续可扩展为 HMI 画面设计器。", m_workspaceDisplayPage);
    displayHint->setWordWrap(true);
    displayHint->setStyleSheet("QLabel { color: #57606a; padding: 4px 2px; }");
    displayLayout->addWidget(displayHint);
    m_displayBlocksWidget = new ProgramBlocksWidget(m_workspaceDisplayPage);
    if (m_dslEditor) {
        m_displayBlocksWidget->setCompletionEngine(m_dslEditor->completionEngine());
    }
    displayLayout->addWidget(m_displayBlocksWidget, 1);
    m_workspaceTabs->addTab(m_workspaceDisplayPage, "显示屏");

    m_explorerDock = new QDockWidget("项目浏览器", this);
    m_explorerDock->setObjectName("ExplorerDock");
    m_explorerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_explorerDock->setFeatures(QDockWidget::DockWidgetMovable |
                                QDockWidget::DockWidgetClosable);

    m_projectExplorerWidget = new ProjectExplorerWidget(m_explorerDock);
    m_explorerDock->setWidget(m_projectExplorerWidget);
    addDockWidget(Qt::LeftDockWidgetArea, m_explorerDock);

    refreshExplorerRoot();

    connect(m_projectExplorerWidget, &ProjectExplorerWidget::fileOpenRequested,
            this, &MainWindow::onExplorerFileOpenRequested);
    connect(m_projectExplorerWidget, &ProjectExplorerWidget::locateCurrentFileRequested,
            this, &MainWindow::onLocateCurrentFileInExplorer);

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
    m_outputViewer->setReadOnly(true);
    m_outputViewer->setLineWrapMode(QTextEdit::NoWrap);
    m_outputViewer->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_outputViewer, &QTextEdit::customContextMenuRequested,
            this, &MainWindow::onOutputContextMenu);
    m_bottomPanels->addTab(m_outputViewer, "输出");

    connect(m_problemsPanel, &ProblemsPanel::problemCountChanged, this, [this](int count) {
        m_alarmCount = count;
        if (m_globalStatusBar) {
            m_globalStatusBar->setAlarmCount(count);
        }
    });

    m_logDock->setWidget(m_bottomPanels);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    m_logDock->setMinimumHeight(200);
    if (m_workspaceTabs) {
        resizeDocks({m_logDock}, {260}, Qt::Vertical);
    }

    connect(m_logDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_actToggleOutputDock) {
            m_actToggleOutputDock->blockSignals(true);
            m_actToggleOutputDock->setChecked(visible);
            m_actToggleOutputDock->blockSignals(false);
        }
    });

    m_monitorDock = nullptr;
    m_downloadDock = nullptr;

    connect(m_workspaceTabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (!m_workspaceTabs || !m_inspectorPanel) {
            return;
        }
        m_inspectorPanel->setWorkspaceName(m_workspaceTabs->tabText(index));
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
    m_inspectorDock->setMinimumWidth(280);

    m_inspectorPanel = new InspectorPanel(m_inspectorDock);
    m_inspectorPanel->setMinimumWidth(260);
    m_inspectorPanel->setPanelMode(InspectorPanel::PanelMode::Inspection);
    m_inspectorDock->setWidget(m_inspectorPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_inspectorDock);
    resizeDocks({m_inspectorDock}, {320}, Qt::Horizontal);

    connect(m_inspectorPanel, &InspectorPanel::requestCompile,
            this, &MainWindow::onCompileConfiguration);
    connect(m_inspectorPanel, &InspectorPanel::requestRun,
            this, &MainWindow::onRunProject);
    connect(m_inspectorPanel, &InspectorPanel::requestOpenMonitor,
            this, &MainWindow::onOpenMonitor);
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
}

void MainWindow::createDslEditorSubWindow()
{
    m_dslEditor = new DslScriptEditor(this);
    if (m_displayBlocksWidget) {
        m_displayBlocksWidget->setCompletionEngine(m_dslEditor->completionEngine());
    }

    const bool functionListVisible = (m_actToggleFunctionList ? m_actToggleFunctionList->isChecked() : false);
    m_dslEditor->setFunctionListVisible(functionListVisible);

    m_projectController->setDslEditor(m_dslEditor);

    m_editorSubWindow = m_mdiArea->addSubWindow(m_dslEditor);
    m_editorSubWindow->setWindowTitle("LH脚本编辑器");
    m_editorSubWindow->showMaximized();

    connect(m_editorSubWindow, &QObject::destroyed,
            this, &MainWindow::onDslEditorSubWindowDestroyed);

    connectDslEditorSignals();

    appendOutput(QString("[%1] LH 脚本编辑器已打开")
                 .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
}

void MainWindow::connectDslEditorSignals()
{
    if (!m_dslEditor) {
        return;
    }

    connect(m_dslEditor, &DslScriptEditor::cursorPositionChanged,
            this, &MainWindow::onEditorCursorPositionChanged);
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
