#include <QPointer>
#include <QTimer>
// File: src/designer/RuntimeSessionDownload.cpp

#include "RuntimeSessionController.h"

#include "ProjectController.h"
#include "RunController.h"
#include "ProfileResolution.h"
#include "../communication/ControllerDeviceBackend.h"
#include "../communication/IDeviceBackend.h"
#include "../core/AppLogging.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>

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

QString currentTimeLabel()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
}

} // namespace

QString RuntimeSessionController::computeFileChecksum(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return QString();
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString RuntimeSessionController::currentEndpointDescription() const
{
    if (!m_projectController) {
        return QString();
    }
    const ProjectRuntimeConfig& cfg = m_projectController->runtimeConfig();
    const QVariantMap parameters = cfg.transport.parameters;
    QString port = parameters.value(QStringLiteral("portName")).toString().trimmed();
    if (port.isEmpty()) {
        port = parameters.value(QStringLiteral("port")).toString().trimmed();
    }
    QStringList parts;
    const QString protocol = cfg.transport.protocol.trimmed();
    const QString mode = cfg.transport.mode.trimmed();
    if (!protocol.isEmpty() || !mode.isEmpty()) {
        parts << QStringLiteral("%1/%2")
                 .arg(protocol.isEmpty() ? QStringLiteral("未知") : protocol,
                      mode.isEmpty() ? QStringLiteral("未知") : mode);
    }
    if (!port.isEmpty()) {
        parts << QStringLiteral("端口 %1").arg(port);
    }
    const QVariant baud = parameters.value(QStringLiteral("baudRate"));
    if (baud.isValid() && !baud.toString().trimmed().isEmpty()) {
        parts << QStringLiteral("%1 bps").arg(baud.toString().trimmed());
    }
    if (!cfg.controller.model.trimmed().isEmpty()) {
        parts << QStringLiteral("控制器 %1").arg(cfg.controller.model.trimmed());
    }
    parts << QStringLiteral("从站 %1").arg(cfg.controller.modbusSlaveId);
    return parts.join(QStringLiteral("，"));
}

DownloadConfirmationSnapshot RuntimeSessionController::buildDownloadSnapshot(
        const QString& artifactPath, const QVariantMap& options) const
{
    DownloadConfirmationSnapshot snapshot;
    snapshot.operationId = m_currentDownloadOperationId;
    snapshot.endpoint = currentEndpointDescription();

    ProjectRuntimeConfig cfg;
    if (m_projectController) {
        cfg = m_projectController->runtimeConfig();
        snapshot.projectName = cfg.projectName;
        snapshot.projectPath = m_projectController->currentProjectPath();
        snapshot.entryPath = cfg.mainScriptPath;
        snapshot.configurationFingerprint = QString::fromLatin1(
                    QCryptographicHash::hash(QJsonDocument(cfg.toJson()).toJson(QJsonDocument::Compact),
                                             QCryptographicHash::Sha256).toHex());
    }

    QStringList targetParts;
    if (!cfg.target.family.trimmed().isEmpty()) targetParts << cfg.target.family.trimmed();
    if (!cfg.target.model.trimmed().isEmpty()) targetParts << cfg.target.model.trimmed();
    snapshot.targetName = targetParts.isEmpty() ? QString() : targetParts.join(QStringLiteral(" / "));

    QStringList identityParts;
    if (!cfg.target.nodeId.trimmed().isEmpty()) {
        identityParts << QStringLiteral("节点 %1").arg(cfg.target.nodeId.trimmed());
    }
    if (!cfg.controller.model.trimmed().isEmpty()) {
        identityParts << QStringLiteral("控制器 %1").arg(cfg.controller.model.trimmed());
    }
    identityParts << QStringLiteral("从站 %1").arg(cfg.controller.modbusSlaveId);
    snapshot.targetIdentity = identityParts.join(QStringLiteral("，"));

    const QFileInfo artifactInfo(artifactPath);
    snapshot.artifactPath = artifactInfo.absoluteFilePath();
    snapshot.artifactBytes = artifactInfo.exists() ? artifactInfo.size() : -1;
    // The configuration checksum describes the published artifact, but the
    // file may have changed since publication. Bind approval to the bytes that
    // will actually be sent.
    snapshot.artifactChecksum = artifactInfo.exists()
            ? computeFileChecksum(snapshot.artifactPath) : QString();
    snapshot.backendGeneration = m_backendGeneration;
    QVariantMap boundOptions = options;
    boundOptions.remove(QStringLiteral("reconnectBeforeAttempt"));
    snapshot.optionsFingerprint = QString::fromLatin1(
                QCryptographicHash::hash(QJsonDocument::fromVariant(boundOptions)
                                         .toJson(QJsonDocument::Compact),
                                         QCryptographicHash::Sha256).toHex());
    snapshot.generationId = cfg.downloadArtifact.metadata
            .value(QStringLiteral("generationId")).toString().trimmed();
    snapshot.action = m_pendingRunAfterDownload
            ? QStringLiteral("下载后尝试运行（当前协议不支持启动命令，设备运行状态未知）")
                                                : QStringLiteral("仅下载");

    if (!m_lastDownloadRecord.valid) {
        snapshot.versionEvidence = QStringLiteral("未知（当前协议未提供设备程序版本查询）");
    } else if (m_lastDownloadRecord.targetName != snapshot.targetName
               || m_lastDownloadRecord.targetIdentity != snapshot.targetIdentity
               || m_lastDownloadRecord.endpoint != snapshot.endpoint
               || m_lastDownloadRecord.configurationFingerprint != snapshot.configurationFingerprint
               || m_lastDownloadRecord.backendGeneration != snapshot.backendGeneration) {
        snapshot.versionEvidence = QStringLiteral("未知（本会话下载记录属于其他目标、端点或配置）");
    } else {
        const QString time = m_lastDownloadRecord.completedAt
                .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (!snapshot.artifactChecksum.isEmpty()
                && m_lastDownloadRecord.checksum.compare(snapshot.artifactChecksum,
                                                         Qt::CaseInsensitive) == 0) {
            snapshot.versionEvidence = QStringLiteral(
                    "本会话最近成功下载记录（%1），产物摘要一致；非实时设备读回").arg(time);
        } else {
            snapshot.versionEvidence = QStringLiteral(
                    "本会话最近成功下载记录（%1），与本次产物无法确认一致；非实时设备读回").arg(time);
        }
    }
    return snapshot;
}

bool RuntimeSessionController::confirmExternalDownload(
        const DownloadConfirmationSnapshot& snapshot)
{
    if (!snapshot.isValid() || snapshot.artifactBytes < 0
            || snapshot.artifactChecksum.trimmed().isEmpty()) {
        return false;
    }
    if (!m_downloadConfirmationHook || m_downloadConfirmationPending) {
        return false;
    }
    m_downloadConfirmationPending = true;
    const bool approved = m_downloadConfirmationHook(snapshot);
    m_downloadConfirmationPending = false;
    return approved;
}

bool RuntimeSessionController::confirmDownloadSnapshot(
        const DownloadConfirmationSnapshot& snapshot, const QVariantMap& options)
{
    if (!snapshot.isValid() || snapshot.artifactBytes < 0
            || snapshot.artifactChecksum.trimmed().isEmpty()) {
        return false;
    }
    const QString fingerprint = snapshot.fingerprint();
    if (!m_approvedDownloadFingerprint.isEmpty()
            && m_approvedDownloadFingerprint == fingerprint
            && m_approvedDownloadOperationId == snapshot.operationId) {
        return true;
    }

    if (!m_downloadConfirmationHook) return false;

    m_downloadConfirmationPending = true;
    m_pendingDownloadFingerprint = fingerprint;
    const bool approved = m_downloadConfirmationHook(snapshot);
    m_downloadConfirmationPending = false;
    m_pendingDownloadFingerprint.clear();
    const QFileInfo artifactInfo(snapshot.artifactPath);
    const DownloadConfirmationSnapshot current = buildDownloadSnapshot(
                snapshot.artifactPath, options);
    if (!approved || m_backendGeneration != snapshot.backendGeneration
            || !artifactInfo.exists() || !artifactInfo.isFile()
            || current.fingerprint() != fingerprint) {
        m_approvedDownloadFingerprint.clear();
        m_approvedDownloadOperationId.clear();
        return false;
    }
    m_approvedDownloadFingerprint = fingerprint;
    m_approvedDownloadOperationId = snapshot.operationId;
    return true;
}

void RuntimeSessionController::recordSuccessfulDownload(
        const DownloadConfirmationSnapshot& snapshot)
{
    m_lastDownloadRecord.valid = true;
    m_lastDownloadRecord.operationId = snapshot.operationId;
    m_lastDownloadRecord.artifactPath = snapshot.artifactPath;
    m_lastDownloadRecord.checksum = snapshot.artifactChecksum;
    m_lastDownloadRecord.generationId = snapshot.generationId;
    m_lastDownloadRecord.targetName = snapshot.targetName;
    m_lastDownloadRecord.targetIdentity = snapshot.targetIdentity;
    m_lastDownloadRecord.endpoint = snapshot.endpoint;
    m_lastDownloadRecord.configurationFingerprint = snapshot.configurationFingerprint;
    m_lastDownloadRecord.backendGeneration = snapshot.backendGeneration;
    m_lastDownloadRecord.completedAt = QDateTime::currentDateTime();
    emit logMessage(QStringLiteral("[%1] 已记录本会话最近成功下载：%2")
                    .arg(m_lastDownloadRecord.completedAt.toString(QStringLiteral("HH:mm:ss")),
                         QDir::toNativeSeparators(snapshot.artifactPath)));
}

bool RuntimeSessionController::requestDownload(const QString& artifactPath, const QVariantMap& options)
{
    if (m_demoModeActive) {
        emit runtimeError(QStringLiteral("演示模式不能向设备下载程序。请先停止演示监控。"));
        return false;
    }
    if (m_backendDownloadInProgress || m_downloadConfirmationPending
            || state() == RuntimeSessionState::Downloading) return false;
    // Reject before allocating an operation or publishing a transient state.
    if (state() != RuntimeSessionState::Running && state() != RuntimeSessionState::Connected) return false;
    m_downloadCancelled = false;
    if (m_backend) {
        m_backend->resetDownloadCancellation();
    }
    m_internalReconnect = false;
    m_currentDownloadOperationId = options.value(QStringLiteral("operationId")).toString().trimmed();
    if (m_currentDownloadOperationId.isEmpty())
        m_currentDownloadOperationId = m_runFlowOperationIdContext;
    if (m_currentDownloadOperationId.isEmpty()) {
        m_currentDownloadOperationId = QStringLiteral("op-dl-%1-%2")
            .arg(QDateTime::currentMSecsSinceEpoch())
            .arg(reinterpret_cast<quintptr>(this));
    }
    setDownloadState(DownloadState::Precheck);
    emit downloadProgressChanged(0);

    const QString trimmedArtifactPath = artifactPath.trimmed();
    const RuntimeSessionState prevState = state();
    ProjectRuntimeConfig runtimeConfig;
    QString projectPath;
    if (m_projectController) {
        runtimeConfig = m_projectController->runtimeConfig();
        projectPath = m_projectController->currentProjectPath();
    }
    const QVariantMap effectiveOptions = resolveDownloadProfileOptions(options, runtimeConfig, projectPath);
    const int maxAttempts = qMax(1, effectiveOptions.value(QStringLiteral("retryCount"), 1).toInt());

    const auto restoreStateAfterDownload = [this, prevState]() {
        if (state() == RuntimeSessionState::Idle || state() == RuntimeSessionState::Fault)
            return;

        if (!m_backend || !m_backend->isOnline()) {
            setState(RuntimeSessionState::Fault);
            return;
        }

        setState(prevState);
    };

    AppLogging::writeBusinessEvent(QStringLiteral("download_started"),
                                   QtInfoMsg,
                                   m_currentDownloadOperationId,
                                   QStringLiteral("precheck"),
                                   QStringLiteral("started"),
                                   0,
                                   trimmedArtifactPath,
                                   {{QStringLiteral("retryCount"), maxAttempts}},
                                   1);

    emitDownloadDiagnostic(QStringLiteral("info"),
                           QStringLiteral("precheck"),
                           QStringLiteral("开始下载前置校验。"),
                           {{QStringLiteral("artifactPath"), trimmedArtifactPath},
                            {QStringLiteral("retryCount"), maxAttempts}});

    auto failPrecheck = [&](const QString& message, bool diagnosticAlreadyEmitted = false) {
        if (m_downloadCancelled || state() == RuntimeSessionState::Idle)
            return false;

        setDownloadState(DownloadState::PrecheckFailed);
        AppLogging::writeBusinessEvent(QStringLiteral("download_precheck_failed"),
                                       QtCriticalMsg,
                                       m_currentDownloadOperationId,
                                       QStringLiteral("precheck"),
                                       QStringLiteral("failed"),
                                       -1,
                                       trimmedArtifactPath,
                                       {{QStringLiteral("error"), message}},
                                       1);
        if (!diagnosticAlreadyEmitted) {
            emitDownloadDiagnostic(QStringLiteral("error"),
                                   QStringLiteral("precheck"),
                                   message,
                                   {{QStringLiteral("artifactPath"), trimmedArtifactPath}});
        }
        restoreStateAfterDownload();
        m_runFlowOperationIdContext.clear();
        emit downloadFinished(false, message);
        emit runtimeError(message);
        return false;
    };

    if (effectiveOptions.value(QStringLiteral("profileOverrideConflict")).toBool()) {
        return failPrecheck(effectiveOptions.value(QStringLiteral("profileOverrideError"))
                                    .toString().trimmed().isEmpty()
                                ? QStringLiteral("下载 Profile override 与已发布 generation 不一致。")
                                : effectiveOptions.value(QStringLiteral("profileOverrideError")).toString());
    }

    auto finishFailure = [&](DownloadState failureState,
                             const QString& rawMessage,
                             int attemptsUsed) {
        if (m_downloadCancelled || state() == RuntimeSessionState::Idle)
            return false;

        setDownloadState(failureState);
        m_lastDownloadRecord.valid = false;
        m_runFlowOperationIdContext.clear();
        m_activeDownloadSnapshot = DownloadConfirmationSnapshot();
        m_approvedDownloadFingerprint.clear();
        m_approvedDownloadOperationId.clear();
        const QString finalMessage = QStringLiteral("下载失败[%1/%2,%3]：%4。设备程序可能不完整，当前状态未知。")
                .arg(QString::number(attemptsUsed),
                     QString::number(maxAttempts),
                     downloadStateLabel(failureState),
                     rawMessage);
        AppLogging::writeBusinessEvent(QStringLiteral("download_failed"),
                                       QtCriticalMsg,
                                       m_currentDownloadOperationId,
                                       QStringLiteral("finish"),
                                       QStringLiteral("failed"),
                                       -1,
                                       trimmedArtifactPath,
                                       {{QStringLiteral("attemptsUsed"), attemptsUsed},
                                        {QStringLiteral("finalState"), downloadStateLabel(failureState)},
                                        {QStringLiteral("error"), rawMessage}},
                                       attemptsUsed);
        restoreStateAfterDownload();
        emit downloadFinished(false, finalMessage);
        emit runtimeError(finalMessage);
        emit logMessage(QStringLiteral("[%1] 下载失败并回退到原状态：%2")
                        .arg(currentTimeLabel(), finalMessage));
        emitDownloadDiagnostic(QStringLiteral("error"),
                               QStringLiteral("download"),
                               finalMessage,
                               {{QStringLiteral("artifactPath"), trimmedArtifactPath},
                                {QStringLiteral("attemptsUsed"), attemptsUsed},
                                {QStringLiteral("maxAttempts"), maxAttempts},
                                {QStringLiteral("state"), downloadStateLabel(failureState)}});
        return false;
    };

    if (state() != RuntimeSessionState::Running && state() != RuntimeSessionState::Connected) {
        return failPrecheck(QStringLiteral("当前状态不允许下载。"));
    }

    if (trimmedArtifactPath.isEmpty() || !QFileInfo::exists(trimmedArtifactPath)) {
        const QString message = trimmedArtifactPath.isEmpty()
                ? QStringLiteral("下载前置校验失败：产物路径为空。")
                : QStringLiteral("下载前置校验失败：未找到产物文件。");
        return failPrecheck(message);
    }

    if (!m_backend || !m_backend->isOnline()) {
        return failPrecheck(QStringLiteral("设备后端未连接，无法下载。"));
    }

    QString precheckError;
    if (!runDownloadPrecheck(trimmedArtifactPath, effectiveOptions, &precheckError)) {
        return failPrecheck(precheckError, true);
    }

    AppLogging::writeBusinessEvent(QStringLiteral("download_precheck_passed"),
                                   QtInfoMsg,
                                   m_currentDownloadOperationId,
                                   QStringLiteral("precheck"),
                                   QStringLiteral("passed"),
                                   0,
                                   trimmedArtifactPath,
                                   {{QStringLiteral("retryCount"), maxAttempts}},
                                   1);

    // The confirmation snapshot covers target, endpoint, project/entry, artifact
    // identity, version evidence and the action. It must be approved before the
    // first program write; connection/precheck has completed but nothing has been
    // written to the device yet.
    m_activeDownloadSnapshot = buildDownloadSnapshot(trimmedArtifactPath, effectiveOptions);
    if (!confirmDownloadSnapshot(m_activeDownloadSnapshot, effectiveOptions)) {
        m_pendingRunAfterDownload = false;
        m_runFlowOperationIdContext.clear();
        m_approvedDownloadFingerprint.clear();
        m_approvedDownloadOperationId.clear();
        setDownloadState(DownloadState::Idle);
        restoreStateAfterDownload();
        const QString message = QStringLiteral("下载已取消：用户未确认下载快照，未向设备写入程序。");
        emitDownloadDiagnostic(QStringLiteral("info"),
                               QStringLiteral("confirmation"),
                               message,
                               {{QStringLiteral("artifactPath"), trimmedArtifactPath},
                                {QStringLiteral("operationId"), m_currentDownloadOperationId}});
        AppLogging::writeBusinessEvent(QStringLiteral("download_canceled"),
                                       QtWarningMsg,
                                       m_currentDownloadOperationId,
                                       QStringLiteral("confirmation"),
                                       QStringLiteral("canceled"),
                                       static_cast<int>(CommErrorCode::OperationCancelled),
                                       trimmedArtifactPath);
        emit downloadFinished(false, message);
        emit logMessage(QStringLiteral("[%1] %2").arg(currentTimeLabel(), message));
        return false;
    }

    if (auto* controller = qobject_cast<ControllerDeviceBackend*>(m_backend)) {
        if (controller->supportsAsyncDownload()) {
            requestControllerDownloadAttempt(controller, trimmedArtifactPath, effectiveOptions, prevState,
                                             1, maxAttempts, m_currentDownloadOperationId);
            return true; // Accepted; completion is reported by downloadFinished.
        }
    }

    QString lastErrorMsg;
    DownloadState lastFailureState = DownloadState::Failed;
    int attemptsUsed = 0;

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        if (m_downloadCancelled || state() == RuntimeSessionState::Idle) {
            m_lastDownloadRecord.valid = false;
            AppLogging::writeBusinessEvent(QStringLiteral("download_canceled"),
                                           QtWarningMsg,
                                           m_currentDownloadOperationId,
                                           QStringLiteral("transfer"),
                                           QStringLiteral("canceled"),
                                           0,
                                           trimmedArtifactPath,
                                           {{QStringLiteral("attempt"), attempt}},
                                           attempt);
            return false;
        }

        DownloadConfirmationSnapshot attemptSnapshot = buildDownloadSnapshot(
                    trimmedArtifactPath, effectiveOptions);
        // Keep the operation's approved evidence stable across retries. The
        // previously recorded program becomes unknown as soon as writing starts.
        attemptSnapshot.versionEvidence = m_activeDownloadSnapshot.versionEvidence;
        if (attemptSnapshot.fingerprint() != m_approvedDownloadFingerprint
                || m_approvedDownloadOperationId != m_currentDownloadOperationId) {
            return finishFailure(DownloadState::PrecheckFailed,
                                 QStringLiteral("下载批准快照已变化，已阻止本次写入/重试。"),
                                 attemptsUsed);
        }

        attemptsUsed = attempt;
        if (attempt > 1) {
            setDownloadState(DownloadState::Retrying);
            emit logMessage(QStringLiteral("[%1] 下载重试 %2/%3")
                            .arg(currentTimeLabel(),
                                 QString::number(attempt),
                                 QString::number(maxAttempts)));
        }

        AppLogging::writeBusinessEvent(QStringLiteral("download_attempt_start"),
                                       QtInfoMsg,
                                       m_currentDownloadOperationId,
                                       QStringLiteral("transfer"),
                                       QStringLiteral("started"),
                                       0,
                                       trimmedArtifactPath,
                                       {{QStringLiteral("attempt"), attempt},
                                        {QStringLiteral("maxAttempts"), maxAttempts}},
                                       attempt);

        setState(RuntimeSessionState::Downloading);
        if (m_downloadCancelled || state() == RuntimeSessionState::Idle) {
            m_lastDownloadRecord.valid = false;
            AppLogging::writeBusinessEvent(QStringLiteral("download_canceled"),
                                           QtWarningMsg,
                                           m_currentDownloadOperationId,
                                           QStringLiteral("transfer"),
                                           QStringLiteral("canceled"),
                                           0,
                                           trimmedArtifactPath,
                                           {{QStringLiteral("attempt"), attempt}},
                                           attempt);
            return false;
        }
        setDownloadState(DownloadState::Downloading);
        emit downloadProgressChanged(25);

        if (m_downloadCancelled || state() == RuntimeSessionState::Idle) {
            AppLogging::writeBusinessEvent(QStringLiteral("download_canceled"),
                                           QtWarningMsg,
                                           m_currentDownloadOperationId,
                                           QStringLiteral("transfer"),
                                           QStringLiteral("canceled"),
                                           0,
                                           trimmedArtifactPath,
                                           {{QStringLiteral("attempt"), attempt}},
                                           attempt);
            return false;
        }

        QString errorMsg;
        CommError operationError;
        m_lastDownloadRecord.valid = false;
        m_backendDownloadInProgress = true;
        emit workerOperationBusyChanged(true);
        const bool ok = m_backend->downloadArtifact(trimmedArtifactPath,
                                                    effectiveOptions,
                                                    &errorMsg,
                                                    &operationError);
        m_backendDownloadInProgress = false;
        emit workerOperationBusyChanged(hasWorkerOperationInFlight());

        // requestStop() may run through ModbusInterface's nested event loop while
        // downloadArtifact() owns the backend operation lock. Disconnect only
        // after downloadArtifact() returns and releases that lock.
        if (m_downloadCancelled && m_backend && m_backend == m_ownedControllerBackend) {
            m_backend->disconnectBackend();
        }

        if (m_downloadCancelled || state() == RuntimeSessionState::Idle) {
            AppLogging::writeBusinessEvent(QStringLiteral("download_canceled"),
                                           QtWarningMsg,
                                           m_currentDownloadOperationId,
                                           QStringLiteral("transfer"),
                                           QStringLiteral("canceled"),
                                           0,
                                           trimmedArtifactPath,
                                           {{QStringLiteral("attempt"), attempt}},
                                           attempt);
            return false;
        }

        if (ok) {
            if (state() == RuntimeSessionState::Fault) {
                return finishFailure(DownloadState::TransportFailed,
                                     QStringLiteral("设备后端在下载期间断开。"),
                                     attemptsUsed);
            }
            setDownloadState(DownloadState::Verifying);
            emit downloadProgressChanged(75);

            if (m_downloadCancelled || state() == RuntimeSessionState::Idle) {
                AppLogging::writeBusinessEvent(QStringLiteral("download_canceled"),
                                               QtWarningMsg,
                                               m_currentDownloadOperationId,
                                               QStringLiteral("verification"),
                                               QStringLiteral("canceled"),
                                               0,
                                               trimmedArtifactPath,
                                               {{QStringLiteral("attempt"), attempt}},
                                               attempt);
                return false;
            }

            const BackendStatusSnapshot snapshot = m_backend->statusSnapshot();
            if (!isDownloadVerificationPassed(snapshot)) {
                lastFailureState = DownloadState::VerifyFailed;
                lastErrorMsg = QStringLiteral("下载后传输状态核对失败：后端未保持在线、写入仍在进行或报告错误。");
                AppLogging::writeBusinessEvent(QStringLiteral("download_verify_failed"),
                                               QtCriticalMsg,
                                               m_currentDownloadOperationId,
                                               QStringLiteral("verification"),
                                               QStringLiteral("failed"),
                                               static_cast<int>(snapshot.lastErrorCode),
                                               trimmedArtifactPath,
                                               {{QStringLiteral("attempt"), attempt},
                                                {QStringLiteral("error"), lastErrorMsg}},
                                               attempt);
                if (attempt < maxAttempts) {
                    setDownloadState(DownloadState::Retrying);
                    continue;
                }

                return finishFailure(lastFailureState, lastErrorMsg, attemptsUsed);
            }

            emit downloadProgressChanged(100);
            setDownloadState(DownloadState::Succeeded);
            recordSuccessfulDownload(m_activeDownloadSnapshot);
            m_runFlowOperationIdContext.clear();
            m_activeDownloadSnapshot = DownloadConfirmationSnapshot();
            restoreStateAfterDownload();
            AppLogging::writeBusinessEvent(QStringLiteral("download_succeeded"),
                                           QtInfoMsg,
                                           m_currentDownloadOperationId,
                                           QStringLiteral("finish"),
                                           QStringLiteral("succeeded"),
                                           0,
                                           trimmedArtifactPath,
                                           {{QStringLiteral("attemptsUsed"), attemptsUsed}},
                                           attemptsUsed);
            emit downloadFinished(true, QStringLiteral("下载成功：%1").arg(trimmedArtifactPath));
            emit logMessage(QStringLiteral("[%1] 编译产物下载成功")
                            .arg(currentTimeLabel()));
            emitDownloadDiagnostic(QStringLiteral("info"),
                                   QStringLiteral("download"),
                                   QStringLiteral("下载成功。"),
                                    {{QStringLiteral("artifactPath"), trimmedArtifactPath},
                                    {QStringLiteral("attemptsUsed"), attemptsUsed}});
            return true;
        }

        lastFailureState = classifyDownloadFailure(&operationError, errorMsg);
        lastErrorMsg = QStringLiteral("下载失败：%1").arg(errorMsg.isEmpty()
                                                                 ? QStringLiteral("未知错误")
                                                                 : errorMsg);

        AppLogging::writeBusinessEvent(QStringLiteral("download_attempt_failed"),
                                       QtWarningMsg,
                                       m_currentDownloadOperationId,
                                       QStringLiteral("transfer"),
                                       QStringLiteral("failed"),
                                       static_cast<int>(operationError.code),
                                       trimmedArtifactPath,
                                       {{QStringLiteral("attempt"), attempt},
                                        {QStringLiteral("error"), errorMsg},
                                        {QStringLiteral("state"), downloadStateLabel(lastFailureState)}},
                                       attempt);

        const bool ownedTransportRetry = attempt < maxAttempts
                && lastFailureState == DownloadState::TransportFailed
                && m_backend == m_ownedControllerBackend;
        if (state() == RuntimeSessionState::Fault && !ownedTransportRetry) {
            return finishFailure(DownloadState::TransportFailed,
                                 QStringLiteral("设备后端在下载期间断开。"),
                                 attemptsUsed);
        }

        if (attempt < maxAttempts && isDownloadRetryable(lastFailureState)) {
            setDownloadState(DownloadState::Retrying);
            AppLogging::writeBusinessEvent(QStringLiteral("download_retry"),
                                           QtWarningMsg,
                                           m_currentDownloadOperationId,
                                           QStringLiteral("retry"),
                                           QStringLiteral("retrying"),
                                           0,
                                           trimmedArtifactPath,
                                           {{QStringLiteral("attempt"), attempt},
                                            {QStringLiteral("maxAttempts"), maxAttempts}},
                                           attempt);
            emit logMessage(QStringLiteral("[%1] 下载失败，准备重试：%2")
                            .arg(currentTimeLabel(),
                                 lastErrorMsg));

            if (ownedTransportRetry) {
                m_internalReconnect = true;
                m_backend->disconnectBackend();
                const bool reconnected = m_backend->connectBackend();
                m_internalReconnect = false;
                if (!reconnected) {
                    emit logMessage(QStringLiteral("[%1] 正式后端重连失败，将在下一次尝试中再次检查。")
                                    .arg(currentTimeLabel()));
                }
            }
            continue;
        }

        return finishFailure(lastFailureState, lastErrorMsg, attemptsUsed);
    }

    return finishFailure(lastFailureState, lastErrorMsg, attemptsUsed);
}

