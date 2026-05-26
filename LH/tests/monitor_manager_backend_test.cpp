/**
 * @file monitor_manager_backend_test.cpp
 * @brief MonitorManager backend 切换测试
 */

#include <QtTest/QtTest>

#include "common/ConfigTypes.h"
#include "communication/VirtualDeviceBackend.h"
#include "monitor/MonitorManager.h"

using namespace Monitor;

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
        provider.periodMs = 20;
        cfg.providers.append(provider);
        return cfg;
    }

private slots:
    void init()
    {
        auto& manager = MonitorManager::instance();
        manager.setDatabaseLoggingEnabled(false);
        manager.stopMonitoring();
        manager.setDeviceBackend(nullptr);
        manager.applyConfiguration(ProjectRuntimeConfig());
        manager.clearAllData();
    }

    void cleanup()
    {
        auto& manager = MonitorManager::instance();
        manager.stopMonitoring();
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
};

QTEST_MAIN(MonitorManagerBackendTest)
#include "monitor_manager_backend_test.moc"
