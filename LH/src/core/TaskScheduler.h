/**
 * @file TaskScheduler.h
 * @brief 任务调度器
 *
 * 提供基于优先级和周期的任务调度功能。
 *
 * 【调度策略】
 * - 支持两种调度模式：
 *   1. FixedDelay（固定延迟）：基于上次执行完成时间计算下次执行
 *   2. FixedRate（固定频率）：严格按周期执行，允许补偿执行
 *
 * 【执行防护】
 * - 捕获所有任务执行异常
 * - 执行超时告警（默认阈值：周期的 80%）
 * - 连续失败计数和可选禁用
 *
 * 【线程安全】
 * - 任务的注册/注销操作是线程安全的
 * - 任务执行在主线程（Qt 事件循环）中进行
 */

#ifndef TASKSCHEDULER_H
#define TASKSCHEDULER_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>
#include <QHash>
#include <QStringList>
#include <QDateTime>
#include <functional>
#include <memory>
#include <vector>
#include "Common.h"

// ============================================================================
// 类型定义
// ============================================================================

/**
 * @brief 调度模式
 */
enum class ScheduleMode {
    FixedDelay,     ///< 固定延迟：基于上次执行完成时间
    FixedRate       ///< 固定频率：严格按周期，允许补偿
};

/**
 * @brief 任务状态
 */
enum class TaskState {
    Ready,          ///< 就绪
    Running,        ///< 运行中
    Disabled,       ///< 已禁用
    Error           ///< 错误状态
};

/**
 * @brief 任务统计信息
 */
struct TaskStats {
    quint64 execCount = 0;              ///< 执行次数
    quint64 errorCount = 0;             ///< 错误次数
    quint64 consecutiveErrors = 0;      ///< 连续错误次数
    qint64 totalExecTimeMs = 0;         ///< 累计执行时间（毫秒）
    qint64 maxExecTimeMs = 0;           ///< 最大单次执行时间
    qint64 minExecTimeMs = LLONG_MAX;   ///< 最小单次执行时间
    qint64 lastExecTimeMs = 0;          ///< 上次执行时间
    QDateTime lastExecTimestamp;        ///< 上次执行时间戳
    QDateTime lastErrorTimestamp;       ///< 上次错误时间戳
    QString lastError;                  ///< 上次错误信息
    
    /// 计算平均执行时间
    double avgExecTimeMs() const {
        return execCount > 0 ? static_cast<double>(totalExecTimeMs) / execCount : 0.0;
    }
};

/**
 * @brief 任务结构体
 */
struct Task {
    QString name;                       ///< 任务名称
    int priority = 128;                 ///< 优先级 0-255，数值越小优先级越高
    int periodMs = 100;                 ///< 执行周期（毫秒）
    std::function<void()> executor;     ///< 任务执行函数
    ScheduleMode scheduleMode = ScheduleMode::FixedDelay; ///< 调度模式
    TaskState state = TaskState::Ready; ///< 任务状态
    qint64 lastExecutionTime = 0;       ///< 上次执行时间（相对于调度器启动）
    qint64 nextScheduledTime = 0;       ///< 下次计划执行时间
    int warningThresholdMs = 0;         ///< 超时告警阈值（0=使用默认值）
    int maxConsecutiveErrors = 10;      ///< 最大连续错误次数（达到后禁用）
    bool enabled = true;                ///< 是否启用
    
    Task() = default;
};

/// 任务智能指针类型
using TaskPtr = std::unique_ptr<Task>;

// ============================================================================
// TaskScheduler 类定义
// ============================================================================

class TaskScheduler : public QObject
{
    Q_OBJECT
    SINGLETON(TaskScheduler)
    
public:
    /// 默认 tick 间隔（毫秒）
    static constexpr int DEFAULT_TICK_INTERVAL_MS = 1;
    
    /// 默认超时告警阈值（周期的百分比）
    static constexpr int DEFAULT_WARNING_THRESHOLD_PERCENT = 80;
    
    // =========================================================================
    // 任务管理
    // =========================================================================
    
    /**
     * @brief 注册任务
     * @param name 任务名称（唯一标识）
     * @param priority 优先级 0-255，数值越小优先级越高
     * @param periodMs 执行周期（毫秒）
     * @param executor 任务执行函数
     * @param mode 调度模式（默认：FixedDelay）
     */
    void registerTask(const QString& name,
                     int priority,
                     int periodMs,
                     std::function<void()> executor,
                     ScheduleMode mode = ScheduleMode::FixedDelay);
    
    /**
     * @brief 注销任务
     * @param name 任务名称
     */
    void unregisterTask(const QString& name);
    
