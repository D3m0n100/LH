/**
 * @file TaskSchedulerTest.cpp
 * @brief TaskScheduler 单元测试实现
 */

#include "TaskSchedulerTest.h"
#include <QCoreApplication>
#include <QThread>

void TaskSchedulerTest::initTestCase()
{
    // 测试套件初始化
}

void TaskSchedulerTest::cleanupTestCase()
{
    // 测试套件清理
}

void TaskSchedulerTest::init()
{
    // 确保调度器是干净状态
    if (TaskScheduler::instance().isRunning()) {
        TaskScheduler::instance().stop();
    }
    
    // 清除所有现有任务
    for (const QString& name : TaskScheduler::instance().taskNames()) {
        TaskScheduler::instance().unregisterTask(name);
    }
}

void TaskSchedulerTest::cleanup()
{
    // 停止调度器
    if (TaskScheduler::instance().isRunning()) {
        TaskScheduler::instance().stop();
    }
    
    // 清除所有任务
    for (const QString& name : TaskScheduler::instance().taskNames()) {
        TaskScheduler::instance().unregisterTask(name);
    }
}

void TaskSchedulerTest::waitMs(int ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
}

// ============================================================================
// 基本功能测试
// ============================================================================

void TaskSchedulerTest::testTaskRegistration()
{
    auto& scheduler = TaskScheduler::instance();
    
    // 初始状态应该没有任务
    QCOMPARE(scheduler.taskCount(), 0);
    
    // 注册任务
    scheduler.registerTask("task1", 100, 50, []() {});
    QCOMPARE(scheduler.taskCount(), 1);
    QVERIFY(scheduler.hasTask("task1"));
    
    // 注册同名任务应该替换
    scheduler.registerTask("task1", 50, 100, []() {});
    QCOMPARE(scheduler.taskCount(), 1);
    
    // 注册另一个任务
    scheduler.registerTask("task2", 200, 100, []() {});
    QCOMPARE(scheduler.taskCount(), 2);
    
    // 注销任务
    scheduler.unregisterTask("task1");
    QCOMPARE(scheduler.taskCount(), 1);
    QVERIFY(!scheduler.hasTask("task1"));
    QVERIFY(scheduler.hasTask("task2"));
    
    // 注销不存在的任务不应该出错
    scheduler.unregisterTask("nonexistent");
    QCOMPARE(scheduler.taskCount(), 1);
}

void TaskSchedulerTest::testTaskPriority()
{
    auto& scheduler = TaskScheduler::instance();
    
    // 注册不同优先级的任务
    scheduler.registerTask("lowPriority", 200, 100, []() {});
    scheduler.registerTask("highPriority", 10, 100, []() {});
    scheduler.registerTask("mediumPriority", 100, 100, []() {});
    
    // 获取任务名称列表（应该按优先级排序）
    QStringList names = scheduler.taskNames();
    QCOMPARE(names.size(), 3);
    QCOMPARE(names[0], QString("highPriority"));
    QCOMPARE(names[1], QString("mediumPriority"));
    QCOMPARE(names[2], QString("lowPriority"));
}

void TaskSchedulerTest::testStartStop()
{
    auto& scheduler = TaskScheduler::instance();
    
    QVERIFY(!scheduler.isRunning());
    
    scheduler.registerTask("testTask", 100, 50, []() {});
    
    scheduler.start();
    QVERIFY(scheduler.isRunning());
    
    waitMs(100);
    
    scheduler.stop();
    QVERIFY(!scheduler.isRunning());
    
    // 应该可以重新启动
    scheduler.start();
    QVERIFY(scheduler.isRunning());
    
    scheduler.stop();
}

// ============================================================================
// 调度策略测试
// ============================================================================

void TaskSchedulerTest::testFixedDelayMode()
{
    auto& scheduler = TaskScheduler::instance();
    
    std::atomic<int> counter{0};
    
    scheduler.registerTask("fixedDelay", 100, 50, [&counter]() {
        ++counter;
        QThread::msleep(10);  // 模拟执行耗时
    }, ScheduleMode::FixedDelay);
    
    scheduler.start();
    waitMs(200);
    scheduler.stop();
    
    // FixedDelay 模式下，实际执行间隔 = 周期 + 执行时间
    // 200ms 内，大约执行 200 / (50 + 10) = 3-4 次
    QVERIFY(counter >= 2 && counter <= 5);
}

void TaskSchedulerTest::testFixedRateMode()
{
    auto& scheduler = TaskScheduler::instance();
    
    std::atomic<int> counter{0};
    
    scheduler.registerTask("fixedRate", 100, 50, [&counter]() {
        ++counter;
    }, ScheduleMode::FixedRate);
    
    scheduler.start();
    waitMs(200);
    scheduler.stop();
    
    // FixedRate 模式下，200ms 内应该执行约 4 次
    QVERIFY(counter >= 3 && counter <= 5);
}

