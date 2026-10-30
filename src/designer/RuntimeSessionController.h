// File: src/designer/RuntimeSessionController.h

#ifndef RUNTIMESESSIONCONTROLLER_H
#define RUNTIMESESSIONCONTROLLER_H

#include <QObject>
#include <QMetaType>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <QtGlobal>
#include <functional>
#include <utility>

#include "../common/ConfigTypes.h"
#include "../runtime/RuntimeSessionService.h"
#include "DownloadConfirmation.h"

class QTimer;
class IDeviceBackend;
class IOpcServer;
class ControllerDeviceBackend;
class SampleDataProvider;
class BuildController;
class ProjectController;
class ParameterController;
struct CompileResult;
struct CommError;

#include "../runtime/RuntimeSessionTypes.h"

class RuntimeSessionController : public QObject
{
    Q_OBJECT
public:
    explicit RuntimeSessionController(QObject* parent = nullptr, RuntimeSessionService* service = nullptr);
    ~RuntimeSessionController() override;

    void setDeviceBackend(IDeviceBackend* backend);
    IDeviceBackend* deviceBackend() const;
    quint64 deviceBackendGeneration() const { return m_backendGeneration; }
    void setOpcServer(IOpcServer* opcServer);
    IOpcServer* opcServer() const { return m_opcServer; }

    void setSampleDataProvider(SampleDataProvider* provider);
    void setProjectController(ProjectController* controller);
    void setBuildController(BuildController* controller);
    void setParameterController(ParameterController* controller);

    RuntimeSessionState state() const { return m_runtimeService->state(); }
    DownloadState downloadState() const { return m_runtimeService->downloadState(); }
    bool isRunning() const { return state() == RuntimeSessionState::Running
                                    || state() == RuntimeSessionState::Monitoring; }
    bool isMonitoring() const { return state() == RuntimeSessionState::Monitoring; }
    bool isDebugCommandPending() const { return m_debugCommandPending; }
    bool isPaused() const { return m_isPaused; }
    void setPaused(bool paused);
    bool isDemoMode() const { return m_demoModeActive; }
    bool hasPendingRunAfterCompile() const { return m_pendingRunAfterCompile; }
    QString artifactPath() const { return m_artifactPath; }
    QString currentDownloadOperationId() const { return m_currentDownloadOperationId; }
    bool hasWorkerOperationInFlight() const;

    /**
     * @brief 下载确认钩子。
     *
     * 返回 true 表示批准本次快照；返回 false 表示用户取消，调用方不得继续写入设备。
     * 产品由 MainWindow 注入模态确认；无确认处理器时安全拒绝。
     */
    using DownloadConfirmationHook = std::function<bool(const DownloadConfirmationSnapshot&)>;
    void setDownloadConfirmationHook(DownloadConfirmationHook hook)
    {
        m_downloadConfirmationHook = std::move(hook);
    }

    /**
     * @brief 外部真实写入口（如专家诊断下载）复用同一确认钩子。
     *
     * 快照不完整或未配置钩子时安全拒绝；确认期间阻止新的会话内下载。
     */
    bool confirmExternalDownload(const DownloadConfirmationSnapshot& snapshot);

    /// 本会话最近一次成功下载的记录；仅作为“本会话下载记录”证据，不是实时设备读回。
    struct DownloadRecord
    {
        bool valid = false;
        QString operationId;
        QString artifactPath;
        QString checksum;
        QString generationId;
        QString targetName;
        QString targetIdentity;
        QString endpoint;
        QString configurationFingerprint;
        quint64 backendGeneration = 0;
        QDateTime completedAt;
    };
    DownloadRecord recentDownloadRecord() const { return m_lastDownloadRecord; }

    bool prepareRun();
    bool applyRuntimeConfig();
    void applyRuntimeConfigAsync(std::function<void(bool)> completed);
    /// 启动运行会话；若设备在线且存在产物，允许按既有策略自动下载后运行。
    void executeRun();
    /// 仅启动运行会话（F9 语义）：不编译、不下载、不改写设备程序。
    void executeExistingRun();
    void requestStop();

    void setPendingRunAfterCompile(bool pending) { m_pendingRunAfterCompile = pending; }
    void setSkipNextBuildSave(bool skip) { m_skipNextBuildSave = skip; }
    bool skipNextBuildSave() const { return m_skipNextBuildSave; }

    using RunContinuationValidator = std::function<bool()>;
    using RunPreparationCompletion = std::function<void(bool, const QString&)>;
    bool onCompileSucceeded(const CompileResult& result,
                            const QString& operationId,
                            RunContinuationValidator continuationValid,
                            RunPreparationCompletion completed);
    void invalidateCompiledArtifact();

    void startDemoMode(const QString& reason);
    void stopDemoMode(const QString& reason);
    /// 清理演示通道及其历史；仅在真实配置/工程切换等隔离边界调用。
    void clearDemoChannels();

    void startMonitoring();
    void stopMonitoring();

