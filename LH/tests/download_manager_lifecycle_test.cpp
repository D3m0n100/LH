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

Q_DECLARE_METATYPE(DownloadManager::ErrorCode)

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
};

QTEST_MAIN(DownloadManagerLifecycleTest)
#include "download_manager_lifecycle_test.moc"
