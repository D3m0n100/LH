// File: src/communication/VirtualDeviceBackend.cpp

#include "VirtualDeviceBackend.h"
#include "Common.h"

#include <QDateTime>
#include <QThread>

#include <optional>

VirtualDeviceBackend::VirtualDeviceBackend(QObject* parent)
    : IDeviceBackend(parent)
{
}

void VirtualDeviceBackend::loadPointDefinitions(const QList<RuntimePointDefinition>& defs)
{
    QMutexLocker lock(&m_mutex);
    for (const auto& def : defs) {
        m_definitions.insert(def.id, def);
        if (!m_values.contains(def.id))
            m_values.insert(def.id, def.defaultValue);
    }
}

void VirtualDeviceBackend::loadPointDefinitions(const RuntimePointTable& table)
{
    const auto snapshots = table.allSnapshots();
    QMutexLocker lock(&m_mutex);
    for (const auto& snap : snapshots) {
        m_definitions.insert(snap.definition.id, snap.definition);
        if (!m_values.contains(snap.definition.id))
            m_values.insert(snap.definition.id, snap.current.value);
    }
}

void VirtualDeviceBackend::clearPoints()
{
    QMutexLocker lock(&m_mutex);
    m_definitions.clear();
    m_values.clear();
}

bool VirtualDeviceBackend::connectBackend()
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_online)
            return true;
        m_online = true;
        m_lastOperationPartialSuccess = false;
        m_lastDownloadError.clear();
    }
    clearError();
    emit connectionStateChanged(true);
    LOG_INFO("[VirtualDeviceBackend] connected");
    return true;
}

void VirtualDeviceBackend::disconnectBackend()
{
    {
        QMutexLocker lock(&m_mutex);
        if (!m_online)
            return;
        m_online = false;
        m_downloading = false;
        m_downloadPercent = 0;
    }
    emit connectionStateChanged(false);
    LOG_INFO("[VirtualDeviceBackend] disconnected");
}

bool VirtualDeviceBackend::isOnline() const
{
    QMutexLocker lock(&m_mutex);
    return m_online;
}

bool VirtualDeviceBackend::readPoints(const QStringList& pointIds,
                                      QHash<QString, QVariant>& values,
                                      QString* errorMessage,
                                      QHash<QString, CommError>* pointErrors)
{
    values.clear();
    if (pointErrors) {
        pointErrors->clear();
    }

    QString localError;
    if (!checkOnline(&localError)) {
        if (errorMessage)
            *errorMessage = localError;
        {
            QMutexLocker lock(&m_mutex);
            m_lastOperationPartialSuccess = false;
        }
        reportError(CommErrorCode::ConnectionLost, localError);
        return false;
    }
    if (!checkReadFault(&localError)) {
        if (errorMessage)
            *errorMessage = localError;
        {
            QMutexLocker lock(&m_mutex);
            m_lastOperationPartialSuccess = false;
        }
        reportError(CommErrorCode::InternalError, localError);
        return false;
    }

    if (m_simulatedLatencyMs > 0) {
        QThread::msleep(static_cast<unsigned long>(m_simulatedLatencyMs));
    }

    QHash<QString, CommError> localPointErrors;
    CommError firstError;
    bool hasError = false;

    {
        QMutexLocker lock(&m_mutex);
        for (const auto& id : pointIds) {
            auto it = m_values.constFind(id);
            if (it != m_values.constEnd()) {
                values.insert(id, it.value());
                continue;
            }

            CommError pointError(CommProtocolType::Custom,
                                 CommErrorCode::InvalidAddress,
                                 QStringLiteral("Point not found: %1").arg(id));
            localPointErrors.insert(id, pointError);
            if (!hasError) {
                firstError = pointError;
                hasError = true;
            }
        }
    }

    if (pointErrors) {
        *pointErrors = localPointErrors;
    }

    if (hasError) {
        {
            QMutexLocker lock(&m_mutex);
            m_lastOperationPartialSuccess = true;
        }
        if (errorMessage)
            *errorMessage = firstError.message;
        reportError(firstError.code, firstError.message, QStringLiteral("partial success"));
        return false;
    }

    {
        QMutexLocker lock(&m_mutex);
        m_lastOperationPartialSuccess = false;
    }
    clearError();
    return true;
}

