// File: src/designer/RuntimeSessionController.cpp

#include "RuntimeSessionController.h"
#include "RunController.h"
#include "ProjectController.h"
#include "BuildController.h"
#include "ParameterController.h"
#include "RuntimeMonitorAdapter.h"
#include "../communication/ControllerDeviceBackend.h"
#include "../communication/ControllerDebugProtocol.h"
#include "../communication/IDeviceBackend.h"
#include "../communication/IOpcServer.h"
#include "../communication/OpcServerFactory.h"
#include "../monitor/MonitorManager.h"
#include "../monitor/SampleDataProvider.h"
#include "../common/RuntimePointTypes.h"
#include "../core/AppLogging.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <utility>

RuntimeSessionController::RuntimeSessionController(QObject* parent, RuntimeSessionService* service)
    : QObject(parent)
{
    // Compatibility facade for existing callers; the application composition root
    // supplies an explicit service. The QtCore service performs no global lookup.
    m_runtimeService = service ? service : new RuntimeSessionService(
        makeRuntimeMonitorPort(Monitor::MonitorManager::instance()),
        Monitor::MonitorManager::instance().historyStore(), this);
    connect(m_runtimeService, &RuntimeSessionService::stateChanged,
            this, &RuntimeSessionController::handleRuntimeStateChanged);
    connect(m_runtimeService, &RuntimeSessionService::downloadStateChanged,
            this, &RuntimeSessionController::downloadStateChanged);
    m_opcServer = OpcServerFactory::createDefault(this);
    connectOpcServerSignals();
}

IDeviceBackend* RuntimeSessionController::deviceBackend() const
{
    return m_backend.data();
}

void RuntimeSessionController::setDeviceBackend(IDeviceBackend* backend)
{
    if (m_backend == backend) {
        m_runtimeService->setBackend(backend);
        if (m_backend && m_backend->isOnline()
                && (state() == RuntimeSessionState::Idle
                    || state() == RuntimeSessionState::Compiled)) {
            setState(RuntimeSessionState::Connected);
        }
        return;
    }

    const QString message = QStringLiteral("设备后端已切换，OPC 写入已取消");
    cancelPendingOpcWrite(message);
    if (m_parameterController)
        m_parameterController->cancelPendingReadback(message);

    if (m_backendDownloadInProgress) requestStop();

    IDeviceBackend* oldBackend = m_backend.data();
    if (oldBackend)
        disconnect(oldBackend, nullptr, this, nullptr);
    if (m_ownedControllerBackend && backend != m_ownedControllerBackend) {
        m_ownedControllerBackend->disconnectBackend();
        m_ownedControllerBackend->deleteLater();
        m_ownedControllerBackend = nullptr;
    }
    const quint64 generation = ++m_backendGeneration;
    ++m_debugCommandGeneration;
    setDebugCommandPending(false);
    m_backend = backend;
    if (m_backend) {
        auto* currentBackend = m_backend.data();
        connect(currentBackend, &IDeviceBackend::connectionStateChanged,
                this, &RuntimeSessionController::handleBackendConnectionStateChanged,
                Qt::UniqueConnection);
        connect(currentBackend, &QObject::destroyed, this,
                [this, generation, currentBackend]() {
                    if (m_backendGeneration != generation)
                        return;

                    if (m_ownedControllerBackend == currentBackend)
                        m_ownedControllerBackend = nullptr;
                    m_backend = nullptr;
                    ++m_debugCommandGeneration;
                    setDebugCommandPending(false);
                    const QString failure = QStringLiteral("设备后端已销毁，运行操作已取消");
                    cancelPendingOpcWrite(failure);
                    if (m_parameterController)
                        m_parameterController->cancelPendingReadback(failure);
                    stopOpcServer();
                    m_runtimeService->setBackend(nullptr);
                    if (downloadState() == DownloadState::Precheck
                            || downloadState() == DownloadState::Downloading
                            || downloadState() == DownloadState::Retrying
                            || downloadState() == DownloadState::Verifying) {
                        setDownloadState(DownloadState::TransportFailed);
                    }
                    if (state() != RuntimeSessionState::Idle
                            && state() != RuntimeSessionState::Fault) {
                        setState(RuntimeSessionState::Fault);
                    }
                });
    }
    m_runtimeService->setBackend(backend);
    if (m_backend && m_backend->isOnline()) {
        if (state() == RuntimeSessionState::Idle
                || state() == RuntimeSessionState::Compiled) {
            setState(RuntimeSessionState::Connected);
        }
    }
}

