// 文件：src/monitor/MonitorDataLogger.cpp
// 监控采样落库日志器实现

#include "MonitorDataLogger.h"

#include "core/DataManager.h"

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

void MonitorDataLogger::setEnabled(bool enabled)
{
    {
        QMutexLocker locker(&m_mutex);
        m_enabled = enabled;
    }

    // enabled 只控制新样本接收；已接受的样本必须在禁用前排空。
    if (!enabled) {
        flush();
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

    {
        QMutexLocker locker(&m_mutex);
        m_flushIntervalMs = intervalMs;
    }
    if (m_flushTimer) {
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
        QMetaObject::invokeMethod(this, [this]() {
            flushInThread();
        }, Qt::BlockingQueuedConnection);
        return;
    }

    flushInThread();
}

void MonitorDataLogger::shutdown()
{
    // 关闭时希望尽量同步完成 flush，避免退出丢数据
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() {
            shutdown();
        }, Qt::BlockingQueuedConnection);
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
    }

    m_buffer.push_back(toRuntimeRecord(sample));

    shouldFlush = m_buffer.size() >= m_batchSize;
    locker.unlock();
    if (dropped > 0) emit samplesDropped(dropped);
    if (shouldFlush) flush();
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
    }

    const int skipIncoming = qMax(0, overflow - fromBuffer);
    for (int i = skipIncoming; i < samples.size(); ++i) {
        const auto& s = samples.at(i);
        m_buffer.push_back(toRuntimeRecord(s));
    }

    shouldFlush = m_buffer.size() >= m_batchSize;
    locker.unlock();
    if (dropped > 0) emit samplesDropped(dropped);
    if (shouldFlush) flush();
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
        m_flushing = false;
    }
    if (dropped > 0) emit samplesDropped(dropped);
}

void MonitorDataLogger::flushInThread()
{
    if (m_flushing) {
        return; // 防止重入
    }

    QList<QVariantMap> batch;
    {
        QMutexLocker locker(&m_mutex);
        if (m_buffer.isEmpty()) {
            return;
        }
        m_flushing = true;
        batch.swap(m_buffer);
    }

    // 注意：避免跨线程使用 QSqlDatabase
    // DataManager 初始化通常在主线程，因此 logger 默认也在主线程。
    auto& dm = DataManager::instance();
    if (!dm.isInitialized()) {
        qWarning() << "[MonitorDataLogger] DataManager not initialized, retain batch=" << batch.size();
        restoreFailedBatch(batch);
        return;
    }

    QueryResult r = dm.logRuntimeDataBatch(batch);
    if (!r.success) {
        qWarning() << "[MonitorDataLogger] logRuntimeDataBatch failed:" << r.fullError()
                   << "batch=" << batch.size();
        restoreFailedBatch(batch);
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_flushing = false;
}

} // namespace Monitor
