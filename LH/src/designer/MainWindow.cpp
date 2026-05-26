/**
 * @file MainWindow.cpp
 * @brief 主窗口类实现
 *
 * 重构说明：
 * - 项目管理逻辑已迁移到 ProjectController
 * - DSL 编译逻辑已迁移到 BuildController
 * - 应用设置逻辑已迁移到 SettingsController
 * - MainWindow 仅保留 UI 构建和信号转发逻辑
 */

#include "MainWindow.h"
#include "ProjectController.h"
#include "BuildController.h"
#include "RunController.h"
#include "SettingsController.h"
#include "MonitorWidget.h"
#include "DownloadDockWidget.h"
#include "ProjectExplorerWidget.h"
#include "ProgramBlocksWidget.h"
#include "MonitorManager.h"
#include "../communication/IDeviceBackend.h"
#include "SampleDataProvider.h"
#include "ui/ThemeManager.h"
#include "ui/GlobalStatusBar.h"
#include "ui/InspectorPanel.h"
#include "ui/ProblemsPanel.h"
#include "ParameterTuningWindow.h"
#include "ParameterController.h"
#include "RuntimeSessionController.h"
#include "Common.h"
#include "TextEncoding.h"

#include <QMdiArea>
#include <QMdiSubWindow>
#include <QPointer>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <algorithm>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QProgressBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QStyle>
#include <QIcon>
#include <QSize>
#include <QApplication>
#include <QClipboard>
#include <QScrollBar>
#include <QMenu>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <QStringList>
#include <cmath>

namespace {

bool containsSeparatedToken(const QString& text, const QString& token)
{
    if (text.isEmpty() || token.isEmpty()) {
        return false;
    }

    const QString lower = text.toLower();
    int index = lower.indexOf(token);
    while (index >= 0) {
        const int beforeIndex = index - 1;
        const int afterIndex = index + token.size();
        const bool beforeOk = beforeIndex < 0 || !lower.at(beforeIndex).isLetterOrNumber();
        const bool afterOk = afterIndex >= lower.size() || !lower.at(afterIndex).isLetterOrNumber();
        if (beforeOk && afterOk) {
            return true;
        }
        index = lower.indexOf(token, index + 1);
    }
    return false;
}

bool isPidParameter(const ParameterDefinition& parameter)
{
    const QString name = parameter.name.trimmed().toLower();
    const QString dataType = parameter.dataType.trimmed().toLower();
    const QString unit = parameter.unit.trimmed().toLower();
    const QString kind = parameter.metadata.value("kind").toString().trimmed().toLower();
    const QString role = parameter.metadata.value("role").toString().trimmed().toLower();
    const QString category = parameter.metadata.value("category").toString().trimmed().toLower();

    const QString combined = QStringList{ name, dataType, unit, kind, role, category }.join(' ');
    if (combined.contains("pid")) {
        return true;
    }

    if (containsSeparatedToken(combined, "kp") ||
        containsSeparatedToken(combined, "ki") ||
        containsSeparatedToken(combined, "kd")) {
        return true;
    }

    return false;
}

QList<ParameterDefinition> filterPidParameters(const QList<ParameterDefinition>& parameters)
{
    QList<ParameterDefinition> pidParameters;
    for (const auto& parameter : parameters) {
        if (parameter.onlineEditable && isPidParameter(parameter)) {
            pidParameters.append(parameter);
        }
    }
    return pidParameters;
}

} // namespace

// ================= 鏋勯€?/ 鏋愭瀯 =================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_projectController(nullptr)
    , m_buildController(nullptr)
    , m_settingsController(nullptr)
    , m_mdiArea(nullptr)
    , m_dslEditor(nullptr)
    , m_editorSubWindow(nullptr)
    , m_logDock(nullptr)
    , m_monitorDock(nullptr)
    
    , m_downloadDock(nullptr), m_outputViewer(nullptr)
    , m_statusLabel(nullptr)
    , m_connectionStatusLabel(nullptr)
    , m_editorPositionLabel(nullptr)
    , m_progressBar(nullptr)
    , m_fileToolBar(nullptr)
    , m_runToolBar(nullptr)
    , m_recentProjectsMenu(nullptr)
    , m_actUndo(nullptr)
    , m_actRedo(nullptr)
    , m_actCut(nullptr)
    , m_actCopy(nullptr)
    , m_actPaste(nullptr)
    , m_actSelectAll(nullptr)
    , m_actFind(nullptr)
    , m_actToggleOutputDock(nullptr)
    , m_actToggleMonitorDock(nullptr)
    
    , m_actToggleDownloadDock(nullptr), m_actToggleExplorerDock(nullptr)
    , m_actToggleFunctionList(nullptr)
    , m_actToggleDslEditor(nullptr)
    , m_actResetLayout(nullptr)
    , m_actClearOutput(nullptr)
    , m_actCompileConfig(nullptr)
    , m_actCompileParameters(nullptr)
    , m_actCompileCommunication(nullptr)
    , m_actCompileAndRunProject(nullptr)
    , m_actRunProject(nullptr)
    , m_actStopProject(nullptr)
    , m_actOpenMonitor(nullptr)
    
    , m_actOpenDownload(nullptr), m_actStartMonitor(nullptr)
    , m_actStopMonitor(nullptr)
    , m_actExportMonitorData(nullptr)
    , m_actExportMonitorImage(nullptr)
    , m_actNew(nullptr)
    , m_actOpen(nullptr)
    , m_actSave(nullptr)
    , m_actCompile(nullptr)
    , m_actOpenDslEditorToolBar(nullptr)
    , m_actSettings(nullptr)
    , m_monitorWidget(nullptr)
    , m_sampleDataProvider(nullptr)
    , m_projectRunning(false)
{
    resize(1500, 900);
    setWindowTitle("LH v1.0.0 - DSL组态");

    createControllers();

    // 鍒涘缓 UI
    createMenus();
    createToolBars();
    createStatusBar();
    createDockWidgets();
    createInspectorDock();
    createParameterTuningWindow();
    
    // 寤虹珛淇″彿杩炴帴
    initConnections();
    connectControllerSignals();

    ThemeManager::applyModernTheme(qApp);

    // 鐩戞帶鍛婅锛氭妸闃堝€艰秴闄愪俊鍙峰啓鍏ヨ緭鍑烘棩蹇楋紝渚夸簬杩芥函
    {
        auto& manager = Monitor::MonitorManager::instance();
        connect(&manager, &Monitor::MonitorManager::thresholdExceeded,
                this, &MainWindow::onMonitorThresholdExceeded);
    }

    m_sampleDataProvider = new SampleDataProvider(this);
    if (m_sessionController) {
        m_sessionController->setSampleDataProvider(m_sampleDataProvider);
    }
    // SampleDataProvider 鐩存帴鍚?MonitorManager 鍐欐牱鏈紝涓嶄緷璧?MonitorWidget銆?    
    // 搴旂敤鍒濆璁剧疆
    applyFontSize(m_settingsController->currentFontPointSize());
    updateRecentProjectsMenu();

    updateStatusBar("就绪");
    if (m_globalStatusBar) {
        m_globalStatusBar->setBuildState("空闲");
        m_globalStatusBar->setConnectionState(false);
        m_globalStatusBar->setProjectName("无");
    }
    refreshInspectorPanel();
    
    LOG_INFO("MainWindow 初始化完成");
}

