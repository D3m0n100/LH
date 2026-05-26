/**
 * @file parameter_controller_test.cpp
 * @brief ParameterController 单元测试
 */

#include <QtTest/QtTest>

#include "designer/ParameterController.h"
#include "communication/VirtualDeviceBackend.h"

class ParameterControllerTest : public QObject
{
    Q_OBJECT

private:
    static ParameterDefinition makeParam(const QString& name,
                                         bool editable = true,
                                         const QString& defaultVal = "0",
                                         const QString& id = QString())
    {
        ParameterDefinition def;
        def.id = id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id;
        def.name = name;
        def.dataType = "REAL";
        def.defaultValue = defaultVal;
        def.currentValue = defaultVal;
        def.onlineEditable = editable;
        return def;
    }

private slots:
    void init()
    {
        qRegisterMetaType<ParameterState>("ParameterState");
    }

    void cleanup() {}

    void loadDefinitionsInitializesClean()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp"), makeParam("Ki")});

        const auto states = ctrl.parameterStates();
        QCOMPARE(states.size(), 2);
        for (const auto& state : states)
            QCOMPARE(state.state, ParameterState::Clean);
    }

    void loadDefinitionsPreservesState()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp")});
        ctrl.editParameter("Kp", "1.5");

        ctrl.loadDefinitions({makeParam("Kp")});
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::Modified);
        QCOMPARE(ctrl.parameterState("Kp").editedValue, QStringLiteral("1.5"));
    }

    void clearRemovesAll()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp")});
        ctrl.clear();
        QCOMPARE(ctrl.parameterStates().size(), 0);
    }

    void editTransitionsToModified()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp")});

        QVERIFY(ctrl.editParameter("Kp", "2.0"));
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::Modified);
        QCOMPARE(ctrl.parameterState("Kp").editedValue, QStringLiteral("2.0"));
    }

    void editReadOnlyFails()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp", false)});

        QVERIFY(!ctrl.editParameter("Kp", "2.0"));
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::Clean);
    }

    void editNonexistentFails()
    {
        ParameterController ctrl;
        QVERIFY(!ctrl.editParameter("no_such", "1.0"));
    }

    void editEmitsStateChanged()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp")});

        QSignalSpy spy(&ctrl, &ParameterController::stateChanged);
        ctrl.editParameter("Kp", "1.0");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QStringLiteral("Kp"));
        QCOMPARE(static_cast<ParameterState>(spy.first().at(1).toInt()), ParameterState::Clean);
        QCOMPARE(static_cast<ParameterState>(spy.first().at(2).toInt()), ParameterState::Modified);
    }

    void hasModifiedParameters()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp"), makeParam("Ki")});

        QVERIFY(!ctrl.hasModifiedParameters());
        ctrl.editParameter("Kp", "1.0");
        QVERIFY(ctrl.hasModifiedParameters());
    }

    void applyTransitionsToPendingReadback()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp", true, "0", "param.kp")});
        ctrl.editParameter("Kp", "2.0");

        VirtualDeviceBackend backend;
        RuntimePointDefinition point;
        point.id = "param.kp";
        point.name = "Kp";
        point.kind = RuntimePointKind::Parameter;
        point.access = RuntimePointAccess::ReadWrite;
        point.defaultValue = 0.0;
        backend.loadPointDefinitions({point});
        backend.connectBackend();

        QVERIFY(ctrl.applyModifiedParameters(&backend));
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::PendingReadback);
        QCOMPARE(ctrl.parameterState("Kp").appliedValue, QStringLiteral("2.0"));
    }

    void applyWithReadbackConfirmsParameter()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp", true, "0", "param.kp")});
        ctrl.editParameter("Kp", "2.0");

        VirtualDeviceBackend backend;
        RuntimePointDefinition point;
        point.id = "param.kp";
        point.name = "Kp";
        point.kind = RuntimePointKind::Parameter;
        point.access = RuntimePointAccess::ReadWrite;
        point.defaultValue = 0.0;
        backend.loadPointDefinitions({point});
        backend.connectBackend();

        QString errorMessage;
        QVERIFY(ctrl.applyModifiedParametersWithReadback(&backend, 1, 0, &errorMessage));
        QVERIFY(errorMessage.isEmpty());
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::Confirmed);
    }

    void applyWithNoModifiedReturnsTrue()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp")});

        VirtualDeviceBackend backend;
        backend.connectBackend();
        QVERIFY(ctrl.applyModifiedParameters(&backend));
    }

    void applyNullBackendFails()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp")});
        ctrl.editParameter("Kp", "1.0");

        QVERIFY(!ctrl.applyModifiedParameters(nullptr));
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::Modified);
    }

    void applyBackendWriteFailsGoesToApplyFailed()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp", true, "0", "param.kp")});
        ctrl.editParameter("Kp", "2.0");

        VirtualDeviceBackend backend;
        RuntimePointDefinition point;
        point.id = "param.kp";
        point.name = "Kp";
        point.kind = RuntimePointKind::Parameter;
        point.dataType = "REAL";
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();
        backend.setFaultInjection(false, true, false);

        QVERIFY(!ctrl.applyModifiedParameters(&backend));
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::ApplyFailed);
        QVERIFY(!ctrl.parameterState("Kp").lastError.isEmpty());
    }

    void applyWithReadbackFailureKeepsPendingAndStoresError()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp", true, "0", "param.kp")});
        ctrl.editParameter("Kp", "2.0");

        VirtualDeviceBackend backend;
        RuntimePointDefinition point;
        point.id = "param.kp";
        point.name = "Kp";
        point.kind = RuntimePointKind::Parameter;
        point.dataType = "REAL";
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();
        backend.setFaultInjection(true, false, false);

        QString errorMessage;
        QVERIFY(!ctrl.applyModifiedParametersWithReadback(&backend, 2, 0, &errorMessage));
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::PendingReadback);
        QVERIFY(!ctrl.parameterState("Kp").lastError.isEmpty());
        QVERIFY(!errorMessage.isEmpty());
    }

    void readbackMatchGoesToConfirmed()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp", true, "0", "param.kp")});
        ctrl.editParameter("Kp", "2.0");

        VirtualDeviceBackend backend;
        RuntimePointDefinition point;
        point.id = "param.kp";
        point.name = "Kp";
        point.kind = RuntimePointKind::Parameter;
        point.dataType = "REAL";
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();
        ctrl.applyModifiedParameters(&backend);

        QHash<QString, QVariant> readback;
        readback["param.kp"] = 2.0;
        ctrl.onReadbackValues(readback);

        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::Confirmed);
    }

    void readbackMismatchGoesToMismatch()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp", true, "0", "param.kp")});
        ctrl.editParameter("Kp", "2.0");

        VirtualDeviceBackend backend;
        RuntimePointDefinition point;
        point.id = "param.kp";
        point.name = "Kp";
        point.kind = RuntimePointKind::Parameter;
        point.dataType = "REAL";
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();
        ctrl.applyModifiedParameters(&backend);

        QHash<QString, QVariant> readback;
        readback["param.kp"] = 999.0;
        ctrl.onReadbackValues(readback);

        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::Mismatch);
    }

    void readbackStringMatchGoesToConfirmed()
    {
        ParameterController ctrl;
        auto def = makeParam("Mode", true, "MANUAL", "param.mode");
        def.dataType = "STRING";
        ctrl.loadDefinitions({def});
        ctrl.editParameter("Mode", "AUTO");

        VirtualDeviceBackend backend;
        RuntimePointDefinition point;
        point.id = "param.mode";
        point.name = "Mode";
        point.kind = RuntimePointKind::Parameter;
        point.dataType = "STRING";
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();
        ctrl.applyModifiedParameters(&backend);

        QHash<QString, QVariant> readback;
        readback["param.mode"] = QStringLiteral("AUTO");
        ctrl.onReadbackValues(readback);

        QCOMPARE(ctrl.parameterState("Mode").state, ParameterState::Confirmed);
    }

    void readbackIgnoresNonPendingParams()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp", true, "0", "param.kp"), makeParam("Ki")});
        ctrl.editParameter("Kp", "1.0");

        VirtualDeviceBackend backend;
        RuntimePointDefinition point;
        point.id = "param.kp";
        point.name = "Kp";
        point.kind = RuntimePointKind::Parameter;
        point.dataType = "REAL";
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();
        ctrl.applyModifiedParameters(&backend);

        QHash<QString, QVariant> readback;
        readback["Ki"] = 5.0;
        ctrl.onReadbackValues(readback);

        QCOMPARE(ctrl.parameterState("Ki").state, ParameterState::Clean);
    }

    void parameterNamesByState()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp"), makeParam("Ki"), makeParam("Kd")});
        ctrl.editParameter("Kp", "1.0");
        ctrl.editParameter("Ki", "2.0");

        const auto modified = ctrl.parameterNamesByState(ParameterState::Modified);
        QCOMPARE(modified.size(), 2);
        QVERIFY(modified.contains("Kp"));
        QVERIFY(modified.contains("Ki"));
    }
};

QTEST_MAIN(ParameterControllerTest)
#include "parameter_controller_test.moc"