// ============================================================================
// 异常处理测试
// ============================================================================

void TaskSchedulerTest::testExceptionHandling()
{
    auto& scheduler = TaskScheduler::instance();
    
    std::atomic<int> counter{0};
    
    // 注册一个会抛异常的任务
    scheduler.registerTask("throwingTask", 100, 50, [&counter]() {
        ++counter;
        if (counter == 2) {
            throw std::runtime_error("Test exception");
        }
    });
    
    QSignalSpy errorSpy(&scheduler, &TaskScheduler::taskError);
    
    scheduler.start();
    waitMs(200);
    scheduler.stop();
    
    // 确保调度器没有崩溃，任务继续执行
    QVERIFY(counter >= 3);
    
    // 确保错误信号被发出
    QVERIFY(errorSpy.count() >= 1);
}

void TaskSchedulerTest::testAutoDisable()
{
    auto& scheduler = TaskScheduler::instance();
    
    // 注册一个总是抛异常的任务，设置最大连续错误为 3
    scheduler.registerTask("alwaysFails", 100, 20, []() {
        throw std::runtime_error("Always fails");
    });
    
    QSignalSpy disabledSpy(&scheduler, &TaskScheduler::taskDisabled);
    
    scheduler.start();
    waitMs(500);  // 等待足够长时间触发自动禁用
    scheduler.stop();
    
    // 验证任务被禁用
    QVERIFY(!scheduler.isTaskEnabled("alwaysFails"));
}

// ============================================================================
// 性能测试
// ============================================================================

void TaskSchedulerTest::testHighFrequencyTasks()
{
    auto& scheduler = TaskScheduler::instance();
    
    std::atomic<int> counter{0};
    
    // 注册一个高频率任务（每 5ms 执行一次）
    scheduler.registerTask("highFreq", 100, 5, [&counter]() {
        ++counter;
    });
    
    scheduler.start();
    waitMs(100);
    scheduler.stop();
    
    // 100ms 内应该执行约 20 次
    QVERIFY(counter >= 15 && counter <= 25);
}

void TaskSchedulerTest::testMultipleTasks()
{
    auto& scheduler = TaskScheduler::instance();
    
    std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};
    std::atomic<int> counter3{0};
    
    scheduler.registerTask("task1", 10, 20, [&counter1]() { ++counter1; });
    scheduler.registerTask("task2", 50, 30, [&counter2]() { ++counter2; });
    scheduler.registerTask("task3", 100, 50, [&counter3]() { ++counter3; });
    
    scheduler.start();
    waitMs(200);
    scheduler.stop();
    
    // 验证所有任务都执行了
    QVERIFY(counter1 > 0);
    QVERIFY(counter2 > 0);
    QVERIFY(counter3 > 0);
    
    // 高优先级任务应该执行更多次
    // task1: 200/20 = 10 次
    // task2: 200/30 = 6-7 次
    // task3: 200/50 = 4 次
    QVERIFY(counter1 >= counter2);
}

// ============================================================================
// 统计信息测试
// ============================================================================

void TaskSchedulerTest::testTaskStats()
{
    auto& scheduler = TaskScheduler::instance();
    
    std::atomic<int> counter{0};
    
    scheduler.registerTask("statsTask", 100, 30, [&counter]() {
        ++counter;
        QThread::msleep(5);  // 模拟执行耗时
    });
    
    scheduler.start();
    waitMs(150);
    scheduler.stop();
    
    TaskStats stats = scheduler.getTaskStats("statsTask");
    
    QVERIFY(stats.execCount > 0);
    QVERIFY(stats.totalExecTimeMs > 0);
    QVERIFY(stats.maxExecTimeMs >= 5);
    QVERIFY(stats.minExecTimeMs > 0);
    QVERIFY(stats.errorCount == 0);
}

void TaskSchedulerTest::testTimeoutWarning()
{
    auto& scheduler = TaskScheduler::instance();
    
    // 注册一个执行时间超过周期 80% 的任务
    scheduler.registerTask("slowTask", 100, 50, []() {
        QThread::msleep(45);  // 周期的 90%
    });
    
    QSignalSpy warningSpy(&scheduler, &TaskScheduler::taskWarning);
    
    scheduler.start();
    waitMs(200);
    scheduler.stop();
    
    // 应该收到超时告警
    QVERIFY(warningSpy.count() >= 1);
}

// 注册测试类
// QTEST_MAIN(TaskSchedulerTest)
// 如果要独立运行，取消上面的注释