void RuntimeSessionController::requestControllerDownloadAttempt(ControllerDeviceBackend* controller,
    const QString& path, const QVariantMap& options, RuntimeSessionState previousState,
    int attempt, int maxAttempts, const QString& operationId)
{
    if (m_backend != controller || m_downloadCancelled || state() == RuntimeSessionState::Idle) return;
    DownloadConfirmationSnapshot attemptSnapshot = buildDownloadSnapshot(path, options);
    attemptSnapshot.versionEvidence = m_activeDownloadSnapshot.versionEvidence;
    if (attemptSnapshot.fingerprint() != m_approvedDownloadFingerprint
            || m_approvedDownloadOperationId != operationId) {
        m_approvedDownloadFingerprint.clear();
        m_approvedDownloadOperationId.clear();
        m_pendingRunAfterDownload = false;
        m_activeDownloadSnapshot = DownloadConfirmationSnapshot();
        m_runFlowOperationIdContext.clear();
        setDownloadState(DownloadState::PrecheckFailed);
        setState(controller->isOnline() ? previousState : RuntimeSessionState::Fault);
        const QString message = QStringLiteral("下载批准快照已变化，已阻止本次写入/重试。");
        emitDownloadDiagnostic(QStringLiteral("error"), QStringLiteral("confirmation"), message,
                               {{QStringLiteral("attempt"), attempt}});
        emit downloadFinished(false, message);
        emit runtimeError(message);
        return;
    }
    m_backendDownloadInProgress = true;
    emit workerOperationBusyChanged(true);
    setState(RuntimeSessionState::Downloading);
    setDownloadState(attempt > 1 ? DownloadState::Retrying : DownloadState::Downloading);
    emit downloadProgressChanged(25);
    AppLogging::writeBusinessEvent(QStringLiteral("download_attempt_start"), QtInfoMsg, operationId,
        QStringLiteral("transfer"), QStringLiteral("started"), 0, path, {}, attempt);
    QPointer<ControllerDeviceBackend> backendGuard(controller);
    const quint64 generation = m_backendGeneration;
    m_internalReconnect = options.value(QStringLiteral("reconnectBeforeAttempt")).toBool();
    m_lastDownloadRecord.valid = false;
    controller->downloadArtifactAsync(path, options, this,
        [=](bool ok, QString message, CommError error) {
        m_backendDownloadInProgress = false;
        emit workerOperationBusyChanged(hasWorkerOperationInFlight());
        m_internalReconnect = false;
        if (!backendGuard || m_backendGeneration != generation || m_backend != backendGuard || m_currentDownloadOperationId != operationId) return;
        if (m_downloadCancelled || state() == RuntimeSessionState::Idle) {
            if (m_backend == m_ownedControllerBackend) m_backend->disconnectBackend();
            return; // requestStop already emitted the cancellation terminal event.
        }
        DownloadState failure = classifyDownloadFailure(&error, message);
        if (ok) {
            setDownloadState(DownloadState::Verifying);
            emit downloadProgressChanged(75);
            ok = isDownloadVerificationPassed(controller->statusSnapshot());
            if (!ok) {
                failure = DownloadState::VerifyFailed;
                message = QStringLiteral("下载后传输状态核对失败：后端未保持在线、写入仍在进行或报告错误。");
            }
        }
        if (!ok && attempt < maxAttempts && isDownloadRetryable(failure)) {
            AppLogging::writeBusinessEvent(QStringLiteral("download_retry"), QtWarningMsg, operationId,
                QStringLiteral("retry"), QStringLiteral("retrying"), static_cast<int>(error.code), path, {}, attempt);
            QVariantMap retryOptions = options;
            retryOptions.insert(QStringLiteral("reconnectBeforeAttempt"), failure == DownloadState::TransportFailed);
            QTimer::singleShot(0, this, [=]() {
                if (backendGuard) requestControllerDownloadAttempt(backendGuard, path, retryOptions, previousState,
                                                                  attempt + 1, maxAttempts, operationId);
            });
            return;
        }
        setDownloadState(ok ? DownloadState::Succeeded : failure);
        if (!ok) m_lastDownloadRecord.valid = false;
        setState(controller->isOnline() ? previousState : RuntimeSessionState::Fault);
        if (ok) {
            recordSuccessfulDownload(m_activeDownloadSnapshot);
            m_runFlowOperationIdContext.clear();
        } else {
            m_approvedDownloadFingerprint.clear();
            m_approvedDownloadOperationId.clear();
            m_runFlowOperationIdContext.clear();
        }
        m_activeDownloadSnapshot = DownloadConfirmationSnapshot();
        if (ok) emit downloadProgressChanged(100);
        AppLogging::writeBusinessEvent(ok ? QStringLiteral("download_succeeded") : QStringLiteral("download_failed"),
            ok ? QtInfoMsg : QtCriticalMsg, operationId, QStringLiteral("finish"),
            ok ? QStringLiteral("succeeded") : QStringLiteral("failed"), static_cast<int>(error.code), path,
            {{QStringLiteral("error"), message}}, attempt);
        const bool startRun = m_pendingRunAfterDownload;
        m_pendingRunAfterDownload = false;
        const QString terminalMessage = ok
                ? QStringLiteral("下载成功：%1").arg(path)
                : message + QStringLiteral(" 设备程序可能不完整，当前状态未知。");
        emitDownloadDiagnostic(ok ? QStringLiteral("info") : QStringLiteral("error"), QStringLiteral("download"),
                               terminalMessage, {{QStringLiteral("attemptsUsed"), attempt}});
        emit downloadFinished(ok, terminalMessage);
        if (!ok) {
            emit runtimeError(terminalMessage);
        }
        if (ok && startRun && !m_downloadCancelled && m_backendGeneration == generation
                && m_currentDownloadOperationId == operationId && !m_backendDownloadInProgress) {
            finishRunStart();
        }
    });
}

