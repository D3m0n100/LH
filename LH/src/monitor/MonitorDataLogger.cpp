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
    // 允许跨线程调用
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, enabled]() {
            setEnabled(enabled);
        }, Qt::QueuedConnection);
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_enabled = enabled;
    }

    // 关闭落库时仍可保留定时器，以便后续重新开启
    // 但若希望更省资源，也可以在此 stop；此处保持简单。
    if (!m_enabled) {
        // 可选：关闭时也 flush 一次，避免丢数据
        flushInThread();
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

    m_flushIntervalMs = intervalMs;
    if (m_flushTimer) {
        m_flushTimer->setInterval(m_flushIntervalMs);
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

    m_batchSize = batchSize;
}

int MonitorDataLogger::batchSize() const
{
    QMutexLocker locker(&m_mutex);
    return m_batchSize;
}

void MonitorDataLogger::enqueueSample(const Sample& sample)
{
    if (!m_enabled) {
        return;
    }

    if (QThread::currentThread() != thread()) {
        const Sample copy = sample;
        QMetaObject::invokeMethod(this, [this, copy]() {
            enqueueSampleInThread(copy);
        }, Qt::QueuedConnection);
        return;
    }

    enqueueSampleInThread(sample);
}

void MonitorDataLogger::enqueueSamples(const QList<Sample>& samples)
{
    if (samples.isEmpty() || !m_enabled) {
        return;
    }

    if (QThread::currentThread() != thread()) {
        const QList<Sample> copy = samples;
        QMetaObject::invokeMethod(this, [this, copy]() {
            enqueueSamplesInThread(copy);
        }, Qt::QueuedConnection);
        return;
    }

    enqueueSamplesInThread(samples);
}

void MonitorDataLogger::flush()
{
    if (!m_enabled) {
        return;
    }

    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() {
            flushInThread();
        }, Qt::QueuedConnection);
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

    if (m_flushTimer) {
        m_flushTimer->stop();
    }

    flushInThread();
}

void MonitorDataLogger::onFlushTimer()
{
    if (!m_enabled) {
        return;
    }
    flushInThread();
}

void MonitorDataLogger::enqueueSampleInThread(const Sample& sample)
{
    // logger 所在线程调用
    if (!m_enabled) {
        return;
    }

    QMutexLocker locker(&m_mutex);

    // 避免极端情况下缓冲无限增大
    if (m_buffer.size() >= DEFAULT_MAX_BUFFER_SIZE) {
        // 丢弃最旧的数据（保留最新）
        const int dropCount = std::min(m_buffer.size(), DEFAULT_BATCH_SIZE);
        for (int i = 0; i < dropCount; ++i) {
            m_buffer.removeFirst();
        }
        qWarning() << "[MonitorDataLogger] buffer overflow, drop" << dropCount
                   << "records, buffer=" << m_buffer.size();
    }

    m_buffer.push_back(toRuntimeRecord(sample));

    if (m_buffer.size() >= m_batchSize) {
        // 达到阈值立即 flush（仍在同线程）
        locker.unlock();
        flushInThread();
    }
}

void MonitorDataLogger::enqueueSamplesInThread(const QList<Sample>& samples)
{
    if (!m_enabled) {
        return;
    }

    QMutexLocker locker(&m_mutex);

    // 预先裁剪，避免超限
    if (m_buffer.size() + samples.size() > DEFAULT_MAX_BUFFER_SIZE) {
        const int overflow = (m_buffer.size() + samples.size()) - DEFAULT_MAX_BUFFER_SIZE;
        const int dropCount = std::max(overflow, DEFAULT_BATCH_SIZE);
        const int realDrop = std::min(dropCount, m_buffer.size());
        for (int i = 0; i < realDrop; ++i) {
            m_buffer.removeFirst();
        }
        qWarning() << "[MonitorDataLogger] buffer overflow, drop" << realDrop
                   << "records, buffer=" << m_buffer.size();
    }

    for (const auto& s : samples) {
        m_buffer.push_back(toRuntimeRecord(s));
    }

    if (m_buffer.size() >= m_batchSize) {
        locker.unlock();
        flushInThread();
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

    // 为历史查询/回放保留真实采样时间
    // 统一使用 UTC，避免跨时区/夏令时问题
    record["timestamp"] = sample.timestamp.isValid() ? sample.timestamp.toUTC()
                                                      : QDateTime::currentDateTimeUtc();

    return record;
}

void MonitorDataLogger::flushInThread()
{
    if (m_flushing) {
        return; // 防止重入
    }

    QList<QVariantMap> batch;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_enabled || m_buffer.isEmpty()) {
            return;
        }
        m_flushing = true;
        batch.swap(m_buffer);
    }

    // 注意：避免跨线程使用 QSqlDatabase
    // DataManager 初始化通常在主线程，因此 logger 默认也在主线程。
    auto& dm = DataManager::instance();
    if (!dm.isInitialized()) {
        qWarning() << "[MonitorDataLogger] DataManager not initialized, drop batch=" << batch.size();
        QMutexLocker locker(&m_mutex);
        m_flushing = false;
        return;
    }

    QueryResult r = dm.logRuntimeDataBatch(batch);
    if (!r.success) {
        qWarning() << "[MonitorDataLogger] logRuntimeDataBatch failed:" << r.fullError()
                   << "batch=" << batch.size();
    }

    QMutexLocker locker(&m_mutex);
    m_flushing = false;
}

} // namespace Monitor
