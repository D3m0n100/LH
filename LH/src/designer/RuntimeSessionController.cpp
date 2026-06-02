// File: src/designer/RuntimeSessionController.cpp

#include "RuntimeSessionController.h"
#include "RunController.h"
#include "ProjectController.h"
#include "BuildController.h"
#include "ParameterController.h"
#include "../communication/IDeviceBackend.h"
#include "../communication/IOpcServer.h"
#include "../communication/OpcServerFactory.h"
#include "../monitor/MonitorManager.h"
#include "../monitor/SampleDataProvider.h"
#include "../common/RuntimePointTypes.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

namespace {
bool isDownloadTransportFailure(CommErrorCode code)
{
    return code == CommErrorCode::ConnectionLost
            || code == CommErrorCode::ConnectionFailed
            || code == CommErrorCode::ConnectionTimeout
            || code == CommErrorCode::ReceiveTimeout
            || code == CommErrorCode::SendFailed
            || code == CommErrorCode::ReceiveFailed;
}

bool isDownloadVerifyFailure(CommErrorCode code)
{
    return code == CommErrorCode::InvalidResponse
            || code == CommErrorCode::ProtocolError
            || code == CommErrorCode::CrcError
            || code == CommErrorCode::FrameError
            || code == CommErrorCode::AddressMismatch;
}

bool isDownloadRetryable(DownloadState state)
{
    return state == DownloadState::TransportFailed
            || state == DownloadState::VerifyFailed;
}

QString downloadStateLabel(DownloadState state)
{
    switch (state) {
    case DownloadState::Idle:
        return QStringLiteral("Idle");
    case DownloadState::Precheck:
        return QStringLiteral("Precheck");
    case DownloadState::PrecheckFailed:
        return QStringLiteral("PrecheckFailed");
    case DownloadState::Downloading:
        return QStringLiteral("Downloading");
    case DownloadState::Retrying:
        return QStringLiteral("Retrying");
    case DownloadState::Verifying:
        return QStringLiteral("Verifying");
    case DownloadState::Succeeded:
        return QStringLiteral("Succeeded");
    case DownloadState::TransportFailed:
        return QStringLiteral("TransportFailed");
    case DownloadState::DeviceRejected:
        return QStringLiteral("DeviceRejected");
    case DownloadState::VerifyFailed:
        return QStringLiteral("VerifyFailed");
    case DownloadState::Failed:
    default:
        return QStringLiteral("Failed");
    }
}

bool isDownloadVerificationPassed(const BackendStatusSnapshot& snapshot)
{
    return snapshot.online
            && !snapshot.downloading
            && snapshot.lastErrorCode == CommErrorCode::NoError;
}
}

RuntimeSessionController::RuntimeSessionController(QObject* parent)
    : QObject(parent)
{
    m_opcServer = OpcServerFactory::createDefault(this);
    connectOpcServerSignals();
}

void RuntimeSessionController::setDeviceBackend(IDeviceBackend* backend)
{
    m_backend = backend;
    Monitor::MonitorManager::instance().setDeviceBackend(backend);
}

