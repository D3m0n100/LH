/**
 * @file virtual_device_backend_test.cpp
 * @brief VirtualDeviceBackend 单元测试
 *
 * 测试内容：
 * - connect / disconnect 状态切换
 * - readPoints / writePoints 正常路径
 * - offline 状态拒绝操作
 * - ReadOnly 点拒绝写入
 * - fault injection 后返回预期错误
 * - queryStatus 返回正确信息
 */

#include <QtTest/QtTest>
#include "communication/VirtualDeviceBackend.h"

class VirtualDeviceBackendTest : public QObject
{
    Q_OBJECT

private:
    static RuntimePointDefinition makePoint(const QString& id, const QString& name,
                                            RuntimePointAccess access = RuntimePointAccess::ReadWrite)
    {
        RuntimePointDefinition def;
        def.id = id;
        def.name = name;
        def.kind = RuntimePointKind::Variable;
        def.dataType = QStringLiteral("REAL");
        def.access = access;
        def.defaultValue = 0.0;
        return def;
    }

private slots:
    void init() {}
    void cleanup() {}

    // ── 连接生命周期 ────────────────────────────────────

    void initialStateOffline()
    {
        VirtualDeviceBackend backend;
        QVERIFY(!backend.isOnline());
    }

    void connectSetsOnline()
    {
        VirtualDeviceBackend backend;
        QVERIFY(backend.connectBackend());
        QVERIFY(backend.isOnline());
    }

    void disconnectSetsOffline()
    {
        VirtualDeviceBackend backend;
        backend.connectBackend();
        backend.disconnectBackend();
        QVERIFY(!backend.isOnline());
    }

    void connectEmitsSignal()
    {
        VirtualDeviceBackend backend;
        QSignalSpy spy(&backend, &IDeviceBackend::connectionStateChanged);
        backend.connectBackend();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
    }

