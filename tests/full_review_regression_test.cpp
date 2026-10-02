#include <QtTest>
#include <QtWidgets>
#include <QtNetwork>
#include <QtSerialBus>
#include <optional>
#include <functional>
#include <memory>
#include <atomic>
#include <limits>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#ifdef interface
#undef interface
#endif
#endif
#define private public
#include "communication/CANInterface.h"
#include "communication/MatrikonOpcServer.h"
#include "designer/DownloadDockWidget.h"
#undef private
#include "communication/EthernetInterface.h"
#include "communication/AsyncOpcServer.h"
#include "communication/ClassicOpcServer.h"
#include "communication/ControllerDeviceBackend.h"
#include "common/ProjectMutationGuard.h"
#include "common/LogSafety.h"
#include "common/DeferredThreadCleanup.h"
#include "monitor/MonitorExportHelper.h" // Intentionally before any other monitor header.
#include "monitor/BackendSampler.h"
#include "monitor/ChartWidget.h"
#include "designer/DownloadDockWidget.h"
#include "designer/ui/ThemeManager.h"
#include "core/BackgroundTaskRegistry.h"
#include "core/DataManager.h"
#include "monitor/ReadOnlyHistorySnapshot.h"
#include <QScopeGuard>
#include <QSqlDatabase>

class ReviewCanDevice : public QCanBusDevice {
public:
    explicit ReviewCanDevice(QObject* parent) : QCanBusDevice(parent) { setState(ConnectedState); }
    bool open() override { setState(ConnectedState); return true; }
    void close() override { setState(UnconnectedState); }
    bool writeFrame(const QCanBusFrame&) override { return true; }
    QString interpretErrorFrame(const QCanBusFrame&) override { return {}; }
    void inject() { enqueueReceivedFrames({QCanBusFrame(0x100, QByteArray("x"))}); }
    void disconnectForTest() { setState(UnconnectedState); }
};
class ReviewCanInterface : public CANInterface {
public:
    using CANInterface::sendFrame;
};
struct ReviewOpcState {
    std::atomic<int> calls{0}, wrongThread{0}, destroyed{0}, values{0};
    std::atomic<quintptr> owner{0};
    int delayMs = 150;
    bool fail = false;
};
class ReviewOpcEngine : public IOpcServer {
public:
    explicit ReviewOpcEngine(std::shared_ptr<ReviewOpcState> state) : m_state(std::move(state)) {
        m_state->owner = reinterpret_cast<quintptr>(QThread::currentThreadId()); check();
    }
    ~ReviewOpcEngine() override { check(); ++m_state->destroyed; }
    bool applyConfig(const OpcServerConfig&, QString*) override { check(); return true; }
    bool start(QString* error) override {
        check(); QThread::msleep(m_state->delayMs);
        if (m_state->fail) { if (error) *error = "fake COM failure"; emit errorOccurred("fake COM failure"); return false; }
        m_running = true; emit runningStateChanged(true); return true;
    }
    void stop() override { check(); if (m_running) { m_running = false; emit runningStateChanged(false); } }
    bool isRunning() const override { return m_running; }
    void setRuntimePoints(const QList<RuntimePointDefinition>&) override { check(); }
    void setOpcTags(const QList<OpcTagDefinition>&) override { check(); }
    void updatePointValues(const QList<RuntimePointValue>& values) override { check(); m_state->values += values.size(); }
    void recordWriteResult(const QString&, bool, const QString&) override { check(); }
    BackendStatusSnapshot statusSnapshot() const override {
        BackendStatusSnapshot status; status.online = false; // A running, degraded service is still running.
        status.extras["degraded"] = true; return status;
    }
private:
    void check() { ++m_state->calls; if (m_state->owner != reinterpret_cast<quintptr>(QThread::currentThreadId())) ++m_state->wrongThread; }
    std::shared_ptr<ReviewOpcState> m_state;
    bool m_running = false;
};
class ReviewDebugTransport : public IControllerDebugTransport {
public:
    explicit ReviewDebugTransport(std::shared_ptr<std::atomic<int>> writes) : m_writes(std::move(writes)) {}
    bool isConnected() const override { return true; }
    int stationAddress() const override { return m_station; }
    void setStationAddress(int station) override { m_station = station; }
    bool readHoldingRegisters(int, int count) override { m_count = count; return true; }
    QVector<quint16> holdingRegisterValues(int) const override { return QVector<quint16>(m_count, 1); }
    bool writeMultipleRegisters(int, const QVector<quint16>&) override { ++*m_writes; QThread::msleep(100); return true; }
    CommError lastError() const override { return {}; }
private:
    int m_count = 1, m_station = 1;
    std::shared_ptr<std::atomic<int>> m_writes;
};
class ReviewCommitBarrier : public MonitorExportHelper {
public:
    ReviewCommitBarrier(std::atomic<int>& phase, std::shared_ptr<std::atomic_bool> cancelled)
        : m_phase(phase), m_cancelled(std::move(cancelled)) {}
protected:
    bool commitSaveFile(QSaveFile&) override {
        m_phase = 2;
        while (!m_cancelled->load()) QThread::msleep(1);
        return false;
    }
private:
    std::atomic<int>& m_phase;
    std::shared_ptr<std::atomic_bool> m_cancelled;
};

