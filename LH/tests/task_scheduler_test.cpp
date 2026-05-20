/**
 * @file task_scheduler_test.cpp
 * @brief TaskScheduler 专项测试
 *
 * 测试内容：
 * - 任务注册与注销
 * - 多优先级任务调度
 * - start/stop 多次调用
 * - shutdown 后的状态验证
 * - 任务执行计数
 */

#include <QCoreApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QtGlobal>
#include <atomic>
#include "core/TaskScheduler.h"

// 全局执行计数器
static std::atomic<int> g_task1Count{0};
static std::atomic<int> g_task2Count{0};
static std::atomic<int> g_task3Count{0};

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
    qInfo() << "TaskScheduler Test Suite";
    qInfo() << "========================================";

    auto& scheduler = TaskScheduler::instance();
    int testsPassed = 0;
    int testsFailed = 0;

    // ===== Test 1: 基本注册和启动 =====
    printTestHeader("Basic Registration and Start");
    
    scheduler.registerTask("task1", 10, 50, []() {
        g_task1Count++;
    });
    
    scheduler.registerTask("task2", 20, 100, []() {
        g_task2Count++;
    });
    
    scheduler.registerTask("task3", 30, 200, []() {
        g_task3Count++;
    });
    
    bool test1 = (scheduler.taskCount() == 3);
    printTestResult("Registered 3 tasks", test1);
    if (test1) testsPassed++; else testsFailed++;
    
    bool test2 = scheduler.hasTask("task1") && 
                 scheduler.hasTask("task2") && 
                 scheduler.hasTask("task3");
    printTestResult("All tasks exist", test2);
    if (test2) testsPassed++; else testsFailed++;

    // ===== Test 2: 启动调度器 =====
    printTestHeader("Start Scheduler");
    
    scheduler.start();
    bool test3 = scheduler.isRunning();
    printTestResult("Scheduler is running", test3);
    if (test3) testsPassed++; else testsFailed++;
    
    // 重复调用 start 应该安全
    scheduler.start();
    scheduler.start();
    bool test4 = scheduler.isRunning();
    printTestResult("Multiple start() calls safe", test4);
    if (test4) testsPassed++; else testsFailed++;

    // ===== 运行一段时间后检查执行次数 =====
    QTimer::singleShot(500, [&]() {
        
        // ===== Test 3: 检查执行次数 =====
        printTestHeader("Execution Count Check");
        
        int count1 = g_task1Count.load();
        int count2 = g_task2Count.load();
        int count3 = g_task3Count.load();
        
        qInfo() << "Task1 (50ms period) executed:" << count1 << "times";
        qInfo() << "Task2 (100ms period) executed:" << count2 << "times";
        qInfo() << "Task3 (200ms period) executed:" << count3 << "times";
        
        // task1 应该比 task2 执行更多次
        bool test5 = (count1 > count2);
        printTestResult("Higher frequency task executes more", test5);
        if (test5) testsPassed++; else testsFailed++;
        
        // 所有任务都应该执行过
        bool test6 = (count1 > 0 && count2 > 0 && count3 > 0);
        printTestResult("All tasks executed at least once", test6);
        if (test6) testsPassed++; else testsFailed++;

        // ===== Test 4: Stop 和重新 Start =====
        printTestHeader("Stop and Restart");
        
        scheduler.stop();
        bool test7 = !scheduler.isRunning();
        printTestResult("Scheduler stopped", test7);
        if (test7) testsPassed++; else testsFailed++;
        
        // 记录停止时的计数
        int countBeforeRestart = g_task1Count.load();
        
        // 等待一小段时间，确认没有新执行
        QTimer::singleShot(100, [&, countBeforeRestart]() {
            bool test8 = (g_task1Count.load() == countBeforeRestart);
            printTestResult("No execution after stop", test8);
            if (test8) testsPassed++; else testsFailed++;
            
            // 重新启动
            scheduler.start();
            bool test9 = scheduler.isRunning();
            printTestResult("Scheduler restarted", test9);
            if (test9) testsPassed++; else testsFailed++;
            
            // ===== Test 5: 动态注销任务 =====
            QTimer::singleShot(200, [&]() {
                printTestHeader("Dynamic Task Unregistration");
                
                scheduler.unregisterTask("task2");
                bool test10 = !scheduler.hasTask("task2");
                printTestResult("Task2 unregistered", test10);
                if (test10) testsPassed++; else testsFailed++;
                
                bool test11 = (scheduler.taskCount() == 2);
                printTestResult("Task count is 2", test11);
                if (test11) testsPassed++; else testsFailed++;

                // ===== Test 6: Shutdown =====
                printTestHeader("Shutdown");
                
                scheduler.shutdown();
                
                bool test12 = scheduler.isShutdown();
                printTestResult("Scheduler is shutdown", test12);
                if (test12) testsPassed++; else testsFailed++;
                
                bool test13 = !scheduler.isRunning();
                printTestResult("Scheduler not running after shutdown", test13);
                if (test13) testsPassed++; else testsFailed++;
                
                bool test14 = (scheduler.taskCount() == 0);
                printTestResult("All tasks cleared after shutdown", test14);
                if (test14) testsPassed++; else testsFailed++;
                
                // ===== Test 7: Shutdown 后无法注册新任务 =====
                printTestHeader("Post-Shutdown Behavior");
                
                scheduler.registerTask("newTask", 10, 100, []() {});
                bool test15 = (scheduler.taskCount() == 0);  // 应该无法注册
                printTestResult("Cannot register after shutdown", test15);
                if (test15) testsPassed++; else testsFailed++;
                
                scheduler.start();
                bool test16 = !scheduler.isRunning();  // 应该无法启动
                printTestResult("Cannot start after shutdown", test16);
                if (test16) testsPassed++; else testsFailed++;

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
                
                QCoreApplication::exit(testsFailed);
            });
        });
    });

    return app.exec();
}
