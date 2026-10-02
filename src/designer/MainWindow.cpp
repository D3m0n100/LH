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
#include "OpcServerSettingsDialog.h"
#include "MonitorWidget.h"
#include "DownloadDockWidget.h"
#include "DeviceWorkspaceWidget.h"
#include "ProjectExplorerWidget.h"
#include "ProgramBlocksWidget.h"
#include "MonitorManager.h"
#include "RuntimeMonitorAdapter.h"
#include "../communication/IDeviceBackend.h"
#include "../communication/IOpcServer.h"
#include "../communication/DownloadProfile.h"
#include "ProfileResolution.h"
#include "SampleDataProvider.h"
#include "ui/ThemeManager.h"
#include "ui/GlobalStatusBar.h"
#include "ui/InspectorPanel.h"
#include "ui/StatusTextHelper.h"
#include "ui/ProblemsPanel.h"
#include "ui/CommandPaletteDialog.h"
#include <QUuid>
#include "ParameterTuningPanel.h"
#include "ParameterTuningWindow.h"
#include "ParameterController.h"
#include "RuntimeSessionController.h"
#include "core/AppLogging.h"
#include "../diagnostics/DiagnosticSnapshotService.h"
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
#include <QTabBar>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#include <QEvent>
#include <QCoreApplication>
#include <QStyle>
#include <QIcon>
#include <QSize>
#include <QToolButton>
#include <QApplication>
#include <QMenu>
#include <QTextStream>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>

namespace MainWindowStatusFormatting {
QString formatOpcStatusDetails(const BackendStatusSnapshot& status);
}

// ================= 构造 / 析构 =================

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
    m_projectSessionId = QUuid::createUuid().toString();
    resize(1500, 900);
    setWindowTitle("LH v1.0.0 - DSL组态");

    createControllers();

    // 创建 UI
    createActions();
    createMenus();
    createToolBars();
    createStatusBar();
    createDockWidgets();
    createInspectorDock();
    createTuningDock();
    createParameterTuningWindow();

    // 建立信号连接
    initConnections();
    connectControllerSignals();

    ThemeManager::applyModernTheme(qApp);

    // 监控告警：把阈值超限信号写入输出日志，便于追踪
    {
        auto& manager = Monitor::MonitorManager::instance();
        connect(&manager, &Monitor::MonitorManager::thresholdExceeded,
                this, &MainWindow::onMonitorThresholdExceeded);
    }

    m_sampleDataProvider = new SampleDataProvider(this);
    if (m_sessionController) {
        m_sessionController->setSampleDataProvider(m_sampleDataProvider);
    }
    if (m_monitorWidget) {
        m_monitorWidget->setSampleDataProvider(m_sampleDataProvider);
    }
    // SampleDataProvider 直接向 MonitorManager 写样本，不依赖 MonitorWidget
    // 应用初始设置
    applyFontSize(m_settingsController->currentFontPointSize());
    updateRecentProjectsMenu();

    updateStatusBar(QStringLiteral("就绪"));
    if (m_globalStatusBar) {
        m_globalStatusBar->setBuildState(QStringLiteral("空闲"));
        m_globalStatusBar->setConnectionState(false);
        m_globalStatusBar->setProjectName("无");
        m_globalStatusBar->setOpcState(false);
    }
    refreshInspectorPanel();
    restoreWindowGeometryAndWorkspaces();
    // Keep the optional tuning dock out of the horizontal minimum-size
    // calculation after restoring a previous layout. It is moved to the right
    // on demand for wide windows by onOpenParameterTuningWindow().
    if (m_tuningDock && m_tuningDock->isHidden()) {
        addDockWidget(Qt::BottomDockWidgetArea, m_tuningDock);
        m_tuningDock->hide();
    }

    LOG_INFO("MainWindow 初始化完成");
}

