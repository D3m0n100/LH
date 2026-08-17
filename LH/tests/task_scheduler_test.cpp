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
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>
#include <QtGlobal>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
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

void waitForEvents(int durationMs)
{
    QEventLoop loop;
    QTimer::singleShot(durationMs, &loop, &QEventLoop::quit);
    loop.exec();
}

void unregisterAllTasks(TaskScheduler& scheduler)
{
    const QStringList names = scheduler.taskNames();
    for (const QString& name : names) {
        scheduler.unregisterTask(name);
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

                auto recordTest = [&](const QString& name, bool passed) {
                    printTestResult(name, passed);
                    if (passed) {
                        testsPassed++;
                    } else {
                        testsFailed++;
                    }
                };

                scheduler.stop();
                unregisterAllTasks(scheduler);

                // ===== Test 6: 同一 tick 的优先级和 FixedDelay 完成基线 =====
                printTestHeader("Priority Queue Time and FixedDelay Completion");
                QVector<QString> order;
                QVector<qint64> lowStarts;
                QElapsedTimer priorityTimer;
                bool highFirstRun = true;
                priorityTimer.start();
                scheduler.registerTask("priorityHigh", 1, 30, [&]() {
                    order.append(QStringLiteral("high"));
                    if (highFirstRun) {
                        highFirstRun = false;
                        std::this_thread::sleep_for(std::chrono::milliseconds(60));
                    }
                });
                scheduler.registerTask("priorityLow", 2, 30, [&]() {
                    order.append(QStringLiteral("low"));
                    lowStarts.append(priorityTimer.elapsed());
                });
                scheduler.start();
                waitForEvents(260);
                scheduler.stop();
                const bool priorityOrder = order.size() >= 2
                        && order.at(0) == QStringLiteral("high")
                        && order.at(1) == QStringLiteral("low");
                recordTest("Same-tick tasks preserve priority order", priorityOrder);
                const bool queuedDelayIncluded = lowStarts.size() >= 2
                        && lowStarts.at(1) - lowStarts.at(0) >= 20;
                recordTest("FixedDelay includes queued execution time", queuedDelayIncluded);
                unregisterAllTasks(scheduler);

                // ===== Test 7: 单个慢 FixedDelay =====
                printTestHeader("Slow FixedDelay Uses Completion Time");
                QVector<qint64> slowStarts;
                QElapsedTimer slowTimer;
                slowTimer.start();
                scheduler.registerTask("slowFixedDelay", 1, 30, [&]() {
                    slowStarts.append(slowTimer.elapsed());
                    std::this_thread::sleep_for(std::chrono::milliseconds(45));
                });
                scheduler.start();
                waitForEvents(280);
                scheduler.stop();
                const bool slowDelay = slowStarts.size() >= 2
                        && slowStarts.at(1) - slowStarts.at(0) >= 55;
                recordTest("Slow FixedDelay waits period after completion", slowDelay);
                unregisterAllTasks(scheduler);

                // ===== Test 8: FixedRate 计划推进与完成时刻重同步 =====
                printTestHeader("FixedRate Schedule Progression");
                QVector<qint64> rateStarts;
                QElapsedTimer rateTimer;
                bool firstRateRun = true;
                rateTimer.start();
                scheduler.registerTask("fixedRate", 1, 25, [&]() {
                    rateStarts.append(rateTimer.elapsed());
                    if (firstRateRun) {
                        firstRateRun = false;
                        std::this_thread::sleep_for(std::chrono::milliseconds(70));
                    }
                }, ScheduleMode::FixedRate);
                scheduler.start();
                waitForEvents(260);
                scheduler.stop();
                const bool rateResynced = rateStarts.size() >= 2
                        && rateStarts.at(1) - rateStarts.at(0) >= 65;
                recordTest("FixedRate resynchronizes from real completion", rateResynced);
                const bool rateContinuesByPlan = rateStarts.size() >= 3
                        && rateStarts.at(2) - rateStarts.at(1) >= 15
                        && rateStarts.at(2) - rateStarts.at(1) <= 60;
                recordTest("FixedRate continues on scheduled periods", rateContinuesByPlan);
                unregisterAllTasks(scheduler);

                // ===== Test 9: 异常后的 FixedDelay 基线 =====
                printTestHeader("Exception FixedDelay Completion Time");
                QVector<qint64> exceptionStarts;
                QElapsedTimer exceptionTimer;
                bool throwOnce = true;
                exceptionTimer.start();
                scheduler.registerTask("exceptionFixedDelay", 1, 25, [&]() {
                    exceptionStarts.append(exceptionTimer.elapsed());
                    if (throwOnce) {
                        throwOnce = false;
                        std::this_thread::sleep_for(std::chrono::milliseconds(35));
                        throw std::runtime_error("expected scheduler test error");
                    }
                });
                scheduler.start();
                waitForEvents(180);
                scheduler.stop();
                const bool exceptionDelay = exceptionStarts.size() >= 2
                        && exceptionStarts.at(1) - exceptionStarts.at(0) >= 45;
                recordTest("Exception FixedDelay waits from return time", exceptionDelay);
                unregisterAllTasks(scheduler);

                // ===== Test 10: stop/restart 基线 =====
                printTestHeader("Stop and Restart Baseline");
                int restartCount = 0;
                scheduler.registerTask("restartBaseline", 1, 25, [&]() { ++restartCount; });
                scheduler.start();
                waitForEvents(80);
                scheduler.stop();
                const int countAtStop = restartCount;
                waitForEvents(60);
                const bool stoppedStable = restartCount == countAtStop;
                recordTest("Stop prevents further execution", stoppedStable);
                scheduler.start();
                waitForEvents(80);
                scheduler.stop();
                recordTest("Restart resets and resumes execution", restartCount > countAtStop);
                unregisterAllTasks(scheduler);

                // ===== Test 11: 执行器重入时的任务身份保护 =====
                printTestHeader("Task Generation Reentrancy Protection");
                int unregisterRuns = 0;
                scheduler.registerTask("unregisterSelf", 1, 10, [&]() {
                    ++unregisterRuns;
                    scheduler.unregisterTask("unregisterSelf");
                });
                scheduler.start();
                waitForEvents(80);
                scheduler.stop();
                const TaskStats unregisterStats = scheduler.getTaskStats("unregisterSelf");
                recordTest("Self-unregister does not recreate statistics",
                           unregisterRuns == 1 && !scheduler.hasTask("unregisterSelf")
                               && unregisterStats.execCount == 0);
                unregisterAllTasks(scheduler);

                int oldReplacementRuns = 0;
                int newReplacementRuns = 0;
                scheduler.registerTask("replaceSelf", 1, 10, [&]() {
                    ++oldReplacementRuns;
                    scheduler.registerTask("replaceSelf", 1, 10, [&]() {
                        ++newReplacementRuns;
                    });
                });
                scheduler.start();
                waitForEvents(100);
                scheduler.stop();
                const TaskStats replacementStats = scheduler.getTaskStats("replaceSelf");
                recordTest("Same-name replacement rejects old completion",
                           oldReplacementRuns == 1 && newReplacementRuns > 0
                               && replacementStats.execCount == static_cast<quint64>(newReplacementRuns));
                unregisterAllTasks(scheduler);

                int disabledRuns = 0;
                scheduler.registerTask("disableDuringRun", 1, 10, [&]() {
                    ++disabledRuns;
                    scheduler.setTaskEnabled("disableDuringRun", false);
                });
                scheduler.start();
                waitForEvents(80);
                scheduler.stop();
                recordTest("Disable during execution remains disabled", disabledRuns == 1
                           && !scheduler.isTaskEnabled("disableDuringRun"));
                unregisterAllTasks(scheduler);

                // ===== Test 12: 运行中动态注册的 FixedDelay 基线 =====
                printTestHeader("Dynamic FixedDelay Registration Baseline");
                QVector<qint64> dynamicStarts;
                QElapsedTimer dynamicTimer;
                dynamicTimer.start();
                qint64 registeredAt = -1;
                scheduler.start();
                waitForEvents(140);
                registeredAt = dynamicTimer.elapsed();
                scheduler.registerTask("dynamicFixedDelay", 1, 100, [&]() {
                    dynamicStarts.append(dynamicTimer.elapsed());
                });
                waitForEvents(70);
                const bool dynamicNotEarly = dynamicStarts.isEmpty();
                waitForEvents(80);
                scheduler.stop();
                const bool dynamicWaitedFullPeriod = registeredAt >= 0
                        && !dynamicStarts.isEmpty()
                        && dynamicStarts.first() - registeredAt >= 75;
                recordTest("Dynamic FixedDelay does not execute early", dynamicNotEarly);
                recordTest("Dynamic FixedDelay waits from registration",
                           dynamicWaitedFullPeriod);
                unregisterAllTasks(scheduler);

                // ===== Test 13: 到期快照的任务代次保护 =====
                printTestHeader("Due Snapshot Generation Protection");
                QVector<qint64> replacementStarts;
                QElapsedTimer replacementTimer;
                replacementTimer.start();
                qint64 replacementAt = -1;
                int oldDueRuns = 0;
                bool replacedDueTask = false;
                scheduler.registerTask("dueSnapshotA", 1, 40, [&]() {
                    if (replacedDueTask) {
                        return;
                    }
                    replacedDueTask = true;
                    replacementAt = replacementTimer.elapsed();
                    scheduler.registerTask("dueSnapshotB", 2, 150, [&]() {
                        replacementStarts.append(replacementTimer.elapsed());
                    });
                });
                scheduler.registerTask("dueSnapshotB", 2, 40, [&]() {
                    ++oldDueRuns;
                });
                scheduler.start();
                waitForEvents(280);
                scheduler.stop();
                const bool replacementWaited = replacementAt >= 0
                        && !replacementStarts.isEmpty()
                        && replacementStarts.first() - replacementAt >= 120;
                recordTest("Replaced task waits after old due snapshot", replacementWaited);
                recordTest("Old due task executor is skipped", oldDueRuns == 0);
                const TaskStats replacementDueStats = scheduler.getTaskStats("dueSnapshotB");
                recordTest("Replacement statistics match new executions",
                           replacementDueStats.execCount
                               == static_cast<quint64>(replacementStarts.size()));
                unregisterAllTasks(scheduler);

                QVector<qint64> rebuiltStarts;
                QElapsedTimer rebuiltTimer;
                rebuiltTimer.start();
                qint64 rebuiltAt = -1;
                int oldRebuiltRuns = 0;
                bool rebuiltDueTask = false;
                scheduler.registerTask("rebuildSnapshotA", 1, 40, [&]() {
                    if (rebuiltDueTask) {
                        return;
                    }
                    rebuiltDueTask = true;
                    rebuiltAt = rebuiltTimer.elapsed();
                    scheduler.unregisterTask("rebuildSnapshotB");
                    scheduler.registerTask("rebuildSnapshotB", 2, 150, [&]() {
                        rebuiltStarts.append(rebuiltTimer.elapsed());
                    });
                });
                scheduler.registerTask("rebuildSnapshotB", 2, 40, [&]() {
                    ++oldRebuiltRuns;
                });
                scheduler.start();
                waitForEvents(280);
                scheduler.stop();
                const bool rebuildWaited = rebuiltAt >= 0
                        && !rebuiltStarts.isEmpty()
                        && rebuiltStarts.first() - rebuiltAt >= 120;
                recordTest("Unregister/rebuild skips old due task", rebuildWaited);
                recordTest("Unregistered task executor is not called", oldRebuiltRuns == 0);
                const TaskStats rebuiltStats = scheduler.getTaskStats("rebuildSnapshotB");
                recordTest("Rebuilt task statistics match new executions",
                           rebuiltStats.execCount == static_cast<quint64>(rebuiltStarts.size()));
                unregisterAllTasks(scheduler);

                // ===== Test 14: Shutdown =====
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
