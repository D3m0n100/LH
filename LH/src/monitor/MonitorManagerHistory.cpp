#include "MonitorManager.h"
#include "MonitorChannel.h"
#include "communication/RuntimePointQualityMapper.h"
#include "core/DataManager.h"

#include <QReadLocker>

#include <algorithm>

namespace Monitor {

namespace {

static QString qualityToString(RuntimePointQuality q)
{
    return runtimePointQualityToString(q);
}

static void attachRuntimeRecordMetadata(Sample& sample, const RuntimeRecord& record)
{
    if (!record.origin.isEmpty()) {
        sample.metadata[QStringLiteral("origin")] = record.origin;
    }
    if (!record.errorCode.isEmpty()) {
        sample.metadata[QStringLiteral("errorCode")] = record.errorCode;
    }
    if (!record.errorText.isEmpty()) {
        sample.metadata[QStringLiteral("error")] = record.errorText;
    }
}

} // namespace

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

    auto& dm = DataManager::instance();
    if (!dm.isInitialized()) {
        return {};
    }

    QList<RuntimeRecord> records = dm.getLatestRecords(channelName, count);
    QList<Sample> out;

    for (const auto& r : records) {
        Sample s;
        s.channelName = channelName;
        s.value = r.value;
        s.valueValid = r.valueValid;
        s.quality = r.quality;
        s.unit = r.unit;
        s.timestamp = r.timestamp.isValid() ? r.timestamp : QDateTime();
        s.metadata[QStringLiteral("quality")] = qualityToString(s.quality);
        s.metadata[QStringLiteral("valueValid")] = s.valueValid;
        attachRuntimeRecordMetadata(s, r);
        out.append(s);
    }

    // getLatestRecords 默认按 DESC 返回，这里统一转为 ASC
    std::reverse(out.begin(), out.end());
    return out;
}

QList<Sample> MonitorManager::historyFromDatabase(const QString& channelName,
                                                  const QDateTime& start,
                                                  const QDateTime& end) const
{
    auto& dm = DataManager::instance();
    if (!dm.isInitialized()) {
        return {};
    }

    const QDateTime s = start.isValid() ? start.toUTC() : QDateTime::fromMSecsSinceEpoch(0, Qt::UTC);
    const QDateTime e = end.isValid() ? end.toUTC() : QDateTime::currentDateTimeUtc();

    QList<RuntimeRecord> records = dm.queryHistory(channelName, s, e);
    QList<Sample> out;

    for (const auto& r : records) {
        Sample srec;
        srec.channelName = channelName;
        srec.value = r.value;
        srec.valueValid = r.valueValid;
        srec.quality = r.quality;
        srec.unit = r.unit;
        srec.timestamp = r.timestamp.isValid() ? r.timestamp : QDateTime();
        srec.metadata[QStringLiteral("quality")] = qualityToString(srec.quality);
        srec.metadata[QStringLiteral("valueValid")] = srec.valueValid;
        attachRuntimeRecordMetadata(srec, r);
        out.append(srec);
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
    auto& dm = DataManager::instance();
    const RuntimeHistoryPage page = dm.queryHistoryPage(
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
        Sample sample;
        sample.channelName = channelName;
        sample.value = record.value;
        sample.valueValid = record.valueValid;
        sample.quality = record.quality;
        sample.unit = record.unit;
        sample.timestamp = record.timestamp.isValid() ? record.timestamp : QDateTime();
        sample.metadata[QStringLiteral("quality")] = qualityToString(sample.quality);
        sample.metadata[QStringLiteral("valueValid")] = sample.valueValid;
        sample.metadata[QStringLiteral("id")] = record.id;
        attachRuntimeRecordMetadata(sample, record);
        result.samples.append(sample);
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
    auto& dm = DataManager::instance();
    const RuntimeHistoryPage page = dm.queryLatestHistoryPage(
        channelName, maxCount, pageSize, cursor,
        end.isValid() ? end.toUTC() : QDateTime());

    result.status = page.status;
    result.nextCursor = page.nextCursor;
    result.hasMore = page.hasMore;
    result.errorCode = page.errorCode;
    result.errorText = page.errorText;
    result.samples.reserve(page.records.size());
    for (const RuntimeRecord& record : page.records) {
        Sample sample;
        sample.channelName = channelName;
        sample.value = record.value;
        sample.valueValid = record.valueValid;
        sample.quality = record.quality;
        sample.unit = record.unit;
        sample.timestamp = record.timestamp.isValid() ? record.timestamp : QDateTime();
        sample.metadata[QStringLiteral("quality")] = qualityToString(sample.quality);
        sample.metadata[QStringLiteral("valueValid")] = sample.valueValid;
        sample.metadata[QStringLiteral("id")] = record.id;
        attachRuntimeRecordMetadata(sample, record);
        result.samples.append(sample);
    }
    return result;
}

RuntimeHistoryCount MonitorManager::historyFromDatabaseCount(
    const QString& channelName,
    const QDateTime& start,
    const QDateTime& end) const
{
    auto& dm = DataManager::instance();
    return dm.countHistory(
        channelName,
        start.isValid() ? start.toUTC() : QDateTime::fromMSecsSinceEpoch(0, Qt::UTC),
        end.isValid() ? end.toUTC() : QDateTime::currentDateTimeUtc());
}

RuntimeHistoryCount MonitorManager::historyFromDatabaseLatestCount(
    const QString& channelName,
    int maxCount,
    const QDateTime& end) const
{
    return DataManager::instance().countLatestHistory(
        channelName, maxCount, end.isValid() ? end.toUTC() : QDateTime());
}

} // namespace Monitor