void RuntimeSessionController::setOpcServer(IOpcServer* opcServer)
{
    if (m_opcServer == opcServer) {
        return;
    }

    cancelPendingOpcWrite(QStringLiteral("OPC 服务已切换，写入已取消"));

    if (m_opcServer) {
        disconnect(m_opcServer, nullptr, this, nullptr);
        if (m_opcServer->parent() == this) {
            m_opcServer->deleteLater();
        }
    }

    m_opcServer = opcServer;
    connectOpcServerSignals();
}

void RuntimeSessionController::setSampleDataProvider(SampleDataProvider* provider)
{
    m_sampleDataProvider = provider;
}

void RuntimeSessionController::setProjectController(ProjectController* controller)
{
    m_projectController = controller;
}

void RuntimeSessionController::setBuildController(BuildController* controller)
{
    m_buildController = controller;
}

void RuntimeSessionController::setParameterController(ParameterController* controller)
{
    if (m_parameterController == controller) {
        return;
    }

    const QString message = QStringLiteral("参数控制器已切换，回读已取消");
    cancelPendingOpcWrite(message);
    if (m_parameterController)
        m_parameterController->cancelPendingReadback(message);
    if (m_parameterController) {
        disconnect(m_parameterController,
                   &ParameterController::readbackFinished,
                   this,
                   &RuntimeSessionController::handleParameterReadbackFinished);
    }
    m_parameterController = controller;
    m_runtimeService->setParameters(controller);
    if (m_parameterController) {
        connect(m_parameterController,
                &ParameterController::readbackFinished,
                this,
                &RuntimeSessionController::handleParameterReadbackFinished,
                Qt::UniqueConnection);
    }
}

bool RuntimeSessionController::prepareRun()
{
    if (!m_projectController || !m_projectController->hasOpenProject()) {
        emit runtimeError(QStringLiteral("请先打开或创建项目。"));
        return false;
    }

    if (state() == RuntimeSessionState::Running || state() == RuntimeSessionState::Monitoring) {
        emit runtimeError(QStringLiteral("项目已在运行中。"));
        return false;
    }

    const auto& cfg = m_projectController->runtimeConfig();
    if (!RunController::usesModbusTransport(cfg)) {
        emit runtimeError(QStringLiteral("当前运行链路要求通过 Modbus RTU 连接控制器，请先配置 Modbus 传输。"));
        return false;
    }
    const QString transportMode = cfg.transport.mode.trimmed();
    if (!transportMode.isEmpty()
            && transportMode.compare(QStringLiteral("rtu"), Qt::CaseInsensitive) != 0) {
        emit runtimeError(QStringLiteral("当前控制器运行链仅支持 Modbus RTU，尚未启用 Modbus TCP。"));
        return false;
    }

    const CompileResult compileResult = m_buildController
            ? m_buildController->lastCompileResult()
            : CompileResult();
    m_artifactPath = RunController::findDownloadArtifactPath(
            cfg,
            m_projectController->currentProjectPath(),
            compileResult);

    if (m_artifactPath.isEmpty() || !QFileInfo::exists(m_artifactPath)) {
        emit runtimeError(QStringLiteral("NO_ARTIFACT"));
        return false;
    }

    return true;
}

bool RuntimeSessionController::applyRuntimeConfig()
{
    if (!m_projectController)
        return false;

    QStringList errors;
    if (!m_projectController->validateConfiguration(errors)) {
        emit runtimeError(QStringLiteral("配置校验失败：%1").arg(errors.join(QStringLiteral("; "))));
        return false;
    }

    const auto& cfg = m_projectController->runtimeConfig();
    if (RunController::usesModbusTransport(cfg) && !ensureControllerBackend()) {
        return false;
    }

    const bool ok = m_runtimeService->monitor().applyConfiguration(cfg);
    if (!ok) {
        emit runtimeError(QStringLiteral("运行时配置应用到监控系统失败"));
        return false;
    }

    const QStringList providers = m_runtimeService->monitor().providerIds();
    if (!providers.isEmpty()) {
        stopDemoMode(QStringLiteral("已应用运行时配置，并检测到 %1 个 provider").arg(providers.size()));
    }

    emit logMessage(QStringLiteral("[%1] 运行时配置已应用到监控系统")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));

    syncOpcRuntimePoints();
    return true;
}

void RuntimeSessionController::executeRun()
{
    if (state() == RuntimeSessionState::Running || state() == RuntimeSessionState::Monitoring
            || m_backendDownloadInProgress || state() == RuntimeSessionState::Downloading)
        return;

    if (shouldAutoDownload()) {
        setState(RuntimeSessionState::Connected);
        m_pendingRunAfterDownload = true;
        if (!requestDownload(m_artifactPath)) {
            m_pendingRunAfterDownload = false;
            return;
        }
        if (m_backendDownloadInProgress) return;
        m_pendingRunAfterDownload = false;
    }
    finishRunStart();
}

