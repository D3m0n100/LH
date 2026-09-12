#include "MonitorManager.h"
#include "MonitorChannel.h"
#include "IMonitorHistoryStore.h"
#include "communication/RuntimePointQualityMapper.h"

#include <QReadLocker>

#include <algorithm>

namespace Monitor {



QList<Sample> MonitorManager::history(const QString& channelName, int count) const
{
    QReadLocker locker(&m_channelLock);
    auto ch = m_channels.value(channelName);
    if (!ch) {
        return {};
    }
    return ch->history(count);
}

QList<Sample> MonitorManager::history(const QString& channelName,
                                      const QDateTime& start,
                                      const QDateTime& end) const
{
    QReadLocker locker(&m_channelLock);
    auto ch = m_channels.value(channelName);
    if (!ch) {
        return {};
    }
    return ch->history(start, end);
}

QList<Sample> MonitorManager::historyFromDatabase(const QString& channelName, int count) const
{
    if (count <= 0) {
        return {};
    }

    if (!m_historyStore || !m_historyStore->isAvailable()) {
        return {};
    }

    QList<RuntimeRecord> records = m_historyStore->getLatestRecords(channelName, count);
    QList<Sample> out;
    out.reserve(records.size());

    for (const auto& r : records) {
        out.append(MonitorHistoryService::toSample(channelName, r, false));
    }

    // getLatestRecords 默认按 DESC 返回，这里统一转为 ASC
    std::reverse(out.begin(), out.end());
    return out;
}

QList<Sample> MonitorManager::historyFromDatabase(const QString& channelName,
                                                  const QDateTime& start,
                                                  const QDateTime& end) const
{
    if (!m_historyStore || !m_historyStore->isAvailable()) {
        return {};
    }

    const QDateTime s = start.isValid() ? start.toUTC() : QDateTime::fromMSecsSinceEpoch(0, Qt::UTC);
    const QDateTime e = end.isValid() ? end.toUTC() : QDateTime::currentDateTimeUtc();

    QList<RuntimeRecord> records = m_historyStore->queryHistory(channelName, s, e);
    QList<Sample> out;
    out.reserve(records.size());

    for (const auto& r : records) {
        out.append(MonitorHistoryService::toSample(channelName, r, false));
    }
    return out;
}

DatabaseHistoryPage MonitorManager::historyFromDatabasePage(
    const QString& channelName,
    const QDateTime& start,
    const QDateTime& end,
    int pageSize,
    const RuntimeHistoryCursor& cursor) const
{
    DatabaseHistoryPage result;
    if (!m_historyStore || !m_historyStore->isAvailable()) {
        result.status = RuntimeHistoryPageStatus::NotInitialized;
        result.errorCode = QStringLiteral("NOT_INITIALIZED");
        result.errorText = QStringLiteral("History store is not initialized or unavailable");
        return result;
    }

    const RuntimeHistoryPage page = m_historyStore->queryHistoryPage(
        channelName,
        start.isValid() ? start.toUTC() : QDateTime::fromMSecsSinceEpoch(0, Qt::UTC),
        end.isValid() ? end.toUTC() : QDateTime::currentDateTimeUtc(),
        pageSize,
        cursor);

    result.status = page.status;
    result.nextCursor = page.nextCursor;
    result.hasMore = page.hasMore;
    result.errorCode = page.errorCode;
    result.errorText = page.errorText;
    result.samples.reserve(page.records.size());
    for (const RuntimeRecord& record : page.records) {
        result.samples.append(MonitorHistoryService::toSample(channelName, record, true));
    }
    return result;
}

DatabaseHistoryPage MonitorManager::historyFromDatabaseLatestPage(
    const QString& channelName,
    int maxCount,
    int pageSize,
    const RuntimeHistoryCursor& cursor,
    const QDateTime& end) const
{
    DatabaseHistoryPage result;
    if (!m_historyStore || !m_historyStore->isAvailable()) {
        result.status = RuntimeHistoryPageStatus::NotInitialized;
        result.errorCode = QStringLiteral("NOT_INITIALIZED");
        result.errorText = QStringLiteral("History store is not initialized or unavailable");
        return result;
    }

    const RuntimeHistoryPage page = m_historyStore->queryLatestHistoryPage(
        channelName, maxCount, pageSize, cursor,
        end.isValid() ? end.toUTC() : QDateTime());

    result.status = page.status;
    result.nextCursor = page.nextCursor;
    result.hasMore = page.hasMore;
    result.errorCode = page.errorCode;
    result.errorText = page.errorText;
    result.samples.reserve(page.records.size());
    for (const RuntimeRecord& record : page.records) {
        result.samples.append(MonitorHistoryService::toSample(channelName, record, true));
    }
    return result;
}

RuntimeHistoryCount MonitorManager::historyFromDatabaseCount(
    const QString& channelName,
    const QDateTime& start,
    const QDateTime& end,
    qint64 maxRecordId) const
{
    if (!m_historyStore || !m_historyStore->isAvailable()) {
        RuntimeHistoryCount count;
        count.status = RuntimeHistoryPageStatus::NotInitialized;
        count.errorCode = QStringLiteral("NOT_INITIALIZED");
        count.errorText = QStringLiteral("History store is not initialized or unavailable");
        return count;
    }

    return m_historyStore->countHistory(
        channelName,
        start.isValid() ? start.toUTC() : QDateTime::fromMSecsSinceEpoch(0, Qt::UTC),
        end.isValid() ? end.toUTC() : QDateTime::currentDateTimeUtc(),
        maxRecordId);
}

RuntimeHistoryCount MonitorManager::historyFromDatabaseLatestCount(
    const QString& channelName,
    int maxCount,
    const QDateTime& end,
    qint64 maxRecordId) const
{
    if (!m_historyStore || !m_historyStore->isAvailable()) {
        RuntimeHistoryCount count;
        count.status = RuntimeHistoryPageStatus::NotInitialized;
        count.errorCode = QStringLiteral("NOT_INITIALIZED");
        count.errorText = QStringLiteral("History store is not initialized or unavailable");
        return count;
    }

    return m_historyStore->countLatestHistory(
        channelName, maxCount, end.isValid() ? end.toUTC() : QDateTime(), maxRecordId);
}

} // namespace Monitor