MainWindow::~MainWindow()
{
    m_parameterTuningWindow = nullptr; // parent 会在 Qt 对象树中删除它
    m_settingsController->saveSettings();
    m_projectController->saveRecentProjects();

    LOG_INFO("MainWindow 已销毁");
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    QString warningMsg = "确定要退出 LH 吗？";
    if (m_projectController->isModified()) {
        warningMsg = "当前有未保存修改，确定要退出吗？";
    }

    auto ret = QMessageBox::question(
        this,
        "退出",
        warningMsg,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
        
    if (ret == QMessageBox::Yes) {
        m_settingsController->saveSettings();
        m_projectController->saveRecentProjects();
        event->accept();
    } else {
        event->ignore();
    }
}

// ================= 鎺у埗鍣ㄥ垱寤?=================

void MainWindow::createControllers()
{
    m_settingsController = new SettingsController(this);
    m_settingsController->loadSettings();
    
    m_projectController = new ProjectController(this);
    m_projectController->setDefaultProjectDir(m_settingsController->defaultProjectDir());
    m_projectController->loadRecentProjects();
    
    m_buildController = new BuildController(this);

    m_parameterController = new ParameterController(this);

    m_sessionController = new RuntimeSessionController(this);
    m_sessionController->setProjectController(m_projectController);
    m_sessionController->setBuildController(m_buildController);

    LOG_DEBUG("鎺у埗鍣ㄥ凡鍒涘缓");
}

void MainWindow::connectControllerSignals()
{
    // ===== ProjectController 淇″彿杩炴帴 =====
    connect(m_projectController, &ProjectController::projectCreated,
            this, &MainWindow::onProjectCreated);
    connect(m_projectController, &ProjectController::projectOpened,
            this, &MainWindow::onProjectOpened);
    connect(m_projectController, &ProjectController::projectSaved,
            this, &MainWindow::onProjectSaved);
    connect(m_projectController, &ProjectController::projectClosed,
            this, &MainWindow::onProjectClosed);
    connect(m_projectController, &ProjectController::modifiedChanged,
            this, &MainWindow::onProjectModifiedChanged);
    connect(m_projectController, &ProjectController::recentProjectsChanged,
            this, &MainWindow::onRecentProjectsChanged);
    connect(m_projectController, &ProjectController::logMessage,
            this, &MainWindow::onLogMessage);
    connect(m_projectController, &ProjectController::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    connect(m_projectController, &ProjectController::warningOccurred,
            this, &MainWindow::onWarningOccurred);
    connect(m_projectController, &ProjectController::projectNameRequired,
            this, &MainWindow::onProjectNameRequired);
    connect(m_projectController, &ProjectController::directorySelectionRequired,
            this, &MainWindow::onDirectorySelectionRequired);
    connect(m_projectController, &ProjectController::saveConfirmationRequired,
            this, &MainWindow::onSaveConfirmationRequired);

    // ===== ParameterController 淇″彿杩炴帴 =====
    connect(m_parameterController, &ParameterController::stateChanged,
            this, [this](const QString& name, ParameterState, ParameterState newState) {
                if (!m_projectController) {
                    return;
                }

                auto& parameters = m_projectController->runtimeConfig().parameters;
                auto it = std::find_if(parameters.begin(), parameters.end(),
                                       [&name](const ParameterDefinition& parameter) {
                                           return parameter.name == name;
                                       });
                if (it == parameters.end()) {
                    return;
                }

                const auto info = m_parameterController->parameterState(name);
                it->confirmed = (newState == ParameterState::Confirmed);
                if (newState == ParameterState::Confirmed && !info.readbackValue.isEmpty()) {
                    it->currentValue = info.readbackValue;
                }
            });
    connect(m_parameterController, &ParameterController::statesChanged,
            this, [this]() { refreshInspectorPanel(); });

    // ===== RuntimeSessionController 淇″彿杩炴帴 =====
    connect(m_sessionController, &RuntimeSessionController::stateChanged,
            this, [this](RuntimeSessionState oldState, RuntimeSessionState newState) {
                Q_UNUSED(oldState);
                const bool running = (newState == RuntimeSessionState::Running
                                      || newState == RuntimeSessionState::Monitoring);
                updateStatusBar(running ? "项目运行中" : "项目已停止");
                if (m_globalStatusBar) {
                    m_globalStatusBar->setBuildState(running ? "运行中" : "已停止");
                }
                refreshInspectorPanel();
            });
    connect(m_sessionController, &RuntimeSessionController::runtimeError,
            this, [this](const QString& msg) {
                if (msg == QStringLiteral("NO_ARTIFACT")) {
                    // 需要弹窗交互，由 MainWindow 处理
                    return;
                }
                appendOutput(QString("[%1] %2")
                             .arg(QDateTime::currentDateTime().toString("HH:mm:ss"), msg));
            });
    connect(m_sessionController, &RuntimeSessionController::logMessage,
            this, &MainWindow::appendOutput);
    connect(m_sessionController, &RuntimeSessionController::demoModeChanged,
            this, [this](bool active) {
                updateStatusBar(active ? "演示模式：采集中" : "");
            });
    connect(m_sessionController, &RuntimeSessionController::monitoringChanged,
            this, [this](bool active) {
                Q_UNUSED(active);
                refreshInspectorPanel();
            });
    connect(m_projectController, &ProjectController::scriptLoadRequired,
            this, &MainWindow::onScriptLoadRequired);
    connect(m_projectController, &ProjectController::editorClearRequired,
            this, &MainWindow::onEditorClearRequired);
    connect(m_projectController, &ProjectController::validationFailed,
            this, &MainWindow::onValidationFailed);

    // ===== BuildController 淇″彿杩炴帴 =====
    connect(m_buildController, &BuildController::compileStarted,
            this, &MainWindow::onCompileStarted);
    connect(m_buildController, &BuildController::compileSucceeded,
            this, &MainWindow::onCompileSucceeded);
    connect(m_buildController, &BuildController::compileFailed,
            this, &MainWindow::onCompileFailed);
    connect(m_buildController, &BuildController::busyChanged,
            this, &MainWindow::onBuildBusyChanged);
    connect(m_buildController, &BuildController::logMessage,
            this, &MainWindow::onLogMessage);
    connect(m_buildController, &BuildController::saveRequired,
            this, &MainWindow::onBuildSaveRequired);
    // validationRequired 已改为回调模式，通过 setValidationCallback 设置
    m_buildController->setValidationCallback([this](BuildType type, QStringList& errors) -> bool {
        return onBuildValidation(type, errors);
    });

    // ===== SettingsController 淇″彿杩炴帴 =====
    connect(m_settingsController, &SettingsController::fontSizeChanged,
            this, &MainWindow::onFontSizeChanged);
    connect(m_settingsController, &SettingsController::logMessage,
            this, &MainWindow::onLogMessage);
    connect(m_settingsController, &SettingsController::defaultProjectDirChanged,
            m_projectController, &ProjectController::setDefaultProjectDir);
            
    LOG_DEBUG("鎺у埗鍣ㄤ俊鍙峰凡杩炴帴");
}

// ================= UI 鏋勫缓 =================

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

    m_actToggleDslEditor = viewMenu->addAction("DSL编辑器(&D)");
    m_actToggleDslEditor->setCheckable(true);
    m_actToggleDslEditor->setChecked(true);
    m_actToggleDslEditor->setShortcut(QKeySequence("Ctrl+D"));
    m_actToggleDslEditor->setToolTip("显示或隐藏 DSL 编辑器窗口");
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

    m_actCompileConfig = buildMenu->addAction("编译DSL (F7)");
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

    m_actCompile = makeAction(":/icons/compile.svg", "编译", "编译 DSL (F7)", QKeySequence(Qt::Key_F7));
    connect(m_actCompile, &QAction::triggered, this, &MainWindow::onCompileConfiguration);
    m_fileToolBar->addAction(m_actCompile);

    m_actOpenDownload = makeAction(":/icons/download.svg", "下载", "打开下载工作区");
    connect(m_actOpenDownload, &QAction::triggered, this, &MainWindow::onOpenDownloadWindow);
    m_fileToolBar->addAction(m_actOpenDownload);

    m_fileToolBar->addSeparator();

    m_actOpenDslEditorToolBar = makeAction(":/icons/output.svg", "DSL", "打开 DSL 编辑器 (Ctrl+D)", QKeySequence("Ctrl+D"));
    m_actOpenDslEditorToolBar->setToolTip("打开 DSL 编辑器 (Ctrl+D)");
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

    QAction* actCompileAndRun = makeAction(":/icons/compile.svg", "编译并运行", "先编译再运行 (F8)", QKeySequence("F8"));
    connect(actCompileAndRun, &QAction::triggered, this, &MainWindow::onCompileAndRunProject);
    m_runToolBar->addAction(actCompileAndRun);

    QAction* actStop = makeAction(":/icons/stop.svg", "停止", "停止项目 (Shift+F9)", QKeySequence("Shift+F9"));
    connect(actStop, &QAction::triggered, this, &MainWindow::onStopProject);
    m_runToolBar->addAction(actStop);

    m_runToolBar->addSeparator();

    QAction* actMonitor = makeAction(":/icons/monitor.svg", "监控", "打开监控工作区 (Ctrl+M)", QKeySequence("Ctrl+M"));
    connect(actMonitor, &QAction::triggered, this, &MainWindow::onOpenMonitor);
    m_runToolBar->addAction(actMonitor);

    m_runToolBar->addSeparator();

    QAction* actParameterTuning = makeAction(":/icons/settings.svg", "调参", "打开独立调参窗口 (Ctrl+Shift+M)", QKeySequence("Ctrl+Shift+M"));
    connect(actParameterTuning, &QAction::triggered, this, &MainWindow::onOpenParameterTuningWindow);
    m_runToolBar->addAction(actParameterTuning);

    m_runToolBar->addSeparator();

    QAction* actSettings = makeAction(":/icons/settings.svg", "选项", "打开选项 (Ctrl+,)", QKeySequence("Ctrl+,"));
    connect(actSettings, &QAction::triggered, this, &MainWindow::onOpenSettings);
    m_runToolBar->addAction(actSettings);
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
    m_workspaceTabs->addTab(m_workspaceDslPage, "DSL工作区");

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
    m_inspectorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_inspectorDock->setFeatures(QDockWidget::DockWidgetMovable |
                                 QDockWidget::DockWidgetClosable);

    m_inspectorPanel = new InspectorPanel(m_inspectorDock);
    m_inspectorPanel->setPanelMode(InspectorPanel::PanelMode::Inspection);
    m_inspectorDock->setWidget(m_inspectorPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_inspectorDock);

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

    // 鍚姩榛樿闅愯棌鍑芥暟鍒楄〃锛氫笉鍗犵敤缂栬緫鍣ㄥ乏渚т富绌洪棿
    const bool functionListVisible = (m_actToggleFunctionList ? m_actToggleFunctionList->isChecked() : false);
    m_dslEditor->setFunctionListVisible(functionListVisible);
    
    m_projectController->setDslEditor(m_dslEditor);

    m_editorSubWindow = m_mdiArea->addSubWindow(m_dslEditor);
    m_editorSubWindow->setWindowTitle("DSL脚本编辑器");
    m_editorSubWindow->showMaximized();

    connect(m_editorSubWindow, &QObject::destroyed,
            this, &MainWindow::onDslEditorSubWindowDestroyed);

    connectDslEditorSignals();

    appendOutput(QString("[%1] DSL 脚本编辑器已打开")
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
    
    // 璁剧疆鐘舵€佹爮鍥炶皟
    m_dslEditor->setStatusCallback([this](const QString& msg) {
        updateStatusBar(msg);
    });
}

void MainWindow::initConnections()
{
    connect(qApp, &QApplication::focusChanged,
            this, &MainWindow::onFocusChanged);
}

// ================= UI 杈呭姪鏂规硶 =================

void MainWindow::updateStatusBar(const QString& message)
{
    m_statusLabel->setText(message);
    if (m_globalStatusBar) {
        m_globalStatusBar->setBuildState(message);
    }
    refreshInspectorPanel();
}

void MainWindow::updateConnectionStatus(bool connected)
{
    if (connected) {
        m_connectionStatusLabel->setText("已连接");
    } else {
        m_connectionStatusLabel->setText("未连接");
    }
    m_connectionStatusLabel->setProperty("connected", connected);
    m_connectionStatusLabel->style()->unpolish(m_connectionStatusLabel);
    m_connectionStatusLabel->style()->polish(m_connectionStatusLabel);
    if (m_globalStatusBar) {
        m_globalStatusBar->setConnectionState(connected);
    }
    refreshInspectorPanel();
}

void MainWindow::appendOutput(const QString& message)
{
    if (m_outputViewer) {
        m_outputViewer->append(message);
        
        if (m_settingsController->autoScrollLog()) {
            QScrollBar* scrollBar = m_outputViewer->verticalScrollBar();
            scrollBar->setValue(scrollBar->maximum());
        }
    }
}

void MainWindow::updateWindowTitle()
{
    QString title = "LH v1.0.0";
    
    const auto& config = m_projectController->runtimeConfig();
    if (!config.projectName.isEmpty()) {
        title += " - " + config.projectName;
    }
    
    if (m_projectController->isModified()) {
        title += " *";
    }
    
    setWindowTitle(title);
    if (m_globalStatusBar) {
        m_globalStatusBar->setProjectName(config.projectName);
    }
    refreshInspectorPanel();
}

void MainWindow::updateEditorSubWindowTitle()
{
    if (m_editorSubWindow) {
        QString title = "DSL脚本编辑器";
        if (m_projectController->isModified()) {
            title += " *";
        }
        m_editorSubWindow->setWindowTitle(title);
    }
}

void MainWindow::refreshInspectorPanel()
{
    if (!m_projectController || m_refreshingInspector) {
        return;
    }

    m_refreshingInspector = true;
    refreshInspectorPanel(m_inspectorPanel);
    refreshInspectorPanel(m_parameterTuningWindow);
    m_refreshingInspector = false;
}

void MainWindow::refreshInspectorPanel(InspectorPanel* panel)
{
    if (!panel || !m_projectController) {
        return;
    }

    panel->setProjectPath(m_projectController->currentProjectPath());
    panel->setCurrentFile(m_projectController->currentScriptFile());
    panel->setRuntimeState(m_sessionController && m_sessionController->isRunning() ? "运行中" : "已停止");
    panel->setBuildState(m_buildController && m_buildController->isBusy() ? "忙碌" : "空闲");
    panel->setMonitoringState(m_monitorWidget && m_monitorWidget->isMonitoring() ? "活动" : "未活动");
    const auto& cfg = m_projectController->runtimeConfig();

    // 同步参数定义到状态机控制器（保留已有状态）
    m_parameterController->loadDefinitions(cfg.parameters);

    int editableParameters = 0;
    for (const auto& p : cfg.parameters) {
        if (p.onlineEditable) {
            ++editableParameters;
        }
    }
    panel->setVariableSummary(QStringLiteral("%1 个，已挂载监控").arg(cfg.variables.size()));
    panel->setParameterSummary(QStringLiteral("%1 个，在线可改 %2 个")
                                            .arg(cfg.parameters.size())
                                            .arg(editableParameters));
    panel->setResourceSummary(QStringLiteral("%1 个，已挂载监控").arg(cfg.resources.size()));
    panel->setParameterDetails(cfg.parameters);

    // 推送参数状态映射
    QMap<QString, ParameterStateInfo> stateMap;
    for (const auto& si : m_parameterController->parameterStates())
        stateMap.insert(si.name, si);
    panel->setParameterStateMap(stateMap);

    QStringList readbackReady;
    QMap<QString, double> deviationMap;
    for (const auto& p : cfg.parameters) {
        const QString channelName = QStringLiteral("param::%1").arg(p.name);
        const auto samples = Monitor::MonitorManager::instance().history(channelName, 1);
        const bool hasReadback = !samples.isEmpty();
        if (hasReadback && !p.currentValue.isEmpty()) {
            readbackReady.append(p.name);
            bool okCurrent = false;
            const double currentValue = p.currentValue.toDouble(&okCurrent);
            const double sampleValue = samples.last().value;
            if (okCurrent && std::isfinite(sampleValue)) {
                deviationMap.insert(p.name, sampleValue - currentValue);
                continue;
            }
        }
    }
    panel->setParameterReadbackReady(readbackReady);
    panel->setParameterDeviationMap(deviationMap);
    if (m_workspaceTabs) {
        panel->setWorkspaceName(m_workspaceTabs->tabText(m_workspaceTabs->currentIndex()));
    }
}

void MainWindow::refreshInspectorPanel(ParameterTuningWindow* window)
{
    if (!window || !m_projectController) {
        return;
    }

    const auto& cfg = m_projectController->runtimeConfig();
    const QList<ParameterDefinition> pidParameters = filterPidParameters(cfg.parameters);
    window->setPidParameterDetails(pidParameters);
    QStringList readbackReady;
    QMap<QString, double> deviationMap;
    for (const auto& p : pidParameters) {
        const QString channelName = QStringLiteral("param::%1").arg(p.name);
        const auto samples = Monitor::MonitorManager::instance().history(channelName, 1);
        const bool hasReadback = !samples.isEmpty();
        if (hasReadback && !p.currentValue.isEmpty()) {
            readbackReady.append(p.name);
            bool okCurrent = false;
            const double currentValue = p.currentValue.toDouble(&okCurrent);
            const double sampleValue = samples.last().value;
            if (okCurrent && std::isfinite(sampleValue)) {
                deviationMap.insert(p.name, sampleValue - currentValue);
                continue;
            }
        }
    }
    window->setParameterReadbackReady(readbackReady);
    window->setParameterDeviationMap(deviationMap);

    // 推送参数状态映射
    QMap<QString, ParameterStateInfo> stateMap;
    for (const auto& si : m_parameterController->parameterStates())
        stateMap.insert(si.name, si);
    window->setParameterStateMap(stateMap);
}

void MainWindow::addProblem(const QString& severity, const QString& source, const QString& message)
{
    if (m_problemsPanel) {
        m_problemsPanel->addProblem(severity, source, message);
    }
}

void MainWindow::updateRecentProjectsMenu()
{
    m_recentProjectsMenu->clear();
    
    const QStringList& projects = m_projectController->recentProjects();
    for (const QString& path : projects) {
        QAction* action = m_recentProjectsMenu->addAction(path);
        action->setData(path);
        connect(action, &QAction::triggered, this, &MainWindow::onRecentProjectTriggered);
    }
    
    if (projects.isEmpty()) {
        QAction* emptyAction = m_recentProjectsMenu->addAction("（无最近项目）");
        emptyAction->setEnabled(false);
    }
}

void MainWindow::setCompileActionsEnabled(bool enabled)
{
    if (m_actCompileConfig) m_actCompileConfig->setEnabled(enabled);
    if (m_actCompileParameters) m_actCompileParameters->setEnabled(enabled);
    if (m_actCompileCommunication) m_actCompileCommunication->setEnabled(enabled);
    if (m_actCompile) m_actCompile->setEnabled(enabled);
    if (m_actRunProject) m_actRunProject->setEnabled(enabled);
}

void MainWindow::applyFontSize(int pointSize)
{
    QFont font;
    font.setPointSize(pointSize);
    font.setFamily("Consolas");
    
    if (m_dslEditor && m_dslEditor->editor()) {
        m_dslEditor->editor()->setFont(font);
    }
    
    if (m_outputViewer) {
        m_outputViewer->setFont(font);
    }
}

void MainWindow::showValidationErrors(const QStringList& errors)
{
    QString message = "项目配置校验失败，发现以下问题：\n\n";
    
    for (int i = 0; i < errors.size(); ++i) {
        message += QString("%1. %2\n").arg(i + 1).arg(errors[i]);
    }
    
    message += "\n请修复以上问题后重试。";
    
    QMessageBox::warning(this, "配置校验失败", message);
    
    appendOutput(QString("[%1] 配置校验失败:")
                 .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    for (const auto& error : errors) {
        appendOutput(QString("  - %1").arg(error));
        addProblem("warning", "配置校验", error);
    }
}

const ProjectRuntimeConfig& MainWindow::runtimeConfig() const
{
    return m_projectController->runtimeConfig();
}


void MainWindow::startDemoModeIfNeeded(const QString& reason)
{
    m_sessionController->startDemoMode(reason);
    m_demoModeActive = m_sessionController->isDemoMode();
}

void MainWindow::stopDemoMode(const QString& reason)
{
    m_sessionController->stopDemoMode(reason);
    m_demoModeActive = m_sessionController->isDemoMode();
}

bool MainWindow::applyRuntimeConfigToMonitor()
{
    // 更新 GlobalStatusBar 协议和采样率
    if (m_projectController && m_globalStatusBar) {
        const auto& cfg = m_projectController->runtimeConfig();
        m_globalStatusBar->setProtocolName(cfg.protocol);
        int maxHz = 0;
        for (const auto& provider : cfg.providers) {
            if (provider.periodMs > 0) {
                const int hz = qMax(1, 1000 / provider.periodMs);
                maxHz = qMax(maxHz, hz);
            }
        }
        m_globalStatusBar->setSamplingRateHz(maxHz);
    }

    return m_sessionController->applyRuntimeConfig();
}


// ================= 椤圭洰鎿嶄綔妲藉嚱鏁帮紙杞彂缁?ProjectController锛?================

void MainWindow::onNewProject()
{
    m_projectController->createNewProject();
}

void MainWindow::onOpenProject()
{
    m_projectController->openProject();
}

void MainWindow::onSaveProject()
{
    bool savedAuxiliaryFile = false;
    if (m_mdiArea) {
        if (QMdiSubWindow* activeSub = m_mdiArea->activeSubWindow()) {
            if (activeSub != m_editorSubWindow) {
                const QString auxiliaryFilePath = activeSub->property("filePath").toString();
                if (!auxiliaryFilePath.isEmpty()) {
                    if (auto* auxiliaryEditor = qobject_cast<QPlainTextEdit*>(activeSub->widget())) {
                        QFile auxiliaryFile(auxiliaryFilePath);
                        if (!auxiliaryFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                            QMessageBox::warning(this, "保存失败", QString("无法保存文件: %1").arg(auxiliaryFilePath));
                            return;
                        }

                        QTextStream out(&auxiliaryFile);
                        TextEncoding::setUtf8(out);
                        out << auxiliaryEditor->toPlainText();
                        auxiliaryFile.close();

                        activeSub->setProperty("modified", false);
                        activeSub->setWindowTitle(QFileInfo(auxiliaryFilePath).fileName());
                        updateStatusBar(QString("已保存文件: %1").arg(QFileInfo(auxiliaryFilePath).fileName()));
                        savedAuxiliaryFile = true;
                    }
                }
            }
        }
    }

    if (savedAuxiliaryFile) {
        return;
    }

    m_projectController->syncDslMappingsFromEditor();
    m_projectController->syncDslMappingsToEditor();

    // 鍏堜繚瀛?DSL 鑴氭湰鍐呭
    if (m_dslEditor && !m_projectController->currentScriptFile().isEmpty()) {
        const QString scriptForSave = m_dslEditor->scriptForSave();
        QFile scriptFile(m_projectController->currentScriptFile());
        if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&scriptFile);
            TextEncoding::setUtf8(stream);
            stream << scriptForSave;
            scriptFile.close();
        }
        if (scriptForSave != m_dslEditor->currentScript()) {
            m_dslEditor->setScript(scriptForSave);
        }
        m_dslEditor->setModified(false);
    }
    
    m_projectController->saveProject();
}

void MainWindow::onSaveAll()
{
    onSaveProject();
}

void MainWindow::onCloseProject()
{
    m_projectController->closeProject();
}

void MainWindow::onRecentProjectTriggered()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action) {
        m_projectController->openRecentProject(action->data().toString());
    }
}

