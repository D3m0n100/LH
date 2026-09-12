// 文件：src/monitor/MonitorDataLogger.cpp
// 监控采样落库日志器实现

#include "MonitorDataLogger.h"

#include "core/DataManager.h"
#include "core/AsyncDatabaseWorker.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QMetaObject>
#include <QDebug>

#include <algorithm>

namespace Monitor {

MonitorDataLogger::MonitorDataLogger(QObject* parent)
    : QObject(parent)
    , m_flushTimer(new QTimer(this))
{
    m_flushTimer->setInterval(m_flushIntervalMs);
    connect(m_flushTimer, &QTimer::timeout, this, &MonitorDataLogger::onFlushTimer);
    m_flushTimer->start();
}

MonitorDataLogger::~MonitorDataLogger()
{
    // 析构时尽力 flush（不要依赖后台定时器）
    shutdown();
}

void MonitorDataLogger::setAsyncWorker(Core::AsyncDatabaseWorker* worker)
{
    if (m_asyncWorker == worker) return;
    if (m_asyncWorker) disconnect(m_asyncWorker, nullptr, this, nullptr);
    { QMutexLocker lock(&m_mutex); m_asyncWorker = worker; }
    if (!worker) return;
    connect(worker, &Core::AsyncDatabaseWorker::batchCommitted, this,
            [this](quint64 id, int count) {
        if (!m_pendingCommits.contains(id)) return;
        const int accepted = qMin(count, m_pendingCommits.value(id));
        m_pendingCommits[id] -= accepted;
        if (m_pendingCommits.value(id) == 0) m_pendingCommits.remove(id);
        recordFlushSuccess(accepted);
        emit samplesCommitted(accepted);
    }, Qt::QueuedConnection);
    connect(worker, &Core::AsyncDatabaseWorker::batchAbandoned, this,
            [this](quint64 id, int count, const QString& error) {
        if (!m_pendingCommits.contains(id)) return;
        const int lost = qMin(count, m_pendingCommits.value(id));
        m_pendingCommits[id] -= lost;
        if (m_pendingCommits.value(id) == 0) m_pendingCommits.remove(id);
        { QMutexLocker lock(&m_mutex); m_droppedSamples += lost; }
        recordFlushFailure(error, lost);
        emit samplesDropped(lost);
    }, Qt::QueuedConnection);
}

Core::AsyncDatabaseWorker* MonitorDataLogger::asyncWorker() const
{
    QMutexLocker locker(&m_mutex);
    return m_asyncWorker;
}

void MonitorDataLogger::setEnabled(bool enabled)
{
    if (QThread::currentThread() != thread()) {
        recordOperationError(QStringLiteral("setEnabled must be called from logger owner thread"));
        return;
    }

    if (enabled) {
        {
            QMutexLocker locker(&m_mutex);
            m_enabled = true;
        }
        if (m_flushTimer && !m_flushTimer->isActive()) {
            m_flushTimer->start();
        }
        return;
    }

    // 先关闭 admission，避免 flush 期间并发入队形成尾部竞态。
    {
        QMutexLocker locker(&m_mutex);
        m_enabled = false;
    }

    // flushInThread 不依赖 enabled，仍会处理已经接受的样本。
    flushInThread();

    bool flushed = false;
    {
        QMutexLocker locker(&m_mutex);
        flushed = m_buffer.isEmpty();
        if (!flushed) {
            // 失败时恢复 admission，让退避 timer 继续重试并接收后续样本。
            m_enabled = true;
        }
    }
    if (flushed && m_flushTimer) {
        m_flushTimer->stop();
    } else if (!flushed && m_flushTimer && !m_flushTimer->isActive()) {
        m_flushTimer->start();
    }
}

bool MonitorDataLogger::isEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_enabled;
}

