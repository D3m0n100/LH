/**
 * @file TaskScheduler.cpp
 * @brief 任务调度器实现
 */

#include "TaskScheduler.h"

#include <QDateTime>
#include <algorithm>

TaskScheduler::TaskScheduler()
    : QObject(nullptr)
    , m_tickTimer(nullptr)
    , m_running(false)
    , m_shutdown(false)
    , m_tickIntervalMs(DEFAULT_TICK_INTERVAL_MS)
{
}

TaskScheduler::~TaskScheduler()
{
    if (!m_shutdown) {
        LOG_WARN("TaskScheduler 析构时未调用 shutdown()，将自动清理。");
        shutdown();
    }
}

void TaskScheduler::registerTask(const QString& name,
                                 int priority,
                                 int periodMs,
                                 std::function<void()> executor,
                                 ScheduleMode mode)
{
    if (m_shutdown) {
        LOG_WARN("TaskScheduler 已关闭，无法注册任务: " + name);
        return;
    }

    if (name.isEmpty() || !executor) {
        LOG_WARN("无法注册任务：名称为空或执行器无效。");
        return;
    }

    priority = qBound(0, priority, 255);
    periodMs = qMax(1, periodMs);

    QMutexLocker locker(&m_taskMutex);

    auto it = std::find_if(m_tasks.begin(), m_tasks.end(),
                           [&name](const TaskPtr& t) { return t->name == name; });
    if (it != m_tasks.end()) {
        LOG_INFO("替换已存在的任务: " + name);
        m_tasks.erase(it);
    }

    auto task = std::make_unique<Task>();
    task->name = name;
    task->priority = priority;
    task->periodMs = periodMs;
    task->executor = std::move(executor);
    task->scheduleMode = mode;
    task->state = TaskState::Ready;
    task->lastExecutionTime = 0;
    task->nextScheduledTime = 0;
    task->enabled = true;

    m_tasks.push_back(std::move(task));
    m_taskStats[name] = TaskStats();

    sortTasksByPriority();

    const QString modeStr = (mode == ScheduleMode::FixedDelay) ? "FixedDelay" : "FixedRate";
    LOG_INFO(QString("注册任务: %1 (优先级=%2, 周期=%3ms, 模式=%4)")
             .arg(name).arg(priority).arg(periodMs).arg(modeStr));
}

void TaskScheduler::unregisterTask(const QString& name)
{
    QMutexLocker locker(&m_taskMutex);

    auto it = std::find_if(m_tasks.begin(), m_tasks.end(),
                           [&name](const TaskPtr& t) { return t->name == name; });
    if (it != m_tasks.end()) {
        m_tasks.erase(it);
        m_taskStats.remove(name);
        LOG_INFO("注销任务: " + name);
    }
}

bool TaskScheduler::hasTask(const QString& name) const
{
    QMutexLocker locker(&m_taskMutex);
    return std::any_of(m_tasks.begin(), m_tasks.end(),
                       [&name](const TaskPtr& t) { return t->name == name; });
}

int TaskScheduler::taskCount() const
{
    QMutexLocker locker(&m_taskMutex);
    return static_cast<int>(m_tasks.size());
}

QStringList TaskScheduler::taskNames() const
{
    QMutexLocker locker(&m_taskMutex);

    QStringList names;
    names.reserve(static_cast<int>(m_tasks.size()));
    for (const auto& task : m_tasks) {
        names.append(task->name);
    }
    return names;
}

void TaskScheduler::setTaskEnabled(const QString& name, bool enabled)
{
    QMutexLocker locker(&m_taskMutex);

    auto it = std::find_if(m_tasks.begin(), m_tasks.end(),
                           [&name](const TaskPtr& t) { return t->name == name; });
    if (it == m_tasks.end()) {
        return;
    }

    (*it)->enabled = enabled;
    if (enabled) {
        (*it)->state = TaskState::Ready;
        m_taskStats[name].consecutiveErrors = 0;
    } else {
        (*it)->state = TaskState::Disabled;
    }

    LOG_INFO(QString("任务 %1 已%2").arg(name, enabled ? "启用" : "禁用"));
}

bool TaskScheduler::isTaskEnabled(const QString& name) const
{
    QMutexLocker locker(&m_taskMutex);

    auto it = std::find_if(m_tasks.begin(), m_tasks.end(),
                           [&name](const TaskPtr& t) { return t->name == name; });
    return it != m_tasks.end() && (*it)->enabled;
}

void TaskScheduler::setTaskWarningThreshold(const QString& name, int thresholdMs)
{
    QMutexLocker locker(&m_taskMutex);

    auto it = std::find_if(m_tasks.begin(), m_tasks.end(),
                           [&name](const TaskPtr& t) { return t->name == name; });
    if (it != m_tasks.end()) {
        (*it)->warningThresholdMs = thresholdMs;
    }
}

