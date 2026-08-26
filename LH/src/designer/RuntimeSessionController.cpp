// File: src/designer/RuntimeSessionController.cpp

#include "RuntimeSessionController.h"
#include "RunController.h"
#include "ProjectController.h"
#include "BuildController.h"
#include "ParameterController.h"
#include "../communication/ControllerDeviceBackend.h"
#include "../communication/ControllerDebugProtocol.h"
#include "../communication/IDeviceBackend.h"
#include "../communication/IOpcServer.h"
#include "../communication/OpcServerFactory.h"
#include "../monitor/MonitorManager.h"
#include "../monitor/SampleDataProvider.h"
#include "../common/RuntimePointTypes.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <utility>

RuntimeSessionController::RuntimeSessionController(QObject* parent)
    : QObject(parent)
{
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
        Monitor::MonitorManager::instance().setDeviceBackend(backend);
        if (m_backend && m_backend->isOnline()
                && (m_state == RuntimeSessionState::Idle
                    || m_state == RuntimeSessionState::Compiled)) {
            setState(RuntimeSessionState::Connected);
        }
        return;
    }

    const QString message = QStringLiteral("设备后端已切换，OPC 写入已取消");
    cancelPendingOpcWrite(message);
    if (m_parameterController)
        m_parameterController->cancelPendingReadback(message);

    IDeviceBackend* oldBackend = m_backend.data();
    if (oldBackend)
        disconnect(oldBackend, nullptr, this, nullptr);
    if (m_ownedControllerBackend && backend != m_ownedControllerBackend) {
        m_ownedControllerBackend->disconnectBackend();
        m_ownedControllerBackend->deleteLater();
        m_ownedControllerBackend = nullptr;
    }
    const quint64 generation = ++m_backendGeneration;
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
                    const QString failure = QStringLiteral("设备后端已销毁，运行操作已取消");
                    cancelPendingOpcWrite(failure);
                    if (m_parameterController)
                        m_parameterController->cancelPendingReadback(failure);
                    stopOpcServer();
                    Monitor::MonitorManager::instance().setDeviceBackend(nullptr);
                    if (m_downloadState == DownloadState::Precheck
                            || m_downloadState == DownloadState::Downloading
                            || m_downloadState == DownloadState::Retrying
                            || m_downloadState == DownloadState::Verifying) {
                        setDownloadState(DownloadState::TransportFailed);
                    }
                    if (m_state != RuntimeSessionState::Idle
                            && m_state != RuntimeSessionState::Fault) {
                        setState(RuntimeSessionState::Fault);
                    }
                });
    }
    Monitor::MonitorManager::instance().setDeviceBackend(backend);
    if (m_backend && m_backend->isOnline()) {
        if (m_state == RuntimeSessionState::Idle
                || m_state == RuntimeSessionState::Compiled) {
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

    if (m_state == RuntimeSessionState::Running || m_state == RuntimeSessionState::Monitoring) {
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

    const bool ok = Monitor::MonitorManager::instance().applyConfiguration(cfg);
    if (!ok) {
        emit runtimeError(QStringLiteral("运行时配置应用到监控系统失败"));
        return false;
    }

    const QStringList providers = Monitor::MonitorManager::instance().providerIds();
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
    if (m_state == RuntimeSessionState::Running || m_state == RuntimeSessionState::Monitoring)
        return;

    if (shouldAutoDownload()) {
        setState(RuntimeSessionState::Connected);
        if (!requestDownload(m_artifactPath)) {
            return;
        }
    }

    setState(RuntimeSessionState::Running);

    emit logMessage(QStringLiteral("[%1] 项目已启动，下载产物：%2")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         QDir::toNativeSeparators(m_artifactPath)));

    startOpcServerIfEnabled();
}

void RuntimeSessionController::requestStop()
{
    if (m_state == RuntimeSessionState::Idle && m_downloadState == DownloadState::Idle) {
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
    const bool wasMonitoring = m_state == RuntimeSessionState::Monitoring;
    const bool wasDownloading = m_state == RuntimeSessionState::Downloading
            || m_downloadState == DownloadState::Precheck
            || m_downloadState == DownloadState::Downloading
            || m_downloadState == DownloadState::Retrying
            || m_downloadState == DownloadState::Verifying;
    setDownloadState(DownloadState::Idle);
    stopOpcServer();
    Monitor::MonitorManager::instance().stopMonitoring();

    // 先发布 Idle，再断开后端，避免断开信号把停止过程短暂推入 Fault。
    setState(RuntimeSessionState::Idle);
    if (m_backend && m_backend == m_ownedControllerBackend) {
        m_backend->disconnectBackend();
    }
    if (wasDownloading) {
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
    auto& manager = Monitor::MonitorManager::instance();
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

    auto& manager = Monitor::MonitorManager::instance();
    const QStringList channels = manager.channelNames();
    for (const QString& ch : channels) {
        const auto cfg = manager.channelConfig(ch);
        if (cfg.metadata.value(QStringLiteral("__demoMode")).toBool())
            manager.removeChannel(ch);
    }

    m_demoModeActive = false;
    emit demoModeChanged(false);
    emit logMessage(QStringLiteral("[%1] 演示模式已停止（%2）")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         reason));
}

void RuntimeSessionController::startMonitoring()
{
    if (m_state == RuntimeSessionState::Monitoring)
        return;

    if (m_state != RuntimeSessionState::Running) {
        emit runtimeError(QStringLiteral("当前状态不允许启动监控。"));
        return;
    }

    Monitor::MonitorManager::instance().startMonitoring();
    setState(RuntimeSessionState::Monitoring);
}

void RuntimeSessionController::stopMonitoring()
{
    if (m_state != RuntimeSessionState::Monitoring)
        return;

    Monitor::MonitorManager::instance().stopMonitoring();
    setState(RuntimeSessionState::Running);
}

void RuntimeSessionController::setState(RuntimeSessionState newState)
{
    if (m_state == newState)
        return;

    const RuntimeSessionState oldState = m_state;
    m_state = newState;
    emit stateChanged(oldState, newState);

    const bool wasMonitoring = oldState == RuntimeSessionState::Monitoring;
    const bool isMonitoringNow = newState == RuntimeSessionState::Monitoring;
    if (wasMonitoring != isMonitoringNow) {
        emit monitoringChanged(isMonitoringNow);
    }
}

void RuntimeSessionController::setDownloadState(DownloadState newState)
{
    if (m_downloadState == newState)
        return;

    const DownloadState oldState = m_downloadState;
    m_downloadState = newState;
    emit downloadStateChanged(oldState, newState);
}

void RuntimeSessionController::handleBackendConnectionStateChanged(bool connected)
{
    if (connected) {
        if (m_state == RuntimeSessionState::Idle
                || m_state == RuntimeSessionState::Compiled
                || m_state == RuntimeSessionState::Fault) {
            setState(RuntimeSessionState::Connected);
        }
        return;
    }

    if (m_state == RuntimeSessionState::Idle || m_state == RuntimeSessionState::Fault)
        return;

    if (m_internalReconnect) {
        return;
    }

    const bool wasMonitoring = m_state == RuntimeSessionState::Monitoring;
    Monitor::MonitorManager::instance().stopMonitoring();
    stopOpcServer();
    if (m_downloadState == DownloadState::Precheck
            || m_downloadState == DownloadState::Downloading
            || m_downloadState == DownloadState::Retrying
            || m_downloadState == DownloadState::Verifying) {
        setDownloadState(DownloadState::TransportFailed);
    }
    setState(RuntimeSessionState::Fault);
    if (wasMonitoring) {
        emit logMessage(QStringLiteral("[%1] 后端断开，监控已停止")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
    }
    emit runtimeError(QStringLiteral("设备后端已断开，运行会话进入故障状态。"));
}
