/**
 * @file data_manager_test.cpp
 * @brief DataManager 专项测试
 *
 * 测试内容：
 * - 初始化和关闭
 * - 运行时数据记录
 * - 缓存操作
 * - 重新初始化
 * - 异常处理
 */

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QtGlobal>
#include "core/DataManager.h"

void printTestHeader(const QString& testName)
{
    qInfo() << "";
    qInfo() << "========================================";
    qInfo() << "TEST:" << testName;
    qInfo() << "========================================";
}

void printTestResult(const QString& testName, bool passed)
{
    if (passed) {
        qInfo() << "[PASS]" << testName;
    } else {
        qCritical() << "[FAIL]" << testName;
    }
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    
    qInfo() << "========================================";
    qInfo() << "DataManager Test Suite";
    qInfo() << "========================================";

    auto& dataManager = DataManager::instance();
    int testsPassed = 0;
    int testsFailed = 0;

    QString dbPath1 = QDir::temp().absoluteFilePath("data_manager_test_1.db");
    QString dbPath2 = QDir::temp().absoluteFilePath("data_manager_test_2.db");
    
    // 清理旧的测试数据库
    QFile::remove(dbPath1);
    QFile::remove(dbPath2);

    // ===== Test 1: 初始化 =====
    printTestHeader("Initialization");
    
    bool test1 = dataManager.initialize(dbPath1);
    printTestResult("Initialize with valid path", test1);
    if (test1) testsPassed++; else testsFailed++;
    
    bool test2 = dataManager.isInitialized();
    printTestResult("isInitialized() returns true", test2);
    if (test2) testsPassed++; else testsFailed++;
    
    bool test3 = QFile::exists(dbPath1);
    printTestResult("Database file created", test3);
    if (test3) testsPassed++; else testsFailed++;

    // ===== Test 2: 运行时数据记录 =====
    printTestHeader("Runtime Data Logging");
    
    dataManager.logRuntimeData("pressure", 10.5, "bar");
    dataManager.logRuntimeData("temperature", 25.0, "°C");
    dataManager.logRuntimeData("flow", 100.0, "L/min");
    
    QVariant pressure = dataManager.getRuntimeValue("pressure");
    bool test4 = (pressure.isValid() && qAbs(pressure.toDouble() - 10.5) < 0.001);
    printTestResult("Read pressure value", test4);
    if (test4) testsPassed++; else testsFailed++;
    
    QVariant temperature = dataManager.getRuntimeValue("temperature");
    bool test5 = (temperature.isValid() && qAbs(temperature.toDouble() - 25.0) < 0.001);
    printTestResult("Read temperature value", test5);
    if (test5) testsPassed++; else testsFailed++;
    
    // 更新值
    dataManager.logRuntimeData("pressure", 15.0, "bar");
    pressure = dataManager.getRuntimeValue("pressure");
    bool test6 = (pressure.isValid() && qAbs(pressure.toDouble() - 15.0) < 0.001);
    printTestResult("Update and read new value", test6);
    if (test6) testsPassed++; else testsFailed++;

    // ===== Test 3: 缓存操作 =====
    printTestHeader("Cache Operations");
    
    dataManager.clearRuntimeCache();
    QVariant clearedValue = dataManager.getRuntimeValue("pressure");
    bool test7 = !clearedValue.isValid();
    printTestResult("Cache cleared", test7);
    if (test7) testsPassed++; else testsFailed++;

    // ===== Test 4: 系统日志 =====
    printTestHeader("System Logging");
    
    dataManager.writeLog("INFO", "TestModule", "Test message 1");
    dataManager.writeLog("WARN", "TestModule", "Test message 2");
    dataManager.writeLog("ERROR", "TestModule", "Test message 3");
    
    // 日志写入应该不会抛出异常
    bool test8 = true;
    printTestResult("Write logs without exception", test8);
    if (test8) testsPassed++; else testsFailed++;

    // ===== Test 5: Shutdown =====
    printTestHeader("Shutdown");
    
    dataManager.shutdown();
    
    bool test9 = !dataManager.isInitialized();
    printTestResult("isInitialized() returns false after shutdown", test9);
    if (test9) testsPassed++; else testsFailed++;

    // ===== Test 6: Shutdown 后操作 =====
    printTestHeader("Post-Shutdown Behavior");
    
    // Shutdown 后记录数据应该不会崩溃
    dataManager.logRuntimeData("test", 1.0, "unit");
    bool test10 = true;  // 如果没崩溃就算通过
    printTestResult("logRuntimeData after shutdown (no crash)", test10);
    if (test10) testsPassed++; else testsFailed++;
    
    QVariant postShutdown = dataManager.getRuntimeValue("test");
    bool test11 = !postShutdown.isValid();  // 应该返回无效值
    printTestResult("getRuntimeValue after shutdown returns invalid", test11);
    if (test11) testsPassed++; else testsFailed++;

    // ===== Test 7: 重新初始化 =====
    printTestHeader("Reinitialization");
    
    bool test12 = dataManager.initialize(dbPath2);
    printTestResult("Reinitialize with different path", test12);
    if (test12) testsPassed++; else testsFailed++;
    
    bool test13 = dataManager.isInitialized();
    printTestResult("isInitialized() returns true after reinit", test13);
    if (test13) testsPassed++; else testsFailed++;
    
    bool test14 = QFile::exists(dbPath2);
    printTestResult("New database file created", test14);
    if (test14) testsPassed++; else testsFailed++;
    
    // 写入新数据
    dataManager.logRuntimeData("newData", 42.0, "units");
    QVariant newData = dataManager.getRuntimeValue("newData");
    bool test15 = (newData.isValid() && qAbs(newData.toDouble() - 42.0) < 0.001);
    printTestResult("Write and read after reinit", test15);
    if (test15) testsPassed++; else testsFailed++;

    // ===== Test 8: 初始化到同一路径 =====
    printTestHeader("Reinit to Same Path");
    
    // 先关闭
    dataManager.shutdown();
    
    // 再次初始化到第一个路径
    bool test16 = dataManager.initialize(dbPath1);
    printTestResult("Reinitialize to first path", test16);
    if (test16) testsPassed++; else testsFailed++;
    
    // 验证可以正常工作
    dataManager.logRuntimeData("verify", 99.0, "test");
    QVariant verify = dataManager.getRuntimeValue("verify");
    bool test17 = (verify.isValid() && qAbs(verify.toDouble() - 99.0) < 0.001);
    printTestResult("Operations work after reinit to same path", test17);
    if (test17) testsPassed++; else testsFailed++;

    // ===== 最终清理 =====
    dataManager.shutdown();

    // ===== 测试总结 =====
    qInfo() << "";
    qInfo() << "========================================";
    qInfo() << "TEST SUMMARY";
    qInfo() << "========================================";
    qInfo() << "Passed:" << testsPassed;
    qInfo() << "Failed:" << testsFailed;
    qInfo() << "Total:" << (testsPassed + testsFailed);
    
    if (testsFailed == 0) {
        qInfo() << "";
        qInfo() << "All tests PASSED!";
    } else {
        qCritical() << "";
        qCritical() << "Some tests FAILED!";
    }

    // 清理测试数据库文件
    QFile::remove(dbPath1);
    QFile::remove(dbPath2);

    return testsFailed;
}
