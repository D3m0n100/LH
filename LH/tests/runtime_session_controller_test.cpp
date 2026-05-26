/**
 * @file runtime_session_controller_test.cpp
 * @brief RuntimeSessionController 单元测试
 */

#include <QtTest/QtTest>

#define private public
#include "designer/RuntimeSessionController.h"
#undef private
#include "communication/VirtualDeviceBackend.h"

class RuntimeSessionControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        qRegisterMetaType<RuntimeSessionState>("RuntimeSessionState");
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
        ctrl.m_artifactPath = QStringLiteral("dummy.code");

        QSignalSpy spy(&ctrl, &RuntimeSessionController::downloadFinished);
        ctrl.executeRun();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
    }

    void executeRunSkipsAutoDownloadWhenBackendOffline()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        ctrl.setDeviceBackend(&backend);
        ctrl.m_artifactPath = QStringLiteral("dummy.code");

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

        QSignalSpy spy(&ctrl, &RuntimeSessionController::runtimeError);
        const bool ok = ctrl.requestDownload("test.code");

        QVERIFY(!ok);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Fault);
    }

    void downloadOfflineBackendFails()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::runtimeError);
        const bool ok = ctrl.requestDownload("test.code");

        QVERIFY(!ok);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Fault);
    }

    void downloadSuccessRestoresState()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::downloadFinished);
        const bool ok = ctrl.requestDownload("test.code");

        QVERIFY(ok);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Running);
    }

    void downloadFaultInjectionGoesToFault()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;
        backend.connectBackend();
        backend.setFaultInjection(false, false, true);
        ctrl.setDeviceBackend(&backend);
        ctrl.executeRun();

        QSignalSpy spy(&ctrl, &RuntimeSessionController::downloadFinished);
        const bool ok = ctrl.requestDownload("test.code");

        QVERIFY(!ok);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), false);
        QCOMPARE(ctrl.state(), RuntimeSessionState::Fault);
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

    void setAndClearDeviceBackend()
    {
        RuntimeSessionController ctrl;
        VirtualDeviceBackend backend;

        ctrl.setDeviceBackend(&backend);
        QCOMPARE(ctrl.deviceBackend(), &backend);

        ctrl.setDeviceBackend(nullptr);
        QCOMPARE(ctrl.deviceBackend(), nullptr);
    }
};

QTEST_MAIN(RuntimeSessionControllerTest)
#include "runtime_session_controller_test.moc"