MainWindow::~MainWindow()
{
    if (qApp) {
        qApp->disconnect(this);
    }
    if (m_mdiArea && m_editorSubWindow) {
        m_editorSubWindow->disconnect(this);
        m_mdiArea->removeSubWindow(m_dslEditor);
        m_editorSubWindow->deleteLater();
        m_editorSubWindow = nullptr;
    }
    if (m_dslEditor) {
        m_dslEditor->setParent(this);
    }
    m_parameterTuningWindow = nullptr; // parent 会在 Qt 对象树中删除它
    saveWindowGeometryAndWorkspaces();
    m_settingsController->saveSettings();
    m_projectController->saveRecentProjects();

    LOG_INFO("MainWindow 已销毁");
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!confirmAuxiliaryChanges()) {
        event->ignore();
        return;
    }
    if (m_projectController->hasOpenProject() && m_projectController->isModified()) {
        if (!m_projectController->closeProject()) {
            event->ignore();
            return;
        }
    } else {
        const auto ret = QMessageBox::question(
            this,
            "退出",
            "确定要退出 LH 吗？",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (ret != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }

    if (m_sessionController) {
        m_sessionController->requestStop();
    }
    saveWindowGeometryAndWorkspaces();
    m_settingsController->saveSettings();
    m_projectController->saveRecentProjects();
    event->accept();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    auto* sub = qobject_cast<QMdiSubWindow*>(watched);
    if (sub && sub == m_editorSubWindow) {
        if (event->type() == QEvent::Close) {
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
        } else if (event->type() == QEvent::Show) {
            if (m_dslEditor && m_dslEditor->isHidden()) {
                m_dslEditor->show();
            }
            if (m_actToggleDslEditor) {
                m_actToggleDslEditor->blockSignals(true);
                m_actToggleDslEditor->setChecked(true);
                m_actToggleDslEditor->blockSignals(false);
            }
            if (m_actOpenDslEditorToolBar) {
                m_actOpenDslEditorToolBar->blockSignals(true);
                m_actOpenDslEditorToolBar->setChecked(true);
                m_actOpenDslEditorToolBar->blockSignals(false);
            }
        }
    }
    if (sub && event->type() == QEvent::Close && sub != m_editorSubWindow
            && sub->property("modified").toBool()) {
        const auto choice = showMessageBox(QStringLiteral("未保存修改"),
                                            QStringLiteral("文件尚未保存，是否保存？"),
                                            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                            QMessageBox::Save);
        if (choice == QMessageBox::Cancel) return true;
        if (choice == QMessageBox::Save && !saveAuxiliarySubWindow(sub)) return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::saveAuxiliarySubWindow(QMdiSubWindow* sub)
{
    if (!sub || sub == m_editorSubWindow) return true;
    const QString path = sub->property("filePath").toString();
    auto* editor = qobject_cast<QPlainTextEdit*>(sub->widget());
    if (path.isEmpty() || !editor) return true;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showMessageBox(QStringLiteral("保存失败"), QStringLiteral("无法保存文件: %1").arg(path));
        return false;
    }
    QTextStream out(&file);
    TextEncoding::setUtf8(out);
    out << editor->toPlainText();
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit()) {
        file.cancelWriting();
        showMessageBox(QStringLiteral("保存失败"), QStringLiteral("写入文件失败: %1").arg(path));
        return false;
    }
    sub->setProperty("modified", false);
    if (editor) {
        editor->document()->setModified(false);
    }
    sub->setWindowTitle(QFileInfo(path).fileName());
    return true;
}

int MainWindow::showMessageBox(const QString& title, const QString& message,
                               QMessageBox::StandardButtons buttons,
                               QMessageBox::StandardButton defaultButton)
{
    if (m_messageBoxHook) {
        return m_messageBoxHook(title, message, buttons, defaultButton);
    }
    return QMessageBox::warning(this, title, message, buttons, defaultButton);
}

bool MainWindow::saveAuxiliaryFiles(bool all)
{
    if (!m_mdiArea) return true;
    for (QMdiSubWindow* sub : m_mdiArea->subWindowList()) {
        if (!sub || sub == m_editorSubWindow) continue;
        auto* editor = qobject_cast<QPlainTextEdit*>(sub->widget());
        const bool isDirty = sub->property("modified").toBool()
                || (editor && editor->document()->isModified());
        if (isDirty && (all || sub == m_mdiArea->activeSubWindow())) {
            if (!saveAuxiliarySubWindow(sub)) return false;
        }
    }
    return true;
}

bool MainWindow::confirmAuxiliaryChanges()
{
    if (!m_mdiArea) return true;
    bool dirty = false;
    for (QMdiSubWindow* sub : m_mdiArea->subWindowList()) {
        if (sub && sub != m_editorSubWindow) {
            auto* editor = qobject_cast<QPlainTextEdit*>(sub->widget());
            if (sub->property("modified").toBool()
                    || (editor && editor->document()->isModified())) {
                dirty = true;
                break;
            }
        }
    }
    if (!dirty) return true;
    const auto choice = showMessageBox(QStringLiteral("未保存修改"),
                                       QStringLiteral("附属文件有未保存修改，是否保存全部？"),
                                       QMessageBox::SaveAll | QMessageBox::Discard | QMessageBox::Cancel,
                                       QMessageBox::SaveAll);
    if (choice == QMessageBox::Cancel) return false;
    return choice == QMessageBox::Discard || saveAuxiliaryFiles(true);
}

// ================= 控制器创建 =================

void MainWindow::createControllers()
{
    m_settingsController = new SettingsController(this);
    m_settingsController->loadSettings();

    m_projectController = new ProjectController(this);
    m_projectController->setDefaultProjectDir(m_settingsController->defaultProjectDir());
    m_projectController->loadRecentProjects();

    m_buildController = new BuildController(this);

    m_parameterController = new ParameterController(this);

    auto& runtimeMonitor = Monitor::MonitorManager::instance();
    auto* runtimeService = new RuntimeSessionService(
        makeRuntimeMonitorPort(runtimeMonitor), runtimeMonitor.historyStore(), this);
    connect(&runtimeMonitor, &Monitor::MonitorManager::databaseServiceStarted, runtimeService,
            [runtimeService, &runtimeMonitor](bool success, const QString&) {
                if (success) runtimeService->setHistory(runtimeMonitor.historyStore());
            });
    m_sessionController = new RuntimeSessionController(this, runtimeService);
    m_sessionController->setProjectController(m_projectController);
    m_sessionController->setBuildController(m_buildController);
    m_sessionController->setParameterController(m_parameterController);

    LOG_DEBUG("控制器已创建");
}

void MainWindow::connectControllerSignals()
{
    // ===== ProjectController 信号连接 =====
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

    // ===== ParameterController 信号连接 =====
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
                if (newState == ParameterState::Confirmed && !info.readbackValue.isEmpty()) {
                    it->currentValue = info.readbackValue;
                    it->confirmed = true;
                } else if (newState == ParameterState::Mismatch
                           || newState == ParameterState::Timeout
                           || newState == ParameterState::ApplyFailed) {
                    it->currentValue = info.editedValue.isEmpty()
                            ? info.definitionValue
                            : info.editedValue;
                    it->confirmed = false;
                } else {
                    it->currentValue = info.editedValue.isEmpty()
                            ? info.definitionValue
                            : info.editedValue;
                    it->confirmed = false;
                }
            });
    connect(m_parameterController, &ParameterController::statesChanged,
            this, [this]() { refreshInspectorPanel(); });
    connect(m_parameterController, &ParameterController::readbackFinished,
            this, &MainWindow::onParameterReadbackFinished);

    // ===== RuntimeSessionController 信号连接 =====
    const auto refreshRuntimeStatus = [this]() {
        if (!m_sessionController) {
            return;
        }

        const RuntimeSessionState state = m_sessionController->state();
        const DownloadState downloadState = m_sessionController->downloadState();
        const bool downloadActive = downloadState == DownloadState::Precheck
                || downloadState == DownloadState::Downloading
                || downloadState == DownloadState::Retrying
                || downloadState == DownloadState::Verifying;

        QString statusText;
        if (state == RuntimeSessionState::Downloading || downloadActive) {
            statusText = QStringLiteral("下载中");
        } else {
            switch (state) {
            case RuntimeSessionState::Connected:
                statusText = QStringLiteral("已连接");
                break;
            case RuntimeSessionState::Running:
                statusText = QStringLiteral("运行中");
                break;
            case RuntimeSessionState::Monitoring:
                statusText = QStringLiteral("监控中");
                break;
            case RuntimeSessionState::Fault:
                statusText = QStringLiteral("故障");
                break;
            case RuntimeSessionState::Compiled:
                statusText = QStringLiteral("已编译");
                break;
            case RuntimeSessionState::Connecting:
                statusText = QStringLiteral("连接中");
                break;
            case RuntimeSessionState::Idle:
            default:
                statusText = m_sessionController->isDemoMode()
                        ? QStringLiteral("演示模式：采集中")
                        : QStringLiteral("已停止");
                break;
            }
        }

        m_projectRunning = state == RuntimeSessionState::Running
                || state == RuntimeSessionState::Monitoring;
        const bool sessionBusy = state == RuntimeSessionState::Running
                || state == RuntimeSessionState::Monitoring
                || state == RuntimeSessionState::Downloading
                || downloadActive;
        const bool paused = m_sessionController ? m_sessionController->isPaused() : false;
        if (m_actRunProject) {
            m_actRunProject->setEnabled(!sessionBusy);
        }
        if (m_actStopProject) {
            m_actStopProject->setEnabled(sessionBusy
                                         || paused
                                         || state == RuntimeSessionState::Connected
                                         || state == RuntimeSessionState::Fault);
        }
        if (m_actPauseController) {
            m_actPauseController->setEnabled(m_projectRunning && !paused && !m_sessionController->isDebugCommandPending());
        }
        if (m_actResumeController) {
            m_actResumeController->setEnabled(m_projectRunning && paused && !m_sessionController->isDebugCommandPending());
        }
        if (m_actStepController) {
            m_actStepController->setEnabled(m_projectRunning && paused && !m_sessionController->isDebugCommandPending());
        }
        updateStatusBar(statusText);
        updateDeviceWorkspaceInfo();
        if (m_monitorWidget) {
            const bool monitoring = state == RuntimeSessionState::Monitoring
                    && Monitor::MonitorManager::instance().isMonitoring();
            m_monitorWidget->syncMonitoringState(monitoring);
        }
    };

    connect(m_sessionController, &RuntimeSessionController::stateChanged,
            this, [refreshRuntimeStatus](RuntimeSessionState, RuntimeSessionState) {
                refreshRuntimeStatus();
            });
    connect(m_sessionController, &RuntimeSessionController::pausedChanged,
            this, [refreshRuntimeStatus](bool) {
                refreshRuntimeStatus();
            });
    connect(m_sessionController, &RuntimeSessionController::debugCommandPendingChanged,
            this, [refreshRuntimeStatus](bool) { refreshRuntimeStatus(); });
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
    connect(m_sessionController, &RuntimeSessionController::downloadStateChanged,
            this, [refreshRuntimeStatus](DownloadState, DownloadState) {
                refreshRuntimeStatus();
            });
    connect(m_sessionController, &RuntimeSessionController::downloadFinished,
            this, [refreshRuntimeStatus](bool, const QString&) {
                refreshRuntimeStatus();
            });
    connect(m_sessionController, &RuntimeSessionController::monitoringChanged,
            this, [refreshRuntimeStatus](bool) {
                refreshRuntimeStatus();
            });
    connect(m_sessionController, &RuntimeSessionController::demoModeChanged,
            this, [refreshRuntimeStatus](bool) {
                refreshRuntimeStatus();
            });
    connect(m_sessionController, &RuntimeSessionController::downloadDiagnosticChanged,
            this, [this, refreshRuntimeStatus](const QVariantMap& diagnostic) {
                refreshRuntimeStatus();
                m_lastDownloadDiagnostic = diagnostic;
                const QString severity = diagnostic.value(QStringLiteral("severity")).toString();
                const QString message = diagnostic.value(QStringLiteral("message")).toString();
                if (!message.isEmpty()
                        && (severity.compare(QStringLiteral("error"), Qt::CaseInsensitive) == 0
                            || severity.compare(QStringLiteral("warning"), Qt::CaseInsensitive) == 0)) {
                    addProblem(severity, QStringLiteral("下载诊断"), message);
                }
                refreshInspectorPanel();
            });
    connect(m_sessionController, &RuntimeSessionController::opcRunningChanged,
            this, [this](bool running) {
                m_opcRunning = running;
                if (running) {
                    m_lastOpcError.clear();
                }
                if (m_globalStatusBar) {
                    m_globalStatusBar->setOpcState(running, m_lastOpcError);
                }
                refreshInspectorPanel();
            });
    connect(m_sessionController, &RuntimeSessionController::opcErrorOccurred,
            this, [this](const QString& message) {
                m_lastOpcError = message;
                if (m_globalStatusBar) {
                    m_globalStatusBar->setOpcState(m_opcRunning, m_lastOpcError);
                }
                refreshInspectorPanel();
            });
    connect(m_projectController, &ProjectController::scriptLoadRequired,
            this, &MainWindow::onScriptLoadRequired);
    connect(m_projectController, &ProjectController::editorClearRequired,
            this, &MainWindow::onEditorClearRequired);
    connect(m_projectController, &ProjectController::validationFailed,
            this, &MainWindow::onValidationFailed);

    refreshRuntimeStatus();

    // ===== BuildController 信号连接 =====
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
    connect(m_buildController, &BuildController::diagnosticsProduced,
            this, &MainWindow::onDiagnosticsProduced);
    // validationRequired 已改为回调模式，通过 setValidationCallback 设置
    m_buildController->setValidationCallback([this](BuildType type, QStringList& errors) -> bool {
        return onBuildValidation(type, errors);
    });

    // ===== SettingsController 信号连接 =====
    connect(m_settingsController, &SettingsController::fontSizeChanged,
            this, &MainWindow::onFontSizeChanged);
    connect(m_settingsController, &SettingsController::logMessage,
            this, &MainWindow::onLogMessage);
    connect(m_settingsController, &SettingsController::defaultProjectDirChanged,
            m_projectController, &ProjectController::setDefaultProjectDir);

    LOG_DEBUG("控制器信号已连接");
}