    void disconnectEmitsSignal()
    {
        VirtualDeviceBackend backend;
        backend.connectBackend();
        QSignalSpy spy(&backend, &IDeviceBackend::connectionStateChanged);
        backend.disconnectBackend();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), false);
    }

    // ── 点位读写 ────────────────────────────────────────

    void readReturnsDefaultValues()
    {
        VirtualDeviceBackend backend;
        QList<RuntimePointDefinition> defs;
        defs << makePoint("p1", "A") << makePoint("p2", "B");
        backend.loadPointDefinitions(defs);
        backend.connectBackend();

        QStringList ids = {"p1", "p2"};
        QHash<QString, QVariant> values;
        QVERIFY(backend.readPoints(ids, values, nullptr));
        QCOMPARE(values.size(), 2);
    }

    void writeUpdatesValue()
    {
        VirtualDeviceBackend backend;
        backend.loadPointDefinitions({makePoint("p1", "A")});
        backend.connectBackend();

        QHash<QString, QVariant> writes;
        writes["p1"] = 42.0;
        QVERIFY(backend.writePoints(writes, nullptr));

        QStringList ids = {"p1"};
        QHash<QString, QVariant> values;
        backend.readPoints(ids, values, nullptr);
        QCOMPARE(values["p1"].toDouble(), 42.0);
    }

    void writeEmitsPointsChanged()
    {
        VirtualDeviceBackend backend;
        backend.loadPointDefinitions({makePoint("p1", "A")});
        backend.connectBackend();

        QSignalSpy spy(&backend, &IDeviceBackend::pointsChanged);
        QHash<QString, QVariant> writes;
        writes["p1"] = 10.0;
        backend.writePoints(writes, nullptr);

        QCOMPARE(spy.count(), 1);
    }

    void readNonexistentPointFails()
    {
        VirtualDeviceBackend backend;
        backend.connectBackend();

        QStringList ids = {"no_such"};
        QHash<QString, QVariant> values;
        QString errorMsg;
        QVERIFY(!backend.readPoints(ids, values, &errorMsg));
        QVERIFY(!errorMsg.isEmpty());
    }

    void writeNonexistentPointFails()
    {
        VirtualDeviceBackend backend;
        backend.connectBackend();

        QHash<QString, QVariant> writes;
        writes["no_such"] = 1.0;
        QString errorMsg;
        QVERIFY(!backend.writePoints(writes, &errorMsg));
        QVERIFY(!errorMsg.isEmpty());
    }

    void writeReadOnlyPointFails()
    {
        VirtualDeviceBackend backend;
        backend.loadPointDefinitions({makePoint("p1", "A", RuntimePointAccess::ReadOnly)});
        backend.connectBackend();

        QHash<QString, QVariant> writes;
        writes["p1"] = 1.0;
        QString errorMsg;
        QVERIFY(!backend.writePoints(writes, &errorMsg));
        QVERIFY(errorMsg.contains("read-only"));
    }

    void partialReadReturnsPointErrors()
    {
        VirtualDeviceBackend backend;
        backend.loadPointDefinitions({makePoint("p1", "A"), makePoint("p2", "B")});
        backend.connectBackend();

        QStringList ids = {"p1", "missing"};
        QHash<QString, QVariant> values;
        QHash<QString, CommError> pointErrors;
        QString errorMsg;
        QVERIFY(!backend.readPoints(ids, values, &errorMsg, &pointErrors));
        QCOMPARE(values.size(), 1);
        QVERIFY(values.contains("p1"));
        QCOMPARE(pointErrors.size(), 1);
        QCOMPARE(pointErrors.value("missing").code, CommErrorCode::InvalidAddress);
        QVERIFY(!errorMsg.isEmpty());
        QCOMPARE(backend.lastError().code, CommErrorCode::InvalidAddress);
    }

    void partialWriteAppliesWritablePoints()
    {
        VirtualDeviceBackend backend;
        backend.loadPointDefinitions({
            makePoint("p1", "A"),
            makePoint("p2", "B", RuntimePointAccess::ReadOnly)
        });
        backend.connectBackend();

        QHash<QString, QVariant> writes;
        writes["p1"] = 42.0;
        writes["p2"] = 84.0;
        QHash<QString, CommError> pointErrors;
        QString errorMsg;
        QVERIFY(!backend.writePoints(writes, &errorMsg, &pointErrors));
        QCOMPARE(pointErrors.size(), 1);
        QCOMPARE(pointErrors.value("p2").code, CommErrorCode::PermissionDenied);
        QCOMPARE(backend.lastError().code, CommErrorCode::PermissionDenied);

        QStringList ids = {"p1", "p2"};
        QHash<QString, QVariant> values;
        QVERIFY(backend.readPoints(ids, values, nullptr));
        QCOMPARE(values.value("p1").toDouble(), 42.0);
        QCOMPARE(values.value("p2").toDouble(), 0.0);
    }

    // ── offline 拒绝操作 ────────────────────────────────

    void readWhileOfflineFails()
    {
        VirtualDeviceBackend backend;
        backend.loadPointDefinitions({makePoint("p1", "A")});
        // 不调用 connectBackend

        QStringList ids = {"p1"};
        QHash<QString, QVariant> values;
        QString errorMsg;
        QVERIFY(!backend.readPoints(ids, values, &errorMsg));
        QVERIFY(errorMsg.contains("offline"));
    }

    void writeWhileOfflineFails()
    {
        VirtualDeviceBackend backend;
        backend.loadPointDefinitions({makePoint("p1", "A")});

        QHash<QString, QVariant> writes;
        writes["p1"] = 1.0;
        QString errorMsg;
        QVERIFY(!backend.writePoints(writes, &errorMsg));
        QVERIFY(errorMsg.contains("offline"));
    }

    // ── 故障注入 ────────────────────────────────────────

    void readFaultInjection()
    {
        VirtualDeviceBackend backend;
        backend.loadPointDefinitions({makePoint("p1", "A")});
        backend.connectBackend();
        backend.setFaultInjection(true, false, false);

        QStringList ids = {"p1"};
        QHash<QString, QVariant> values;
        QString errorMsg;
        QVERIFY(!backend.readPoints(ids, values, &errorMsg));
        QVERIFY(errorMsg.contains("read fault"));
    }

    void writeFaultInjection()
    {
        VirtualDeviceBackend backend;
        backend.loadPointDefinitions({makePoint("p1", "A")});
        backend.connectBackend();
        backend.setFaultInjection(false, true, false);

        QHash<QString, QVariant> writes;
        writes["p1"] = 1.0;
        QString errorMsg;
        QVERIFY(!backend.writePoints(writes, &errorMsg));
        QVERIFY(errorMsg.contains("write fault"));
    }

    void downloadFaultInjection()
    {
        VirtualDeviceBackend backend;
        backend.connectBackend();
        backend.setFaultInjection(false, false, true);

        QString errorMsg;
        QVERIFY(!backend.downloadArtifact("test.code", {}, &errorMsg));
        QVERIFY(errorMsg.contains("download fault"));
    }

    // ── 下载 ────────────────────────────────────────────

    void downloadWhileOfflineFails()
    {
        VirtualDeviceBackend backend;
        QString errorMsg;
        QVERIFY(!backend.downloadArtifact("test.code", {}, &errorMsg));
        QVERIFY(errorMsg.contains("offline"));
    }

    void downloadSuccess()
    {
        VirtualDeviceBackend backend;
        backend.connectBackend();
        QVERIFY(backend.downloadArtifact("test.code", {}, nullptr));
    }

    void downloadFaultInjectionCanBeConsumedOnce()
    {
        VirtualDeviceBackend backend;
        backend.connectBackend();
        backend.setDownloadFaultInjection(1, CommErrorCode::ConnectionLost, QStringLiteral("one-shot"));

        QString errorMsg;
        CommError operationError;
        QVERIFY(!backend.downloadArtifact(QStringLiteral("test.code"), {}, &errorMsg, &operationError));
        QCOMPARE(operationError.code, CommErrorCode::ConnectionLost);
        QVERIFY(errorMsg.contains(QStringLiteral("one-shot")));

        errorMsg.clear();
        operationError = CommError();
        QVERIFY(backend.downloadArtifact(QStringLiteral("test.code"), {}, &errorMsg, &operationError));
        QCOMPARE(operationError.code, CommErrorCode::NoError);

        const auto snapshot = backend.statusSnapshot();
        QCOMPARE(snapshot.extras.value(QStringLiteral("downloadFaultInjectionRemaining")).toInt(), 0);
    }

    // ── 状态查询 ────────────────────────────────────────

    void statusSnapshotContainsBackend()
    {
        VirtualDeviceBackend backend;
        const auto snapshot = backend.statusSnapshot();
        QCOMPARE(snapshot.backendType, QStringLiteral("virtual"));
        QCOMPARE(snapshot.online, false);
    }

    void statusSnapshotReflectsOnline()
    {
        VirtualDeviceBackend backend;
        backend.connectBackend();
        const auto snapshot = backend.statusSnapshot();
        QCOMPARE(snapshot.online, true);
    }

    void statusSnapshotReflectsPointCount()
    {
        VirtualDeviceBackend backend;
        backend.loadPointDefinitions({makePoint("p1", "A"), makePoint("p2", "B")});
        const auto snapshot = backend.statusSnapshot();
        QCOMPARE(snapshot.extras.value("pointCount").toInt(), 2);
    }

    void statusSnapshotExposesLastErrorFields()
    {
        VirtualDeviceBackend backend;
        backend.connectBackend();
        backend.setFaultInjection(true, false, false);

        QStringList ids = {"p1"};
        QHash<QString, QVariant> values;
        QString errorMsg;
        QVERIFY(!backend.readPoints(ids, values, &errorMsg));

        const auto snapshot = backend.statusSnapshot();
        QCOMPARE(snapshot.lastErrorCode, CommErrorCode::InternalError);
        QVERIFY(!snapshot.lastErrorMessage.isEmpty());
        QVERIFY(snapshot.timestamp.isValid());
        QCOMPARE(snapshot.partialSuccess, false);
        QCOMPARE(snapshot.extras.value("lastErrorCode").toInt(), static_cast<int>(CommErrorCode::InternalError));
    }
};

QTEST_MAIN(VirtualDeviceBackendTest)
#include "virtual_device_backend_test.moc"
