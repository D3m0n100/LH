/**
 * @file runtime_session_controller_test.cpp
 * @brief RuntimeSessionController 单元测试
 */

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QEventLoop>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include <functional>
#include <utility>

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
        m_updatedValues.append(values);
    }

    void recordWriteResult(const QString& pointId, bool success, const QString& message) override
    {
        ++m_recordCount;
        m_recordSuccesses.append(success);
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
    int m_recordCount = 0;
    QList<bool> m_recordSuccesses;
    QList<RuntimePointValue> m_updatedValues;
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
        const auto callback = std::move(onNextWrite);
        if (callback) {
            callback();
        }
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
    std::function<void()> onNextWrite;
};

class DelayedRuntimeTransport : public FakeRuntimeControllerTransport {
public:
    explicit DelayedRuntimeTransport(bool lateMatch) : late(lateMatch) { registers[300] = {999}; }
    void setRequestBudget(int ms, const std::atomic_bool* token) override {
        remaining = ms; cancelled = token;
    }
    bool readHoldingRegisters(int address, int count) override {
        if (address != 300) return FakeRuntimeControllerTransport::readHoldingRegisters(address,count);
        QElapsedTimer timer; timer.start();
        while (timer.elapsed() < (late ? 260 : 5000)) {
            if (cancelled && cancelled->load()) { error=CommError(CommProtocolType::ModbusRTU,CommErrorCode::OperationCancelled,"cancelled");return false; }
            // Late-match case deliberately ignores the budget to test the post-read check.
            QThread::msleep(2);
        }
        return FakeRuntimeControllerTransport::readHoldingRegisters(address,count);
    }
    bool late = false; int remaining=-1; const std::atomic_bool* cancelled=nullptr;
};

class BorrowedTrackingBackend : public VirtualDeviceBackend
{
public:
    using VirtualDeviceBackend::VirtualDeviceBackend;

    void disconnectBackend() override
    {
        ++disconnectCount;
        VirtualDeviceBackend::disconnectBackend();
    }

    int disconnectCount = 0;
};

class TrackingControllerDeviceBackend : public ControllerDeviceBackend
{
public:
    using ControllerDeviceBackend::ControllerDeviceBackend;

    bool downloadArtifact(const QString& artifactPath,
                          const QVariantMap& options,
                          QString* errorMessage = nullptr,
                          CommError* operationError = nullptr) override
    {
        downloadCallActive = true;
        const bool result = ControllerDeviceBackend::downloadArtifact(
                artifactPath, options, errorMessage, operationError);
        downloadCallActive = false;
        return result;
    }

    void disconnectBackend() override
    {
        ++disconnectCount;
        if (downloadCallActive) {
            disconnectDuringDownload = true;
            return;
        }
        ControllerDeviceBackend::disconnectBackend();
    }

    bool downloadCallActive = false;
    bool disconnectDuringDownload = false;
    int disconnectCount = 0;
};

