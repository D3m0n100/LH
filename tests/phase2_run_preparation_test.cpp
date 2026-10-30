#include <QtTest/QtTest>
#include <QtWidgets>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QScopeGuard>
#include <atomic>
#include <memory>

// The Windows COM headers included by QtWidgets define this macro; LH has a
// legitimate CAN config field named `interface`.
#ifdef interface
#undef interface
#endif

#define private public
#include "designer/RuntimeSessionController.h"
#include "designer/BuildController.h"
#include "designer/MainWindow.h"
#undef private
#include "communication/ControllerDeviceBackend.h"
#include "communication/IOpcServer.h"
#include "designer/ProjectController.h"
#include "designer/DslScriptEditor.h"
#include "runtime/RunController.h"

namespace {
QByteArray sha256(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(bytes) == bytes.size();
}

QString fileHash(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QString::fromLatin1(sha256(file.readAll()));
}

bool writeBundle(const QString& root)
{
    const QByteArray source = QByteArrayLiteral("PROGRAM Main\nEND_PROGRAM\n");
    const QByteArray code = QByteArrayLiteral("payload");
    const QByteArray profile = QByteArrayLiteral(
            "{\"name\":\"phase2\",\"slaveId\":1,\"steps\":["
            "{\"type\":\"sendChunk\",\"params\":{\"dataAddress\":210,\"chunkWords\":1}}]}");
    const QByteArray points = QByteArrayLiteral("[]");
    const QString generation = QStringLiteral("build_output/configuration/generations/prep-1");
    const QString base = QDir(root).filePath(generation);
    if (!writeFile(QDir(root).filePath(QStringLiteral("project_config.json")), QByteArrayLiteral("{}"))
            || !writeFile(QDir(root).filePath(QStringLiteral("main.lh")), source)
            || !writeFile(QDir(base).filePath(QStringLiteral("main.code")), code)
            || !writeFile(QDir(base).filePath(QStringLiteral("download_profile.json")), profile)
            || !writeFile(QDir(base).filePath(QStringLiteral("runtime_points.json")), points)) return false;

    QJsonObject checksums;
    checksums.insert(QStringLiteral("main.code"), QString::fromLatin1(sha256(code)));
    checksums.insert(QStringLiteral("download_profile.json"), QString::fromLatin1(sha256(profile)));
    checksums.insert(QStringLiteral("runtime_points.json"), QString::fromLatin1(sha256(points)));
    QJsonObject sourceChecksums;
    sourceChecksums.insert(QStringLiteral("main.lh"), QString::fromLatin1(sha256(source)));
    QJsonObject manifest;
    manifest.insert(QStringLiteral("generationId"), QStringLiteral("prep-1"));
    manifest.insert(QStringLiteral("complete"), true);
    manifest.insert(QStringLiteral("projectName"), QStringLiteral("Phase2Prep"));
    manifest.insert(QStringLiteral("mainScriptPath"), QStringLiteral("main.lh"));
    manifest.insert(QStringLiteral("dslScriptPath"), QStringLiteral("main.lh"));
    manifest.insert(QStringLiteral("scriptFiles"), QJsonArray{QStringLiteral("main.lh")});
    manifest.insert(QStringLiteral("sourceChecksums"), sourceChecksums);
    manifest.insert(QStringLiteral("sourcePaths"), QJsonObject{
                        {QStringLiteral("mainScriptPath"), QStringLiteral("main.lh")},
                        {QStringLiteral("dslScriptPath"), QStringLiteral("main.lh")}});
    manifest.insert(QStringLiteral("sourceFileCount"), 1);
    manifest.insert(QStringLiteral("scriptFileCount"), 1);
    manifest.insert(QStringLiteral("artifactPaths"), QJsonArray{
                        QStringLiteral("main.code"), QStringLiteral("download_profile.json"),
                        QStringLiteral("runtime_points.json")});
    manifest.insert(QStringLiteral("artifactChecksums"), checksums);
    manifest.insert(QStringLiteral("codePath"), QStringLiteral("main.code"));
    manifest.insert(QStringLiteral("codeChecksum"), QString::fromLatin1(sha256(code)));
    manifest.insert(QStringLiteral("downloadProfilePath"), QStringLiteral("download_profile.json"));
    manifest.insert(QStringLiteral("downloadProfileChecksum"), QString::fromLatin1(sha256(profile)));
    manifest.insert(QStringLiteral("runtimePointsPath"), QStringLiteral("runtime_points.json"));
    manifest.insert(QStringLiteral("runtimePointsChecksum"), QString::fromLatin1(sha256(points)));
    manifest.insert(QStringLiteral("runtimeManifestPath"), QStringLiteral("runtime_manifest.json"));
    manifest.insert(QStringLiteral("pointCount"), 0);
    return writeFile(QDir(base).filePath(QStringLiteral("runtime_manifest.json")),
                     QJsonDocument(manifest).toJson(QJsonDocument::Compact));
}

bool refreshBundleSourceIdentity(ProjectController& project, const QString& root)
{
    const QString generationDir = QDir(root).filePath(
            QStringLiteral("build_output/configuration/generations/prep-1"));
    const QString manifestPath = QDir(generationDir).filePath(QStringLiteral("runtime_manifest.json"));
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) return false;
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    manifestFile.close();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return false;
    QFile sourceFile(QDir(root).filePath(QStringLiteral("main.lh")));
    if (!sourceFile.open(QIODevice::ReadOnly)) return false;
    const QByteArray savedSourceBytes = sourceFile.readAll();
    sourceFile.close();
    QJsonObject manifest = document.object();
    manifest.insert(QStringLiteral("sourceChecksums"), QJsonObject{
                        {QStringLiteral("main.lh"), QString::fromLatin1(sha256(savedSourceBytes))}});
    if (!writeFile(manifestPath, QJsonDocument(manifest).toJson(QJsonDocument::Compact))) return false;

