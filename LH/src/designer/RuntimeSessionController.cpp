// File: src/designer/RuntimeSessionController.cpp

#include "RuntimeSessionController.h"
#include "RunController.h"
#include "ProjectController.h"
#include "BuildController.h"
#include "../communication/IDeviceBackend.h"
#include "../monitor/MonitorManager.h"
#include "../monitor/SampleDataProvider.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

RuntimeSessionController::RuntimeSessionController(QObject* parent)
    : QObject(parent)
{
}

void RuntimeSessionController::setDeviceBackend(IDeviceBackend* backend)
{
    m_backend = backend;
    Monitor::MonitorManager::instance().setDeviceBackend(backend);
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
        emit runtimeError(QStringLiteral("当前运行链路要求通过 Modbus RTU/TCP 连接控制器，请先配置 Modbus 传输。"));
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
    return true;
}

void RuntimeSessionController::executeRun()
{
    if (m_state == RuntimeSessionState::Running || m_state == RuntimeSessionState::Monitoring)
        return;

    setState(RuntimeSessionState::Running);

    emit logMessage(QStringLiteral("[%1] 项目已启动，下载产物：%2")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         QDir::toNativeSeparators(m_artifactPath)));

    if (shouldAutoDownload()) {
        requestDownload(m_artifactPath);
    }
}

void RuntimeSessionController::requestStop()
{
    if (m_state == RuntimeSessionState::Idle)
        return;

    Monitor::MonitorManager::instance().stopMonitoring();
    m_monitoringActive = false;
    emit monitoringChanged(false);

    stopDemoMode(QStringLiteral("项目已停止"));
    setState(RuntimeSessionState::Idle);

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
    if (m_monitoringActive)
        return;

    m_monitoringActive = true;
    if (m_state == RuntimeSessionState::Running)
        setState(RuntimeSessionState::Monitoring);
    emit monitoringChanged(true);
}

void RuntimeSessionController::stopMonitoring()
{
    if (!m_monitoringActive)
        return;

    Monitor::MonitorManager::instance().stopMonitoring();
    m_monitoringActive = false;
    if (m_state == RuntimeSessionState::Monitoring)
        setState(RuntimeSessionState::Running);
    emit monitoringChanged(false);
}

bool RuntimeSessionController::requestDownload(const QString& artifactPath, const QVariantMap& options)
{
    if (m_state != RuntimeSessionState::Running && m_state != RuntimeSessionState::Connected) {
        emit runtimeError(QStringLiteral("当前状态不允许下载。"));
        return false;
    }

    if (!m_backend || !m_backend->isOnline()) {
        emit runtimeError(QStringLiteral("设备后端未连接，无法下载。"));
        if (m_state == RuntimeSessionState::Running) {
            setState(RuntimeSessionState::Fault);
        }
        return false;
    }

    const RuntimeSessionState prevState = m_state;
    setState(RuntimeSessionState::Downloading);

    QString errorMsg;
    const bool ok = m_backend->downloadArtifact(artifactPath, options, &errorMsg);

    if (ok) {
        emit downloadFinished(true, QStringLiteral("下载成功：%1").arg(artifactPath));
        emit logMessage(QStringLiteral("[%1] 编译产物下载成功")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
        setState(prevState);
        return true;
    }

    emit downloadFinished(false, QStringLiteral("下载失败：%1").arg(errorMsg));
    emit runtimeError(QStringLiteral("下载失败：%1").arg(errorMsg));
    setState(RuntimeSessionState::Fault);
    return false;
}

void RuntimeSessionController::setState(RuntimeSessionState newState)
{
    if (m_state == newState)
        return;

    const RuntimeSessionState oldState = m_state;
    m_state = newState;
    emit stateChanged(oldState, newState);
}

bool RuntimeSessionController::shouldAutoDownload() const
{
    return !m_artifactPath.isEmpty() && m_backend && m_backend->isOnline();
}