    bool requestDownload(const QString& artifactPath, const QVariantMap& options = {});
    bool pauseController();
    bool resumeController();
    bool stepController();
    bool startContinuousStep(int intervalMs = 500);
    void stopContinuousStep();
    bool isContinuousStepActive() const { return m_continuousStepActive; }
    bool runControllerToCursor(int lineNumber);
    bool setControllerBreakpoints(int firstLine, int secondLine);
    bool testControllerConnection();

signals:
    void stateChanged(RuntimeSessionState oldState, RuntimeSessionState newState);
    void runtimeError(const QString& message);
    void logMessage(const QString& message);
    void demoModeChanged(bool active);
    void monitoringChanged(bool active);
    void downloadStateChanged(DownloadState oldState, DownloadState newState);
    void downloadProgressChanged(int percent);
    void downloadFinished(bool success, const QString& message);
    void downloadDiagnosticChanged(const QVariantMap& diagnostic);
    void opcRunningChanged(bool running);
    void opcErrorOccurred(const QString& message);
    void pausedChanged(bool paused);
    void debugCommandPendingChanged(bool pending);
    void continuousStepChanged(bool active);
    void workerOperationBusyChanged(bool busy);

private:
    void onControllerBackendBusyChanged(bool busy);
    void scheduleContinuousStep();
    QTimer* m_continuousStepTimer = nullptr;
    bool m_continuousStepActive = false;
    int m_continuousStepIntervalMs = 500;
    void setDebugCommandPending(bool pending);
    std::function<void()> m_cancelDebugAudit;
    bool requestControllerDebug(const QString& command, const QVector<quint16>& arguments,
                                int pauseState, const QString& description);
    void finishRunStart();
    void requestControllerDownloadAttempt(ControllerDeviceBackend* controller, const QString& path,
        const QVariantMap& options, RuntimeSessionState previousState, int attempt, int maxAttempts,
        const QString& operationId);

private:
    void setState(RuntimeSessionState newState);
    void handleRuntimeStateChanged(RuntimeSessionState oldState, RuntimeSessionState newState);
    void setDownloadState(DownloadState newState);
    void handleBackendConnectionStateChanged(bool connected);
    DownloadState classifyDownloadFailure(const CommError* operationError,
                                          const QString& errorMessage) const;
    void emitDownloadDiagnostic(const QString& severity,
                                const QString& stage,
                                const QString& message,
                                const QVariantMap& details = QVariantMap());
    bool runDownloadPrecheck(const QString& artifactPath,
                             const QVariantMap& options,
                             QString* errorMessage);
    void executeRunInternal(bool allowDownload);
    DownloadConfirmationSnapshot buildDownloadSnapshot(const QString& artifactPath,
                                                       const QVariantMap& options) const;
    bool confirmDownloadSnapshot(const DownloadConfirmationSnapshot& snapshot,
                                 const QVariantMap& options);
    void recordSuccessfulDownload(const DownloadConfirmationSnapshot& snapshot);
    QString computeFileChecksum(const QString& path) const;
    QString currentEndpointDescription() const;
    bool shouldAutoDownload() const;
    void connectOpcServerSignals();
    void handleOpcRunningStateChanged(bool running);
    void handleOpcWriteRequest(const QString& pointId, const QVariant& value);
    void handleParameterReadbackFinished(bool success, const QString& message);
    void cancelPendingOpcWrite(const QString& message);
    void syncOpcRuntimePoints();
    void startOpcServerIfEnabled();
    void stopOpcServer();
    bool ensureControllerBackend();
    void ensureControllerBackendAsync(std::function<void(bool)> completed);
    std::shared_ptr<std::atomic_bool> m_connectionCancelled;
    ControllerDeviceBackend* controllerBackend() const;

    QPointer<IDeviceBackend> m_backend;
    ControllerDeviceBackend* m_ownedControllerBackend = nullptr;
    IOpcServer* m_opcServer = nullptr;
    SampleDataProvider* m_sampleDataProvider = nullptr;
    QPointer<ProjectController> m_projectController;
    BuildController* m_buildController = nullptr;
    ParameterController* m_parameterController = nullptr;

    RuntimeSessionService* m_runtimeService = nullptr;
    bool m_demoModeActive = false;
    bool m_isPaused = false;
    bool m_pendingRunAfterCompile = false;
    bool m_pendingRunAfterDownload = false;
    bool m_skipNextBuildSave = false;
    bool m_downloadCancelled = false;
    bool m_backendDownloadInProgress = false;
    bool m_internalReconnect = false;
    QString m_artifactPath;
    QString m_currentDownloadOperationId;
    QString m_runFlowOperationIdContext;
    DownloadConfirmationHook m_downloadConfirmationHook;
    DownloadConfirmationSnapshot m_activeDownloadSnapshot;
    QString m_approvedDownloadFingerprint;
    QString m_approvedDownloadOperationId;
    bool m_downloadConfirmationPending = false;
    QString m_pendingDownloadFingerprint;
    DownloadRecord m_lastDownloadRecord;
    quint64 m_runPreparationGeneration = 0;
    bool m_pendingOpcWriteActive = false;
    QString m_pendingOpcPointId;
    QString m_pendingOpcParameterName;
    QString m_pendingOpcOperationId;
    quint64 m_backendGeneration = 0;
    quint64 m_debugCommandGeneration = 0;
    bool m_debugCommandPending = false;
};

#endif // RUNTIMESESSIONCONTROLLER_H
