#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSqlQuery>
#include <QSqlError>
#include <QTcpSocket>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "common/PathSecurityUtils.h"
#ifdef interface
#undef interface
#endif
#include <QScopeGuard>
#include <QProcess>
#include <QSemaphore>
#include "core/AsyncDatabaseWorker.h"
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

class BlockedStartupWorker : public Core::AsyncDatabaseWorker {
public:
    std::atomic_bool entered{false}, released{false};
protected:
    bool openDatabaseInThread() override {
        entered = true;
        while (!released.load()) QThread::msleep(1);
        return false;
    }
};

class AdmissionBarrierWorker : public Core::AsyncDatabaseWorker {
public:
    QSemaphore passedInitialCheck, resumeAdmission;
protected:
    void beforeBatchAdmissionForTest() override {
        passedInitialCheck.release();
        resumeAdmission.acquire();
    }
};
#include "monitor/AtomicPixmap.h"
#include "common/FileSnapshot.h"
#include "common/DeferredThreadCleanup.h"
#include "core/AsyncDatabaseWorker.h"
#include "core/HistoryQuery.h"
#include "communication/EthernetInterface.h"
#include "designer/ui/CommandPaletteDialog.h"
#include "designer/RuntimeSessionController.h"
#include "designer/MainWindow.h"
#include "designer/OutputPaneController.h"
#include "designer/ProjectController.h"
#include "monitor/AsyncHistoryStoreAdapter.h"
#include "communication/ProtocolUtils.h"

class FailingReadDevice : public QIODevice {
public:
    FailingReadDevice() { open(ReadOnly | Unbuffered); }
    bool isSequential() const override { return true; }
    bool atEnd() const override { return false; }
protected:
    qint64 readData(char* buffer, qint64) override {
        if (m_read) { setErrorString("injected read failure"); return -1; }
        m_read = true; memcpy(buffer, "prefix", 6); return 6;
    }
    qint64 writeData(const char*, qint64) override { return -1; }
private:
    bool m_read = false;
};