TaskStats TaskScheduler::getTaskStats(const QString& name) const
{
    QMutexLocker locker(&m_taskMutex);
    return m_taskStats.value(name);
}

QHash<QString, TaskStats> TaskScheduler::getAllTaskStats() const
{
    QMutexLocker locker(&m_taskMutex);
    return m_taskStats;
}

void TaskScheduler::resetTaskStats(const QString& name)
{
    QMutexLocker locker(&m_taskMutex);
    if (m_taskStats.contains(name)) {
        m_taskStats[name] = TaskStats();
    }
}

void TaskScheduler::resetAllTaskStats()
{
    QMutexLocker locker(&m_taskMutex);
    for (auto& stats : m_taskStats) {
        stats = TaskStats();
    }
}

void TaskScheduler::sortTasksByPriority()
{
    std::sort(m_tasks.begin(), m_tasks.end(),
              [](const TaskPtr& a, const TaskPtr& b) {
                  return a->priority < b->priority;
              });
}

void TaskScheduler::clearAllTasks()
{
    m_tasks.clear();
    m_taskStats.clear();
    LOG_DEBUG("所有任务已清除");
}

void TaskScheduler::start()
{
    if (m_shutdown) {
        LOG_WARN("TaskScheduler 已关闭，无法启动。");
        return;
    }

    if (m_running) {
        LOG_DEBUG("TaskScheduler 已在运行。");
        return;
    }

    m_running = true;
    m_globalTimer.start();

    {
        QMutexLocker locker(&m_taskMutex);
        for (auto& task : m_tasks) {
            task->lastExecutionTime = 0;
            task->nextScheduledTime = 0;
            if (task->enabled && task->state != TaskState::Disabled) {
                task->state = TaskState::Ready;
            }
        }
    }

    m_tickTimer = new QTimer(this);
    connect(m_tickTimer, &QTimer::timeout, this, &TaskScheduler::onTick);
    m_tickTimer->start(m_tickIntervalMs);

    LOG_INFO(QString("任务调度器已启动 (tick=%1ms)").arg(m_tickIntervalMs));
    emit started();
}

void TaskScheduler::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;

    if (m_tickTimer) {
        m_tickTimer->stop();
        m_tickTimer->deleteLater();
        m_tickTimer = nullptr;
    }

    const qint64 totalMs = m_globalTimer.isValid() ? m_globalTimer.elapsed() : 0;
    LOG_INFO(QString("任务调度器已停止，总运行时间: %1 ms").arg(totalMs));

    printTaskStats();
    emit stopped();
}

void TaskScheduler::shutdown()
{
    if (m_shutdown) {
        return;
    }

    LOG_INFO("TaskScheduler 正在关闭...");

    stop();

    {
        QMutexLocker locker(&m_taskMutex);
        clearAllTasks();
    }

    m_shutdown = true;
    LOG_INFO("TaskScheduler 已关闭。");
}

void TaskScheduler::setTickInterval(int intervalMs)
{
    m_tickIntervalMs = qMax(1, intervalMs);
    if (m_tickTimer && m_running) {
        m_tickTimer->setInterval(m_tickIntervalMs);
    }
}

qint64 TaskScheduler::runningTimeMs() const
{
    return m_globalTimer.isValid() ? m_globalTimer.elapsed() : 0;
}

void TaskScheduler::printTaskStats() const
{
    QMutexLocker locker(&m_taskMutex);

    for (auto it = m_taskStats.constBegin(); it != m_taskStats.constEnd(); ++it) {
        const QString& taskName = it.key();
        const TaskStats& stats = it.value();

        if (stats.execCount == 0) {
            LOG_INFO(QString("任务[%1] 未执行").arg(taskName));
            continue;
        }

        LOG_INFO(QString("任务[%1] 执行=%2次, 错误=%3次, 累计=%4ms, 平均=%5ms, 最小=%6ms, 最大=%7ms")
                 .arg(taskName)
                 .arg(stats.execCount)
                 .arg(stats.errorCount)
                 .arg(stats.totalExecTimeMs)
                 .arg(stats.avgExecTimeMs(), 0, 'f', 2)
                 .arg(stats.minExecTimeMs == LLONG_MAX ? 0 : stats.minExecTimeMs)
                 .arg(stats.maxExecTimeMs));
    }
}

void TaskScheduler::onTick()
{
    if (!m_running) {
        return;
    }

    const qint64 currentTime = m_globalTimer.elapsed();
    QStringList dueTaskNames;

    {
        QMutexLocker locker(&m_taskMutex);

        for (auto& task : m_tasks) {
            if (!task->enabled || task->state == TaskState::Disabled) {
                continue;
            }

            bool shouldExecute = false;
            if (task->scheduleMode == ScheduleMode::FixedDelay) {
                shouldExecute = (currentTime - task->lastExecutionTime >= task->periodMs);
            } else {
                if (task->nextScheduledTime == 0) {
                    task->nextScheduledTime = currentTime + task->periodMs;
                }
                shouldExecute = (currentTime >= task->nextScheduledTime);
            }

            if (shouldExecute) {
                dueTaskNames.append(task->name);
            }
        }
    }

    for (const QString& taskName : dueTaskNames) {
        executeTask(taskName, currentTime);
    }
}