void RuntimeSessionController::emitDownloadDiagnostic(const QString& severity,
                                                      const QString& stage,
                                                      const QString& message,
                                                      const QVariantMap& details)
{
    QVariantMap diagnostic = details;
    diagnostic.insert(QStringLiteral("category"), QStringLiteral("download"));
    diagnostic.insert(QStringLiteral("severity"), severity);
    diagnostic.insert(QStringLiteral("stage"), stage);
    diagnostic.insert(QStringLiteral("message"), message);
    diagnostic.insert(QStringLiteral("state"), downloadStateLabel(downloadState()));
    diagnostic.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(Qt::ISODate));
    emit downloadDiagnosticChanged(diagnostic);
}

bool RuntimeSessionController::runDownloadPrecheck(const QString& artifactPath,
                                                   const QVariantMap& options,
                                                   QString* errorMessage)
{
    if (options.value(QStringLiteral("profileOverrideConflict")).toBool()) {
        if (errorMessage) {
            *errorMessage = options.value(QStringLiteral("profileOverrideError"))
                                    .toString().trimmed();
        }
        return false;
    }

    ProjectRuntimeConfig config;
    QString projectPath;
    if (m_projectController) {
        config = m_projectController->runtimeConfig();
        projectPath = m_projectController->currentProjectPath();
    }

    auto* controller = qobject_cast<ControllerDeviceBackend*>(m_backend);
    const auto report = m_runtimeService->precheck(
            config, projectPath, artifactPath, controller != nullptr);
    const QVariantMap details = report.details;
    emitDownloadDiagnostic(QStringLiteral("info"),
                           QStringLiteral("precheck"),
                           QStringLiteral("下载产物一致性检查完成。"),
                           details);
    emit logMessage(QStringLiteral("[%1] 下载前置校验：产物=%2 字节，manifest=%3，runtime_points=%4")
                    .arg(currentTimeLabel(),
                         QString::number(details.value(QStringLiteral("artifactBytes")).toLongLong()),
                         details.value(QStringLiteral("manifestExists")).toBool() ? QStringLiteral("存在") : QStringLiteral("缺失"),
                         details.value(QStringLiteral("runtimePointsExists")).toBool() ? QStringLiteral("存在") : QStringLiteral("缺失")));

    for (const QString& warning : report.warnings) {
        emit logMessage(QStringLiteral("[%1] 下载前置校验警告：%2")
                        .arg(currentTimeLabel(), warning));
        emitDownloadDiagnostic(QStringLiteral("warning"),
                               QStringLiteral("precheck"),
                               warning,
                               details);
    }

    if (!report.valid) {
        const QString message = QStringLiteral("下载前置校验失败：%1")
                .arg(report.errors.join(QStringLiteral("; ")));
        emitDownloadDiagnostic(QStringLiteral("error"),
                               QStringLiteral("precheck"),
                               message,
                               details);
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    }

    if (!controller) {
        emitDownloadDiagnostic(QStringLiteral("info"),
                               QStringLiteral("dry-run"),
                               QStringLiteral("当前后端不需要控制器 dry-run。"),
                               {{QStringLiteral("artifactPath"), artifactPath}});
        return true;
    }

    QVariantMap dryRunReport;
    QString dryRunError;
    CommError operationError;
    if (!controller->dryRunDownloadArtifact(artifactPath,
                                            options,
                                            &dryRunError,
                                            &operationError,
                                            &dryRunReport)) {
        const QString message = dryRunError.isEmpty()
                ? QStringLiteral("下载 dry-run 校验失败。")
                : QStringLiteral("下载 dry-run 校验失败：%1").arg(dryRunError);
        QVariantMap diagnosticDetails = dryRunReport;
        diagnosticDetails.insert(QStringLiteral("artifactPath"), artifactPath);
        diagnosticDetails.insert(QStringLiteral("operationErrorCode"),
                                 static_cast<int>(operationError.code));
        emitDownloadDiagnostic(QStringLiteral("error"),
                               QStringLiteral("dry-run"),
                               message,
                               diagnosticDetails);
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    }

    const QString dryRunMode = dryRunReport.value(QStringLiteral("mode")).toString().isEmpty()
            ? QStringLiteral("profile")
            : dryRunReport.value(QStringLiteral("mode")).toString();
    emit logMessage(QStringLiteral("[%1] 下载 dry-run 通过：mode=%2 steps=%3 packets=%4")
                    .arg(currentTimeLabel(),
                         dryRunMode,
                         QString::number(dryRunReport.value(QStringLiteral("stepCount")).toInt()),
                         QString::number(dryRunReport.value(QStringLiteral("estimatedPacketCount")).toInt())));
    QVariantMap diagnosticDetails = dryRunReport;
    diagnosticDetails.insert(QStringLiteral("artifactPath"), artifactPath);
    emitDownloadDiagnostic(QStringLiteral("info"),
                           QStringLiteral("dry-run"),
                           QStringLiteral("下载 dry-run 通过。"),
                           diagnosticDetails);
    return true;
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