// ================= 缂栬緫鎿嶄綔妲藉嚱鏁?=================

void MainWindow::onUndo()
{
    QWidget* editor = getCurrentTextEditor();
    if (auto* textEdit = qobject_cast<QTextEdit*>(editor)) {
        textEdit->undo();
    } else if (auto* plainEdit = qobject_cast<QPlainTextEdit*>(editor)) {
        plainEdit->undo();
    }
}

void MainWindow::onRedo()
{
    QWidget* editor = getCurrentTextEditor();
    if (auto* textEdit = qobject_cast<QTextEdit*>(editor)) {
        textEdit->redo();
    } else if (auto* plainEdit = qobject_cast<QPlainTextEdit*>(editor)) {
        plainEdit->redo();
    }
}

void MainWindow::onCut()
{
    QWidget* editor = getCurrentTextEditor();
    if (auto* textEdit = qobject_cast<QTextEdit*>(editor)) {
        textEdit->cut();
    } else if (auto* plainEdit = qobject_cast<QPlainTextEdit*>(editor)) {
        plainEdit->cut();
    }
}

void MainWindow::onCopy()
{
    QWidget* editor = getCurrentTextEditor();
    if (auto* textEdit = qobject_cast<QTextEdit*>(editor)) {
        textEdit->copy();
    } else if (auto* plainEdit = qobject_cast<QPlainTextEdit*>(editor)) {
        plainEdit->copy();
    }
}

