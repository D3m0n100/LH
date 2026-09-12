#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QUdpSocket>
#include <thread>
#include "common/FileSnapshot.h"
#include "communication/EthernetInterface.h"
#include "communication/IDeviceBackend.h"
#include "communication/ControllerDeviceBackend.h"
#include "core/AsyncDatabaseWorker.h"
#include "monitor/MonitorManager.h"
#include "monitor/MonitorChartView.h"
#include "monitor/ChartWidget.h"
#include "monitor/SvgExport.h"
#include "designer/ProjectController.h"
#include "designer/DslScriptEditor.h"
#include <QtCharts/QLineSeries>

class PartialReadDevice : public QIODevice {
    bool first = true;
public:
    PartialReadDevice() { open(ReadOnly); }
    bool isSequential() const override { return true; }
    bool atEnd() const override { return false; }
protected:
    qint64 readData(char* data, qint64) override {
        if (first) { first = false; memcpy(data, "prefix", 6); return 6; }
        setErrorString("injected read failure"); return -1;
    }
    qint64 writeData(const char*, qint64) override { return -1; }
};

class BackupFailingProject : public ProjectController {
protected:
    bool readScriptBackup(const QString&, QByteArray* bytes, QString* error) override {
        PartialReadDevice device;
        const bool ok = FileSnapshot::readComplete(device, bytes);
        *error = device.errorString();
        return ok;
    }
};

class DeferredBackend : public IDeviceBackend {
public:
    int reads = 0;
    ReadCompletion pending;
    std::shared_ptr<std::atomic_bool> cancelled;
    bool connectBackend() override { return true; }
    void disconnectBackend() override {}
    bool isOnline() const override { return true; }
    bool supportsAsyncRead() const override { return true; }
    void readPointsAsync(const QStringList&, int budget, std::shared_ptr<std::atomic_bool> token,
                         QObject*, ReadCompletion cb) override {
        Q_ASSERT(budget > 0 && budget <= 1000);
        ++reads; cancelled = token; pending = std::move(cb);
    }
    bool readPoints(const QStringList&, QHash<QString,QVariant>&, QString*, QHash<QString,CommError>*) override { return false; }
    bool writePoints(const QHash<QString,QVariant>&, QString*, QHash<QString,CommError>*) override { return false; }
    bool downloadArtifact(const QString&, const QVariantMap&, QString*, CommError*) override { return false; }
    BackendStatusSnapshot statusSnapshot() const override { BackendStatusSnapshot s; s.online = true; return s; }
};

class SlowDatabaseWorker : public Core::AsyncDatabaseWorker {
protected:
    bool openDatabaseInThread() override {
        QThread::msleep(160);
        return Core::AsyncDatabaseWorker::openDatabaseInThread();
    }
};

class BudgetTransport : public IControllerDebugTransport {
    QDeadlineTimer deadline{QDeadlineTimer::Forever};
    const std::atomic_bool* cancelled = nullptr;
    QVector<quint16> values;
public:
    bool isConnected() const override { return true; }
    int stationAddress() const override { return 1; }
    void setStationAddress(int) override {}
    void setRequestBudget(int ms, const std::atomic_bool* token) override {
        deadline = ms < 0 ? QDeadlineTimer(QDeadlineTimer::Forever) : QDeadlineTimer(ms);
        cancelled = token;
    }
    bool readHoldingRegisters(int, int count) override {
        if (!deadline.isForever()) {
            for (int i = 0; i < 25; ++i) {
                if (deadline.hasExpired() || (cancelled && cancelled->load())) return false;
                QThread::msleep(1);
            }
        }
        values = QVector<quint16>(count, 0); return true;
    }
    QVector<quint16> holdingRegisterValues(int) const override { return values; }
    bool writeMultipleRegisters(int, const QVector<quint16>&) override { return true; }
    CommError lastError() const override {
        return CommError(CommProtocolType::ModbusRTU, CommErrorCode::ReceiveTimeout, "deadline");
    }
};