void MonitorDataLogger::setFlushIntervalMs(int intervalMs)
{
    if (intervalMs <= 0) {
        intervalMs = DEFAULT_FLUSH_INTERVAL_MS;
    }

    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, intervalMs]() {
            setFlushIntervalMs(intervalMs);
        }, Qt::QueuedConnection);
        return;
    }

    bool failureActive = false;
    {
        QMutexLocker locker(&m_mutex);
        m_flushIntervalMs = intervalMs;
        failureActive = m_flushFailureActive;
        if (!failureActive) {
            m_retryDelayMs = intervalMs;
        }
    }
    if (m_flushTimer && !failureActive) {
        m_flushTimer->setInterval(intervalMs);
    }
}

int MonitorDataLogger::flushIntervalMs() const
{
    QMutexLocker locker(&m_mutex);
    return m_flushIntervalMs;
}

void MonitorDataLogger::setBatchSize(int batchSize)
{
    if (batchSize <= 0) {
        batchSize = DEFAULT_BATCH_SIZE;
    }

    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, batchSize]() {
            setBatchSize(batchSize);
        }, Qt::QueuedConnection);
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_batchSize = batchSize;
}

int MonitorDataLogger::batchSize() const
{
    QMutexLocker locker(&m_mutex);
    return m_batchSize;
}

quint64 MonitorDataLogger::droppedSampleCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_droppedSamples;
}

int MonitorDataLogger::retryDelayMs() const
{
    QMutexLocker locker(&m_mutex);
    return m_retryDelayMs;
}

QString MonitorDataLogger::lastError() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

void MonitorDataLogger::enqueueSample(const Sample& sample)
{
    enqueueSampleInThread(sample);
}

void MonitorDataLogger::enqueueSamples(const QList<Sample>& samples)
{
    if (samples.isEmpty()) {
        return;
    }

    enqueueSamplesInThread(samples);
}

void MonitorDataLogger::flush()
{
    if (QThread::currentThread() != thread()) {
        recordOperationError(QStringLiteral("flush must be called from logger owner thread"));
        return;
    }

    flushInThread();
}

void MonitorDataLogger::shutdown()
{
    // 关闭时希望尽量同步完成 flush，避免退出丢数据
    if (QThread::currentThread() != thread()) {
        recordOperationError(QStringLiteral("shutdown must be called from logger owner thread"));
        return;
    }

    shutdownInThread();
}

void MonitorDataLogger::shutdownInThread()
{
    if (QThread::currentThread() != thread()) {
        recordOperationError(QStringLiteral("logger shutdown executed on the wrong thread"));
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_enabled = false;
    }

    if (m_flushTimer) {
        m_flushTimer->stop();
    }

    flushInThread();

    int discarded = 0;
    QString shutdownError;
    {
        QMutexLocker locker(&m_mutex);
        discarded = m_buffer.size();
        if (discarded > 0) {
            const QString flushError = m_lastError;
            m_buffer.clear();
            m_droppedSamples += static_cast<quint64>(discarded);
            shutdownError = QStringLiteral("shutdown discarded %1 buffered samples: %2")
                                .arg(discarded)
                                .arg(flushError.isEmpty() ? QStringLiteral("flush failed") : flushError);
            m_lastError = shutdownError;
            m_flushFailureActive = true;
        }
    }
    if (discarded > 0) {
        qCritical() << "[MonitorDataLogger]" << shutdownError;
        emit samplesDropped(discarded);
        emit flushFailed(shutdownError);
    }
}

void MonitorDataLogger::onFlushTimer()
{
    if (!isEnabled()) {
        return;
    }
    flushInThread();
}

void MonitorDataLogger::enqueueSampleInThread(const Sample& sample)
{
    int dropped = 0;
    bool shouldFlush = false;
    QMutexLocker locker(&m_mutex);
    if (!m_enabled) {
        return;
    }

    // 避免极端情况下缓冲无限增大
    if (m_buffer.size() >= DEFAULT_MAX_BUFFER_SIZE) {
        // 丢弃最旧的数据（保留最新）
        const int dropCount = 1;
        for (int i = 0; i < dropCount; ++i) {
            m_buffer.removeFirst();
        }
        dropped = dropCount;
        m_droppedSamples += static_cast<quint64>(dropCount);
    }

    m_buffer.push_back(toRuntimeRecord(sample));

    shouldFlush = m_buffer.size() >= m_batchSize && !m_flushFailureActive;
    locker.unlock();
    if (dropped > 0) emit samplesDropped(dropped);
    if (shouldFlush && QThread::currentThread() == thread()) {
        flush();
    }
}