void RuntimeSessionController::setOpcServer(IOpcServer* opcServer)
{
    if (m_opcServer == opcServer) {
        return;
    }

    if (m_opcServer && m_opcServer->parent() == this) {
        m_opcServer->deleteLater();
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
    m_parameterController = controller;
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

    syncOpcRuntimePoints();
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

    startOpcServerIfEnabled();

    if (shouldAutoDownload()) {
        requestDownload(m_artifactPath);
    }
}

void RuntimeSessionController::requestStop()
{
    if (m_state == RuntimeSessionState::Idle)
        return;

    setDownloadState(DownloadState::Idle);
    stopOpcServer();
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
    setDownloadState(DownloadState::Precheck);
    emit downloadProgressChanged(0);

    const QString trimmedArtifactPath = artifactPath.trimmed();
    const RuntimeSessionState prevState = m_state;
    if (m_state != RuntimeSessionState::Running && m_state != RuntimeSessionState::Connected) {
        setDownloadState(DownloadState::PrecheckFailed);
        const QString message = QStringLiteral("当前状态不允许下载。");
        emit downloadFinished(false, message);
        emit runtimeError(message);
        setState(prevState);
        return false;
    }

    if (trimmedArtifactPath.isEmpty() || !QFileInfo::exists(trimmedArtifactPath)) {
        setDownloadState(DownloadState::PrecheckFailed);
        const QString message = trimmedArtifactPath.isEmpty()
                ? QStringLiteral("下载前置校验失败：产物路径为空。")
                : QStringLiteral("下载前置校验失败：未找到产物文件。");
        emit downloadFinished(false, message);
        emit runtimeError(message);
        setState(prevState);
        return false;
    }

    if (!m_backend || !m_backend->isOnline()) {
        setDownloadState(DownloadState::PrecheckFailed);
        const QString message = QStringLiteral("设备后端未连接，无法下载。");
        emit downloadFinished(false, message);
        emit runtimeError(message);
        setState(prevState);
        return false;
    }

    const int maxAttempts = qMax(1, options.value(QStringLiteral("retryCount"), 1).toInt());

    QString lastErrorMsg;
    DownloadState lastFailureState = DownloadState::Failed;
    int attemptsUsed = 0;

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        attemptsUsed = attempt;
        if (attempt > 1) {
            setDownloadState(DownloadState::Retrying);
            emit logMessage(QStringLiteral("[%1] 下载重试 %2/%3")
                            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                 QString::number(attempt),
                                 QString::number(maxAttempts)));
        }

        setState(RuntimeSessionState::Downloading);
        setDownloadState(DownloadState::Downloading);
        emit downloadProgressChanged(25);

        QString errorMsg;
        CommError operationError;
        const bool ok = m_backend->downloadArtifact(trimmedArtifactPath, options, &errorMsg, &operationError);

        if (ok) {
            setDownloadState(DownloadState::Verifying);
            emit downloadProgressChanged(75);

            const BackendStatusSnapshot snapshot = m_backend->statusSnapshot();
            if (!isDownloadVerificationPassed(snapshot)) {
                lastFailureState = DownloadState::VerifyFailed;
                lastErrorMsg = QStringLiteral("下载后校验失败：后端状态未达成成功条件。");
                if (attempt < maxAttempts) {
                    setDownloadState(DownloadState::Retrying);
                    continue;
                }

                setDownloadState(lastFailureState);
                lastErrorMsg = QStringLiteral("下载失败[%1/%2,%3]：%4")
                        .arg(QString::number(attemptsUsed),
                             QString::number(maxAttempts),
                             downloadStateLabel(lastFailureState),
                             lastErrorMsg);
                emit downloadFinished(false, lastErrorMsg);
                emit runtimeError(lastErrorMsg);
                emit logMessage(QStringLiteral("[%1] 下载失败并回退到原状态：%2")
                                .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                     lastErrorMsg));
                setState(prevState);
                return false;
            }

            emit downloadProgressChanged(100);
            setDownloadState(DownloadState::Succeeded);
            emit downloadFinished(true, QStringLiteral("下载成功：%1").arg(trimmedArtifactPath));
            emit logMessage(QStringLiteral("[%1] 编译产物下载成功")
                            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
            setState(prevState);
            return true;
        }

        lastFailureState = classifyDownloadFailure(&operationError, errorMsg);
        lastErrorMsg = QStringLiteral("下载失败：%1").arg(errorMsg.isEmpty()
                                                                 ? QStringLiteral("未知错误")
                                                                 : errorMsg);

        if (attempt < maxAttempts && isDownloadRetryable(lastFailureState)) {
            setDownloadState(DownloadState::Retrying);
            emit logMessage(QStringLiteral("[%1] 下载失败，准备重试：%2")
                            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                 lastErrorMsg));
            continue;
        }

        setDownloadState(lastFailureState);
        lastErrorMsg = QStringLiteral("下载失败[%1/%2,%3]：%4")
                .arg(QString::number(attemptsUsed),
                     QString::number(maxAttempts),
                     downloadStateLabel(lastFailureState),
                     lastErrorMsg);
        emit downloadFinished(false, lastErrorMsg);
        emit runtimeError(lastErrorMsg);
        emit logMessage(QStringLiteral("[%1] 下载失败并回退到原状态：%2")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                             lastErrorMsg));
        setState(prevState);
        return false;
    }

    setDownloadState(lastFailureState);
    lastErrorMsg = QStringLiteral("下载失败[%1/%2,%3]：%4")
            .arg(QString::number(attemptsUsed),
                 QString::number(maxAttempts),
                 downloadStateLabel(lastFailureState),
                 lastErrorMsg);
    emit downloadFinished(false, lastErrorMsg);
    emit runtimeError(lastErrorMsg);
    emit logMessage(QStringLiteral("[%1] 下载失败并回退到原状态：%2")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         lastErrorMsg));
    setState(prevState);
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

void RuntimeSessionController::setDownloadState(DownloadState newState)
{
    if (m_downloadState == newState)
        return;

    const DownloadState oldState = m_downloadState;
    m_downloadState = newState;
    emit downloadStateChanged(oldState, newState);
}

bool RuntimeSessionController::shouldAutoDownload() const
{
    return !m_artifactPath.isEmpty() && m_backend && m_backend->isOnline();
}

DownloadState RuntimeSessionController::classifyDownloadFailure(const CommError* operationError,
                                                                const QString& errorMessage) const
{
    Q_UNUSED(errorMessage)

    if (!operationError || !operationError->isError()) {
        return DownloadState::Failed;
    }

    if (operationError->code == CommErrorCode::PermissionDenied) {
        return DownloadState::DeviceRejected;
    }

    if (isDownloadTransportFailure(operationError->code)) {
        return DownloadState::TransportFailed;
    }

    if (isDownloadVerifyFailure(operationError->code)) {
        return DownloadState::VerifyFailed;
    }

    return DownloadState::Failed;
}