    const QString base = QDir(generationDir).absolutePath();
    auto& config = project.runtimeConfig();
    config.downloadArtifact.metadata.insert(QStringLiteral("runtimeManifestChecksum"), fileHash(manifestPath));
    config.downloadArtifact.metadata.insert(QStringLiteral("downloadProfileChecksum"),
            fileHash(QDir(base).filePath(QStringLiteral("download_profile.json"))));
    config.downloadArtifact.metadata.insert(QStringLiteral("runtimePointsChecksum"),
            fileHash(QDir(base).filePath(QStringLiteral("runtime_points.json"))));
    return project.saveProject();
}

struct ConnectionBarrier { QSemaphore entered; QSemaphore release; };

class PreparationTransport : public IControllerDebugTransport
{
public:
    explicit PreparationTransport(std::shared_ptr<ConnectionBarrier> barrier = {}, bool available = true,
                                  std::shared_ptr<std::atomic_int> writes = {})
        : m_barrier(std::move(barrier)), m_available(available), m_writes(std::move(writes)) {}
    // Report an established fake transport so ControllerDebugClient never
    // falls back to opening a native serial port; failures occur in the fake I/O.
    bool isConnected() const override { return true; }
    int stationAddress() const override { return m_station; }
    void setStationAddress(int value) override { m_station = value; }
    bool readHoldingRegisters(int address, int) override
    {
        if (m_barrier && !m_waited) {
            m_waited = true;
            m_barrier->entered.release();
            m_barrier->release.acquire();
        }
        return m_available && m_registers.contains(address);
    }
    QVector<quint16> holdingRegisterValues(int address) const override
    { return m_registers.value(address); }
    bool writeMultipleRegisters(int, const QVector<quint16>&) override
    { if (m_writes) ++*m_writes; return m_available; }
    CommError lastError() const override { return {}; }
private:
    std::shared_ptr<ConnectionBarrier> m_barrier;
    bool m_available = true;
    bool m_waited = false;
    std::shared_ptr<std::atomic_int> m_writes;
    int m_station = 1;
    QHash<int, QVector<quint16>> m_registers{{10,{1}}, {23,{0}}, {26,{2}}, {27,{3}}, {38,{105}}};
};

