/**
 * @file monitor_manager_backend_test.cpp
 * @brief MonitorManager backend 切换测试
 */

#include <QtTest/QtTest>

#include <utility>

#include "common/ConfigTypes.h"
#include "communication/IDeviceBackend.h"
#include "communication/RuntimePointQualityMapper.h"
#include "communication/VirtualDeviceBackend.h"
#include "monitor/MonitorDataProcessor.h"
#include "monitor/MonitorManager.h"

using namespace Monitor;

class FailingReadBackend final : public IDeviceBackend
{
public:
    bool connectBackend() override { return true; }
    void disconnectBackend() override {}
    bool isOnline() const override { return true; }

    bool readPoints(const QStringList& pointIds,
                    QHash<QString, QVariant>& values,
                    QString* errorMessage,
                    QHash<QString, CommError>* pointErrors) override
    {
        values.clear();
        const CommError error(CommProtocolType::Custom,
                              CommErrorCode::InvalidAddress,
                              QStringLiteral("read failed"));
        if (errorMessage) {
            *errorMessage = error.message;
        }
        if (pointErrors) {
            for (const QString& pointId : pointIds) {
                pointErrors->insert(pointId, error);
            }
        }
        return false;
    }

    bool writePoints(const QHash<QString, QVariant>&,
                     QString* errorMessage,
                     QHash<QString, CommError>*) override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("unsupported");
        }
        return false;
    }

    bool downloadArtifact(const QString&,
                          const QVariantMap&,
                          QString* errorMessage,
                          CommError*) override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("unsupported");
        }
        return false;
    }

    BackendStatusSnapshot statusSnapshot() const override
    {
        BackendStatusSnapshot status;
        status.online = true;
        status.backendType = QStringLiteral("failing-read");
        status.lastErrorCode = CommErrorCode::InvalidAddress;
        status.lastErrorMessage = QStringLiteral("read failed");
        return status;
    }
};

class RecordingReadBackend final : public IDeviceBackend
{
public:
    bool connectBackend() override
    {
        m_online = true;
        return true;
    }

    void disconnectBackend() override { m_online = false; }
    bool isOnline() const override { return m_online; }

    bool readPoints(const QStringList& pointIds,
                    QHash<QString, QVariant>& values,
                    QString* errorMessage,
                    QHash<QString, CommError>* pointErrors) override
    {
        Q_UNUSED(errorMessage);
        if (pointErrors) {
            pointErrors->clear();
        }
        requests.append(pointIds);
        values.clear();
        for (const QString& pointId : pointIds) {
            if (pointValues.contains(pointId)) {
                values.insert(pointId, pointValues.value(pointId));
            }
        }
        return true;
    }

    bool writePoints(const QHash<QString, QVariant>&,
                     QString* errorMessage,
                     QHash<QString, CommError>*) override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("unsupported");
        }
        return false;
    }

    bool downloadArtifact(const QString&,
                          const QVariantMap&,
                          QString* errorMessage,
                          CommError*) override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("unsupported");
        }
        return false;
    }

    BackendStatusSnapshot statusSnapshot() const override
    {
        BackendStatusSnapshot status;
        status.online = m_online;
        status.backendType = QStringLiteral("recording");
        return status;
    }

    QList<QStringList> requests;
    QHash<QString, QVariant> pointValues;
    bool m_online = true;
};

class MonitorManagerBackendTest : public QObject
{
    Q_OBJECT

private:
    static RuntimePointDefinition makePoint()
    {
        RuntimePointDefinition point;
        point.id = QStringLiteral("pt1");
        point.name = QStringLiteral("Point1");
        point.kind = RuntimePointKind::Status;
        point.access = RuntimePointAccess::ReadWrite;
        point.dataType = QStringLiteral("REAL");
        point.defaultValue = 0.0;
        return point;
    }

    static ProjectRuntimeConfig makeConfig()
    {
        ProjectRuntimeConfig cfg;
        cfg.projectName = QStringLiteral("backend-switch-test");
        MonitorProviderRuntimeConfig provider;
        provider.id = QStringLiteral("pt1");
        provider.channelName = QStringLiteral("channel.1");
        provider.unit = QStringLiteral("bar");
        provider.periodMs = 50;
        cfg.providers.append(provider);
        return cfg;
    }

private slots:
    void init()
    {
        auto& manager = MonitorManager::instance();
        manager.setDatabaseLoggingEnabled(false);
        manager.stopMonitoring();
        manager.setDataProcessor(nullptr);
        manager.setDeviceBackend(nullptr);
        manager.applyConfiguration(ProjectRuntimeConfig());
        manager.clearAllData();
    }

    void cleanup()
    {
        auto& manager = MonitorManager::instance();
        manager.stopMonitoring();
        manager.setDataProcessor(nullptr);
        manager.setDeviceBackend(nullptr);
        manager.applyConfiguration(ProjectRuntimeConfig());
        manager.clearAllData();
    }

