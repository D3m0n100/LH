#include <QtTest/QtTest>

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <memory>

#include "core/DataManager.h"
#include "MonitorExportHelper.h"
#include "MonitorManager.h"
#include "MonitorTypes.h"

using namespace Monitor;

class MonitorHistoryExportIntegrationTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_tempDir;

    struct ProviderTrace
    {
        QMap<QString, QList<int>> requestedPageSizes;
        QMap<QString, QList<ExportCursor>> requestedCursors;
        QMap<QString, QList<ExportCursor>> returnedCursors;
        QMap<QString, QList<int>> returnedPageSizes;
    };

    static QString unitFor(const QString& channel)
    {
        if (channel == QStringLiteral("pressure")) {
            return QStringLiteral("bar");
        }
        if (channel == QStringLiteral("flow")) {
            return QStringLiteral("L/min");
        }
        return QStringLiteral("degC");
    }

    static QString displayNameFor(const QString& channel)
    {
        if (channel == QStringLiteral("pressure")) {
            return QStringLiteral("Pressure");
        }
        if (channel == QStringLiteral("flow")) {
            return QStringLiteral("Flow");
        }
        return QStringLiteral("Temperature");
    }

    static ExportPage toExportPage(const DatabaseHistoryPage& page)
    {
        ExportPage result;
        result.success = page.succeeded();
        result.samples = page.samples;
        result.hasMore = page.hasMore;
        result.nextCursor.timestamp = page.nextCursor.timestamp;
        result.nextCursor.id = page.nextCursor.id;
        result.errorMessage = page.errorText;
        if (!result.success && result.errorMessage.isEmpty()) {
            result.errorMessage = QStringLiteral("database history query failed");
        }
        return result;
    }

    static void verifyRecordSample(const RuntimeRecord& record,
                                   const Sample& sample,
                                   const QString& channel)
    {
        QCOMPARE(sample.channelName, channel);
        QCOMPARE(sample.timestamp.toMSecsSinceEpoch(),
                 record.timestamp.toMSecsSinceEpoch());
        QCOMPARE(sample.unit, record.unit);
        QCOMPARE(sample.quality, record.quality);
        QCOMPARE(sample.valueValid, record.valueValid);
        QCOMPARE(sample.metadata.value(QStringLiteral("id")).toLongLong(), record.id);
        QCOMPARE(sample.metadata.value(QStringLiteral("quality")).toString(),
                 runtimePointQualityToString(record.quality));
        QCOMPARE(sample.metadata.value(QStringLiteral("valueValid")).toBool(),
                 record.valueValid);
        QCOMPARE(sample.metadata.value(QStringLiteral("origin")).toString(), record.origin);
        QCOMPARE(sample.metadata.value(QStringLiteral("errorCode")).toString(),
                 record.errorCode);
        QCOMPARE(sample.metadata.value(QStringLiteral("error")).toString(), record.errorText);

        if (record.valueValid) {
            QCOMPARE(sample.value, record.value);
        } else {
            QVERIFY(!sample.valueValid);
        }
        if (record.quality == RuntimePointQuality::Bad
                || record.quality == RuntimePointQuality::Stale) {
            QVERIFY(sample.quality != RuntimePointQuality::Good);
        }
    }

    static void verifyRecordOrder(const QList<RuntimeRecord>& records)
    {
        for (int i = 1; i < records.size(); ++i) {
            const RuntimeRecord& previous = records.at(i - 1);
            const RuntimeRecord& current = records.at(i);
            QVERIFY(current.timestamp.toMSecsSinceEpoch() > previous.timestamp.toMSecsSinceEpoch()
                    || (current.timestamp.toMSecsSinceEpoch()
                            == previous.timestamp.toMSecsSinceEpoch()
                        && current.id > previous.id));
        }
    }

    static void verifyDistinctIdsAtSameTimestamp(const QList<RuntimeRecord>& records)
    {
        bool found = false;
        for (int i = 1; i < records.size(); ++i) {
            if (records.at(i - 1).timestamp.toMSecsSinceEpoch()
                    == records.at(i).timestamp.toMSecsSinceEpoch()) {
                QVERIFY(records.at(i - 1).id != records.at(i).id);
                found = true;
            }
        }
        QVERIFY(found);
    }

    static void verifyProviderTrace(const ProviderTrace& trace,
                                    const QStringList& channels,
                                    int pageSize)
    {
        for (const QString& channel : channels) {
            const QList<int> requestedSizes = trace.requestedPageSizes.value(channel);
            const QList<int> returnedSizes = trace.returnedPageSizes.value(channel);
            const QList<ExportCursor> requested = trace.requestedCursors.value(channel);
            const QList<ExportCursor> returned = trace.returnedCursors.value(channel);

            QVERIFY2(requested.size() >= 3, qPrintable(channel));
            QCOMPARE(requested.size(), returned.size());
            QCOMPARE(requested.size(), requestedSizes.size());
            QCOMPARE(requested.size(), returnedSizes.size());
            QVERIFY(!requested.first().isValid());
            for (int i = 0; i < requested.size(); ++i) {
                QCOMPARE(requestedSizes.at(i), pageSize);
                QVERIFY(returnedSizes.at(i) <= pageSize);
                if (i > 0) {
                    QCOMPARE(requested.at(i).timestamp.toMSecsSinceEpoch(),
                             returned.at(i - 1).timestamp.toMSecsSinceEpoch());
                    QCOMPARE(requested.at(i).id, returned.at(i - 1).id);
                }
            }
            QVERIFY(!returned.last().isValid() || returned.last().id > 0);
        }
    }

    static void verifyJson(const QString& path,
                           const QStringList& channels,
                           const QMap<QString, QList<RuntimeRecord>>& records)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        QVERIFY2(parseError.error == QJsonParseError::NoError,
                 qPrintable(parseError.errorString()));
        QVERIFY(document.isObject());

        const QJsonArray channelArray = document.object().value(QStringLiteral("channels"))
                                            .toArray();
        QCOMPARE(channelArray.size(), channels.size());
        for (int channelIndex = 0; channelIndex < channels.size(); ++channelIndex) {
            const QString& channel = channels.at(channelIndex);
            const QJsonObject channelObject = channelArray.at(channelIndex).toObject();
            QCOMPARE(channelObject.value(QStringLiteral("channelId")).toString(), channel);
            QCOMPARE(channelObject.value(QStringLiteral("unit")).toString(), unitFor(channel));

            const QList<RuntimeRecord>& expected = records.value(channel);
            const QJsonArray samples = channelObject.value(QStringLiteral("samples")).toArray();
            QCOMPARE(samples.size(), expected.size());
            for (int i = 0; i < expected.size(); ++i) {
                const RuntimeRecord& record = expected.at(i);
                const QJsonObject sample = samples.at(i).toObject();
                QCOMPARE(sample.value(QStringLiteral("timestampMs")).toVariant().toLongLong(),
                         record.timestamp.toMSecsSinceEpoch());
                QVERIFY(!sample.value(QStringLiteral("timestamp")).toString().isEmpty());
                QCOMPARE(sample.value(QStringLiteral("quality")).toString(),
                         runtimePointQualityToString(record.quality));
                QCOMPARE(sample.value(QStringLiteral("valueValid")).toBool(),
                         record.valueValid);
                if (record.valueValid) {
                    QVERIFY(!sample.value(QStringLiteral("value")).isNull());
                    QCOMPARE(sample.value(QStringLiteral("value")).toDouble(), record.value);
                } else {
                    QVERIFY(sample.value(QStringLiteral("value")).isNull());
                    QVERIFY(!sample.value(QStringLiteral("valueValid")).toBool());
                }
            }
        }
    }

    static void verifyDelimited(const QString& path,
                                const QStringList& channels,
                                const QMap<QString, QList<RuntimeRecord>>& records,
                                const QString& separator)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QStringList lines = QString::fromUtf8(file.readAll())
                                       .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        int headerIndex = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (!lines.at(i).startsWith(QLatin1Char('#'))) {
                headerIndex = i;
                break;
            }
        }
        QVERIFY(headerIndex >= 0);
        const QString expectedHeader = QStringLiteral("timestamp") + separator
            + QStringLiteral("timestamp_ms") + separator + QStringLiteral("channel_id")
            + separator + QStringLiteral("channel_name") + separator + QStringLiteral("value")
            + separator + QStringLiteral("unit") + separator + QStringLiteral("quality")
            + separator + QStringLiteral("value_valid");
        QCOMPARE(lines.at(headerIndex), expectedHeader);

        int lineIndex = headerIndex + 1;
        int exportedCount = 0;
        for (const QString& channel : channels) {
            const QList<RuntimeRecord>& expected = records.value(channel);
            for (const RuntimeRecord& record : expected) {
                QVERIFY(lineIndex < lines.size());
                const QStringList fields = lines.at(lineIndex++).split(
                    separator, Qt::KeepEmptyParts);
                QCOMPARE(fields.size(), 8);
                QCOMPARE(fields.at(0),
                         record.timestamp.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")));
                QCOMPARE(fields.at(1), QString::number(record.timestamp.toMSecsSinceEpoch()));
                QCOMPARE(fields.at(2), channel);
                QCOMPARE(fields.at(3), displayNameFor(channel));
                if (record.valueValid) {
                    QCOMPARE(fields.at(4), QString::number(record.value, 'g', 6));
                } else {
                    QVERIFY(fields.at(4).isEmpty());
                    QCOMPARE(fields.at(7), QStringLiteral("0"));
                }
                QCOMPARE(fields.at(5), unitFor(channel));
                QCOMPARE(fields.at(6), runtimePointQualityToString(record.quality));
                QCOMPARE(fields.at(7), record.valueValid ? QStringLiteral("1")
                                                           : QStringLiteral("0"));
                ++exportedCount;
            }
        }
        QCOMPARE(lineIndex, lines.size());
        QCOMPARE(exportedCount, 18);
    }

    static void verifyAlignedDelimited(const QString& path,
                                       const QStringList& channels,
                                       const QMap<QString, QList<RuntimeRecord>>& records,
                                       const QString& separator)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QStringList lines = QString::fromUtf8(file.readAll())
                                       .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        int headerIndex = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (!lines.at(i).startsWith(QLatin1Char('#'))) {
                headerIndex = i;
                break;
            }
        }
        QVERIFY(headerIndex >= 0);
        QString expectedHeader = QStringLiteral("timestamp") + separator
            + QStringLiteral("timestamp_ms");
        for (const QString& channel : channels) {
            expectedHeader += separator + channel + separator + channel
                + QStringLiteral("_quality") + separator + channel
                + QStringLiteral("_value_valid");
        }
        QCOMPARE(lines.at(headerIndex), expectedHeader);
        QCOMPARE(records.value(channels.first()).size(), 6);

        int lineIndex = headerIndex + 1;
        for (int sampleIndex = 0; sampleIndex < 6; ++sampleIndex) {
            QVERIFY(lineIndex < lines.size());
            const QStringList fields = lines.at(lineIndex++).split(
                separator, Qt::KeepEmptyParts);
            QCOMPARE(fields.size(), 2 + channels.size() * 3);
            const qint64 timestamp = records.value(channels.first()).at(sampleIndex)
                                         .timestamp.toMSecsSinceEpoch();
            QVERIFY(!fields.at(0).isEmpty());
            QCOMPARE(fields.at(1), QString::number(timestamp));
            for (int channelIndex = 0; channelIndex < channels.size(); ++channelIndex) {
                const RuntimeRecord& record = records.value(channels.at(channelIndex))
                                                   .at(sampleIndex);
                const int column = 2 + channelIndex * 3;
                if (record.valueValid) {
                    QCOMPARE(fields.at(column), QString::number(record.value, 'g', 6));
                } else {
                    QVERIFY(fields.at(column).isEmpty());
                }
                QCOMPARE(fields.at(column + 1),
                         runtimePointQualityToString(record.quality));
                QCOMPARE(fields.at(column + 2), record.valueValid ? QStringLiteral("1")
                                                                    : QStringLiteral("0"));
            }
        }
        QCOMPARE(lineIndex, lines.size());
    }

    static void appendFixtureRecord(QList<QVariantMap>& batch,
                                    const QString& channel,
                                    const QString& unit,
                                    const QDateTime& timestamp,
                                    int ordinal,
                                    RuntimePointQuality quality,
                                    bool valueValid,
                                    double value)
    {
        QVariantMap record;
        record.insert(QStringLiteral("varName"), channel);
        record.insert(QStringLiteral("value"), value);
        record.insert(QStringLiteral("unit"), unit);
        record.insert(QStringLiteral("timestamp"), timestamp);
        record.insert(QStringLiteral("quality"), runtimePointQualityToString(quality));
        record.insert(QStringLiteral("valueValid"), valueValid);
        record.insert(QStringLiteral("origin"),
                      QStringLiteral("origin.%1.%2").arg(channel).arg(ordinal));
        record.insert(QStringLiteral("errorCode"),
                      QStringLiteral("CODE_%1_%2").arg(channel).arg(ordinal));
        record.insert(QStringLiteral("errorText"),
                      QStringLiteral("error-%1-%2").arg(channel).arg(ordinal));
        batch.append(record);
    }

    static void verifyDistinctTimestamps(const QList<RuntimeRecord>& records)
    {
        for (int i = 1; i < records.size(); ++i) {
            QVERIFY(records.at(i).timestamp.toMSecsSinceEpoch()
                    != records.at(i - 1).timestamp.toMSecsSinceEpoch());
        }
    }

