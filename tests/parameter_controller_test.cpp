/**
 * @file parameter_controller_test.cpp
 * @brief ParameterController 单元测试
 */

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QPointer>
#include <QScopedValueRollback>

#include "designer/ParameterController.h"
#include "communication/VirtualDeviceBackend.h"

class ScriptedReadbackBackend : public VirtualDeviceBackend
{
public:
    using VirtualDeviceBackend::VirtualDeviceBackend;
    void readPointsAsync(const QStringList& ids, int budget, std::shared_ptr<std::atomic_bool> cancelled,
                         QObject* context, ReadCompletion completion) override
    {
        ++asyncReadCalls;
        if (!scripted) { VirtualDeviceBackend::readPointsAsync(ids, budget, cancelled, context, completion); return; }
          QPointer<ScriptedReadbackBackend> safe(this);
          QTimer::singleShot(0, context, [=] {
              if (!safe) return;
            QHash<QString, QVariant> values; QHash<QString, CommError> errors; QString message;
              const bool ok = (!cancelled || !cancelled->load()) && safe->readPoints(ids, values, &message, &errors);
              completion(ok, values, message, errors, safe->statusSnapshot());
        });
    }

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
    int asyncReadCalls = 0;
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
    void readbackNotificationReentry_data()
    {
        QTest::addColumn<int>("entry"); // external values, sync polling, async polling
        QTest::addColumn<int>("terminal");
        QTest::addColumn<int>("action");
        const QList<ParameterState> terminals{ParameterState::Confirmed,
                                              ParameterState::Mismatch,
                                              ParameterState::Timeout};
        for (int entry = 0; entry < 3; ++entry) {
            for (auto terminal : terminals) {
                if (entry == 0 && terminal == ParameterState::Timeout) continue;
                for (int action = 0; action < 6; ++action) {
                    const QByteArray name = QStringLiteral("entry_%1_state_%2_action_%3")
                            .arg(entry).arg(int(terminal)).arg(action).toLatin1();
                    QTest::newRow(name.constData()) << entry << int(terminal) << action;
                }
            }
        }
    }

    void readbackNotificationReentry()
    {
        QFETCH(int, entry);
        QFETCH(int, terminal);
        QFETCH(int, action);
        const auto expected = static_cast<ParameterState>(terminal);
        const QList<ParameterDefinition> definitions{makeParam("A", true, "0", "param.a"),
                                                      makeParam("B", true, "0", "param.b")};
        QPointer<ParameterController> ctrl = new ParameterController;
        ctrl->loadDefinitions(definitions);
        QVERIFY(ctrl->editParameter("A", "2"));
        QVERIFY(ctrl->editParameter("B", "2"));
        ScriptedReadbackBackend backend;
        QList<RuntimePointDefinition> points;
        for (const auto& def : definitions) {
            RuntimePointDefinition point;
            point.id = def.id; point.name = def.name;
            point.kind = RuntimePointKind::Parameter;
            point.dataType = QStringLiteral("REAL");
            point.access = RuntimePointAccess::ReadWrite;
            points.append(point);
        }
        backend.loadPointDefinitions(points);
        QVERIFY(backend.connectBackend());
        backend.scripted = true;
        if (expected != ParameterState::Timeout) {
            backend.readbackValues.insert("param.a", expected == ParameterState::Confirmed ? 2.0 : 99.0);
            backend.readbackValues.insert("param.b", expected == ParameterState::Confirmed ? 2.0 : 99.0);
        }
        bool acted = false;
        int terminalNotifications = 0;
        int completions = 0;
        connect(ctrl.data(), &ParameterController::readbackFinished, &backend,
                [&](bool, const QString&) { ++completions; });
        connect(ctrl.data(), &ParameterController::stateChanged, &backend,
                [&](const QString&, ParameterState, ParameterState next) {
            if (next != expected) return;
            ++terminalNotifications;
            if (acted) return;
            acted = true;
            // Both parameters must already have their new state before the first signal.
            QCOMPARE(ctrl->parameterState("A").state, expected);
            QCOMPARE(ctrl->parameterState("B").state, expected);
            switch (action) {
            case 0: ctrl->loadDefinitions({}); break;
            case 1: ctrl->clear(); break;
            case 2: ctrl->loadDefinitions({makeParam("New", true, "0", "other.project")}); break;
            case 3: ctrl->cancelPendingReadback(); break;
            case 4: delete ctrl.data(); break;
            case 5: ctrl->loadDefinitions(definitions); break;
            }
        });
        if (entry == 0) {
            QVERIFY(ctrl->applyModifiedParameters(&backend));
            ctrl->onReadbackValues(backend.readbackValues);
        } else if (entry == 1) {
            const bool success = ctrl->applyModifiedParametersWithReadback(&backend, 1, 0);
            QCOMPARE(success, action == 5 && expected == ParameterState::Confirmed);
        } else {
            QVERIFY(ctrl->applyModifiedParametersWithReadbackAsync(&backend, 1, 0));
            QTRY_VERIFY_WITH_TIMEOUT(acted, 1000);
        }
        QVERIFY(acted);
        QCOMPARE(terminalNotifications, action == 5 ? 2 : 1);
        QVERIFY(completions <= 1);
        const int completed = completions;
        QTest::qWait(30);
        QCOMPARE(terminalNotifications, action == 5 ? 2 : 1);
        QCOMPARE(completions, completed);
        if (action == 4) QVERIFY(ctrl.isNull());
        else if (action <= 1) QVERIFY(ctrl->parameterStates().isEmpty());
        else if (action == 2) {
            QCOMPARE(ctrl->parameterStates().size(), 1);
            QCOMPARE(ctrl->parameterState("New").state, ParameterState::Clean);
        }
        delete ctrl.data();
    }

