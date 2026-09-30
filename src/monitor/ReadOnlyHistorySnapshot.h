#pragma once
#include "IMonitorHistoryStore.h"
#include "../core/HistoryQuery.h"
#include <QSqlDatabase>

namespace Monitor {
// Construct, open, query and destroy on the dedicated export thread.
class ReadOnlyHistorySnapshot final : public IMonitorHistoryStore {
public:
    ReadOnlyHistorySnapshot(const QString& path, std::shared_ptr<std::atomic_bool> cancelled);
    ~ReadOnlyHistorySnapshot() override;
    bool open(QString* error);
    qint64 maxRecordId() const { return m_maxId; }
    bool isAvailable() const override { return bool(m_query); }
    QList<RuntimeRecord> getLatestRecords(const QString&, int) override;
    QList<RuntimeRecord> queryHistory(const QString&, const QDateTime&, const QDateTime&) override;
    RuntimeHistoryPage queryHistoryPage(const QString&, const QDateTime&, const QDateTime&, int,
                                        const RuntimeHistoryCursor& = {}) override;
    RuntimeHistoryPage queryLatestHistoryPage(const QString&, int, int, const RuntimeHistoryCursor& = {},
                                              const QDateTime& = {}) override;
    RuntimeHistoryCount countHistory(const QString&, const QDateTime&, const QDateTime&, qint64 = -1) override;
    RuntimeHistoryCount countLatestHistory(const QString&, int, const QDateTime& = {}, qint64 = -1) override;
private:
    QString m_path, m_connection;
    QSqlDatabase m_database;
    std::shared_ptr<std::atomic_bool> m_cancelled;
    std::unique_ptr<Core::HistoryQuery> m_query;
    qint64 m_maxId = -1;
};
}
