#include <QtTest/QtTest>
#include <QtWidgets>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QSemaphore>
#include <QScopeGuard>
#include <functional>

// Windows COM headers expose an `interface` macro; the project's CAN config
// uses that ordinary identifier as a field name.
#ifdef interface
#undef interface
#endif

#include "designer/DslScriptEditor.h"
#include "designer/ProjectController.h"
#include "runtime/RuntimeSessionService.h"
#include "communication/VirtualDeviceBackend.h"
#include "communication/ControllerDeviceBackend.h"
#include "communication/ControllerDebugClient.h"
#include "core/ProjectCommandCoordinator.h"
// Exercise completion admission without starting a subprocess in the mutation
// cases. The separate F8 case below uses the real compiler and public actions.
#define private public
#include "designer/RuntimeSessionController.h"
#include "designer/BuildController.h"
#include "designer/MainWindow.h"
#undef private

namespace {
bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(bytes) == bytes.size() && file.flush();
}

bool createProject(const QString& root)
{
    ProjectRuntimeConfig config;
    config.projectName = QStringLiteral("Phase2WorkflowSafety");
    config.mainScriptPath = QStringLiteral("main.lh");
    config.dslScriptPath = QStringLiteral("main.lh");
    config.scriptFiles = QStringList{QStringLiteral("main.lh")};
    return writeBytes(QDir(root).filePath(QStringLiteral("main.lh")),
                      "PROGRAM Main\nVAR\n    system_1 : System;\nEND_VAR\n"
                      "system_1(Author := 1, Config := 100, Date := 2601);\nEND_PROGRAM\n")
            && writeBytes(QDir(root).filePath(QStringLiteral("project_config.json")),
                          QJsonDocument(config.toJson()).toJson());
}

class CountingBackend : public VirtualDeviceBackend
{
public:
    int connectCalls = 0;
    int downloadCalls = 0;
    int writeCalls = 0;
    bool connectBackend() override
    {
        ++connectCalls;
        return VirtualDeviceBackend::connectBackend();
    }
    bool downloadArtifact(const QString& path, const QVariantMap& options,
                          QString* message, CommError* error) override
    {
        ++downloadCalls;
        return VirtualDeviceBackend::downloadArtifact(path, options, message, error);
    }
    bool writePoints(const QHash<QString, QVariant>& writes, QString* message,
                     QHash<QString, CommError>* errors) override
    {
        ++writeCalls;
        return VirtualDeviceBackend::writePoints(writes, message, errors);
    }
};

struct ConnectBarrier {
    QSemaphore entered;
    QSemaphore release;
};

class DelayedConnectTransport : public IControllerDebugTransport
{
public:
    explicit DelayedConnectTransport(std::shared_ptr<ConnectBarrier> barrier)
        : m_barrier(std::move(barrier)) {}
    bool isConnected() const override { return true; }
    int stationAddress() const override { return m_station; }
    void setStationAddress(int station) override { m_station = station; }
    bool readHoldingRegisters(int address, int) override
    {
        if (!m_delayed) {
            m_delayed = true;
            m_barrier->entered.release();
            m_barrier->release.acquire(); // Deliberately ignores cancellation.
        }
        return m_registers.contains(address);
    }
    QVector<quint16> holdingRegisterValues(int address) const override
    { return m_registers.value(address); }
    bool writeMultipleRegisters(int, const QVector<quint16>&) override { return true; }
    CommError lastError() const override { return CommError(); }
private:
    std::shared_ptr<ConnectBarrier> m_barrier;
    bool m_delayed = false;
    int m_station = 1;
    QHash<int, QVector<quint16>> m_registers{
        {10, {1}}, {23, {0}}, {26, {2}}, {27, {3}}, {38, {105}}};
};
}

class Phase2WorkflowSafetyTest : public QObject
{
    Q_OBJECT
private slots:
    void changedBuildInputRejectsOldCompletion_data()
    {
        QTest::addColumn<bool>("changeMemoryConfiguration");
        QTest::newRow("same-size-and-mtime-disk-edit") << false;
        QTest::newRow("unsaved-target-configuration-edit") << true;
    }