void RuntimeSessionController::finishRunStart()
{
    setPaused(false);
    setState(RuntimeSessionState::Running);

    emit logMessage(QStringLiteral("[%1] 项目已启动，下载产物：%2")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         QDir::toNativeSeparators(m_artifactPath)));

    startOpcServerIfEnabled();
}

void RuntimeSessionController::setPaused(bool paused)
{
    if (m_isPaused == paused) {
        return;
    }
    m_isPaused = paused;
    emit pausedChanged(m_isPaused);
}

void RuntimeSessionController::requestStop()
{
    m_runtimeService->cancelOperations();
    setPaused(false);
    m_pendingRunAfterDownload = false;
    if (state() == RuntimeSessionState::Idle && downloadState() == DownloadState::Idle) {
        const QString message = QStringLiteral("运行会话已停止，OPC 写入已取消");
        cancelPendingOpcWrite(message);
        if (m_parameterController)
            m_parameterController->cancelPendingReadback(message);
        return;
    }

    const QString message = QStringLiteral("运行会话已停止，OPC 写入已取消");
    cancelPendingOpcWrite(message);
    if (m_parameterController)
        m_parameterController->cancelPendingReadback(message);
    m_downloadCancelled = true;
    if (m_backend) {
        m_backend->cancelDownload();
    }
    const bool wasMonitoring = state() == RuntimeSessionState::Monitoring;
    const bool wasDownloading = state() == RuntimeSessionState::Downloading
            || downloadState() == DownloadState::Precheck
            || downloadState() == DownloadState::Downloading
            || downloadState() == DownloadState::Retrying
            || downloadState() == DownloadState::Verifying;
    setDownloadState(DownloadState::Idle);
    stopOpcServer();
    m_runtimeService->monitor().stopMonitoring();

    // 先发布 Idle，再断开后端，避免断开信号把停止过程短暂推入 Fault。
    setState(RuntimeSessionState::Idle);
    if (m_backend && m_backend == m_ownedControllerBackend && !m_backendDownloadInProgress) {
        m_backend->disconnectBackend();
    }
    if (wasDownloading) {
        AppLogging::writeBusinessEvent(QStringLiteral("download_canceled"), QtWarningMsg, m_currentDownloadOperationId,
            QStringLiteral("transfer"), QStringLiteral("canceled"), static_cast<int>(CommErrorCode::OperationCancelled), m_artifactPath);
        emit downloadFinished(false, QStringLiteral("下载已停止。"));
    }
    if (wasMonitoring) {
        emit logMessage(QStringLiteral("[%1] 监控已停止")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
    }

    stopDemoMode(QStringLiteral("项目已停止"));

    emit logMessage(QStringLiteral("[%1] 项目已停止")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
}

void RuntimeSessionController::invalidateCompiledArtifact()
{
    m_artifactPath.clear();
    m_pendingRunAfterCompile = false;
    if (state() == RuntimeSessionState::Compiled) setState(RuntimeSessionState::Idle);
}

bool RuntimeSessionController::onCompileSucceeded(const CompileResult& result)
{
    if (!m_pendingRunAfterCompile)
        return false;

    m_pendingRunAfterCompile = false;

    const auto& cfg = m_projectController->runtimeConfig();
    m_artifactPath = RunController::findDownloadArtifactPath(
            cfg,
            m_projectController->currentProjectPath(),
            result);

    if (m_artifactPath.isEmpty() || !QFileInfo::exists(m_artifactPath)) {
        emit runtimeError(QStringLiteral("编译成功但未找到产物，无法自动运行。"));
        return false;
    }

    if (!applyRuntimeConfig())
        return false;

    executeRun();
    return true;
}

void RuntimeSessionController::startDemoMode(const QString& reason)
{
    auto& manager = m_runtimeService->monitor();
    if (!manager.providerIds().isEmpty()) {
        if (m_demoModeActive) {
            stopDemoMode(QStringLiteral("检测到真实采集器，关闭演示模式：%1").arg(reason));
        }
        return;
    }

    if (!m_sampleDataProvider)
        return;

    if (!m_demoModeActive) {
        if (!m_sampleDataProvider->isRunning())
            m_sampleDataProvider->start();

        m_demoModeActive = true;
        emit demoModeChanged(true);
        emit logMessage(QStringLiteral("[%1] 演示模式已启动（原因：%2）")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                             reason));
    }
}

void RuntimeSessionController::stopDemoMode(const QString& reason)
{
    if (!m_demoModeActive && !(m_sampleDataProvider && m_sampleDataProvider->isRunning()))
        return;

    if (m_sampleDataProvider && m_sampleDataProvider->isRunning())
        m_sampleDataProvider->stop();

    auto& manager = m_runtimeService->monitor();
    manager.removeDemoChannels();

    m_demoModeActive = false;
    emit demoModeChanged(false);
    emit logMessage(QStringLiteral("[%1] 演示模式已停止（%2）")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         reason));
}