// ================= UI 辅助方法 =================

void MainWindow::updateStatusBar(const QString& message)
{
    if (m_statusLabel) {
        m_statusLabel->setText(message);
    }
    if (m_globalStatusBar) {
        m_globalStatusBar->setBuildState(message);
    }
    refreshInspectorPanel();
}

void MainWindow::updateConnectionStatus(bool connected)
{
    if (m_connectionStatusLabel) {
        if (connected) {
            m_connectionStatusLabel->setText("已连接");
        } else {
            m_connectionStatusLabel->setText("未连接");
        }
        m_connectionStatusLabel->setProperty("connected", connected);
        m_connectionStatusLabel->style()->unpolish(m_connectionStatusLabel);
        m_connectionStatusLabel->style()->polish(m_connectionStatusLabel);
    }
    if (m_globalStatusBar) {
        m_globalStatusBar->setConnectionState(connected);
    }
    updateDeviceWorkspaceInfo();
    refreshInspectorPanel();
}

void MainWindow::updateDeviceWorkspaceInfo()
{
    if (!m_deviceWorkspaceWidget) {
        return;
    }

    const bool hasProject = m_projectController && m_projectController->hasOpenProject();
    QString targetName = QStringLiteral("未配置目标");
    QString configSource = QStringLiteral("未打开工程");
    QString address = QStringLiteral("未连接 (离线)");
    QString expertTarget = QStringLiteral("未配置");
    QString downloadProfileStatus = QStringLiteral("未配置下载目标");
    QString downloadTooltip;

    if (hasProject) {
        const ProjectRuntimeConfig config = m_projectController->runtimeConfig();
        if (!config.target.model.isEmpty()) {
            targetName = config.target.model;
            configSource = config.projectName.isEmpty() ? QStringLiteral("默认配置") : QStringLiteral("工程配置: ") + config.projectName;
        } else if (!config.target.family.isEmpty()) {
            targetName = config.target.family;
            configSource = config.projectName.isEmpty() ? QStringLiteral("默认配置") : QStringLiteral("工程配置: ") + config.projectName;
        } else {
            targetName = QStringLiteral("未配置目标型号");
            configSource = config.projectName.isEmpty() ? QStringLiteral("默认配置") : QStringLiteral("工程配置: ") + config.projectName;
        }

        if (!config.commParameters.isEmpty()) {
            QStringList parts;
            if (config.commParameters.contains(QStringLiteral("port"))) {
                const QString p = config.commParameters.value(QStringLiteral("port")).toString().trimmed();
                if (!p.isEmpty()) {
                    parts << QStringLiteral("串口: %1").arg(p);
                }
            }
            if (config.commParameters.contains(QStringLiteral("baudRate"))) {
                parts << QStringLiteral("波特率: %1").arg(config.commParameters.value(QStringLiteral("baudRate")).toString());
            }
            if (config.commParameters.contains(QStringLiteral("ip"))) {
                const QString ip = config.commParameters.value(QStringLiteral("ip")).toString().trimmed();
                if (!ip.isEmpty()) {
                    parts << QStringLiteral("IP: %1").arg(ip);
                }
            }
            if (!parts.isEmpty()) {
                address = parts.join(QStringLiteral(" | "));
            } else {
                address = config.protocol.isEmpty() ? QStringLiteral("未配置通信参数") : config.protocol;
            }
        } else if (!config.protocol.isEmpty()) {
            address = config.protocol;
        } else if (m_sessionController && m_sessionController->isDemoMode()) {
            address = QStringLiteral("演示虚拟通道");
        } else {
            address = QStringLiteral("未配置通信参数");
        }

        // 解析并校验下载 Profile
        const QVariantMap resolvedProfile = resolveDownloadProfileOptions(
                QVariantMap(), config, m_projectController->currentProjectPath());
        const QString configuredProfilePath = resolvedProfile.value(
                QStringLiteral("downloadProfilePath")).toString().trimmed();

        if (!configuredProfilePath.isEmpty()) {
            const QString projectPath = m_projectController->currentProjectPath();
            const QString projectDir = QFileInfo(projectPath).isDir() ? projectPath : QFileInfo(projectPath).absolutePath();
            QFileInfo fi(configuredProfilePath);
            QString absPath = configuredProfilePath;
            if (fi.isRelative() && !projectDir.isEmpty()) {
                absPath = QDir(projectDir).absoluteFilePath(configuredProfilePath);
                fi.setFile(absPath);
            }
            if (!fi.exists()) {
                downloadProfileStatus = QStringLiteral("无效Profile: 文件不存在");
                downloadTooltip = QStringLiteral("下载Profile路径不存在: %1").arg(absPath);
            } else {
                DownloadProfile profile;
                QString err;
                if (!DownloadProfile::fromJsonFile(absPath, profile, &err)) {
                    downloadProfileStatus = QStringLiteral("无效Profile: 解析失败");
                    downloadTooltip = QStringLiteral("下载Profile解析失败: %1").arg(err);
                } else {
                    int commStationId = -1;
                    if (config.commParameters.contains(QStringLiteral("stationId"))) {
                        commStationId = config.commParameters.value(QStringLiteral("stationId")).toInt();
                    } else if (config.commParameters.contains(QStringLiteral("slaveId"))) {
                        commStationId = config.commParameters.value(QStringLiteral("slaveId")).toInt();
                    }
                    if (commStationId > 0 && profile.slaveId != commStationId) {
                        downloadProfileStatus = QStringLiteral("Profile与通信目标不一致: 站号(%1 vs %2)")
                                                    .arg(profile.slaveId).arg(commStationId);
                        downloadTooltip = QStringLiteral("下载Profile站号(%1)与项目通信参数站号(%2)不匹配").arg(profile.slaveId).arg(commStationId);
                    } else {
                        const QString displayName = profile.name.isEmpty() ? fi.fileName() : profile.name;
                        downloadProfileStatus = QStringLiteral("有效Profile: %1 (站号: %2, 步骤: %3)")
                                                    .arg(displayName)
                                                    .arg(profile.slaveId)
                                                    .arg(profile.steps.size());
                        downloadTooltip = QStringLiteral("Profile路径: %1").arg(absPath);
                    }
                }
            }
        } else {
            downloadProfileStatus = QStringLiteral("未配置下载Profile（仅离线编译）");
            downloadTooltip = QStringLiteral("未配置下载Profile；当前项目仅支持离线编译，配置 Profile 后才能生成并发布下载包");
        }
    } else {
        downloadProfileStatus = QStringLiteral("未打开工程");
    }

    auto* dlWidget = m_deviceWorkspaceWidget->downloadWidget();
    if (dlWidget) {
        const QString dlPort = dlWidget->selectedPort();
        const int stationId = dlWidget->targetStationId();
        if (m_deviceWorkspaceWidget->isExpertDiagnosticVisible()) {
            if (dlPort.isEmpty()) {
                expertTarget = QStringLiteral("专家手动目标: 未选择端口 (ID: %1)").arg(stationId);
            } else {
                expertTarget = QStringLiteral("专家手动目标: %1 (ID: %2)").arg(dlPort).arg(stationId);
            }
        } else {
            if (dlPort.isEmpty()) {
                expertTarget = QStringLiteral("未展开 / 未配置端口");
            } else {
                expertTarget = QStringLiteral("专家预设: %1 (ID: %2)").arg(dlPort).arg(stationId);
            }
        }
    }

    m_deviceWorkspaceWidget->setTargetInfo(targetName, configSource, address, expertTarget, downloadProfileStatus);
    if (m_deviceWorkspaceWidget->downloadProfileValue()) {
        m_deviceWorkspaceWidget->downloadProfileValue()->setToolTip(downloadTooltip);
    }

    bool connected = false;
    QString connectionText = QStringLiteral("离线");
    bool running = false;
    bool paused = false;

    if (m_sessionController) {
        const RuntimeSessionState state = m_sessionController->state();
        connected = (state == RuntimeSessionState::Connected
                     || state == RuntimeSessionState::Running
                     || state == RuntimeSessionState::Monitoring
                     || (m_sessionController->isDemoMode() && state != RuntimeSessionState::Idle));
        if (connected) {
            connectionText = m_sessionController->isDemoMode() ? QStringLiteral("演示连接") : QStringLiteral("已连接");
        } else if (state == RuntimeSessionState::Connecting) {
            connectionText = QStringLiteral("连接中");
        } else if (state == RuntimeSessionState::Fault) {
            connectionText = QStringLiteral("通信故障");
        }

        running = (state == RuntimeSessionState::Running || state == RuntimeSessionState::Monitoring);
        paused = m_sessionController->isPaused();
    }

    m_deviceWorkspaceWidget->setConnectionStatus(connected, connectionText);
    m_deviceWorkspaceWidget->setControllerRunning(running, paused);
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
    updateDeviceWorkspaceInfo();
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


// ================= 项目操作槽函数（转发给 ProjectController） =================

void MainWindow::onNewProject()
{
    if (!confirmAuxiliaryChanges()) return;
    m_projectController->createNewProject();
}

void MainWindow::onOpenProject()
{
    if (!confirmAuxiliaryChanges()) return;
    m_projectController->openProject();
}

void MainWindow::onSaveProject()
{
    m_projectCommands.save(false, {
        [this](bool all) { return saveAuxiliaryFiles(all); },
        [this] { return m_projectController->saveProject(); }
    });
}

void MainWindow::onSaveAll()
{
    m_projectCommands.save(true, {
        [this](bool all) { return saveAuxiliaryFiles(all); },
        [this] { return m_projectController->saveProject(); }
    });
}

void MainWindow::onCloseProject()
{
    if (!confirmAuxiliaryChanges()) return;
    m_projectController->closeProject();
}

void MainWindow::onRecentProjectTriggered()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action && confirmAuxiliaryChanges()) {
        m_projectController->openRecentProject(action->data().toString());
    }
}