bool VirtualDeviceBackend::writePoints(const QHash<QString, QVariant>& writes,
                                       QString* errorMessage,
                                       QHash<QString, CommError>* pointErrors)
{
    if (pointErrors) {
        pointErrors->clear();
    }

    QString localError;
    if (!checkOnline(&localError)) {
        if (errorMessage)
            *errorMessage = localError;
        {
            QMutexLocker lock(&m_mutex);
            m_lastOperationPartialSuccess = false;
        }
        reportError(CommErrorCode::ConnectionLost, localError);
        return false;
    }
    if (!checkWriteFault(&localError)) {
        if (errorMessage)
            *errorMessage = localError;
        {
            QMutexLocker lock(&m_mutex);
            m_lastOperationPartialSuccess = false;
        }
        reportError(CommErrorCode::InternalError, localError);
        return false;
    }

    if (m_simulatedLatencyMs > 0) {
        QThread::msleep(static_cast<unsigned long>(m_simulatedLatencyMs));
    }

    QHash<QString, CommError> localPointErrors;
    QHash<QString, QVariant> changed;
    CommError firstError;
    bool hasError = false;

    {
        QMutexLocker lock(&m_mutex);
        for (auto it = writes.constBegin(); it != writes.constEnd(); ++it) {
            auto defIt = m_definitions.constFind(it.key());
            if (defIt == m_definitions.constEnd()) {
                CommError pointError(CommProtocolType::Custom,
                                     CommErrorCode::InvalidAddress,
                                     QStringLiteral("Point not found: %1").arg(it.key()));
                localPointErrors.insert(it.key(), pointError);
                if (!hasError) {
                    firstError = pointError;
                    hasError = true;
                }
                continue;
            }
            if (defIt->access == RuntimePointAccess::ReadOnly) {
                CommError pointError(CommProtocolType::Custom,
                                     CommErrorCode::PermissionDenied,
                                     QStringLiteral("Point is read-only: %1").arg(it.key()));
                localPointErrors.insert(it.key(), pointError);
                if (!hasError) {
                    firstError = pointError;
                    hasError = true;
                }
                continue;
            }
            m_values.insert(it.key(), it.value());
            changed.insert(it.key(), it.value());
        }
    }

    if (pointErrors) {
        *pointErrors = localPointErrors;
    }

    if (!changed.isEmpty()) {
        emit pointsChanged(changed);
    }

    if (hasError) {
        {
            QMutexLocker lock(&m_mutex);
            m_lastOperationPartialSuccess = true;
        }
        if (errorMessage)
            *errorMessage = firstError.message;
        reportError(firstError.code, firstError.message, QStringLiteral("partial success"));
        return false;
    }

    {
        QMutexLocker lock(&m_mutex);
        m_lastOperationPartialSuccess = false;
    }
    clearError();
    return true;
}

bool VirtualDeviceBackend::downloadArtifact(const QString& artifactPath,
                                            const QVariantMap& options,
                                            QString* errorMessage,
                                            CommError* operationError)
{
    QString localError;
    auto setFailure = [&](CommErrorCode code, const QString& message) {
        localError = message;
        if (errorMessage)
            *errorMessage = localError;
        if (operationError)
            *operationError = CommError(CommProtocolType::Custom, code, localError);
        {
            QMutexLocker lock(&m_mutex);
            m_downloading = false;
            m_downloadPercent = 0;
            m_lastOperationPartialSuccess = false;
            m_lastDownloadError = localError;
        }
        reportError(code, localError);
        return false;
    };

    auto consumeInjectedDownloadFailure = [&]() -> std::optional<CommErrorCode> {
        QMutexLocker lock(&m_mutex);
        if (m_downloadFailureAttemptsRemaining <= 0) {
            return std::nullopt;
        }

        --m_downloadFailureAttemptsRemaining;
        return m_downloadFailureCode;
    };

    auto failureCodeFromOptions = [&]() -> std::optional<CommErrorCode> {
        const QVariant codeValue = options.value(QStringLiteral("simulateDownloadErrorCode"));
        bool ok = false;
        const int rawCode = codeValue.toInt(&ok);
        if (ok) {
            return static_cast<CommErrorCode>(rawCode);
        }

        const QString mode = options.value(QStringLiteral("simulateDownloadFailure")).toString().trimmed().toLower();
        if (mode == QStringLiteral("transport")) {
            return CommErrorCode::ConnectionLost;
        }
        if (mode == QStringLiteral("rejected")) {
            return CommErrorCode::PermissionDenied;
        }
        if (mode == QStringLiteral("verify")) {
            return CommErrorCode::InvalidResponse;
        }
        if (mode == QStringLiteral("protocol")) {
            return CommErrorCode::ProtocolError;
        }
        return std::nullopt;
    };

    if (const auto injectedCode = consumeInjectedDownloadFailure()) {
        const QString message = m_downloadFailureMessage.isEmpty()
                ? QStringLiteral("Injected download fault")
                : m_downloadFailureMessage;
        return setFailure(*injectedCode, message);
    }

    if (const auto simulatedCode = failureCodeFromOptions()) {
        const QString simulatedMessage = options.value(QStringLiteral("simulateDownloadMessage")).toString();
        const QString message = simulatedMessage.isEmpty()
                ? QStringLiteral("Injected download fault")
                : simulatedMessage;
        return setFailure(*simulatedCode, message);
    }

    if (artifactPath.trimmed().isEmpty()) {
        localError = QStringLiteral("Artifact path is empty");
        return setFailure(CommErrorCode::InvalidParameter, localError);
    }

    if (!checkOnline(&localError)) {
        return setFailure(CommErrorCode::ConnectionLost, localError);
    }
    if (!checkDownloadFault(&localError)) {
        return setFailure(CommErrorCode::InternalError, localError);
    }

    if (m_simulatedLatencyMs > 0) {
        QThread::msleep(static_cast<unsigned long>(m_simulatedLatencyMs));
    }

    {
        QMutexLocker lock(&m_mutex);
        m_downloading = true;
        m_downloadPercent = 0;
        m_lastDownloadError.clear();
        m_lastOperationPartialSuccess = false;
    }

    {
        QMutexLocker lock(&m_mutex);
        m_downloading = false;
        m_downloadPercent = 100;
        m_lastDownloadError.clear();
    }

    clearError();
    if (operationError) {
        *operationError = CommError();
    }

    LOG_INFO(QStringLiteral("[VirtualDeviceBackend] download simulated: %1").arg(artifactPath));
    return true;
}

