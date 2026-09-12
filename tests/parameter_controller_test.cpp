/**
 * @file parameter_controller_test.cpp
 * @brief ParameterController 单元测试
 */

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QEventLoop>

#include "designer/ParameterController.h"
#include "communication/VirtualDeviceBackend.h"

class ScriptedReadbackBackend : public VirtualDeviceBackend
{
public:
    using VirtualDeviceBackend::VirtualDeviceBackend;

    bool readPoints(const QStringList& pointIds,
                    QHash<QString, QVariant>& values,
                    QString* errorMessage,
                    QHash<QString, CommError>* pointErrors = nullptr) override
    {
        if (!scripted) {
            return VirtualDeviceBackend::readPoints(pointIds, values, errorMessage, pointErrors);
        }

        values.clear();
        if (pointErrors)
            pointErrors->clear();
        if (errorMessage)
            errorMessage->clear();
        for (const auto& pointId : pointIds) {
            if (readbackValues.contains(pointId))
                values.insert(pointId, readbackValues.value(pointId));
        }
        return readResult;
    }

    bool scripted = false;
    bool readResult = true;
    QHash<QString, QVariant> readbackValues;
};

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

    void loadDefinitionsStoresCanonicalDataType()
    {
        ParameterController ctrl;
        auto def = makeParam("Kp", true, "0", "param.kp");
        def.dataType = QStringLiteral("FLOAT32");
        ctrl.loadDefinitions({def});
        ctrl.editParameter("Kp", QStringLiteral("1.5"));

        QCOMPARE(ctrl.parameterState("Kp").dataType, QStringLiteral("REAL"));

        def.dataType = QStringLiteral("DINT");
        ctrl.loadDefinitions({def});
        QCOMPARE(ctrl.parameterState("Kp").dataType, QStringLiteral("INT32"));
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

    void editByPointIdTransitionsToModified()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp", true, "0", "param.kp")});

        QVERIFY(ctrl.editParameterByPointId("param.kp", "2.0"));
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::Modified);
        QCOMPARE(ctrl.parameterState("Kp").editedValue, QStringLiteral("2.0"));
    }

    void parameterStateByPointIdReturnsMatchedState()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp", true, "0", "param.kp")});
        ctrl.editParameter("Kp", "1.5");

        const auto state = ctrl.parameterStateByPointId("param.kp");
        QCOMPARE(state.name, QStringLiteral("Kp"));
        QCOMPARE(state.pointId, QStringLiteral("param.kp"));
        QCOMPARE(state.state, ParameterState::Modified);
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
        const auto state = ctrl.parameterState("Kp");
        QCOMPARE(state.state, ParameterState::Confirmed);
        QCOMPARE(state.readbackAttempts, 1);
        QVERIFY(state.lastWriteTime.isValid());
        QVERIFY(state.lastReadbackTime.isValid());
    }

    void applyWithReadbackAsyncConfirmsParameter()
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

        QSignalSpy finishedSpy(&ctrl, &ParameterController::readbackFinished);
        QString errorMessage;
        QVERIFY(ctrl.applyModifiedParametersWithReadbackAsync(&backend, 1, 0, &errorMessage));
        QVERIFY(errorMessage.isEmpty());
        QTRY_COMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().at(0).toBool(), true);
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::Confirmed);
        QCOMPARE(ctrl.parameterState("Kp").readbackAttempts, 1);
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
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::Timeout);
        QVERIFY(!ctrl.parameterState("Kp").lastError.isEmpty());
        QVERIFY(!errorMessage.isEmpty());
    }

    void mixedWriteConfirmsSuccessPointsButReturnsFalse()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("A", true, "0", "param.a"),
                              makeParam("B", true, "0", "param.b"),
                              makeParam("C", true, "0", "param.c")});
        ctrl.editParameter("A", "2.0");
        ctrl.editParameter("B", "3.0");
        ctrl.editParameter("C", "4.0");

        VirtualDeviceBackend backend;
        RuntimePointDefinition writable;
        writable.id = "param.a";
        writable.name = "A";
        writable.kind = RuntimePointKind::Parameter;
        writable.dataType = "REAL";
        writable.access = RuntimePointAccess::ReadWrite;
        RuntimePointDefinition readOnly = writable;
        readOnly.id = "param.b";
        readOnly.name = "B";
        readOnly.access = RuntimePointAccess::ReadOnly;
        RuntimePointDefinition thirdWritable = writable;
        thirdWritable.id = "param.c";
        thirdWritable.name = "C";
        backend.loadPointDefinitions({writable, readOnly, thirdWritable});
        backend.connectBackend();

        QString errorMessage;
        QVERIFY(!ctrl.applyModifiedParametersWithReadback(&backend, 1, 0, &errorMessage));
        QCOMPARE(ctrl.parameterState("A").state, ParameterState::Confirmed);
        QCOMPARE(ctrl.parameterState("B").state, ParameterState::ApplyFailed);
        QCOMPARE(ctrl.parameterState("C").state, ParameterState::Confirmed);
        QVERIFY(!ctrl.parameterState("B").lastError.isEmpty());
    }

    void mixedWriteAsyncStartsAndFinishesFailure()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("A", true, "0", "param.a"),
                              makeParam("B", true, "0", "param.b"),
                              makeParam("C", true, "0", "param.c")});
        ctrl.editParameter("A", "2.0");
        ctrl.editParameter("B", "3.0");
        ctrl.editParameter("C", "4.0");

        VirtualDeviceBackend backend;
        RuntimePointDefinition writable;
        writable.id = "param.a";
        writable.name = "A";
        writable.kind = RuntimePointKind::Parameter;
        writable.dataType = "REAL";
        writable.access = RuntimePointAccess::ReadWrite;
        RuntimePointDefinition readOnly = writable;
        readOnly.id = "param.b";
        readOnly.name = "B";
        readOnly.access = RuntimePointAccess::ReadOnly;
        RuntimePointDefinition thirdWritable = writable;
        thirdWritable.id = "param.c";
        thirdWritable.name = "C";
        backend.loadPointDefinitions({writable, readOnly, thirdWritable});
        backend.connectBackend();

        QSignalSpy finishedSpy(&ctrl, &ParameterController::readbackFinished);
        QVERIFY(ctrl.applyModifiedParametersWithReadbackAsync(&backend, 1, 0));
        QCOMPARE(finishedSpy.count(), 0);
        QTRY_COMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().at(0).toBool(), false);
        QCOMPARE(ctrl.parameterState("A").state, ParameterState::Confirmed);
        QCOMPARE(ctrl.parameterState("B").state, ParameterState::ApplyFailed);
        QCOMPARE(ctrl.parameterState("C").state, ParameterState::Confirmed);
    }

    void allWriteFailuresAsyncDoNotStartReadback()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("A", true, "0", "param.a"),
                              makeParam("B", true, "0", "param.b")});
        ctrl.editParameter("A", "2.0");
        ctrl.editParameter("B", "3.0");

        VirtualDeviceBackend backend;
        RuntimePointDefinition readOnly;
        readOnly.kind = RuntimePointKind::Parameter;
        readOnly.dataType = "REAL";
        readOnly.access = RuntimePointAccess::ReadOnly;
        readOnly.id = "param.a";
        readOnly.name = "A";
        RuntimePointDefinition second = readOnly;
        second.id = "param.b";
        second.name = "B";
        backend.loadPointDefinitions({readOnly, second});
        backend.connectBackend();

        QSignalSpy finishedSpy(&ctrl, &ParameterController::readbackFinished);
        QVERIFY(!ctrl.applyModifiedParametersWithReadbackAsync(&backend, 1, 0));
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(ctrl.parameterState("A").state, ParameterState::ApplyFailed);
        QCOMPARE(ctrl.parameterState("B").state, ParameterState::ApplyFailed);
    }

    void syncMismatchReturnsFalse()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp", true, "0", "param.kp")});
        ctrl.editParameter("Kp", "2.0");

        ScriptedReadbackBackend backend;
        RuntimePointDefinition point;
        point.id = "param.kp";
        point.name = "Kp";
        point.kind = RuntimePointKind::Parameter;
        point.dataType = "REAL";
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();
        backend.scripted = true;
        backend.readbackValues.insert("param.kp", 999.0);

        QVERIFY(!ctrl.applyModifiedParametersWithReadback(&backend, 1, 0));
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::Mismatch);
    }

    void asyncMismatchReturnsFalseAfterStarting()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp", true, "0", "param.kp")});
        ctrl.editParameter("Kp", "2.0");

        ScriptedReadbackBackend backend;
        RuntimePointDefinition point;
        point.id = "param.kp";
        point.name = "Kp";
        point.kind = RuntimePointKind::Parameter;
        point.dataType = "REAL";
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();
        backend.scripted = true;
        backend.readbackValues.insert("param.kp", 999.0);

        QSignalSpy finishedSpy(&ctrl, &ParameterController::readbackFinished);
        QVERIFY(ctrl.applyModifiedParametersWithReadbackAsync(&backend, 1, 0));
        QCOMPARE(finishedSpy.count(), 0);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().at(0).toBool(), false);
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::Mismatch);
    }

    void asyncRetryExhaustionReturnsTimeout()
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

        QSignalSpy finishedSpy(&ctrl, &ParameterController::readbackFinished);
        QVERIFY(ctrl.applyModifiedParametersWithReadbackAsync(&backend, 1, 0));
        QTRY_COMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().at(0).toBool(), false);
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::Timeout);
    }

    void destroyingBackendBeforeReadbackTimerFailsOnce()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp", true, "0", "param.kp")});
        ctrl.editParameter("Kp", QStringLiteral("2.0"));

        auto* backend = new ScriptedReadbackBackend;
        RuntimePointDefinition point;
        point.id = QStringLiteral("param.kp");
        point.name = QStringLiteral("Kp");
        point.kind = RuntimePointKind::Parameter;
        point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadWrite;
        backend->loadPointDefinitions({point});
        backend->connectBackend();

        QSignalSpy finishedSpy(&ctrl, &ParameterController::readbackFinished);
        QVERIFY(ctrl.applyModifiedParametersWithReadbackAsync(backend, 2, 20));
        delete backend;

        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().at(0).toBool(), false);
        QCOMPARE(ctrl.parameterState(QStringLiteral("Kp")).state, ParameterState::Timeout);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCOMPARE(finishedSpy.count(), 1);
    }

    void destroyingBackendDuringReadbackRetryFailsOnce()
    {
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("Kp", true, "0", "param.kp")});
        ctrl.editParameter("Kp", QStringLiteral("2.0"));

        auto* backend = new ScriptedReadbackBackend;
        RuntimePointDefinition point;
        point.id = QStringLiteral("param.kp");
        point.name = QStringLiteral("Kp");
        point.kind = RuntimePointKind::Parameter;
        point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadWrite;
        backend->loadPointDefinitions({point});
        backend->connectBackend();
        backend->scripted = true;

        QSignalSpy finishedSpy(&ctrl, &ParameterController::readbackFinished);
        QVERIFY(ctrl.applyModifiedParametersWithReadbackAsync(backend, 3, 50));
        QTRY_VERIFY(ctrl.parameterState(QStringLiteral("Kp")).readbackAttempts >= 1);
        delete backend;

        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().at(0).toBool(), false);
        QTest::qWait(80);
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(ctrl.parameterState(QStringLiteral("Kp")).state, ParameterState::Timeout);
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

    void typedBoolReadbackUsesStrictValues()
    {
        ParameterController ctrl;
        auto def = makeParam("Enabled", true, "0", "param.enabled");
        def.dataType = QStringLiteral("BOOL");
        ctrl.loadDefinitions({def});

        VirtualDeviceBackend backend;
        RuntimePointDefinition point;
        point.id = QStringLiteral("param.enabled");
        point.name = QStringLiteral("Enabled");
        point.kind = RuntimePointKind::Parameter;
        point.dataType = QStringLiteral("BOOL");
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();

        QHash<QString, QVariant> readback;
        ctrl.editParameter(QStringLiteral("Enabled"), QStringLiteral("1"));
        QVERIFY(ctrl.applyModifiedParameters(&backend));
        readback.insert(QStringLiteral("param.enabled"), QVariant(true));
        ctrl.onReadbackValues(readback);
        QCOMPARE(ctrl.parameterState("Enabled").state, ParameterState::Confirmed);

        ctrl.editParameter(QStringLiteral("Enabled"), QStringLiteral("0"));
        QVERIFY(ctrl.applyModifiedParameters(&backend));
        readback.clear();
        readback.insert(QStringLiteral("param.enabled"), QVariant(false));
        ctrl.onReadbackValues(readback);
        QCOMPARE(ctrl.parameterState("Enabled").state, ParameterState::Confirmed);
    }

    void typedIntegerReadbackRejectsFractionAndDifferentValue()
    {
        ParameterController ctrl;
        auto def = makeParam("Count", true, "0", "param.count");
        def.dataType = QStringLiteral("INT16");
        ctrl.loadDefinitions({def});

        VirtualDeviceBackend backend;
        RuntimePointDefinition point;
        point.id = QStringLiteral("param.count");
        point.name = QStringLiteral("Count");
        point.kind = RuntimePointKind::Parameter;
        point.dataType = QStringLiteral("INT16");
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();

        QHash<QString, QVariant> readback;
        ctrl.editParameter(QStringLiteral("Count"), QStringLiteral("7"));
        QVERIFY(ctrl.applyModifiedParameters(&backend));
        readback.insert(QStringLiteral("param.count"), QVariant(qint16(7)));
        ctrl.onReadbackValues(readback);
        QCOMPARE(ctrl.parameterState("Count").state, ParameterState::Confirmed);

        ctrl.editParameter(QStringLiteral("Count"), QStringLiteral("7.5"));
        QVERIFY(ctrl.applyModifiedParameters(&backend));
        readback.clear();
        readback.insert(QStringLiteral("param.count"), QVariant(qint16(7)));
        ctrl.onReadbackValues(readback);
        QCOMPARE(ctrl.parameterState("Count").state, ParameterState::Mismatch);

        ctrl.editParameter(QStringLiteral("Count"), QStringLiteral("8"));
        QVERIFY(ctrl.applyModifiedParameters(&backend));
        readback.clear();
        readback.insert(QStringLiteral("param.count"), QVariant(qint16(7)));
        ctrl.onReadbackValues(readback);
        QCOMPARE(ctrl.parameterState("Count").state, ParameterState::Mismatch);

        def.dataType = QStringLiteral("UINT16");
        ctrl.loadDefinitions({def});
        ctrl.editParameter(QStringLiteral("Count"), QStringLiteral("65535"));
        QVERIFY(ctrl.applyModifiedParameters(&backend));
        readback.clear();
        readback.insert(QStringLiteral("param.count"), QVariant(quint16(65535)));
        ctrl.onReadbackValues(readback);
        QCOMPARE(ctrl.parameterState("Count").state, ParameterState::Confirmed);

        ctrl.editParameter(QStringLiteral("Count"), QStringLiteral("-1"));
        QVERIFY(ctrl.applyModifiedParameters(&backend));
        readback.clear();
        readback.insert(QStringLiteral("param.count"), QVariant(quint16(65535)));
        ctrl.onReadbackValues(readback);
        QCOMPARE(ctrl.parameterState("Count").state, ParameterState::Mismatch);
    }

    void typedRealReadbackUsesFloat32AndRejectsInvalid()
    {
        ParameterController ctrl;
        auto def = makeParam("Gain", true, "0", "param.gain");
        def.dataType = QStringLiteral("FLOAT32");
        ctrl.loadDefinitions({def});

        VirtualDeviceBackend backend;
        RuntimePointDefinition point;
        point.id = QStringLiteral("param.gain");
        point.name = QStringLiteral("Gain");
        point.kind = RuntimePointKind::Parameter;
        point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        backend.connectBackend();

        QHash<QString, QVariant> readback;
        ctrl.editParameter(QStringLiteral("Gain"), QStringLiteral("0.1"));
        QVERIFY(ctrl.applyModifiedParameters(&backend));
        readback.insert(QStringLiteral("param.gain"), QVariant(float(0.1f)));
        ctrl.onReadbackValues(readback);
        QCOMPARE(ctrl.parameterState("Gain").state, ParameterState::Confirmed);

        ctrl.editParameter(QStringLiteral("Gain"), QStringLiteral("0.10000002"));
        QVERIFY(ctrl.applyModifiedParameters(&backend));
        readback.clear();
        readback.insert(QStringLiteral("param.gain"), QVariant(float(0.1f)));
        ctrl.onReadbackValues(readback);
        QCOMPARE(ctrl.parameterState("Gain").state, ParameterState::Mismatch);

        ctrl.editParameter(QStringLiteral("Gain"), QStringLiteral("0.2"));
        QVERIFY(ctrl.applyModifiedParameters(&backend));
        readback.clear();
        readback.insert(QStringLiteral("param.gain"), QVariant());
        ctrl.onReadbackValues(readback);
        QCOMPARE(ctrl.parameterState("Gain").state, ParameterState::Mismatch);
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
