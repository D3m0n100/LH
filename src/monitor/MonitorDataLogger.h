// 文件：src/monitor/MonitorDataLogger.h
// 监控采样落库日志器（最小闭环版本）
// - 负责将 MonitorManager 采样数据缓冲并批量写入 DataManager
// - 支持开关关闭落库（高频实时测试场景）
// - 线程安全：外部可在任意线程 enqueue；数据库写入在对象线程中 flush

#ifndef MONITOR_DATA_LOGGER_H
#define MONITOR_DATA_LOGGER_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QVariantMap>
#include <QList>
#include <QThread>

#include "MonitorTypes.h"

namespace Core {
class AsyncDatabaseWorker;
}

namespace Monitor {

/**
 * @brief MonitorDataLogger
 *
 * 设计目标：
 * - recordSample/recordSamples 可在任意线程调用
 * - 数据库写入统一在 logger 所在线程（通常是主线程）执行，避免跨线程使用 QSqlDatabase
 * - 使用缓存队列 + 定时 flush + batch size 触发 flush
 */
class MonitorDataLogger : public QObject
{
    Q_OBJECT

public:
    // ===== 默认参数（可按需调整） =====
    static constexpr int DEFAULT_FLUSH_INTERVAL_MS = 500;
    static constexpr int DEFAULT_BATCH_SIZE = 200;
    static constexpr int DEFAULT_MAX_BUFFER_SIZE = 50000; // 防止极端情况下内存无限增长
    static constexpr int MAX_RETRY_DELAY_MS = 30000;

    explicit MonitorDataLogger(QObject* parent = nullptr);
    ~MonitorDataLogger() override;

    // ===== 异步 Worker 关联 =====
    void setAsyncWorker(Core::AsyncDatabaseWorker* worker);
    Core::AsyncDatabaseWorker* asyncWorker() const;

    // ===== 开关（仅对象所属线程） =====
    // 跨线程调用会立即返回并设置 lastError，不修改状态或访问数据库。
    void setEnabled(bool enabled);
    bool isEnabled() const;

    // ===== 参数 =====
    void setFlushIntervalMs(int intervalMs);
    int flushIntervalMs() const;

    void setBatchSize(int batchSize);
    int batchSize() const;

    // ===== 状态 =====
    quint64 droppedSampleCount() const;
    int retryDelayMs() const;
    QString lastError() const;

    // ===== 入队（线程安全） =====
    void enqueueSample(const Sample& sample);
    void enqueueSamples(const QList<Sample>& samples);

    // ===== flush（仅对象所属线程） =====
    // flush/shutdown 跨线程调用会立即返回并设置 lastError；数据库写入不会跨线程转发。
    void flush();

    /**
     * @brief shutdown
     * 停止定时器并确保 flush 完成（可在 aboutToQuit / stopMonitoring 调用）。
     * 必须从对象所属线程调用；跨线程调用立即返回并设置 lastError。
     */
    void shutdown();

signals:
    void samplesCommitted(int count);
    void samplesDropped(int count);
    void flushFailed(const QString& error);

private slots:
    void onFlushTimer();

private:
    void enqueueSampleInThread(const Sample& sample);
    void enqueueSamplesInThread(const QList<Sample>& samples);

    QVariantMap toRuntimeRecord(const Sample& sample) const;
    void flushInThread();
    void restoreFailedBatch(QList<QVariantMap>& batch);
    void recordFlushFailure(const QString& error, int batchSize);
    void recordFlushSuccess(int batchSize);
    void recordOperationError(const QString& error);
    void shutdownInThread();

private:
    mutable QMutex m_mutex;
    QList<QVariantMap> m_buffer;

    QTimer* m_flushTimer = nullptr;

    bool m_enabled = true;
    int m_flushIntervalMs = DEFAULT_FLUSH_INTERVAL_MS;
    int m_batchSize = DEFAULT_BATCH_SIZE;
    bool m_flushing = false;
    bool m_flushFailureActive = false;
    bool m_retryDelayCapReported = false;
    int m_retryDelayMs = DEFAULT_FLUSH_INTERVAL_MS;
    int m_consecutiveFlushFailures = 0;
    quint64 m_droppedSamples = 0;
    QString m_lastFlushError;
    QString m_lastError;
    QHash<quint64, int> m_pendingCommits;
    Core::AsyncDatabaseWorker* m_asyncWorker = nullptr;
};

} // namespace Monitor

#endif // MONITOR_DATA_LOGGER_H
