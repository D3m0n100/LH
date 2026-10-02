#pragma once
#include "MonitorSample.h"
#include "../communication/IDeviceBackend.h"
#include <QElapsedTimer>
#include <QTimer>
#include <QPointer>
#include <QHash>
#include <functional>
#include <atomic>
#include <memory>

namespace Monitor {
// Sampling/connection coordinator. It produces samples through an injected sink and owns no SQL/UI.
class BackendSampler : public QObject {
public:
    explicit BackendSampler(std::function<void(const Sample&)> sink, QObject* parent = nullptr);
    ~BackendSampler() override;
    void setDeviceBackend(IDeviceBackend* backend);
    IDeviceBackend* backend() const { return m_backend.data(); }
    void configure(QStringList ids, QHash<QString, QString> channels, QHash<QString, int> periods, int intervalMs);
    void start();
    void stop();
    void poll();
private:
    void disconnectBackendSignals();
    std::function<void(const Sample&)> m_sink;
    bool m_active = false;
    QPointer<IDeviceBackend> m_backend;
    QTimer* m_backendPollTimer = nullptr;
    QStringList m_backendPointIds;
    QHash<QString, QString> m_pointIdToChannel;
    QHash<QString, int> m_backendPointPeriodsMs;
    QHash<QString, qint64> m_backendPointNextDueMs;
    QElapsedTimer m_backendPollClock;
    quint64 m_backendPollGeneration = 0;
    bool m_backendPollPending = false;
    std::shared_ptr<std::atomic_bool> m_backendPollCancelled;
    QMetaObject::Connection m_backendPointsChangedConnection;
    QMetaObject::Connection m_backendConnectionStateConnection;
    QMetaObject::Connection m_backendDestroyedConnection;
};
}
