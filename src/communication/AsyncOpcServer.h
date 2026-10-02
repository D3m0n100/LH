#pragma once
#include "IOpcServer.h"
#include <QTimer>
#include <QHash>
#include <functional>
#include <memory>

// UI facade. The factory, all native interfaces and their destruction run in one worker apartment.
class AsyncOpcServer final : public IOpcServer {
    Q_OBJECT
public:
    using Factory = std::function<IOpcServer*()>;
    explicit AsyncOpcServer(Factory factory, QObject* parent = nullptr);
    ~AsyncOpcServer() override;
    bool applyConfig(const OpcServerConfig&, QString* error = nullptr) override;
    bool start(QString* error = nullptr) override; // true means admitted; runningStateChanged reports completion.
    void stop() override;
    bool isRunning() const override { return m_running; }
    void setRuntimePoints(const QList<RuntimePointDefinition>&) override;
    void setOpcTags(const QList<OpcTagDefinition>&) override;
    void updatePointValues(const QList<RuntimePointValue>&) override;
    void recordWriteResult(const QString&, bool, const QString&) override;
    BackendStatusSnapshot statusSnapshot() const override;
private:
    struct WorkerState;
    bool enqueue(std::function<void(IOpcServer*)> action, bool finishesStart = false, bool finishesValues = false);
    void scheduleValues();
    void acceptSnapshot(const BackendStatusSnapshot&, quint64 generation, bool finishesStart);
    std::shared_ptr<WorkerState> m_worker;
    OpcServerConfig m_config;
    BackendStatusSnapshot m_snapshot;
    QList<RuntimePointDefinition> m_points;
    QList<OpcTagDefinition> m_tags;
    QHash<QString, RuntimePointValue> m_values;
    qint64 m_valueBytes = 0;
    quint64 m_droppedValues = 0;
    bool m_valuesScheduled = false;
    bool m_configured = false;
    bool m_running = false;
    bool m_starting = false;
    QTimer m_startDeadline;
};