void MainWindow::onPaste()
{
    QWidget* editor = getCurrentTextEditor();
    if (auto* textEdit = qobject_cast<QTextEdit*>(editor)) {
        textEdit->paste();
    } else if (auto* plainEdit = qobject_cast<QPlainTextEdit*>(editor)) {
        plainEdit->paste();
    }
}

void MainWindow::onSelectAll()
{
    QWidget* editor = getCurrentTextEditor();
    if (auto* textEdit = qobject_cast<QTextEdit*>(editor)) {
        textEdit->selectAll();
    } else if (auto* plainEdit = qobject_cast<QPlainTextEdit*>(editor)) {
        plainEdit->selectAll();
    }
}

void MainWindow::onFind()
{
    if (m_dslEditor) {
        m_dslEditor->showFindBar();
    }
}

QWidget* MainWindow::getCurrentTextEditor() const
{
    QWidget* focused = QApplication::focusWidget();
    
    if (qobject_cast<QTextEdit*>(focused) || qobject_cast<QPlainTextEdit*>(focused)) {
        return focused;
    }
    
    if (m_dslEditor) {
        return m_dslEditor->editor();
    }
    
    return nullptr;
}

void MainWindow::updateEditActions()
{
    bool hasEditor = (getCurrentTextEditor() != nullptr);
    
    m_actUndo->setEnabled(hasEditor);
    m_actRedo->setEnabled(hasEditor);
    m_actCut->setEnabled(hasEditor);
    m_actCopy->setEnabled(hasEditor);
    m_actPaste->setEnabled(hasEditor);
    m_actSelectAll->setEnabled(hasEditor);
    m_actFind->setEnabled(hasEditor);
}

