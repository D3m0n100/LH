/**
 * @file download_manager_lifecycle_test.cpp
 * @brief DownloadManager worker 生命周期回归测试
 */

#include <QtTest/QtTest>

#include <QObject>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QVariantMap>

#define private public
#include "communication/DownloadManager.h"
#undef private
#define private public
#include "communication/ControllerBridge.h"
#undef private
#include "communication/Communication.h"
#include "communication/ModbusInterface.h"

class DownloadManagerLifecycleTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<DownloadManager::ErrorCode>();
    }

    void invalidInputCleansWorkerAndAllowsRestart()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        DownloadManager manager;
        QSignalSpy errorSpy(&manager, &DownloadManager::errorOccurred);

        const QString profilePath = tempDir.filePath(QStringLiteral("missing-profile.json"));
        const QString payloadPath = tempDir.filePath(QStringLiteral("missing-payload.bin"));

        manager.startDownload(profilePath, payloadPath);
        QTRY_COMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).value<DownloadManager::ErrorCode>(),
                 DownloadManager::ErrorCode::INVALID_CONFIG);
        QTRY_VERIFY(!manager.m_thread.isRunning());
        QTRY_VERIFY(manager.m_activeWorker.isNull());

        // 首次 worker 的线程退出后再次启动；第二次同样不触碰真实设备。
        manager.startDownload(profilePath, payloadPath);
        QTRY_COMPARE(errorSpy.count(), 2);
        QCOMPARE(errorSpy.at(1).at(0).value<DownloadManager::ErrorCode>(),
                 DownloadManager::ErrorCode::INVALID_CONFIG);
        QTRY_VERIFY(!manager.m_thread.isRunning());
        QTRY_VERIFY(manager.m_activeWorker.isNull());
    }

    void destructorCleansImmediatelyStartedWorker()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto* manager = new DownloadManager();
        QPointer<DownloadManager> managerGuard(manager);
        QSignalSpy startedSpy(&manager->m_thread, &QThread::started);
        int finishedCount = 0;
        QObject::connect(&manager->m_thread,
                         &QThread::finished,
                         manager,
                         [&finishedCount]() { ++finishedCount; },
                         Qt::DirectConnection);
        manager->startDownload(
                tempDir.filePath(QStringLiteral("missing-profile.json")),
                tempDir.filePath(QStringLiteral("missing-payload.bin")));
        QTRY_COMPARE(startedSpy.count(), 1);

        delete manager;

        QVERIFY(managerGuard.isNull());
        QCOMPARE(finishedCount, 1);
    }

    void diagnosticPortBusyFailsBeforeWorkerOpen()
    {
        const QString port = QStringLiteral("LH-diagnostic-busy");
        QVERIFY(Communication::tryClaimRtuPort(port, QStringLiteral("formal-backend")));

        DownloadManager manager;
        const QVariantMap comm {
            {QStringLiteral("protocol"), QStringLiteral("MODBUS")},
            {QStringLiteral("mode"), QStringLiteral("RTU")},
            {QStringLiteral("port"), port}
        };
        manager.setConfig({{QStringLiteral("comm"), comm}});
        QSignalSpy errorSpy(&manager, &DownloadManager::errorOccurred);
        manager.startConnectProbe();

        QTRY_COMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.first().at(0).value<DownloadManager::ErrorCode>(),
                 DownloadManager::ErrorCode::DEVICE_BUSY);
        QTRY_VERIFY(!manager.m_thread.isRunning());
        QTRY_VERIFY(manager.m_activeWorker.isNull());
        Communication::releaseRtuPort(port, QStringLiteral("formal-backend"));
    }

    void cancelReachesBridgeWithoutWorkerEventLoop()
    {
        ControllerBridge bridge(nullptr);
        DownloadManager manager;
        const auto cancellation = std::make_shared<DownloadCancellationHandle>();
        cancellation->setBridge(&bridge);
        manager.m_activeCancelState = cancellation;

        manager.cancel();

        QVERIFY(cancellation->isRequested());
        QVERIFY(bridge.m_abortRequested.load(std::memory_order_acquire));
        cancellation->clearBridge();
    }

    void bridgeStepPollRespectsDeadline()
    {
        ModbusInterface modbus;
        ControllerBridge bridge(&modbus);
        QVariantMap params;
        params.insert(QStringLiteral("address"), 100);
        params.insert(QStringLiteral("count"), 1);
        params.insert(QStringLiteral("expected"), QVariantList{42});
        params.insert(QStringLiteral("timeoutMs"), 200);
        params.insert(QStringLiteral("pollIntervalMs"), 5000);

        QElapsedTimer timer;
        timer.start();
        const bool ok = bridge.stepPoll(params);
        const qint64 elapsed = timer.elapsed();

        QVERIFY(!ok);
        // Acceptance criteria: timeout=200ms, interval=5000ms must not wait 5s
        QVERIFY2(elapsed < 800, qPrintable(QStringLiteral("Elapsed %1 ms, expected < 800 ms").arg(elapsed)));
    }

    void bridgeStepPollRespondsToAbortPromptly()
    {
        ModbusInterface modbus;
        ControllerBridge bridge(&modbus);
        QVariantMap params;
        params.insert(QStringLiteral("address"), 100);
        params.insert(QStringLiteral("count"), 1);
        params.insert(QStringLiteral("expected"), QVariantList{42});
        params.insert(QStringLiteral("timeoutMs"), 5000);
        params.insert(QStringLiteral("pollIntervalMs"), 2000);

        QTimer abortTimer;
        abortTimer.setSingleShot(true);
        QElapsedTimer abortElapsed;
        qint64 abortToReturnMs = 0;
        QObject::connect(&abortTimer, &QTimer::timeout, [&bridge, &abortElapsed]() {
            abortElapsed.start();
            bridge.requestAbort();
        });

        QThread timerThread;
        abortTimer.moveToThread(&timerThread);
        QObject::connect(&timerThread, &QThread::started, [&abortTimer]() {
            abortTimer.start(60);
        });
        timerThread.start();

        const bool ok = bridge.stepPoll(params);
        abortToReturnMs = abortElapsed.isValid() ? abortElapsed.elapsed() : 9999;

        timerThread.quit();
        timerThread.wait();

        QVERIFY(!ok);
        // Acceptance criteria: cancel response target <= 250 ms
        QVERIFY2(abortToReturnMs <= 250,
                 qPrintable(QStringLiteral("Abort response was %1 ms, expected <= 250 ms").arg(abortToReturnMs)));
    }
};

QTEST_MAIN(DownloadManagerLifecycleTest)
#include "download_manager_lifecycle_test.moc"
