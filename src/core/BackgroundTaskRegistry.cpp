#include "BackgroundTaskRegistry.h"
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QPointer>
#include <QVector>
#include <mutex>
#include <algorithm>

namespace Core {
struct BackgroundTaskRegistry::State {
    struct Entry { QPointer<QThread> thread; std::shared_ptr<std::atomic_bool> cancelled; };
    std::mutex mutex;
    QVector<std::shared_ptr<Entry>> tasks;
    bool closing = false;
};
BackgroundTaskRegistry::BackgroundTaskRegistry() : m_state(std::make_shared<State>()) {}
BackgroundTaskRegistry& BackgroundTaskRegistry::instance() {
    static auto* registry = new BackgroundTaskRegistry;
    return *registry;
}
bool BackgroundTaskRegistry::start(QThread* task, std::shared_ptr<std::atomic_bool> cancelled) {
    auto* app = QCoreApplication::instance();
    if (!app || !task || !cancelled || task->isRunning() || task->parent()
        || task->thread() != QThread::currentThread()) return false;
    const auto state = m_state;
    const auto entry = std::make_shared<State::Entry>();
    entry->thread = task;
    entry->cancelled = std::move(cancelled);
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->closing) return false;
    // The QThread object is cleaned on the application thread; its function runs elsewhere.
    task->moveToThread(app->thread());
    state->tasks.append(entry);
    QObject::connect(task, &QThread::finished, app, [state, entry] {
        std::lock_guard<std::mutex> lock(state->mutex);
        // finished can precede thread-local destruction; retain ownership until native wait succeeds.
        if (entry->thread && !entry->thread->wait(100)) return;
        if (entry->thread) delete entry->thread.data();
        state->tasks.removeAll(entry);
    }, Qt::QueuedConnection);
    task->start();
    return true;
}
bool BackgroundTaskRegistry::cancelAndWait(int timeoutMs) {
    if (QCoreApplication::instance()
        && QThread::currentThread() != QCoreApplication::instance()->thread()) return false;
    const auto state = m_state;
    QVector<std::shared_ptr<State::Entry>> tasks;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->closing = true;
        tasks = state->tasks;
        for (const auto& task : tasks) task->cancelled->store(true);
    }
    QDeadlineTimer deadline(qMax(0, timeoutMs));
    bool drained = true;
    for (const auto& entry : tasks) {
        if (entry->thread && !entry->thread->wait(static_cast<unsigned long>(qMax<qint64>(0, deadline.remainingTime())))) {
            drained = false;
            continue;
        }
        std::lock_guard<std::mutex> lock(state->mutex);
        if (entry->thread) delete entry->thread.data();
        state->tasks.removeAll(entry);
    }
    return drained;
}
int BackgroundTaskRegistry::activeCount() const {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->tasks.size();
}
}