// ================= 编辑操作槽函数 =================

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

// ================= 视图操作槽函数 =================

void MainWindow::onToggleDownloadDock(bool checked)
{
    switchToWorkspace(checked ? WorkspaceId::Device : WorkspaceId::Programming);
}

void MainWindow::onOpenDownloadWindow()
{
    switchToWorkspace(WorkspaceId::Device);
    if (m_deviceWorkspaceWidget) {
        m_deviceWorkspaceWidget->setExpertDiagnosticVisible(true);
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

void MainWindow::onToggleInspectorDock(bool checked)
{
    if (m_inspectorDock) {
        m_inspectorDock->setVisible(checked);
    }
}

void MainWindow::onToggleDslEditor(bool checked)
{
    if (checked) {
        if (!m_editorSubWindow) {
            createDslEditorSubWindow();
        } else {
            if (m_dslEditor) {
                m_dslEditor->show();
            }
            m_editorSubWindow->show();
            m_editorSubWindow->raise();
            m_editorSubWindow->activateWindow();
            if (m_mdiArea) {
                m_mdiArea->setActiveSubWindow(m_editorSubWindow);
            }
            if (m_dslEditor) {
                m_dslEditor->setFocus();
            }
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
    onResetAllWorkspaceLayouts();
}

void MainWindow::onDslEditorSubWindowDestroyed()
{
    m_editorSubWindow = nullptr;
    m_dslEditor = nullptr;
    m_projectController->setDslEditor(nullptr);
    bindFunctionLibraryDataSource();

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

// ================= 设置操作槽函数 =================

void MainWindow::onOpenSettings()
{
    m_settingsController->openSettingsDialog(this);
}

void MainWindow::onOpenOpcServerSettings()
{
    if (!m_projectController || !m_projectController->hasOpenProject()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先打开或创建项目。"));
        return;
    }

    OpcServerSettingsDialog dialog(this);
    ProjectRuntimeConfig& cfg = m_projectController->runtimeConfig();
    dialog.setConfig(cfg.opcServer);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    cfg.opcServer = dialog.config();
    m_projectController->setModified(true);

    appendOutput(QStringLiteral("[%1] OPC 服务设置已更新：enabled=%2 progId=%3 channel=%4 device=%5 mode=%6")
                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                      cfg.opcServer.enabled ? QStringLiteral("true") : QStringLiteral("false"),
                      cfg.opcServer.opcProgId,
                      cfg.opcServer.channelName,
                      cfg.opcServer.deviceName,
                      cfg.opcServer.serialMode));
    refreshInspectorPanel();
}

// ================= 编译操作槽函数（转发给 BuildController） =================

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
    if (!m_projectController->hasOpenProject()) {
        QMessageBox::warning(this, "警告", "请先打开或创建项目。");
        return;
    }

    if (m_sessionController->isRunning()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("项目已在运行中。"));
        return;
    }

    // 保存未保存的修改
    const bool hasUnsavedChanges =
            (m_projectController && m_projectController->isModified())
            || (m_dslEditor && m_dslEditor->isModified());
    if (hasUnsavedChanges) {
        onBuildSaveRequired();
    }

    // 设置在编译并发布成功后触发运行流程
    m_sessionController->setPendingRunAfterCompile(true);
    m_buildController->compileConfiguration(m_projectController->currentProjectPath(),
                                           m_projectController->runtimeConfig());
}

// ================= 运行控制槽函数 =================

void MainWindow::onRunProject()
{
    // 前置校验（含 UI 弹窗）
    if (!m_projectController->hasOpenProject()) {
        QMessageBox::warning(this, "警告", "请先打开或创建项目。");
        return;
    }

    if (m_sessionController->isRunning()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("项目已在运行中。"));
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

    // 执行运行
    m_sessionController->executeRun();
    if (m_sessionController->state() == RuntimeSessionState::Running) {
        m_sessionController->startMonitoring();
    }
}

void MainWindow::onStopProject()
{
    if (!m_sessionController) {
        return;
    }

    const bool sessionIdle = m_sessionController->state() == RuntimeSessionState::Idle
            && m_sessionController->downloadState() == DownloadState::Idle;
    if (!sessionIdle) {
        m_sessionController->requestStop();
        return;
    }

    // 保留无运行会话时的演示模式停止行为。
    if (m_sessionController->isDemoMode()) {
        stopDemoMode(QStringLiteral("停止项目"));
        if (m_monitorWidget) {
            m_monitorWidget->stopMonitoring();
        }
    }
}

void MainWindow::onPauseController()
{
    if (!m_sessionController) {
        return;
    }
    m_sessionController->pauseController();
}

void MainWindow::onResumeController()
{
    if (!m_sessionController) {
        return;
    }
    m_sessionController->resumeController();
}

void MainWindow::onStepController()
{
    if (!m_sessionController) {
        return;
    }
    m_sessionController->stepController();
}

void MainWindow::onRunControllerToCursor()
{
    if (!m_sessionController) {
        return;
    }
    const int lineNumber = m_dslEditor ? m_dslEditor->currentLineNumber() : 1;
    m_sessionController->runControllerToCursor(lineNumber);
}

void MainWindow::onTestControllerConnection()
{
    if (!m_sessionController) {
        return;
    }
    m_sessionController->testControllerConnection();
}

// ================= 监控操作槽函数 =================

void MainWindow::onOpenParameterTuningWindow()
{
    if (!m_parameterTuningPanel) {
        createTuningDock();
    }
    refreshInspectorPanel(m_parameterTuningPanel);
    if (m_parameterTuningPanel && m_parameterTuningPanel->isStandaloneMode()) {
        if (!m_parameterTuningWindow) {
            createParameterTuningWindow();
            m_parameterTuningWindow->setTuningPanel(m_parameterTuningPanel);
        }
        m_parameterTuningWindow->show();
        m_parameterTuningWindow->raise();
        m_parameterTuningWindow->activateWindow();
    } else if (m_tuningDock) {
        // A side-by-side tuning dock can make a narrow logical viewport
        // impossible. Move it below the workspace so both panels remain
        // reachable without clamping the main window width.
        if (width() < 1200 && dockWidgetArea(m_tuningDock) != Qt::BottomDockWidgetArea) {
            addDockWidget(Qt::BottomDockWidgetArea, m_tuningDock);
        } else if (width() >= 1200 && dockWidgetArea(m_tuningDock) != Qt::RightDockWidgetArea) {
            addDockWidget(Qt::RightDockWidgetArea, m_tuningDock);
            resizeDocks({m_tuningDock}, {340}, Qt::Horizontal);
        }
        m_tuningDock->show();
        m_tuningDock->raise();
        if (dockWidgetArea(m_tuningDock) == Qt::BottomDockWidgetArea) {
            adjustBottomTuningDockHeight();
        }
    }
}

void MainWindow::adjustBottomTuningDockHeight()
{
    if (!m_tuningDock || dockWidgetArea(m_tuningDock) != Qt::BottomDockWidgetArea) {
        return;
    }
    // Calculate total vertical height available between top bars and bottom status bar
    int topBars = 0;
    if (menuBar() && menuBar()->isVisible()) topBars += menuBar()->height();
    if (m_mainToolBar && m_mainToolBar->isVisible()) topBars += m_mainToolBar->height();
    int bottomBars = 0;
    if (statusBar() && statusBar()->isVisible()) bottomBars += statusBar()->height();

    int availableHeight = height() - topBars - bottomBars;
    if (availableHeight <= 100) {
        availableHeight = height() - 85;
    }

    int monitorMinHeight = 260;
    if (m_monitorWidget) {
        int tabHeaderHeight = 36;
        if (m_workspaceTabs && m_workspaceTabs->tabBar()) {
            tabHeaderHeight = qMax(30, m_workspaceTabs->tabBar()->sizeHint().height());
        }
        monitorMinHeight = m_monitorWidget->minimumReadableHeight() + tabHeaderHeight;
    }

    const int targetDockHeight = qBound(80, availableHeight - monitorMinHeight, 280);
    resizeDocks({m_tuningDock}, {targetDockHeight}, Qt::Vertical);
}

// ================= 其他槽函数 =================

void MainWindow::onOpenLogDirectory()
{
    const QString logDir = AppLogging::logDirectory();
    if (!AppLogging::isAvailable()) {
        QMessageBox::warning(this, QStringLiteral("日志"),
                             QStringLiteral("持久化日志不可用：%1")
                                 .arg(AppLogging::lastError()));
        return;
    }
    if (logDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("日志"),
                             QStringLiteral("无法确定可写的日志目录。"));
        return;
    }
    QDir dir(logDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        QMessageBox::warning(this, QStringLiteral("日志"),
                             QStringLiteral("无法创建日志目录：%1").arg(logDir));
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(logDir))) {
        QMessageBox::warning(this, QStringLiteral("日志"),
                             QStringLiteral("无法打开日志目录：%1").arg(logDir));
    }
}