class FullReviewRegressionTest : public QObject {
    Q_OBJECT
private slots:
    void redactionHandlesJsonEscapesAndTextAliases() {
        const auto input = QJsonObject{{"requestBody", "{\"password\":\"a\\\"secret\",\"normal\":\"visible\"}"}};
        const auto text = QJsonDocument(LogSafety::redact(input).toObject()).toJson();
        QVERIFY(!text.contains("secret")); QVERIFY(text.contains("visible"));
        for (const auto& value : {"PASSWORD=probe-secret", "Authorization: Bearer probe-secret", "https://user:probe-secret@example.com/", "prefix={\"token\":\"probe-secret\"}"})
            QVERIFY(!LogSafety::redactText(value).contains("probe-secret"));
    }
    void csvLayoutsUseUtcAndUnsignedMetadataIsExact() {
        QTemporaryDir dir;
        MonitorExportHelper helper;
        ExportConfig config; config.includeMetadataComments = false;
        ExportMetadata metadata;
        metadata.customFields["unsignedMaximum"] = QVariant::fromValue(std::numeric_limits<qulonglong>::max());
        metadata.customFields["signedMaximum"] = QVariant::fromValue(std::numeric_limits<qlonglong>::max());
        metadata.customFields["signedMinimum"] = QVariant::fromValue(std::numeric_limits<qlonglong>::min());
        metadata.customFields["aboveSignedMaximum"] = QVariant::fromValue(qulonglong(std::numeric_limits<qlonglong>::max()) + 1);
        metadata.customFields["zero"] = 0;
        metadata.customFields["negative"] = -1;
        metadata.customFields["nested"] = QVariantMap{{"number", QVariant::fromValue(std::numeric_limits<qulonglong>::max())}};
        const QList<ExportChannelInfo> channels{{"ch", "channel", "bar", 100}};
        // Both DST sides identify instants; layouts must retain the UTC epoch/time correspondence.
        const auto first = QDateTime::fromString("2026-11-01T01:30:00-04:00", Qt::ISODate);
        const auto second = QDateTime::fromString("2026-11-01T01:30:00-05:00", Qt::ISODate);
        ExportPageProvider provider = [=](const QString&, const ExportCursor& cursor, int) {
            ExportPage page;
            if (!cursor.isValid()) {
                for (const auto& time : {first, second}) { Monitor::Sample sample; sample.channelName = "ch"; sample.timestamp = time; sample.value = 1; page.samples.append(sample); }
                page.nextCursor.timestamp = second; page.nextCursor.id = 2;
            }
            return page;
        };
        for (bool aligned : {false, true}) {
            config.alignMultiChannelByTime = aligned; helper.setConfig(config);
            const auto path = dir.filePath(QString("%1.csv").arg(aligned));
            QVERIFY(helper.exportPackagePaged(channels, metadata, provider, path).success);
            QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const auto bytes = file.readAll();
            QVERIFY(bytes.contains("2026-11-01 05:30:00.000Z"));
            QVERIFY(bytes.contains("2026-11-01 06:30:00.000Z"));
            QVERIFY(bytes.contains(QByteArray::number(first.toMSecsSinceEpoch())));
        }
        const auto path = dir.filePath("metadata.json");
        QVERIFY(helper.exportPackagePaged(channels, metadata, provider, path).success);
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto bytes = file.readAll(); QVERIFY(bytes.contains("\"18446744073709551615\""));
        QVERIFY(!bytes.contains("\"unsignedMaximum\": -1"));
        QVERIFY(bytes.contains("\"9223372036854775807\"")); QVERIFY(bytes.contains("\"-9223372036854775808\""));
        QVERIFY(bytes.contains("\"9223372036854775808\""));
        const auto pagedMetadata = QJsonDocument::fromJson(bytes).object().value("metadata").toObject();
        QCOMPARE(pagedMetadata.value("negative").toInt(), -1); QCOMPARE(pagedMetadata.value("zero").toInt(), 0);
        QCOMPARE(pagedMetadata.value("nested").toObject().value("number").toString(), QString("18446744073709551615"));
        ExportDataPackage package; package.channelInfos = channels; package.metadata = metadata;
        package.channelSamples["ch"] = provider("ch", {}, 10).samples;
        for (bool aligned : {false, true}) {
            config.alignMultiChannelByTime = aligned; helper.setConfig(config);
            for (const auto& extension : {"csv", "tsv"}) {
                const auto memoryPath = dir.filePath(QString("memory-%1.%2").arg(aligned).arg(extension));
                const auto result = QString(extension) == "csv" ? helper.exportPackageAsCsv(package, memoryPath) : helper.exportPackageAsTsv(package, memoryPath);
                QVERIFY(result.success); QFile memory(memoryPath); QVERIFY(memory.open(QIODevice::ReadOnly));
                const auto content = memory.readAll(); QVERIFY(content.contains("2026-11-01 05:30:00.000Z")); QVERIFY(content.contains("2026-11-01 06:30:00.000Z"));
            }
        }
        const auto memoryPath = dir.filePath("memory.json"); QVERIFY(helper.exportPackageAsJson(package, memoryPath).success);
        QFile memory(memoryPath); QVERIFY(memory.open(QIODevice::ReadOnly));
        const auto memoryMetadata = QJsonDocument::fromJson(memory.readAll()).object().value("metadata").toObject();
        QCOMPARE(memoryMetadata.value("signedMaximum").toString(), QString("9223372036854775807"));
        QCOMPARE(memoryMetadata.value("signedMinimum").toString(), QString("-9223372036854775808"));
        QCOMPARE(memoryMetadata.value("unsignedMaximum").toString(), QString("18446744073709551615"));
    }
    void udpFloodHasCountAndByteBounds() {
        QUdpSocket socket; QVERIFY(socket.bind(QHostAddress(QHostAddress::LocalHost), quint16(0)));
        const auto port = socket.localPort(); socket.close();
        EthernetInterface receiver; EthernetConfig config;
        config.protocol = EthernetConfig::Protocol::UDP; config.host = "127.0.0.1"; config.port = port; config.keepAliveInterval = 0;
        QVERIFY(receiver.open(config));
        QUdpSocket sender;
        for (int batch = 0; batch < 30; ++batch) {
            for (int i = 0; i < 100; ++i) QCOMPARE(sender.writeDatagram(i % 2 ? QByteArray("x") : QByteArray(), QHostAddress::LocalHost, port), qint64(i % 2));
            QTest::qWait(2);
        }
        QTRY_VERIFY(receiver.droppedUdpDatagrams() > 0);
        QVERIFY(receiver.queuedUdpDatagrams() <= 1024);
        receiver.close(); QCOMPARE(receiver.queuedUdpDatagrams(), 0);
    }
    void canWaitPumpsOwnerEventsAndUsesDeadline() {
        ReviewCanInterface iface;
        auto* device = new ReviewCanDevice(&iface); iface.m_device = device;
        connect(device, &QCanBusDevice::framesReceived, &iface, &CANInterface::onFramesReceived);
        QTimer::singleShot(20, device, [=] { device->inject(); });
        QElapsedTimer elapsed; elapsed.start(); QCOMPARE(iface.receive(150), QByteArray("x"));
        QVERIFY(elapsed.elapsed() < 150);
        elapsed.restart(); QVERIFY(iface.receive(30).isEmpty()); QVERIFY(elapsed.elapsed() < 150);
        CANMessage invalid; invalid.id = 0x800; invalid.extended = false; invalid.payload = "x";
        QVERIFY(!iface.sendFrame(invalid));
    }
    void canMapsRejectInvalidNumbersBeforeNarrowing() {
        for (const auto& value : {QVariant(257), QVariant(-1), QVariant(1.5), QVariant(true), QVariant::fromValue(0xffffffffffffffffULL)})
            QVERIFY(!CANOpenConfig::fromMap({{"nodeId", value}}).isValid());
        QVERIFY(!CANOpenConfig::fromMap({{"functionCode", 16}}).isValid());
        QVERIFY(!CANOpenConfig::fromMap({{"cobId", 0x800}}).isValid());
        QVERIFY(!J1939Config::fromMap({{"priority", 8}}).isValid());
        QVERIFY(!J1939Config::fromMap({{"sourceAddress", 256}}).isValid());
        QVERIFY(!J1939Config::fromMap({{"pgn", 0x12345}}).isValid());
        QVERIFY(CANOpenConfig::fromMap({{"nodeId", 127}, {"functionCode", 15}, {"cobId", 0x7ff}}).isValid());
        QVERIFY(J1939Config::fromMap({{"pgn", 0xf123}, {"sourceAddress", 255}, {"priority", 7}}).isValid());
    }
    void canCrossThreadWaitHandlesFramesDisconnectAndFalseWakeups_data() {
        QTest::addColumn<int>("action");
        QTest::newRow("frame") << 0;
        QTest::newRow("disconnect") << 1;
        QTest::newRow("close-cancels") << 2;
        QTest::newRow("false-wakeups-deadline") << 3;
    }
    void canCrossThreadWaitHandlesFramesDisconnectAndFalseWakeups() {
        QFETCH(int, action);
        ReviewCanInterface iface;
        auto* device = new ReviewCanDevice(&iface); iface.m_device = device;
        connect(device, &QCanBusDevice::framesReceived, &iface, &CANInterface::onFramesReceived);
        connect(device, &QCanBusDevice::stateChanged, &iface, &CANInterface::onDeviceStateChanged);
        QByteArray result; std::atomic_bool entered{false}, done{false}; qint64 elapsed = 0;
        auto* thread = QThread::create([&] {
            entered = true; QElapsedTimer timer; timer.start();
            result = iface.receive(action == 3 ? 80 : 1000); elapsed = timer.elapsed(); done = true;
        });
        auto cleanup = qScopeGuard([&] { iface.close(); thread->wait(2000); delete thread; });
        QTimer falseWakeups; connect(&falseWakeups, &QTimer::timeout, [&] {
            QMutexLocker lock(&iface.m_receiveMutex); iface.m_frameAvailable.wakeAll();
        });
        thread->start(); QTRY_VERIFY(entered.load()); QTest::qWait(10);
        if (action == 0) device->inject();
        else if (action == 1) device->disconnectForTest();
        else if (action == 2) iface.close();
        else falseWakeups.start(1);
        QTRY_VERIFY_WITH_TIMEOUT(done.load(), 500); QVERIFY(thread->wait(100));
        if (action == 0) QCOMPARE(result, QByteArray("x")); else QVERIFY(result.isEmpty());
        QVERIFY(elapsed < 400); if (action == 3) QVERIFY(elapsed >= 60);
    }
    void reservedNamesLeaveNoFiles() {
#ifdef Q_OS_WIN
        QTemporaryDir dir;
        for (const auto& name : {"NUL", "con.txt", "PRN.data", "AUX", "COM1.lh", "LPT9", "CONIN$", "CONOUT$", "NUL .txt", "bad?.lh", "bad|name"}) {
            ProjectMutationGuard guard; QString error;
            const bool acquired = guard.acquire(dir.path(), dir.filePath(name), false, &error);
            QVERIFY(!acquired || !guard.create(false, "bytes"));
        }
        QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
#else
        QSKIP("Windows native naming contract");
#endif
    }
    void expertLogIsLiteralAndBounded() {
        DownloadDockWidget widget;
        auto* log = widget.findChild<QTextEdit*>(); QVERIFY(log);
        widget.appendLog("<b>markup</b> & text\nsecond");
        QVERIFY(log->toPlainText().contains("<b>markup</b> & text\nsecond"));
        for (int i = 0; i < 5000; ++i) widget.appendLog(QString("line-%1").arg(i));
        QVERIFY(log->document()->blockCount() <= 2000);
        QVERIFY(log->toPlainText().endsWith("line-4999"));
        QVERIFY(!log->toPlainText().contains("markup"));
        QVERIFY(!log->toolTip().isEmpty());
    }
    void nativeOpcCallbackBurstIsBounded() {
#ifdef Q_OS_WIN
        MatrikonOpcServer server; server.activateDataCallbackContext();
        VARIANT value; VariantInit(&value); value.vt = VT_I4; value.lVal = 42;
        unsigned long handle = 1; unsigned short quality = 0xc0; FILETIME timestamp{}; long error = 0;
        for (int i = 0; i < 20000; ++i) server.enqueueDataChange(0, 0, 0, 0, 1, &handle, &value, &quality, &timestamp, &error);
        const auto extras = server.statusSnapshot().extras;
        QVERIFY(extras.value("pendingCallbackCount").toInt() <= 128);
        QCOMPARE(extras.value("droppedCallbackCount").toULongLong(), 19872ULL);
        QTRY_COMPARE(server.m_callbackCount, 128ULL);
        QCOMPARE(server.statusSnapshot().extras.value("pendingCallbackCount").toInt(), 0);
#else
        QSKIP("Windows native OPC callback shape");
#endif
    }
    void opcWriteStateContractIsShared() {
        ClassicOpcServer classic; MatrikonOpcServer native;
        for (auto* server : {static_cast<IOpcServer*>(&classic), static_cast<IOpcServer*>(&native)}) {
            server->recordWriteResult("point", true, "confirmed");
            server->recordWriteResult("point", false, "failed");
            const auto extras = server->statusSnapshot().extras;
            QCOMPARE(extras.value("successfulWriteCount").toInt(), 1);
            QCOMPARE(extras.value("failedWriteCount").toInt(), 1);
            QCOMPARE(extras.value("lastWriteMessage").toString(), QString("failed"));
            QVERIFY(extras.value("lastSuccessfulWriteTime").toDateTime().isValid());
            QVERIFY(extras.value("lastFailedWriteTime").toDateTime().isValid());
        }
    }
    void asyncOpcPreservesApartmentAndDegradedLifecycle() {
        auto state = std::make_shared<ReviewOpcState>(); int heartbeats = 0;
        QTimer timer; connect(&timer, &QTimer::timeout, [&] { ++heartbeats; }); timer.start(5);
        {
            AsyncOpcServer server([=] { return new ReviewOpcEngine(state); });
            OpcServerConfig config; config.opcProgId = "fake"; config.timeoutMs = 1000;
            QVERIFY(server.applyConfig(config));
            QElapsedTimer elapsed; elapsed.start(); QVERIFY(server.start()); QVERIFY(elapsed.elapsed() < 50);
            QVERIFY(server.statusSnapshot().extras.value("startPending").toBool());
            QTRY_VERIFY(server.isRunning()); QVERIFY(!server.statusSnapshot().online);
            QVERIFY(heartbeats >= 5); QCOMPARE(state->wrongThread.load(), 0);
            RuntimePointValue value; value.pointId = "point"; value.value = 1;
            server.updatePointValues({value}); QTRY_COMPARE(state->values.load(), 1);
            server.stop(); QVERIFY(!server.isRunning());
        }
        QCOMPARE(state->destroyed.load(), 1); QCOMPARE(state->wrongThread.load(), 0);
        QVERIFY(state->owner != reinterpret_cast<quintptr>(QThread::currentThreadId()));
    }
    void asyncOpcTimeoutCannotReviveCancelledSession() {
        auto state = std::make_shared<ReviewOpcState>();
        AsyncOpcServer server([=] { return new ReviewOpcEngine(state); });
        OpcServerConfig config; config.opcProgId = "fake"; config.timeoutMs = 20;
        QVERIFY(server.applyConfig(config)); QSignalSpy errors(&server, &IOpcServer::errorOccurred);
        QVERIFY(server.start()); QTRY_VERIFY(!errors.isEmpty());
        QVERIFY(!server.isRunning()); QTest::qWait(200); QVERIFY(!server.isRunning());
        state->delayMs = 20; config.timeoutMs = 1000; QVERIFY(server.applyConfig(config));
        QVERIFY(server.start()); QTRY_VERIFY(server.isRunning());
        RuntimePointValue value; value.pointId = "point"; value.value = 2;
        server.updatePointValues({value}); QTRY_COMPARE(state->values.load(), 1);
    }
    void asyncOpcPendingValuesHaveTotalMemoryBound() {
        auto state = std::make_shared<ReviewOpcState>(); AsyncOpcServer server([=] { return new ReviewOpcEngine(state); });
        QList<RuntimePointValue> values;
        for (int i = 0; i < 100; ++i) { RuntimePointValue value; value.pointId = QString::number(i); value.value = QString(60000, 'x'); values.append(value); }
        server.updatePointValues(values);
        const auto extras = server.statusSnapshot().extras;
        QVERIFY(extras.value("pendingValueBytes").toLongLong() <= 4 * 1024 * 1024);
        QVERIFY(extras.value("droppedValueCount").toULongLong() > 0);
    }
    void asyncOpcFailureAndQueueSaturationKeepCancellationAdmissible() {
        auto state = std::make_shared<ReviewOpcState>(); state->fail = true;
        AsyncOpcServer server([=] { return new ReviewOpcEngine(state); });
        OpcServerConfig config; config.opcProgId = "fake"; config.timeoutMs = 1000;
        QVERIFY(server.applyConfig(config)); QSignalSpy errors(&server, &IOpcServer::errorOccurred);
        QVERIFY(server.start()); QTRY_VERIFY(!server.statusSnapshot().extras.value("startPending").toBool());
        QVERIFY(!server.isRunning()); QVERIFY(!errors.isEmpty()); state->fail = false;
        QVERIFY(server.start());
        for (int i = 0; i < 100; ++i) server.recordWriteResult(QString::number(i), true, "confirmed");
        QVERIFY(server.statusSnapshot().extras.value("pendingRequests").toInt() <= 32);
        QElapsedTimer elapsed; elapsed.start(); server.stop(); QVERIFY(elapsed.elapsed() < 50);
        QTest::qWait(250); QVERIFY(!server.isRunning());
        QCOMPARE(server.statusSnapshot().extras.value("pendingRequests").toInt(), 0);
    }
    void asyncDebugCommandsStayResponsiveAndCompleteOnce() {
        auto writes = std::make_shared<std::atomic<int>>(0);
        ControllerDebugClient client; client.enableWorkerThread([=] { return new ReviewDebugTransport(writes); });
        ControllerDeviceBackend backend; backend.setDebugClientForTest(&client);
        ProjectRuntimeConfig config; config.transport.parameters["port"] = "COM-FAKE-REVIEW";
        QVERIFY(backend.configure(config)); QVERIFY(backend.connectBackend());
        int heartbeats = 0; QTimer timer; connect(&timer, &QTimer::timeout, [&] { ++heartbeats; }); timer.start(5);
        for (const auto& command : {"pause", "resume", "step", "cursor", "breakpoints"}) {
            int completed = 0; bool success = false;
            const QVector<quint16> args = QString(command) == "cursor" ? QVector<quint16>{7}
                : QString(command) == "breakpoints" ? QVector<quint16>{7, 8} : QVector<quint16>{};
            QElapsedTimer elapsed; elapsed.start();
            QVERIFY(backend.requestDebugCommand(command, args, this, [&](bool ok, QString) { ++completed; success = ok; }, nullptr, 1000));
            QVERIFY(elapsed.elapsed() < 50); QTRY_COMPARE(completed, 1); QVERIFY(success);
        }
        QVERIFY(heartbeats >= 20); QCOMPARE(writes->load(), 5);
        int completed = 0; bool success = true;
        QVERIFY(backend.requestDebugCommand("pause", {}, this, [&](bool ok, QString) { ++completed; success = ok; }, nullptr, 20));
        QTRY_COMPARE(completed, 1); QVERIFY(!success); QTest::qWait(150); QCOMPARE(completed, 1);
        completed = 0;
        QVERIFY(backend.requestDebugCommand("pause", {}, this, [&](bool ok, QString) { ++completed; success = ok; }, nullptr, 1000));
        QTest::qWait(10); QElapsedTimer elapsed; elapsed.start(); backend.disconnectBackend(); QVERIFY(elapsed.elapsed() < 50);
        QVERIFY(!backend.connectBackend()); QTRY_COMPARE(completed, 1); QVERIFY(!success);
        QTest::qWait(150);
    }
    void darkThemeHasIndependentReadablePalette() {
        ThemeManager::applyTheme(qApp, ThemeManager::ThemeMode::Light);
        const auto light = qApp->palette().color(QPalette::Base);
        ChartWidget chart; QVERIFY(chart.addChannelSeries("point", "pressure"));
        chart.appendPoint("point", QPointF(QDateTime::currentMSecsSinceEpoch(), 42));
        ThemeManager::applyTheme(qApp, ThemeManager::ThemeMode::Dark);
        QCoreApplication::processEvents();
        const auto dark = qApp->palette().color(QPalette::Base);
        QVERIFY(light != dark); QVERIFY(dark.lightness() < 80);
        QVERIFY(qApp->palette().color(QPalette::Text).lightness() > 180);
        QVERIFY(qApp->palette().color(QPalette::Disabled, QPalette::Text).lightness() > dark.lightness() + 60);
        QGroupBox group; group.setStyleSheet(ThemeManager::groupBoxStyleSheet()); group.ensurePolished();
        QLabel inlineLabel("readable");
        inlineLabel.setStyleSheet("QLabel { color: #24292f; background: #ffffff; }"); inlineLabel.ensurePolished();
        QCoreApplication::processEvents();
        QVERIFY(group.styleSheet().contains("#171c22"));
        QVERIFY(inlineLabel.palette().color(QPalette::WindowText).lightness() > 180);
        QVERIFY(inlineLabel.palette().color(QPalette::Window).lightness() < 80);
        const QString dynamicStyle = "color: #57606a; background: #f6f8fa;";
        inlineLabel.setStyleSheet(dynamicStyle); QCoreApplication::processEvents();
        QVERIFY(inlineLabel.palette().color(QPalette::WindowText).lightness() > 150);
        QVERIFY(chart.chart()->backgroundBrush().color().lightness() < 80);
        QVERIFY(chart.axisY()->labelsBrush().color().lightness() > 180);
        QVERIFY(chart.chart()->legend()->labelColor().lightness() > 180);
        QCOMPARE(chart.channelPointCount("point"), 1);
        ThemeManager::applyTheme(qApp, ThemeManager::ThemeMode::Light);
        QCoreApplication::processEvents();
        QVERIFY(chart.chart()->backgroundBrush().color().lightness() > 180);
        QCOMPARE(chart.channelPointCount("point"), 1);
        QCOMPARE(inlineLabel.styleSheet(), dynamicStyle);
    }
    void exportRegistryCancelsAndJoinsWithoutWindowOwner() {
        Core::BackgroundTaskRegistry registry;
        auto cancelled = std::make_shared<std::atomic_bool>(false);
        std::atomic_bool finished{false};
        auto* task = QThread::create([=, &finished] { while (!cancelled->load()) QThread::msleep(1); finished = true; });
        QVERIFY(registry.start(task, cancelled)); QCOMPARE(registry.activeCount(), 1);
        QVERIFY(registry.cancelAndWait(500)); QVERIFY(finished); QCOMPARE(registry.activeCount(), 0);
        auto* rejected = QThread::create([] {}); QVERIFY(!registry.start(rejected, cancelled)); delete rejected;
        QCoreApplication::processEvents();
    }
    void nativeThreadCleanupRetainsUntilJoin() {
        std::atomic_bool release{false}, entered{false};
        auto* thread = QThread::create([&] {
            entered = true;
            while (!release.load()) QThread::msleep(1);
        });
        QPointer<QThread> alive(thread);
        thread->start();
        auto cleanup = qScopeGuard([&] { release = true; DeferredThreadCleanup::drain(1000); });
        DeferredThreadCleanup::retain(thread);
        QTRY_VERIFY(entered.load());
        QVERIFY(!DeferredThreadCleanup::drain(10));
        QVERIFY(alive);
        release = true;
        QVERIFY(DeferredThreadCleanup::drain(1000));
        QVERIFY(alive.isNull());
        QCoreApplication::processEvents();
    }
    void exportWindowCloseDrainsAllPhases_data() {
        QTest::addColumn<int>("phase");
        QTest::newRow("before-snapshot") << 0;
        QTest::newRow("database-page") << 1;
        QTest::newRow("atomic-file-publication") << 2;
    }
    void exportWindowCloseDrainsAllPhases() {
        QFETCH(int, phase);
        QTemporaryDir dir;
        const auto database = dir.filePath("history.db");
        auto& manager = DataManager::instance(); QVERIFY(manager.initialize(database));
        QVERIFY(manager.logRuntimeData("point", 42)); manager.shutdown();
        const auto destination = dir.filePath("history.csv");
        QFile original(destination); QVERIFY(original.open(QIODevice::WriteOnly)); original.write("keep-original"); original.close();
        Core::BackgroundTaskRegistry registry;
        auto cancelled = std::make_shared<std::atomic_bool>(false);
        std::atomic<int> reached{-1}; ExportResult result; std::atomic_bool sqlClosed{false};
        auto* owner = new QWidget;
        connect(owner, &QObject::destroyed, [cancelled] { cancelled->store(true); });
        auto* task = QThread::create([&] {
            {
                if (phase == 0) { reached = 0; while (!cancelled->load()) QThread::msleep(1); return; }
                Monitor::ReadOnlyHistorySnapshot snapshot(database, cancelled); QString error;
                if (!snapshot.open(&error)) { result.errorMessage = error; return; }
                ExportPageProvider provider = [&](const QString&, const ExportCursor&, int) {
                    const auto page = snapshot.queryLatestHistoryPage("point", 10, 10);
                    ExportPage output; output.success = page.succeeded();
                    for (const auto& record : page.records) output.samples.append(Monitor::Sample("point", record.value, record.unit, record.timestamp));
                    if (phase == 1) {
                        reached = 1; while (!cancelled->load()) QThread::msleep(1);
                        output.success = false; output.errorMessage = "cancelled";
                    }
                    return output;
                };
                ExportChannelInfo channel("point", "point", "", 100); ExportMetadata metadata;
                if (phase == 2) {
                    ReviewCommitBarrier helper(reached, cancelled); helper.setCancellationPredicate([=] { return cancelled->load(); });
                    result = helper.exportPackagePaged({channel}, metadata, provider, destination);
                } else {
                    MonitorExportHelper helper; helper.setCancellationPredicate([=] { return cancelled->load(); });
                    result = helper.exportPackagePaged({channel}, metadata, provider, destination);
                }
            }
            sqlClosed = true;
        });
        auto cleanup = qScopeGuard([&] { delete owner; cancelled->store(true); registry.cancelAndWait(3000); });
        QVERIFY(registry.start(task, cancelled)); QTRY_COMPARE(reached.load(), phase);
        delete owner; owner = nullptr;
        QElapsedTimer elapsed; elapsed.start(); QVERIFY(registry.cancelAndWait(500)); QVERIFY(elapsed.elapsed() < 750);
        QVERIFY(!result.success); if (phase != 0) QVERIFY(sqlClosed);
        QCOMPARE(registry.activeCount(), 0);
        QFile output(destination); QVERIFY(output.open(QIODevice::ReadOnly)); QCOMPARE(output.readAll(), QByteArray("keep-original"));
        for (const auto& connection : QSqlDatabase::connectionNames()) QVERIFY(!connection.startsWith("HistoryExport_"));
    }
};
QTEST_MAIN(FullReviewRegressionTest)
#include "full_review_regression_test.moc"