private slots:
    void init()
    {
        MonitorManager& monitor = MonitorManager::instance();
        monitor.shutdown();
        monitor.setDatabaseLoggingEnabled(false);
        DataManager::instance().shutdown();
        m_tempDir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_tempDir->isValid());
    }

    void cleanup()
    {
        MonitorManager& monitor = MonitorManager::instance();
        monitor.shutdown();
        monitor.setDatabaseLoggingEnabled(false);
        DataManager::instance().shutdown();
        m_tempDir.reset();
    }

    void historyPagingAndPagedExportPreserveTheContract()
    {
        const QStringList channels = {
            QStringLiteral("pressure"), QStringLiteral("flow"), QStringLiteral("temperature")
        };
        const QDateTime base = QDateTime::fromMSecsSinceEpoch(1700000000000LL, Qt::UTC);
        const QDateTime start = base;
        const QDateTime end = base.addMSecs(5);
        const int pageSize = 2;
        const int recordsPerChannel = 6;

        DataManager& dataManager = DataManager::instance();
        QVERIFY(dataManager.initialize(m_tempDir->filePath(QStringLiteral("history_export.db"))));
        MonitorManager& monitor = MonitorManager::instance();

        QList<QVariantMap> batch;
        const QList<RuntimePointQuality> qualities = {
            RuntimePointQuality::Good, RuntimePointQuality::Stale,
            RuntimePointQuality::Bad, RuntimePointQuality::Good,
            RuntimePointQuality::Stale, RuntimePointQuality::Bad
        };
        for (const QString& channel : channels) {
            const QList<qint64> offsets = channel == QStringLiteral("pressure")
                ? QList<qint64>{0, 1, 1, 2, 3, 4}
                : QList<qint64>{0, 1, 2, 3, 4, 5};
            for (int i = 0; i < recordsPerChannel; ++i) {
                appendFixtureRecord(batch, channel, unitFor(channel),
                                     base.addMSecs(offsets.at(i)), i,
                                     qualities.at(i), qualities.at(i) != RuntimePointQuality::Bad,
                                     qualities.at(i) == RuntimePointQuality::Bad
                                         ? 0.0 : (i + 1) * 10.25);
            }
        }
        const QueryResult writeResult = dataManager.logRuntimeDataBatch(batch);
        QVERIFY(writeResult.success);
        QCOMPARE(writeResult.affectedRows, batch.size());

        QMap<QString, QList<RuntimeRecord>> expectedRecords;
        for (const QString& channel : channels) {
            expectedRecords.insert(channel, dataManager.queryHistory(channel, start, end));
            QCOMPARE(expectedRecords.value(channel).size(), recordsPerChannel);
            verifyRecordOrder(expectedRecords.value(channel));
            if (channel == QStringLiteral("pressure")) {
                verifyDistinctIdsAtSameTimestamp(expectedRecords.value(channel));
            } else {
                verifyDistinctTimestamps(expectedRecords.value(channel));
            }
        }

        // HIST-1: every RuntimeRecord field is preserved by the database-to-Sample page adapter.
        for (const QString& channel : channels) {
            QList<Sample> converted;
            RuntimeHistoryCursor cursor;
            while (true) {
                const DatabaseHistoryPage page = monitor.historyFromDatabasePage(
                    channel, start, end, pageSize, cursor);
                QCOMPARE(page.status, RuntimeHistoryPageStatus::Success);
                QVERIFY(page.samples.size() <= pageSize);
                converted.append(page.samples);
                if (!page.hasMore) {
                    break;
                }
                cursor = page.nextCursor;
            }
            QCOMPARE(converted.size(), recordsPerChannel);
            for (int i = 0; i < converted.size(); ++i) {
                verifyRecordSample(expectedRecords.value(channel).at(i),
                                   converted.at(i), channel);
            }
        }

        // The fixed end bound remains stable when a newer row is inserted between pages.
        const DatabaseHistoryPage firstLatestPage = monitor.historyFromDatabaseLatestPage(
            QStringLiteral("pressure"), recordsPerChannel, pageSize, {}, end);
        QCOMPARE(firstLatestPage.status, RuntimeHistoryPageStatus::Success);
        QCOMPARE(firstLatestPage.samples.size(), pageSize);
        QVERIFY(firstLatestPage.hasMore);

        QVariantMap futureRecord;
        futureRecord.insert(QStringLiteral("varName"), QStringLiteral("pressure"));
        futureRecord.insert(QStringLiteral("value"), 999.0);
        futureRecord.insert(QStringLiteral("unit"), QStringLiteral("bar"));
        futureRecord.insert(QStringLiteral("timestamp"), end.addMSecs(1));
        futureRecord.insert(QStringLiteral("quality"), QStringLiteral("Good"));
        futureRecord.insert(QStringLiteral("valueValid"), true);
        futureRecord.insert(QStringLiteral("origin"), QStringLiteral("future"));
        futureRecord.insert(QStringLiteral("errorCode"), QStringLiteral("FUTURE"));
        futureRecord.insert(QStringLiteral("errorText"), QStringLiteral("future row"));
        QVERIFY(dataManager.logRuntimeDataBatch({futureRecord}).success);

        QList<Sample> boundedLatest = firstLatestPage.samples;
        RuntimeHistoryCursor latestCursor = firstLatestPage.nextCursor;
        int latestPageCount = 1;
        while (latestCursor.isValid() && boundedLatest.size() < recordsPerChannel) {
            const DatabaseHistoryPage page = monitor.historyFromDatabaseLatestPage(
                QStringLiteral("pressure"), recordsPerChannel, pageSize,
                latestCursor, end);
            QCOMPARE(page.status, RuntimeHistoryPageStatus::Success);
            QVERIFY(page.samples.size() <= pageSize);
            boundedLatest.append(page.samples);
            latestCursor = page.nextCursor;
            ++latestPageCount;
            if (!page.hasMore) {
                break;
            }
        }
        QCOMPARE(latestPageCount, 3);
        QCOMPARE(boundedLatest.size(), recordsPerChannel);
        for (int i = 0; i < boundedLatest.size(); ++i) {
            verifyRecordSample(expectedRecords.value(QStringLiteral("pressure")).at(i),
                               boundedLatest.at(i), QStringLiteral("pressure"));
        }

        QList<ExportChannelInfo> channelInfos;
        for (const QString& channel : channels) {
            ExportChannelInfo info(channel, displayNameFor(channel), unitFor(channel), 100);
            info.sampleCount = recordsPerChannel;
            channelInfos.append(info);
        }

        ExportMetadata metadata;
        metadata.projectName = QStringLiteral("HIST-B1");
        metadata.exportTime = end.addMSecs(100);
        metadata.timeWindowMs = 5;
        metadata.totalChannels = channels.size();
        metadata.totalSamples = channels.size() * recordsPerChannel;
        metadata.softwareVersion = QStringLiteral("test");

        auto makeProvider = [&](ProviderTrace& trace) {
            return [&](const QString& channel,
                       const ExportCursor& cursor,
                       int requestedPageSize) {
                trace.requestedPageSizes[channel].append(requestedPageSize);
                trace.requestedCursors[channel].append(cursor);

                RuntimeHistoryCursor databaseCursor;
                databaseCursor.timestamp = cursor.timestamp;
                databaseCursor.id = cursor.id;
                const DatabaseHistoryPage page = monitor.historyFromDatabasePage(
                    channel, start, end, requestedPageSize, databaseCursor);
                trace.returnedPageSizes[channel].append(page.samples.size());
                ExportCursor nextCursor;
                nextCursor.timestamp = page.nextCursor.timestamp;
                nextCursor.id = page.nextCursor.id;
                trace.returnedCursors[channel].append(nextCursor);
                return toExportPage(page);
            };
        };

        for (const QString& extension : {QStringLiteral("json"), QStringLiteral("csv"),
                                         QStringLiteral("tsv")}) {
            ProviderTrace trace;
            MonitorExportHelper helper;
            ExportConfig config = helper.config();
            config.alignMultiChannelByTime = false;
            config.timestampFormat = QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz");
            helper.setConfig(config);

            const QString path = m_tempDir->filePath(QStringLiteral("history.%1").arg(extension));
            const ExportResult result = helper.exportPackagePaged(
                channelInfos, metadata, makeProvider(trace), path, pageSize);
            QVERIFY2(result.success, qPrintable(result.errorMessage));
            QCOMPARE(result.exportedCount, channels.size() * recordsPerChannel);
            QVERIFY(result.fileSizeBytes > 0);
            verifyProviderTrace(trace, channels, pageSize);
            if (extension == QStringLiteral("json")) {
                verifyJson(path, channels, expectedRecords);
            } else {
                verifyDelimited(path, channels, expectedRecords,
                                extension == QStringLiteral("tsv") ? QStringLiteral("\t")
                                                                    : QStringLiteral(","));
            }
        }

        // Aligned output is checked only with per-channel unique timestamps.
        const QStringList alignedChannels = {
            QStringLiteral("flow"), QStringLiteral("temperature")
        };
        QList<ExportChannelInfo> alignedInfos;
        for (const QString& channel : alignedChannels) {
            ExportChannelInfo info(channel, displayNameFor(channel), unitFor(channel), 100);
            info.sampleCount = recordsPerChannel;
            alignedInfos.append(info);
        }
        ExportMetadata alignedMetadata = metadata;
        alignedMetadata.totalChannels = alignedChannels.size();
        alignedMetadata.totalSamples = alignedChannels.size() * recordsPerChannel;
        for (const QString& extension : {QStringLiteral("csv"), QStringLiteral("tsv")}) {
            ProviderTrace trace;
            MonitorExportHelper helper;
            ExportConfig config = helper.config();
            config.alignMultiChannelByTime = true;
            config.timestampFormat = QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz");
            helper.setConfig(config);
            const QString path = m_tempDir->filePath(QStringLiteral("aligned.%1").arg(extension));
            const ExportResult result = helper.exportPackagePaged(
                alignedInfos, alignedMetadata, makeProvider(trace), path, pageSize);
            QVERIFY2(result.success, qPrintable(result.errorMessage));
            QCOMPARE(result.exportedCount, alignedChannels.size() * recordsPerChannel);
            verifyProviderTrace(trace, alignedChannels, pageSize);
            verifyAlignedDelimited(path, alignedChannels, expectedRecords,
                                   extension == QStringLiteral("tsv") ? QStringLiteral("\t")
                                                                       : QStringLiteral(","));
        }
    }

    void queryFailureMapsToExportFailure()
    {
        const QString path = m_tempDir->filePath(QStringLiteral("query_failure.db"));
        DataManager& dataManager = DataManager::instance();
        QVERIFY(dataManager.initialize(path));

        const QString connectionName = QStringLiteral("history_export_failure");
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                                connectionName);
            database.setDatabaseName(path);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral("DROP TABLE runtime_data")));
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        const DatabaseHistoryPage page = MonitorManager::instance().historyFromDatabasePage(
            QStringLiteral("pressure"),
            QDateTime::fromMSecsSinceEpoch(1700000000000LL, Qt::UTC),
            QDateTime::fromMSecsSinceEpoch(1700000000005LL, Qt::UTC), 2);
        QCOMPARE(page.status, RuntimeHistoryPageStatus::SqlError);
        const ExportPage exportPage = toExportPage(page);
        QVERIFY(!exportPage.success);
        QVERIFY(!exportPage.errorMessage.isEmpty());
    }
};

QTEST_MAIN(MonitorHistoryExportIntegrationTest)
#include "monitor_history_export_integration_test.moc"