void RuntimeSessionController::syncOpcRuntimePoints()
{
    if (!m_opcServer || !m_projectController) {
        return;
    }

    const auto& cfg = m_projectController->runtimeConfig();
    const QList<RuntimePointDefinition> points = RuntimePointConverter::fromProjectConfig(cfg);
    m_opcServer->setRuntimePoints(points);
    m_opcServer->setOpcTags(RuntimePointConverter::runtimePointsToOpcTags(points));
}

void RuntimeSessionController::startOpcServerIfEnabled()
{
    if (!m_opcServer || !m_projectController) {
        return;
    }

    const auto& opcConfig = m_projectController->runtimeConfig().opcServer;
    if (!opcConfig.enabled) {
        stopOpcServer();
        return;
    }

    QString errorMessage;
    if (!m_opcServer->applyConfig(opcConfig, &errorMessage)) {
        emit runtimeError(QStringLiteral("OPC 服务配置失败：%1").arg(errorMessage));
        return;
    }

    if (!m_opcServer->start(&errorMessage)) {
        emit runtimeError(QStringLiteral("OPC 服务启动失败：%1").arg(errorMessage));
        return;
    }

    emit logMessage(QStringLiteral("[%1] OPC 服务已启动：channel=%2 device=%3 mode=%4")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         opcConfig.channelName,
                         opcConfig.deviceName,
                         opcConfig.serialMode));
}

void RuntimeSessionController::stopOpcServer()
{
    if (!m_opcServer || !m_opcServer->isRunning()) {
        return;
    }

    m_opcServer->stop();
    emit logMessage(QStringLiteral("[%1] OPC 服务已停止")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
}

void RuntimeSessionController::connectOpcServerSignals()
{
    if (!m_opcServer) {
        return;
    }

    connect(m_opcServer, &IOpcServer::runningStateChanged,
            this, &RuntimeSessionController::opcRunningChanged,
            Qt::UniqueConnection);
    connect(m_opcServer, &IOpcServer::errorOccurred,
            this, &RuntimeSessionController::opcErrorOccurred,
            Qt::UniqueConnection);
    connect(m_opcServer, &IOpcServer::writeRequestReceived,
            this, &RuntimeSessionController::handleOpcWriteRequest,
            Qt::UniqueConnection);
}

void RuntimeSessionController::handleOpcWriteRequest(const QString& pointId, const QVariant& value)
{
    if (!m_opcServer) {
        return;
    }

    if (!m_parameterController) {
        const QString message = QStringLiteral("OPC 写入失败：参数控制器不可用");
        m_opcServer->recordWriteResult(pointId, false, message);
        emit runtimeError(message);
        return;
    }

    const ParameterStateInfo stateInfo = m_parameterController->parameterStateByPointId(pointId);
    if (stateInfo.name.isEmpty()) {
        const QString message = QStringLiteral("OPC 写入失败：未找到点位 %1").arg(pointId);
        m_opcServer->recordWriteResult(pointId, false, message);
        emit runtimeError(message);
        return;
    }

    if (!stateInfo.onlineEditable) {
        const QString message = QStringLiteral("OPC 写入失败：参数 %1 只读").arg(stateInfo.name);
        m_opcServer->recordWriteResult(pointId, false, message);
        emit runtimeError(message);
        return;
    }

    const QString valueText = value.toString();
    if (!m_parameterController->editParameterByPointId(pointId, valueText)) {
        const QString message = QStringLiteral("OPC 写入失败：参数 %1 编辑失败").arg(stateInfo.name);
        m_opcServer->recordWriteResult(pointId, false, message);
        emit runtimeError(message);
        return;
    }

    QString applyError;
    const bool ok = m_parameterController->applyModifiedParametersWithReadbackAsync(
            m_backend, 1, 0, &applyError);
    if (!ok) {
        const QString message = applyError.isEmpty()
                ? QStringLiteral("OPC 写入失败：参数 %1 下发失败").arg(stateInfo.name)
                : QStringLiteral("OPC 写入失败：%1").arg(applyError);
        m_opcServer->recordWriteResult(pointId, false, message);
        emit runtimeError(message);
        return;
    }

    const QString successMessage = QStringLiteral("OPC 写入成功：%1=%2")
            .arg(stateInfo.name, valueText);
    m_opcServer->recordWriteResult(pointId, true, successMessage);
    RuntimePointValue syncedValue;
    syncedValue.pointId = pointId;
    syncedValue.value = stateInfo.appliedValue.isEmpty() ? valueText : stateInfo.appliedValue;
    syncedValue.quality = RuntimePointQuality::Good;
    syncedValue.timestamp = QDateTime::currentDateTime();
    syncedValue.origin = QStringLiteral("opc-write");
    m_opcServer->updatePointValues({syncedValue});
    emit logMessage(QStringLiteral("[%1] %2")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         successMessage));
}
