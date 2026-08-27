/**
 * @file DataManagerTest.cpp
 * @brief DataManager 单元测试实现
 */

#include "DataManagerTest.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QThread>
#include <QSignalSpy>
#include <QUuid>

namespace {

QStringList* capturedCriticalMessages = nullptr;

void captureCriticalMessage(QtMsgType type,
                            const QMessageLogContext&,
                            const QString& message)
{
    if (type == QtCriticalMsg && capturedCriticalMessages) {
        capturedCriticalMessages->append(message);
    }
}

bool executeTestSql(const QString& dbPath, const QStringList& statements)
{
    const QString connectionName = QStringLiteral("DataManagerTest_%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(dbPath);
        if (database.open()) {
            success = true;
            for (const QString& statement : statements) {
                QSqlQuery query(database);
                if (!query.exec(statement)) {
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

bool queryTestScalar(const QString& dbPath, const QString& statement, QVariant& value)
{
    const QString connectionName = QStringLiteral("DataManagerTest_%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(dbPath);
        if (database.open()) {
            QSqlQuery query(database);
            if (query.exec(statement) && query.next()) {
                value = query.value(0);
                success = true;
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

bool queryPlanUsesIndex(const QString& dbPath,
                        const QString& statement,
                        const QString& indexName)
{
    const QString connectionName = QStringLiteral("DataManagerTest_%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool usesIndex = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(dbPath);
        if (database.open()) {
            QSqlQuery query(database);
            if (query.exec(statement)) {
                while (query.next()) {
                    usesIndex = usesIndex || query.value(3).toString().contains(indexName);
                }
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return usesIndex;
}

} // namespace

void DataManagerTest::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
    m_dbPath = m_tempDir.path() + "/test.db";
}

void DataManagerTest::cleanupTestCase()
{
    // 清理
}

void DataManagerTest::init()
{
    // 每个测试前初始化数据库
    QVERIFY(DataManager::instance().initialize(m_dbPath));
}

void DataManagerTest::cleanup()
{
    // 每个测试后关闭数据库
    DataManager::instance().shutdown();
    
    // 删除测试数据库文件
    QFile::remove(m_dbPath);
}

// ============================================================================
// 初始化测试
// ============================================================================

void DataManagerTest::testInitializeAndShutdown()
{
    // init() 已经初始化，验证状态
    QVERIFY(DataManager::instance().isInitialized());
    
    DataManager::instance().shutdown();
    QVERIFY(!DataManager::instance().isInitialized());
    
    // 重新初始化
    QVERIFY(DataManager::instance().initialize(m_dbPath));
    QVERIFY(DataManager::instance().isInitialized());
}

void DataManagerTest::testSchemaVersion()
{
    QCOMPARE(DataManager::instance().schemaVersion(), DataManager::CURRENT_SCHEMA_VERSION);
}

void DataManagerTest::testReinitialize()
{
    // 写入一些数据
    DataManager::instance().logRuntimeData("testVar", 123.456);
    
    // 重新初始化（使用相同路径）
    QVERIFY(DataManager::instance().initialize(m_dbPath));
    
    // 数据应该仍然存在
    auto records = DataManager::instance().getLatestRecords("testVar", 1);
    QCOMPARE(records.size(), 1);
    QCOMPARE(records[0].value, 123.456);
}

void DataManagerTest::testDefaultDatabasePath()
{
    const QString expected = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)).filePath(QStringLiteral("platform.db"));
    QCOMPARE(DataManager::defaultDatabasePath(), expected);
}

void DataManagerTest::testDatabaseDirectoryFailure()
{
    DataManager::instance().shutdown();

    const QString blockerPath = m_tempDir.path() + QStringLiteral("/directory-blocker");
    QFile blocker(blockerPath);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();

    QSignalSpy errorSpy(&DataManager::instance(), &DataManager::databaseError);
    const QString dbPath = blockerPath + QStringLiteral("/platform.db");
    QVERIFY(!DataManager::instance().initialize(dbPath));
    QVERIFY(!DataManager::instance().isInitialized());
    QVERIFY(!errorSpy.isEmpty());
    QVERIFY(errorSpy.constLast().at(1).toString().contains(QStringLiteral("无法创建数据库目录")));
}

void DataManagerTest::testLegacyDatabaseMigration()
{
    DataManager::instance().shutdown();

    const QString legacyPath = m_tempDir.path() + QStringLiteral("/prefix/data/platform.db");
    const QString existingTargetPath = m_tempDir.path()
                                     + QStringLiteral("/user-data/existing/platform.db");
    const QString migratedTargetPath = m_tempDir.path()
                                     + QStringLiteral("/user-data/migrated/platform.db");
    QVERIFY(QDir().mkpath(QFileInfo(legacyPath).absolutePath()));

    QVERIFY(DataManager::instance().initialize(legacyPath));
    QVERIFY(DataManager::instance().logRuntimeData(QStringLiteral("legacy.value"), 7.0).success);
    DataManager::instance().shutdown();

    QVERIFY(DataManager::instance().initialize(existingTargetPath));
    QVERIFY(DataManager::instance().logRuntimeData(QStringLiteral("target.value"), 9.0).success);
    DataManager::instance().shutdown();

    QVERIFY(DataManager::instance().initialize(existingTargetPath, legacyPath));
    QCOMPARE(DataManager::instance().getLatestRecords(QStringLiteral("target.value"), 1).size(), 1);
    QCOMPARE(DataManager::instance().getLatestRecords(QStringLiteral("legacy.value"), 1).size(), 0);
    DataManager::instance().shutdown();

    QVERIFY(DataManager::instance().initialize(migratedTargetPath, legacyPath));
    const QList<RuntimeRecord> migratedRecords = DataManager::instance().getLatestRecords(
        QStringLiteral("legacy.value"), 1);
    QCOMPARE(migratedRecords.size(), 1);
    QCOMPARE(migratedRecords.first().value, 7.0);
    QVERIFY(QFile::exists(legacyPath));
}

void DataManagerTest::testLegacyMigrationFailureLeavesTargetAbsent()
{
    DataManager::instance().shutdown();

    const QString legacyPath = m_tempDir.path() + QStringLiteral("/broken-legacy.db");
    const QString targetPath = m_tempDir.path() + QStringLiteral("/migration-target/platform.db");
    QFile legacyFile(legacyPath);
    QVERIFY(legacyFile.open(QIODevice::WriteOnly));
    QVERIFY(legacyFile.write("not a SQLite database") > 0);
    legacyFile.close();

    QSignalSpy errorSpy(&DataManager::instance(), &DataManager::databaseError);
    QVERIFY(!DataManager::instance().initialize(targetPath, legacyPath));
    QVERIFY(!DataManager::instance().isInitialized());
    QVERIFY(!QFile::exists(targetPath));
    QVERIFY(QFile::exists(legacyPath));
    QVERIFY(!errorSpy.isEmpty());
    QVERIFY(errorSpy.constLast().at(1).toString().contains(QStringLiteral("数据库")));
}

void DataManagerTest::testInvalidSchemaVersionsRejected()
{
    DataManager::instance().shutdown();

    const QStringList invalidVersions = {
        QString::number(DataManager::CURRENT_SCHEMA_VERSION + 1),
        QStringLiteral("-1"),
        QStringLiteral("'not-an-integer'"),
        QStringLiteral("1.5"),
        QStringLiteral("9223372036854775808")
    };

    for (int i = 0; i < invalidVersions.size(); ++i) {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/invalid_schema_version_%1.db").arg(i);
        QFile::remove(dbPath);
        QVERIFY(executeTestSql(dbPath, {
            QStringLiteral("CREATE TABLE schema_version (id INTEGER PRIMARY KEY, version INTEGER NOT NULL, "
                           "updated_at DATETIME)"),
            QStringLiteral("INSERT INTO schema_version (id, version) VALUES (1, ")
                + invalidVersions[i] + QLatin1Char(')')
        }));

        QVERIFY(!DataManager::instance().initialize(dbPath));
        QVERIFY(!DataManager::instance().isInitialized());
        QCOMPARE(DataManager::instance().schemaVersion(), -1);

        if (i == 0) {
            QVariant storedVersion;
            QVERIFY(queryTestScalar(dbPath,
                                    QStringLiteral("SELECT version FROM schema_version WHERE id = 1"),
                                    storedVersion));
            QCOMPARE(storedVersion.toLongLong(),
                     static_cast<qlonglong>(DataManager::CURRENT_SCHEMA_VERSION + 1));

            QVERIFY(QFile::remove(dbPath));
            QVERIFY(DataManager::instance().initialize(dbPath));
            QVERIFY(DataManager::instance().isInitialized());
            DataManager::instance().shutdown();
        } else {
            QVERIFY(QFile::remove(dbPath));
        }
    }
}

void DataManagerTest::testInitializationFailureRecovery()
{
    const QString createFailurePath = m_tempDir.path() + "/version_table_create_failure.db";
    QFile::remove(createFailurePath);
    QVERIFY(executeTestSql(createFailurePath, {
        QStringLiteral("CREATE VIEW schema_version AS SELECT 1 AS version")
    }));

    QVERIFY(!DataManager::instance().initialize(createFailurePath));
    QVERIFY(!DataManager::instance().isInitialized());
    QCOMPARE(DataManager::instance().schemaVersion(), -1);
    QVERIFY(QFile::remove(createFailurePath));

    const QString dbPath = m_tempDir.path() + "/version_query_failure.db";
    QFile::remove(dbPath);
    QVERIFY(executeTestSql(dbPath, {
        QStringLiteral("CREATE TABLE schema_version (id INTEGER PRIMARY KEY)")
    }));

    QVERIFY(!DataManager::instance().initialize(dbPath));
    QVERIFY(!DataManager::instance().isInitialized());
    QCOMPARE(DataManager::instance().schemaVersion(), -1);

    QVERIFY(QFile::remove(dbPath));
    QVERIFY(DataManager::instance().initialize(dbPath));
    QVERIFY(DataManager::instance().isInitialized());
}

void DataManagerTest::testInitialRequiredIndexFailureRollsBack()
{
    const QString dbPath = m_tempDir.path() + "/initial_index_failure.db";
    QFile::remove(dbPath);
    QVERIFY(executeTestSql(dbPath, {
        QStringLiteral("CREATE TABLE idx_runtime_timestamp (id INTEGER)")
    }));

    QVERIFY(!DataManager::instance().initialize(dbPath));
    QVERIFY(!DataManager::instance().isInitialized());
    QCOMPARE(DataManager::instance().schemaVersion(), -1);

    QVariant value;
    QVERIFY(queryTestScalar(dbPath,
                            QStringLiteral("SELECT COUNT(*) FROM sqlite_master "
                                           "WHERE type = 'table' AND name = 'runtime_data'"),
                            value));
    QCOMPARE(value.toInt(), 0);
    QVERIFY(queryTestScalar(dbPath,
                            QStringLiteral("SELECT COUNT(*) FROM schema_version WHERE id = 1"),
                            value));
    QCOMPARE(value.toInt(), 0);
}

void DataManagerTest::testVersion2CompositeIndexFailureRollsBack()
{
    const QString dbPath = m_tempDir.path() + "/version2_index_failure.db";
    QFile::remove(dbPath);
    QVERIFY(executeTestSql(dbPath, {
        QStringLiteral("CREATE TABLE schema_version (id INTEGER PRIMARY KEY, version INTEGER NOT NULL, "
                       "updated_at DATETIME)"),
        QStringLiteral("INSERT INTO schema_version (id, version) VALUES (1, 1)"),
        QStringLiteral("CREATE TABLE runtime_data (id INTEGER PRIMARY KEY, timestamp DATETIME, "
                       "variable_name TEXT NOT NULL, value REAL, unit TEXT)"),
        QStringLiteral("CREATE TABLE idx_runtime_var_time (id INTEGER)")
    }));

    QVERIFY(!DataManager::instance().initialize(dbPath));
    QVERIFY(!DataManager::instance().isInitialized());
    QCOMPARE(DataManager::instance().schemaVersion(), -1);

    QVariant value;
    QVERIFY(queryTestScalar(dbPath,
                            QStringLiteral("SELECT version FROM schema_version WHERE id = 1"),
                            value));
    QCOMPARE(value.toInt(), 1);
    QVERIFY(queryTestScalar(dbPath,
                            QStringLiteral("SELECT COUNT(*) FROM sqlite_master "
                                           "WHERE type = 'table' AND name = 'data_summary'"),
                            value));
    QCOMPARE(value.toInt(), 0);
}

void DataManagerTest::testRequiredIndexesExist()
{
    const QStringList requiredIndexes = {
        QStringLiteral("idx_runtime_timestamp"),
        QStringLiteral("idx_runtime_variable"),
        QStringLiteral("idx_runtime_var_time"),
        QStringLiteral("idx_runtime_julianday_timestamp"),
        QStringLiteral("idx_logs_timestamp"),
        QStringLiteral("idx_logs_level"),
        QStringLiteral("idx_logs_level_time"),
        QStringLiteral("idx_logs_julianday_timestamp")
    };

    for (const QString& indexName : requiredIndexes) {
        QVariant value;
        QVERIFY(queryTestScalar(
            m_dbPath,
            QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type = 'index' AND name = '%1'")
                .arg(indexName),
            value));
        QCOMPARE(value.toInt(), 1);
    }
}

void DataManagerTest::testVersion5TimeIndexesMigrateAndAreUsable()
{
    DataManager::instance().shutdown();
    QFile::remove(m_dbPath);
    QVERIFY(executeTestSql(m_dbPath, {
        QStringLiteral("CREATE TABLE schema_version (id INTEGER PRIMARY KEY, version INTEGER NOT NULL, "
                       "updated_at DATETIME)"),
        QStringLiteral("INSERT INTO schema_version (id, version) VALUES (1, 4)"),
        QStringLiteral("CREATE TABLE runtime_data (id INTEGER PRIMARY KEY, timestamp DATETIME, "
                       "variable_name TEXT NOT NULL, value REAL, unit TEXT, quality TEXT NOT NULL, "
                       "value_valid INTEGER NOT NULL, origin TEXT NOT NULL, error_code TEXT NOT NULL, "
                       "error_text TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE system_logs (id INTEGER PRIMARY KEY, timestamp DATETIME, "
                       "level TEXT NOT NULL, module TEXT, message TEXT)")
    }));

    QVERIFY(DataManager::instance().initialize(m_dbPath));
    QCOMPARE(DataManager::instance().schemaVersion(), DataManager::CURRENT_SCHEMA_VERSION);
    QVERIFY(queryPlanUsesIndex(
        m_dbPath,
        QStringLiteral("EXPLAIN QUERY PLAN SELECT id FROM system_logs "
                       "WHERE julianday(timestamp) BETWEEN julianday('2020-01-01') "
                       "AND julianday('2030-01-01')"),
        QStringLiteral("idx_logs_julianday_timestamp")));
    QVERIFY(queryPlanUsesIndex(
        m_dbPath,
        QStringLiteral("EXPLAIN QUERY PLAN DELETE FROM runtime_data "
                       "WHERE julianday(timestamp) < julianday('2030-01-01')"),
        QStringLiteral("idx_runtime_julianday_timestamp")));
    QVERIFY(queryPlanUsesIndex(
        m_dbPath,
        QStringLiteral("EXPLAIN QUERY PLAN DELETE FROM system_logs "
                       "WHERE julianday(timestamp) < julianday('2030-01-01')"),
        QStringLiteral("idx_logs_julianday_timestamp")));
}

// ============================================================================
// 运行时数据测试
// ============================================================================

void DataManagerTest::testLogRuntimeData()
{
    QSignalSpy spy(&DataManager::instance(), &DataManager::dataUpdated);
    
    auto result = DataManager::instance().logRuntimeData("pressure", 150.5, "bar");
    
    QVERIFY(result.success);
    QCOMPARE(spy.count(), 1);
    
    // 验证缓存
    QVariant cached = DataManager::instance().getRuntimeValue("pressure");
    QCOMPARE(cached.toDouble(), 150.5);
}

void DataManagerTest::testLogRuntimeDataBatch()
{
    QList<QVariantMap> records;
    
    for (int i = 0; i < 100; ++i) {
        QVariantMap record;
        record["varName"] = QString("var_%1").arg(i);
        record["value"] = i * 1.5;
        record["unit"] = "unit";
        records.append(record);
    }
    
    auto result = DataManager::instance().logRuntimeDataBatch(records);
    
    QVERIFY(result.success);
    QCOMPARE(result.affectedRows, 100);
}

void DataManagerTest::testRuntimeDataTimestampsUseCanonicalUtc()
{
    const auto isCanonicalUtc = [](const QString& text) {
        const QDateTime parsed = QDateTime::fromString(text, Qt::ISODateWithMs);
        return parsed.isValid()
                && parsed.toUTC().toString(Qt::ISODateWithMs) == text;
    };

    QVERIFY(DataManager::instance().logRuntimeData(QStringLiteral("time.single"), 1.0).success);

    QVariant singleTimestamp;
    QVERIFY(queryTestScalar(m_dbPath,
                            QStringLiteral(
                                "SELECT timestamp FROM runtime_data "
                                "WHERE variable_name = 'time.single'"),
                            singleTimestamp));
    QVERIFY(isCanonicalUtc(singleTimestamp.toString()));

    QVariantMap missingTimestamp;
    missingTimestamp.insert(QStringLiteral("varName"), QStringLiteral("time.batch.missing"));
    missingTimestamp.insert(QStringLiteral("value"), 2.0);

    QVariantMap invalidTimestamp;
    invalidTimestamp.insert(QStringLiteral("varName"), QStringLiteral("time.batch.invalid"));
    invalidTimestamp.insert(QStringLiteral("value"), 3.0);
    invalidTimestamp.insert(QStringLiteral("timestamp"), QStringLiteral("not-a-timestamp"));

    QVERIFY(DataManager::instance().logRuntimeDataBatch(
            {missingTimestamp, invalidTimestamp}).success);

    QVariant batchTimestamp;
    QVERIFY(queryTestScalar(m_dbPath,
                            QStringLiteral(
                                "SELECT timestamp FROM runtime_data "
                                "WHERE variable_name = 'time.batch.missing'"),
                            batchTimestamp));
    QVERIFY(isCanonicalUtc(batchTimestamp.toString()));

    QVERIFY(queryTestScalar(m_dbPath,
                            QStringLiteral(
                                "SELECT timestamp FROM runtime_data "
                                "WHERE variable_name = 'time.batch.invalid'"),
                            batchTimestamp));
    QVERIFY(isCanonicalUtc(batchTimestamp.toString()));
}

void DataManagerTest::testSqlErrorLogRedactsBoundValues()
{
    const QString secretName = QStringLiteral("business-secret-token");
    const QString secretUnit = QStringLiteral("password-cookie-value");
    const QString secretValue = QStringLiteral("12.5");

    QVERIFY(executeTestSql(m_dbPath, {
        QStringLiteral(
            "CREATE TRIGGER reject_runtime_insert "
            "BEFORE INSERT ON runtime_data "
            "BEGIN SELECT RAISE(ABORT, 'forced insert failure'); END")
    }));

    QVariantMap record;
    record.insert(QStringLiteral("varName"), secretName);
    record.insert(QStringLiteral("value"), 12.5);
    record.insert(QStringLiteral("unit"), secretUnit);

    QStringList messages;
    capturedCriticalMessages = &messages;
    const QtMessageHandler previousHandler = qInstallMessageHandler(captureCriticalMessage);
    const QueryResult result = DataManager::instance().logRuntimeDataBatch({record});
    qInstallMessageHandler(previousHandler);
    capturedCriticalMessages = nullptr;

    QVERIFY(!result.success);
    QVERIFY(!messages.isEmpty());
    const QString message = messages.constLast();
    QVERIFY(message.contains(QStringLiteral("SQL 执行失败")));
    QVERIFY(message.contains(QStringLiteral("批量记录运行数据")));
    QVERIFY(message.contains(QStringLiteral("SQL 模板")));
    QVERIFY(message.contains(QStringLiteral("INSERT INTO runtime_data")));
    QVERIFY(message.contains(QStringLiteral(":name")));
    QVERIFY(message.contains(QStringLiteral("type=QString")));
    QVERIFY(message.contains(QStringLiteral("len=")));
    QVERIFY(!message.contains(secretName));
    QVERIFY(!message.contains(secretUnit));
    QVERIFY(!message.contains(secretValue));
}

void DataManagerTest::testRuntimeCache()
{
    DataManager::instance().logRuntimeData("var1", 1.0);
    DataManager::instance().logRuntimeData("var2", 2.0);
    DataManager::instance().logRuntimeData("var3", 3.0);
    
    QStringList names = DataManager::instance().getCachedVariableNames();
    QVERIFY(names.contains("var1"));
    QVERIFY(names.contains("var2"));
    QVERIFY(names.contains("var3"));
    
    DataManager::instance().clearRuntimeCache();
    
    names = DataManager::instance().getCachedVariableNames();
    QVERIFY(names.isEmpty());
}

// ============================================================================
// 历史数据查询测试
// ============================================================================

void DataManagerTest::testQueryHistory()
{
    QDateTime start = QDateTime::currentDateTime();
    
    // 写入数据
    for (int i = 0; i < 10; ++i) {
        DataManager::instance().logRuntimeData("historyVar", i * 10.0);
        QThread::msleep(10);
    }
    
    QDateTime end = QDateTime::currentDateTime();
    
    auto records = DataManager::instance().queryHistory("historyVar", start, end);
    
    QCOMPARE(records.size(), 10);
    
    // 验证顺序（应该按时间升序）
    for (int i = 1; i < records.size(); ++i) {
        QVERIFY(records[i].timestamp >= records[i-1].timestamp);
    }
}

void DataManagerTest::testGetLatestRecords()
{
    // 写入 20 条数据
    for (int i = 0; i < 20; ++i) {
        DataManager::instance().logRuntimeData("latestVar", i * 1.0);
    }
    
    // 获取最近 5 条
    auto records = DataManager::instance().getLatestRecords("latestVar", 5);
    
    QCOMPARE(records.size(), 5);
    
    // 应该是最新的 5 条（值为 15-19）
    QVERIFY(records[0].value >= 15.0);
}

void DataManagerTest::testGetRecordsSince()
{
    QDateTime before = QDateTime::currentDateTime();
    QThread::msleep(50);
    
    // 写入数据
    for (int i = 0; i < 5; ++i) {
        DataManager::instance().logRuntimeData("sinceVar", i * 1.0);
    }
    
    auto records = DataManager::instance().getRecordsSince("sinceVar", before);
    
    QCOMPARE(records.size(), 5);
}

void DataManagerTest::testGetStatistics()
{
    QDateTime start = QDateTime::currentDateTime();
    
    // 写入数据：1, 2, 3, 4, 5
    for (int i = 1; i <= 5; ++i) {
        DataManager::instance().logRuntimeData("statsVar", i * 1.0);
    }
    
    QDateTime end = QDateTime::currentDateTime();
    
    auto stats = DataManager::instance().getStatistics("statsVar", start, end);
    
    QVERIFY(stats.valid);
    QCOMPARE(stats.count, 5);
    QCOMPARE(stats.minValue, 1.0);
    QCOMPARE(stats.maxValue, 5.0);
    QCOMPARE(stats.avgValue, 3.0);
    QCOMPARE(stats.sumValue, 15.0);
}

// ============================================================================
// 系统日志测试
// ============================================================================

void DataManagerTest::testWriteLog()
{
    auto result = DataManager::instance().writeLog("INFO", "TestModule", "Test message");
    QVERIFY(result.success);
    QVERIFY(result.lastInsertId > 0);
}

void DataManagerTest::testQueryLogs()
{
    QDateTime start = QDateTime::currentDateTimeUtc();
    
    DataManager::instance().writeLog("INFO", "Module1", "Info message");
    DataManager::instance().writeLog("WARN", "Module2", "Warning message");
    DataManager::instance().writeLog("ERROR", "Module3", "Error message");
    
    QDateTime end = QDateTime::currentDateTimeUtc();
    
    // 查询所有日志
    auto logs = DataManager::instance().queryLogs(start, end);
    QCOMPARE(logs.size(), 3);
    
    // 按级别过滤
    logs = DataManager::instance().queryLogs(start, end, "ERROR");
    QCOMPARE(logs.size(), 1);
    QCOMPARE(logs[0].level, QString("ERROR"));
}

void DataManagerTest::testSystemLogTimestampsUseCanonicalUtcAndEquivalentRanges()
{
    const QString module = QStringLiteral("time.module");
    const QString errorMessage = QStringLiteral("canonical.error");
    QVERIFY(DataManager::instance().writeLog(QStringLiteral("ERROR"),
                                              module,
                                              errorMessage).success);
    QVERIFY(DataManager::instance().writeLog(QStringLiteral("INFO"),
                                              module,
                                              QStringLiteral("canonical.info")).success);

    QVariant rawTimestamp;
    QVERIFY(queryTestScalar(
        m_dbPath,
        QStringLiteral("SELECT timestamp FROM system_logs "
                       "WHERE module = 'time.module' AND message = 'canonical.error'"),
        rawTimestamp));

    const QString timestampText = rawTimestamp.toString();
    const QDateTime logged = QDateTime::fromString(timestampText, Qt::ISODateWithMs);
    QVERIFY(logged.isValid());
    QCOMPARE(timestampText, logged.toUTC().toString(Qt::ISODateWithMs));

    const QDateTime startUtc = logged.addSecs(-2);
    const QDateTime endUtc = logged.addSecs(2);
    const QDateTime startLocal = startUtc.toLocalTime();
    const QDateTime endLocal = endUtc.toLocalTime();
    const QDateTime startOffset = startUtc.toOffsetFromUtc(8 * 60 * 60);
    const QDateTime endOffset = endUtc.toOffsetFromUtc(8 * 60 * 60);

    const auto queryErrors = [](const QDateTime& start, const QDateTime& end) {
        return DataManager::instance().queryLogs(start, end, QStringLiteral("ERROR"), 20);
    };
    const QList<LogRecord> utcLogs = queryErrors(startUtc, endUtc);
    const QList<LogRecord> localLogs = queryErrors(startLocal, endLocal);
    const QList<LogRecord> offsetLogs = queryErrors(startOffset, endOffset);

    QCOMPARE(utcLogs.size(), 1);
    QCOMPARE(localLogs.size(), utcLogs.size());
    QCOMPARE(offsetLogs.size(), utcLogs.size());
    QCOMPARE(utcLogs.first().id, localLogs.first().id);
    QCOMPARE(utcLogs.first().id, offsetLogs.first().id);
    QCOMPARE(utcLogs.first().level, QStringLiteral("ERROR"));
    QCOMPARE(utcLogs.first().message, errorMessage);

    const QDateTime legacyInstant(QDate(2020, 1, 2), QTime(3, 4, 5), Qt::UTC);
    QVERIFY(executeTestSql(m_dbPath, {
        QStringLiteral(
            "INSERT INTO system_logs (timestamp, level, module, message) "
            "VALUES ('2020-01-02 03:04:05', 'ERROR', 'legacy.time', 'legacy log')")
    }));

    const QList<LogRecord> legacyUtcLogs = queryErrors(
        legacyInstant.addMSecs(-500), legacyInstant.addMSecs(500));
    const QList<LogRecord> legacyLocalLogs = queryErrors(
        legacyInstant.addMSecs(-500).toLocalTime(),
        legacyInstant.addMSecs(500).toLocalTime());
    QCOMPARE(legacyUtcLogs.size(), 1);
    QCOMPARE(legacyLocalLogs.size(), legacyUtcLogs.size());
    QCOMPARE(legacyUtcLogs.first().message, QStringLiteral("legacy log"));
}

// ============================================================================
// 数据维护测试
// ============================================================================

void DataManagerTest::testCleanupOldData()
{
    // 写入一些数据
    for (int i = 0; i < 10; ++i) {
        DataManager::instance().logRuntimeData("cleanupVar", i * 1.0);
    }
    
    // 清理 0 天前的数据（即所有数据）
    // 注意：由于数据刚写入，时间戳是当前时间，所以不会被清理
    int deleted = DataManager::instance().cleanupOldData(1);
    
    // 验证数据没有被删除（因为数据是今天的）
    auto records = DataManager::instance().getLatestRecords("cleanupVar", 100);
    QCOMPARE(records.size(), 10);
    QVERIFY(deleted >= 0);
}

void DataManagerTest::testCleanupOldDataIsTransactionalAndUsesUtc()
{
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    const QString oldTimestamp = nowUtc.addDays(-1).addSecs(-1).toString(Qt::ISODateWithMs);
    const QString retainedTimestamp = nowUtc.addDays(-1).addSecs(1).toString(Qt::ISODateWithMs);

    QVERIFY(executeTestSql(m_dbPath, {
        QStringLiteral(
            "INSERT INTO runtime_data "
            "(timestamp, variable_name, value, unit, quality, value_valid, origin, error_code, error_text) "
            "VALUES ('%1', 'old.runtime', 1.0, 'bar', 'Good', 1, '', '', '')")
            .arg(oldTimestamp),
        QStringLiteral(
            "INSERT INTO runtime_data "
            "(timestamp, variable_name, value, unit, quality, value_valid, origin, error_code, error_text) "
            "VALUES ('%1', 'retained.runtime', 2.0, 'bar', 'Good', 1, '', '', '')")
            .arg(retainedTimestamp),
        QStringLiteral(
            "INSERT INTO system_logs (timestamp, level, module, message) "
            "VALUES ('%1', 'INFO', 'old.module', 'old log')")
            .arg(oldTimestamp),
        QStringLiteral(
            "INSERT INTO system_logs (timestamp, level, module, message) "
            "VALUES ('%1', 'INFO', 'retained.module', 'retained log')")
            .arg(retainedTimestamp),
        QStringLiteral(
            "CREATE TRIGGER reject_cleanup_log_delete "
            "BEFORE DELETE ON system_logs "
            "BEGIN SELECT RAISE(ABORT, 'reject cleanup'); END")
    }));

    QCOMPARE(DataManager::instance().cleanupOldData(1), -1);

    QVariant value;
    QVERIFY(queryTestScalar(m_dbPath,
                            QStringLiteral("SELECT COUNT(*) FROM runtime_data"),
                            value));
    QCOMPARE(value.toInt(), 2);
    QVERIFY(queryTestScalar(m_dbPath,
                            QStringLiteral("SELECT COUNT(*) FROM system_logs"),
                            value));
    QCOMPARE(value.toInt(), 2);

    QVERIFY(executeTestSql(m_dbPath, {
        QStringLiteral("DROP TRIGGER reject_cleanup_log_delete")
    }));

    QCOMPARE(DataManager::instance().cleanupOldData(1), 2);
    QVERIFY(queryTestScalar(m_dbPath,
                            QStringLiteral("SELECT COUNT(*) FROM runtime_data WHERE variable_name = 'old.runtime'"),
                            value));
    QCOMPARE(value.toInt(), 0);
    QVERIFY(queryTestScalar(m_dbPath,
                            QStringLiteral("SELECT COUNT(*) FROM runtime_data WHERE variable_name = 'retained.runtime'"),
                            value));
    QCOMPARE(value.toInt(), 1);
    QVERIFY(queryTestScalar(m_dbPath,
                            QStringLiteral("SELECT COUNT(*) FROM system_logs WHERE module = 'old.module'"),
                            value));
    QCOMPARE(value.toInt(), 0);
    QVERIFY(queryTestScalar(m_dbPath,
                            QStringLiteral("SELECT COUNT(*) FROM system_logs WHERE module = 'retained.module'"),
                            value));
    QCOMPARE(value.toInt(), 1);

    QCOMPARE(DataManager::instance().cleanupOldData(0), -1);
}

void DataManagerTest::testOptimizeDatabase()
{
    auto result = DataManager::instance().optimizeDatabase();
    QVERIFY(result.success);
}

QTEST_MAIN(DataManagerTest)