void MonitorDataLogger::enqueueSamplesInThread(const QList<Sample>& samples)
{
    int dropped = 0;
    bool shouldFlush = false;
    int fromBuffer = 0;
    QMutexLocker locker(&m_mutex);
    if (!m_enabled) {
        return;
    }

    // 预先裁剪，避免超限
    const int overflow = qMax(0, m_buffer.size() + samples.size() - DEFAULT_MAX_BUFFER_SIZE);
    if (overflow > 0) {
        fromBuffer = qMin(overflow, m_buffer.size());
        for (int i = 0; i < fromBuffer; ++i) {
            m_buffer.removeFirst();
        }
        dropped = overflow;
        m_droppedSamples += static_cast<quint64>(overflow);
    }

    const int skipIncoming = qMax(0, overflow - fromBuffer);
    for (int i = skipIncoming; i < samples.size(); ++i) {
        const auto& s = samples.at(i);
        m_buffer.push_back(toRuntimeRecord(s));
    }

    shouldFlush = m_buffer.size() >= m_batchSize && !m_flushFailureActive;
    locker.unlock();
    if (dropped > 0) emit samplesDropped(dropped);
    if (shouldFlush && QThread::currentThread() == thread()) {
        flush();
    }
}

QVariantMap MonitorDataLogger::toRuntimeRecord(const Sample& sample) const
{
    QVariantMap record;

    // DataManager::logRuntimeDataBatch 约定字段：varName/value/unit
    // - variableName 推荐使用 channelName
    record["varName"] = sample.channelName;
    record["value"] = sample.value;
    record["unit"] = sample.unit;
    record["quality"] = runtimePointQualityToString(sample.quality);
    record["valueValid"] = sample.valueValid;

    // 仅保留稳定的 provenance/error 字段，避免把整张运行时 metadata
    // JSON 带入数据库；DataManager 仍兼容 source/errorCode 等旧键。
    const QVariantMap& metadata = sample.metadata;
    QString origin = metadata.value(QStringLiteral("origin")).toString();
    if (origin.isEmpty()) {
        origin = metadata.value(QStringLiteral("source")).toString();
    }
    record["origin"] = origin;

    QString errorCode = metadata.value(QStringLiteral("errorCodeName")).toString();
    if (errorCode.isEmpty()) {
        errorCode = metadata.value(QStringLiteral("errorCode")).toString();
    }
    record["errorCode"] = errorCode;
    record["errorText"] = metadata.value(QStringLiteral("error")).toString();
    if (metadata.contains(QStringLiteral("errorDetails"))) {
        record["errorDetails"] = metadata.value(QStringLiteral("errorDetails"));
    }

    // 为历史查询/回放保留真实采样时间
    // 统一使用 UTC，避免跨时区/夏令时问题
    record["timestamp"] = sample.timestamp.isValid() ? sample.timestamp.toUTC()
                                                      : QDateTime::currentDateTimeUtc();

    return record;
}

void MonitorDataLogger::restoreFailedBatch(QList<QVariantMap>& batch)
{
    int dropped = 0;
    {
        QMutexLocker locker(&m_mutex);
        batch.append(m_buffer);
        m_buffer.swap(batch);

        dropped = qMax(0, m_buffer.size() - DEFAULT_MAX_BUFFER_SIZE);
        for (int i = 0; i < dropped; ++i) {
            m_buffer.removeFirst();
        }
        m_droppedSamples += static_cast<quint64>(dropped);
        m_flushing = false;
    }
    if (dropped > 0) emit samplesDropped(dropped);
}