void MainWindow::onFocusChanged(QWidget* old, QWidget* now)
{
    Q_UNUSED(old);
    Q_UNUSED(now);
    updateEditActions();
}

// ================= 瑙嗗浘鎿嶄綔妲藉嚱鏁?=================

void MainWindow::onToggleOutputDock(bool checked)
{
    if (m_logDock) {
        m_logDock->setVisible(checked);
    }
}

void MainWindow::onToggleMonitorDock(bool checked)
{
    if (m_workspaceTabs && m_workspaceMonitorPage) {
        if (checked) {
            m_workspaceTabs->setCurrentWidget(m_workspaceMonitorPage);
        } else if (m_workspaceDslPage) {
            m_workspaceTabs->setCurrentWidget(m_workspaceDslPage);
        }
    } else if (m_monitorDock) {
        m_monitorDock->setVisible(checked);
    }
}


void MainWindow::onToggleDownloadDock(bool checked)
{
    if (m_workspaceTabs && m_workspaceBuildPage) {
        if (checked) {
            m_workspaceTabs->setCurrentWidget(m_workspaceBuildPage);
        } else if (m_workspaceDslPage) {
            m_workspaceTabs->setCurrentWidget(m_workspaceDslPage);
        }
    } else if (m_downloadDock) {
        m_downloadDock->setVisible(checked);
    }
}

void MainWindow::onOpenDownloadWindow()
{
    if (m_workspaceTabs && m_workspaceBuildPage) {
        m_workspaceTabs->setCurrentWidget(m_workspaceBuildPage);
    } else if (m_actToggleDownloadDock) {
        m_actToggleDownloadDock->setChecked(true);
    }
}

void MainWindow::onToggleFunctionList(bool visible)
{
    if (m_dslEditor) {
        m_dslEditor->setFunctionListVisible(visible);
    }
}

void MainWindow::onToggleExplorerDock(bool checked)
{
    if (m_explorerDock) {
        m_explorerDock->setVisible(checked);
    }
}

void MainWindow::onToggleDslEditor(bool checked)
{
    if (checked) {
        if (!m_editorSubWindow) {
            createDslEditorSubWindow();
        } else {
            m_editorSubWindow->show();
        }
    } else {
        if (m_editorSubWindow) {
            m_editorSubWindow->hide();
        }
    }
    
    if (m_actOpenDslEditorToolBar && m_actOpenDslEditorToolBar != sender()) {
        m_actOpenDslEditorToolBar->blockSignals(true);
        m_actOpenDslEditorToolBar->setChecked(checked);
        m_actOpenDslEditorToolBar->blockSignals(false);
    }
    
    if (m_actToggleDslEditor && m_actToggleDslEditor != sender()) {
        m_actToggleDslEditor->blockSignals(true);
        m_actToggleDslEditor->setChecked(checked);
        m_actToggleDslEditor->blockSignals(false);
    }
}

void MainWindow::onResetLayout()
{
    if (m_explorerDock) {
        m_explorerDock->setVisible(true);
    }
    if (m_logDock) {
        m_logDock->setVisible(true);
    }
    if (m_workspaceTabs && m_workspaceDslPage) {
        m_workspaceTabs->setCurrentWidget(m_workspaceDslPage);
    } else if (m_monitorDock) {
        m_monitorDock->setVisible(false);
    }
    
    if (m_dslEditor) {
        m_dslEditor->setFunctionListVisible(false);
    }
    
    if (m_actToggleOutputDock) m_actToggleOutputDock->setChecked(true);
    if (m_actToggleMonitorDock) m_actToggleMonitorDock->setChecked(false);
    if (m_actToggleExplorerDock) m_actToggleExplorerDock->setChecked(true);
    if (m_actToggleFunctionList) m_actToggleFunctionList->setChecked(false);
}

void MainWindow::onClearOutput()
{
    if (m_outputViewer) {
        m_outputViewer->clear();
    }
}

void MainWindow::onDslEditorSubWindowDestroyed()
{
    m_editorSubWindow = nullptr;
    m_dslEditor = nullptr;
    m_projectController->setDslEditor(nullptr);

    if (m_actToggleDslEditor) {
        m_actToggleDslEditor->blockSignals(true);
        m_actToggleDslEditor->setChecked(false);
        m_actToggleDslEditor->blockSignals(false);
    }
    
    if (m_actOpenDslEditorToolBar) {
        m_actOpenDslEditorToolBar->blockSignals(true);
        m_actOpenDslEditorToolBar->setChecked(false);
        m_actOpenDslEditorToolBar->blockSignals(false);
    }
}

// ================= 璁剧疆鎿嶄綔妲藉嚱鏁?=================

void MainWindow::onOpenSettings()
{
    m_settingsController->openSettingsDialog(this);
}

// ================= 缂栬瘧鎿嶄綔妲藉嚱鏁帮紙杞彂缁?BuildController锛?================

void MainWindow::onCompileConfiguration()
{
    if (!m_projectController->hasOpenProject()) {
        QMessageBox::warning(this, "警告", "请先打开或创建项目。");
        return;
    }
    
    m_buildController->compileConfiguration(
        m_projectController->currentProjectPath(),
        m_projectController->runtimeConfig());
}

void MainWindow::onCompileParameters()
{
    m_buildController->compileParameters(m_projectController->currentProjectPath(),
                                         m_projectController->runtimeConfig());
}

void MainWindow::onCompileCommunication()
{
    m_buildController->compileCommunication(m_projectController->currentProjectPath(),
                                            m_projectController->runtimeConfig());
}

void MainWindow::onCompileAndRunProject()
{
    onRunProject();
}

// ================= 杩愯鎺у埗妲藉嚱鏁?=================

