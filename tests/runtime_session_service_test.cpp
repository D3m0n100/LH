#include <QtTest>
#include <QTemporaryDir>
#include "RuntimeSessionService.h"
#include "VirtualDeviceBackend.h"

class TestMonitorPort : public RuntimeMonitorPort {
public:
    IDeviceBackend* backend = nullptr;
    bool running = false;
    int stops = 0;
    void setDeviceBackend(IDeviceBackend* value) override { backend = value; }
    bool applyConfiguration(const ProjectRuntimeConfig&) override { return true; }
    QStringList providerIds() const override { return {}; }
    void startMonitoring() override { running = true; }
    void stopMonitoring() override { running = false; ++stops; }
    bool isMonitoring() const override { return running; }
    void removeDemoChannels() override {}
};
class TestHistoryPort : public Monitor::IMonitorHistoryStore {
public:
    int cancellations = 0;
    bool isAvailable() const override { return true; }
    QList<RuntimeRecord> getLatestRecords(const QString&, int) override { return {}; }
    QList<RuntimeRecord> queryHistory(const QString&, const QDateTime&, const QDateTime&) override { return {}; }
    RuntimeHistoryPage queryHistoryPage(const QString&, const QDateTime&, const QDateTime&, int, const RuntimeHistoryCursor&) override { return {}; }
    RuntimeHistoryPage queryLatestHistoryPage(const QString&, int, int, const RuntimeHistoryCursor&, const QDateTime&) override { return {}; }
    RuntimeHistoryCount countHistory(const QString&, const QDateTime&, const QDateTime&, qint64) override { return {}; }
    RuntimeHistoryCount countLatestHistory(const QString&, int, const QDateTime&, qint64) override { return {}; }
    void cancelPendingRequests() override { ++cancellations; }
};
class RuntimeSessionServiceTest : public QObject {
    Q_OBJECT
private slots:
    void twoInstancesIsolateStateCancellationAndParameters() {
        auto monitorA = std::make_shared<TestMonitorPort>();
        auto monitorB = std::make_shared<TestMonitorPort>();
        auto historyA = std::make_shared<TestHistoryPort>();
        auto historyB = std::make_shared<TestHistoryPort>();
        RuntimeSessionService a(monitorA, historyA), b(monitorB, historyB);
        QSignalSpy stateA(&a, &RuntimeSessionService::stateChanged);
        QSignalSpy stateB(&b, &RuntimeSessionService::stateChanged);
        ParameterDefinition parameter;
        parameter.id = "param.kp"; parameter.name = "Kp"; parameter.dataType = "REAL";
        parameter.defaultValue = "0"; parameter.currentValue = "0"; parameter.onlineEditable = true;
        a.parameters()->loadDefinitions({parameter}); b.parameters()->loadDefinitions({parameter});
        QVERIFY(a.parameters()->editParameter("Kp", "2"));
        QCOMPARE(b.parameters()->parameterState("Kp").state, ParameterState::Clean);
        const auto tokenA = a.cancellationToken(), tokenB = b.cancellationToken();
        a.setState(RuntimeSessionState::Monitoring);
        a.setDownloadState(DownloadState::Precheck);
        a.stop();
        QCOMPARE(a.state(), RuntimeSessionState::Idle);
        QCOMPARE(a.downloadState(), DownloadState::Idle);
        QCOMPARE(stateA.count(), 2); QCOMPARE(stateB.count(), 0);
        QVERIFY(tokenA->load()); QVERIFY(!tokenB->load());
        QCOMPARE(a.operationGeneration(), quint64(1)); QCOMPARE(b.operationGeneration(), quint64(0));
        QCOMPARE(historyA->cancellations, 1); QCOMPARE(historyB->cancellations, 0);
        QCOMPARE(monitorA->stops, 1); QCOMPARE(monitorB->stops, 0);
    }
    void parameterApplyAndStopUseInjectedBackend() {
        auto monitor = std::make_shared<TestMonitorPort>();
        RuntimeSessionService service(monitor);
        VirtualDeviceBackend backend;
        RuntimePointDefinition point;
        point.id = "param.kp"; point.name = "Kp"; point.kind = RuntimePointKind::Parameter;
        point.dataType = "REAL"; point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point}); QVERIFY(backend.connectBackend());
        service.setBackend(&backend);
        ParameterDefinition parameter;
        parameter.id = point.id; parameter.name = point.name; parameter.dataType = "REAL";
        parameter.defaultValue = "0"; parameter.currentValue = "0"; parameter.onlineEditable = true;
        service.parameters()->loadDefinitions({parameter});
        QVERIFY(service.parameters()->editParameter("Kp", "2"));
        QSignalSpy finished(service.parameters(), &ParameterController::readbackFinished);
        QVERIFY(service.applyParameters());
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(service.parameters()->parameterState("Kp").state, ParameterState::Confirmed);
        QCOMPARE(monitor->backend, static_cast<IDeviceBackend*>(&backend));
    }
    void backendDestructionCancelsOnlyBoundSession() {
        auto monitor = std::make_shared<TestMonitorPort>();
        RuntimeSessionService service(monitor);
        auto* backend = new VirtualDeviceBackend;
        service.setBackend(backend);
        service.setState(RuntimeSessionState::Connected);
        const auto token = service.cancellationToken();
        delete backend;
        QVERIFY(token->load()); QVERIFY(!service.backend()); QVERIFY(!monitor->backend);
        QCOMPARE(service.state(), RuntimeSessionState::Fault);
    }
    void downloadPrecheckRejectsMissingArtifactWithoutUiOrTransport() {
        RuntimeSessionService service(std::make_shared<TestMonitorPort>());
        QTemporaryDir project;
        QVERIFY(project.isValid());
        const auto report = service.precheck({}, project.path(), project.path() + "/missing.code", true);
        QVERIFY(!report.valid); QVERIFY(!report.errors.isEmpty());
        QCOMPARE(service.state(), RuntimeSessionState::Idle);
    }
};
QTEST_GUILESS_MAIN(RuntimeSessionServiceTest)
#include "runtime_session_service_test.moc"
