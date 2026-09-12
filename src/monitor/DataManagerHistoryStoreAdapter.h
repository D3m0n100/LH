#ifndef DATA_MANAGER_HISTORY_STORE_ADAPTER_H
#define DATA_MANAGER_HISTORY_STORE_ADAPTER_H

#include "IMonitorHistoryStore.h"

class DataManager;

namespace Monitor {

/**
 * @brief DataManager 历史存储适配器
 *
 * 作为 IMonitorHistoryStore 的默认实现，将历史查询委托给 DataManager::instance()
 * 或指定的 DataManager 实例。
 */
class DataManagerHistoryStoreAdapter : public IMonitorHistoryStore
{
public:
    explicit DataManagerHistoryStoreAdapter(DataManager* dm = nullptr);
    ~DataManagerHistoryStoreAdapter() override = default;

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

    void cancelPendingRequests() override;

private:
    DataManager& dataManager() const;

    DataManager* m_customDataManager = nullptr;
};

} // namespace Monitor

#endif // DATA_MANAGER_HISTORY_STORE_ADAPTER_H