void MainWindow::onOpenDiagnosisWizard()
{
    if (m_bottomPanels && m_problemsPanel) {
        m_bottomPanels->setCurrentWidget(m_problemsPanel);
    }

    QString snapshotPath;
    QString snapshotError;
    if (m_projectController && m_projectController->hasOpenProject()) {
        QVariantMap opcStatus;
        if (m_sessionController && m_sessionController->opcServer()) {
            const auto status = m_sessionController->opcServer()->statusSnapshot();
            opcStatus = status.extras;
            opcStatus.insert(QStringLiteral("online"), status.online);
            opcStatus.insert(QStringLiteral("backendType"), status.backendType);
            opcStatus.insert(QStringLiteral("lastErrorCode"), static_cast<int>(status.lastErrorCode));
            opcStatus.insert(QStringLiteral("lastErrorMessage"), status.lastErrorMessage);
            opcStatus.insert(QStringLiteral("statusText"), MainWindowStatusFormatting::formatOpcStatusDetails(status));
            m_lastOpcStatusExtras = opcStatus;
        }

        const QString diagDir = QDir(m_projectController->currentProjectPath())
                                        .filePath(QStringLiteral("diagnostics"));
        DiagnosticSnapshotService::exportSnapshot(diagDir,
                                                  m_projectController->runtimeConfig(),
                                                  m_opcRunning,
                                                  m_lastOpcError,
                                                  m_lastOpcStatusExtras,
                                                  &snapshotPath,
                                                  &snapshotError);
    }

    if (m_problemsPanel) {
        QString summary = QStringLiteral("诊断摘要：先看错误，再看警告；若问题面板为空，优先检查编译、下载和运行日志。");
        summary += QStringLiteral("\nOPC 状态：%1").arg(m_opcRunning ? QStringLiteral("运行中") : QStringLiteral("关闭"));
        if (!m_lastOpcError.isEmpty()) {
            summary += QStringLiteral("\nOPC 最近错误：%1").arg(m_lastOpcError);
        }
        if (!snapshotPath.isEmpty()) {
            summary += QStringLiteral("\n诊断快照：%1").arg(QDir::toNativeSeparators(snapshotPath));
        } else if (!snapshotError.isEmpty()) {
            summary += QStringLiteral("\n快照导出失败：%1").arg(snapshotError);
        }
        m_problemsPanel->setDiagnosticSummary(summary);
    }

    if (!snapshotPath.isEmpty()) {
        appendOutput(QStringLiteral("诊断向导：已导出诊断快照 %1").arg(QDir::toNativeSeparators(snapshotPath)));
    } else {
        appendOutput("诊断向导：请优先查看问题面板中的错误和警告，再检查编译、下载和运行日志。");
    }
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
        appendOutput(QStringLiteral("设备后端未连接，无法应用参数。"));
        updateStatusBar(QStringLiteral("参数应用失败：设备离线"));
        return;
    }

    QString errorMessage;
    const bool ok = m_parameterController->applyModifiedParametersWithReadbackAsync(
        backend,
        2,
        100,
        &errorMessage);
    if (ok) {
        appendOutput(QStringLiteral("参数已下发，正在等待回读确认。"));
        updateStatusBar(QStringLiteral("参数回读处理中"));
    } else {
        if (errorMessage.isEmpty()) {
            errorMessage = QStringLiteral("参数下发失败");
        }
        appendOutput(QString("参数下发/回读失败：%1").arg(errorMessage));
        updateStatusBar(QStringLiteral("参数下发/回读失败"));
    }
    refreshInspectorPanel();
}

