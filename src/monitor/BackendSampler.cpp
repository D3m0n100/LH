#include "BackendSampler.h"
#include "communication/RuntimePointQualityMapper.h"
#include <QSet>
#include <QDebug>
#include <cmath>
#include <utility>
namespace Monitor {
BackendSampler::BackendSampler(std::function<void(const Sample&)> sink, QObject* parent)
    : QObject(parent), m_sink(std::move(sink)), m_backendPollTimer(new QTimer(this)) {
    m_backendPollClock.start();
    connect(m_backendPollTimer, &QTimer::timeout, this, &BackendSampler::poll);
}
BackendSampler::~BackendSampler() { stop(); disconnectBackendSignals(); }
void BackendSampler::configure(QStringList ids, QHash<QString, QString> channels,
    QHash<QString, int> periods, int intervalMs) {
    stop();
    m_backendPointIds = std::move(ids);
    m_pointIdToChannel = std::move(channels);
    m_backendPointPeriodsMs = std::move(periods);
    m_backendPollTimer->setInterval(qMax(1, intervalMs));
}
void BackendSampler::start() {
    m_active = true;
    for (const auto& id : m_backendPointIds) m_backendPointNextDueMs.insert(id, 0);
    if (m_backend && !m_backendPointIds.isEmpty()) m_backendPollTimer->start();
}
void BackendSampler::stop() {
    m_active = false;
    ++m_backendPollGeneration;
    if (m_backendPollCancelled) m_backendPollCancelled->store(true);
    m_backendPollPending = false;
    m_backendPollTimer->stop();
    m_backendPointNextDueMs.clear();
}
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

static bool extractFiniteDouble(const QVariant& var, double* outValue)
{
    if (!var.isValid() || var.isNull()) {
        return false;
    }
    bool ok = false;
    const double val = var.toDouble(&ok);
    if (!ok || !std::isfinite(val)) {
        return false;
    }
    if (outValue) {
        *outValue = val;
    }
    return true;
}

static RuntimePointQuality determineBaseQuality(const BackendStatusSnapshot& status)
{
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
    return baseQuality;
}

static Sample createPointSample(const QString& channelName,
                                const QVariant& rawValue,
                                const BackendStatusSnapshot& status,
                                const QString& source,
                                const QDateTime& timestamp)
{
    Sample sample;
    sample.channelName = channelName;
    sample.timestamp = timestamp;

    double numericValue = 0.0;
    const bool valueOk = extractFiniteDouble(rawValue, &numericValue);
    sample.value = numericValue;
    sample.valueValid = valueOk;

    const RuntimePointQuality baseQuality = determineBaseQuality(status);
    sample.quality = valueOk ? baseQuality : RuntimePointQuality::Bad;

    sample.metadata[QStringLiteral("quality")] = qualityToString(sample.quality);
    sample.metadata[QStringLiteral("valueValid")] = sample.valueValid;
    sample.metadata[QStringLiteral("source")] = source;
    attachBackendStatusMetadata(sample, status);

    return sample;
}

} // namespace

void BackendSampler::disconnectBackendSignals()
{
    for (auto* connection : {&m_backendPointsChangedConnection, &m_backendConnectionStateConnection,
                             &m_backendDestroyedConnection}) {
        disconnect(*connection);
        *connection = {};
    }
}

