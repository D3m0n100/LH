/**
 * @file TaskSchedulerTest.h
 * @brief TaskScheduler 单元测试
 */

#ifndef TASKSCHEDULER_TEST_H
#define TASKSCHEDULER_TEST_H

#include <QObject>
#include <QTest>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <atomic>
#include "TaskScheduler.h"

class TaskSchedulerTest : public QObject
{
    Q_OBJECT
    
private slots:
    /// 初始化测试环境
    void initTestCase();
    
    /// 清理测试环境
    void cleanupTestCase();
    
    /// 每个测试前的初始化
    void init();
    
    /// 每个测试后的清理
    void cleanup();
    
    // ===== 基本功能测试 =====
    
    /// 测试任务注册和注销
    void testTaskRegistration();
    
    /// 测试任务优先级排序
    void testTaskPriority();
    
    /// 测试调度器启动和停止
    void testStartStop();
    
    // ===== 调度策略测试 =====
    
    /// 测试 FixedDelay 模式
    void testFixedDelayMode();
    
    /// 测试 FixedRate 模式
    void testFixedRateMode();
    
    // ===== 异常处理测试 =====
    
    /// 测试任务异常捕获
    void testExceptionHandling();
    
    /// 测试连续错误自动禁用
    void testAutoDisable();
    
    // ===== 性能测试 =====
    
    /// 测试高频率任务执行
    void testHighFrequencyTasks();
    
    /// 测试多任务并发
    void testMultipleTasks();
    
    // ===== 统计信息测试 =====
    
    /// 测试任务统计信息
    void testTaskStats();
    
    /// 测试超时告警
    void testTimeoutWarning();
    
private:
    /// 等待指定毫秒数（处理事件循环）
    void waitMs(int ms);
};

#endif // TASKSCHEDULER_TEST_H