    /**
     * @brief 检查任务是否存在
     */
    bool hasTask(const QString& name) const;
    
    /**
     * @brief 获取已注册任务数量
     */
    int taskCount() const;
    
    /**
     * @brief 获取所有任务名称
     */
    QStringList taskNames() const;
    
    /**
     * @brief 启用/禁用指定任务
     */
    void setTaskEnabled(const QString& name, bool enabled);
    
    /**
     * @brief 获取任务是否启用
     */
    bool isTaskEnabled(const QString& name) const;
    
    /**
     * @brief 设置任务超时告警阈值
     * @param name 任务名称
     * @param thresholdMs 阈值（毫秒），0 表示使用默认值
     */
    void setTaskWarningThreshold(const QString& name, int thresholdMs);
    
    /**
     * @brief 获取任务统计信息
     */
    TaskStats getTaskStats(const QString& name) const;
    
    /**
     * @brief 获取所有任务统计信息
     */
    QHash<QString, TaskStats> getAllTaskStats() const;
    
    /**
     * @brief 重置任务统计信息
     */
    void resetTaskStats(const QString& name);
    
    /**
     * @brief 重置所有任务统计信息
     */
    void resetAllTaskStats();
    
    // =========================================================================
    // 调度控制
    // =========================================================================
    
    /**
     * @brief 启动调度器
     */
    void start();
    
    /**
     * @brief 停止调度器
     */
    void stop();
    
    /**
     * @brief 关闭调度器，释放所有资源
     */
    void shutdown();
    
    /**
     * @brief 获取运行状态
     */
    bool isRunning() const { return m_running; }
    
    /**
     * @brief 获取调度器是否已关闭
     */
    bool isShutdown() const { return m_shutdown; }
    
    /**
     * @brief 设置 tick 间隔
     * @param intervalMs tick 间隔（毫秒）
     */
    void setTickInterval(int intervalMs);
    
    /**
     * @brief 获取调度器运行时间
     * @return 运行时间（毫秒）
     */
    qint64 runningTimeMs() const;
    
signals:
    /**
     * @brief 任务执行完成信号
     * @param taskName 任务名称
     * @param elapsedMs 执行耗时（毫秒）
     */
    void taskExecuted(const QString& taskName, qint64 elapsedMs);
    
    /**
     * @brief 任务执行错误信号
     * @param taskName 任务名称
     * @param error 错误信息
     */
    void taskError(const QString& taskName, const QString& error);
    
    /**
     * @brief 任务执行超时告警信号
     * @param taskName 任务名称
     * @param elapsedMs 执行耗时
     * @param thresholdMs 阈值
     */
    void taskWarning(const QString& taskName, qint64 elapsedMs, int thresholdMs);
    
    /**
     * @brief 任务被自动禁用信号（连续错误过多）
     * @param taskName 任务名称
     * @param errorCount 连续错误次数
     */
    void taskDisabled(const QString& taskName, int errorCount);
    
    /**
     * @brief 调度器启动信号
     */
    void started();
    
    /**
     * @brief 调度器停止信号
     */
    void stopped();
    
private slots:
    void onTick();
    
private:
    /**
     * @brief 对任务列表按优先级排序
     */
    void sortTasksByPriority();
    
    /**
     * @brief 输出任务统计信息
     */
    void printTaskStats() const;
    
    /**
     * @brief 清除所有任务
     */
    void clearAllTasks();
    
    /**
     * @brief 执行单个任务
     * @param task 任务指针
     * @param currentTime 当前时间
     */
    void executeTask(const QString& taskName, qint64 currentTime);
    
    /**
     * @brief 计算任务的告警阈值
     */
    int getWarningThreshold(const Task* task) const;
    
    /**
     * @brief 更新任务统计信息
     */
    void updateTaskStats(const QString& taskName, qint64 execTimeMs, bool success, const QString& error = QString());

private:
    QTimer* m_tickTimer = nullptr;              ///< 调度 tick 定时器
    QElapsedTimer m_globalTimer;                ///< 全局运行计时器
    std::vector<TaskPtr> m_tasks;               ///< 已注册任务列表
    mutable QMutex m_taskMutex;                 ///< 任务列表互斥锁
    bool m_running = false;                     ///< 调度器是否正在运行
    bool m_shutdown = false;                    ///< 调度器是否已关闭
    int m_tickIntervalMs = DEFAULT_TICK_INTERVAL_MS; ///< tick 间隔

    QHash<QString, TaskStats> m_taskStats;      ///< 任务统计信息
};

#endif // TASKSCHEDULER_H
