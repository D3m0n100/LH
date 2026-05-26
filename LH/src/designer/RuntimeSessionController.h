// File: src/designer/RuntimeSessionController.h

#ifndef RUNTIMESESSIONCONTROLLER_H
#define RUNTIMESESSIONCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "common/ConfigTypes.h"

class IDeviceBackend;
class SampleDataProvider;
class BuildController;
class ProjectController;
struct CompileResult;

enum class RuntimeSessionState {
    Idle,
    Compiled,
    Connecting,
    Connected,
    Running,
    Monitoring,
    Downloading,
    Fault
};

class RuntimeSessionController : public QObject
{
    Q_OBJECT
public:
    explicit RuntimeSessionController(QObject* parent = nullptr);

    void setDeviceBackend(IDeviceBackend* backend);
    IDeviceBackend* deviceBackend() const { return m_backend; }

    void setSampleDataProvider(SampleDataProvider* provider);
    void setProjectController(ProjectController* controller);
    void setBuildController(BuildController* controller);

    RuntimeSessionState state() const { return m_state; }
    bool isRunning() const { return m_state == RuntimeSessionState::Running; }
    bool isMonitoring() const { return m_monitoringActive; }
    bool isDemoMode() const { return m_demoModeActive; }
    bool hasPendingRunAfterCompile() const { return m_pendingRunAfterCompile; }
    QString artifactPath() const { return m_artifactPath; }

    bool prepareRun();
    bool applyRuntimeConfig();
    void executeRun();
    void requestStop();

    void setPendingRunAfterCompile(bool pending) { m_pendingRunAfterCompile = pending; }
    void setSkipNextBuildSave(bool skip) { m_skipNextBuildSave = skip; }
    bool skipNextBuildSave() const { return m_skipNextBuildSave; }

    bool onCompileSucceeded(const CompileResult& result);

    void startDemoMode(const QString& reason);
    void stopDemoMode(const QString& reason);

    void startMonitoring();
    void stopMonitoring();

    bool requestDownload(const QString& artifactPath, const QVariantMap& options = {});

signals:
    void stateChanged(RuntimeSessionState oldState, RuntimeSessionState newState);
    void runtimeError(const QString& message);
    void logMessage(const QString& message);
    void demoModeChanged(bool active);
    void monitoringChanged(bool active);
    void downloadFinished(bool success, const QString& message);

private:
    void setState(RuntimeSessionState newState);
    bool shouldAutoDownload() const;

    IDeviceBackend* m_backend = nullptr;
    SampleDataProvider* m_sampleDataProvider = nullptr;
    ProjectController* m_projectController = nullptr;
    BuildController* m_buildController = nullptr;

    RuntimeSessionState m_state = RuntimeSessionState::Idle;
    bool m_demoModeActive = false;
    bool m_monitoringActive = false;
    bool m_pendingRunAfterCompile = false;
    bool m_skipNextBuildSave = false;
    QString m_artifactPath;
};

#endif // RUNTIMESESSIONCONTROLLER_H