void RuntimeSessionController::startMonitoring()
{
    if (state() == RuntimeSessionState::Monitoring)
        return;

    if (state() != RuntimeSessionState::Running) {
        emit runtimeError(QStringLiteral("当前状态不允许启动监控。"));
        return;
    }

    auto& manager = m_runtimeService->monitor();
    if (m_projectController || m_sampleDataProvider) {
        if (manager.providerIds().isEmpty()
                && !m_demoModeActive
                && (!m_projectController || !m_projectController->hasOpenProject())) {
            startDemoMode(QStringLiteral("开始监控"));
        }

        const bool hasDemoSource = (!m_projectController || !m_projectController->hasOpenProject())
                && m_demoModeActive
                && m_sampleDataProvider
                && m_sampleDataProvider->isRunning();
        if (manager.providerIds().isEmpty() && !hasDemoSource) {
            emit runtimeError(QStringLiteral(
                "当前没有可用监控数据源，请先应用运行时配置或启用演示数据模式。"));
            return;
        }
    }

    manager.startMonitoring();
    if (!manager.isMonitoring()) {
        emit runtimeError(QStringLiteral("监控启动失败，数据采集器未进入运行状态。"));
        return;
    }
    setState(RuntimeSessionState::Monitoring);
}

void RuntimeSessionController::stopMonitoring()
{
    if (state() != RuntimeSessionState::Monitoring)
        return;

    m_runtimeService->monitor().stopMonitoring();
    setState(RuntimeSessionState::Running);
}

void RuntimeSessionController::setState(RuntimeSessionState newState)
{
    m_runtimeService->setState(newState);
}

void RuntimeSessionController::handleRuntimeStateChanged(RuntimeSessionState oldState, RuntimeSessionState newState)
{
    AppLogging::writeBusinessEvent(
        QStringLiteral("session_state_changed"),
        (newState == RuntimeSessionState::Fault ? QtCriticalMsg : QtInfoMsg),
        QString(),
        QStringLiteral("session"),
        QStringLiteral("state_change"),
        (newState == RuntimeSessionState::Fault ? -1 : 0),
        QStringLiteral("RuntimeSessionController"),
        {{QStringLiteral("from"), static_cast<int>(oldState)},
         {QStringLiteral("to"), static_cast<int>(newState)}});

    emit stateChanged(oldState, newState);
    if (newState != RuntimeSessionState::Running && newState != RuntimeSessionState::Monitoring) {
        setPaused(false);
    }

    const bool wasMonitoring = oldState == RuntimeSessionState::Monitoring;
    const bool isMonitoringNow = newState == RuntimeSessionState::Monitoring;
    if (wasMonitoring != isMonitoringNow) {
        emit monitoringChanged(isMonitoringNow);
    }
}

void RuntimeSessionController::setDownloadState(DownloadState newState)
{
    m_runtimeService->setDownloadState(newState);
}

void RuntimeSessionController::handleBackendConnectionStateChanged(bool connected)
{
    if (!connected) {
        ++m_debugCommandGeneration;
        setDebugCommandPending(false);
    }
    AppLogging::writeBusinessEvent(
        QStringLiteral("backend_connection_changed"),
        (connected ? QtInfoMsg : QtWarningMsg),
        QString(),
        QStringLiteral("connection"),
        (connected ? QStringLiteral("connected") : QStringLiteral("disconnected")),
        0,
        QStringLiteral("DeviceBackend"));

    if (connected) {
        if (state() == RuntimeSessionState::Idle
                || state() == RuntimeSessionState::Compiled
                || state() == RuntimeSessionState::Fault) {
            setState(RuntimeSessionState::Connected);
        }
        return;
    }

    if (state() == RuntimeSessionState::Idle || state() == RuntimeSessionState::Fault)
        return;

    if (m_internalReconnect) {
        return;
    }

    const bool wasMonitoring = state() == RuntimeSessionState::Monitoring;
    m_runtimeService->monitor().stopMonitoring();
    stopOpcServer();
    if (downloadState() == DownloadState::Precheck
            || downloadState() == DownloadState::Downloading
            || downloadState() == DownloadState::Retrying
            || downloadState() == DownloadState::Verifying) {
        setDownloadState(DownloadState::TransportFailed);
    }
    setState(RuntimeSessionState::Fault);
    if (wasMonitoring) {
        emit logMessage(QStringLiteral("[%1] 后端断开，监控已停止")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
    }
    emit runtimeError(QStringLiteral("设备后端已断开，运行会话进入故障状态。"));
}
