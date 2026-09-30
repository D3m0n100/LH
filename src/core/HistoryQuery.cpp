#include "HistoryQuery.h"
#include <QSqlQuery>
#include <QSqlError>
#include <algorithm>
namespace Core {
namespace {
QString formatIsoUtc(const QDateTime& value) { return value.toUTC().toString(Qt::ISODateWithMs); }
}
RuntimeHistoryPage HistoryQuery::queryHistoryPage(
    const QString& channelName,
    const QDateTime& start,
    const QDateTime& end,
    int pageSize,
    const RuntimeHistoryCursor& cursor,
    const std::atomic_bool* cancelToken)
{
    if (QThread::currentThread() != m_ownerThread) {
        RuntimeHistoryPage page;
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = QStringLiteral("ASYNC_REQUEST_REQUIRED");
        page.errorText = QStringLiteral("Use the asynchronous history request API from the caller thread");
        return page;
    }

    RuntimeHistoryPage page;
    if (isCancelled() || (cancelToken && cancelToken->load(std::memory_order_relaxed))) {
        page.status = RuntimeHistoryPageStatus::Cancelled;
        page.errorCode = QStringLiteral("CANCELLED");
        page.errorText = QStringLiteral("Query was cancelled");
        return page;
    }

    if (!m_db.isOpen()) {
        page.status = RuntimeHistoryPageStatus::NotInitialized;
        page.errorCode = QStringLiteral("NOT_INITIALIZED");
        page.errorText = QStringLiteral("Worker database is not open");
        return page;
    }

    if (!RuntimeHistoryContract::validPageSize(pageSize)) {
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = QStringLiteral("INVALID_PAGE_SIZE");
        return page;
    }

    const QString startText = formatIsoUtc(RuntimeHistoryContract::startUtc(start));
    const QString endText = formatIsoUtc(RuntimeHistoryContract::endUtc(end));

    QString sql = QStringLiteral("SELECT ") + RUNTIME_RECORD_SELECT_COLUMNS + QStringLiteral(
        " FROM runtime_data "
        "WHERE variable_name = :varName AND timestamp >= :start AND timestamp <= :end ");

    if (cursor.isValid()) {
        sql += QStringLiteral("AND (timestamp > :cursorTime OR (timestamp = :cursorTime AND id > :cursorId)) ");
    }
    if (cursor.maxId >= 0) {
        sql += QStringLiteral("AND id <= :maxId ");
    }

    sql += QStringLiteral("ORDER BY timestamp ASC, id ASC LIMIT :limit");

    QSqlQuery query(m_db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":varName"), channelName);
    query.bindValue(QStringLiteral(":start"), startText);
    query.bindValue(QStringLiteral(":end"), endText);
    if (cursor.isValid()) {
        query.bindValue(QStringLiteral(":cursorTime"), formatIsoUtc(cursor.timestamp.toUTC()));
        query.bindValue(QStringLiteral(":cursorId"), cursor.id);
    }
    if (cursor.maxId >= 0) {
        query.bindValue(QStringLiteral(":maxId"), cursor.maxId);
    }
    query.bindValue(QStringLiteral(":limit"), pageSize + 1);

    if (!query.exec()) {
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = query.lastError().nativeErrorCode();
        page.errorText = query.lastError().text();
        return page;
    }

    QList<RuntimeRecord> records;
    while (query.next()) {
        if (isCancelled() || (cancelToken && cancelToken->load())) {
            page.status = RuntimeHistoryPageStatus::Cancelled; return page;
        }
        records.append(runtimeRecordFromSql(query));
    }

    page.hasMore = records.size() > pageSize;
    if (page.hasMore) {
        records.removeLast();
    }

    page.nextCursor = cursor;
    page.records = records;
    page.status = RuntimeHistoryPageStatus::Success;

    if (!page.records.isEmpty()) {
        page.nextCursor.timestamp = page.records.last().timestamp;
        page.nextCursor.id = page.records.last().id;
        page.nextCursor.maxId = cursor.maxId;
    }

    return page;
}