class UnexpectedModalGuard final : public QObject
{
public:
    QStringList messages;
    int count = 0;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Show) {
            if (auto* box = qobject_cast<QMessageBox*>(watched)) {
                ++count;
                const QString message = QStringLiteral("%1 | %2").arg(box->windowTitle(), box->text());
                messages.append(message);
                qInfo().noquote() << "unexpected modal:" << message;
                QTimer::singleShot(0, box, &QMessageBox::reject);
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

void configureProject(ProjectController& project, const QString& root)
{
    project.setCurrentProjectPath(root);
    auto& config = project.runtimeConfig();
    config.projectName = QStringLiteral("Phase2Prep");
    config.mainScriptPath = QStringLiteral("main.lh");
    config.dslScriptPath = QStringLiteral("main.lh");
    config.transport.protocol = QStringLiteral("modbus");
    config.transport.mode = QStringLiteral("rtu");
    config.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("phase2-preparation"));
    config.downloadArtifact.filePath =
            QStringLiteral("build_output/configuration/generations/prep-1/main.code");
    config.downloadArtifact.checksum = fileHash(QDir(root).filePath(config.downloadArtifact.filePath));
    config.downloadArtifact.metadata.insert(QStringLiteral("generationId"), QStringLiteral("prep-1"));
    config.downloadArtifact.metadata.insert(QStringLiteral("runtimeManifestPath"),
            QStringLiteral("build_output/configuration/generations/prep-1/runtime_manifest.json"));
    config.downloadArtifact.metadata.insert(QStringLiteral("downloadProfilePath"),
            QStringLiteral("build_output/configuration/generations/prep-1/download_profile.json"));
    const QString base = QFileInfo(QDir(root).filePath(config.downloadArtifact.filePath)).absolutePath();
    config.downloadArtifact.metadata.insert(QStringLiteral("downloadProfileChecksum"),
            fileHash(QDir(base).filePath(QStringLiteral("download_profile.json"))));
    config.downloadArtifact.metadata.insert(QStringLiteral("runtimePointsChecksum"),
            fileHash(QDir(base).filePath(QStringLiteral("runtime_points.json"))));
    config.downloadArtifact.metadata.insert(QStringLiteral("runtimeManifestChecksum"),
            fileHash(QDir(base).filePath(QStringLiteral("runtime_manifest.json"))));
}
}

class Phase2RunPreparationTest : public QObject
{
    Q_OBJECT
private slots:
    void validPublishedRunPreparationCarriesParentOperationId()
    {
        QTemporaryDir projectRoot;
        QVERIFY(projectRoot.isValid());
        QVERIFY(writeBundle(projectRoot.path()));
        ProjectController project;
        configureProject(project, projectRoot.path());
        auto writeCalls = std::make_shared<std::atomic_int>(0);
        ControllerDebugClient client;
        client.enableWorkerThread([writeCalls] { return new PreparationTransport({}, true, writeCalls); });
        RuntimeSessionController controller;
        auto* backend = new ControllerDeviceBackend(&controller);
        backend->setDebugClientForTest(&client);
        controller.m_ownedControllerBackend = backend;
        controller.setProjectController(&project);
        controller.setPendingRunAfterCompile(true);
        CompileResult result; result.success = true;
        int confirmationCount = 0;
        QString confirmedOperationId;
        controller.setDownloadConfirmationHook([&](const DownloadConfirmationSnapshot& snapshot) {
            ++confirmationCount;
            confirmedOperationId = snapshot.operationId;
            return false; // Reach the real confirmation boundary without issuing a device write.
        });
        int completionCount = 0;
        bool prepared = false;
        const QString operationId = QStringLiteral("op-f8-preparation-positive");
        QVERIFY(controller.onCompileSucceeded(result, operationId, [] { return true; },
                    [&](bool ok, const QString&) { ++completionCount; prepared = ok; }));
        QTRY_COMPARE_WITH_TIMEOUT(completionCount, 1, 6000);
        QVERIFY(prepared);
        QCOMPARE(confirmationCount, 1);
        QCOMPARE(confirmedOperationId, operationId);
        QCOMPARE(writeCalls->load(), 0);
        QVERIFY(!controller.m_backendDownloadInProgress);
        QVERIFY(controller.deviceBackend()->isOnline());
        controller.requestStop();
    }