void MainWindow::onRunProject()
{
    // 前置校验（含 UI 弹窗）
    if (!m_projectController->hasOpenProject()) {
        QMessageBox::warning(this, "警告", "请先打开或创建项目。");
        return;
    }

    if (m_sessionController->isRunning()) {
        QMessageBox::information(this, "提示", "项目已在运行中。");
        return;
    }

    // 未保存修改处理
    const bool hasUnsavedChanges =
            (m_projectController && m_projectController->isModified())
            || (m_dslEditor && m_dslEditor->isModified());
    bool runWithDiskVersion = false;
    if (hasUnsavedChanges) {
        const QMessageBox::StandardButton choice =
                QMessageBox::question(this,
                                      "保存修改",
                                      "当前工程或 DSL 脚本存在未保存修改。是否先保存后再运行？",
                                      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                      QMessageBox::Save);
        if (choice == QMessageBox::Cancel)
            return;
        if (choice == QMessageBox::Save) {
            onBuildSaveRequired();
        } else if (choice == QMessageBox::Discard) {
            runWithDiskVersion = true;
        }
    }

    // 委托控制器做业务校验
    if (!m_sessionController->prepareRun()) {
        // 检查是否需要编译后运行
        if (m_sessionController->artifactPath().isEmpty()) {
            const QMessageBox::StandardButton choice =
                    QMessageBox::question(this,
                                          "缺少编译产物",
                                          "未找到 DSL 编译生成的 .code 文件。是否立即编译并在成功后继续运行？",
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::Yes);
            if (choice == QMessageBox::Yes) {
                m_sessionController->setPendingRunAfterCompile(true);
                m_sessionController->setSkipNextBuildSave(runWithDiskVersion);
                m_buildController->compileConfiguration(m_projectController->currentProjectPath(),
                                                       m_projectController->runtimeConfig());
                return;
            }
            appendOutput(QString("[%1] 运行已阻止：未找到 DSL 编译产物")
                         .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        }
        return;
    }

    // 应用运行时配置
    if (!m_sessionController->applyRuntimeConfig())
        return;

    // 启动监控 UI
    if (m_monitorWidget) {
        m_monitorWidget->startMonitoring();
    }

    // 执行运行
    m_sessionController->executeRun();

    // 更新 UI
    m_projectRunning = true;
    updateStatusBar("项目运行中");
    if (m_globalStatusBar)
        m_globalStatusBar->setBuildState("运行中");
    refreshInspectorPanel();
}

void MainWindow::onStopProject()
{
    if (!m_sessionController->isRunning())
        return;

    m_sessionController->requestStop();
    m_projectRunning = false;
    updateStatusBar("项目已停止");
    if (m_globalStatusBar)
        m_globalStatusBar->setBuildState("已停止");
    refreshInspectorPanel();
}

// ================= 鐩戞帶鎿嶄綔妲藉嚱鏁?=================

void MainWindow::onOpenMonitor()
{
    if (m_workspaceTabs && m_workspaceMonitorPage) {
        m_workspaceTabs->setCurrentWidget(m_workspaceMonitorPage);
    } else if (m_monitorDock) {
        m_monitorDock->setVisible(true);
    }
    if (m_actToggleMonitorDock) {
        m_actToggleMonitorDock->setChecked(true);
    }

    const bool demoWasActive = m_demoModeActive;
    startDemoModeIfNeeded(QStringLiteral("打开监控"));

    if (!demoWasActive && m_demoModeActive && m_monitorWidget && !m_monitorWidget->isMonitoring()) {
        // Demo Mode 婵€娲诲悗锛岃嚜鍔ㄥ紑濮嬬洃鎺т互渚跨敤鎴蜂竴鎵撳紑闈㈡澘灏辫兘鐪嬪埌鏁版嵁鍙樺寲
        m_monitorWidget->startMonitoring();
    }
    refreshInspectorPanel();
}

void MainWindow::onOpenParameterTuningWindow()
{
    if (!m_parameterTuningWindow) {
        createParameterTuningWindow();
    }
    refreshInspectorPanel(m_parameterTuningWindow);
    if (m_parameterTuningWindow) {
        m_parameterTuningWindow->show();
        m_parameterTuningWindow->raise();
        m_parameterTuningWindow->activateWindow();
    }
}

void MainWindow::onStartMonitoring()
{
    if (m_projectController && m_projectController->hasOpenProject()) {
        if (!applyRuntimeConfigToMonitor()) {
            return;
        }
    } else {
        appendOutput(QString("[%1] 未打开项目，进入演示模式进行监控")
                         .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    }

    startDemoModeIfNeeded(QStringLiteral("开始监控"));

    if (m_monitorWidget) {
        m_monitorWidget->startMonitoring();
    }
    refreshInspectorPanel();
}

void MainWindow::onStopMonitoring()
{
    if (m_monitorWidget) {
        m_monitorWidget->stopMonitoring();
    }

    // 鑻ュ浜?Demo Mode锛屽垯鍋滄婕旂ず鏁版嵁閲囬泦
    stopDemoMode(QStringLiteral("停止监控"));
    refreshInspectorPanel();
}

void MainWindow::onExportMonitorData()
{
    if (m_monitorWidget) {
        m_monitorWidget->onExportData();
    }
}

void MainWindow::onExportMonitorImage()
{
    if (m_monitorWidget) {
        m_monitorWidget->exportCurrentChartImage();
    }
}

// ================= 鍏朵粬妲藉嚱鏁?=================

void MainWindow::onOpenLogDirectory()
{
    QString logDir = QCoreApplication::applicationDirPath() + "/logs";
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(logDir));
}

void MainWindow::onOpenDiagnosisWizard()
{
    if (m_bottomPanels && m_problemsPanel) {
        m_bottomPanels->setCurrentWidget(m_problemsPanel);
    }

    if (m_problemsPanel) {
        m_problemsPanel->setDiagnosticSummary("诊断摘要：先看错误，再看警告；若问题面板为空，优先检查编译、下载和运行日志。");
    }

    appendOutput("诊断向导：请优先查看问题面板中的错误和警告，再检查编译、下载和运行日志。");
    updateStatusBar("已打开诊断向导");
}

void MainWindow::onEditParameterRequested(const QString& parameterName)
{
    if (parameterName.isEmpty() || !m_projectController || !m_parameterController) {
        return;
    }

    const auto stateInfo = m_parameterController->parameterState(parameterName);
    if (!stateInfo.onlineEditable) {
        appendOutput(QString("参数 %1 不允许在线修改").arg(parameterName));
        return;
    }

    bool ok = false;
    const QString current = stateInfo.editedValue.isEmpty()
                                ? stateInfo.definitionValue
                                : stateInfo.editedValue;
    const QString value = QInputDialog::getText(
        this,
        "编辑参数",
        QString("输入参数 %1 的新值").arg(parameterName),
        QLineEdit::Normal,
        current,
        &ok);
    if (!ok) {
        return;
    }

    if (m_parameterController->editParameter(parameterName, value.trimmed())) {
        // 同步写回 runtimeConfig（保持旧字段兼容）
        auto& cfg = m_projectController->runtimeConfig();
        for (auto& p : cfg.parameters) {
            if (p.name == parameterName) {
                p.currentValue = value.trimmed();
                p.confirmed = false;
                break;
            }
        }
        m_projectController->setModified(true);
        refreshInspectorPanel();
        appendOutput(QString("参数 %1 已更新为 %2").arg(parameterName, value.trimmed()));
        updateStatusBar(QString("参数 %1 已更新").arg(parameterName));
    } else {
        appendOutput(QString("参数 %1 编辑失败").arg(parameterName));
    }
}

void MainWindow::onApplyParametersRequested()
{
    if (!m_projectController || !m_parameterController) {
        return;
    }

    IDeviceBackend* backend = Monitor::MonitorManager::instance().deviceBackend();
    if (!backend || !backend->isOnline()) {
        appendOutput("设备后端未连接，无法应用参数。");
        updateStatusBar("参数应用失败：设备离线");
        return;
    }

    QString errorMessage;
    const bool ok = m_parameterController->applyModifiedParametersWithReadback(
        backend,
        2,
        100,
        &errorMessage);
    if (ok) {
        appendOutput("参数已下发并完成回读确认。");
        updateStatusBar("参数下发与回读完成");
    } else {
        if (errorMessage.isEmpty()) {
            errorMessage = QStringLiteral("参数下发失败");
        }
        appendOutput(QString("参数下发/回读失败：%1").arg(errorMessage));
        updateStatusBar("参数下发/回读失败");
    }
    refreshInspectorPanel();
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, "关于",
        "LH v1.0.0\n\n"
        "基于 DSL 的控制平台，集成构建、下载与监控流程。\n\n"
        "版权所有 (c) 2024-2026。");
}

// ================= 缂栬緫鍣ㄧ浉鍏虫Ы鍑芥暟 =================

void MainWindow::onEditorCursorPositionChanged(int line, int column, int totalLines)
{
    m_editorPositionLabel->setText(QString("行 %1, 列 %2 / 共 %3 行")
                                   .arg(line).arg(column).arg(totalLines));
}

void MainWindow::onEditorModified(bool modified)
{
    m_projectController->setModified(modified);
}

void MainWindow::onSnippetInserted(const DslInsertRecord& record)
{
    appendOutput(QString("[%1] 已插入组件: %2 (行 %3)")
                 .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                 .arg(record.snippetName.isEmpty() ? record.snippetId : record.snippetName)
                 .arg(record.lineNumber));
    
    m_projectController->syncDslMappingsFromEditor();
    m_projectController->syncDslMappingsToEditor();

    m_projectController->setModified(true);
}

void MainWindow::onDropError(const QString& errorMessage)
{
    appendOutput(QString("[%1] 拖拽错误: %2")
                 .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                 .arg(errorMessage));
    addProblem("warning", "DSL编辑器", errorMessage);
}

// ================= 杈撳嚭绐楀彛鍙抽敭鑿滃崟妲藉嚱鏁?=================

void MainWindow::onOutputContextMenu(const QPoint& pos)
{
    QMenu menu(this);
    
    QAction* actCopy = menu.addAction("复制选中");
    connect(actCopy, &QAction::triggered, this, &MainWindow::onCopySelectedOutput);
    
    QAction* actCopyAll = menu.addAction("复制全部");
    connect(actCopyAll, &QAction::triggered, this, &MainWindow::onCopyAllOutput);
    
    menu.addSeparator();
    
    QAction* actSave = menu.addAction("保存到文件...");
    connect(actSave, &QAction::triggered, this, &MainWindow::onSaveOutputToFile);
    
    menu.addSeparator();
    
    QAction* actClear = menu.addAction("清空");
    connect(actClear, &QAction::triggered, this, &MainWindow::onClearOutput);
    
    menu.exec(m_outputViewer->mapToGlobal(pos));
}

void MainWindow::onCopySelectedOutput()
{
    if (m_outputViewer) {
        m_outputViewer->copy();
    }
}

void MainWindow::onCopyAllOutput()
{
    if (m_outputViewer) {
        QApplication::clipboard()->setText(m_outputViewer->toPlainText());
    }
}

