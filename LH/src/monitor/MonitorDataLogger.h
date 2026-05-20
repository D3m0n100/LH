// 文件：src/monitor/MonitorDataLogger.h
// 监控采样落库日志器（最小闭环版本）
// - 负责将 MonitorManager 采样数据缓冲并批量写入 DataManager
// - 支持开关关闭落库（高频实时测试场景）
// - 线程安全：外部可在任意线程 enqueue，内部在对象线程中 flush

#ifndef MONITOR_DATA_LOGGER_H
#define MONITOR_DATA_LOGGER_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QVariantMap>
#include <QList>
#include <QThread>

#include "MonitorTypes.h"

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

    explicit MonitorDataLogger(QObject* parent = nullptr);
    ~MonitorDataLogger() override;

    // ===== 开关 =====
    void setEnabled(bool enabled);
    bool isEnabled() const;

    // ===== 参数 =====
    void setFlushIntervalMs(int intervalMs);
    int flushIntervalMs() const;

    void setBatchSize(int batchSize);
    int batchSize() const;

    // ===== 入队（线程安全） =====
    void enqueueSample(const Sample& sample);
    void enqueueSamples(const QList<Sample>& samples);

    // ===== flush（线程安全） =====
    void flush();

    /**
     * @brief shutdown
     * 停止定时器并确保 flush 完成（可在 aboutToQuit / stopMonitoring 调用）
     */
    void shutdown();

private slots:
    void onFlushTimer();

private:
    void enqueueSampleInThread(const Sample& sample);
    void enqueueSamplesInThread(const QList<Sample>& samples);

    QVariantMap toRuntimeRecord(const Sample& sample) const;
    void flushInThread();

private:
    mutable QMutex m_mutex;
    QList<QVariantMap> m_buffer;

    QTimer* m_flushTimer = nullptr;

    bool m_enabled = true;
    int m_flushIntervalMs = DEFAULT_FLUSH_INTERVAL_MS;
    int m_batchSize = DEFAULT_BATCH_SIZE;
    bool m_flushing = false;
};

} // namespace Monitor

#endif // MONITOR_DATA_LOGGER_H
