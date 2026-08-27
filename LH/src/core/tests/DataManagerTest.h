/**
 * @file DataManagerTest.h
 * @brief DataManager 单元测试
 */

#ifndef DATAMANAGER_TEST_H
#define DATAMANAGER_TEST_H

#include <QObject>
#include <QTest>
#include <QTemporaryDir>
#include "DataManager.h"

class DataManagerTest : public QObject
{
    Q_OBJECT
    
private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // ===== 初始化测试 =====
    void testInitializeAndShutdown();
    void testSchemaVersion();
    void testReinitialize();
    void testDefaultDatabasePath();
    void testDatabaseDirectoryFailure();
    void testLegacyDatabaseMigration();
    void testLegacyMigrationFailureLeavesTargetAbsent();
    void testInvalidSchemaVersionsRejected();
    void testInitializationFailureRecovery();
    void testInitialRequiredIndexFailureRollsBack();
    void testVersion2CompositeIndexFailureRollsBack();
    void testRequiredIndexesExist();
    void testVersion5TimeIndexesMigrateAndAreUsable();
    
    // ===== 运行时数据测试 =====
    void testLogRuntimeData();
    void testLogRuntimeDataBatch();
    void testRuntimeDataTimestampsUseCanonicalUtc();
    void testSqlErrorLogRedactsBoundValues();
    void testRuntimeCache();
    
    // ===== 历史数据查询测试 =====
    void testQueryHistory();
    void testGetLatestRecords();
    void testGetRecordsSince();
    void testGetStatistics();
    
    // ===== 系统日志测试 =====
    void testWriteLog();
    void testQueryLogs();
    void testSystemLogTimestampsUseCanonicalUtcAndEquivalentRanges();
    
    // ===== 数据维护测试 =====
    void testCleanupOldData();
    void testCleanupOldDataIsTransactionalAndUsesUtc();
    void testOptimizeDatabase();
    
private:
    QTemporaryDir m_tempDir;
    QString m_dbPath;
};

#endif // DATAMANAGER_TEST_H
