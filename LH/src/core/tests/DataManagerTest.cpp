/**
 * @file DataManagerTest.cpp
 * @brief DataManager 单元测试实现
 */

#include "DataManagerTest.h"
#include <QThread>
#include <QSignalSpy>

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
    QDateTime start = QDateTime::currentDateTime();
    
    DataManager::instance().writeLog("INFO", "Module1", "Info message");
    DataManager::instance().writeLog("WARN", "Module2", "Warning message");
    DataManager::instance().writeLog("ERROR", "Module3", "Error message");
    
    QDateTime end = QDateTime::currentDateTime();
    
    // 查询所有日志
    auto logs = DataManager::instance().queryLogs(start, end);
    QCOMPARE(logs.size(), 3);
    
    // 按级别过滤
    logs = DataManager::instance().queryLogs(start, end, "ERROR");
    QCOMPARE(logs.size(), 1);
    QCOMPARE(logs[0].level, QString("ERROR"));
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
}

void DataManagerTest::testOptimizeDatabase()
{
    auto result = DataManager::instance().optimizeDatabase();
    QVERIFY(result.success);
}

// QTEST_MAIN(DataManagerTest)