    void switchingBackendDisconnectsOldPushSource()
    {
        auto& manager = MonitorManager::instance();
        VirtualDeviceBackend backend1;
        VirtualDeviceBackend backend2;

        backend1.loadPointDefinitions({makePoint()});
        backend2.loadPointDefinitions({makePoint()});
        backend1.connectBackend();
        backend2.connectBackend();

        manager.setDeviceBackend(&backend1);
        QVERIFY(manager.applyConfiguration(makeConfig()));
        manager.startMonitoring();

        QSignalSpy spy(&manager, &MonitorManager::sampleRecorded);

        QVERIFY(backend1.writePoints({{QStringLiteral("pt1"), 1.0}}, nullptr));
        QCOMPARE(spy.count(), 1);

        manager.setDeviceBackend(&backend2);
        QVERIFY(manager.applyConfiguration(makeConfig()));

        QVERIFY(backend1.writePoints({{QStringLiteral("pt1"), 2.0}}, nullptr));
        QCOMPARE(spy.count(), 1);

        QVERIFY(backend2.writePoints({{QStringLiteral("pt1"), 3.0}}, nullptr));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("channel.1"));
        QCOMPARE(spy.at(1).at(1).toDouble(), 3.0);

        manager.setDeviceBackend(nullptr);
        QVERIFY(backend2.writePoints({{QStringLiteral("pt1"), 4.0}}, nullptr));
        QCOMPARE(spy.count(), 2);
    }

    void backendPollCarriesBackendStatusMetadata()
    {
        auto& manager = MonitorManager::instance();
        VirtualDeviceBackend backend;

        backend.loadPointDefinitions({makePoint()});
        backend.connectBackend();

        manager.setDeviceBackend(&backend);
        QVERIFY(manager.applyConfiguration(makeConfig()));
        manager.startMonitoring();

        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));

        const auto history = manager.history(QStringLiteral("channel.1"), 1);
        QVERIFY(!history.isEmpty());
        const auto sample = history.last();
        QCOMPARE(sample.metadata.value(QStringLiteral("source")).toString(), QStringLiteral("backend_poll"));
        QCOMPARE(sample.metadata.value(QStringLiteral("backendType")).toString(), QStringLiteral("virtual"));
        QVERIFY(sample.metadata.contains(QStringLiteral("backendOnline")));
        QVERIFY(sample.metadata.contains(QStringLiteral("backendLastErrorCode")));
    }

    void qualityAndErrorHelpersAreStable()
    {
        QCOMPARE(commErrorCodeToString(CommErrorCode::ConnectionLost), QStringLiteral("ConnectionLost"));
        QCOMPARE(runtimePointQualityToString(RuntimePointQuality::Good), QStringLiteral("Good"));

        const CommError timeoutError(CommProtocolType::Custom, CommErrorCode::ConnectionTimeout, QStringLiteral("timeout"));
        QCOMPARE(runtimePointQualityFromBackendError(timeoutError, true), RuntimePointQuality::Stale);

        const CommError offlineError(CommProtocolType::Custom, CommErrorCode::ConnectionLost, QStringLiteral("lost"));
        QCOMPARE(runtimePointQualityFromBackendError(offlineError, false), RuntimePointQuality::Offline);

        const CommError badError(CommProtocolType::Custom, CommErrorCode::InvalidAddress, QStringLiteral("bad address"));
        QCOMPARE(runtimePointQualityFromBackendError(badError, true), RuntimePointQuality::Bad);
    }

    void failedPollDoesNotCreateNumericZero()
    {
        auto& manager = MonitorManager::instance();
        FailingReadBackend backend;
        MonitorDataProcessor processor;

        manager.setDataProcessor(&processor);
        manager.setDeviceBackend(&backend);
        QVERIFY(manager.applyConfiguration(makeConfig()));
        manager.startMonitoring();

        QSignalSpy numericSpy(&manager, &MonitorManager::sampleRecorded);
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));

        QCOMPARE(numericSpy.count(), 0);
        QVERIFY(manager.history(QStringLiteral("channel.1"), 1).isEmpty());
        QVERIFY(processor.getChannelData(QStringLiteral("channel.1")).isEmpty());
    }

    void destroyedObserversAreCleared()
    {
        auto& manager = MonitorManager::instance();

        auto* processor = new MonitorDataProcessor;
        manager.setDataProcessor(processor);
        delete processor;
        QVERIFY(manager.dataProcessor() == nullptr);

        auto* backend = new FailingReadBackend;
        manager.setDeviceBackend(backend);
        QVERIFY(manager.applyConfiguration(makeConfig()));
        manager.startMonitoring();
        delete backend;
        QVERIFY(manager.deviceBackend() == nullptr);

        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));
    }

    void duplicatePointIdsAreReadOnceAndKeepLastChannelMapping()
    {
        auto& manager = MonitorManager::instance();
        RecordingReadBackend backend;
        backend.connectBackend();
        backend.pointValues.insert(QStringLiteral("pt1"), 42.0);

        ProjectRuntimeConfig cfg;
        MonitorProviderRuntimeConfig first;
        first.id = QStringLiteral("pt1");
        first.channelName = QStringLiteral("channel.first");
        first.periodMs = 1000;
        cfg.providers.append(first);
        MonitorProviderRuntimeConfig second = first;
        second.channelName = QStringLiteral("channel.last");
        second.periodMs = 1000;
        cfg.providers.append(second);

        manager.setDeviceBackend(&backend);
        QVERIFY(manager.applyConfiguration(cfg));
        manager.startMonitoring();
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));

        QCOMPARE(backend.requests.size(), 1);
        QCOMPARE(backend.requests.first(), QStringList{QStringLiteral("pt1")});
        const auto history = manager.history(QStringLiteral("channel.last"), 1);
        QVERIFY(!history.isEmpty());
        QCOMPARE(history.last().value, 42.0);
        QVERIFY(manager.history(QStringLiteral("channel.first"), 1).isEmpty());
    }

    void backendPointsRespectIndividualPeriods()
    {
        auto& manager = MonitorManager::instance();
        RecordingReadBackend backend;
        backend.connectBackend();
        backend.pointValues.insert(QStringLiteral("fast"), 1.0);
        backend.pointValues.insert(QStringLiteral("slow"), 2.0);

        ProjectRuntimeConfig cfg;
        MonitorProviderRuntimeConfig fast;
        fast.id = QStringLiteral("fast");
        fast.channelName = QStringLiteral("channel.fast");
        fast.periodMs = 30;
        cfg.providers.append(fast);
        MonitorProviderRuntimeConfig slow;
        slow.id = QStringLiteral("slow");
        slow.channelName = QStringLiteral("channel.slow");
        slow.periodMs = 120;
        cfg.providers.append(slow);

        manager.setDeviceBackend(&backend);
        QVERIFY(manager.applyConfiguration(cfg));
        manager.startMonitoring();
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));
        QCOMPARE(backend.requests.size(), 1);

        QTest::qWait(45);
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));
        manager.stopMonitoring();

        QVERIFY(!backend.requests.isEmpty());
        for (int i = 1; i < backend.requests.size(); ++i) {
            QVERIFY2(!backend.requests.at(i).contains(QStringLiteral("slow")),
                     "slow point was read on the shortest provider period");
        }
        bool sawFast = false;
        for (int i = 1; i < backend.requests.size(); ++i) {
            sawFast = sawFast || backend.requests.at(i).contains(QStringLiteral("fast"));
        }
        QVERIFY(sawFast);

        manager.startMonitoring();
        QTRY_VERIFY_WITH_TIMEOUT([&backend]() {
            for (const QStringList& request : std::as_const(backend.requests)) {
                if (request.contains(QStringLiteral("slow"))) {
                    return true;
                }
            }
            return false;
        }(), 150);
        manager.stopMonitoring();
    }

    void repeatedStopPreventsProviderAndBackendReads()
    {
        auto& manager = MonitorManager::instance();
        RecordingReadBackend backend;
        backend.connectBackend();
        backend.pointValues.insert(QStringLiteral("pt1"), 7.0);
        manager.setDeviceBackend(&backend);
        QVERIFY(manager.applyConfiguration(makeConfig()));
        manager.startMonitoring();
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));
        const int readsAtStop = backend.requests.size();

        manager.stopMonitoring();
        manager.stopMonitoring();
        QVERIFY(!manager.isMonitoring());
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));
        QTest::qWait(35);
        QCOMPARE(backend.requests.size(), readsAtStop);
    }

    void explicitShutdownIsIdempotentAndStopsBackendPolling()
    {
        auto& manager = MonitorManager::instance();
        RecordingReadBackend backend;
        backend.connectBackend();
        backend.pointValues.insert(QStringLiteral("pt1"), 7.0);
        manager.setDeviceBackend(&backend);
        QVERIFY(manager.applyConfiguration(makeConfig()));
        manager.startMonitoring();
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));
        const int readsBeforeShutdown = backend.requests.size();

        manager.shutdown();
        manager.shutdown();

        QVERIFY(!manager.isMonitoring());
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));
        QTest::qWait(60);
        QCOMPARE(backend.requests.size(), readsBeforeShutdown);
    }

    void repeatedStopStopsProviderTimer()
    {
        auto& manager = MonitorManager::instance();
        manager.setDeviceBackend(nullptr);
        QVERIFY(manager.applyConfiguration(makeConfig()));

        QSignalSpy samples(&manager, &MonitorManager::sampleRecorded);
        manager.startMonitoring();
        QTRY_VERIFY_WITH_TIMEOUT(samples.count() > 0, 250);
        manager.stopMonitoring();
        manager.stopMonitoring();
        const int samplesAtStop = samples.count();
        QTest::qWait(50);
        QCOMPARE(samples.count(), samplesAtStop);
    }
};

QTEST_MAIN(MonitorManagerBackendTest)
#include "monitor_manager_backend_test.moc"