void MainWindow::onParameterReadbackFinished(bool success, const QString& message)
{
    if (success) {
        appendOutput(QStringLiteral("参数已下发并完成回读确认。"));
        updateStatusBar(QStringLiteral("参数下发与回读完成"));
    } else {
        const QString text = message.isEmpty()
                ? QStringLiteral("参数回读失败")
                : message;
        appendOutput(QString("参数下发/回读失败：%1").arg(text));
        updateStatusBar(QStringLiteral("参数下发/回读失败"));
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

// ================= 编辑器相关槽函数 =================

void MainWindow::onEditorCursorPositionChanged(int line, int column, int totalLines)
{
    if (m_editorPositionLabel) {
        m_editorPositionLabel->setText(QString("行 %1, 列 %2 / 共 %3 行")
                                       .arg(line).arg(column).arg(totalLines));
    }
}

void MainWindow::onEditorModified(bool modified)
{
    if (m_projectController) {
        m_projectController->setModified(modified);
    }

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

// ================= ProjectController 信号处理槽函数 =================

void MainWindow::onProjectCreated(const QString& projectPath, const QString& projectName)
{
    Q_UNUSED(projectPath);
    Q_UNUSED(projectName);
    m_projectSessionId = QUuid::createUuid().toString();
    m_documentVersions.clear();
    m_latestDiagnosticGeneration = 0;
    m_activeScanTaskId = 0;

    updateWindowTitle();
    updateRecentProjectsMenu();
    refreshExplorerRoot();
    refreshInspectorPanel();
}

void MainWindow::onProjectOpened(const ProjectRuntimeConfig& config)
{
    Q_UNUSED(config);
    m_projectSessionId = QUuid::createUuid().toString();
    m_documentVersions.clear();
    m_latestDiagnosticGeneration = 0;
    m_activeScanTaskId = 0;

    if (m_mdiArea) {
        for (QMdiSubWindow* sub : m_mdiArea->subWindowList()) {
            if (sub && sub != m_editorSubWindow && !sub->property("modified").toBool()) sub->close();
        }
    }
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
    updateDeviceWorkspaceInfo();
}

void MainWindow::onProjectSaved()
{
    updateWindowTitle();
    updateEditorSubWindowTitle();
    refreshInspectorPanel();
}

void MainWindow::onProjectClosed()
{
    m_projectSessionId = QUuid::createUuid().toString();
    m_documentVersions.clear();
    m_latestDiagnosticGeneration = 0;
    m_activeScanTaskId = 0;

    if (m_sessionController) {
        m_sessionController->requestStop();
    }

    if (m_mdiArea) {
        for (QMdiSubWindow* sub : m_mdiArea->subWindowList()) {
            if (sub && sub != m_editorSubWindow && !sub->property("modified").toBool()) sub->close();
        }
    }
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
        this, "未保存修改",
        "当前项目有未保存修改，是否立即保存？",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    shouldSave = (ret == QMessageBox::Save);
    cancelled = (ret == QMessageBox::Cancel);
}

void MainWindow::onScriptLoadRequired(const QString& scriptPath, const QString& content)
{
    if (m_dslEditor) {
        m_dslEditor->setCurrentFilePath(scriptPath);
        m_dslEditor->setScript(content);
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

// ================= BuildController 信号处理槽函数 =================

void MainWindow::onCompileStarted(BuildType type)
{
    Q_UNUSED(type);
    m_progressBar->setVisible(true);
    setCompileActionsEnabled(false);
    updateStatusBar(QStringLiteral("编译中"));
    if (m_globalStatusBar) {
        m_globalStatusBar->setBuildState(QStringLiteral("编译中"));
    }

    const QString scriptPath = (m_dslEditor && !m_dslEditor->currentFilePath().isEmpty())
        ? normalizeDocumentIdentity(m_dslEditor->currentFilePath())
        : (m_projectController ? normalizeDocumentIdentity(m_projectController->currentScriptFile()) : QString());
    const quint64 docVer = m_documentVersions.value(scriptPath, 0);
    if (m_buildController) {
        m_buildController->setCompileSessionContext(m_projectSessionId, scriptPath, docVer, m_documentVersions);
    }

    refreshInspectorPanel();
}

void MainWindow::onCompileSucceeded(BuildType type)
{
    m_progressBar->setVisible(false);
    setCompileActionsEnabled(true);
    updateStatusBar(QStringLiteral("编译成功"));
    if (m_globalStatusBar) {
        m_globalStatusBar->setBuildState(QStringLiteral("成功"));
    }

    bool publicationCommitted = (type != BuildType::Configuration);
    if (type == BuildType::Configuration && m_buildController && m_projectController) {
        const CompileResult result = m_buildController->lastCompileResult();
        ProjectRuntimeConfig& cfg = m_projectController->runtimeConfig();
        const ProjectRuntimeConfig previousConfig = cfg;
        if (result.metadata.value(QStringLiteral("compileOnly")).toBool()) {
            // Offline compilation must not leave an old artifact available for download.
            cfg.downloadArtifact = DownloadArtifactConfig();
            m_sessionController->setPendingRunAfterCompile(false);
            m_lastBuildSaveSucceeded = m_projectController->saveProject();
            if (!m_lastBuildSaveSucceeded) {
                cfg = previousConfig;
                addProblem("error", "构建", "编译成功，但项目配置保存失败。");
            }
            const QString message = QStringLiteral("编译成功：%1")
                    .arg(result.metadata.value(QStringLiteral("generationDirectory")).toString());
            appendOutput(message);
            addProblem("info", "构建", message);
            refreshInspectorPanel();
            return;
        }
        const bool bundlePrepared = RunController::writeDownloadArtifact(
                cfg,
                m_projectController->currentProjectPath(),
                result);
        if (!bundlePrepared) {
            cfg = previousConfig;
            publicationCommitted = false;
            m_lastBuildSaveSucceeded = false;
            m_sessionController->setPendingRunAfterCompile(false);
            const QString message = QStringLiteral(
                    "编译成功，但 generation 发布失败；旧下载产物和项目配置保持不变。\n"
                    "未启动待运行任务。");
            appendOutput(QStringLiteral("[%1] %2")
                         .arg(QDateTime::currentDateTime().toString("HH:mm:ss"), message));
            addProblem("error", "发布", message);
        } else if (!m_projectController->saveProject()) {
            cfg = previousConfig;
            publicationCommitted = false;
            m_lastBuildSaveSucceeded = false;
            m_sessionController->setPendingRunAfterCompile(false);
            const QString message = QStringLiteral(
                    "编译成功，但项目配置 pointer 提交失败；已恢复旧配置，未启动待运行任务。");
            appendOutput(QStringLiteral("[%1] %2")
                         .arg(QDateTime::currentDateTime().toString("HH:mm:ss"), message));
            addProblem("error", "发布", message);
        } else {
            m_lastBuildSaveSucceeded = true;
            appendOutput(QString("[%1] 已更新下载产物路径：%2")
                         .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                         .arg(cfg.downloadArtifact.filePath));
            publicationCommitted = true;
        }
    } else if (type == BuildType::Configuration) {
        publicationCommitted = false;
        m_sessionController->setPendingRunAfterCompile(false);
    }

    if (publicationCommitted && m_sessionController->hasPendingRunAfterCompile()
            && type == BuildType::Configuration) {
        const CompileResult compileResult = m_buildController
                ? m_buildController->lastCompileResult()
                : CompileResult();
        if (m_sessionController->onCompileSucceeded(compileResult)) {
            if (m_sessionController->state() == RuntimeSessionState::Running) {
                m_sessionController->startMonitoring();
            }
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
    updateStatusBar(QStringLiteral("编译失败"));
    if (m_globalStatusBar) {
        m_globalStatusBar->setBuildState(QStringLiteral("失败"));
    }
    addProblem("error", "构建", errorMessage.isEmpty() ? "编译失败" : errorMessage);

    // U04: 编译失败自动展开问题并定位首个错误；若用户主动关闭后不会被同一事件重复弹开
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

    m_lastBuildSaveSucceeded = m_projectController->saveProject();
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

// ================= SettingsController 信号处理槽函数 =================

void MainWindow::onFontSizeChanged(int pointSize)
{
    applyFontSize(pointSize);
}

// ================= 通用消息处理槽函数 =================

void MainWindow::onLogMessage(const QString& message)
{
    QString displayMessage = message;
    if (sender() == m_buildController && m_buildController) {
        const CompileResult result = m_buildController->lastCompileResult();
        if (result.success && result.metadata.value(QStringLiteral("requiresTargetValidation")).toBool()) {
            // Execution status remains in the compiler report and raw result.
            // Do not render this internal notice as a user-facing build warning.
            QStringList lines = message.split(QRegularExpression(QStringLiteral("[\r\n]+")));
            const QString notice = QStringLiteral("警告: 赋值写回尚未通过目标固件验证，当前产物不可下载或运行。");
            lines.erase(std::remove_if(lines.begin(), lines.end(), [&](const QString& line) {
                return line.trimmed() == notice;
            }), lines.end());
            displayMessage = lines.join(QLatin1Char('\n'));
        }
    }
    if (!displayMessage.trimmed().isEmpty()) appendOutput(displayMessage);
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

// ================= 诊断、命令面板与快速导航实现 =================

void MainWindow::onDocumentModified(const QString& filePath)
{
    const QString norm = normalizeDocumentIdentity(filePath);
    if (!norm.isEmpty()) {
        m_documentVersions[norm]++;
        if (m_problemsPanel) {
            m_problemsPanel->markDiagnosticsOutdatedForFile(norm);
        }
    }
}

void MainWindow::onDiagnosticsProduced(quint64 generation, const QString& projectSessionId, const QList<DiagnosticItem>& items)
{
    if (generation < m_latestDiagnosticGeneration) return;
    if (projectSessionId != m_projectSessionId) {
        return; // 项目已切换，丢弃旧批次
    }

    m_latestDiagnosticGeneration = generation;
    QList<DiagnosticItem> processedItems = items;
    for (auto& item : processedItems) {
        if (!item.compiledFilePath.isEmpty()
                && m_documentVersions.value(item.compiledFilePath, 0) != item.compiledDocVersion) {
            item.isOutdated = true;
        }
    }

    if (m_problemsPanel) {
        m_problemsPanel->replaceBuildDiagnostics(processedItems);
    }
}

void MainWindow::onDiagnosticActivated(const DiagnosticItem& item)
{
    if (item.filePath.isEmpty() || !QFile::exists(item.filePath)) {
        updateStatusBar(QStringLiteral("诊断详情: %1").arg(item.message));
        return;
    }

    QWidget* editor = openAndActivateFile(item.filePath);
    if (!editor) {
        return; // 用户取消或打开失败：绝对不执行任何行列跳转
    }

    QString activePath;
    if (auto* dsl = qobject_cast<DslScriptEditor*>(editor)) {
        activePath = dsl->currentFilePath();
    } else if (auto* sub = m_mdiArea ? m_mdiArea->activeSubWindow() : nullptr) {
        activePath = sub->property("filePath").toString();
    }

    if (!isSameDocument(activePath, item.filePath)) {
        return;
    }

    navigateEditorPosition(editor, item.line, item.column, item.hasExactColumn);
}

void MainWindow::openCommandPalette()
{
    CommandPaletteDialog dlg(this, CommandPaletteDialog::Mode::Command);
    dlg.exec();
}

void MainWindow::openQuickOpen()
{
    CommandPaletteDialog dlg(this, CommandPaletteDialog::Mode::QuickOpen);
    dlg.exec();
}

void MainWindow::openGotoLine()
{
    CommandPaletteDialog dlg(this, CommandPaletteDialog::Mode::GotoLine);
    dlg.exec();
}

void MainWindow::closeCurrentActiveTab()
{
    if (!m_mdiArea) {
        return;
    }
    QMdiSubWindow* sub = m_mdiArea->activeSubWindow();
    if (!sub) {
        return;
    }

    // 若为主 DSL 编辑器子窗口：执行“关闭仅隐藏”保护（对齐 FL15-01）
    if (sub == m_editorSubWindow) {
        if (m_actToggleDslEditor) {
            m_actToggleDslEditor->setChecked(false);
        } else {
            sub->hide();
        }
        return;
    }

    // 统一由子窗口 Close 事件过滤器执行保存/放弃/取消决策。
    sub->close();
}