void MonitorDataLogger::recordFlushFailure(const QString& error, int batchSize)
{
    bool stateChanged = false;
    bool capReached = false;
    int retryDelay = DEFAULT_FLUSH_INTERVAL_MS;
    {
        QMutexLocker locker(&m_mutex);
        stateChanged = !m_flushFailureActive || m_lastFlushError != error;
        m_flushFailureActive = true;
        m_lastFlushError = error;
        m_lastError = error;
        ++m_consecutiveFlushFailures;

        const int retryCap = qMax(MAX_RETRY_DELAY_MS, m_flushIntervalMs);
        const int shift = qMin(m_consecutiveFlushFailures - 1, 30);
        const qint64 candidate = static_cast<qint64>(m_flushIntervalMs)
            * (qint64(1) << shift);
        retryDelay = static_cast<int>(qMin<qint64>(retryCap, candidate));
        m_retryDelayMs = retryDelay;
        capReached = retryDelay >= retryCap;
        if (capReached && !m_retryDelayCapReported) {
            m_retryDelayCapReported = true;
        } else {
            capReached = false;
        }
    }

    if (m_flushTimer) {
        m_flushTimer->setInterval(retryDelay);
    }
    if (stateChanged || capReached) {
        qWarning() << "[MonitorDataLogger] flush failed; retry in" << retryDelay
                   << "ms:" << error << "batch=" << batchSize;
        emit flushFailed(error);
    }
}

void MonitorDataLogger::recordFlushSuccess(int batchSize)
{
    bool recovered = false;
    int flushInterval = DEFAULT_FLUSH_INTERVAL_MS;
    {
        QMutexLocker locker(&m_mutex);
        recovered = m_flushFailureActive;
        flushInterval = m_flushIntervalMs;
        m_flushFailureActive = false;
        m_retryDelayCapReported = false;
        m_consecutiveFlushFailures = 0;
        m_retryDelayMs = flushInterval;
        m_lastFlushError.clear();
        m_lastError.clear();
        m_flushing = false;
    }

    if (m_flushTimer) {
        m_flushTimer->setInterval(flushInterval);
    }
    if (recovered) {
        qInfo() << "[MonitorDataLogger] flush recovered; batch=" << batchSize;
    }
}

void MonitorDataLogger::recordOperationError(const QString& error)
{
    {
        QMutexLocker locker(&m_mutex);
        m_lastError = error;
    }
    qWarning() << "[MonitorDataLogger]" << error;
}

void MonitorDataLogger::flushInThread()
{
    QList<QVariantMap> batch;
    {
        QMutexLocker locker(&m_mutex);
        if (m_flushing) {
            return; // 防止重入
        }
        if (m_buffer.isEmpty()) {
            return;
        }
        m_flushing = true;
        batch.swap(m_buffer);
    }

    Core::AsyncDatabaseWorker* worker = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        worker = m_asyncWorker;
    }

    if (worker) {
        if (!worker->isDbOpen()) {
            const int failedBatchSize = batch.size();
            restoreFailedBatch(batch);
            recordFlushFailure(QStringLiteral("Async database worker is unavailable"), failedBatchSize);
            return;
        }
        const quint64 batchId = worker->enqueueBatch(batch);
        if (batchId > 0) {
            m_pendingCommits.insert(batchId, batch.size());
            { QMutexLocker lock(&m_mutex); m_flushing = false; }
            return;
        } else {
            const int failedBatchSize = batch.size();
            restoreFailedBatch(batch);
            recordFlushFailure(QStringLiteral("Failed to enqueue batch to async worker"), failedBatchSize);
            return;
        }
    }

    // 注意：避免跨线程使用 QSqlDatabase
    // DataManager 初始化通常在主线程，因此 logger 默认也在主线程。
    auto& dm = DataManager::instance();
    dm.checkThreadOwnership();
    if (!dm.isInitialized()) {
        const int failedBatchSize = batch.size();
        restoreFailedBatch(batch);
        recordFlushFailure(QStringLiteral("DataManager not initialized"), failedBatchSize);
        return;
    }

    QueryResult r = dm.logRuntimeDataBatch(batch);
    if (!r.success) {
        const int failedBatchSize = batch.size();
        restoreFailedBatch(batch);
        recordFlushFailure(r.fullError(), failedBatchSize);
        return;
    }

    recordFlushSuccess(batch.size());
}

} // namespace Monitor
