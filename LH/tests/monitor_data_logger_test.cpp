#include <QtTest/QtTest>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QSignalSpy>
#include <QMutexLocker>

#include "core/DataManager.h"
#define private public
#include "monitor/MonitorDataLogger.h"
#undef private
#include "monitor/MonitorManager.h"

using namespace Monitor;

class MonitorDataLoggerTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    static Sample makeSample(const QString& channelName,
                             RuntimePointQuality quality,
                             bool valueValid,
                             double value = 42.0)
    {
        Sample sample;
        sample.channelName = channelName;
        sample.value = value;
        sample.unit = QStringLiteral("bar");
        sample.timestamp = QDateTime::currentDateTimeUtc();
        sample.quality = quality;
        sample.valueValid = valueValid;
        return sample;
    }

    static bool executeSql(const QString& databasePath, const QStringList& statements)
    {
        const QString connectionName = QStringLiteral("monitor_logger_test_%1").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces));
        bool success = false;

        {
            QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                               connectionName);
            database.setDatabaseName(databasePath);
            if (!database.open()) {
                qWarning() << database.lastError().text();
            } else {
                success = true;
                QSqlQuery query(database);
                for (const QString& statement : statements) {
                    if (!query.exec(statement)) {
                        qWarning() << query.lastError().text() << statement;
                        success = false;
                        break;
                    }
                }
                database.close();
            }
        }

        QSqlDatabase::removeDatabase(connectionName);
        return success;
    }

    static QList<RuntimeRecord> allRecords(const QString& channelName)
    {
        return DataManager::instance().queryHistory(
            channelName,
            QDateTime::currentDateTimeUtc().addDays(-1),
            QDateTime::currentDateTimeUtc().addDays(1));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
    }

    void init()
    {
        auto& manager = MonitorManager::instance();
        manager.stopMonitoring();
        manager.setDatabaseLoggingEnabled(false);
        manager.clearAllData();
        DataManager::instance().shutdown();
    }

    void cleanup()
    {
        auto& manager = MonitorManager::instance();
        manager.stopMonitoring();
        manager.setDatabaseLoggingEnabled(false);
        manager.clearAllData();
        DataManager::instance().shutdown();
    }

    void disablingFlushesBufferedInvalidSampleWithQuality()
    {
        auto& dataManager = DataManager::instance();
        const QString databasePath = m_tempDir.path() + QStringLiteral("/disable_flush.db");
        QVERIFY(dataManager.initialize(databasePath));

        {
            MonitorDataLogger logger;
            logger.setBatchSize(1000);
            logger.enqueueSample(makeSample(QStringLiteral("invalid.point"),
                                            RuntimePointQuality::Bad,
                                            false));

            logger.setEnabled(false);

            const QList<RuntimeRecord> records = allRecords(QStringLiteral("invalid.point"));
            QCOMPARE(records.size(), 1);
            QVERIFY(!records.first().valueValid);
            QCOMPARE(records.first().quality, RuntimePointQuality::Bad);
        }
    }

    void oversizedBatchKeepsLimitAndReportsDroppedCount()
    {
        MonitorDataLogger logger;
        logger.setBatchSize(MonitorDataLogger::DEFAULT_MAX_BUFFER_SIZE + 100);
        QSignalSpy droppedSpy(&logger, &MonitorDataLogger::samplesDropped);
        QList<Sample> samples;
        samples.reserve(MonitorDataLogger::DEFAULT_MAX_BUFFER_SIZE + 3);
        for (int i = 0; i < MonitorDataLogger::DEFAULT_MAX_BUFFER_SIZE + 3; ++i) {
            samples.append(makeSample(QStringLiteral("overflow.%1").arg(i),
                                      RuntimePointQuality::Good, true, i));
        }
        logger.enqueueSamples(samples);
        QCOMPARE(droppedSpy.count(), 1);
        QCOMPARE(droppedSpy.first().at(0).toInt(), 3);
    }

    void failedBatchMergeKeepsNewestRecordsWithinLimit()
    {
        MonitorDataLogger logger;
        QSignalSpy droppedSpy(&logger, &MonitorDataLogger::samplesDropped);

        QList<QVariantMap> failedBatch;
        failedBatch.reserve(MonitorDataLogger::DEFAULT_MAX_BUFFER_SIZE);
        for (int i = 0; i < MonitorDataLogger::DEFAULT_MAX_BUFFER_SIZE; ++i) {
            failedBatch.append({{QStringLiteral("varName"), QStringLiteral("old.%1").arg(i)}});
        }
        {
            QMutexLocker locker(&logger.m_mutex);
            logger.m_buffer = {
                {{QStringLiteral("varName"), QStringLiteral("new.0")}},
                {{QStringLiteral("varName"), QStringLiteral("new.1")}},
                {{QStringLiteral("varName"), QStringLiteral("new.2")}}
            };
            logger.m_flushing = true;
        }

        logger.restoreFailedBatch(failedBatch);

        QCOMPARE(logger.m_buffer.size(), MonitorDataLogger::DEFAULT_MAX_BUFFER_SIZE);
        QCOMPARE(logger.m_buffer.first().value(QStringLiteral("varName")).toString(),
                 QStringLiteral("old.3"));
        QCOMPARE(logger.m_buffer.last().value(QStringLiteral("varName")).toString(),
                 QStringLiteral("new.2"));
        QCOMPARE(droppedSpy.count(), 1);
        QCOMPARE(droppedSpy.first().at(0).toInt(), 3);
        QVERIFY(!logger.m_flushing);

        QVariantMap retryRecord;
        retryRecord.insert(QStringLiteral("varName"), QStringLiteral("retry.point"));
        retryRecord.insert(QStringLiteral("value"), 1.0);
        retryRecord.insert(QStringLiteral("unit"), QStringLiteral("bar"));
        retryRecord.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc());
        {
            QMutexLocker locker(&logger.m_mutex);
            logger.m_buffer = {retryRecord};
        }
        QVERIFY(DataManager::instance().initialize(m_tempDir.path() + QStringLiteral("/merge_retry.db")));
        logger.flush();
        QCOMPARE(allRecords(QStringLiteral("retry.point")).size(), 1);
    }

    void unavailableDatabaseRetainsBatchUntilRetry()
    {
        auto& dataManager = DataManager::instance();
        const QString databasePath = m_tempDir.path() + QStringLiteral("/unavailable_retry.db");

        MonitorDataLogger logger;
        logger.setBatchSize(1000);
        logger.enqueueSample(makeSample(QStringLiteral("retained.point"),
                                        RuntimePointQuality::Stale,
                                        true,
                                        9.5));

        logger.flush();
        QVERIFY(!dataManager.isInitialized());

        QVERIFY(dataManager.initialize(databasePath));
        logger.flush();

        const QList<RuntimeRecord> records = allRecords(QStringLiteral("retained.point"));
        QCOMPARE(records.size(), 1);
        QVERIFY(records.first().valueValid);
        QCOMPARE(records.first().value, 9.5);
        QCOMPARE(records.first().quality, RuntimePointQuality::Stale);
    }

    void shutdownFlushesBufferedSample()
    {
        auto& dataManager = DataManager::instance();
        const QString databasePath = m_tempDir.path() + QStringLiteral("/shutdown_flush.db");
        QVERIFY(dataManager.initialize(databasePath));

        MonitorDataLogger logger;
        logger.setBatchSize(1000);
        logger.enqueueSample(makeSample(QStringLiteral("shutdown.point"),
                                        RuntimePointQuality::Good,
                                        true,
                                        7.0));

        logger.shutdown();

        const QList<RuntimeRecord> records = allRecords(QStringLiteral("shutdown.point"));
        QCOMPARE(records.size(), 1);
        QCOMPARE(records.first().value, 7.0);
    }

    void stoppingManagerFlushesBufferedSample()
    {
        auto& dataManager = DataManager::instance();
        auto& manager = MonitorManager::instance();
        const QString databasePath = m_tempDir.path() + QStringLiteral("/manager_stop_flush.db");
        QVERIFY(dataManager.initialize(databasePath));

        QVERIFY(manager.isDatabaseHistoryAvailable());
        manager.setDatabaseLoggingEnabled(true);
        manager.recordSample(makeSample(QStringLiteral("manager.stop.point"),
                                        RuntimePointQuality::Good,
                                        true,
                                        8.0));

        manager.stopMonitoring();

        const QList<RuntimeRecord> records = allRecords(QStringLiteral("manager.stop.point"));
        QCOMPARE(records.size(), 1);
        QCOMPARE(records.first().value, 8.0);
    }

    void cleanupTimerPurgesPersistentDataWhenLoggingEnabled()
    {
        auto& dataManager = DataManager::instance();
        auto& manager = MonitorManager::instance();
        const QString databasePath = m_tempDir.path() + QStringLiteral("/cleanup_timer.db");
        QVERIFY(dataManager.initialize(databasePath));

        QVERIFY(executeSql(databasePath, {
            QStringLiteral(
                "INSERT INTO runtime_data (timestamp, variable_name, value, unit) "
                "VALUES (datetime('now', '-2 day'), 'cleanup.runtime', 1.0, 'bar')"),
            QStringLiteral(
                "INSERT INTO system_logs (timestamp, level, module, message) "
                "VALUES (datetime('now', '-2 day'), 'INFO', 'cleanup.module', 'old log')")
        }));

        manager.setDataRetentionDays(1);
        manager.setDatabaseLoggingEnabled(false);
        manager.startMonitoring();
        QVERIFY(QMetaObject::invokeMethod(&manager, "onCleanupTimeout", Qt::DirectConnection));

        const QDateTime start = QDateTime::currentDateTimeUtc().addDays(-3);
        const QDateTime end = QDateTime::currentDateTimeUtc().addDays(1);
        QCOMPARE(dataManager.queryHistory(QStringLiteral("cleanup.runtime"), start, end).size(), 1);
        QCOMPARE(dataManager.queryLogs(start, end).size(), 1);

        manager.setDatabaseLoggingEnabled(true);
        QVERIFY(QMetaObject::invokeMethod(&manager, "onCleanupTimeout", Qt::DirectConnection));
        QCOMPARE(dataManager.queryHistory(QStringLiteral("cleanup.runtime"), start, end).size(), 0);
        QCOMPARE(dataManager.queryLogs(start, end).size(), 0);

        manager.stopMonitoring();
        manager.setDataRetentionDays(7);
    }

    void failedBatchIsAtomicAndLoggerRetriesIt()
    {
        auto& dataManager = DataManager::instance();
        const QString databasePath = m_tempDir.path() + QStringLiteral("/atomic_retry.db");
        QVERIFY(dataManager.initialize(databasePath));

        QVERIFY(executeSql(databasePath, {
            QStringLiteral(
                "CREATE TRIGGER reject_runtime_insert "
                "BEFORE INSERT ON runtime_data "
                "WHEN NEW.variable_name = 'reject.point' "
                "BEGIN SELECT RAISE(ABORT, 'rejected by test'); END")
        }));

        MonitorDataLogger logger;
        logger.setBatchSize(1000);
        logger.enqueueSamples({
            makeSample(QStringLiteral("accepted.point"), RuntimePointQuality::Good, true, 1.0),
            makeSample(QStringLiteral("reject.point"), RuntimePointQuality::Bad, false)
        });

        logger.flush();
        QVERIFY(allRecords(QStringLiteral("accepted.point")).isEmpty());
        QVERIFY(allRecords(QStringLiteral("reject.point")).isEmpty());
        QVERIFY(!dataManager.getRuntimeValue(QStringLiteral("accepted.point")).isValid());

        QVERIFY(executeSql(databasePath, {
            QStringLiteral("DROP TRIGGER reject_runtime_insert")
        }));
        logger.flush();

        const QList<RuntimeRecord> acceptedRecords = allRecords(QStringLiteral("accepted.point"));
        const QList<RuntimeRecord> rejectedRecords = allRecords(QStringLiteral("reject.point"));
        QCOMPARE(acceptedRecords.size(), 1);
        QCOMPARE(rejectedRecords.size(), 1);
        QVERIFY(acceptedRecords.first().valueValid);
        QVERIFY(!rejectedRecords.first().valueValid);
        QCOMPARE(rejectedRecords.first().quality, RuntimePointQuality::Bad);
    }

    void versionThreeDatabaseMigratesProvenanceColumns()
    {
        auto& dataManager = DataManager::instance();
        const QString databasePath = m_tempDir.path() + QStringLiteral("/migration_v3.db");

        QVERIFY(executeSql(databasePath, {
            QStringLiteral(
                "CREATE TABLE schema_version ("
                "id INTEGER PRIMARY KEY CHECK (id = 1), "
                "version INTEGER NOT NULL, "
                "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP)"),
            QStringLiteral("INSERT INTO schema_version (id, version) VALUES (1, 3)"),
            QStringLiteral(
                "CREATE TABLE runtime_data ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "variable_name TEXT NOT NULL, "
                "value REAL, unit TEXT, "
                "quality TEXT NOT NULL DEFAULT 'Unknown', "
                "value_valid INTEGER NOT NULL DEFAULT 1)"),
            QStringLiteral(
                "INSERT INTO runtime_data (variable_name, value, unit, quality, value_valid) "
                "VALUES ('legacy.point', NULL, 'bar', 'Bad', 0)")
        }));

        QVERIFY(dataManager.initialize(databasePath));
        QCOMPARE(dataManager.schemaVersion(), DataManager::CURRENT_SCHEMA_VERSION);

        const QList<RuntimeRecord> records = allRecords(QStringLiteral("legacy.point"));
        QCOMPARE(records.size(), 1);
        QVERIFY(!records.first().valueValid);
        QCOMPARE(records.first().quality, RuntimePointQuality::Bad);
        QVERIFY(records.first().origin.isEmpty());
        QVERIFY(records.first().errorCode.isEmpty());
        QVERIFY(records.first().errorText.isEmpty());
    }

    void freshDatabaseUsesCurrentSchemaAndLegacyWriteDefaults()
    {
        auto& dataManager = DataManager::instance();
        const QString databasePath = m_tempDir.path() + QStringLiteral("/fresh_v4.db");
        QVERIFY(dataManager.initialize(databasePath));
        QCOMPARE(dataManager.schemaVersion(), DataManager::CURRENT_SCHEMA_VERSION);

        QVERIFY(dataManager.logRuntimeData(QStringLiteral("fresh.point"), 1.5,
                                            QStringLiteral("bar")).success);
        const QList<RuntimeRecord> records = allRecords(QStringLiteral("fresh.point"));
        QCOMPARE(records.size(), 1);
        QVERIFY(records.first().origin.isEmpty());
        QVERIFY(records.first().errorCode.isEmpty());
        QVERIFY(records.first().errorText.isEmpty());
    }

    void loggerPersistsProvenanceAndErrorContext()
    {
        auto& dataManager = DataManager::instance();
        const QString databasePath = m_tempDir.path() + QStringLiteral("/provenance.db");
        QVERIFY(dataManager.initialize(databasePath));

        MonitorDataLogger logger;
        logger.setBatchSize(1000);

        Sample normal = makeSample(QStringLiteral("normal.source"),
                                    RuntimePointQuality::Good, true, 4.5);
        normal.metadata[QStringLiteral("source")] = QStringLiteral("simulation");

        Sample failed = makeSample(QStringLiteral("failed.source"),
                                   RuntimePointQuality::Bad, false);
        failed.metadata[QStringLiteral("source")] = QStringLiteral("backend_poll");
        failed.metadata[QStringLiteral("errorCodeName")] = QStringLiteral("ReadTimeout");
        failed.metadata[QStringLiteral("errorCode")] = 7;
        failed.metadata[QStringLiteral("error")] = QStringLiteral("read failed");
        failed.metadata[QStringLiteral("errorDetails")] = QStringLiteral("point=pressure");

        logger.enqueueSamples({normal, failed});
        logger.flush();

        const QList<RuntimeRecord> normalRecords = allRecords(QStringLiteral("normal.source"));
        QCOMPARE(normalRecords.size(), 1);
        QCOMPARE(normalRecords.first().origin, QStringLiteral("simulation"));
        QVERIFY(normalRecords.first().errorCode.isEmpty());
        QVERIFY(normalRecords.first().errorText.isEmpty());

        const QList<RuntimeRecord> failedRecords = allRecords(QStringLiteral("failed.source"));
        QCOMPARE(failedRecords.size(), 1);
        QVERIFY(!failedRecords.first().valueValid);
        QCOMPARE(failedRecords.first().quality, RuntimePointQuality::Bad);
        QCOMPARE(failedRecords.first().origin, QStringLiteral("backend_poll"));
        QCOMPARE(failedRecords.first().errorCode, QStringLiteral("ReadTimeout"));
        QCOMPARE(failedRecords.first().errorText,
                 QStringLiteral("read failed (details: point=pressure)"));

        MonitorManager& manager = MonitorManager::instance();
        const QList<Sample> normalHistory = manager.historyFromDatabase(
            QStringLiteral("normal.source"),
            QDateTime::currentDateTimeUtc().addDays(-1),
            QDateTime::currentDateTimeUtc().addDays(1));
        QCOMPARE(normalHistory.size(), 1);
        QVERIFY(!normalHistory.first().metadata.contains(QStringLiteral("error")));
        QVERIFY(!normalHistory.first().metadata.contains(QStringLiteral("errorCode")));

        const DatabaseHistoryPage page = manager.historyFromDatabasePage(
            QStringLiteral("failed.source"),
            QDateTime::currentDateTimeUtc().addDays(-1),
            QDateTime::currentDateTimeUtc().addDays(1),
            10);
        QCOMPARE(page.status, RuntimeHistoryPageStatus::Success);
        QCOMPARE(page.samples.size(), 1);
        const Sample& pageSample = page.samples.first();
        QVERIFY(!pageSample.valueValid);
        QCOMPARE(pageSample.quality, RuntimePointQuality::Bad);
        QCOMPARE(pageSample.metadata.value(QStringLiteral("origin")).toString(),
                 QStringLiteral("backend_poll"));
        QCOMPARE(pageSample.metadata.value(QStringLiteral("errorCode")).toString(),
                 QStringLiteral("ReadTimeout"));
        QCOMPARE(pageSample.metadata.value(QStringLiteral("error")).toString(),
                 QStringLiteral("read failed (details: point=pressure)"));
    }
};

QTEST_GUILESS_MAIN(MonitorDataLoggerTest)
#include "monitor_data_logger_test.moc"