BackendStatusSnapshot VirtualDeviceBackend::statusSnapshot() const
{
    QMutexLocker lock(&m_mutex);
    const CommError err = lastError();

    BackendStatusSnapshot snapshot;
    snapshot.online = m_online;
    snapshot.backendType = QStringLiteral("virtual");
    snapshot.downloading = m_downloading;
    snapshot.downloadPercent = m_downloadPercent;
    snapshot.lastErrorCode = err.code;
    snapshot.lastErrorMessage = err.message;
    snapshot.lastErrorDetails = err.details;
    snapshot.partialSuccess = m_lastOperationPartialSuccess;
    snapshot.timestamp = QDateTime::currentDateTimeUtc();

    snapshot.extras[QStringLiteral("online")] = snapshot.online;
    snapshot.extras[QStringLiteral("backend")] = snapshot.backendType;
    snapshot.extras[QStringLiteral("pointCount")] = m_definitions.size();
    snapshot.extras[QStringLiteral("downloading")] = snapshot.downloading;
    snapshot.extras[QStringLiteral("downloadPercent")] = snapshot.downloadPercent;
    snapshot.extras[QStringLiteral("lastErrorCode")] = static_cast<int>(snapshot.lastErrorCode);
    snapshot.extras[QStringLiteral("lastErrorMessage")] = snapshot.lastErrorMessage;
    snapshot.extras[QStringLiteral("lastErrorDetails")] = snapshot.lastErrorDetails;
    snapshot.extras[QStringLiteral("partialSuccess")] = snapshot.partialSuccess;
    snapshot.extras[QStringLiteral("timestamp")] = snapshot.timestamp;
    if (!m_lastDownloadError.isEmpty())
        snapshot.extras[QStringLiteral("lastDownloadError")] = m_lastDownloadError;
    snapshot.extras[QStringLiteral("faultRead")] = m_faultRead;
    snapshot.extras[QStringLiteral("faultWrite")] = m_faultWrite;
    snapshot.extras[QStringLiteral("faultDownload")] = m_faultDownload;
    snapshot.extras[QStringLiteral("simulatedLatencyMs")] = m_simulatedLatencyMs;
    snapshot.extras[QStringLiteral("downloadFaultInjectionRemaining")] = m_downloadFailureAttemptsRemaining;
    snapshot.extras[QStringLiteral("downloadFaultInjectionCode")] = static_cast<int>(m_downloadFailureCode);
    if (!m_downloadFailureMessage.isEmpty())
        snapshot.extras[QStringLiteral("downloadFaultInjectionMessage")] = m_downloadFailureMessage;
    return snapshot;
}

void VirtualDeviceBackend::setFaultInjection(bool readFail, bool writeFail, bool downloadFail)
{
    QMutexLocker lock(&m_mutex);
    m_faultRead = readFail;
    m_faultWrite = writeFail;
    m_faultDownload = downloadFail;
}

void VirtualDeviceBackend::setSimulatedLatencyMs(int ms)
{
    QMutexLocker lock(&m_mutex);
    m_simulatedLatencyMs = ms;
}

void VirtualDeviceBackend::setDownloadFaultInjection(int attempts,
                                                     CommErrorCode code,
                                                     const QString& message)
{
    QMutexLocker lock(&m_mutex);
    m_downloadFailureAttemptsRemaining = qMax(0, attempts);
    m_downloadFailureCode = code;
    m_downloadFailureMessage = message;
}

void VirtualDeviceBackend::clearDownloadFaultInjection()
{
    QMutexLocker lock(&m_mutex);
    m_downloadFailureAttemptsRemaining = 0;
    m_downloadFailureCode = CommErrorCode::InternalError;
    m_downloadFailureMessage.clear();
}

bool VirtualDeviceBackend::checkOnline(QString* errorMessage) const
{
    QMutexLocker lock(&m_mutex);
    if (!m_online) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Device is offline");
        return false;
    }
    return true;
}

bool VirtualDeviceBackend::checkReadFault(QString* errorMessage) const
{
    QMutexLocker lock(&m_mutex);
    if (m_faultRead) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Injected read fault");
        return false;
    }
    return true;
}

bool VirtualDeviceBackend::checkWriteFault(QString* errorMessage) const
{
    QMutexLocker lock(&m_mutex);
    if (m_faultWrite) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Injected write fault");
        return false;
    }
    return true;
}

bool VirtualDeviceBackend::checkDownloadFault(QString* errorMessage) const
{
    QMutexLocker lock(&m_mutex);
    if (m_faultDownload) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Injected download fault");
        return false;
    }
    return true;
}
