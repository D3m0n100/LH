// File: src/designer/RuntimeSessionDownload.cpp

#include "RuntimeSessionController.h"

#include "ProjectController.h"
#include "RunController.h"
#include "../communication/ControllerDeviceBackend.h"
#include "../communication/IDeviceBackend.h"

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

QString currentTimeLabel()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
}

QVariantMap resolveDownloadOptions(const QVariantMap& options,
                                   const ProjectRuntimeConfig& config,
                                   const QString& projectPath)
{
    QVariantMap resolved = options;
    QString profilePath;
    const QStringList profileKeys = {
        QStringLiteral("downloadProfilePath"),
        QStringLiteral("profileJsonPath"),
        QStringLiteral("profilePath")
    };
    for (const QString& key : profileKeys) {
        profilePath = options.value(key).toString().trimmed();
        if (!profilePath.isEmpty()) {
            break;
        }
    }
    if (profilePath.isEmpty()) {
        profilePath = config.downloadArtifact.metadata
                              .value(QStringLiteral("downloadProfilePath"))
                              .toString().trimmed();
    }
    if (!profilePath.isEmpty()) {
        QFileInfo profileInfo(profilePath);
        if (profileInfo.isRelative() && !projectPath.trimmed().isEmpty()) {
            profilePath = QDir(projectPath).absoluteFilePath(profilePath);
        }
        resolved.insert(QStringLiteral("downloadProfilePath"), QDir::cleanPath(profilePath));
    }
    return resolved;
}

} // namespace

bool RuntimeSessionController::requestDownload(const QString& artifactPath, const QVariantMap& options)
{
    m_downloadCancelled = false;
    setDownloadState(DownloadState::Precheck);
    emit downloadProgressChanged(0);

    const QString trimmedArtifactPath = artifactPath.trimmed();
    const RuntimeSessionState prevState = m_state;
    ProjectRuntimeConfig runtimeConfig;
    QString projectPath;
    if (m_projectController) {
        runtimeConfig = m_projectController->runtimeConfig();
        projectPath = m_projectController->currentProjectPath();
    }
    const QVariantMap effectiveOptions = resolveDownloadOptions(options, runtimeConfig, projectPath);
    const int maxAttempts = qMax(1, effectiveOptions.value(QStringLiteral("retryCount"), 1).toInt());

    const auto restoreStateAfterDownload = [this, prevState]() {
        if (m_state == RuntimeSessionState::Idle || m_state == RuntimeSessionState::Fault)
            return;

        if (!m_backend || !m_backend->isOnline()) {
            setState(RuntimeSessionState::Fault);
            return;
        }

        setState(prevState);
    };

    emitDownloadDiagnostic(QStringLiteral("info"),
                           QStringLiteral("precheck"),
                           QStringLiteral("开始下载前置校验。"),
                           {{QStringLiteral("artifactPath"), trimmedArtifactPath},
                            {QStringLiteral("retryCount"), maxAttempts}});

    auto failPrecheck = [&](const QString& message, bool diagnosticAlreadyEmitted = false) {
        if (m_downloadCancelled || m_state == RuntimeSessionState::Idle)
            return false;

        setDownloadState(DownloadState::PrecheckFailed);
        if (!diagnosticAlreadyEmitted) {
            emitDownloadDiagnostic(QStringLiteral("error"),
                                   QStringLiteral("precheck"),
                                   message,
                                   {{QStringLiteral("artifactPath"), trimmedArtifactPath}});
        }
        restoreStateAfterDownload();
        emit downloadFinished(false, message);
        emit runtimeError(message);
        return false;
    };

    auto finishFailure = [&](DownloadState failureState,
                             const QString& rawMessage,
                             int attemptsUsed) {
        if (m_downloadCancelled || m_state == RuntimeSessionState::Idle)
            return false;

        setDownloadState(failureState);
        const QString finalMessage = QStringLiteral("下载失败[%1/%2,%3]：%4")
                .arg(QString::number(attemptsUsed),
                     QString::number(maxAttempts),
                     downloadStateLabel(failureState),
                     rawMessage);
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

    if (m_state != RuntimeSessionState::Running && m_state != RuntimeSessionState::Connected) {
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

    QString lastErrorMsg;
    DownloadState lastFailureState = DownloadState::Failed;
    int attemptsUsed = 0;

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        if (m_downloadCancelled || m_state == RuntimeSessionState::Idle) {
            return false;
        }

        attemptsUsed = attempt;
        if (attempt > 1) {
            setDownloadState(DownloadState::Retrying);
            emit logMessage(QStringLiteral("[%1] 下载重试 %2/%3")
                            .arg(currentTimeLabel(),
                                 QString::number(attempt),
                                 QString::number(maxAttempts)));
        }

        setState(RuntimeSessionState::Downloading);
        if (m_downloadCancelled || m_state == RuntimeSessionState::Idle) {
            return false;
        }
        setDownloadState(DownloadState::Downloading);
        emit downloadProgressChanged(25);

        if (m_downloadCancelled || m_state == RuntimeSessionState::Idle) {
            return false;
        }

        QString errorMsg;
        CommError operationError;
        const bool ok = m_backend->downloadArtifact(trimmedArtifactPath,
                                                    effectiveOptions,
                                                    &errorMsg,
                                                    &operationError);

        if (m_downloadCancelled || m_state == RuntimeSessionState::Idle) {
            return false;
        }

        if (m_state == RuntimeSessionState::Fault) {
            return finishFailure(DownloadState::TransportFailed,
                                 QStringLiteral("设备后端在下载期间断开。"),
                                 attemptsUsed);
        }

        if (ok) {
            setDownloadState(DownloadState::Verifying);
            emit downloadProgressChanged(75);

            if (m_downloadCancelled || m_state == RuntimeSessionState::Idle) {
                return false;
            }

            const BackendStatusSnapshot snapshot = m_backend->statusSnapshot();
            if (!isDownloadVerificationPassed(snapshot)) {
                lastFailureState = DownloadState::VerifyFailed;
                lastErrorMsg = QStringLiteral("下载后校验失败：后端状态未达成成功条件。");
                if (attempt < maxAttempts) {
                    setDownloadState(DownloadState::Retrying);
                    continue;
                }

                return finishFailure(lastFailureState, lastErrorMsg, attemptsUsed);
            }

            emit downloadProgressChanged(100);
            setDownloadState(DownloadState::Succeeded);
            restoreStateAfterDownload();
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

        if (attempt < maxAttempts && isDownloadRetryable(lastFailureState)) {
            setDownloadState(DownloadState::Retrying);
            emit logMessage(QStringLiteral("[%1] 下载失败，准备重试：%2")
                            .arg(currentTimeLabel(),
                                 lastErrorMsg));
            continue;
        }

        return finishFailure(lastFailureState, lastErrorMsg, attemptsUsed);
    }

    return finishFailure(lastFailureState, lastErrorMsg, attemptsUsed);
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
    diagnostic.insert(QStringLiteral("state"), downloadStateLabel(m_downloadState));
    diagnostic.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(Qt::ISODate));
    emit downloadDiagnosticChanged(diagnostic);
}

bool RuntimeSessionController::runDownloadPrecheck(const QString& artifactPath,
                                                   const QVariantMap& options,
                                                   QString* errorMessage)
{
    ProjectRuntimeConfig config;
    QString projectPath;
    if (m_projectController) {
        config = m_projectController->runtimeConfig();
        projectPath = m_projectController->currentProjectPath();
    }

    const auto report = RunController::validateDownloadArtifact(config, projectPath, artifactPath);
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

    auto* controller = qobject_cast<ControllerDeviceBackend*>(m_backend);
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
