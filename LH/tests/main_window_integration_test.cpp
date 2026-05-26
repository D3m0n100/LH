/**
 * @file main_window_integration_test.cpp
 * @brief MainWindow 参数下发-回读集成测试
 */

#include <QtTest/QtTest>

#include "designer/MainWindow.h"
#include "designer/ProjectController.h"
#include "designer/ParameterController.h"
#include "communication/VirtualDeviceBackend.h"
#include "monitor/MonitorManager.h"

class MainWindowIntegrationTest : public QObject
{
    Q_OBJECT

private:
    static ParameterDefinition makeParameter()
    {
        ParameterDefinition p;
        p.id = QStringLiteral("param.kp");
        p.name = QStringLiteral("Kp");
        p.dataType = QStringLiteral("REAL");
        p.defaultValue = QStringLiteral("1.0");
        p.currentValue = QStringLiteral("1.0");
        p.onlineEditable = true;
        p.confirmed = false;
        return p;
    }

    static RuntimePointDefinition makePoint()
    {
        RuntimePointDefinition point;
        point.id = QStringLiteral("param.kp");
        point.name = QStringLiteral("Kp");
        point.kind = RuntimePointKind::Parameter;
        point.access = RuntimePointAccess::ReadWrite;
        point.dataType = QStringLiteral("REAL");
        point.defaultValue = 1.0;
        return point;
    }

private slots:
    void init()
    {
        qRegisterMetaType<ProjectRuntimeConfig>("ProjectRuntimeConfig");
        auto& manager = Monitor::MonitorManager::instance();
        manager.setDatabaseLoggingEnabled(false);
        manager.stopMonitoring();
        manager.setDeviceBackend(nullptr);
        manager.applyConfiguration(ProjectRuntimeConfig());
        manager.clearAllData();
    }

    void cleanup()
    {
        auto& manager = Monitor::MonitorManager::instance();
        manager.stopMonitoring();
        manager.setDeviceBackend(nullptr);
        manager.applyConfiguration(ProjectRuntimeConfig());
        manager.clearAllData();
    }

    void applyParameterSyncsConfirmedAndCurrentValue()
    {
        MainWindow* window = new MainWindow();
        auto* projectController = window->findChild<ProjectController*>();
        auto* parameterController = window->findChild<ParameterController*>();
        QVERIFY(projectController != nullptr);
        QVERIFY(parameterController != nullptr);

        ProjectRuntimeConfig cfg;
        cfg.projectName = QStringLiteral("mw-integration-test");
        cfg.parameters.append(makeParameter());
        projectController->runtimeConfig() = cfg;

        QVERIFY(QMetaObject::invokeMethod(
            window,
            "onProjectOpened",
            Qt::DirectConnection,
            Q_ARG(ProjectRuntimeConfig, cfg)));

        QVERIFY(parameterController->editParameter(QStringLiteral("Kp"), QStringLiteral("2.5")));

        VirtualDeviceBackend backend;
        backend.loadPointDefinitions({makePoint()});
        backend.connectBackend();
        Monitor::MonitorManager::instance().setDeviceBackend(&backend);

        QVERIFY(QMetaObject::invokeMethod(window, "onApplyParametersRequested", Qt::DirectConnection));

        const auto& updatedCfg = window->runtimeConfig();
        QCOMPARE(updatedCfg.parameters.size(), 1);
        QCOMPARE(updatedCfg.parameters.first().name, QStringLiteral("Kp"));
        QVERIFY(updatedCfg.parameters.first().confirmed);
        QCOMPARE(updatedCfg.parameters.first().currentValue, QStringLiteral("2.5"));

        Monitor::MonitorManager::instance().setDeviceBackend(nullptr);
    }
};

QTEST_MAIN(MainWindowIntegrationTest)
#include "main_window_integration_test.moc"
