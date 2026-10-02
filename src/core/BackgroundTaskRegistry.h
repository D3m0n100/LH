#pragma once
#include <QObject>
#include <QThread>
#include <atomic>
#include <memory>

namespace Core {
// Owns detached export threads independently of their initiating widget.
class BackgroundTaskRegistry {
public:
    BackgroundTaskRegistry();
    static BackgroundTaskRegistry& instance();
    bool start(QThread* task, std::shared_ptr<std::atomic_bool> cancelled);
    // Closes admission, cancels all tasks, and uses one total shutdown budget.
    bool cancelAndWait(int timeoutMs = 3000);
    int activeCount() const;
private:
    struct State;
    std::shared_ptr<State> m_state;
};
}
