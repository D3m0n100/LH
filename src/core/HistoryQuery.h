#pragma once
#include "DataManager.h"
#include <QSqlDatabase>
#include <QThread>
#include <atomic>

namespace Core {
// Connection remains owned by its caller thread; this class shares query semantics only.
class HistoryQuery {
public:
    explicit HistoryQuery(const QSqlDatabase& database, const std::atomic_bool* cancelled = nullptr)
        : m_db(database), m_ownerThread(QThread::currentThread()), m_cancelled(cancelled) {}
    RuntimeHistoryPage queryHistoryPage(const QString& channel, const QDateTime& start, const QDateTime& end,
        int pageSize, const RuntimeHistoryCursor& cursor, const std::atomic_bool* cancelToken = nullptr);
    RuntimeHistoryPage queryLatestHistoryPage(const QString& channel, int maxCount, int pageSize,
        const RuntimeHistoryCursor& cursor, const QDateTime& end, const std::atomic_bool* cancelToken = nullptr);
    RuntimeHistoryCount countHistory(const QString& channel, const QDateTime& start, const QDateTime& end, qint64 maxRecordId);
    RuntimeHistoryCount countLatestHistory(const QString& channel, int maxCount, const QDateTime& end, qint64 maxRecordId);
    qint64 latestRecordId();
private:
    bool isCancelled() const { return m_cancelled && m_cancelled->load(); }
    QSqlDatabase m_db;
    QThread* m_ownerThread;
    const std::atomic_bool* m_cancelled;
};
}