class Review20261023Test : public QObject {
    Q_OBJECT
private slots:
    void producerPausedAfterInitialCheckIsRejectedAfterShutdown() {
        QTemporaryDir dir;
        const auto path = dir.filePath("admission-barrier.db");
        auto& manager = DataManager::instance();
        QVERIFY(manager.initialize(path)); manager.shutdown();
        AdmissionBarrierWorker worker; QVERIFY(worker.startWorker(path));
        QSignalSpy committed(&worker, &Core::AsyncDatabaseWorker::batchCommitted);
        QSignalSpy abandoned(&worker, &Core::AsyncDatabaseWorker::batchAbandoned);
        const QVariantMap record{{"varName", "barrier"}, {"value", 1.0},
                                 {"timestamp", QDateTime::currentDateTimeUtc()}};
        quint64 batchId = 999;
        std::thread producer([&] { batchId = worker.enqueueBatch({record}); });
        const auto cleanup = qScopeGuard([&] {
            worker.resumeAdmission.release();
            if (producer.joinable()) producer.join();
        });
        QVERIFY(worker.passedInitialCheck.tryAcquire(1, 1000));
        QVERIFY(worker.stopWorker(0));
        worker.resumeAdmission.release(); producer.join();
        QCOMPARE(batchId, quint64(0));
        QCOMPARE(worker.pendingRecordCount(), 0);
        QCOMPARE(committed.count(), 0); QCOMPARE(abandoned.count(), 0);
    }
    void failedNewProjectReadPreservesThePreviousProject() {
        QTemporaryDir dir;
        const auto oldPath = dir.filePath("old"); QVERIFY(QDir().mkpath(oldPath));
        ProjectRuntimeConfig config; config.projectName = "old";
        config.mainScriptPath = "main.lh"; config.dslScriptPath = "main.lh";
        config.scriptFiles = QStringList{"main.lh"};
        QFile script(oldPath + "/main.lh"); QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("PROGRAM Main\nEND_PROGRAM\n"); script.close();
        QFile configuration(oldPath + "/project_config.json"); QVERIFY(configuration.open(QIODevice::WriteOnly));
        configuration.write(QJsonDocument(config.toJson()).toJson()); configuration.close();
        ProjectController controller;
        QVERIFY(controller.openProjectFromPath(oldPath));
        const auto previous = controller.runtimeConfig().toJson();
        QSignalSpy load(&controller, &ProjectController::scriptLoadRequired);
        QSignalSpy created(&controller, &ProjectController::projectCreated);
        QSignalSpy errors(&controller, &ProjectController::errorOccurred);
        connect(&controller, &ProjectController::projectNameRequired, this,
            [](QString& name, bool& accepted) { name = "new"; accepted = true; });
        connect(&controller, &ProjectController::directorySelectionRequired, this,
            [&](const QString&, const QString&, QString& path, bool& accepted) { path = dir.path(); accepted = true; });
        bool removed = false;
        connect(&controller, &ProjectController::logMessage, this, [&](const QString& message) {
            if (message.contains(QStringLiteral("项目配置已保存")))
                removed = QFile::remove(dir.filePath("new/main.lh"));
        });
        controller.createNewProject();
        QVERIFY(removed); QVERIFY(!errors.isEmpty());
        QCOMPARE(controller.currentProjectPath(), oldPath);
        QCOMPARE(controller.runtimeConfig().toJson(), previous);
        QCOMPARE(load.count(), 0); QCOMPARE(created.count(), 0);
    }
    void deferredSqlWorkerRemainsVisibleToExitBarrier() {
        auto* worker = new BlockedStartupWorker;
        QPointer<BlockedStartupWorker> alive(worker);
        QVERIFY(worker->startWorkerAsync(":memory:"));
        QTRY_VERIFY(worker->entered.load());
        const auto release = qScopeGuard([&] {
            if (alive) alive->released = true;
            DeferredThreadCleanup::drain(1000);
        });
        QVERIFY(!worker->stopWorker(0));
        worker->deleteWhenStopped();
        QVERIFY(!DeferredThreadCleanup::drain(0));
        QVERIFY(alive);
        worker->released = true;
        QTRY_VERIFY(DeferredThreadCleanup::drain(10));
        QTRY_VERIFY(alive.isNull());
    }
    void sharedProtocolAndBoundaryVectors() {
        QFile file(QFINDTESTDATA("protocol_golden_vectors.json")); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto vectors = QJsonDocument::fromJson(file.readAll()).object();
        for (const auto& item : vectors.value("crc16_modbus").toArray()) {
            const auto vector = item.toObject();
            QCOMPARE(int(Communication::modbusCrc16(QByteArray::fromHex(vector.value("hex").toString().toLatin1()))),
                vector.value("crc").toInt());
        }
        for (const auto& item : vectors.value("registers").toArray()) {
            const auto vector = item.toObject();
            for (bool little : {false, true}) {
                QVector<quint16> expected;
                for (const auto& value : vector.value(little ? "little" : "big").toArray()) expected.append(value.toInt());
                QCOMPARE(Communication::bytesToRegisters(QByteArray::fromHex(vector.value("hex").toString().toLatin1()), little), expected);
            }
        }
        int value = 0;
        QVERIFY(Communication::readPositiveInt({{"n", "12"}}, {"n"}, &value)); QCOMPARE(value, 12);
        for (const auto& bad : {QString("0"), QString("-1"), QString("1.5"), QString("2147483648"), QString("abc")})
            QVERIFY(!Communication::readPositiveInt({{"n", bad}}, {"n"}, &value));
        QVERIFY(PathSecurityUtils::pathWithinRoot("D:/review/root", "D:/review/root/child/file"));
        QVERIFY(!PathSecurityUtils::pathWithinRoot("D:/review/root", "D:/review/root-other/file"));
        QVERIFY(!PathSecurityUtils::pathWithinRoot("//server/share/root", "//server/share/other/file"));
        QVERIFY(PathSecurityUtils::pathWithinRoot("//server/share/root", "//server/share/root/sub/file"));
    }
    void latestWindowKeepsTiesAndSnapshotDuringConcurrentMutation() {
        QTemporaryDir dir;
        const auto path = dir.filePath("window.db");
        auto& manager = DataManager::instance(); QVERIFY(manager.initialize(path));
        QList<QVariantMap> records;
        const auto base = QDateTime::fromMSecsSinceEpoch(100000, Qt::UTC);
        for (int i = 0; i < 503; ++i) records.append({{"varName", "point"}, {"value", i},
            {"timestamp", base.addSecs((i * 13 % 41) / 3)}});
        QVERIFY(manager.logRuntimeDataBatch(records).success); manager.shutdown();
        {
            auto reader = QSqlDatabase::addDatabase("QSQLITE", "review-window-reader");
            auto writer = QSqlDatabase::addDatabase("QSQLITE", "review-window-writer");
            reader.setDatabaseName(path); writer.setDatabaseName(path);
            QVERIFY(reader.open()); QVERIFY(writer.open()); QVERIFY(reader.transaction());
            Core::HistoryQuery query(reader);
            RuntimeHistoryCursor cursor; cursor.maxId = query.latestRecordId();
            QList<qint64> expected;
            QSqlQuery reference(reader);
            QVERIFY(reference.exec("SELECT id FROM runtime_data WHERE variable_name='point' ORDER BY timestamp DESC,id DESC LIMIT 173"));
            while (reference.next()) expected.prepend(reference.value(0).toLongLong());
            reference.finish();
            QList<qint64> actual;
            bool more = true, mutated = false;
            while (more) {
                auto page = query.queryLatestHistoryPage("point", 173, 17, cursor, {});
                QVERIFY2(page.succeeded(), qPrintable(page.errorText));
                for (const auto& record : page.records) actual.append(record.id);
                cursor = page.nextCursor;
                QVERIFY(cursor.windowStartTimestamp.isValid()); QVERIFY(cursor.windowStartId > 0);
                more = page.hasMore;
                if (!mutated) {
                    QSqlQuery mutation(writer);
                    QVERIFY(mutation.exec("DELETE FROM runtime_data WHERE id%2=0"));
                    QVERIFY(mutation.exec("INSERT INTO runtime_data(timestamp,variable_name,value,unit) VALUES('2099-01-01T00:00:00.000Z','point',999,'')"));
                    mutated = true;
                }
            }
            QCOMPARE(actual, expected);
            reader.rollback(); reader.close(); writer.close();
        }
        QSqlDatabase::removeDatabase("review-window-reader");
        QSqlDatabase::removeDatabase("review-window-writer");
    }
    void productionOutputUsesControllerAndAtomicSave() {
        MainWindow window;
        auto* output = window.findChild<QTextEdit*>("outputViewer");
        auto* controller = window.findChild<OutputPaneController*>();
        QVERIFY(output); QVERIFY(controller); QCOMPARE(controller->textEdit(), output);
        QVERIFY(QMetaObject::invokeMethod(&window, "onClearOutput", Qt::DirectConnection));
        const QString message = QStringLiteral("<b>纯文本</b> & ✓");
        window.appendOutput(message); QCOMPARE(output->toPlainText(), message);
        QTemporaryDir dir; const auto path = dir.filePath("output.txt");
        QVERIFY(controller->saveToPath(path));
        QFile saved(path); QVERIFY(saved.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(saved.readAll()), message);
        QVERIFY(QMetaObject::invokeMethod(&window, "onClearOutput", Qt::DirectConnection));
        QVERIFY(output->toPlainText().isEmpty());
    }
    void structuredHistoryReportsUnavailableAndWrongThread() {
        Monitor::AsyncHistoryStoreAdapter missing;
        QCOMPARE(missing.getLatestRecordsResult("p", 1).status, RuntimeHistoryPageStatus::NotInitialized);
        QTemporaryDir dir;
        const auto path = dir.filePath("history.db");
        auto& manager = DataManager::instance(); QVERIFY(manager.initialize(path)); manager.shutdown();
        Core::AsyncDatabaseWorker worker; QVERIFY(worker.startWorker(path));
        Monitor::AsyncHistoryStoreAdapter adapter(&worker);
        auto result = adapter.getLatestRecordsResult("p", 1);
        QCOMPARE(result.status, RuntimeHistoryPageStatus::SqlError);
        QCOMPARE(result.errorCode, QString("ASYNC_REQUEST_REQUIRED"));
        QVERIFY(worker.stopWorker());
    }
    void atomicImageFailurePreservesExistingBytes() {
        QTemporaryDir dir;
        const auto path = dir.filePath("image.png");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("original"); file.close();
        QPixmap pixmap(20, 20); pixmap.fill(Qt::red);
        QVERIFY(!AtomicPixmap::save(pixmap, path, "invalid-encoder"));
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("original")); file.close();
#ifdef Q_OS_WIN
        {
            const auto nativePath = QDir::toNativeSeparators(path);
            HANDLE locked = CreateFileW(reinterpret_cast<LPCWSTR>(nativePath.utf16()), GENERIC_READ,
                FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            QVERIFY(locked != INVALID_HANDLE_VALUE);
            const auto unlock = qScopeGuard([&] { CloseHandle(locked); });
            QVERIFY(!AtomicPixmap::save(pixmap, path, "PNG"));
        }
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("original")); file.close();
#endif
        QVERIFY(AtomicPixmap::save(pixmap, path, "PNG"));
        QImage decoded(path); QVERIFY(!decoded.isNull()); QCOMPARE(decoded.size(), QSize(20, 20));
        QVERIFY(AtomicPixmap::save(pixmap, path, "JPG")); QVERIFY(!QImage(path).isNull());
    }
    void partialReadNeverPublishesPrefix() {
        FailingReadDevice device; QByteArray snapshot("previous");
        QVERIFY(!FileSnapshot::readComplete(device, &snapshot)); QCOMPARE(snapshot, QByteArray("previous"));
        QTemporaryDir dir; QFile empty(dir.filePath("empty"));
        QVERIFY(empty.open(QIODevice::WriteOnly)); empty.close(); QVERIFY(empty.open(QIODevice::ReadOnly));
        QVERIFY(FileSnapshot::readComplete(empty, &snapshot)); QVERIFY(snapshot.isEmpty());
    }
    void idleDownloadDoesNotStartAnOperation() {
        RuntimeSessionController session;
        QSignalSpy state(&session, &RuntimeSessionController::downloadStateChanged);
        QSignalSpy completion(&session, &RuntimeSessionController::downloadFinished);
        QVERIFY(!session.requestDownload("missing.code"));
        QCOMPARE(session.downloadState(), DownloadState::Idle);
        QCOMPARE(state.count(), 0); QCOMPARE(completion.count(), 0);
    }
    void rejectsNonWalDatabase() {
        Core::AsyncDatabaseWorker worker;
        QSignalSpy error(&worker, &Core::AsyncDatabaseWorker::workerError);
        QVERIFY(!worker.startWorker(":memory:"));
        QTRY_VERIFY(!error.isEmpty());
        QCOMPARE(error.first().at(0).toString(), QString("PRAGMA journal_mode"));
        QVERIFY(worker.stopWorker());
    }
    void concurrentShutdownAccountsForAcceptedRecords() {
        QTemporaryDir dir;
        const auto path = dir.filePath("concurrent.db");
        auto& manager = DataManager::instance();
        QVERIFY(manager.initialize(path)); manager.shutdown();
        Core::AsyncDatabaseWorker worker; QVERIFY(worker.startWorker(path));
        worker.setMaxQueueRecords(100000);
        std::atomic<int> accepted{0}, committed{0}, abandoned{0};
        connect(&worker, &Core::AsyncDatabaseWorker::batchCommitted, &worker,
            [&](quint64, int count) { committed += count; }, Qt::DirectConnection);
        connect(&worker, &Core::AsyncDatabaseWorker::batchAbandoned, &worker,
            [&](quint64, int count, const QString&) { abandoned += count; }, Qt::DirectConnection);
        const QVariantMap record{{"varName", "race"}, {"value", 1.0}, {"timestamp", QDateTime::currentDateTimeUtc()}};
        std::atomic_bool start{false};
        std::thread producer([&] {
            while (!start.load()) std::this_thread::yield();
            for (int i = 0; i < 10000; ++i) if (worker.enqueueBatch({record})) ++accepted;
        });
        start = true;
        QTest::qWait(10);
        const bool stopped = worker.stopWorker(0);
        producer.join();
        if (!stopped) { QTest::qWait(300); QVERIFY(worker.stopWorker()); }
        QVERIFY(accepted > 0);
        QCOMPARE(accepted.load(), committed.load() + abandoned.load());
        QCOMPARE(worker.pendingRecordCount(), 0);
        QCOMPARE(worker.enqueueBatch({record}), quint64(0));
    }
    void tcpCapacityAndBackpressure() {
        EthernetInterface server;
        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        EthernetConfig config; config.protocol = EthernetConfig::Protocol::TCP;
        config.role = EthernetConfig::Role::Server; config.host = "127.0.0.1";
        config.port = reservation.serverPort(); reservation.close();
        QVERIFY(server.open(config));
        std::vector<std::unique_ptr<QTcpSocket>> clients;
        for (int i = 0; i < EthernetInterface::MAX_ACTIVE_CLIENTS + 2; ++i) {
            auto socket = std::make_unique<QTcpSocket>(); socket->connectToHost(QHostAddress::LocalHost, server.localPort());
            QVERIFY(socket->waitForConnected(1000)); clients.push_back(std::move(socket)); QTest::qWait(5);
        }
        QTRY_COMPARE(server.activeTcpClients(), EthernetInterface::MAX_ACTIVE_CLIENTS);
        QTRY_VERIFY(server.rejectedTcpConnections() >= 2);
        QCOMPARE(server.send(QByteArray(EthernetInterface::MAX_PENDING_SEND_PER_CLIENT + 1, 'x')), -1);
        QCOMPARE(server.backpressuredSends(), quint64(1));
        QCOMPARE(server.send("hello"), 5);
        QTRY_VERIFY(clients.front()->bytesAvailable() >= 5);
        QCOMPARE(clients.front()->readAll(), QByteArray("hello"));
        for (auto& client : clients) client->setReadBufferSize(1);
        bool backpressured = false;
        for (int i = 0; i < 1024; ++i) {
            if (server.send(QByteArray(16 * 1024, 'x')) < 0) { backpressured = true; break; }
        }
        QVERIFY(backpressured); QVERIFY(server.backpressuredSends() >= 2);
        server.close();
    }
    void scannerStopsOnDirectoryBudgetWithoutMatchingFiles() {
        QTemporaryDir root;
        QDir directory(root.path());
        for (int i = 0; i < 4200; ++i) QVERIFY(directory.mkdir(QString::number(i)));
        auto cancel = std::make_shared<QAtomicInt>(0);
        QString reason; QElapsedTimer timer; timer.start();
        QVERIFY(CommandPaletteDialog::scanFiles(root.path(), cancel, 5000, &reason).isEmpty());
        QVERIFY(!reason.isEmpty()); QVERIFY(timer.elapsed() < 3000);
    }
    void scannerRejectsExternalLinks() {
        QTemporaryDir root, external;
        QFile inside(root.filePath("inside.lh")); QVERIFY(inside.open(QIODevice::WriteOnly)); inside.close();
        QFile outside(external.filePath("outside.lh")); QVERIFY(outside.open(QIODevice::WriteOnly)); outside.close();
        const auto shortcut = root.filePath("escape.lnk");
        QVERIFY(QFile::link(outside.fileName(), shortcut));
        QVERIFY(QFileInfo(shortcut).isSymLink());
#ifdef Q_OS_WIN
        QCOMPARE(QProcess::execute("cmd", {"/c", "mklink", "/J", QDir::toNativeSeparators(root.filePath("external-junction")),
            QDir::toNativeSeparators(external.path())}), 0);
#endif
        auto cancel = std::make_shared<QAtomicInt>(0); QString reason;
        const auto files = CommandPaletteDialog::scanFiles(root.path(), cancel, 5000, &reason);
        QCOMPARE(files.size(), 1); QCOMPARE(files.first(), inside.fileName());
        cancel->storeRelaxed(1);
        QVERIFY(CommandPaletteDialog::scanFiles(root.path(), cancel).isEmpty());
    }
};
QTEST_MAIN(Review20261023Test)
#include "review_20261023_test.moc"