void BackendSampler::setDeviceBackend(IDeviceBackend* backend)
{
    if (m_backend == backend)
        return;

    disconnectBackendSignals();

    ++m_backendPollGeneration;
    if (m_backendPollCancelled) m_backendPollCancelled->store(true);
    m_backendPollPending = false;
    m_backend = backend;
    m_backendPollTimer->stop();
    m_backendPointIds.clear();
    m_pointIdToChannel.clear();
    m_backendPointPeriodsMs.clear();
    m_backendPointNextDueMs.clear();

    if (backend) {
        m_backendDestroyedConnection = connect(
            backend, &QObject::destroyed, this, [this]() {
                ++m_backendPollGeneration;
                if (m_backendPollCancelled) m_backendPollCancelled->store(true);
                m_backendPollPending = false;
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
                            if (!m_active
                                    || !sourceBackend
                                    || m_backend.data() != sourceBackend.data()) {
                                return;
                            }
                            const BackendStatusSnapshot status = sourceBackend->statusSnapshot();
                            const QDateTime now = QDateTime::currentDateTimeUtc();
                            for (auto it = updates.constBegin(); it != updates.constEnd(); ++it) {
                                const QString channelName = m_pointIdToChannel.value(it.key());
                                if (channelName.isEmpty())
                                    continue;
                                Sample sample = createPointSample(channelName, it.value(), status,
                                                                 QStringLiteral("backend_push"), now);
                                m_sink(sample);
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
void BackendSampler::poll()
{
    if (!m_active
            || !m_backend
            || m_backendPointIds.isEmpty()
            || m_backendPollPending)
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

    const auto generation = m_backendPollGeneration;
    const QPointer<IDeviceBackend> backend = m_backend;
    QElapsedTimer pollTimer;
    pollTimer.start();
    m_backendPollPending = true;
    auto completed = [this, generation, backend, duePointIds, pollTimer](
        bool ok, QHash<QString, QVariant> values, QString errorMsg,
        QHash<QString, CommError> pointErrors, BackendStatusSnapshot status) {
        Q_UNUSED(errorMsg);
        if (generation != m_backendPollGeneration || backend != m_backend
                || !m_active || !backend) return;
        m_backendPollPending = false;
        const qint64 elapsedMs = pollTimer.elapsed();
        const int pollInterval = m_backendPollTimer ? m_backendPollTimer->interval() : 100;
        if (elapsedMs > pollInterval) {
            qWarning() << QStringLiteral("[MonitorManager] 监控轮询耗时超限: 耗时 %1 ms > 周期 %2 ms (到期点位=%3, 成功=%4, 错误=%5)")
                          .arg(elapsedMs)
                          .arg(pollInterval)
                          .arg(duePointIds.size())
                          .arg(values.size())
                          .arg(pointErrors.size());
        }

        const bool backendOnline = status.online;

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

            Sample sample = createPointSample(channelName, it.value(), status,
                                             QStringLiteral("backend_poll"), now);
            sample.metadata[QStringLiteral("pollElapsedMs")] = elapsedMs;
            sample.metadata[QStringLiteral("duePointCount")] = duePointIds.size();
            if (elapsedMs > pollInterval) {
                sample.metadata[QStringLiteral("pollOverrun")] = true;
            }

            m_sink(sample);
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
            sample.metadata[QStringLiteral("pollElapsedMs")] = elapsedMs;
            sample.metadata[QStringLiteral("duePointCount")] = duePointIds.size();
            if (elapsedMs > pollInterval) {
                sample.metadata[QStringLiteral("pollOverrun")] = true;
            }
            sample.metadata[QStringLiteral("errorCode")] = static_cast<int>(pointError.code);
            sample.metadata[QStringLiteral("errorCodeName")] = commErrorCodeToString(pointError.code);
            sample.metadata[QStringLiteral("error")] = pointError.message.isEmpty() ? errorMsg : pointError.message;
            if (!pointError.details.isEmpty()) {
                sample.metadata[QStringLiteral("errorDetails")] = pointError.details;
            }
            attachBackendStatusMetadata(sample, status);
            m_sink(sample);
        }
    };
    if (backend->supportsAsyncRead()) {
        m_backendPollCancelled = std::make_shared<std::atomic_bool>(false);
        // Sampling cadence is not a transport deadline: allow one second for
        // status plus all register batches, while keeping only one request active.
        backend->readPointsAsync(duePointIds, 1000,
                                 m_backendPollCancelled, this, std::move(completed));
    } else {
        // Compatibility backends explicitly keep their synchronous contract.
        QHash<QString, QVariant> values;
        QHash<QString, CommError> errors;
        QString message;
        const bool ok = backend->readPoints(duePointIds, values, &message, &errors);
        completed(ok, values, message, errors, backend->statusSnapshot());
    }
}

}