void TaskScheduler::executeTask(const QString& taskName, qint64 currentTime)
{
    std::function<void()> executor;
    ScheduleMode scheduleMode = ScheduleMode::FixedDelay;
    int periodMs = 0;
    int warningThreshold = 0;
    int maxConsecutiveErrors = 0;

    {
        QMutexLocker locker(&m_taskMutex);

        auto it = std::find_if(m_tasks.begin(), m_tasks.end(),
                               [&taskName](const TaskPtr& t) { return t->name == taskName; });
        if (it == m_tasks.end()) {
            return;
        }

        Task* task = it->get();
        if (!task->enabled || task->state == TaskState::Disabled || !task->executor) {
            return;
        }

        task->state = TaskState::Running;
        executor = task->executor;
        scheduleMode = task->scheduleMode;
        periodMs = task->periodMs;
        warningThreshold = getWarningThreshold(task);
        maxConsecutiveErrors = task->maxConsecutiveErrors;
    }

    QElapsedTimer execTimer;
    execTimer.start();

    bool success = true;
    QString errorMsg;
    try {
        executor();
    } catch (const std::exception& e) {
        success = false;
        errorMsg = QString("std::exception: %1").arg(e.what());
        LOG_ERROR(QString("任务执行异常[%1]: %2").arg(taskName, errorMsg));
    } catch (...) {
        success = false;
        errorMsg = "未知异常";
        LOG_ERROR(QString("任务执行发生未知异常[%1]").arg(taskName));
    }

    const qint64 costMs = execTimer.elapsed();
    bool taskDisabledByErrors = false;
    int consecutiveErrors = 0;

    {
        QMutexLocker locker(&m_taskMutex);

        auto it = std::find_if(m_tasks.begin(), m_tasks.end(),
                               [&taskName](const TaskPtr& t) { return t->name == taskName; });
        if (it == m_tasks.end()) {
            return;
        }

        Task* task = it->get();
        task->lastExecutionTime = currentTime + costMs;

        if (scheduleMode == ScheduleMode::FixedRate) {
            task->nextScheduledTime += periodMs;
            if (task->nextScheduledTime < currentTime) {
                LOG_WARN(QString("任务[%1] 执行落后，重新同步调度时间。").arg(taskName));
                task->nextScheduledTime = currentTime + periodMs;
            }
        }

        updateTaskStats(taskName, costMs, success, errorMsg);

        if (success) {
            task->state = TaskState::Ready;
        } else {
            task->state = TaskState::Error;

            TaskStats& stats = m_taskStats[taskName];
            consecutiveErrors = static_cast<int>(stats.consecutiveErrors);
            if (stats.consecutiveErrors >= static_cast<quint64>(maxConsecutiveErrors)) {
                task->enabled = false;
                task->state = TaskState::Disabled;
                taskDisabledByErrors = true;
                LOG_ERROR(QString("任务[%1] 连续错误 %2 次，已自动禁用。")
                          .arg(taskName).arg(stats.consecutiveErrors));
            }
        }
    }

    if (costMs > warningThreshold) {
        LOG_WARN(QString("任务[%1] 执行时间过长: %2ms (阈值: %3ms)")
                 .arg(taskName).arg(costMs).arg(warningThreshold));
        emit taskWarning(taskName, costMs, warningThreshold);
    }

    if (success) {
        emit taskExecuted(taskName, costMs);
        return;
    }

    emit taskError(taskName, errorMsg);
    if (taskDisabledByErrors) {
        emit taskDisabled(taskName, consecutiveErrors);
    }
}

int TaskScheduler::getWarningThreshold(const Task* task) const
{
    if (task->warningThresholdMs > 0) {
        return task->warningThresholdMs;
    }
    return task->periodMs * DEFAULT_WARNING_THRESHOLD_PERCENT / 100;
}

void TaskScheduler::updateTaskStats(const QString& taskName,
                                    qint64 execTimeMs,
                                    bool success,
                                    const QString& error)
{
    TaskStats& stats = m_taskStats[taskName];

    stats.execCount += 1;
    stats.totalExecTimeMs += execTimeMs;
    stats.lastExecTimeMs = execTimeMs;
    stats.lastExecTimestamp = QDateTime::currentDateTime();
    stats.maxExecTimeMs = qMax(stats.maxExecTimeMs, execTimeMs);
    stats.minExecTimeMs = qMin(stats.minExecTimeMs, execTimeMs);

    if (success) {
        stats.consecutiveErrors = 0;
        return;
    }

    stats.errorCount += 1;
    stats.consecutiveErrors += 1;
    stats.lastError = error;
    stats.lastErrorTimestamp = QDateTime::currentDateTime();
}
