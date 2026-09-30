#pragma once
#include "RuntimeSessionTypes.h"
#include "ParameterController.h"
#include "RunController.h"
#include "../communication/IDeviceBackend.h"
#include "../monitor/IMonitorHistoryStore.h"
#include <QObject>
#include <QPointer>

class RuntimeMonitorPort {
public:
    virtual ~RuntimeMonitorPort() = default;
    virtual void setDeviceBackend(IDeviceBackend*) = 0;
    virtual bool applyConfiguration(const ProjectRuntimeConfig&) = 0;
    virtual QStringList providerIds() const = 0;
    virtual void startMonitoring() = 0;
    virtual void stopMonitoring() = 0;
    virtual bool isMonitoring() const = 0;
    virtual void removeDemoChannels() = 0;
};

// Runtime control has no widget/chart dependency or implicit singleton lookup.
class RuntimeSessionService : public QObject {
    Q_OBJECT
public:
    RuntimeSessionService(std::shared_ptr<RuntimeMonitorPort> monitor,
                          std::shared_ptr<Monitor::IMonitorHistoryStore> history = {}, QObject* parent = nullptr);
    void setBackend(IDeviceBackend* backend);
    IDeviceBackend* backend() const { return m_backend.data(); }
    RuntimeMonitorPort& monitor() const { return *m_monitor; }
    void setHistory(std::shared_ptr<Monitor::IMonitorHistoryStore> history) { m_history = std::move(history); }
    ParameterController* parameters() const { return m_parameters.data(); }
    void setParameters(ParameterController* parameters);
    bool applyParameters(QString* errorMessage = nullptr);
    RuntimeSessionState state() const { return m_state; }
    DownloadState downloadState() const { return m_downloadState; }
    void setState(RuntimeSessionState state);
    void setDownloadState(DownloadState state);
    void cancelOperations();
    void stop();
    quint64 operationGeneration() const { return m_generation; }
    std::shared_ptr<std::atomic_bool> cancellationToken() const { return m_cancelled; }
    RunController::DownloadArtifactPrecheckReport precheck(const ProjectRuntimeConfig& config,
        const QString& projectPath, const QString& artifactPath, bool requirePublishedBinding = false) const;
signals:
    void stateChanged(RuntimeSessionState oldState, RuntimeSessionState newState);
    void downloadStateChanged(DownloadState oldState, DownloadState newState);
private:
    std::shared_ptr<RuntimeMonitorPort> m_monitor;
    std::shared_ptr<Monitor::IMonitorHistoryStore> m_history;
    QPointer<IDeviceBackend> m_backend;
    QPointer<ParameterController> m_parameters;
    RuntimeSessionState m_state = RuntimeSessionState::Idle;
    DownloadState m_downloadState = DownloadState::Idle;
    quint64 m_generation = 0;
    quint64 m_backendBindingGeneration = 0;
    std::shared_ptr<std::atomic_bool> m_cancelled;
};