    void changedBuildInputRejectsOldCompletion()
    {
        QFETCH(bool, changeMemoryConfiguration);
        QTemporaryDir projectDirectory;
        QVERIFY(projectDirectory.isValid());
        QVERIFY(createProject(projectDirectory.path()));
        CountingBackend backend;
        MainWindow window;
        auto* project = window.findChild<ProjectController*>();
        auto* build = window.findChild<BuildController*>();
        auto* session = window.findChild<RuntimeSessionController*>();
        QVERIFY(project && build && session);
        QVERIFY(project->openProjectFromPath(projectDirectory.path()));
        session->setDeviceBackend(&backend);
        project->runtimeConfig().downloadArtifact.filePath = QStringLiteral("old-approved.code");
        window.beginCompileAndRunFlow();
        session->setPendingRunAfterCompile(true);
        window.captureBuildSnapshot();
        QVERIFY(window.isBuildSnapshotCurrent());

        if (changeMemoryConfiguration) {
            project->runtimeConfig().target.nodeId = QStringLiteral("changed-target");
        } else {
            const QString path = projectDirectory.filePath(QStringLiteral("main.lh"));
            const QFileInfo before(path);
            const QDateTime originalTime = before.lastModified();
            const qint64 originalSize = before.size();
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadWrite));
            QCOMPARE(file.write("//"), qint64(2));
            QVERIFY(file.flush());
            QVERIFY(file.setFileTime(originalTime, QFileDevice::FileModificationTime));
            file.close();
            QCOMPARE(QFileInfo(path).size(), originalSize);
            QCOMPARE(QFileInfo(path).lastModified(), originalTime);
        }

        QVERIFY(!window.isBuildSnapshotCurrent());
        build->m_lastCompileResult = CompileResult();
        build->m_lastCompileResult.success = true;
        build->m_lastCompileResult.metadata.insert(QStringLiteral("compileOnly"), true);
        QSignalSpy runtimeStateChanges(session, &RuntimeSessionController::stateChanged);
        QVERIFY(QMetaObject::invokeMethod(&window, "onCompileSucceeded", Qt::DirectConnection,
                                          Q_ARG(BuildType, BuildType::Configuration)));
        QCOMPARE(window.runFlowStage(), MainWindow::RunFlowStage::Cancelled);
        QVERIFY(!window.isRunFlowActive());
        QVERIFY(!session->hasPendingRunAfterCompile());
        QCOMPARE(project->runtimeConfig().downloadArtifact.filePath,
                 QStringLiteral("old-approved.code"));
        QCOMPARE(backend.connectCalls, 0);
        QCOMPARE(backend.downloadCalls, 0);
        QCOMPARE(backend.writeCalls, 0);
        QCOMPARE(runtimeStateChanges.count(), 0);
        session->setDeviceBackend(nullptr);
    }

    void f9UnsupportedDoesNotBuildConnectDownloadOrRun()
    {
        QTemporaryDir projectDirectory;
        QVERIFY(projectDirectory.isValid());
        QVERIFY(createProject(projectDirectory.path()));
        CountingBackend backend;
        MainWindow window;
        auto* project = window.findChild<ProjectController*>();
        auto* build = window.findChild<BuildController*>();
        auto* session = window.findChild<RuntimeSessionController*>();
        QVERIFY(project && build && session);
        QVERIFY(project->openProjectFromPath(projectDirectory.path()));
        session->setDeviceBackend(&backend);
        auto* action = window.findChild<QAction*>(QStringLiteral("actRunProject"));
        QVERIFY(action);
        QVERIFY(!action->isEnabled());
        QVERIFY(!action->toolTip().isEmpty());
        QSignalSpy compileStarts(build, &BuildController::compileStarted);
        QSignalSpy runtimeChanges(session, &RuntimeSessionController::stateChanged);
        int confirmations = 0;
        window.setMessageBoxHook([&](const QString&, const QString&, QMessageBox::StandardButtons,
                                     QMessageBox::StandardButton) {
            ++confirmations;
            return QMessageBox::Yes;
        });
        const auto originalState = session->state();
        action->trigger();
        QVERIFY(QMetaObject::invokeMethod(&window, "onRunProject", Qt::DirectConnection));
        QCoreApplication::processEvents();
        QCOMPARE(compileStarts.count(), 0);
        QCOMPARE(runtimeChanges.count(), 0);
        QCOMPARE(session->state(), originalState);
        QCOMPARE(backend.connectCalls, 0);
        QCOMPARE(backend.downloadCalls, 0);
        QCOMPARE(backend.writeCalls, 0);
        QCOMPARE(confirmations, 0);
        QVERIFY(!build->isBusy());
        session->setDeviceBackend(nullptr);
    }

    void activeFlowRejectsCompetingBuildEntries_data()
    {
        QTest::addColumn<int>("flowStage");
        QTest::addColumn<int>("downloadStage");
        const auto addFlow = [](const char* name, MainWindow::RunFlowStage stage) {
            QTest::newRow(name) << static_cast<int>(stage) << static_cast<int>(DownloadState::Idle);
        };
        addFlow("saving", MainWindow::RunFlowStage::Saving);
        addFlow("compiling", MainWindow::RunFlowStage::Compiling);
        addFlow("connecting", MainWindow::RunFlowStage::Connecting);
        addFlow("confirming", MainWindow::RunFlowStage::AwaitingConfirmation);
        addFlow("downloading", MainWindow::RunFlowStage::Downloading);
        addFlow("requesting-run", MainWindow::RunFlowStage::RequestingRun);
        const auto addDownload = [](const char* name, DownloadState stage) {
            QTest::newRow(name) << static_cast<int>(MainWindow::RunFlowStage::Idle)
                                << static_cast<int>(stage);
        };
        addDownload("standalone-precheck", DownloadState::Precheck);
        addDownload("standalone-download", DownloadState::Downloading);
        addDownload("standalone-retry", DownloadState::Retrying);
        addDownload("standalone-verification", DownloadState::Verifying);
    }

    void activeFlowRejectsCompetingBuildEntries()
    {
        QFETCH(int, flowStage);
        QFETCH(int, downloadStage);
        QTemporaryDir projectDirectory;
        QVERIFY(projectDirectory.isValid());
        QVERIFY(createProject(projectDirectory.path()));
        CountingBackend backend;
        MainWindow window;
        auto* project = window.findChild<ProjectController*>();
        auto* build = window.findChild<BuildController*>();
        auto* session = window.findChild<RuntimeSessionController*>();
        auto* service = window.findChild<RuntimeSessionService*>();
        QVERIFY(project && build && session && service);
        QVERIFY(project->openProjectFromPath(projectDirectory.path()));
        session->setDeviceBackend(&backend);
        const auto expectedStage = static_cast<MainWindow::RunFlowStage>(flowStage);
        window.setRunFlowStage(expectedStage);
        service->setDownloadState(static_cast<DownloadState>(downloadStage));
        // Even a build-completed callback requesting enabled actions may not
        // override the active connection/confirmation/download gate.
        window.setCompileActionsEnabled(true);
        QSignalSpy compileStarts(build, &BuildController::compileStarted);
        const QStringList actionNames{QStringLiteral("actCompile"),
            QStringLiteral("actCompileParameters"), QStringLiteral("actCompileCommunication"),
            QStringLiteral("actCompileAndRun")};
        for (const QString& name : actionNames) {
            auto* action = window.findChild<QAction*>(name);
            QVERIFY(action);
            QVERIFY2(!action->isEnabled(), qPrintable(name));
            action->trigger();
        }
        for (const char* slot : {"onCompileConfiguration", "onCompileParameters",
                                 "onCompileCommunication", "onCompileAndRunProject"}) {
            QVERIFY(QMetaObject::invokeMethod(&window, slot, Qt::DirectConnection));
        }
        QCOMPARE(compileStarts.count(), 0);
        QVERIFY(!build->isBusy());
        QCOMPARE(window.runFlowStage(), expectedStage);
        QCOMPARE(backend.connectCalls, 0);
        QCOMPARE(backend.downloadCalls, 0);
        QCOMPARE(backend.writeCalls, 0);

        auto* editor = qobject_cast<DslScriptEditor*>(window.openAndActivateFile(
            projectDirectory.filePath(QStringLiteral("main.lh"))));
        QVERIFY(editor && editor->editor());
        QVERIFY(editor->editor()->isEnabled());
        editor->editor()->insertPlainText(QStringLiteral("// editing remains available\n"));
        QVERIFY(editor->currentScript().contains(QStringLiteral("editing remains available")));
        QVERIFY(editor->isModified());
        service->setDownloadState(DownloadState::Idle);
        window.setRunFlowStage(MainWindow::RunFlowStage::Cancelled);
        for (const QString& name : actionNames) {
            auto* action = window.findChild<QAction*>(name);
            QVERIFY2(action->isEnabled(), qPrintable(name));
        }
        QCOMPARE(compileStarts.count(), 0);
        session->setDeviceBackend(nullptr);
    }

    void cancelledConnectionKeepsBuildGateClosedUntilWorkerReturns()
    {
        QTemporaryDir projectDirectory;
        QVERIFY(projectDirectory.isValid());
        QVERIFY(createProject(projectDirectory.path()));
        auto barrier = std::make_shared<ConnectBarrier>();
        ControllerDebugClient client;
        client.enableWorkerThread([barrier]() { return new DelayedConnectTransport(barrier); });
        MainWindow window;
        auto* project = window.findChild<ProjectController*>();
        auto* build = window.findChild<BuildController*>();
        auto* session = window.findChild<RuntimeSessionController*>();
        QVERIFY(project && build && session);
        QVERIFY(project->openProjectFromPath(projectDirectory.path()));
        auto& config = project->runtimeConfig();
        config.transport.protocol = QStringLiteral("MODBUS");
        config.transport.mode = QStringLiteral("rtu");
        config.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("workflow-delayed-connect"));
        QVERIFY(session->m_ownedControllerBackend == nullptr);
        auto* backend = new ControllerDeviceBackend(session);
        backend->setDebugClientForTest(&client);
        session->m_ownedControllerBackend = backend;
        window.setRunFlowStage(MainWindow::RunFlowStage::Connecting);
        int completions = 0;
        bool preparationSucceeded = false;
        session->applyRuntimeConfigAsync([&](bool ok) {
            ++completions;
            preparationSucceeded = ok;
        });
        const auto releaseOnFailure = qScopeGuard([barrier]() { barrier->release.release(); });
        QTRY_VERIFY_WITH_TIMEOUT(barrier->entered.available() > 0, 3000);
        // Bound failure recovery even if a buggy stop call blocks the GUI.
        QThread* watchdog = QThread::create([barrier]() {
            QThread::msleep(5000);
            barrier->release.release();
        });
        watchdog->start();
        const auto joinWatchdog = qScopeGuard([watchdog]() {
            watchdog->wait();
            delete watchdog;
        });
        QSignalSpy compileStarts(build, &BuildController::compileStarted);
        QElapsedTimer stopDuration;
        stopDuration.start();
        QVERIFY(QMetaObject::invokeMethod(&window, "onStopProject", Qt::DirectConnection));
        QVERIFY2(stopDuration.elapsed() < 1500, "Stop blocked while the connection worker was held");
        QCOMPARE(completions, 0);
        QVERIFY(!window.canStartCompileAction());
        for (const char* slot : {"onCompileConfiguration", "onCompileAndRunProject"})
            QVERIFY(QMetaObject::invokeMethod(&window, slot, Qt::DirectConnection));
        QCOMPARE(compileStarts.count(), 0);
        QVERIFY(!build->isBusy());
        barrier->release.release();
        QTRY_COMPARE_WITH_TIMEOUT(completions, 1, 4000);
        QVERIFY(!preparationSucceeded);
        QTRY_VERIFY_WITH_TIMEOUT(window.canStartCompileAction(), 4000);
        for (const QString& name : {QStringLiteral("actCompile"), QStringLiteral("actCompileAndRun")}) {
            auto* action = window.findChild<QAction*>(name);
            QVERIFY(action);
            QTRY_VERIFY_WITH_TIMEOUT(action->isEnabled(), 4000);
        }
        QVERIFY(!backend->isOnline());
        QCOMPARE(compileStarts.count(), 0);
    }

    void compileOnlyF8ReleasesBusyGateForNextRealBuild()
    {
        QTemporaryDir projectDirectory;
        QVERIFY(projectDirectory.isValid());
        QVERIFY(createProject(projectDirectory.path()));
        CountingBackend backend;
        MainWindow window;
        window.show();
        auto* project = window.findChild<ProjectController*>();
        auto* build = window.findChild<BuildController*>();
        auto* session = window.findChild<RuntimeSessionController*>();
        QVERIFY(project && build && session);
        QVERIFY(project->openProjectFromPath(projectDirectory.path()));
        session->setDeviceBackend(&backend);
        auto* compileAndRun = window.findChild<QAction*>(QStringLiteral("actCompileAndRun"));
        auto* compile = window.findChild<QAction*>(QStringLiteral("actCompile"));
        QVERIFY(compileAndRun && compile);
        QSignalSpy successes(build, &BuildController::compileSucceeded);
        QSignalSpy failures(build, &BuildController::compileFailed);
        compileAndRun->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(!build->isBusy(), 60000);
        QCOMPARE(failures.count(), 0);
        QCOMPARE(successes.count(), 1);
        QVERIFY(build->lastCompileResult().success);
        QVERIFY(build->lastCompileResult().metadata.value(QStringLiteral("compileOnly")).toBool());
        QCOMPARE(window.runFlowStage(), MainWindow::RunFlowStage::Failed);
        QVERIFY(!window.isRunFlowActive());
        QVERIFY(!session->hasPendingRunAfterCompile());
        QCOMPARE(backend.connectCalls, 0);
        QCOMPARE(backend.downloadCalls, 0);
        QVERIFY(compile->isEnabled());
        compile->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(successes.count() + failures.count() >= 2, 60000);
        QCOMPARE(successes.count(), 2);
        QCOMPARE(failures.count(), 0);
        QVERIFY(!build->isBusy());
        QCOMPARE(backend.connectCalls, 0);
        QCOMPARE(backend.downloadCalls, 0);
        session->setDeviceBackend(nullptr);
    }
};

int main(int argc, char* argv[])
{
    if (qEnvironmentVariable("QT_QPA_PLATFORM").isEmpty())
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);
    QTemporaryDir settings;
    if (!settings.isValid()) return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    Phase2WorkflowSafetyTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "phase2_workflow_safety_test.moc"