    void lateReadbackCannotCompleteCancelledOrReplacementBatch()
    {
        class DeferredBackend : public VirtualDeviceBackend {
        public:
            QList<ReadCompletion> reads;
            void readPointsAsync(const QStringList&, int, std::shared_ptr<std::atomic_bool>,
                                 QObject*, ReadCompletion completion) override
            {
                reads.append(completion);
            }
        } backend;
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("A", true, "0", "param.a")});
        RuntimePointDefinition point;
        point.id = QStringLiteral("param.a"); point.name = QStringLiteral("A");
        point.kind = RuntimePointKind::Parameter; point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        QVERIFY(backend.connectBackend());
        QSignalSpy finished(&ctrl, &ParameterController::readbackFinished);
        QVERIFY(ctrl.editParameter("A", "2"));
        QVERIFY(ctrl.applyModifiedParametersWithReadbackAsync(&backend));
        QTRY_COMPARE_WITH_TIMEOUT(backend.reads.size(), 1, 1000);
        const auto oldRead = backend.reads.first();
        ctrl.cancelPendingReadback();
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(0).toBool(), false);
        QVERIFY(ctrl.editParameter("A", "3"));
        QVERIFY(ctrl.applyModifiedParametersWithReadbackAsync(&backend));
        QTRY_COMPARE_WITH_TIMEOUT(backend.reads.size(), 2, 1000);
        oldRead(true, {{QStringLiteral("param.a"), 99.0}}, QString(), {}, {});
        QCOMPARE(ctrl.parameterState("A").state, ParameterState::PendingReadback);
        QCOMPARE(finished.count(), 1);
        backend.reads.last()(true, {{QStringLiteral("param.a"), 3.0}}, QString(), {}, {});
        QCOMPARE(ctrl.parameterState("A").state, ParameterState::Confirmed);
        QCOMPARE(finished.count(), 2);
        QCOMPARE(finished.last().at(0).toBool(), true);
        oldRead(true, {{QStringLiteral("param.a"), 2.0}}, QString(), {}, {});
        QTest::qWait(30);
        QCOMPARE(finished.count(), 2);
        QCOMPARE(ctrl.parameterState("A").appliedValue, QStringLiteral("3"));
    }

    void inspectorRefreshPreservesReadback_data()
    {
        QTest::addColumn<bool>("async");
        QTest::newRow("sync") << false;
        QTest::newRow("async") << true;
    }

    void inspectorRefreshPreservesReadback()
    {
        QFETCH(bool, async);
        ParameterController ctrl;
        const QList<ParameterDefinition> definitions{makeParam("A", true, "0", "param.a"),
                                                      makeParam("B", true, "0", "param.b")};
        ctrl.loadDefinitions(definitions);
        VirtualDeviceBackend backend;
        QList<RuntimePointDefinition> points;
        for (const auto& def : definitions) {
            RuntimePointDefinition point;
            point.id = def.id; point.name = def.name;
            point.kind = RuntimePointKind::Parameter;
            point.dataType = QStringLiteral("REAL");
            point.access = RuntimePointAccess::ReadWrite;
            points.append(point);
        }
        backend.loadPointDefinitions(points);
        QVERIFY(backend.connectBackend());
        bool refreshing = false;
        connect(&ctrl, &ParameterController::statesChanged, &ctrl, [&] {
            if (refreshing) return;
            QScopedValueRollback<bool> guard(refreshing, true);
            ctrl.loadDefinitions(definitions);
        });
        QVERIFY(ctrl.editParameter("A", "2"));
        QVERIFY(ctrl.editParameter("B", "2"));
        QSignalSpy finished(&ctrl, &ParameterController::readbackFinished);
        if (async) {
            QVERIFY(ctrl.applyModifiedParametersWithReadbackAsync(&backend));
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);
            QCOMPARE(finished.first().at(0).toBool(), true);
        } else {
            QVERIFY(ctrl.applyModifiedParametersWithReadback(&backend));
        }
        QCOMPARE(ctrl.parameterState("A").state, ParameterState::Confirmed);
        QCOMPARE(ctrl.parameterState("B").state, ParameterState::Confirmed);
    }

    void totalDeadlineRejectsLateCompletion_data()
    {
        QTest::addColumn<bool>("stallWrite");
        QTest::newRow("write") << true;
        QTest::newRow("read") << false;
    }

    void totalDeadlineRejectsLateCompletion()
    {
        QFETCH(bool, stallWrite);
        class DeferredBackend : public VirtualDeviceBackend {
        public:
            bool stallWrite = false;
            WriteCompletion write;
            ReadCompletion read;
            int reads = 0;
            void writePointsAsync(const QHash<QString, QVariant>& values, int budget,
                                  std::shared_ptr<std::atomic_bool> cancelled,
                                  QObject* context, WriteCompletion completion) override
            {
                if (stallWrite) write = completion;
                else VirtualDeviceBackend::writePointsAsync(values, budget, cancelled, context, completion);
            }
            void readPointsAsync(const QStringList&, int, std::shared_ptr<std::atomic_bool>,
                                 QObject*, ReadCompletion completion) override
            {
                ++reads;
                read = completion;
            }
        } backend;
        backend.stallWrite = stallWrite;
        ParameterController ctrl;
        ctrl.loadDefinitions({makeParam("A", true, "0", "param.a")});
        RuntimePointDefinition point;
        point.id = QStringLiteral("param.a"); point.name = QStringLiteral("A");
        point.kind = RuntimePointKind::Parameter; point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        QVERIFY(backend.connectBackend());
        QVERIFY(ctrl.editParameter("A", "2"));
        QSignalSpy finished(&ctrl, &ParameterController::readbackFinished);
        QElapsedTimer elapsed;
        elapsed.start();
        QVERIFY(ctrl.applyModifiedParametersWithReadbackAsync(&backend, 2, 0));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 17000);
        QVERIFY(elapsed.elapsed() < 17000);
        QCOMPARE(finished.first().at(0).toBool(), false);
        QVERIFY(finished.first().at(1).toString().contains(QStringLiteral("总时限")));
        QCOMPARE(ctrl.parameterState("A").state, ParameterState::Timeout);
        if (stallWrite) {
            QVERIFY(bool(backend.write));
            backend.write(true, QString(), {});
            QCOMPARE(backend.reads, 0);
        } else {
            QVERIFY(bool(backend.read));
            backend.read(true, {{QStringLiteral("param.a"), 2.0}}, QString(), {}, {});
        }
        QTest::qWait(30);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(ctrl.parameterState("A").state, ParameterState::Timeout);
    }

    void asyncWriteKeepsEventLoopResponsiveAndCancellationPreventsLateWrite()
    {
        ParameterController controller;
        controller.loadDefinitions({makeParam("Kp", true, "0", "param.kp")});
        QVERIFY(controller.editParameter("Kp", "2.0"));
        VirtualDeviceBackend backend;
        RuntimePointDefinition point;
        point.id = QStringLiteral("param.kp"); point.name = QStringLiteral("Kp");
        point.kind = RuntimePointKind::Parameter; point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point});
        QVERIFY(backend.connectBackend());
        backend.setSimulatedLatencyMs(250);
        QSignalSpy finished(&controller, &ParameterController::readbackFinished);
        bool heartbeat = false;
        QTimer::singleShot(0, &controller, [&] { heartbeat = true; });
        QVERIFY(controller.applyModifiedParametersWithReadbackAsync(&backend, 2, 0));
        QCOMPARE(controller.parameterState("Kp").state, ParameterState::Applying);
        QTRY_VERIFY(heartbeat);
        QCOMPARE(finished.count(), 0);
        controller.cancelPendingReadback(QStringLiteral("cancel during write"));
        QCOMPARE(finished.count(), 1);
        QTest::qWait(300);
        QCOMPARE(finished.count(), 1);
        QVERIFY(controller.parameterState("Kp").appliedValue != QStringLiteral("2.0"));
    }

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

        ScriptedReadbackBackend backend;
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
        QVERIFY(ctrl.applyModifiedParametersWithReadbackAsync(&backend, 1, 0));
        QCOMPARE(finishedSpy.count(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 1000);
        QCOMPARE(finishedSpy.first().at(0).toBool(), false);
        QCOMPARE(backend.asyncReadCalls, 0);
        QCOMPARE(ctrl.parameterState("A").state, ParameterState::ApplyFailed);
        QCOMPARE(ctrl.parameterState("B").state, ParameterState::ApplyFailed);
        QTest::qWait(30);
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(backend.asyncReadCalls, 0);
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
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 1000);
        QCOMPARE(finishedSpy.first().at(0).toBool(), false);
        QCOMPARE(ctrl.parameterState("Kp").state, ParameterState::Mismatch);
        QTest::qWait(30);
        QCOMPARE(finishedSpy.count(), 1);
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

QTEST_GUILESS_MAIN(ParameterControllerTest)
#include "parameter_controller_test.moc"