RuntimeHistoryPage HistoryQuery::queryLatestHistoryPage(
    const QString& channelName,
    int maxCount,
    int pageSize,
    const RuntimeHistoryCursor& cursor,
    const QDateTime& end,
    const std::atomic_bool* cancelToken)
{
    if (QThread::currentThread() != m_ownerThread) {
        RuntimeHistoryPage page;
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = QStringLiteral("ASYNC_REQUEST_REQUIRED");
        page.errorText = QStringLiteral("Use the asynchronous history request API from the caller thread");
        return page;
    }

    RuntimeHistoryPage page;
    if (isCancelled() || (cancelToken && cancelToken->load(std::memory_order_relaxed))) {
        page.status = RuntimeHistoryPageStatus::Cancelled;
        page.errorCode = QStringLiteral("CANCELLED");
        page.errorText = QStringLiteral("Query was cancelled");
        return page;
    }

    if (!m_db.isOpen()) {
        page.status = RuntimeHistoryPageStatus::NotInitialized;
        page.errorCode = QStringLiteral("NOT_INITIALIZED");
        page.errorText = QStringLiteral("Worker database is not open");
        return page;
    }

    if (!RuntimeHistoryContract::validPageSize(pageSize) || maxCount <= 0) {
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = !RuntimeHistoryContract::validPageSize(pageSize) ? QStringLiteral("INVALID_PAGE_SIZE") : QStringLiteral("INVALID_MAX_COUNT");
        return page;
    }

    const QString endText = end.isValid() ? formatIsoUtc(end.toUTC()) : QString();

    QString subquery = QStringLiteral("SELECT ") + RUNTIME_RECORD_SELECT_COLUMNS + QStringLiteral(
        " FROM runtime_data WHERE variable_name = :varName ");
    if (!endText.isEmpty()) {
        subquery += QStringLiteral("AND timestamp <= :end ");
    }
    if (cursor.maxId >= 0) {
        subquery += QStringLiteral("AND id <= :maxId ");
    }
    subquery += QStringLiteral("ORDER BY timestamp DESC, id DESC LIMIT :maxCount");

    QString sql = QStringLiteral("SELECT * FROM (") + subquery + QStringLiteral(") WHERE 1=1 ");
    if (cursor.isValid()) {
        sql += QStringLiteral("AND (timestamp > :cursorTime OR (timestamp = :cursorTime AND id > :cursorId)) ");
    }
    if (cursor.maxId >= 0) {
        sql += QStringLiteral("AND id <= :maxId ");
    }
    sql += QStringLiteral("ORDER BY timestamp ASC, id ASC LIMIT :limit");

    QSqlQuery query(m_db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":varName"), channelName);
    if (!endText.isEmpty()) {
        query.bindValue(QStringLiteral(":end"), endText);
    }
    query.bindValue(QStringLiteral(":maxCount"), maxCount);
    if (cursor.isValid()) {
        query.bindValue(QStringLiteral(":cursorTime"), formatIsoUtc(cursor.timestamp.toUTC()));
        query.bindValue(QStringLiteral(":cursorId"), cursor.id);
    }
    if (cursor.maxId >= 0) {
        query.bindValue(QStringLiteral(":maxId"), cursor.maxId);
    }
    query.bindValue(QStringLiteral(":limit"), pageSize + 1);

    if (!query.exec()) {
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = query.lastError().nativeErrorCode();
        page.errorText = query.lastError().text();
        return page;
    }

    QList<RuntimeRecord> records;
    while (query.next()) {
        if (isCancelled() || (cancelToken && cancelToken->load())) {
            page.status = RuntimeHistoryPageStatus::Cancelled; return page;
        }
        records.append(runtimeRecordFromSql(query));
    }

    page.hasMore = records.size() > pageSize;
    if (page.hasMore) {
        records.removeLast();
    }

    page.nextCursor = cursor;
    page.records = records;
    page.status = RuntimeHistoryPageStatus::Success;

    if (!page.records.isEmpty()) {
        page.nextCursor.timestamp = page.records.last().timestamp;
        page.nextCursor.id = page.records.last().id;
        page.nextCursor.maxId = cursor.maxId;
    }

    return page;
}

qint64 HistoryQuery::latestRecordId()
{
    if (QThread::currentThread() != m_ownerThread || !m_db.isOpen()) return -1;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT COALESCE(MAX(id), 0) FROM runtime_data")) || !query.next()) return -1;
    return query.value(0).toLongLong();
}

