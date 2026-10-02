#pragma once
#include <QCoreApplication>
#include <QThread>
#include <QPointer>
#include <QVector>
#include <QDeadlineTimer>
#include <mutex>

// Retains timed-out native apartments without destroying a running QThread.
// The composition root must check drain() before destroying QApplication.
namespace DeferredThreadCleanup {
struct State { std::mutex mutex; QVector<QPointer<QThread>> threads; };
inline State& state() { static auto* value = new State; return *value; }
inline bool drain(int timeoutMs = 0) {
    if (QCoreApplication::instance()
        && QThread::currentThread() != QCoreApplication::instance()->thread()) return false;
    auto& value = state();
    std::lock_guard<std::mutex> lock(value.mutex);
    QDeadlineTimer deadline(qMax(0, timeoutMs));
    for (int i = value.threads.size() - 1; i >= 0; --i) {
        auto* thread = value.threads.at(i).data();
        if (!thread || thread->wait(static_cast<unsigned long>(qMax<qint64>(0, deadline.remainingTime())))) {
            delete thread;
            value.threads.removeAt(i);
        }
    }
    return value.threads.isEmpty();
}
inline void retain(QThread* thread) {
    Q_ASSERT(thread && !thread->parent());
    Q_ASSERT(thread->thread() == QThread::currentThread());
    {
        auto& value = state(); std::lock_guard<std::mutex> lock(value.mutex);
        if (!value.threads.contains(thread)) value.threads.append(thread);
    }
    if (auto* app = QCoreApplication::instance())
        QObject::connect(thread, &QThread::finished, app, [] { drain(); }, Qt::QueuedConnection);
}
}