class OptimizationAcceptanceTest : public QObject {
    Q_OBJECT
private slots:
    void controllerAsyncReadSharesOneDeadlineAndKeepsUiResponsive() {
        ControllerDebugClient client;
        client.enableWorkerThread([]() { return new BudgetTransport; });
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);
        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert("port", "LH-async-read-test");
        QString configurationError;
        QVERIFY2(backend.configure(cfg, &configurationError), qPrintable(configurationError));
        QVERIFY(backend.connectBackend());
        int heartbeat = 0;
        QTimer timer; connect(&timer, &QTimer::timeout, [&]() { ++heartbeat; }); timer.start(5);
        bool done = false, success = true;
        QElapsedTimer elapsed; elapsed.start();
        backend.readPointsAsync({"controller.state"}, 65, std::make_shared<std::atomic_bool>(false),
            this, [&](bool ok, auto, auto, auto, auto) { success = ok; done = true; });
        QVERIFY(elapsed.elapsed() < 40);
        QTRY_VERIFY_WITH_TIMEOUT(done, 500);
        QVERIFY(!success);
        QVERIFY(heartbeat > 2);
        QVERIFY(elapsed.elapsed() < 250);
    }

    void slowDatabaseOpenDoesNotBlockHeartbeat() {
        QTemporaryDir dir;
        SlowDatabaseWorker worker;
        QSignalSpy ready(&worker, &Core::AsyncDatabaseWorker::startupFinished);
        QElapsedTimer elapsed; elapsed.start();
        QVERIFY(worker.startWorkerAsync(dir.filePath("slow.db")));
        QVERIFY(elapsed.elapsed() < 100);
        int heartbeat = 0;
        QTimer timer; connect(&timer, &QTimer::timeout, [&]() { ++heartbeat; }); timer.start(5);
        QTRY_COMPARE(ready.count(), 1);
        QVERIFY(heartbeat > 5);
        QVERIFY(worker.stopWorker());
    }
    void asyncPollDoesNotAccumulateAndDropsStoppedResult() {
        Monitor::MonitorManager manager;
        DeferredBackend backend;
        manager.setDatabaseLoggingEnabled(false);
        manager.setDeviceBackend(&backend);
        ProjectRuntimeConfig cfg;
        MonitorProviderRuntimeConfig provider;
        provider.id = "pt1"; provider.channelName = "channel.1"; provider.periodMs = 10;
        cfg.providers.append(provider);
        QVERIFY(manager.applyConfiguration(cfg));
        QSignalSpy samples(&manager, &Monitor::MonitorManager::sampleRecorded);
        manager.startMonitoring();
        QTRY_COMPARE(backend.reads, 1);
        int heartbeat = 0;
        QTimer timer;
        connect(&timer, &QTimer::timeout, [&]() { ++heartbeat; });
        timer.start(5);
        QTest::qWait(80);
        QCOMPARE(backend.reads, 1);
        QVERIFY(heartbeat > 3);
        manager.stopMonitoring();
        QVERIFY(backend.cancelled->load());
        backend.pending(true, {{"pt1", 42.0}}, {}, {}, backend.statusSnapshot());
        QCOMPARE(samples.count(), 0);
        manager.startMonitoring();
        QTRY_COMPARE(backend.reads, 2);
        const auto oldResult = backend.pending;
        DeferredBackend replacement;
        manager.setDeviceBackend(&replacement);
        oldResult(true, {{"pt1", 43.0}}, {}, {}, backend.statusSnapshot());
        QCOMPARE(samples.count(), 0);
        manager.setDeviceBackend(nullptr);
    }

    void udpOwnerThreadReceivesDelayedData() {
        QUdpSocket reserve;
        QVERIFY(reserve.bind(QHostAddress(QHostAddress::LocalHost), quint16(0)));
        const quint16 port = reserve.localPort(); reserve.close();
        EthernetInterface receiver;
        EthernetConfig cfg;
        cfg.protocol = EthernetConfig::Protocol::UDP; cfg.role = EthernetConfig::Role::Server;
        cfg.host = "127.0.0.1"; cfg.port = port; cfg.keepAliveInterval = 0;
        QVERIFY(receiver.open(cfg));
        std::thread sender([port]() {
            QThread::msleep(30);
            QUdpSocket socket;
            socket.writeDatagram("probe", 5, QHostAddress::LocalHost, port);
        });
        const auto bytes = receiver.receive(500);
        sender.join();
        QCOMPARE(bytes, QByteArray("probe"));
        QElapsedTimer elapsed; elapsed.start();
        QVERIFY(receiver.receive(30).isEmpty());
        QVERIFY(elapsed.elapsed() < 200);
        receiver.close();
    }

    void backupReadFailurePreservesFilesAndModifiedState() {
        QTemporaryDir dir;
        const QString path = dir.filePath("main.lh");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("PROGRAM P\nEND_PROGRAM\n"); file.close();
        ProjectRuntimeConfig cfg;
        cfg.projectName = "probe"; cfg.mainScriptPath = path; cfg.dslScriptPath = path;
        cfg.scriptFiles = QStringList{path};
        const QByteArray config = QJsonDocument(cfg.toJson()).toJson();
        QFile json(dir.filePath("project_config.json")); QVERIFY(json.open(QIODevice::WriteOnly));
        json.write(config); json.close();
        BackupFailingProject project;
        DslScriptEditor editor;
        project.setDslEditor(&editor);
        QVERIFY(project.openProjectFromPath(dir.path()));
        editor.setScript("PROGRAM Changed\nEND_PROGRAM\n");
        editor.setModified(true); project.setModified(true);
        QVERIFY(!project.saveProject());
        QVERIFY(editor.isModified());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("PROGRAM P\nEND_PROGRAM\n"));
        QVERIFY(json.open(QIODevice::ReadOnly)); QCOMPARE(json.readAll(), config);
    }

    void svgFailureIsAtomicAndNormalSvgParses() {
        QTemporaryDir dir;
        MonitorChartView chart;
        QVERIFY(!chart.exportAsSvg(dir.filePath("missing/chart.svg")));
        const QString path = dir.filePath("chart.svg");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("old"); file.close();
        QVERIFY(!SvgExport::write(&chart, path, [](QSaveFile&) { return false; }));
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("old")); file.close();
        QVERIFY(chart.exportAsSvg(path));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QXmlStreamReader xml(file.readAll());
        while (!xml.atEnd()) xml.readNext();
        QVERIFY(!xml.hasError());
    }

    void batchChartUpdateIsBoundedAndSingleNotification() {
        ChartWidget chart;
        chart.setMaxPointsPerSeries(200);
        QVector<QPointF> points;
        for (int i = 0; i < 1000; ++i) points.append(QPointF(i, i));
        chart.appendPoints("test", {QPointF(-1, 0)});
        auto* view = chart.findChild<QtCharts::QChartView*>();
        QVERIFY(view);
        auto* series = qobject_cast<QtCharts::QLineSeries*>(view->chart()->series().last());
        QVERIFY(series);
        QSignalSpy single(series, &QtCharts::QLineSeries::pointAdded);
        QSignalSpy batch(series, &QtCharts::QLineSeries::pointsReplaced);
        chart.appendPoints("test", points);
        QCOMPARE(single.count(), 0);
        QCOMPARE(batch.count(), 1);
        QCOMPARE(series->count(), 200);
        QCOMPARE(series->at(0), points.at(800));
        QCOMPARE(series->at(series->count()-1), points.last());
    }

    void databaseStartReportsReadyAndFailureAsynchronously() {
        QTemporaryDir dir;
        Monitor::MonitorManager manager;
        QSignalSpy ready(&manager, &Monitor::MonitorManager::databaseServiceStarted);
        QElapsedTimer elapsed; elapsed.start();
        manager.startDatabaseServiceAsync(dir.filePath("valid.db"));
        QVERIFY(elapsed.elapsed() < 150);
        QTRY_COMPARE(ready.count(), 1);
        QVERIFY(ready.first().at(0).toBool());
        manager.shutdown();
        Monitor::MonitorManager invalid;
        QSignalSpy failed(&invalid, &Monitor::MonitorManager::databaseServiceStarted);
        invalid.startDatabaseServiceAsync(dir.filePath("missing/invalid.db"));
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(!failed.first().at(0).toBool());
        QTest::qWait(50); QCOMPARE(failed.count(), 1);
        invalid.startDatabaseServiceAsync(dir.filePath("retry.db"));
        QTRY_COMPARE(failed.count(), 2);
        QVERIFY(failed.last().at(0).toBool());
    }
};
QTEST_MAIN(OptimizationAcceptanceTest)
#include "optimization_acceptance_test.moc"