RuntimeHistoryCount HistoryQuery::countHistory(
    const QString& channelName,
    const QDateTime& start,
    const QDateTime& end,
    qint64 maxRecordId)
{
    if (QThread::currentThread() != m_ownerThread) {
        RuntimeHistoryCount count;
        count.status = RuntimeHistoryPageStatus::SqlError;
        count.errorCode = QStringLiteral("ASYNC_REQUEST_REQUIRED");
        count.errorText = QStringLiteral("Use the asynchronous history request API from the caller thread");
        return count;
    }

    RuntimeHistoryCount count;
    if (isCancelled()) {
        count.status = RuntimeHistoryPageStatus::Cancelled;
        count.errorCode = QStringLiteral("CANCELLED"); return count;
    }
    if (!m_db.isOpen()) {
        count.status = RuntimeHistoryPageStatus::NotInitialized;
        count.errorCode = QStringLiteral("NOT_INITIALIZED");
        return count;
    }

    const QString startText = formatIsoUtc(RuntimeHistoryContract::startUtc(start));
    const QString endText = formatIsoUtc(RuntimeHistoryContract::endUtc(end));

    QString sql = QStringLiteral(
        "SELECT COUNT(*) FROM runtime_data "
        "WHERE variable_name = :varName AND timestamp >= :start AND timestamp <= :end ");
    if (maxRecordId >= 0) {
        sql += QStringLiteral("AND id <= :maxId");
    }

    QSqlQuery query(m_db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":varName"), channelName);
    query.bindValue(QStringLiteral(":start"), startText);
    query.bindValue(QStringLiteral(":end"), endText);
    if (maxRecordId >= 0) {
        query.bindValue(QStringLiteral(":maxId"), maxRecordId);
    }

    if (!query.exec() || !query.next()) {
        count.status = RuntimeHistoryPageStatus::SqlError;
        count.errorCode = query.lastError().nativeErrorCode();
        count.errorText = query.lastError().text();
        return count;
    }

    count.status = RuntimeHistoryPageStatus::Success;
    count.count = query.value(0).toLongLong();
    return count;
}

RuntimeHistoryCount HistoryQuery::countLatestHistory(
    const QString& channelName,
    int maxCount,
    const QDateTime& end,
    qint64 maxRecordId)
{
    if (QThread::currentThread() != m_ownerThread) {
        RuntimeHistoryCount count;
        count.status = RuntimeHistoryPageStatus::SqlError;
        count.errorCode = QStringLiteral("ASYNC_REQUEST_REQUIRED");
        count.errorText = QStringLiteral("Use the asynchronous history request API from the caller thread");
        return count;
    }

    RuntimeHistoryCount count;
    if (isCancelled()) {
        count.status = RuntimeHistoryPageStatus::Cancelled;
        count.errorCode = QStringLiteral("CANCELLED"); return count;
    }
    if (maxCount <= 0) {
        count.status = RuntimeHistoryPageStatus::SqlError;
        count.errorCode = QStringLiteral("INVALID_MAX_COUNT");
        count.errorText = QStringLiteral("Latest record count must be positive");
        return count;
    }
    if (!m_db.isOpen()) {
        count.status = RuntimeHistoryPageStatus::NotInitialized;
        count.errorCode = QStringLiteral("NOT_INITIALIZED");
        return count;
    }

    const QString endText = end.isValid() ? formatIsoUtc(end.toUTC()) : QString();

    QString sql = QStringLiteral("SELECT COUNT(*) FROM runtime_data WHERE variable_name = :varName ");
    if (!endText.isEmpty()) {
        sql += QStringLiteral("AND timestamp <= :end ");
    }
    if (maxRecordId >= 0) {
        sql += QStringLiteral("AND id <= :maxId ");
    }

    QSqlQuery query(m_db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":varName"), channelName);
    if (!endText.isEmpty()) {
        query.bindValue(QStringLiteral(":end"), endText);
    }
    if (maxRecordId >= 0) {
        query.bindValue(QStringLiteral(":maxId"), maxRecordId);
    }

    if (!query.exec() || !query.next()) {
        count.status = RuntimeHistoryPageStatus::SqlError;
        count.errorCode = query.lastError().nativeErrorCode();
        count.errorText = query.lastError().text();
        return count;
    }

    const qint64 total = query.value(0).toLongLong();
    count.status = RuntimeHistoryPageStatus::Success;
    count.count = (maxCount > 0) ? qMin(static_cast<qint64>(maxCount), total) : total;
    return count;
}


} // namespace Core
