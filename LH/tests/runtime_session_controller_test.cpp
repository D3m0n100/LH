/**
 * @file runtime_session_controller_test.cpp
 * @brief RuntimeSessionController 单元测试
 */

#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QTemporaryFile>

#define private public
#include "designer/RuntimeSessionController.h"
#undef private
#include "communication/IOpcServer.h"
#include "communication/ControllerDeviceBackend.h"
#include "common/RuntimePointTypes.h"
#include "communication/VirtualDeviceBackend.h"
#include "designer/ParameterController.h"
#include "designer/ProjectController.h"
#include "designer/RunController.h"
#include "monitor/MonitorManager.h"

class TestOpcServer : public IOpcServer
{
    Q_OBJECT
public:
    explicit TestOpcServer(QObject* parent = nullptr)
        : IOpcServer(parent)
    {
    }

    bool applyConfig(const OpcServerConfig& config, QString* errorMessage = nullptr) override
    {
        Q_UNUSED(errorMessage)
        m_lastConfig = config;
        return true;
    }

    bool start(QString* errorMessage = nullptr) override
    {
        Q_UNUSED(errorMessage)
        m_running = true;
        emit runningStateChanged(true);
        return true;
    }

    void stop() override
    {
        m_running = false;
        emit runningStateChanged(false);
    }

    bool isRunning() const override { return m_running; }

    void setRuntimePoints(const QList<RuntimePointDefinition>& points) override
    {
        m_points = points;
    }

    void setOpcTags(const QList<OpcTagDefinition>& tags) override
    {
        m_tags = tags;
    }

    void updatePointValues(const QList<RuntimePointValue>& values) override
    {
        Q_UNUSED(values)
    }

    void recordWriteResult(const QString& pointId, bool success, const QString& message) override
    {
        m_lastWritePointId = pointId;
        m_lastWriteSuccess = success;
        m_lastWriteMessage = message;
    }

    BackendStatusSnapshot statusSnapshot() const override
    {
        BackendStatusSnapshot snapshot;
        snapshot.online = m_running;
        snapshot.backendType = QStringLiteral("test-opc");
        return snapshot;
    }

    OpcServerConfig m_lastConfig;
    QList<RuntimePointDefinition> m_points;
    QList<OpcTagDefinition> m_tags;
    bool m_running = false;
    QString m_lastWritePointId;
    bool m_lastWriteSuccess = false;
    QString m_lastWriteMessage;
};

class FakeRuntimeControllerTransport : public IControllerDebugTransport
{
public:
    bool isConnected() const override { return connected; }
    int stationAddress() const override { return station; }
    void setStationAddress(int deviceId) override { station = deviceId; }

    bool readHoldingRegisters(int address, int count) override
    {
        lastReadAddress = address;
        lastReadCount = count;
        return connected && registers.contains(address);
    }

    QVector<quint16> holdingRegisterValues(int address) const override
    {
        return registers.value(address);
    }

    bool writeMultipleRegisters(int address, const QVector<quint16>& values) override
    {
        lastWriteAddress = address;
        lastWriteValues = values;
        return connected;
    }

    CommError lastError() const override { return error; }

    bool connected = true;
    CommError error;
    QHash<int, QVector<quint16>> registers {
        {10, {1}},
        {23, {0}},
        {26, {2}},
        {27, {3}},
        {38, {105}}
    };
    int lastReadAddress = -1;
    int lastReadCount = 0;
    int lastWriteAddress = -1;
    QVector<quint16> lastWriteValues;
    int station = 1;
};

class RuntimeSessionControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        qRegisterMetaType<RuntimeSessionState>("RuntimeSessionState");
        qRegisterMetaType<DownloadState>("DownloadState");
    }

    void initialStateIsIdle()
    {
        RuntimeSessionController ctrl;
        QCOMPARE(ctrl.state(), RuntimeSessionState::Idle);
        QVERIFY(!ctrl.isRunning());
        QVERIFY(!ctrl.isMonitoring());
        QVERIFY(!ctrl.isDemoMode());
    }

    void stateChangedSignalOnRun()
    {
        RuntimeSessionController ctrl;
        QSignalSpy spy(&ctrl, &RuntimeSessionController::stateChanged);

        ctrl.executeRun();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(static_cast<RuntimeSessionState>(spy.first().at(0).toInt()), RuntimeSessionState::Idle);
        QCOMPARE(static_cast<RuntimeSessionState>(spy.first().at(1).toInt()), RuntimeSessionState::Running);
    }

    void executeRunTransitionsToRunning()
    {
        RuntimeSessionController ctrl;
        ctrl.executeRun();

        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
        QVERIFY(ctrl.isRunning());
    }

    void executeRunAutoDownloadWhenBackendOnline()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        ctrl.setDeviceBackend(&backend);

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        ctrl.m_artifactPath = artifactFile.fileName();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::downloadFinished);
        ctrl.executeRun();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
    }

    void executeRunAutoDownloadFailureDoesNotEnterRunning()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        backend.setFaultInjection(false, false, true);
        ctrl.setDeviceBackend(&backend);

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        ctrl.m_artifactPath = artifactFile.fileName();

        QSignalSpy stateSpy(&ctrl, &RuntimeSessionController::stateChanged);
        QSignalSpy finishedSpy(&ctrl, &RuntimeSessionController::downloadFinished);
        ctrl.executeRun();

        QVERIFY(finishedSpy.count() == 1);
        QVERIFY(!finishedSpy.first().first().toBool());
        QCOMPARE(ctrl.state(), RuntimeSessionState::Connected);
        QVERIFY(!ctrl.isRunning());
        QVERIFY(!stateSpy.isEmpty());
        for (const auto& args : stateSpy) {
            QVERIFY(static_cast<RuntimeSessionState>(args.at(1).toInt())
                    != RuntimeSessionState::Running);
        }
    }

    void executeRunAutoDownloadPrecheckFailureRestoresConnected()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        ctrl.setDeviceBackend(&backend);
        ctrl.m_artifactPath = QStringLiteral("missing-auto-download.code");

        QSignalSpy finishedSpy(&ctrl, &RuntimeSessionController::downloadFinished);
        ctrl.executeRun();

        QCOMPARE(finishedSpy.count(), 1);
        QVERIFY(!finishedSpy.first().first().toBool());
        QCOMPARE(ctrl.state(), RuntimeSessionState::Connected);
        QVERIFY(!ctrl.isRunning());
    }

    void executeRunSkipsAutoDownloadWhenBackendOffline()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        ctrl.setDeviceBackend(&backend);

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        ctrl.m_artifactPath = artifactFile.fileName();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::downloadFinished);
        ctrl.executeRun();

        QCOMPARE(spy.count(), 0);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
    }

    void requestStopFromRunningGoesToIdle()
    {
        RuntimeSessionController ctrl;
        ctrl.executeRun();
        ctrl.requestStop();

        QCOMPARE(ctrl.state(), RuntimeSessionState::Idle);
        QVERIFY(!ctrl.isRunning());
    }

    void startMonitoringFromRunningGoesToMonitoring()
    {
        RuntimeSessionController ctrl;
        ctrl.executeRun();
        ctrl.startMonitoring();

        QCOMPARE(ctrl.state(), RuntimeSessionState::Monitoring);
        QVERIFY(ctrl.isMonitoring());
    }

    void startMonitoringFromIdleIsRejected()
    {
        RuntimeSessionController ctrl;
        QSignalSpy errorSpy(&ctrl, &RuntimeSessionController::runtimeError);

        ctrl.startMonitoring();

        QCOMPARE(ctrl.state(), RuntimeSessionState::Idle);
        QVERIFY(!ctrl.isMonitoring());
        QCOMPARE(errorSpy.count(), 1);
    }

    void stopMonitoringFromMonitoringGoesToRunning()
    {
        RuntimeSessionController ctrl;
        ctrl.executeRun();
        ctrl.startMonitoring();
        ctrl.stopMonitoring();

        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
        QVERIFY(!ctrl.isMonitoring());
    }

    void downloadWithoutBackendFails()
    {
        RuntimeSessionController ctrl;
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        ctrl.m_artifactPath = artifactFile.fileName();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::runtimeError);
        const bool ok = ctrl.requestDownload(ctrl.m_artifactPath);

        QVERIFY(!ok);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Fault);
        QCOMPARE(ctrl.downloadState(), DownloadState::PrecheckFailed);
    }

    void downloadOfflineBackendFails()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        ctrl.m_artifactPath = artifactFile.fileName();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::runtimeError);
        const bool ok = ctrl.requestDownload(ctrl.m_artifactPath);

        QVERIFY(!ok);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Fault);
        QCOMPARE(ctrl.downloadState(), DownloadState::PrecheckFailed);
    }

    void downloadSuccessRestoresState()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        ctrl.m_artifactPath = artifactFile.fileName();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::downloadFinished);
        QSignalSpy stateSpy(&ctrl, &RuntimeSessionController::downloadStateChanged);
        QSignalSpy progressSpy(&ctrl, &RuntimeSessionController::downloadProgressChanged);
        const bool ok = ctrl.requestDownload(ctrl.m_artifactPath);

        QVERIFY(ok);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
        QCOMPARE(progressSpy.count(), 4);
        QCOMPARE(progressSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(progressSpy.at(1).at(0).toInt(), 25);
        QCOMPARE(progressSpy.at(2).at(0).toInt(), 75);
        QCOMPARE(progressSpy.at(3).at(0).toInt(), 100);
        QVERIFY(stateSpy.count() >= 4);
        QCOMPARE(static_cast<DownloadState>(stateSpy.first().at(1).toInt()), DownloadState::Precheck);
        QCOMPARE(static_cast<DownloadState>(stateSpy.at(1).at(1).toInt()), DownloadState::Downloading);
        QCOMPARE(static_cast<DownloadState>(stateSpy.at(2).at(1).toInt()), DownloadState::Verifying);
        QCOMPARE(ctrl.downloadState(), DownloadState::Succeeded);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
    }

    void downloadFaultInjectionRestoresRunning()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        backend.setFaultInjection(false, false, true);
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        ctrl.m_artifactPath = artifactFile.fileName();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::downloadFinished);
        QSignalSpy stateSpy(&ctrl, &RuntimeSessionController::downloadStateChanged);
        const bool ok = ctrl.requestDownload(ctrl.m_artifactPath);

        QVERIFY(!ok);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), false);
        QVERIFY(stateSpy.count() >= 1);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
        QCOMPARE(ctrl.downloadState(), DownloadState::Failed);
    }

    void manualDownloadFailureRestoresRunningState()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        const bool ok = ctrl.requestDownload(artifactFile.fileName(),
                                             {{QStringLiteral("simulateDownloadFailure"),
                                               QStringLiteral("verify")}});

        QVERIFY(!ok);
        QCOMPARE(ctrl.downloadState(), DownloadState::VerifyFailed);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
        QVERIFY(ctrl.isRunning());
    }

    void stoppingDuringDownloadDoesNotRestoreRunning()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        QSignalSpy stateSpy(&ctrl, &RuntimeSessionController::stateChanged);
        QSignalSpy finishedSpy(&ctrl, &RuntimeSessionController::downloadFinished);
        connect(&ctrl, &RuntimeSessionController::stateChanged,
                &ctrl, [&ctrl](RuntimeSessionState, RuntimeSessionState newState) {
                    if (newState == RuntimeSessionState::Downloading) {
                        ctrl.requestStop();
                    }
                });

        QVERIFY(!ctrl.requestDownload(artifactFile.fileName()));
        QCOMPARE(ctrl.state(), RuntimeSessionState::Idle);
        QCOMPARE(ctrl.downloadState(), DownloadState::Idle);
        QVERIFY(!stateSpy.isEmpty());
        QCOMPARE(static_cast<RuntimeSessionState>(stateSpy.last().at(1).toInt()),
                 RuntimeSessionState::Idle);
        QCOMPARE(finishedSpy.count(), 1);
        QVERIFY(!finishedSpy.first().first().toBool());
    }

    void backendDisconnectLeavesNoRunningOrMonitoringState()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();
        ctrl.startMonitoring();
        QCOMPARE(ctrl.state(), RuntimeSessionState::Monitoring);

        backend.disconnectBackend();

        QCOMPARE(ctrl.state(), RuntimeSessionState::Fault);
        QVERIFY(!ctrl.isRunning());
        QVERIFY(!ctrl.isMonitoring());
        QVERIFY(!Monitor::MonitorManager::instance().isMonitoring());
    }

    void downloadTransportFailureClassifiesState()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        ctrl.m_artifactPath = artifactFile.fileName();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::downloadFinished);
        const bool ok = ctrl.requestDownload(ctrl.m_artifactPath,
                                              {{"simulateDownloadFailure", "transport"}});

        QVERIFY(!ok);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(ctrl.downloadState(), DownloadState::TransportFailed);
    }

    void downloadRejectedFailureClassifiesState()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        ctrl.m_artifactPath = artifactFile.fileName();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::downloadFinished);
        const bool ok = ctrl.requestDownload(ctrl.m_artifactPath,
                                              {{"simulateDownloadFailure", "rejected"}});

        QVERIFY(!ok);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(ctrl.downloadState(), DownloadState::DeviceRejected);
    }

    void downloadVerifyFailureClassifiesState()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        ctrl.m_artifactPath = artifactFile.fileName();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::downloadFinished);
        const bool ok = ctrl.requestDownload(ctrl.m_artifactPath,
                                              {{"simulateDownloadFailure", "verify"}});

        QVERIFY(!ok);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(ctrl.downloadState(), DownloadState::VerifyFailed);
    }

    void downloadRetrySucceedsOnSecondAttempt()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        backend.setDownloadFaultInjection(1, CommErrorCode::ConnectionLost, QStringLiteral("one-shot transport fault"));
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        ctrl.m_artifactPath = artifactFile.fileName();

        QSignalSpy finishedSpy(&ctrl, &RuntimeSessionController::downloadFinished);
        QSignalSpy stateSpy(&ctrl, &RuntimeSessionController::downloadStateChanged);
        QSignalSpy progressSpy(&ctrl, &RuntimeSessionController::downloadProgressChanged);
        const bool ok = ctrl.requestDownload(ctrl.m_artifactPath, {{"retryCount", 2}});

        QVERIFY(ok);
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().first().toBool(), true);
        QVERIFY(progressSpy.count() >= 5);
        QCOMPARE(progressSpy.first().at(0).toInt(), 0);
        QCOMPARE(progressSpy.at(1).at(0).toInt(), 25);
        QCOMPARE(progressSpy.last().at(0).toInt(), 100);
        bool sawRetrying = false;
        for (const auto& args : stateSpy) {
            if (static_cast<DownloadState>(args.at(1).toInt()) == DownloadState::Retrying) {
                sawRetrying = true;
                break;
            }
        }
        QVERIFY(sawRetrying);
        QCOMPARE(ctrl.downloadState(), DownloadState::Succeeded);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
    }

    void downloadRetryExhaustedFails()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        ctrl.m_artifactPath = artifactFile.fileName();

        QSignalSpy finishedSpy(&ctrl, &RuntimeSessionController::downloadFinished);
        QSignalSpy stateSpy(&ctrl, &RuntimeSessionController::downloadStateChanged);
        const bool ok = ctrl.requestDownload(ctrl.m_artifactPath,
                                              {{"retryCount", 2}, {"simulateDownloadFailure", "verify"}});

        QVERIFY(!ok);
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().first().toBool(), false);
        bool sawRetrying = false;
        for (const auto& args : stateSpy) {
            if (static_cast<DownloadState>(args.at(1).toInt()) == DownloadState::Retrying) {
                sawRetrying = true;
                break;
            }
        }
        QVERIFY(sawRetrying);
        QCOMPARE(ctrl.downloadState(), DownloadState::VerifyFailed);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
    }

    void downloadMissingArtifactFailsPrecheck()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        ctrl.m_artifactPath = QStringLiteral("missing-artifact.code");

        QSignalSpy spy(&ctrl, &RuntimeSessionController::runtimeError);
        const bool ok = ctrl.requestDownload(ctrl.m_artifactPath);

        QVERIFY(!ok);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(ctrl.downloadState(), DownloadState::PrecheckFailed);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
    }

    void downloadChecksumMismatchFailsPrecheck()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        ctrl.setDeviceBackend(&backend);

        ProjectController projectController;
        projectController.runtimeConfig().downloadArtifact.checksum = QStringLiteral("0000");
        ctrl.setProjectController(&projectController);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        artifactFile.write("payload");
        artifactFile.flush();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::runtimeError);
        QSignalSpy diagnosticSpy(&ctrl, &RuntimeSessionController::downloadDiagnosticChanged);
        const bool ok = ctrl.requestDownload(artifactFile.fileName());

        QVERIFY(!ok);
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.first().first().toString().contains(QStringLiteral("checksum")));
        bool sawChecksumDiagnostic = false;
        for (const auto& args : diagnosticSpy) {
            const QVariantMap diagnostic = args.first().toMap();
            if (diagnostic.value(QStringLiteral("severity")).toString() == QStringLiteral("error")
                    && diagnostic.value(QStringLiteral("message")).toString().contains(QStringLiteral("checksum"))) {
                sawChecksumDiagnostic = true;
                break;
            }
        }
        QVERIFY(sawChecksumDiagnostic);
        QCOMPARE(ctrl.downloadState(), DownloadState::PrecheckFailed);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
    }

    void downloadManifestMismatchFailsPrecheck()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString artifactPath = dir.filePath(QStringLiteral("main.code"));
        QFile artifactFile(artifactPath);
        QVERIFY(artifactFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        artifactFile.write("payload");
        artifactFile.close();

        QFile manifestFile(dir.filePath(QStringLiteral("runtime_manifest.json")));
        QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
        manifestFile.write(R"({"artifactPaths":["other.code"],"pointCount":0,"parameterCount":0})");
        manifestFile.close();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::runtimeError);
        const bool ok = ctrl.requestDownload(artifactPath);

        QVERIFY(!ok);
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.first().first().toString().contains(QStringLiteral("runtime_manifest.json")));
        QCOMPARE(ctrl.downloadState(), DownloadState::PrecheckFailed);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
    }

    void downloadControllerDryRunFailureStopsBeforeWrite()
    {
        FakeRuntimeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());

        RuntimeSessionController ctrl;
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        artifactFile.write(QByteArray::fromHex("1234"));
        artifactFile.flush();

        QTemporaryFile profileFile;
        QVERIFY(profileFile.open());
        profileFile.write(R"({
            "name": "bad",
            "slaveId": 1,
            "steps": [
                {"type": "sendChunk", "params": {"chunkWords": 200}}
            ]
        })");
        profileFile.flush();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::runtimeError);
        QSignalSpy diagnosticSpy(&ctrl, &RuntimeSessionController::downloadDiagnosticChanged);
        const bool ok = ctrl.requestDownload(
                artifactFile.fileName(),
                {{QStringLiteral("downloadProfilePath"), profileFile.fileName()}});

        QVERIFY(!ok);
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.first().first().toString().contains(QStringLiteral("dry-run")));
        bool sawDryRunDiagnostic = false;
        for (const auto& args : diagnosticSpy) {
            const QVariantMap diagnostic = args.first().toMap();
            if (diagnostic.value(QStringLiteral("severity")).toString() == QStringLiteral("error")
                    && diagnostic.value(QStringLiteral("stage")).toString() == QStringLiteral("dry-run")) {
                sawDryRunDiagnostic = true;
                break;
            }
        }
        QVERIFY(sawDryRunDiagnostic);
        QCOMPARE(ctrl.downloadState(), DownloadState::PrecheckFailed);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
        QCOMPARE(transport.lastWriteAddress, -1);
    }

    void downloadControllerMissingProfileStopsBeforeWrite()
    {
        FakeRuntimeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());

        RuntimeSessionController ctrl;
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        artifactFile.write(QByteArray::fromHex("1234"));
        artifactFile.flush();

        QSignalSpy errorSpy(&ctrl, &RuntimeSessionController::runtimeError);
        const bool ok = ctrl.requestDownload(artifactFile.fileName());

        QVERIFY(!ok);
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains(QStringLiteral("Profile")));
        QCOMPARE(ctrl.downloadState(), DownloadState::PrecheckFailed);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
        QCOMPARE(transport.lastWriteAddress, -1);
    }

    void downloadControllerResolvesProjectRelativeProfile()
    {
        QTemporaryDir projectDir;
        QVERIFY(projectDir.isValid());

        QFile projectConfig(projectDir.filePath(QStringLiteral("project_config.json")));
        QVERIFY(projectConfig.open(QIODevice::WriteOnly | QIODevice::Truncate));
        projectConfig.write("{}");
        projectConfig.close();

        const QString artifactPath = projectDir.filePath(QStringLiteral("main.code"));
        QFile artifactFile(artifactPath);
        QVERIFY(artifactFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        artifactFile.write(QByteArray::fromHex("1234"));
        artifactFile.close();

        QFile profileFile(projectDir.filePath(QStringLiteral("download_profile.json")));
        QVERIFY(profileFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        profileFile.write(R"({
            "name": "project-relative",
            "slaveId": 1,
            "steps": [
                {"type": "sendChunk", "params": {"dataAddress": 210, "chunkWords": 1}}
            ]
        })");
        profileFile.close();

        ProjectController projectController;
        QVERIFY(projectController.openProjectFromPath(projectDir.path()));
        projectController.runtimeConfig().downloadArtifact.metadata.insert(
                QStringLiteral("downloadProfilePath"),
                QStringLiteral("download_profile.json"));

        FakeRuntimeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);
        ProjectRuntimeConfig backendConfig;
        backendConfig.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(backendConfig));
        QVERIFY(backend.connectBackend());

        RuntimeSessionController ctrl;
        ctrl.setProjectController(&projectController);
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QVERIFY(ctrl.requestDownload(artifactPath));
        QCOMPARE(ctrl.downloadState(), DownloadState::Succeeded);
        QCOMPARE(transport.lastWriteAddress, 210);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0x1234}));
    }

    void writeDownloadArtifactPreservesDownloadProfileMetadata()
    {
        QTemporaryDir projectDir;
        QVERIFY(projectDir.isValid());
        const QString artifactPath = projectDir.filePath(QStringLiteral("main.code"));
        QFile artifactFile(artifactPath);
        QVERIFY(artifactFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        artifactFile.write("payload");
        artifactFile.close();

        ProjectRuntimeConfig config;
        config.downloadArtifact.metadata.insert(QStringLiteral("downloadProfilePath"),
                                                QStringLiteral("config/download_profile.json"));
        config.downloadArtifact.metadata.insert(QStringLiteral("projectOwned"), true);

        CompileArtifact artifact;
        artifact.type = QStringLiteral("download");
        artifact.format = QStringLiteral("dsl_custom");
        artifact.path = artifactPath;
        artifact.checksum = QStringLiteral("checksum");
        artifact.metadata.insert(QStringLiteral("sourceFile"), QStringLiteral("main.lh"));
        CompileResult result;
        result.artifacts.append(artifact);

        QVERIFY(RunController::writeDownloadArtifact(config, projectDir.path(), result));
        QCOMPARE(config.downloadArtifact.metadata.value(QStringLiteral("downloadProfilePath")).toString(),
                 QStringLiteral("config/download_profile.json"));
        QCOMPARE(config.downloadArtifact.metadata.value(QStringLiteral("projectOwned")).toBool(), true);
        QCOMPARE(config.downloadArtifact.metadata.value(QStringLiteral("sourceFile")).toString(),
                 QStringLiteral("main.lh"));
    }

    void pendingRunFlag()
    {
        RuntimeSessionController ctrl;
        QVERIFY(!ctrl.hasPendingRunAfterCompile());

        ctrl.setPendingRunAfterCompile(true);
        QVERIFY(ctrl.hasPendingRunAfterCompile());

        ctrl.setPendingRunAfterCompile(false);
        QVERIFY(!ctrl.hasPendingRunAfterCompile());
    }

    void skipNextBuildSaveFlag()
    {
        RuntimeSessionController ctrl;
        QVERIFY(!ctrl.skipNextBuildSave());

        ctrl.setSkipNextBuildSave(true);
        QVERIFY(ctrl.skipNextBuildSave());
    }

    void artifactPathInitiallyEmpty()
    {
        RuntimeSessionController ctrl;
        QVERIFY(ctrl.artifactPath().isEmpty());
    }

    void defaultOpcServerIsCreated()
    {
        RuntimeSessionController ctrl;
        QVERIFY(ctrl.opcServer() != nullptr);
        const auto snapshot = ctrl.opcServer()->statusSnapshot();
        QVERIFY(!snapshot.backendType.isEmpty());
        QCOMPARE(snapshot.backendType, QStringLiteral("matrikon-opc-da"));
        QCOMPARE(ctrl.opcServer()->objectName(), QStringLiteral("OpcServerFactory::MatrikonOpcServer"));
    }

    void syncOpcRuntimePointsAddsControllerStatusRegisters()
    {
        RuntimeSessionController ctrl;
        ProjectController projectController;
        projectController.runtimeConfig().controller.modbusSlaveId = 7;
        ctrl.setProjectController(&projectController);

        auto* opc = new TestOpcServer;
        ctrl.setOpcServer(opc);
        ctrl.syncOpcRuntimePoints();

        QHash<QString, RuntimePointDefinition> byId;
        for (const auto& point : opc->m_points) {
            byId.insert(point.id, point);
        }

        QVERIFY(byId.contains(QStringLiteral("controller.state")));
        QCOMPARE(byId.value(QStringLiteral("controller.state")).addressing.value(QStringLiteral("address")).toLongLong(), qint64(10));
        QCOMPARE(byId.value(QStringLiteral("controller.state")).addressing.value(QStringLiteral("unitId")).toInt(), 7);
        QCOMPARE(byId.value(QStringLiteral("controller.state")).addressing.value(QStringLiteral("opcItemId")).toString(),
                 QStringLiteral("CommPort.InitDevParamnt.4:10"));
        QCOMPARE(byId.value(QStringLiteral("controller.state")).opcItemName, QStringLiteral("4:10"));
        QCOMPARE(byId.value(QStringLiteral("controller.state")).opcMetadata.value(QStringLiteral("opcItemId")).toString(),
                 QStringLiteral("CommPort.InitDevParamnt.4:10"));

        QVERIFY(byId.contains(QStringLiteral("controller.workMode")));
        QCOMPARE(byId.value(QStringLiteral("controller.workMode")).addressing.value(QStringLiteral("address")).toLongLong(), qint64(26));

        QVERIFY(byId.contains(QStringLiteral("controller.componentLine")));
        QCOMPARE(byId.value(QStringLiteral("controller.componentLine")).addressing.value(QStringLiteral("address")).toLongLong(), qint64(27));

        QVERIFY(byId.contains(QStringLiteral("controller.reset")));
        QCOMPARE(byId.value(QStringLiteral("controller.reset")).addressing.value(QStringLiteral("address")).toLongLong(), qint64(23));

        QVERIFY(byId.contains(QStringLiteral("controller.version")));
        QCOMPARE(byId.value(QStringLiteral("controller.version")).addressing.value(QStringLiteral("address")).toLongLong(), qint64(38));
        QCOMPARE(byId.value(QStringLiteral("controller.version")).addressing.value(QStringLiteral("opcItemId")).toString(),
                 QStringLiteral("CommPort.InitDevParamnt.4:38"));
        QCOMPARE(opc->m_tags.size(), opc->m_points.size());

        QHash<QString, OpcTagDefinition> tagsByName;
        for (const auto& tag : opc->m_tags) {
            tagsByName.insert(tag.tagName, tag);
        }
        QVERIFY(tagsByName.contains(QStringLiteral("controller.state")));
        QCOMPARE(tagsByName.value(QStringLiteral("controller.state")).item, QStringLiteral("4:10"));
        QCOMPARE(tagsByName.value(QStringLiteral("controller.state")).metadata.value(QStringLiteral("opcItemId")).toString(),
                 QStringLiteral("CommPort.InitDevParamnt.4:10"));
    }

    void setAndClearDeviceBackend()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;

        ctrl.setDeviceBackend(&backend);
        QCOMPARE(ctrl.deviceBackend(), &backend);

        ctrl.setDeviceBackend(nullptr);
        QCOMPARE(ctrl.deviceBackend(), nullptr);
    }

    void executeRunSkipsOpcWhenDisabled()
    {
        RuntimeSessionController ctrl;
        ProjectController projectController;
        projectController.runtimeConfig().opcServer.enabled = false;
        ctrl.setProjectController(&projectController);

        auto* opc = new TestOpcServer;
        ctrl.setOpcServer(opc);
        ctrl.executeRun();

        QVERIFY(!opc->isRunning());
    }

    void executeRunInvalidOpcConfigEmitsRuntimeError()
    {
        RuntimeSessionController ctrl;
        ProjectController projectController;
        projectController.runtimeConfig().opcServer.enabled = true;
        projectController.runtimeConfig().opcServer.opcProgId.clear();
        ctrl.setProjectController(&projectController);

        QSignalSpy errorSpy(&ctrl, &RuntimeSessionController::runtimeError);
        ctrl.executeRun();

        QVERIFY(errorSpy.count() >= 1);
        QVERIFY(errorSpy.last().first().toString().contains(QStringLiteral("OPC 服务配置失败")));
    }

    void requestStopStopsOpcServer()
    {
        RuntimeSessionController ctrl;
        ProjectController projectController;
        projectController.runtimeConfig().opcServer.enabled = true;
        ctrl.setProjectController(&projectController);

        auto* opc = new TestOpcServer;
        ctrl.setOpcServer(opc);

        ctrl.executeRun();
        QVERIFY(opc->isRunning());

        ctrl.requestStop();
        QVERIFY(!opc->isRunning());
        QCOMPARE(ctrl.state(), RuntimeSessionState::Idle);
    }

    void opcWriteUnknownPointRejected()
    {
        RuntimeSessionController ctrl;
        ParameterController parameterController;
        auto* opc = new TestOpcServer;
        ctrl.setParameterController(&parameterController);
        ctrl.setOpcServer(opc);

        ctrl.handleOpcWriteRequest(QStringLiteral("missing.point"), QVariant(1.0));

        QCOMPARE(opc->m_lastWritePointId, QStringLiteral("missing.point"));
        QVERIFY(!opc->m_lastWriteSuccess);
        QVERIFY(opc->m_lastWriteMessage.contains(QStringLiteral("未找到点位")));
    }

    void opcWriteReadOnlyRejected()
    {
        RuntimeSessionController ctrl;
        ParameterController parameterController;
        parameterController.loadDefinitions({[] {
            ParameterDefinition def;
            def.id = QStringLiteral("param.readonly");
            def.name = QStringLiteral("Kp");
            def.dataType = QStringLiteral("REAL");
            def.defaultValue = QStringLiteral("1.0");
            def.currentValue = QStringLiteral("1.0");
            def.onlineEditable = false;
            return def;
        }()});

        auto* opc = new TestOpcServer;
        ctrl.setParameterController(&parameterController);
        ctrl.setOpcServer(opc);

        ctrl.handleOpcWriteRequest(QStringLiteral("param.readonly"), QVariant(2.0));

        QCOMPARE(opc->m_lastWritePointId, QStringLiteral("param.readonly"));
        QVERIFY(!opc->m_lastWriteSuccess);
        QVERIFY(opc->m_lastWriteMessage.contains(QStringLiteral("只读")));
    }

    void opcWriteEditableSuccess()
    {
        RuntimeSessionController ctrl;
        ParameterController parameterController;

        ParameterDefinition def;
        def.id = QStringLiteral("param.kp");
        def.name = QStringLiteral("Kp");
        def.dataType = QStringLiteral("REAL");
        def.defaultValue = QStringLiteral("1.0");
        def.currentValue = QStringLiteral("1.0");
        def.onlineEditable = true;
        parameterController.loadDefinitions({def});

        VirtualDeviceBackend backend;
        RuntimePointDefinition point = RuntimePointConverter::fromParameter(def);
        point.access = RuntimePointAccess::ReadWrite;
        point.defaultValue = 1.0;
        backend.loadPointDefinitions({point});
        backend.connectBackend();

        auto* opc = new TestOpcServer;
        ctrl.setDeviceBackend(&backend);
        ctrl.setParameterController(&parameterController);
        ctrl.setOpcServer(opc);

        ctrl.handleOpcWriteRequest(QStringLiteral("param.kp"), QVariant(3.5));

        QCOMPARE(opc->m_lastWritePointId, QStringLiteral("param.kp"));
        QVERIFY(opc->m_lastWriteSuccess);
        QVERIFY(opc->m_lastWriteMessage.contains(QStringLiteral("OPC 写入成功")));
    }

    void controllerDebugCommandsUseConfiguredBackend()
    {
        RuntimeSessionController ctrl;
        FakeRuntimeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());
        ctrl.setDeviceBackend(&backend);

        QVERIFY(ctrl.pauseController());
        QCOMPARE(transport.lastWriteAddress, 32);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({1}));

        QVERIFY(ctrl.resumeController());
        QCOMPARE(transport.lastWriteAddress, 32);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0}));

        QVERIFY(ctrl.stepController());
        QCOMPARE(transport.lastWriteAddress, 31);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({1}));

        QVERIFY(ctrl.runControllerToCursor(77));
        QCOMPARE(transport.lastWriteAddress, 28);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({77}));
    }

    void controllerConnectionTestUsesConfiguredBackend()
    {
        RuntimeSessionController ctrl;
        FakeRuntimeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());
        ctrl.setDeviceBackend(&backend);

        QSignalSpy logSpy(&ctrl, &RuntimeSessionController::logMessage);
        QVERIFY(ctrl.testControllerConnection());
        QVERIFY(logSpy.count() >= 1);
        QVERIFY(logSpy.first().first().toString().contains(QStringLiteral("控制器连接测试")));
    }
};

QTEST_MAIN(RuntimeSessionControllerTest)
#include "runtime_session_controller_test.moc"
