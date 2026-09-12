#pragma once
#include "IMonitorHistoryStore.h"
#include "MonitorSample.h"
#include <QObject>
namespace Monitor {
struct DatabaseHistoryPage
{
    RuntimeHistoryPageStatus status = RuntimeHistoryPageStatus::NotInitialized;
    QList<Sample> samples;
    RuntimeHistoryCursor nextCursor;
    bool hasMore = false;
    QString errorCode;
    QString errorText;

    bool succeeded() const { return status == RuntimeHistoryPageStatus::Success; }
    bool isEnd() const { return succeeded() && !hasMore; }
};
// QtCore-only history boundary. No SQL singleton, chart or widget dependency.
class MonitorHistoryService {
public:
    explicit MonitorHistoryService(std::shared_ptr<IMonitorHistoryStore> store) : m_store(std::move(store)) {}
    static Sample toSample(const QString& channel, const RuntimeRecord& record, bool includeId = true);
    static DatabaseHistoryPage toPage(const QString& channel, const RuntimeHistoryPage& page);
    DatabaseHistoryPage page(const QString& channel, const QDateTime& start, const QDateTime& end,
                             int size, const RuntimeHistoryCursor& cursor = {}, int latestCount = 0) const;
    void requestPage(QObject* context, const QString& channel, const QDateTime& start, const QDateTime& end,
                     int size, const RuntimeHistoryCursor& cursor, int latestCount,
                     std::function<void(DatabaseHistoryPage)> completed);
    void cancel() { if (m_store) m_store->cancelPendingRequests(); }
private:
    std::shared_ptr<IMonitorHistoryStore> m_store;
};
}
