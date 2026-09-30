#include "ReadOnlyHistorySnapshot.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QDebug>

namespace Monitor {
ReadOnlyHistorySnapshot::ReadOnlyHistorySnapshot(const QString& path, std::shared_ptr<std::atomic_bool> cancelled)
    : m_path(path), m_connection(QStringLiteral("HistoryExport_%1").arg(QUuid::createUuid().toString())),
      m_cancelled(std::move(cancelled)) {}
ReadOnlyHistorySnapshot::~ReadOnlyHistorySnapshot()
{
    m_query.reset();
    if (m_database.isValid()) { m_database.rollback(); m_database.close(); }
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connection);
}
bool ReadOnlyHistorySnapshot::open(QString* error)
{
    auto fail = [&](const QString& message) { if (error) *error = message; return false; };
    if (m_query) return true;
    if (m_cancelled && m_cancelled->load()) return fail(QStringLiteral("Export cancelled"));
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    m_database.setDatabaseName(m_path);
    m_database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=2000"));
    if (!m_database.open()) return fail(m_database.lastError().text());
    {
        QSqlQuery mode(m_database);
        if (!mode.exec(QStringLiteral("PRAGMA journal_mode")) || !mode.next()
                || mode.value(0).toString().compare(QStringLiteral("wal"), Qt::CaseInsensitive) != 0)
            return fail(QStringLiteral("Concurrent history export requires a WAL database"));
    }
    if (!m_database.transaction()) return fail(m_database.lastError().text());
    auto query = std::make_unique<Core::HistoryQuery>(m_database, m_cancelled.get());
    // The first SELECT pins the read transaction before the writer's barrier callback returns.
    m_maxId = query->latestRecordId();
    if (m_maxId < 0) return fail(QStringLiteral("Cannot pin history snapshot"));
    m_query = std::move(query);
    return true;
}
QList<RuntimeRecord> ReadOnlyHistorySnapshot::getLatestRecords(const QString&, int)
{ qWarning("History snapshot requires the paged API"); return {}; }
QList<RuntimeRecord> ReadOnlyHistorySnapshot::queryHistory(const QString&, const QDateTime&, const QDateTime&)
{ qWarning("History snapshot requires the paged API"); return {}; }
RuntimeHistoryPage ReadOnlyHistorySnapshot::queryHistoryPage(const QString& channel, const QDateTime& start,
    const QDateTime& end, int size, const RuntimeHistoryCursor& cursor)
{ return m_query ? m_query->queryHistoryPage(channel, start, end, size, cursor, m_cancelled.get()) : RuntimeHistoryPage(); }
RuntimeHistoryPage ReadOnlyHistorySnapshot::queryLatestHistoryPage(const QString& channel, int maxCount,
    int size, const RuntimeHistoryCursor& cursor, const QDateTime& end)
{ return m_query ? m_query->queryLatestHistoryPage(channel, maxCount, size, cursor, end, m_cancelled.get()) : RuntimeHistoryPage(); }
RuntimeHistoryCount ReadOnlyHistorySnapshot::countHistory(const QString& channel, const QDateTime& start,
    const QDateTime& end, qint64 maxId)
{ return m_query ? m_query->countHistory(channel, start, end, maxId) : RuntimeHistoryCount(); }
RuntimeHistoryCount ReadOnlyHistorySnapshot::countLatestHistory(const QString& channel, int maxCount,
    const QDateTime& end, qint64 maxId)
{ return m_query ? m_query->countLatestHistory(channel, maxCount, end, maxId) : RuntimeHistoryCount(); }
}