void MainWindow::onSaveOutputToFile()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "保存输出", QString(), "文本文件 (*.txt);;所有文件 (*.*)");
    
    if (fileName.isEmpty()) {
        return;
    }
    
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        TextEncoding::setUtf8(stream);
        stream << m_outputViewer->toPlainText();
        file.close();
        
        appendOutput(QString("[%1] 输出已保存到: %2")
                     .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                     .arg(fileName));
    }
}


QString MainWindow::resolveExplorerRootPath() const
{
    if (m_projectController && m_projectController->hasOpenProject()) {
        return QDir(m_projectController->currentProjectPath()).absolutePath();
    }

    if (m_projectController) {
        const QStringList recentProjects = m_projectController->recentProjects();
        for (const QString& recentPath : recentProjects) {
            const QFileInfo dirInfo(recentPath);
            if (!dirInfo.exists() || !dirInfo.isDir()) {
                continue;
            }

            if (QFileInfo::exists(QDir(recentPath).filePath("project_config.json"))) {
                return dirInfo.absoluteFilePath();
            }
        }
    }

    return QString();
}

void MainWindow::refreshExplorerRoot()
{
    if (m_projectExplorerWidget) {
        m_projectExplorerWidget->setRootPath(resolveExplorerRootPath());
    }
}

bool MainWindow::isSupportedTextFile(const QString& filePath) const
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    static const QSet<QString> allowed = {
        "txt", "json", "xml", "yaml", "yml", "ini",
        "dsl", "cpp", "c", "h", "hpp", "cc", "cxx",
        "ui", "qss", "pro", "pri", "cmake", "md", "log"
    };
    return allowed.contains(suffix) || QFileInfo(filePath).fileName() == "CMakeLists.txt";
}

bool MainWindow::loadTextFileToEditor(const QString& filePath)
{
    if (!m_dslEditor) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "打开失败", QString("无法打开文件: %1").arg(filePath));
        return false;
    }

    const QString content = TextEncoding::decodeUtf8WithLocalFallback(file.readAll());
    file.close();

    m_dslEditor->setScript(content);
    m_dslEditor->setCurrentFilePath(filePath);
    m_dslEditor->editor()->setReadOnly(false);
    m_dslEditor->setModified(false);
    updateStatusBar(QString("已打开文件: %1").arg(QFileInfo(filePath).fileName()));
    refreshInspectorPanel();
    return true;
}

void MainWindow::openAuxiliaryTextFileInMdi(const QString& filePath)
{
    const QString canonicalPath = QFileInfo(filePath).canonicalFilePath();

    const auto subWindows = m_mdiArea->subWindowList();
    for (QMdiSubWindow* sub : subWindows) {
        if (!sub) {
            continue;
        }
        const QString existingPath = sub->property("filePath").toString();
        if (!existingPath.isEmpty() && QFileInfo(existingPath).canonicalFilePath() == canonicalPath) {
            sub->show();
            sub->raise();
            m_mdiArea->setActiveSubWindow(sub);
            return;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "打开失败", QString("无法打开文件: %1").arg(filePath));
        return;
    }

    const QString content = TextEncoding::decodeUtf8WithLocalFallback(file.readAll());
    file.close();

    auto* viewer = new QPlainTextEdit;
    viewer->setReadOnly(false);
    viewer->setLineWrapMode(QPlainTextEdit::NoWrap);
    viewer->setPlainText(content);
    viewer->setWindowTitle(QFileInfo(filePath).fileName());

    auto* sub = m_mdiArea->addSubWindow(viewer);
    sub->setAttribute(Qt::WA_DeleteOnClose, true);
    sub->setProperty("filePath", filePath);
    sub->setProperty("modified", false);
    sub->setWindowTitle(QFileInfo(filePath).fileName());
    connect(viewer, &QPlainTextEdit::textChanged, this, [sub = QPointer<QMdiSubWindow>(sub), filePath]() {
        if (!sub) {
            return;
        }
        if (sub->property("modified").toBool()) {
            return;
        }
        sub->setProperty("modified", true);
        sub->setWindowTitle(QFileInfo(filePath).fileName() + "*");
    });
    sub->show();
    m_mdiArea->setActiveSubWindow(sub);

    updateStatusBar(QString("已打开文件: %1").arg(QFileInfo(filePath).fileName()));
    refreshInspectorPanel();
}

void MainWindow::openFileFromExplorer(const QString& filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        return;
    }

    if (!isSupportedTextFile(filePath)) {
        updateStatusBar(QString("不支持直接打开该文件类型: %1").arg(info.fileName()));
        return;
    }

    // If no project is currently open, auto-open the nearest project root.
    if (m_projectController && !m_projectController->hasOpenProject()) {
        QDir dir = info.absoluteDir();
        QString projectRoot;
        while (dir.exists()) {
            if (QFileInfo::exists(dir.filePath("project_config.json"))) {
                projectRoot = dir.absolutePath();
                break;
            }
            if (!dir.cdUp()) {
                break;
            }
        }

        if (!projectRoot.isEmpty()) {
            m_projectController->openProjectFromPath(projectRoot);
        }
    }

    if (info.suffix().compare("dsl", Qt::CaseInsensitive) == 0) {
        if (loadTextFileToEditor(filePath)) {
            if (m_projectController) {
                m_projectController->setCurrentScriptFile(filePath);
            }
            if (m_editorSubWindow) {
                m_editorSubWindow->show();
                m_editorSubWindow->raise();
                m_mdiArea->setActiveSubWindow(m_editorSubWindow);
            }
            if (m_projectExplorerWidget) {
                m_projectExplorerWidget->revealPath(filePath);
            }
        }
        return;
    }

    const QString canonicalTarget = info.canonicalFilePath();
    const QString currentDslFile = QFileInfo(m_projectController ? m_projectController->currentScriptFile() : QString()).canonicalFilePath();

    if (!currentDslFile.isEmpty() && canonicalTarget == currentDslFile) {
        if (loadTextFileToEditor(filePath)) {
            if (m_editorSubWindow) {
                m_editorSubWindow->show();
                m_editorSubWindow->raise();
                m_mdiArea->setActiveSubWindow(m_editorSubWindow);
            }
            if (m_projectExplorerWidget) {
                m_projectExplorerWidget->revealPath(filePath);
            }
        }
        return;
    }

    openAuxiliaryTextFileInMdi(filePath);
}

void MainWindow::onExplorerFileOpenRequested(const QString& filePath)
{
    openFileFromExplorer(filePath);
}

void MainWindow::onLocateCurrentFileInExplorer()
{
    if (!m_projectExplorerWidget) {
        return;
    }

    if (m_dslEditor && !m_dslEditor->currentFilePath().isEmpty()) {
        m_projectExplorerWidget->revealPath(m_dslEditor->currentFilePath());
        return;
    }

    if (QMdiSubWindow* sub = m_mdiArea->activeSubWindow()) {
        const QString filePath = sub->property("filePath").toString();
        if (!filePath.isEmpty()) {
            m_projectExplorerWidget->revealPath(filePath);
        }
    }
}

// ================= ProjectController 淇″彿澶勭悊妲藉嚱鏁?=================

void MainWindow::onProjectCreated(const QString& projectPath, const QString& projectName)
{
    Q_UNUSED(projectPath);
    Q_UNUSED(projectName);
    updateWindowTitle();
    updateRecentProjectsMenu();
    refreshExplorerRoot();
    refreshInspectorPanel();
}

void MainWindow::onProjectOpened(const ProjectRuntimeConfig& config)
{
    Q_UNUSED(config);
    updateWindowTitle();
    updateRecentProjectsMenu();
    refreshExplorerRoot();

    if (m_dslEditor && m_dslEditor->completionEngine()) {
        const QString projectPath = m_projectController->currentProjectPath();
        m_dslEditor->completionEngine()->reloadSnippets(projectPath);
    }

    if (m_projectExplorerWidget && m_projectController) {
        m_projectExplorerWidget->revealPath(m_projectController->currentScriptFile());
    }
    refreshInspectorPanel();
}

void MainWindow::onProjectSaved()
{
    updateWindowTitle();
    updateEditorSubWindowTitle();
    refreshInspectorPanel();
}

void MainWindow::onProjectClosed()
{
    updateWindowTitle();
    updateEditorSubWindowTitle();

    stopDemoMode(QStringLiteral("项目关闭"));

    if (m_dslEditor && m_dslEditor->completionEngine()) {
        m_dslEditor->completionEngine()->reloadSnippets();
    }

    refreshExplorerRoot();
    refreshInspectorPanel();
}

