#include "DataManagerHistoryStoreAdapter.h"
#include "core/DataManager.h"

namespace Monitor {

DataManagerHistoryStoreAdapter::DataManagerHistoryStoreAdapter(DataManager* dm)
    : m_customDataManager(dm)
{
}

DataManager& DataManagerHistoryStoreAdapter::dataManager() const
{
    return m_customDataManager ? *m_customDataManager : DataManager::instance();
}

bool DataManagerHistoryStoreAdapter::isAvailable() const
{
    return dataManager().isInitialized();
}

QList<RuntimeRecord> DataManagerHistoryStoreAdapter::getLatestRecords(const QString& channelName, int count)
{
    return dataManager().getLatestRecords(channelName, count);
}

QList<RuntimeRecord> DataManagerHistoryStoreAdapter::queryHistory(const QString& channelName,
                                                                 const QDateTime& start,
                                                                 const QDateTime& end)
{
    return dataManager().queryHistory(channelName, start, end);
}

RuntimeHistoryPage DataManagerHistoryStoreAdapter::queryHistoryPage(const QString& channelName,
                                                                   const QDateTime& start,
                                                                   const QDateTime& end,
                                                                   int pageSize,
                                                                   const RuntimeHistoryCursor& cursor)
{
    return dataManager().queryHistoryPage(channelName, start, end, pageSize, cursor);
}

RuntimeHistoryPage DataManagerHistoryStoreAdapter::queryLatestHistoryPage(const QString& channelName,
                                                                         int maxCount,
                                                                         int pageSize,
                                                                         const RuntimeHistoryCursor& cursor,
                                                                         const QDateTime& end)
{
    return dataManager().queryLatestHistoryPage(channelName, maxCount, pageSize, cursor, end);
}

RuntimeHistoryCount DataManagerHistoryStoreAdapter::countHistory(const QString& channelName,
                                                                const QDateTime& start,
                                                                const QDateTime& end,
                                                                qint64 maxRecordId)
{
    return dataManager().countHistory(channelName, start, end, maxRecordId);
}

RuntimeHistoryCount DataManagerHistoryStoreAdapter::countLatestHistory(const QString& channelName,
                                                                      int maxCount,
                                                                      const QDateTime& end,
                                                                      qint64 maxRecordId)
{
    return dataManager().countLatestHistory(channelName, maxCount, end, maxRecordId);
}

void DataManagerHistoryStoreAdapter::cancelPendingRequests()
{
    // 同步适配器无需取消动作，为后续异步化提供统一占位
}

} // namespace Monitor