    void mainWindowPublishedBundleContinuesThroughConfirmation()
    {
        QTemporaryDir projectRoot, settings;
        QVERIFY(projectRoot.isValid() && settings.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        QVERIFY(writeBundle(projectRoot.path()));
        UnexpectedModalGuard modalGuard;
        qApp->installEventFilter(&modalGuard);
        ControllerDebugClient client;
        client.enableWorkerThread([] { return new PreparationTransport; });
        qInfo() << "prep marker: before MainWindow";
        MainWindow window;
        qInfo() << "prep marker: after MainWindow";
        auto* project = window.m_projectController;
        auto* build = window.m_buildController;
        auto* session = window.m_sessionController;
        QVERIFY(project && build && session);
        qInfo() << "prep marker: before openProjectFromPath";
        QVERIFY(project->openProjectFromPath(projectRoot.path()));
        qInfo() << "prep marker: after openProjectFromPath";
        configureProject(*project, projectRoot.path());
        qInfo() << "prep marker: before initial saveProject";
        QVERIFY(project->saveProject());
        qInfo() << "prep marker: after initial saveProject";
        qInfo() << "prep marker: before refreshBundleSourceIdentity";
        QVERIFY2(refreshBundleSourceIdentity(*project, projectRoot.path()), "refresh fixture bundle after project save");
        qInfo() << "prep marker: after refreshBundleSourceIdentity";
        const QString configuredArtifact = QDir(projectRoot.path()).filePath(
                project->runtimeConfig().downloadArtifact.filePath);
        const auto precheck = RunController::validateDownloadArtifact(
                project->runtimeConfig(), projectRoot.path(), configuredArtifact, true);
        QVERIFY2(precheck.valid, qPrintable(precheck.errors.join(QLatin1Char('\n'))));
        qInfo() << "prep marker: bundle precheck passed";

        auto* backend = new ControllerDeviceBackend(session);
        backend->setDebugClientForTest(&client);
        session->m_ownedControllerBackend = backend;
        session->setProjectController(project);

        const QString codePath = QDir(projectRoot.path()).filePath(
                QStringLiteral("build_output/configuration/generations/prep-1/main.code"));
        CompileArtifact artifact;
        artifact.type = QStringLiteral("download");
        artifact.format = QStringLiteral("dsl_custom");
        artifact.path = codePath;
        artifact.metadata.insert(QStringLiteral("generationId"), QStringLiteral("prep-1"));
        CompileResult result;
        result.success = true;
        result.artifacts.append(artifact);
        build->m_lastCompileResult = result;

        int confirmations = 0;
        QString confirmedOperationId;
        session->setDownloadConfirmationHook([&](const DownloadConfirmationSnapshot& snapshot) {
            ++confirmations;
            confirmedOperationId = snapshot.operationId;
            return false;
        });
        window.beginCompileAndRunFlow();
        window.captureBuildSnapshot();
        window.setRunFlowStage(MainWindow::RunFlowStage::Compiling);
        const QString operationId = window.m_runFlowOperationId;
        session->setPendingRunAfterCompile(true);
        QVERIFY(window.isBuildSnapshotCurrent());

        window.onCompileSucceeded(BuildType::Configuration);
        QTest::qWait(250);
        qInfo().noquote() << "prep diagnostic stage=" << static_cast<int>(window.runFlowStage())
                          << "session=" << static_cast<int>(session->state())
                          << "save=" << window.m_lastBuildSaveSucceeded
                          << "output=" << (window.m_outputViewer ? window.m_outputViewer->toPlainText() : QStringLiteral("<none>"));
        QTRY_COMPARE_WITH_TIMEOUT(confirmations, 1, 8000);
        QCOMPARE(confirmedOperationId, operationId);
        qApp->removeEventFilter(&modalGuard);
        QCOMPARE(modalGuard.count, 0);
        QVERIFY(!session->m_backendDownloadInProgress);
        QVERIFY(!session->m_opcServer->isRunning());
        QVERIFY(session->state() != RuntimeSessionState::Running);
        QVERIFY(session->state() != RuntimeSessionState::Monitoring);
        QVERIFY(!session->m_ownedControllerBackend->hasAsyncOperationInFlight());
        session->requestStop();
    }

    void buildInputConfigSaveFailureKeepsSavedScriptAndStopsBuild()
    {
        QTemporaryDir projectRoot, settings;
        QVERIFY(projectRoot.isValid() && settings.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        QVERIFY(writeBundle(projectRoot.path()));
        UnexpectedModalGuard modalGuard;
        qApp->installEventFilter(&modalGuard);
        MainWindow window;
        QVERIFY(window.m_projectController->openProjectFromPath(projectRoot.path()));
        configureProject(*window.m_projectController, projectRoot.path());
        QVERIFY(window.m_projectController->saveProject());
        QVERIFY(window.m_dslEditor);

        const QString configPath = QDir(projectRoot.path()).filePath(QStringLiteral("project_config.json"));
        QVERIFY(QFile::remove(configPath));
        QVERIFY(QDir().mkpath(configPath));
        const QString changedScript = QStringLiteral("PROGRAM SavedForBuild\nEND_PROGRAM\n");
        window.m_dslEditor->setScript(changedScript);
        window.m_dslEditor->setModified(true);
        QSignalSpy compileStarted(window.m_buildController, &BuildController::compileStarted);
        window.onCompileConfiguration();
        QVERIFY(!window.m_lastBuildSaveSucceeded);
        QVERIFY2(window.m_outputViewer->toPlainText().contains(QDir::toNativeSeparators(configPath)),
                 qPrintable(window.m_outputViewer->toPlainText()));
        QFile savedScript(QDir(projectRoot.path()).filePath(QStringLiteral("main.lh")));
        QVERIFY(savedScript.open(QIODevice::ReadOnly | QIODevice::Text));
        QVERIFY(QString::fromUtf8(savedScript.readAll()).contains(QStringLiteral("SavedForBuild")));
        QVERIFY(!window.m_dslEditor->isModified());
        QCOMPARE(compileStarted.count(), 0);
        QCOMPARE(modalGuard.count, 0);
        qApp->removeEventFilter(&modalGuard);
    }

    void invalidatedContinuationCannotReachDownload_data()
    {
        QTest::addColumn<QString>("invalidation");
        QTest::newRow("input-edit") << QStringLiteral("input-edit");
        QTest::newRow("configuration-change") << QStringLiteral("configuration-change");
        QTest::newRow("project-switch") << QStringLiteral("project-switch");
        QTest::newRow("user-stop") << QStringLiteral("user-stop");
    }

    void invalidatedContinuationCannotReachDownload()
    {
        QFETCH(QString, invalidation);
        QTemporaryDir root, replacementRoot;
        QVERIFY(root.isValid() && replacementRoot.isValid());
        QVERIFY(writeBundle(root.path()));
        QVERIFY(writeBundle(replacementRoot.path()));
        ProjectController project, replacement;
        configureProject(project, root.path());
        configureProject(replacement, replacementRoot.path());
        auto barrier = std::make_shared<ConnectionBarrier>();
        auto writeCalls = std::make_shared<std::atomic_int>(0);
        ControllerDebugClient client;
        client.enableWorkerThread([barrier, writeCalls] {
            return new PreparationTransport(barrier, true, writeCalls);
        });
        RuntimeSessionController controller;
        const auto releaseOnFailure = qScopeGuard([barrier] { barrier->release.release(); });
        auto* backend = new ControllerDeviceBackend(&controller);
        backend->setDebugClientForTest(&client);
        controller.m_ownedControllerBackend = backend;
        controller.setProjectController(&project);
        controller.setPendingRunAfterCompile(true);
        CompileResult result; result.success = true;
        int confirmationCount = 0;
        controller.setDownloadConfirmationHook([&](const DownloadConfirmationSnapshot&) {
            ++confirmationCount; return false;
        });
        int completionCount = 0;
        bool prepared = true;
        bool snapshotValid = true;
        const QString operationId = QStringLiteral("op-f8-preparation-stale");
        QVERIFY(controller.onCompileSucceeded(result, operationId, [&]() {
                    return snapshotValid;
                }, [&](bool ok, const QString&) { ++completionCount; prepared = ok; }));
        QVERIFY(barrier->entered.tryAcquire(1, 3000));
        if (invalidation == QStringLiteral("input-edit")) {
            snapshotValid = false;
        } else if (invalidation == QStringLiteral("configuration-change")) {
            project.runtimeConfig().target.nodeId = QStringLiteral("changed-during-connection");
        } else if (invalidation == QStringLiteral("project-switch")) {
            controller.setProjectController(&replacement);
        } else {
            controller.requestStop();
        }
        barrier->release.release();
        QTRY_COMPARE_WITH_TIMEOUT(completionCount, 1, 5000);
        QVERIFY(!prepared);
        QCOMPARE(confirmationCount, 0);
        QVERIFY(!controller.m_backendDownloadInProgress);
        QVERIFY(controller.state() != RuntimeSessionState::Connecting);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.hasWorkerOperationInFlight(), 3000);
        QCOMPARE(writeCalls->load(), 0);
        QVERIFY(!controller.m_opcServer->isRunning());
        QVERIFY(controller.state() != RuntimeSessionState::Monitoring);
        if (invalidation == QStringLiteral("input-edit"))
            QCOMPARE(controller.state(), RuntimeSessionState::Connected);
        else
            QVERIFY(!backend->isOnline());
    }

    void failedConnectionNotifiesPreparationCompletion()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(writeBundle(root.path()));
        ProjectController project;
        configureProject(project, root.path());
        ControllerDebugClient client;
        client.enableWorkerThread([] { return new PreparationTransport({}, false); });
        RuntimeSessionController controller;
        auto* backend = new ControllerDeviceBackend(&controller);
        backend->setDebugClientForTest(&client);
        controller.m_ownedControllerBackend = backend;
        controller.setProjectController(&project);
        controller.setPendingRunAfterCompile(true);
        CompileResult result; result.success = true;
        int completionCount = 0;
        bool prepared = true;
        int confirmationCount = 0;
        controller.setDownloadConfirmationHook([&](const DownloadConfirmationSnapshot&) {
            ++confirmationCount; return false;
        });
        QVERIFY(controller.onCompileSucceeded(result, QStringLiteral("op-f8-connect-failure"),
                    [] { return true; }, [&](bool ok, const QString&) {
            ++completionCount; prepared = ok;
        }));
        QTRY_COMPARE_WITH_TIMEOUT(completionCount, 1, 5000);
        QVERIFY(!prepared);
        QCOMPARE(confirmationCount, 0);
        QVERIFY(controller.state() != RuntimeSessionState::Connecting);
        QVERIFY(!controller.hasWorkerOperationInFlight());
    }
};

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    Phase2RunPreparationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "phase2_run_preparation_test.moc"
