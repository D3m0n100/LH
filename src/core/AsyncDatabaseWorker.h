#ifndef ASYNC_DATABASE_WORKER_H
#define ASYNC_DATABASE_WORKER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QList>
#include <QVariantMap>
#include <QSqlDatabase>
#include <QDateTime>
#include <atomic>
#include <memory>
#include <functional>
#include <QPointer>
#include <QWaitCondition>

#include "Common.h"
#include "core/DataManager.h"

namespace Core {

/**
 * @brief 异步数据库工作者
 *
 * 在专用工作线程中管理独立的 SQLite 连接，支持 WAL 模式并发读写。
 * 核心能力：
 * 1. 异步批量写入队列（有界，丢弃旧数据保最新，防内存爆炸）
 * 2. 分批短事务过期数据清理（可取消，基于 (timestamp, id) 索引）
 * 3. 独立连接历史查询（非 GUI 线程执行）
 * 4. 有界安全退出与连接清理
 */
class AsyncDatabaseWorker : public QObject
{
    Q_OBJECT

public:
    static constexpr int DEFAULT_MAX_QUEUE_RECORDS = 50000;
    static constexpr int DEFAULT_CHUNK_SIZE = 1000;
    static constexpr int DEFAULT_MAX_RETRY_DELAY_MS = 5000;

    struct BatchItem {
        quint64 batchId = 0;
        QList<QVariantMap> records;
        int retryCount = 0;
    };

    struct CleanupResult {
        int totalDeleted = 0;
        bool success = false;
        bool cancelled = false;
        QString errorText;
    };

    explicit AsyncDatabaseWorker(QObject* parent = nullptr);
    ~AsyncDatabaseWorker() override;

    /// 启动专用工作线程并打开独立数据库连接
    bool startWorker(const QString& dbPath);
    bool startWorkerAsync(const QString& dbPath);

    /// 停止工作线程，最多等待 drainTimeoutMs 毫秒排空剩余写入
    bool stopWorker(int drainTimeoutMs = 2000);
    void deleteWhenStopped();

    bool isRunning() const;
    bool isDbOpen() const;
    QString databasePath() const;

    /// 提交一批运行时记录（线程安全，从任何线程调用）
    quint64 enqueueBatch(const QList<QVariantMap>& records);

    /// 待提交队列中的记录总数
    int pendingRecordCount() const;

    /// 丢弃的记录总数
    quint64 droppedRecordCount() const;

    /// 设置待处理队列上限
    void setMaxQueueRecords(int maxRecords);
    int maxQueueRecords() const;

    /// 请求执行异步分批清理（线程安全）
    void requestCleanup(int retentionDays, int chunkSize = DEFAULT_CHUNK_SIZE);

    /// 取消正在进行中的清理
    void cancelCleanup();

    /// 执行 keyset 历史分页查询（在独立连接上执行）
    // Synchronous SQL primitives below are worker-thread-only; cross-thread calls fail immediately.
    // UI callers must use submitHistoryTask / the history service request API.
    RuntimeHistoryPage queryHistoryPage(const QString& channelName,
                                        const QDateTime& start,
                                        const QDateTime& end,
                                        int pageSize,
                                        const RuntimeHistoryCursor& cursor,
                                        const std::atomic_bool* cancelToken = nullptr);

    /// 执行最近记录 keyset 分页查询
    RuntimeHistoryPage queryLatestHistoryPage(const QString& channelName,
                                              int maxCount,
                                              int pageSize,
                                              const RuntimeHistoryCursor& cursor,
                                              const QDateTime& end = QDateTime(),
                                              const std::atomic_bool* cancelToken = nullptr);

    /// 统计历史记录数
    qint64 latestRecordId();
    RuntimeHistoryCount countHistory(const QString& channelName,
                                     const QDateTime& start,
                                     const QDateTime& end,
                                     qint64 maxRecordId = -1);

    /// 统计最近记录数
    RuntimeHistoryCount countLatestHistory(const QString& channelName,
                                           int maxCount,
                                           const QDateTime& end = QDateTime(),
                                           qint64 maxRecordId = -1);

    /// 获取通道最新记录（按降序排列）
    QList<RuntimeRecord> getLatestRecords(const QString& channelName, int count);

    /// 查询指定时间段全部记录
    QList<RuntimeRecord> queryHistory(const QString& channelName,
                                      const QDateTime& start,
                                      const QDateTime& end);

    /// 用于测试的延迟注入（毫秒）
    void setInjectedStorageDelayMs(int delayMs);
    int injectedStorageDelayMs() const;

    /// 获取工作线程指针（用于断言和测试）
    bool submitHistoryTask(QObject* context, std::function<void()> task,
                           std::shared_ptr<std::atomic_bool> cancelled = {});

    QThread* workerThread() const { return m_workerThread; }

signals:
    void startupFinished(bool success, const QString& error);
    void batchCommitted(quint64 batchId, int recordCount);
    void batchAbandoned(quint64 batchId, int recordCount, const QString& error);
    void batchFailed(quint64 batchId, const QString& error);
    void recordsDropped(int count);
    void cleanupBatchCompleted(int deleted);
    void cleanupCompleted(int totalDeleted, bool cancelled);
    void cleanupFailed(const QString& error);
    void workerError(const QString& operation, const QString& error);

public slots:
    void processWorkQueue();

private slots:
    void onThreadStarted();
    void onCleanupRequested(int retentionDays, int chunkSize);

private:
    void finishActiveBatch();
    void scheduleHistoryTask(QObject* context, std::function<void()> task, std::shared_ptr<std::atomic_bool> cancelled, quint64 barrier);
protected:
    virtual bool openDatabaseInThread();
private:
    void closeDatabaseInThread();
    bool executeBatchInsert(const QList<QVariantMap>& records, QString& errorText);
    CleanupResult doCleanupInThread(int retentionDays, int chunkSize);

private:
    BatchItem m_activeBatch;
    QMutex m_startMutex;
    QWaitCondition m_startCondition;
    bool m_startComplete = false;
    bool m_processing = false;
    bool m_shutdownQueued = false;
    QThread* m_ownerThread = nullptr;
    std::atomic_int m_activeRecords{0};
    std::atomic_int m_pendingHistoryTasks{0};
    std::atomic_bool m_cleanupScheduled{false};
    int m_cleanupTotal = 0;
    QString m_dbPath;
    QString m_connectionName;
    QSqlDatabase m_db;
    QThread* m_workerThread = nullptr;

    mutable QMutex m_queueMutex;
    QList<BatchItem> m_pendingBatches;
    int m_totalPendingRecords = 0;
    int m_maxQueueRecords = DEFAULT_MAX_QUEUE_RECORDS;
    quint64 m_nextBatchId = 1;
    quint64 m_droppedRecords = 0;

    std::atomic_bool m_isRunning{false};
    std::atomic_bool m_dbOpen{false};
    std::atomic_bool m_stopRequested{false};
    std::atomic_bool m_cleanupCancelled{false};
    std::atomic_int m_injectedDelayMs{0};
};

} // namespace Core

#endif // ASYNC_DATABASE_WORKER_H
