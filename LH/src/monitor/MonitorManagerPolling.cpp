#include "MonitorManager.h"
#include "communication/RuntimePointQualityMapper.h"
#include "communication/IDeviceBackend.h"

#include <QReadLocker>
#include <QWriteLocker>
#include <QDebug>
#include <QSet>

#include <exception>
#include <utility>

namespace Monitor {

namespace {

static QString qualityToString(RuntimePointQuality q)
{
    return runtimePointQualityToString(q);
}

static RuntimePointQuality qualityFromBackendError(const CommError& error, bool backendOnline)
{
    return runtimePointQualityFromBackendError(error, backendOnline);
}

static void attachBackendStatusMetadata(Sample& sample, const BackendStatusSnapshot& status)
{
    sample.metadata[QStringLiteral("backendType")] = status.backendType;
    sample.metadata[QStringLiteral("backendOnline")] = status.online;
    sample.metadata[QStringLiteral("backendDownloading")] = status.downloading;
    sample.metadata[QStringLiteral("backendDownloadPercent")] = status.downloadPercent;
    sample.metadata[QStringLiteral("backendLastErrorCode")] = static_cast<int>(status.lastErrorCode);
    sample.metadata[QStringLiteral("backendLastErrorCodeName")] = commErrorCodeToString(status.lastErrorCode);
    sample.metadata[QStringLiteral("backendLastErrorMessage")] = status.lastErrorMessage;
    sample.metadata[QStringLiteral("backendLastErrorDetails")] = status.lastErrorDetails;
    sample.metadata[QStringLiteral("backendPartialSuccess")] = status.partialSuccess;
    sample.metadata[QStringLiteral("backendTimestamp")] = status.timestamp;
}

} // namespace

void MonitorManager::setDeviceBackend(IDeviceBackend* backend)
{
    if (m_backend == backend)
        return;

    if (m_backendPointsChangedConnection) {
        disconnect(m_backendPointsChangedConnection);
        m_backendPointsChangedConnection = {};
    }
    if (m_backendConnectionStateConnection) {
        disconnect(m_backendConnectionStateConnection);
        m_backendConnectionStateConnection = {};
    }
    if (m_backendDestroyedConnection) {
        disconnect(m_backendDestroyedConnection);
        m_backendDestroyedConnection = {};
    }

    m_backend = backend;
    m_backendPollTimer->stop();
    m_backendPointIds.clear();
    m_pointIdToChannel.clear();
    m_backendPointPeriodsMs.clear();
    m_backendPointNextDueMs.clear();

    if (backend) {
        m_backendDestroyedConnection = connect(
            backend, &QObject::destroyed, this, [this]() {
                m_backend = nullptr;
                m_backendPollTimer->stop();
                m_backendPointIds.clear();
                m_pointIdToChannel.clear();
                m_backendPointPeriodsMs.clear();
                m_backendPointNextDueMs.clear();
                m_backendPointsChangedConnection = {};
                m_backendConnectionStateConnection = {};
                m_backendDestroyedConnection = {};
            });

        // 连接 backend 的 pointsChanged 信号，实时更新通道
        const QPointer<IDeviceBackend> sourceBackend = backend;
        m_backendPointsChangedConnection =
                connect(backend, &IDeviceBackend::pointsChanged,
                        this, [this, sourceBackend](const QHash<QString, QVariant>& updates) {
                            if (!m_isMonitoring.load(std::memory_order_acquire)
                                    || !sourceBackend
                                    || m_backend.data() != sourceBackend.data()) {
                                return;
                            }
                            for (auto it = updates.constBegin(); it != updates.constEnd(); ++it) {
                                const QString channelName = m_pointIdToChannel.value(it.key());
                                if (channelName.isEmpty())
                                    continue;
                                bool valueOk = false;
                                const double value = it.value().toDouble(&valueOk);
                                Sample sample;
                                sample.channelName = channelName;
                                sample.value = value;
                                sample.valueValid = valueOk;
                                sample.quality = valueOk ? RuntimePointQuality::Good
                                                         : RuntimePointQuality::Bad;
                                sample.timestamp = QDateTime::currentDateTimeUtc();
                                sample.metadata[QStringLiteral("quality")] = qualityToString(sample.quality);
                                sample.metadata[QStringLiteral("valueValid")] = sample.valueValid;
                                sample.metadata[QStringLiteral("source")] = QStringLiteral("backend_push");
                                recordSample(sample);
                            }
                        });

        m_backendConnectionStateConnection =
                connect(backend, &IDeviceBackend::connectionStateChanged,
                        this, [this](bool connected) {
                            qDebug() << "[MonitorManager] backend connectionStateChanged:" << connected;
                        });

        qDebug() << "[MonitorManager] device backend set:" << backend;
    } else {
        qDebug() << "[MonitorManager] device backend cleared";
    }
}
bool MonitorManager::registerProvider(const ProviderConfig& config)
{
    if (config.id.isEmpty() || config.channelName.isEmpty()) {
        qWarning() << "[MonitorManager] 无法注册采集器：ID 或通道名为空";
        return false;
    }

    // 确保通道存在
    if (!hasChannel(config.channelName)) {
        ChannelConfig channelConfig;
        channelConfig.name = config.channelName;
        channelConfig.unit = config.unit;
        if (config.channelOverride.has_value()) {
            channelConfig = config.channelOverride.value();
        }
        registerChannel(channelConfig);
    }

    {
        QWriteLocker locker(&m_providerLock);

        // 若已存在，先停止旧定时器
        if (m_providerTimers.contains(config.id)) {
            m_providerTimers[config.id]->stop();
            m_providerTimers[config.id]->deleteLater();
            m_providerTimers.remove(config.id);
        }

        m_providers[config.id] = config;

        if (config.sampler && config.periodMs > 0) {
            QTimer* timer = new QTimer(this);
            timer->setProperty("providerId", config.id);
            connect(timer, &QTimer::timeout, this, &MonitorManager::onProviderTimeout);
            timer->setInterval(config.periodMs);
            m_providerTimers[config.id] = timer;

            if (m_isMonitoring) {
                timer->start();
            }
        }
    }

    qDebug() << "[MonitorManager] 注册采集器:" << config.id
             << "-> 通道:" << config.channelName;
    return true;
}

bool MonitorManager::unregisterProvider(const QString& providerId)
{
    QWriteLocker locker(&m_providerLock);

    if (!m_providers.contains(providerId)) {
        return false;
    }

    if (m_providerTimers.contains(providerId)) {
        m_providerTimers[providerId]->stop();
        m_providerTimers[providerId]->deleteLater();
        m_providerTimers.remove(providerId);
    }

    m_providers.remove(providerId);

    qDebug() << "[MonitorManager] 注销采集器:" << providerId;
    return true;
}

bool MonitorManager::hasProvider(const QString& providerId) const
{
    QReadLocker locker(&m_providerLock);
    return m_providers.contains(providerId);
}

QStringList MonitorManager::providerIds() const
{
    QReadLocker locker(&m_providerLock);
    return m_providers.keys();
}
void MonitorManager::startMonitoring()
{
    if (m_isMonitoring) {
        return;
    }

    m_isMonitoring = true;

    {
        QReadLocker locker(&m_providerLock);
        for (QTimer* timer : m_providerTimers) {
            timer->start();
        }
    }

    for (const QString& pointId : std::as_const(m_backendPointIds)) {
        m_backendPointNextDueMs.insert(pointId, 0);
    }

    // 启动后端轮询定时器（如果已配置）
    if (m_backend && m_backendPollTimer->interval() > 0 && !m_backendPointIds.isEmpty()) {
        m_backendPollTimer->start();
    }

    m_cleanupTimer->start();

    qDebug() << "[MonitorManager] 监控已启动";
}

void MonitorManager::stopMonitoring()
{
    const bool wasMonitoring = m_isMonitoring.load(std::memory_order_acquire);
    m_isMonitoring = false;

    {
        QReadLocker locker(&m_providerLock);
        for (QTimer* timer : m_providerTimers) {
            timer->stop();
        }
    }

    m_backendPollTimer->stop();
    m_backendPointNextDueMs.clear();

    m_cleanupTimer->stop();

    // 停止时 flush 一次，避免丢数据
    flushDatabaseLogging();

    if (wasMonitoring) {
        qDebug() << "[MonitorManager] 监控已停止";
    }
}
void MonitorManager::onBackendPollTimeout()
{
    if (!m_isMonitoring.load(std::memory_order_acquire)
            || !m_backend
            || m_backendPointIds.isEmpty())
        return;

    const qint64 nowMs = m_backendPollClock.isValid() ? m_backendPollClock.elapsed() : 0;
    QStringList duePointIds;
    duePointIds.reserve(m_backendPointIds.size());
    for (const QString& pointId : std::as_const(m_backendPointIds)) {
        const int periodMs = qMax(1, m_backendPointPeriodsMs.value(pointId,
                                                                    m_backendPollTimer->interval()));
        const qint64 nextDueMs = m_backendPointNextDueMs.value(pointId, 0);
        if (nextDueMs <= nowMs) {
            duePointIds.append(pointId);
            m_backendPointNextDueMs.insert(pointId, nowMs + periodMs);
        }
    }
    if (duePointIds.isEmpty()) {
        return;
    }

    QHash<QString, QVariant> values;
    QHash<QString, CommError> pointErrors;
    QString errorMsg;
    const bool ok = m_backend->readPoints(duePointIds, values, &errorMsg, &pointErrors);

    const BackendStatusSnapshot status = m_backend->statusSnapshot();
    const bool backendOnline = status.online;
    const CommError backendError(CommProtocolType::Custom,
                                 status.lastErrorCode,
                                 status.lastErrorMessage,
                                 status.lastErrorDetails);
    RuntimePointQuality baseQuality = qualityFromBackendError(backendError, backendOnline);
    if (status.backendType == QStringLiteral("virtual") && baseQuality == RuntimePointQuality::Good) {
        baseQuality = RuntimePointQuality::Simulated;
    }
    if (status.partialSuccess) {
        baseQuality = RuntimePointQuality::Stale;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    QSet<QString> emittedPoints;
    QSet<QString> duePointSet;
    for (const QString& pointId : duePointIds) {
        duePointSet.insert(pointId);
    }

    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (!duePointSet.contains(it.key())) {
            continue;
        }
        const QString channelName = m_pointIdToChannel.value(it.key());
        if (channelName.isEmpty())
            continue;

        Sample sample;
        sample.channelName = channelName;
        bool valueOk = false;
        sample.value = it.value().toDouble(&valueOk);
        sample.valueValid = valueOk;
        sample.quality = valueOk ? baseQuality : RuntimePointQuality::Bad;
        sample.timestamp = now;
        sample.metadata[QStringLiteral("quality")] = qualityToString(sample.quality);
        sample.metadata[QStringLiteral("valueValid")] = sample.valueValid;
        sample.metadata[QStringLiteral("source")] = QStringLiteral("backend_poll");
        attachBackendStatusMetadata(sample, status);

        recordSample(sample);
        emittedPoints.insert(it.key());
    }

    for (const auto& pointId : duePointIds) {
        if (emittedPoints.contains(pointId))
            continue;

        const QString channelName = m_pointIdToChannel.value(pointId);
        if (channelName.isEmpty())
            continue;

        const CommError pointError = pointErrors.value(pointId);
        const RuntimePointQuality quality = pointError.isError()
                                                ? qualityFromBackendError(pointError, backendOnline)
                                                : (!backendOnline
                                                       ? RuntimePointQuality::Offline
                                                       : (ok ? RuntimePointQuality::Stale
                                                             : RuntimePointQuality::Bad));

        Sample sample;
        sample.channelName = channelName;
        sample.valueValid = false;
        sample.quality = quality;
        sample.timestamp = now;
        sample.metadata[QStringLiteral("quality")] = qualityToString(quality);
        sample.metadata[QStringLiteral("valueValid")] = false;
        sample.metadata[QStringLiteral("source")] = QStringLiteral("backend_poll");
        sample.metadata[QStringLiteral("errorCode")] = static_cast<int>(pointError.code);
        sample.metadata[QStringLiteral("errorCodeName")] = commErrorCodeToString(pointError.code);
        sample.metadata[QStringLiteral("error")] = pointError.message.isEmpty() ? errorMsg : pointError.message;
        if (!pointError.details.isEmpty()) {
            sample.metadata[QStringLiteral("errorDetails")] = pointError.details;
        }
        attachBackendStatusMetadata(sample, status);
        recordSample(sample);
    }
}
IDeviceBackend* MonitorManager::deviceBackend() const
{
    return m_backend.data();
}
void MonitorManager::onProviderTimeout()
{
    if (!m_isMonitoring.load(std::memory_order_acquire)) {
        return;
    }

    QTimer* timer = qobject_cast<QTimer*>(sender());
    if (!timer) {
        return;
    }

    QString providerId = timer->property("providerId").toString();

    ProviderConfig config;
    {
        QReadLocker locker(&m_providerLock);
        if (!m_providers.contains(providerId)) {
            return;
        }
        config = m_providers[providerId];
    }

    if (!config.sampler) {
        return;
    }

    try {
        double value = config.sampler();
        recordSample(config.channelName, value, config.unit);
    } catch (const std::exception& e) {
        qWarning() << "[MonitorManager] 采集器异常:" << providerId << e.what();
        if (config.errorHandler) {
            config.errorHandler(QString::fromStdString(e.what()));
        }
    }
}

} // namespace Monitor
