#include "MonitorHistoryService.h"
namespace Monitor {
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

Sample MonitorHistoryService::toSample(const QString& channelName,
                                    const RuntimeRecord& record,
                                    bool includeIdMetadata)
{
    Sample sample;
    sample.channelName = channelName;
    sample.value = record.value;
    sample.valueValid = record.valueValid;
    sample.quality = record.quality;
    sample.unit = record.unit;
    sample.timestamp = record.timestamp.isValid() ? record.timestamp : QDateTime();
    sample.metadata[QStringLiteral("quality")] = qualityToString(sample.quality);
    sample.metadata[QStringLiteral("valueValid")] = sample.valueValid;
    if (includeIdMetadata) {
        sample.metadata[QStringLiteral("id")] = record.id;
    }
    attachRuntimeRecordMetadata(sample, record);
    return sample;
}

DatabaseHistoryPage MonitorHistoryService::toPage(const QString& channel, const RuntimeHistoryPage& page)
{
    DatabaseHistoryPage out;
    out.status = page.status; out.nextCursor = page.nextCursor; out.hasMore = page.hasMore;
    out.errorCode = page.errorCode; out.errorText = page.errorText;
    for (const auto& record : page.records) out.samples.append(toSample(channel, record));
    return out;
}
DatabaseHistoryPage MonitorHistoryService::page(const QString& channel, const QDateTime& start,
    const QDateTime& end, int size, const RuntimeHistoryCursor& cursor, int latestCount) const
{
    if (!m_store || !m_store->isAvailable()) return {};
    return toPage(channel, latestCount > 0 ? m_store->queryLatestHistoryPage(channel, latestCount, size, cursor, end)
                                           : m_store->queryHistoryPage(channel, start, end, size, cursor));
}
void MonitorHistoryService::requestPage(QObject* context, const QString& channel, const QDateTime& start,
    const QDateTime& end, int size, const RuntimeHistoryCursor& cursor, int latestCount,
    std::function<void(DatabaseHistoryPage)> completed)
{
    if (!m_store || !m_store->isAvailable()) { completed({}); return; }
    m_store->requestPage(context, channel, start, end, size, cursor, latestCount,
                        [channel, completed](RuntimeHistoryPage page) { completed(toPage(channel, page)); });
}
}
