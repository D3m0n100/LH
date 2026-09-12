#ifndef ASYNC_HISTORY_STORE_ADAPTER_H
#define ASYNC_HISTORY_STORE_ADAPTER_H

#include "IMonitorHistoryStore.h"
#include "core/AsyncDatabaseWorker.h"

namespace Monitor {

/**
 * @brief 异步数据库工作者存储适配器
 *
 * 将 IMonitorHistoryStore 的历史查询路由到 Core::AsyncDatabaseWorker，
 * 解除 GUI 线程与数据库读写查询的阻塞耦合。
 */
class AsyncHistoryStoreAdapter : public IMonitorHistoryStore
{
public:
    explicit AsyncHistoryStoreAdapter(Core::AsyncDatabaseWorker* worker = nullptr);
    ~AsyncHistoryStoreAdapter() override = default;

    bool isAvailable() const override;

    QList<RuntimeRecord> getLatestRecords(const QString& channelName, int count) override;

    QList<RuntimeRecord> queryHistory(const QString& channelName,
                                      const QDateTime& start,
                                      const QDateTime& end) override;

    RuntimeHistoryPage queryHistoryPage(const QString& channelName,
                                        const QDateTime& start,
                                        const QDateTime& end,
                                        int pageSize,
                                        const RuntimeHistoryCursor& cursor = {}) override;

    RuntimeHistoryPage queryLatestHistoryPage(const QString& channelName,
                                              int maxCount,
                                              int pageSize,
                                              const RuntimeHistoryCursor& cursor = {},
                                              const QDateTime& end = QDateTime()) override;

    RuntimeHistoryCount countHistory(const QString& channelName,
                                     const QDateTime& start,
                                     const QDateTime& end,
                                     qint64 maxRecordId = -1) override;

    RuntimeHistoryCount countLatestHistory(const QString& channelName,
                                           int maxCount,
                                           const QDateTime& end = QDateTime(),
                                           qint64 maxRecordId = -1) override;

    void requestPage(QObject* context, const QString& channel, const QDateTime& start,
                     const QDateTime& end, int size, const RuntimeHistoryCursor& cursor,
                     int latestCount, std::function<void(RuntimeHistoryPage)> completed) override;
    void cancelPendingRequests() override;

private:
    QPointer<Core::AsyncDatabaseWorker> m_worker;
    std::atomic_bool m_cancelled{false};
    std::shared_ptr<std::atomic_bool> m_requestCancellation = std::make_shared<std::atomic_bool>(false);
};

} // namespace Monitor

#endif // ASYNC_HISTORY_STORE_ADAPTER_H
