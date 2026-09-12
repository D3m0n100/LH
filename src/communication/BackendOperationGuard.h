#ifndef BACKEND_OPERATION_GUARD_H
#define BACKEND_OPERATION_GUARD_H

#include <QMutex>

namespace Communication {

/**
 * @brief RAII 互斥守卫，用于安全保护 ControllerDeviceBackend 内部设备操作
 *
 * 特性：
 * - 禁用复制以防止双重 unlock
 * - 析构时自动释放持有的锁
 * - 支持 tryLock() 与 lock()
 */
class BackendOperationGuard
{
public:
    explicit BackendOperationGuard(QMutex* mutex)
        : m_mutex(mutex)
    {
    }

    ~BackendOperationGuard()
    {
        unlock();
    }

    BackendOperationGuard(const BackendOperationGuard&) = delete;
    BackendOperationGuard& operator=(const BackendOperationGuard&) = delete;

    BackendOperationGuard(BackendOperationGuard&& other) noexcept
        : m_mutex(other.m_mutex)
        , m_locked(other.m_locked)
    {
        other.m_mutex = nullptr;
        other.m_locked = false;
    }

    BackendOperationGuard& operator=(BackendOperationGuard&& other) noexcept
    {
        if (this != &other) {
            unlock();
            m_mutex = other.m_mutex;
            m_locked = other.m_locked;
            other.m_mutex = nullptr;
            other.m_locked = false;
        }
        return *this;
    }

    bool tryLock()
    {
        m_locked = m_mutex && m_mutex->tryLock();
        return m_locked;
    }

    void lock()
    {
        if (m_mutex) {
            m_mutex->lock();
            m_locked = true;
        }
    }

    void unlock()
    {
        if (m_locked && m_mutex) {
            m_mutex->unlock();
            m_locked = false;
        }
    }

    bool isLocked() const { return m_locked; }

private:
    QMutex* m_mutex = nullptr;
    bool m_locked = false;
};

} // namespace Communication

#endif // BACKEND_OPERATION_GUARD_H