namespace {

QByteArray fixtureChecksum(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

QString fixtureFileChecksum(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromLatin1(fixtureChecksum(file.readAll()));
}

bool writeFixtureFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(bytes) == bytes.size();
}

bool writePublishedBundle(const QString& projectPath,
                          const QString& generationId,
                          const QByteArray& code = QByteArrayLiteral("payload"),
                          const QByteArray& profile = QByteArrayLiteral(
                                  "{\"name\":\"fixture\",\"slaveId\":1,\"steps\":["
                                  "{\"type\":\"sendChunk\",\"params\":{\"dataAddress\":210,"
                                  "\"chunkWords\":1}}]}"))
{
    if (!writeFixtureFile(QDir(projectPath).filePath(QStringLiteral("project_config.json")),
                          QByteArrayLiteral("{}"))
            || !writeFixtureFile(QDir(projectPath).filePath(QStringLiteral("main.lh")),
                                 QByteArrayLiteral("PROGRAM Main\nEND_PROGRAM\n"))) {
        return false;
    }

    const QString generationPath = QDir(projectPath).filePath(
            QStringLiteral("build_output/configuration/generations/%1").arg(generationId));
    if (!QDir().mkpath(generationPath))
        return false;

    const QByteArray points = QByteArrayLiteral("[]");
    const QString codePath = QDir(generationPath).filePath(QStringLiteral("main.code"));
    const QString profilePath = QDir(generationPath).filePath(QStringLiteral("download_profile.json"));
    const QString pointsPath = QDir(generationPath).filePath(QStringLiteral("runtime_points.json"));
    if (!writeFixtureFile(codePath, code)
            || !writeFixtureFile(profilePath, profile)
            || !writeFixtureFile(pointsPath, points)) {
        return false;
    }

    QJsonObject checksums;
    checksums.insert(QStringLiteral("main.code"), QString::fromLatin1(fixtureChecksum(code)));
    checksums.insert(QStringLiteral("download_profile.json"), QString::fromLatin1(fixtureChecksum(profile)));
    checksums.insert(QStringLiteral("runtime_points.json"), QString::fromLatin1(fixtureChecksum(points)));

    QJsonObject sourcePaths;
    sourcePaths.insert(QStringLiteral("mainScriptPath"), QStringLiteral("main.lh"));
    sourcePaths.insert(QStringLiteral("dslScriptPath"), QStringLiteral("main.lh"));
    QJsonArray scripts;
    scripts.append(QStringLiteral("main.lh"));
    QJsonObject sourceChecksums;
    sourceChecksums.insert(QStringLiteral("main.lh"),
                           QString::fromLatin1(fixtureChecksum(QByteArrayLiteral("PROGRAM Main\nEND_PROGRAM\n"))));
    QJsonArray artifactPaths;
    artifactPaths.append(QStringLiteral("main.code"));
    artifactPaths.append(QStringLiteral("download_profile.json"));
    artifactPaths.append(QStringLiteral("runtime_points.json"));

    QJsonObject manifest;
    manifest.insert(QStringLiteral("generationId"), generationId);
    manifest.insert(QStringLiteral("complete"), true);
    manifest.insert(QStringLiteral("projectName"), QStringLiteral("fixture"));
    manifest.insert(QStringLiteral("mainScriptPath"), QStringLiteral("main.lh"));
    manifest.insert(QStringLiteral("dslScriptPath"), QStringLiteral("main.lh"));
    manifest.insert(QStringLiteral("scriptFiles"), scripts);
    manifest.insert(QStringLiteral("sourceChecksums"), sourceChecksums);
    manifest.insert(QStringLiteral("sourcePaths"), sourcePaths);
    manifest.insert(QStringLiteral("sourceFileCount"), 1);
    manifest.insert(QStringLiteral("scriptFileCount"), 1);
    manifest.insert(QStringLiteral("artifactPaths"), artifactPaths);
    manifest.insert(QStringLiteral("artifactChecksums"), checksums);
    manifest.insert(QStringLiteral("codePath"), QStringLiteral("main.code"));
    manifest.insert(QStringLiteral("codeChecksum"), QString::fromLatin1(fixtureChecksum(code)));
    manifest.insert(QStringLiteral("downloadProfilePath"), QStringLiteral("download_profile.json"));
    manifest.insert(QStringLiteral("downloadProfileChecksum"),
                    QString::fromLatin1(fixtureChecksum(profile)));
    manifest.insert(QStringLiteral("runtimePointsPath"), QStringLiteral("runtime_points.json"));
    manifest.insert(QStringLiteral("runtimePointsChecksum"),
                    QString::fromLatin1(fixtureChecksum(points)));
    manifest.insert(QStringLiteral("runtimeManifestPath"), QStringLiteral("runtime_manifest.json"));
    manifest.insert(QStringLiteral("pointCount"), 0);
    manifest.insert(QStringLiteral("parameterCount"), 0);
    return writeFixtureFile(QDir(generationPath).filePath(QStringLiteral("runtime_manifest.json")),
                            QJsonDocument(manifest).toJson(QJsonDocument::Indented));
}

} // namespace

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

    void requestStopDoesNotDisconnectBorrowedBackend()
    {
        RuntimeSessionController ctrl;
        BorrowedTrackingBackend backend;
        QVERIFY(backend.connectBackend());
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        ctrl.requestStop();

        QCOMPARE(backend.disconnectCount, 0);
        QVERIFY(backend.isOnline());
        QCOMPARE(ctrl.state(), RuntimeSessionState::Idle);
    }

    void internalReconnectGateSuppressesMarkedDisconnect()
    {
        RuntimeSessionController ctrl;
        BorrowedTrackingBackend backend;
        QVERIFY(backend.connectBackend());
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        ctrl.m_internalReconnect = true;
        backend.disconnectBackend();
        ctrl.m_internalReconnect = false;

        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
        QVERIFY(!backend.isOnline());
        QVERIFY(backend.connectBackend());
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

    void realBackendDisconnectDuringDownloadFailsTransport()
    {
        RuntimeSessionController ctrl;
        BorrowedTrackingBackend backend;
        QVERIFY(backend.connectBackend());
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        connect(&ctrl, &RuntimeSessionController::downloadStateChanged,
                &ctrl, [&backend](DownloadState, DownloadState newState) {
                    if (newState == DownloadState::Downloading)
                        backend.disconnectBackend();
                });

        QVERIFY(!ctrl.requestDownload(artifactFile.fileName()));
        QCOMPARE(ctrl.state(), RuntimeSessionState::Fault);
        QCOMPARE(ctrl.downloadState(), DownloadState::TransportFailed);
        QVERIFY(backend.disconnectCount >= 1);
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

    void formalAsyncDownloadDeadlineAndUiCancellation_data()
    {
        QTest::addColumn<bool>("lateMatch");
        QTest::addColumn<bool>("autoStart");
        QTest::newRow("late-matching-response-is-timeout") << true << false;
        QTest::newRow("cancel-from-ui-during-inflight-read") << false << false;
        QTest::newRow("automatic-run-waits-for-download") << false << true;
    }
    void formalAsyncDownloadDeadlineAndUiCancellation()
    {
        QFETCH(bool, lateMatch);QFETCH(bool, autoStart);
        QTemporaryDir projectDir; QVERIFY(projectDir.isValid());
        const QByteArray profile = QString(R"({"name":"async-poll","slaveId":1,"steps":[
            {"type":"sendChunk","params":{"dataAddress":210,"chunkWords":1}},
            {"type":"poll","params":{"address":300,"count":1,"expected":[999],"timeoutMs":%1,"pollIntervalMs":5000}}]})")
            .arg(lateMatch ? 200 : 5000).toUtf8();
        QVERIFY(writePublishedBundle(projectDir.path(),"generation-1",QByteArray::fromHex("1234"),profile));
        ProjectController project; QVERIFY(project.openProjectFromPath(projectDir.path()));
        auto& config=project.runtimeConfig();
        config.transport.parameters.insert("port","COM7");
        const QString prefix="build_output/configuration/generations/generation-1/";
        auto& artifact=config.downloadArtifact;
        artifact.filePath=prefix+"main.code"; artifact.checksum=fixtureFileChecksum(projectDir.filePath(artifact.filePath));
        artifact.metadata.insert("generationId","generation-1");
        artifact.metadata.insert("runtimeManifestPath",prefix+"runtime_manifest.json");
        artifact.metadata.insert("downloadProfilePath",prefix+"download_profile.json");
        artifact.metadata.insert("downloadProfileChecksum",fixtureFileChecksum(projectDir.filePath(prefix+"download_profile.json")));
        artifact.metadata.insert("runtimePointsChecksum",fixtureFileChecksum(projectDir.filePath(prefix+"runtime_points.json")));
        artifact.metadata.insert("runtimeManifestChecksum",fixtureFileChecksum(projectDir.filePath(prefix+"runtime_manifest.json")));
        QThread* constructedThread=nullptr;
        ControllerDebugClient client;
        client.enableWorkerThread([&]() -> IControllerDebugTransport* {
            constructedThread=QThread::currentThread();
            if (autoStart) { auto* transport=new FakeRuntimeControllerTransport();transport->registers[300]={999};return transport; }
            return new DelayedRuntimeTransport(lateMatch);
        });
        QVERIFY(constructedThread != QThread::currentThread());
        ControllerDeviceBackend backend;backend.setDebugClientForTest(&client);
        QVERIFY(backend.configure(config));QVERIFY(backend.connectBackend());
        RuntimeSessionController ctrl;ctrl.setProjectController(&project);ctrl.setDeviceBackend(&backend);ctrl.executeRun();
        QSignalSpy finished(&ctrl,&RuntimeSessionController::downloadFinished);
        QElapsedTimer elapsed;elapsed.start();int ticks=0;
        QTimer heartbeat;connect(&heartbeat,&QTimer::timeout,[&](){++ticks;});heartbeat.start(10);
        if (autoStart) {
            ctrl.requestStop();ctrl.m_artifactPath=projectDir.filePath(artifact.filePath);ctrl.executeRun();
            QCOMPARE(ctrl.state(),RuntimeSessionState::Downloading);
        } else QVERIFY(ctrl.requestDownload(projectDir.filePath(artifact.filePath)));
        QVERIFY(elapsed.elapsed()<500);elapsed.restart();QVERIFY(!ctrl.requestDownload(projectDir.filePath(artifact.filePath)));
        if (!lateMatch && !autoStart) QTimer::singleShot(60,&ctrl,&RuntimeSessionController::requestStop);
        QTRY_COMPARE(finished.count(),1);
        QCOMPARE(finished[0][0].toBool(),autoStart);QTRY_VERIFY(!ctrl.m_backendDownloadInProgress);
        QVERIFY(elapsed.elapsed()<(lateMatch ? 800 : 250));if(!autoStart)QVERIFY(ticks>=2);
        if (lateMatch) QCOMPARE(ctrl.downloadState(),DownloadState::TransportFailed);
        else QCOMPARE(ctrl.state(),autoStart ? RuntimeSessionState::Running : RuntimeSessionState::Idle);
        QTest::qWait(30);QCOMPARE(finished.count(),1);
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

        QTemporaryDir projectDir;
        QVERIFY(projectDir.isValid());
        const QByteArray invalidProfile = QByteArrayLiteral(
                "{\"name\":\"bad\",\"slaveId\":1,\"steps\":["
                "{\"type\":\"sendChunk\",\"params\":{\"chunkWords\":200}}]}");
        QVERIFY(writePublishedBundle(projectDir.path(), QStringLiteral("generation-1"),
                                     QByteArray::fromHex("1234"), invalidProfile));
        ProjectController projectController;
        QVERIFY(projectController.openProjectFromPath(projectDir.path()));
        auto& artifact = projectController.runtimeConfig().downloadArtifact;
        artifact.filePath = QStringLiteral(
                "build_output/configuration/generations/generation-1/main.code");
        artifact.checksum = QString::fromLatin1(fixtureChecksum(QByteArray::fromHex("1234")));
        artifact.metadata.insert(QStringLiteral("generationId"), QStringLiteral("generation-1"));
        artifact.metadata.insert(QStringLiteral("runtimeManifestPath"), QStringLiteral(
                "build_output/configuration/generations/generation-1/runtime_manifest.json"));
        artifact.metadata.insert(QStringLiteral("downloadProfilePath"), QStringLiteral(
                "build_output/configuration/generations/generation-1/download_profile.json"));
        const QString generationPath = projectDir.filePath(
                QStringLiteral("build_output/configuration/generations/generation-1"));
        artifact.metadata.insert(QStringLiteral("downloadProfileChecksum"),
                                 fixtureFileChecksum(QDir(generationPath).filePath(
                                         QStringLiteral("download_profile.json"))));
        artifact.metadata.insert(QStringLiteral("runtimePointsChecksum"),
                                 fixtureFileChecksum(QDir(generationPath).filePath(
                                         QStringLiteral("runtime_points.json"))));
        artifact.metadata.insert(QStringLiteral("runtimeManifestChecksum"),
                                 fixtureFileChecksum(QDir(generationPath).filePath(
                                         QStringLiteral("runtime_manifest.json"))));

        RuntimeSessionController ctrl;
        ctrl.setProjectController(&projectController);
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::runtimeError);
        QSignalSpy diagnosticSpy(&ctrl, &RuntimeSessionController::downloadDiagnosticChanged);
        const QString artifactPath = projectDir.filePath(artifact.filePath);
        const bool ok = ctrl.requestDownload(artifactPath);

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

        QTemporaryDir projectDir;
        QVERIFY(projectDir.isValid());
        QVERIFY(writePublishedBundle(projectDir.path(), QStringLiteral("generation-1"),
                                     QByteArray::fromHex("1234")));
        const QString profilePath = projectDir.filePath(
                QStringLiteral("build_output/configuration/generations/generation-1/download_profile.json"));
        const QString profileChecksum = fixtureFileChecksum(profilePath);
        QVERIFY(QFile::remove(profilePath));
        ProjectController projectController;
        QVERIFY(projectController.openProjectFromPath(projectDir.path()));
        auto& artifact = projectController.runtimeConfig().downloadArtifact;
        artifact.filePath = QStringLiteral(
                "build_output/configuration/generations/generation-1/main.code");
        artifact.checksum = QString::fromLatin1(fixtureChecksum(QByteArray::fromHex("1234")));
        artifact.metadata.insert(QStringLiteral("generationId"), QStringLiteral("generation-1"));
        artifact.metadata.insert(QStringLiteral("runtimeManifestPath"), QStringLiteral(
                "build_output/configuration/generations/generation-1/runtime_manifest.json"));
        artifact.metadata.insert(QStringLiteral("downloadProfilePath"), QStringLiteral(
                "build_output/configuration/generations/generation-1/download_profile.json"));
        const QString generationPath = projectDir.filePath(
                QStringLiteral("build_output/configuration/generations/generation-1"));
        artifact.metadata.insert(QStringLiteral("downloadProfileChecksum"),
                                 profileChecksum);
        artifact.metadata.insert(QStringLiteral("runtimePointsChecksum"),
                                 fixtureFileChecksum(QDir(generationPath).filePath(
                                         QStringLiteral("runtime_points.json"))));
        artifact.metadata.insert(QStringLiteral("runtimeManifestChecksum"),
                                 fixtureFileChecksum(QDir(generationPath).filePath(
                                         QStringLiteral("runtime_manifest.json"))));

        RuntimeSessionController ctrl;
        ctrl.setProjectController(&projectController);
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QSignalSpy errorSpy(&ctrl, &RuntimeSessionController::runtimeError);
        const bool ok = ctrl.requestDownload(projectDir.filePath(artifact.filePath));

        QVERIFY(!ok);
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains(QStringLiteral("profile"), Qt::CaseInsensitive));
        QCOMPARE(ctrl.downloadState(), DownloadState::PrecheckFailed);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
        QCOMPARE(transport.lastWriteAddress, -1);
    }

    void downloadControllerLegacyArtifactStopsBeforeWrite()
    {
        QTemporaryDir projectDir;
        QVERIFY(projectDir.isValid());

        QFile projectConfig(projectDir.filePath(QStringLiteral("project_config.json")));
        QVERIFY(projectConfig.open(QIODevice::WriteOnly | QIODevice::Truncate));
        projectConfig.write("{}");
        projectConfig.close();

        QFile mainScript(projectDir.filePath(QStringLiteral("main.lh")));
        QVERIFY(mainScript.open(QIODevice::WriteOnly | QIODevice::Truncate));
        mainScript.write("PROGRAM Main\nEND_PROGRAM\n");
        mainScript.close();

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

        transport.lastReadAddress = -1;
        transport.lastReadCount = 0;
        transport.lastWriteAddress = -1;
        transport.lastWriteValues.clear();

        QSignalSpy errorSpy(&ctrl, &RuntimeSessionController::runtimeError);
        QVERIFY(!ctrl.requestDownload(artifactPath));
        QCOMPARE(ctrl.downloadState(), DownloadState::PrecheckFailed);
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains(QStringLiteral("generationId")));
        QCOMPARE(transport.lastReadAddress, -1);
        QCOMPARE(transport.lastWriteAddress, -1);
    }

    void downloadVirtualLegacyArtifactRemainsCompatible()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        QVERIFY(backend.connectBackend());
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QTemporaryFile artifactFile;
        QVERIFY(artifactFile.open());
        artifactFile.write(QByteArrayLiteral("legacy"));
        artifactFile.flush();

        QVERIFY(ctrl.requestDownload(artifactFile.fileName()));
        QCOMPARE(ctrl.downloadState(), DownloadState::Succeeded);
    }

    void downloadControllerPublishedBundleSucceeds()
    {
        QTemporaryDir projectDir;
        QVERIFY(projectDir.isValid());
        QVERIFY(writePublishedBundle(projectDir.path(), QStringLiteral("generation-1"),
                                     QByteArray::fromHex("1234")));

        ProjectController projectController;
        QVERIFY(projectController.openProjectFromPath(projectDir.path()));
        auto& artifact = projectController.runtimeConfig().downloadArtifact;
        artifact.filePath = QStringLiteral(
                "build_output/configuration/generations/generation-1/main.code");
        artifact.checksum = QString::fromLatin1(fixtureChecksum(QByteArray::fromHex("1234")));
        artifact.metadata.insert(QStringLiteral("generationId"), QStringLiteral("generation-1"));
        artifact.metadata.insert(QStringLiteral("runtimeManifestPath"), QStringLiteral(
                "build_output/configuration/generations/generation-1/runtime_manifest.json"));
        artifact.metadata.insert(QStringLiteral("downloadProfilePath"), QStringLiteral(
                "build_output/configuration/generations/generation-1/download_profile.json"));
        const QString generationPath = projectDir.filePath(
                QStringLiteral("build_output/configuration/generations/generation-1"));
        artifact.metadata.insert(QStringLiteral("downloadProfileChecksum"),
                                 fixtureFileChecksum(QDir(generationPath).filePath(
                                         QStringLiteral("download_profile.json"))));
        artifact.metadata.insert(QStringLiteral("runtimePointsChecksum"),
                                 fixtureFileChecksum(QDir(generationPath).filePath(
                                         QStringLiteral("runtime_points.json"))));
        artifact.metadata.insert(QStringLiteral("runtimeManifestChecksum"),
                                 fixtureFileChecksum(QDir(generationPath).filePath(
                                         QStringLiteral("runtime_manifest.json"))));

        FakeRuntimeControllerTransport transport;
        ControllerDebugClient client(&transport);
        TrackingControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);
        ProjectRuntimeConfig backendConfig = projectController.runtimeConfig();
        backendConfig.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(backendConfig));
        QVERIFY(backend.connectBackend());

        RuntimeSessionController ctrl;
        ctrl.setProjectController(&projectController);
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        transport.lastReadAddress = -1;
        transport.lastReadCount = 0;
        transport.lastWriteAddress = -1;
        transport.lastWriteValues.clear();

        const QString artifactPath = projectDir.filePath(artifact.filePath);
        QVERIFY(ctrl.requestDownload(artifactPath));
        QCOMPARE(ctrl.downloadState(), DownloadState::Succeeded);
        QCOMPARE(transport.lastWriteAddress, 210);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0x1234}));

        bool backendOnlineWhenStopReturned = false;
        ctrl.m_ownedControllerBackend = &backend;
        transport.onNextWrite = [&]() {
            ctrl.requestStop();
            backendOnlineWhenStopReturned = backend.isOnline();
        };
        QSignalSpy finishedSpy(&ctrl, &RuntimeSessionController::downloadFinished);

        QVERIFY(!ctrl.requestDownload(artifactPath));
        QVERIFY(backendOnlineWhenStopReturned);
        QVERIFY(!backend.disconnectDuringDownload);
        QCOMPARE(backend.disconnectCount, 1);
        QVERIFY(!backend.isOnline());
        QCOMPARE(ctrl.state(), RuntimeSessionState::Idle);
        QCOMPARE(ctrl.downloadState(), DownloadState::Idle);
        QCOMPARE(finishedSpy.count(), 1);
        ctrl.m_ownedControllerBackend = nullptr;
    }

    void writeDownloadArtifactPreservesDownloadProfileMetadata()
    {
        QTemporaryDir projectDir;
        QVERIFY(projectDir.isValid());
        QVERIFY(writePublishedBundle(projectDir.path(), QStringLiteral("generation-1")));
        const QString sourceProfilePath = projectDir.filePath(
                QStringLiteral("profiles/download_profile.json"));
        QVERIFY(QDir().mkpath(QFileInfo(sourceProfilePath).absolutePath()));
        QVERIFY(writeFixtureFile(sourceProfilePath,
                                 QByteArrayLiteral("{\"steps\":[{\"type\":\"sendChunk\","
                                                   "\"params\":{\"dataAddress\":210,"
                                                   "\"chunkWords\":1}}]}")));

        ProjectRuntimeConfig config;
        config.downloadArtifact.metadata.insert(QStringLiteral("downloadProfileSourcePath"),
                                                QStringLiteral("profiles/download_profile.json"));
        config.downloadArtifact.metadata.insert(QStringLiteral("projectOwned"), true);

        CompileArtifact artifact;
        artifact.type = QStringLiteral("download");
        artifact.format = QStringLiteral("dsl_custom");
        artifact.path = projectDir.filePath(
                QStringLiteral("build_output/configuration/generations/generation-1/main.code"));
        artifact.checksum = QString::fromLatin1(fixtureChecksum(QByteArrayLiteral("payload")));
        artifact.metadata.insert(QStringLiteral("generationId"), QStringLiteral("generation-1"));
        CompileResult result;
        result.success = true;
        result.artifacts.append(artifact);

        QVERIFY(RunController::writeDownloadArtifact(config, projectDir.path(), result));
        QCOMPARE(config.downloadArtifact.metadata.value(QStringLiteral("downloadProfilePath")).toString(),
                 QStringLiteral("build_output/configuration/generations/generation-1/download_profile.json"));
        QCOMPARE(config.downloadArtifact.metadata.value(QStringLiteral("downloadProfileSourcePath")).toString(),
                 QStringLiteral("profiles/download_profile.json"));
        QCOMPARE(config.downloadArtifact.metadata.value(QStringLiteral("projectOwned")).toBool(), true);
        QVERIFY(!config.downloadArtifact.metadata.contains(QStringLiteral("sourceFile")));
    }

    void staleGenerationWithoutManifestIsNotConsumed()
    {
        QTemporaryDir projectDir;
        QVERIFY(projectDir.isValid());
        const QString generationPath = projectDir.filePath(
                QStringLiteral("build_output/configuration/generations/stale"));
        QVERIFY(QDir().mkpath(generationPath));
        QVERIFY(writeFixtureFile(QDir(generationPath).filePath(QStringLiteral("main.code")),
                                 QByteArrayLiteral("payload")));

        ProjectRuntimeConfig config;
        config.downloadArtifact.filePath =
                QStringLiteral("build_output/configuration/generations/stale/main.code");
        config.downloadArtifact.checksum =
                QString::fromLatin1(fixtureChecksum(QByteArrayLiteral("payload")));
        config.downloadArtifact.metadata.insert(QStringLiteral("generationId"), QStringLiteral("stale"));
        config.downloadArtifact.metadata.insert(
                QStringLiteral("runtimeManifestPath"),
                QStringLiteral("build_output/configuration/generations/stale/runtime_manifest.json"));

        QCOMPARE(RunController::findDownloadArtifactPath(config, projectDir.path(), CompileResult()),
                 QString());
        const auto report = RunController::validateDownloadArtifact(
                config,
                projectDir.path(),
                QDir(generationPath).filePath(QStringLiteral("main.code")));
        QVERIFY(!report.valid);
    }

    void failedPublicationKeepsExistingConfigPointer()
    {
        QTemporaryDir projectDir;
        QVERIFY(projectDir.isValid());
        QVERIFY(writePublishedBundle(projectDir.path(), QStringLiteral("old-generation")));
        const QString oldCode = QStringLiteral(
                "build_output/configuration/generations/old-generation/main.code");
        const QString oldManifest = QStringLiteral(
                "build_output/configuration/generations/old-generation/runtime_manifest.json");

        const QString newGeneration = projectDir.filePath(
                QStringLiteral("build_output/configuration/generations/new-generation"));
        QVERIFY(QDir().mkpath(newGeneration));
        const QString newCode = QDir(newGeneration).filePath(QStringLiteral("main.code"));
        QVERIFY(writeFixtureFile(newCode, QByteArrayLiteral("new-payload")));

        ProjectRuntimeConfig config;
        config.downloadArtifact.filePath = oldCode;
        config.downloadArtifact.checksum =
                QString::fromLatin1(fixtureChecksum(QByteArrayLiteral("payload")));
        config.downloadArtifact.metadata.insert(QStringLiteral("generationId"),
                                                QStringLiteral("old-generation"));
        config.downloadArtifact.metadata.insert(QStringLiteral("runtimeManifestPath"), oldManifest);

        CompileArtifact artifact;
        artifact.type = QStringLiteral("download");
        artifact.format = QStringLiteral("dsl_custom");
        artifact.path = newCode;
        artifact.metadata.insert(QStringLiteral("generationId"), QStringLiteral("new-generation"));
        CompileResult result;
        result.success = true;
        result.artifacts.append(artifact);

        QVERIFY(!RunController::writeDownloadArtifact(config, projectDir.path(), result));
        QCOMPARE(config.downloadArtifact.filePath, oldCode);
        QCOMPARE(config.downloadArtifact.metadata.value(QStringLiteral("generationId")).toString(),
                 QStringLiteral("old-generation"));
        QCOMPARE(config.downloadArtifact.metadata.value(QStringLiteral("runtimeManifestPath")).toString(),
                 oldManifest);
    }

    void projectRelativePublishedBundleResolvesAfterMove()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString originalPath = QDir(tempDir.path()).filePath(QStringLiteral("original"));
        const QString movedPath = QDir(tempDir.path()).filePath(QStringLiteral("moved"));
        QVERIFY(QDir().mkpath(originalPath));
        QVERIFY(writePublishedBundle(originalPath, QStringLiteral("generation-1")));

        ProjectRuntimeConfig config;
        config.downloadArtifact.filePath =
                QStringLiteral("build_output/configuration/generations/generation-1/main.code");
        config.downloadArtifact.checksum =
                QString::fromLatin1(fixtureChecksum(QByteArrayLiteral("payload")));
        config.downloadArtifact.metadata.insert(QStringLiteral("generationId"), QStringLiteral("generation-1"));
        config.downloadArtifact.metadata.insert(
                QStringLiteral("runtimeManifestPath"),
                QStringLiteral("build_output/configuration/generations/generation-1/runtime_manifest.json"));
        config.downloadArtifact.metadata.insert(
                QStringLiteral("downloadProfilePath"),
                QStringLiteral("build_output/configuration/generations/generation-1/download_profile.json"));

        QVERIFY(QDir().rename(originalPath, movedPath));
        QCOMPARE(RunController::findDownloadArtifactPath(config, movedPath, CompileResult()),
                 QDir(movedPath).filePath(
                         QStringLiteral("build_output/configuration/generations/generation-1/main.code")));
    }

    void publishedGenerationRejectsProfileOverride()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        QVERIFY(backend.connectBackend());
        ctrl.setDeviceBackend(&backend);
        ProjectController projectController;
        projectController.runtimeConfig().downloadArtifact.metadata.insert(
                QStringLiteral("generationId"), QStringLiteral("generation-1"));
        projectController.runtimeConfig().downloadArtifact.metadata.insert(
                QStringLiteral("runtimeManifestPath"),
                QStringLiteral("build_output/configuration/generations/generation-1/runtime_manifest.json"));
        ctrl.setProjectController(&projectController);
        ctrl.executeRun();

        QTemporaryFile overrideProfile;
        QVERIFY(overrideProfile.open());
        overrideProfile.write(QByteArrayLiteral("{\"steps\":[{\"type\":\"sendChunk\","
                                                "\"params\":{\"dataAddress\":210,"
                                                "\"chunkWords\":1}}]}"));
        overrideProfile.flush();

        QSignalSpy errorSpy(&ctrl, &RuntimeSessionController::runtimeError);
        QVERIFY(!ctrl.requestDownload(QStringLiteral("unused.code"),
                                      {{QStringLiteral("downloadProfilePath"),
                                        overrideProfile.fileName()}}));
        QCOMPARE(ctrl.downloadState(), DownloadState::PrecheckFailed);
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains(QStringLiteral("override")));
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

    void destroyingReplacedBackendDoesNotClearCurrentBackend()
    {
        RuntimeSessionController ctrl;
        auto* backendA = new VirtualDeviceBackend;
        auto* backendB = new VirtualDeviceBackend;

        ctrl.setDeviceBackend(backendA);
        ctrl.setDeviceBackend(backendB);
        delete backendA;

        QCOMPARE(ctrl.deviceBackend(), backendB);
        QCOMPARE(Monitor::MonitorManager::instance().deviceBackend(), backendB);

        delete backendB;
        QCOMPARE(ctrl.deviceBackend(), nullptr);
        QCOMPARE(Monitor::MonitorManager::instance().deviceBackend(), nullptr);
    }

    void destroyingBackendDuringOpcWriteFailsOnceWithoutGoodUpdate()
    {
        RuntimeSessionController ctrl;
        ParameterController parameterController;
        ParameterDefinition def;
        def.id = QStringLiteral("param.kp");
        def.name = QStringLiteral("Kp");
        def.dataType = QStringLiteral("REAL");
        def.defaultValue = QStringLiteral("1.0");
        def.currentValue = def.defaultValue;
        def.onlineEditable = true;
        parameterController.loadDefinitions({def});

        auto* backend = new VirtualDeviceBackend;
        RuntimePointDefinition point = RuntimePointConverter::fromParameter(def);
        point.access = RuntimePointAccess::ReadWrite;
        backend->loadPointDefinitions({point});
        backend->connectBackend();

        auto* opc = new TestOpcServer;
        ctrl.setDeviceBackend(backend);
        ctrl.setParameterController(&parameterController);
        ctrl.setOpcServer(opc);
        ctrl.handleOpcWriteRequest(QStringLiteral("param.kp"), QVariant(3.5));
        QVERIFY(ctrl.m_pendingOpcWriteActive);

        delete backend;

        QCOMPARE(opc->m_recordCount, 1);
        QVERIFY(!opc->m_lastWriteSuccess);
        QCOMPARE(opc->m_updatedValues.size(), 0);
        QVERIFY(!ctrl.m_pendingOpcWriteActive);
        QVERIFY(ctrl.m_pendingOpcPointId.isEmpty());
        QVERIFY(ctrl.m_pendingOpcParameterName.isEmpty());
        QCOMPARE(parameterController.parameterState(QStringLiteral("Kp")).state,
                 ParameterState::Timeout);

        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCOMPARE(opc->m_recordCount, 1);
        QCOMPARE(opc->m_updatedValues.size(), 0);
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

        QCOMPARE(opc->m_recordCount, 0);
        QCOMPARE(opc->m_updatedValues.size(), 0);
        QTRY_COMPARE(opc->m_recordCount, 1);
        QCOMPARE(opc->m_lastWritePointId, QStringLiteral("param.kp"));
        QVERIFY(opc->m_lastWriteSuccess);
        QVERIFY(opc->m_lastWriteMessage.contains(QStringLiteral("OPC 写入成功")));
        QCOMPARE(opc->m_updatedValues.size(), 1);
        QCOMPARE(opc->m_updatedValues.first().value.toString(), QStringLiteral("3.5"));
    }

    void opcWriteOnlyAppliesRequestedPoint()
    {
        RuntimeSessionController ctrl;
        ParameterController parameterController;
        ParameterDefinition first;
        first.id = QStringLiteral("param.a");
        first.name = QStringLiteral("A");
        first.dataType = QStringLiteral("REAL");
        first.defaultValue = QStringLiteral("1.0");
        first.currentValue = first.defaultValue;
        first.onlineEditable = true;
        ParameterDefinition second = first;
        second.id = QStringLiteral("param.b");
        second.name = QStringLiteral("B");
        parameterController.loadDefinitions({first, second});
        parameterController.editParameter(QStringLiteral("B"), QStringLiteral("8.0"));

        VirtualDeviceBackend backend;
        RuntimePointDefinition firstPoint = RuntimePointConverter::fromParameter(first);
        firstPoint.access = RuntimePointAccess::ReadWrite;
        RuntimePointDefinition secondPoint = RuntimePointConverter::fromParameter(second);
        secondPoint.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({firstPoint, secondPoint});
        backend.connectBackend();

        auto* opc = new TestOpcServer;
        ctrl.setDeviceBackend(&backend);
        ctrl.setParameterController(&parameterController);
        ctrl.setOpcServer(opc);
        ctrl.handleOpcWriteRequest(QStringLiteral("param.a"), QVariant(3.5));

        QTRY_COMPARE(opc->m_recordCount, 1);
        QVERIFY(opc->m_lastWriteSuccess);
        QCOMPARE(parameterController.parameterState(QStringLiteral("A")).state,
                 ParameterState::Confirmed);
        QCOMPARE(parameterController.parameterState(QStringLiteral("B")).state,
                 ParameterState::Modified);
        QCOMPARE(opc->m_updatedValues.size(), 1);
        QCOMPARE(opc->m_updatedValues.first().pointId, QStringLiteral("param.a"));
    }

    void opcWriteFailureDoesNotPublishGood()
    {
        RuntimeSessionController ctrl;
        ParameterController parameterController;
        ParameterDefinition def;
        def.id = QStringLiteral("param.kp");
        def.name = QStringLiteral("Kp");
        def.dataType = QStringLiteral("REAL");
        def.defaultValue = QStringLiteral("1.0");
        def.currentValue = def.defaultValue;
        def.onlineEditable = true;
        parameterController.loadDefinitions({def});

        VirtualDeviceBackend backend;
        RuntimePointDefinition point = RuntimePointConverter::fromParameter(def);
        point.access = RuntimePointAccess::ReadOnly;
        backend.loadPointDefinitions({point});
        backend.connectBackend();

        auto* opc = new TestOpcServer;
        ctrl.setDeviceBackend(&backend);
        ctrl.setParameterController(&parameterController);
        ctrl.setOpcServer(opc);
        ctrl.handleOpcWriteRequest(QStringLiteral("param.kp"), QVariant(3.5));

        QCOMPARE(opc->m_recordCount, 1);
        QVERIFY(!opc->m_lastWriteSuccess);
        QCOMPARE(opc->m_updatedValues.size(), 0);
        QCOMPARE(parameterController.parameterState(QStringLiteral("Kp")).state,
                 ParameterState::ApplyFailed);
    }

    void opcWriteReadbackTimeoutDoesNotPublishGood()
    {
        RuntimeSessionController ctrl;
        ParameterController parameterController;
        ParameterDefinition def;
        def.id = QStringLiteral("param.kp");
        def.name = QStringLiteral("Kp");
        def.dataType = QStringLiteral("REAL");
        def.defaultValue = QStringLiteral("1.0");
        def.currentValue = def.defaultValue;
        def.onlineEditable = true;
        parameterController.loadDefinitions({def});

        VirtualDeviceBackend backend;
        RuntimePointDefinition point = RuntimePointConverter::fromParameter(def);
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();
        backend.setFaultInjection(true, false, false);

        auto* opc = new TestOpcServer;
        ctrl.setDeviceBackend(&backend);
        ctrl.setParameterController(&parameterController);
        ctrl.setOpcServer(opc);
        ctrl.handleOpcWriteRequest(QStringLiteral("param.kp"), QVariant(3.5));

        QCOMPARE(opc->m_recordCount, 0);
        QCOMPARE(opc->m_updatedValues.size(), 0);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCOMPARE(opc->m_recordCount, 1);
        QVERIFY(!opc->m_lastWriteSuccess);
        QVERIFY(opc->m_lastWriteMessage.contains(QStringLiteral("回读失败")));
        QCOMPARE(opc->m_updatedValues.size(), 0);
        QCOMPARE(parameterController.parameterState(QStringLiteral("Kp")).state,
                 ParameterState::Timeout);
    }

    void opcWriteReadbackMismatchDoesNotPublishGood()
    {
        RuntimeSessionController ctrl;
        ParameterController parameterController;
        ParameterDefinition def;
        def.id = QStringLiteral("param.kp");
        def.name = QStringLiteral("Kp");
        def.dataType = QStringLiteral("REAL");
        def.defaultValue = QStringLiteral("1.0");
        def.currentValue = def.defaultValue;
        def.onlineEditable = true;
        parameterController.loadDefinitions({def});

        VirtualDeviceBackend backend;
        RuntimePointDefinition point = RuntimePointConverter::fromParameter(def);
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();

        auto* opc = new TestOpcServer;
        ctrl.setDeviceBackend(&backend);
        ctrl.setParameterController(&parameterController);
        ctrl.setOpcServer(opc);
        ctrl.handleOpcWriteRequest(QStringLiteral("param.kp"), QVariant(3.5));

        QCOMPARE(opc->m_recordCount, 0);
        QCOMPARE(opc->m_updatedValues.size(), 0);
        QCOMPARE(parameterController.parameterState(QStringLiteral("Kp")).state,
                 ParameterState::PendingReadback);
        QHash<QString, QVariant> mismatch;
        mismatch.insert(QStringLiteral("param.kp"), QVariant(999.0));
        parameterController.onReadbackValues(mismatch);
        QCOMPARE(opc->m_recordCount, 0);
        QCOMPARE(opc->m_updatedValues.size(), 0);

        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCOMPARE(opc->m_recordCount, 1);
        QVERIFY(!opc->m_lastWriteSuccess);
        QVERIFY(opc->m_lastWriteMessage.contains(QStringLiteral("不匹配")));
        QCOMPARE(opc->m_updatedValues.size(), 0);
        QCOMPARE(parameterController.parameterState(QStringLiteral("Kp")).state,
                 ParameterState::Mismatch);
    }

    void opcWriteReentryIsRejected()
    {
        RuntimeSessionController ctrl;
        ParameterController parameterController;
        ParameterDefinition first;
        first.id = QStringLiteral("param.a");
        first.name = QStringLiteral("A");
        first.dataType = QStringLiteral("REAL");
        first.defaultValue = QStringLiteral("1.0");
        first.currentValue = first.defaultValue;
        first.onlineEditable = true;
        ParameterDefinition second = first;
        second.id = QStringLiteral("param.b");
        second.name = QStringLiteral("B");
        parameterController.loadDefinitions({first, second});

        VirtualDeviceBackend backend;
        RuntimePointDefinition firstPoint = RuntimePointConverter::fromParameter(first);
        RuntimePointDefinition secondPoint = RuntimePointConverter::fromParameter(second);
        firstPoint.access = RuntimePointAccess::ReadWrite;
        secondPoint.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({firstPoint, secondPoint});
        backend.connectBackend();

        auto* opc = new TestOpcServer;
        ctrl.setDeviceBackend(&backend);
        ctrl.setParameterController(&parameterController);
        ctrl.setOpcServer(opc);
        ctrl.handleOpcWriteRequest(QStringLiteral("param.a"), QVariant(3.5));
        ctrl.handleOpcWriteRequest(QStringLiteral("param.b"), QVariant(4.5));

        QTRY_COMPARE(opc->m_recordCount, 2);
        QCOMPARE(opc->m_lastWritePointId, QStringLiteral("param.a"));
        QVERIFY(opc->m_lastWriteSuccess);
        QVERIFY(opc->m_recordSuccesses.contains(false));
        QCOMPARE(parameterController.parameterState(QStringLiteral("B")).state,
                 ParameterState::Clean);
    }

    void opcStopOrSwitchDoesNotCancelIndependentUiReadback()
    {
        for (const bool switchServer : {false, true}) {
            RuntimeSessionController ctrl;
            ParameterController parameterController;
            ParameterDefinition def;
            def.id = QStringLiteral("param.kp");
            def.name = QStringLiteral("Kp");
            def.dataType = QStringLiteral("REAL");
            def.defaultValue = QStringLiteral("1.0");
            def.currentValue = def.defaultValue;
            def.onlineEditable = true;
            parameterController.loadDefinitions({def});

            VirtualDeviceBackend backend;
            RuntimePointDefinition point = RuntimePointConverter::fromParameter(def);
            point.access = RuntimePointAccess::ReadWrite;
            backend.loadPointDefinitions({point});
            backend.connectBackend();
            backend.setFaultInjection(true, false, false);

            ctrl.setDeviceBackend(&backend);
            ctrl.setParameterController(&parameterController);
            ctrl.setOpcServer(new TestOpcServer);
            parameterController.editParameter(QStringLiteral("Kp"), QStringLiteral("2.0"));
            QSignalSpy finishedSpy(&parameterController, &ParameterController::readbackFinished);
            QVERIFY(parameterController.applyModifiedParametersWithReadbackAsync(&backend, 1, 0));

            if (switchServer) {
                ctrl.setOpcServer(new TestOpcServer);
            } else {
                ctrl.stopOpcServer();
            }
            QCOMPARE(finishedSpy.count(), 0);
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            QCOMPARE(finishedSpy.count(), 1);
            QCOMPARE(finishedSpy.first().at(0).toBool(), false);
        }
    }

    void opcPendingWriteClearsOnSessionStop()
    {
        RuntimeSessionController ctrl;
        ParameterController parameterController;
        ParameterDefinition def;
        def.id = QStringLiteral("param.kp");
        def.name = QStringLiteral("Kp");
        def.dataType = QStringLiteral("REAL");
        def.defaultValue = QStringLiteral("1.0");
        def.currentValue = def.defaultValue;
        def.onlineEditable = true;
        parameterController.loadDefinitions({def});
        VirtualDeviceBackend backend;
        RuntimePointDefinition point = RuntimePointConverter::fromParameter(def);
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();
        auto* opc = new TestOpcServer;
        ctrl.setDeviceBackend(&backend);
        ctrl.setParameterController(&parameterController);
        ctrl.setOpcServer(opc);

        ctrl.handleOpcWriteRequest(QStringLiteral("param.kp"), QVariant(3.5));
        ctrl.requestStop();

        QCOMPARE(opc->m_recordCount, 1);
        QVERIFY(!opc->m_lastWriteSuccess);
        QVERIFY(!ctrl.m_pendingOpcWriteActive);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCOMPARE(opc->m_recordCount, 1);
    }

    void opcPendingWriteClearsOnControllerSwitch()
    {
        RuntimeSessionController ctrl;
        ParameterController firstController;
        ParameterController secondController;
        ParameterDefinition def;
        def.id = QStringLiteral("param.kp");
        def.name = QStringLiteral("Kp");
        def.dataType = QStringLiteral("REAL");
        def.defaultValue = QStringLiteral("1.0");
        def.currentValue = def.defaultValue;
        def.onlineEditable = true;
        firstController.loadDefinitions({def});
        secondController.loadDefinitions({def});
        VirtualDeviceBackend backend;
        RuntimePointDefinition point = RuntimePointConverter::fromParameter(def);
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();
        auto* opc = new TestOpcServer;
        ctrl.setDeviceBackend(&backend);
        ctrl.setParameterController(&firstController);
        ctrl.setOpcServer(opc);

        ctrl.handleOpcWriteRequest(QStringLiteral("param.kp"), QVariant(3.5));
        ctrl.setParameterController(&secondController);

        QCOMPARE(opc->m_recordCount, 1);
        QVERIFY(!opc->m_lastWriteSuccess);
        QVERIFY(!ctrl.m_pendingOpcWriteActive);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCOMPARE(opc->m_recordCount, 1);
    }

    void opcPendingWriteClearsOnBackendSwitch()
    {
        RuntimeSessionController ctrl;
        ParameterController parameterController;
        ParameterDefinition def;
        def.id = QStringLiteral("param.kp");
        def.name = QStringLiteral("Kp");
        def.dataType = QStringLiteral("REAL");
        def.defaultValue = QStringLiteral("1.0");
        def.currentValue = def.defaultValue;
        def.onlineEditable = true;
        parameterController.loadDefinitions({def});
        VirtualDeviceBackend backend;
        RuntimePointDefinition point = RuntimePointConverter::fromParameter(def);
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();
        auto* opc = new TestOpcServer;
        ctrl.setDeviceBackend(&backend);
        ctrl.setParameterController(&parameterController);
        ctrl.setOpcServer(opc);

        ctrl.handleOpcWriteRequest(QStringLiteral("param.kp"), QVariant(3.5));
        ctrl.setDeviceBackend(nullptr);

        QCOMPARE(opc->m_recordCount, 1);
        QVERIFY(!opc->m_lastWriteSuccess);
        QVERIFY(!ctrl.m_pendingOpcWriteActive);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCOMPARE(opc->m_recordCount, 1);
    }

    void opcPendingWriteClearsOnOpcServerSwitch()
    {
        RuntimeSessionController ctrl;
        ParameterController parameterController;
        ParameterDefinition def;
        def.id = QStringLiteral("param.kp");
        def.name = QStringLiteral("Kp");
        def.dataType = QStringLiteral("REAL");
        def.defaultValue = QStringLiteral("1.0");
        def.currentValue = def.defaultValue;
        def.onlineEditable = true;
        parameterController.loadDefinitions({def});
        VirtualDeviceBackend backend;
        RuntimePointDefinition point = RuntimePointConverter::fromParameter(def);
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();
        auto* oldOpc = new TestOpcServer;
        auto* newOpc = new TestOpcServer;
        ctrl.setDeviceBackend(&backend);
        ctrl.setParameterController(&parameterController);
        ctrl.setOpcServer(oldOpc);

        ctrl.handleOpcWriteRequest(QStringLiteral("param.kp"), QVariant(3.5));
        ctrl.setOpcServer(newOpc);

        QCOMPARE(oldOpc->m_recordCount, 1);
        QVERIFY(!oldOpc->m_lastWriteSuccess);
        QVERIFY(!ctrl.m_pendingOpcWriteActive);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCOMPARE(oldOpc->m_recordCount, 1);
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

    void manifestSourceModificationRejectsRunBundle()
    {
        QTemporaryDir projectDir;
        QVERIFY(projectDir.isValid());
        QVERIFY(writePublishedBundle(projectDir.path(), QStringLiteral("generation-1")));

        ProjectRuntimeConfig config;
        config.downloadArtifact.filePath =
                QStringLiteral("build_output/configuration/generations/generation-1/main.code");
        config.downloadArtifact.checksum =
                QString::fromLatin1(fixtureChecksum(QByteArrayLiteral("payload")));
        config.downloadArtifact.metadata.insert(QStringLiteral("generationId"),
                                                QStringLiteral("generation-1"));
        config.downloadArtifact.metadata.insert(
                QStringLiteral("runtimeManifestPath"),
                QStringLiteral("build_output/configuration/generations/generation-1/runtime_manifest.json"));

        auto report = RunController::validateDownloadArtifact(
                config,
                projectDir.path(),
                QDir(projectDir.path()).filePath(config.downloadArtifact.filePath));
        QVERIFY2(report.valid, qPrintable(report.errors.join(QLatin1Char(';'))));

        const QString mainPath = QDir(projectDir.path()).filePath(QStringLiteral("main.lh"));
        QVERIFY(writeFixtureFile(mainPath, QByteArrayLiteral("PROGRAM Main\n// modified\nEND_PROGRAM\n")));

        report = RunController::validateDownloadArtifact(
                config,
                projectDir.path(),
                QDir(projectDir.path()).filePath(config.downloadArtifact.filePath));
        QVERIFY(!report.valid);
        QVERIFY(report.errors.join(QLatin1Char(' ')).contains(QStringLiteral("checksum 不一致")));
    }

    void manifestMissingOrMismatchedSourceChecksumsRejectsRunBundle()
    {
        QTemporaryDir projectDir;
        QVERIFY(projectDir.isValid());
        QVERIFY(writePublishedBundle(projectDir.path(), QStringLiteral("generation-1")));

        const QString manifestPath = QDir(projectDir.path()).filePath(
                QStringLiteral("build_output/configuration/generations/generation-1/runtime_manifest.json"));
        QFile manifestFile(manifestPath);
        QVERIFY(manifestFile.open(QIODevice::ReadOnly));
        QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
        manifestFile.close();

        ProjectRuntimeConfig config;
        config.downloadArtifact.filePath =
                QStringLiteral("build_output/configuration/generations/generation-1/main.code");
        config.downloadArtifact.checksum =
                QString::fromLatin1(fixtureChecksum(QByteArrayLiteral("payload")));
        config.downloadArtifact.metadata.insert(QStringLiteral("generationId"),
                                                QStringLiteral("generation-1"));
        config.downloadArtifact.metadata.insert(
                QStringLiteral("runtimeManifestPath"),
                QStringLiteral("build_output/configuration/generations/generation-1/runtime_manifest.json"));

        // 缺少 sourceChecksums
        QJsonObject badManifest1 = manifest;
        badManifest1.remove(QStringLiteral("sourceChecksums"));
        QVERIFY(writeFixtureFile(manifestPath, QJsonDocument(badManifest1).toJson()));

        auto report = RunController::validateDownloadArtifact(
                config,
                projectDir.path(),
                QDir(projectDir.path()).filePath(config.downloadArtifact.filePath));
        QVERIFY(!report.valid);

        // sourceChecksums 含有空 hash
        QJsonObject badManifest2 = manifest;
        QJsonObject emptyHashChecksums;
        emptyHashChecksums.insert(QStringLiteral("main.lh"), QString());
        badManifest2.insert(QStringLiteral("sourceChecksums"), emptyHashChecksums);
        QVERIFY(writeFixtureFile(manifestPath, QJsonDocument(badManifest2).toJson()));

        report = RunController::validateDownloadArtifact(
                config,
                projectDir.path(),
                QDir(projectDir.path()).filePath(config.downloadArtifact.filePath));
        QVERIFY(!report.valid);
    }
};

QTEST_MAIN(RuntimeSessionControllerTest)
#include "runtime_session_controller_test.moc"