void MainWindow::onProjectModifiedChanged(bool modified)
{
    Q_UNUSED(modified);
    updateWindowTitle();
    updateEditorSubWindowTitle();
}

void MainWindow::onRecentProjectsChanged(const QStringList& projects)
{
    Q_UNUSED(projects);
    updateRecentProjectsMenu();
}

void MainWindow::onProjectNameRequired(QString& projectName, bool& accepted)
{
    projectName = QInputDialog::getText(this, "新建项目", "项目名称：");
    accepted = !projectName.isEmpty();
}

void MainWindow::onDirectorySelectionRequired(const QString& title, const QString& defaultDir,
                                               QString& selectedDir, bool& accepted)
{
    selectedDir = QFileDialog::getExistingDirectory(this, title, defaultDir);
    accepted = !selectedDir.isEmpty();
}

void MainWindow::onSaveConfirmationRequired(bool& shouldSave, bool& cancelled)
{
    auto ret = QMessageBox::question(
        this, "关闭项目",
        "当前项目有未保存修改，是否立即保存？",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    
    shouldSave = (ret == QMessageBox::Save);
    cancelled = (ret == QMessageBox::Cancel);
}

void MainWindow::onScriptLoadRequired(const QString& scriptPath, const QString& content)
{
    if (m_dslEditor) {
        m_dslEditor->setScript(content);
        m_dslEditor->setCurrentFilePath(scriptPath);
        m_dslEditor->editor()->setReadOnly(false);
        m_dslEditor->setModified(false);
        m_dslEditor->clearInsertRecords();
        m_projectController->syncDslMappingsToEditor();
    }

    if (m_projectExplorerWidget) {
        m_projectExplorerWidget->revealPath(scriptPath);
    }
    refreshInspectorPanel();
}

void MainWindow::onEditorClearRequired()
{
    if (m_dslEditor) {
        m_dslEditor->clearScript();
        m_dslEditor->clearInsertRecords();
        m_projectController->syncDslMappingsToEditor();
        m_dslEditor->setCurrentFilePath(QString());
        m_dslEditor->setModified(false);
    }
    refreshInspectorPanel();
}

void MainWindow::onValidationFailed(const QStringList& errors)
{
    showValidationErrors(errors);
}

// ================= BuildController 淇″彿澶勭悊妲藉嚱鏁?=================

void MainWindow::onCompileStarted(BuildType type)
{
    Q_UNUSED(type);
    m_progressBar->setVisible(true);
    setCompileActionsEnabled(false);
    updateStatusBar("编译中...");
    if (m_globalStatusBar) {
        m_globalStatusBar->setBuildState("编译中");
    }
    refreshInspectorPanel();
}

void MainWindow::onCompileSucceeded(BuildType type)
{
    m_progressBar->setVisible(false);
    setCompileActionsEnabled(true);
    updateStatusBar("编译成功");
    if (m_globalStatusBar) {
        m_globalStatusBar->setBuildState("成功");
    }

    if (type == BuildType::Configuration && m_buildController && m_projectController) {
        const CompileResult result = m_buildController->lastCompileResult();
        ProjectRuntimeConfig& cfg = m_projectController->runtimeConfig();
        if (RunController::writeDownloadArtifact(cfg,
                                                 m_projectController->currentProjectPath(),
                                                 result)) {
            m_projectController->saveProject();
            appendOutput(QString("[%1] 已更新下载产物路径：%2")
                         .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                         .arg(cfg.downloadArtifact.filePath));
        }
    }

    if (m_sessionController->hasPendingRunAfterCompile() && type == BuildType::Configuration) {
        const CompileResult compileResult = m_buildController
                ? m_buildController->lastCompileResult()
                : CompileResult();
        if (m_sessionController->onCompileSucceeded(compileResult)) {
            // 控制器已自动执行运行，更新 UI
            m_projectRunning = true;
            if (m_monitorWidget)
                m_monitorWidget->startMonitoring();
            updateStatusBar("项目运行中");
            if (m_globalStatusBar)
                m_globalStatusBar->setBuildState("运行中");
        } else {
            QMessageBox::warning(this,
                                 "运行条件不完整",
                                 "编译成功，但没有生成可运行的 .code 文件。");
        }
    }

    refreshInspectorPanel();
}

void MainWindow::onCompileFailed(BuildType type, const QString& errorMessage)
{
    Q_UNUSED(type);
    m_sessionController->setPendingRunAfterCompile(false);
    m_progressBar->setVisible(false);
    setCompileActionsEnabled(true);
    updateStatusBar("编译失败");
    if (m_globalStatusBar) {
        m_globalStatusBar->setBuildState("失败");
    }
    addProblem("error", "构建", errorMessage.isEmpty() ? "编译失败" : errorMessage);
    refreshInspectorPanel();
}

void MainWindow::onBuildBusyChanged(bool busy)
{
    setCompileActionsEnabled(!busy);
    m_progressBar->setVisible(busy);
    refreshInspectorPanel();
}

void MainWindow::onBuildSaveRequired()
{
    if (m_sessionController && m_sessionController->skipNextBuildSave()) {
        m_sessionController->setSkipNextBuildSave(false);
        m_lastBuildSaveSucceeded = true;
        appendOutput(QString("[%1] 已按用户选择跳过保存，编译将使用磁盘上的 DSL 文件")
                     .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        return;
    }

    bool saveSucceeded = true;

    // Build flow must persist the main DSL script, regardless of the active MDI subwindow.
    if (m_dslEditor && !m_projectController->currentScriptFile().isEmpty()) {
        m_projectController->syncDslMappingsFromEditor();
        m_projectController->syncDslMappingsToEditor();

        const QString scriptForSave = m_dslEditor->scriptForSave();
        QFile scriptFile(m_projectController->currentScriptFile());
        if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&scriptFile);
            TextEncoding::setUtf8(stream);
            stream << scriptForSave;
            scriptFile.close();
            if (scriptForSave != m_dslEditor->currentScript()) {
                m_dslEditor->setScript(scriptForSave);
            }
            m_dslEditor->setModified(false);
        } else {
            saveSucceeded = false;
            appendOutput(QString("[%1] 保存 DSL 脚本失败: %2")
                         .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                         .arg(m_projectController->currentScriptFile()));
        }
    }

    if (saveSucceeded) {
        saveSucceeded = m_projectController->saveProject();
    }

    m_lastBuildSaveSucceeded = saveSucceeded;
    refreshInspectorPanel();
}

void MainWindow::onBuildValidationRequired(BuildType type, QStringList& errors, bool& valid)
{
    valid = onBuildValidation(type, errors);
}

bool MainWindow::onBuildValidation(BuildType type, QStringList& errors)
{
    if (!m_lastBuildSaveSucceeded) {
        errors << "保存当前 DSL 脚本失败，已取消编译。";
        showValidationErrors(errors);
        return false;
    }

    if (type == BuildType::Configuration) {
        bool valid = m_projectController->validateConfiguration(errors);
        if (!valid) {
            showValidationErrors(errors);
        }
        return valid;
    }

    return true;
}

// ================= SettingsController 淇″彿澶勭悊妲藉嚱鏁?=================

void MainWindow::onFontSizeChanged(int pointSize)
{
    applyFontSize(pointSize);
}

// ================= 閫氱敤娑堟伅澶勭悊妲藉嚱鏁?=================

void MainWindow::onLogMessage(const QString& message)
{
    appendOutput(message);
    const QString lower = message.toLower();
    if (lower.contains("error") || lower.contains("失败")) {
        addProblem("error", "系统", message);
    } else if (lower.contains("warn") || lower.contains("alarm")) {
        addProblem("warning", "系统", message);
    }
}

void MainWindow::onErrorOccurred(const QString& title, const QString& message)
{
    addProblem("error", title, message);
    QMessageBox::critical(this, title, message);
}

void MainWindow::onWarningOccurred(const QString& title, const QString& message)
{
    addProblem("warning", title, message);
    QMessageBox::warning(this, title, message);
}

void MainWindow::onMonitorThresholdExceeded(const QString& channelName, double value, double thresholdValue)
{
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    appendOutput(QString("[%1] [ALARM] 阈值超限: %2 当前值=%3 阈值=%4")
                    .arg(ts)
                    .arg(channelName)
                     .arg(value, 0, 'f', 3)
                     .arg(thresholdValue, 0, 'f', 3));
    addProblem("error", "监控", QString("%1 value=%2 threshold=%3")
               .arg(channelName)
               .arg(value, 0, 'f', 3)
               .arg(thresholdValue, 0, 'f', 3));
}



