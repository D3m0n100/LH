/**
 * @file controller_device_backend_test.cpp
 * @brief ControllerDeviceBackend unit tests.
 */

#include <QtTest/QtTest>
#include <QTemporaryFile>

#include "communication/Communication.h"
#include "communication/ControllerDeviceBackend.h"

class FakeControllerTransport : public IControllerDebugTransport
{
public:
    struct WriteCall {
        int station = -1;
        int address = -1;
        QVector<quint16> values;
    };

    bool isConnected() const override { return connected; }
    int stationAddress() const override { return station; }
    void setStationAddress(int deviceId) override { station = deviceId; }

    bool readHoldingRegisters(int address, int count) override
    {
        lastReadAddress = address;
        lastReadCount = count;
        lastReadStation = station;
        if (!connected || failRead || address == failReadAddress) {
            error = CommError(CommProtocolType::ModbusRTU,
                              CommErrorCode::ReceiveFailed,
                              QStringLiteral("fake read failed"),
                              QStringLiteral("fake read details"));
            return false;
        }
        return registers.value(station).contains(address);
    }

    QVector<quint16> holdingRegisterValues(int address) const override
    {
        return registers.value(station).value(address);
    }

    bool writeMultipleRegisters(int address, const QVector<quint16>& values) override
    {
        lastWriteAddress = address;
        lastWriteValues = values;
        lastWriteStation = station;
        writes.append({station, address, values});
        if (!connected || failWrite || address == failWriteAddress) {
            error = CommError(CommProtocolType::ModbusRTU,
                              CommErrorCode::SendFailed,
                              QStringLiteral("fake write failed"),
                              QStringLiteral("fake write details"));
            return false;
        }
        registers[station].insert(address, values);
        return true;
    }

    CommError lastError() const override { return error; }

    bool connected = true;
    bool failRead = false;
    bool failWrite = false;
    int failReadAddress = -1;
    int failWriteAddress = -1;
    int station = 1;
    mutable CommError error;
    QHash<int, QHash<int, QVector<quint16>>> registers {
        {1, {
            {10, {1}},
            {23, {0}},
            {26, {2}},
            {27, {3}},
            {38, {105}},
            {100, {55}}
        }},
        {2, {
            {38, {2812}},
            {120, {77}}
        }}
    };
    int lastReadAddress = -1;
    int lastReadCount = 0;
    int lastReadStation = -1;
    int lastWriteAddress = -1;
    int lastWriteStation = -1;
    QVector<quint16> lastWriteValues;
    QVector<WriteCall> writes;
};

class ControllerDeviceBackendTest : public QObject
{
    Q_OBJECT

private slots:
    void rtuPortClaimRejectsWrongOwnerRelease()
    {
        const QString port = QStringLiteral("LH-claim-isolation");
        Communication::releaseRtuPort(port, QStringLiteral("owner-a"));
        QVERIFY(Communication::tryClaimRtuPort(port, QStringLiteral("owner-a")));
        QCOMPARE(Communication::currentRtuPortOwner(port), QStringLiteral("owner-a"));

        QString currentOwner;
        QVERIFY(!Communication::tryClaimRtuPort(port, QStringLiteral("owner-b"), &currentOwner));
        QCOMPARE(currentOwner, QStringLiteral("owner-a"));
        Communication::releaseRtuPort(port, QStringLiteral("owner-b"));
        QCOMPARE(Communication::currentRtuPortOwner(port), QStringLiteral("owner-a"));

        Communication::releaseRtuPort(port, QStringLiteral("owner-a"));
        QVERIFY(Communication::currentRtuPortOwner(port).isEmpty());
    }

    void rtuPortNormalizationPreservesUnixCase()
    {
        const QString port = QStringLiteral("LH/Port-A");
#ifdef Q_OS_WIN
        QCOMPARE(Communication::normalizedRtuPortName(port), port.toLower());
#else
        QCOMPARE(Communication::normalizedRtuPortName(port), port);
#endif
    }

    void backendConnectFailsFastWhenDiagnosticOwnsPort()
    {
        const QString port = QStringLiteral("LH-backend-busy");
        QVERIFY(Communication::tryClaimRtuPort(port, QStringLiteral("diagnostic-worker")));

        FakeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), port);
        QVERIFY(backend.configure(cfg));
        QVERIFY(!backend.connectBackend());
        QCOMPARE(backend.lastError().code, CommErrorCode::DeviceBusy);
        QVERIFY(backend.lastError().details.contains(QStringLiteral("diagnostic-worker")));

        Communication::releaseRtuPort(port, QStringLiteral("diagnostic-worker"));
        QVERIFY(backend.connectBackend());
    }

    void backendOperationBusyFailsFastAndReleases()
    {
        FakeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("LH-operation-busy"));
        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.tryBeginOperation());

        QHash<QString, QVariant> values;
        QString errorMessage;
        QHash<QString, CommError> pointErrors;
        QVERIFY(!backend.readPoints({QStringLiteral("controller.version")},
                                    values,
                                    &errorMessage,
                                    &pointErrors));
        QCOMPARE(backend.lastError().code, CommErrorCode::DeviceBusy);
        QCOMPARE(pointErrors.value(QStringLiteral("controller.version")).code,
                 CommErrorCode::DeviceBusy);
        QVERIFY(errorMessage.contains(QStringLiteral("忙")));

        backend.endOperation();
        QVERIFY(backend.connectBackend());
        QVERIFY(backend.readPoints({QStringLiteral("controller.version")}, values));
        QCOMPARE(values.value(QStringLiteral("controller.version")).toInt(), 105);
    }

    void configureRejectsMissingPort()
    {
        ControllerDeviceBackend backend;
        ProjectRuntimeConfig cfg;

        QString errorMessage;
        QVERIFY(!backend.configure(cfg, &errorMessage));
        QVERIFY(errorMessage.contains(QStringLiteral("串口")));
    }

    void errorSignalCanReenterStatusSnapshot()
    {
        ControllerDeviceBackend backend;
        bool reentered = false;
        connect(&backend, qOverload<const CommError&>(&IDeviceBackend::errorOccurred),
                &backend, [&backend, &reentered](const CommError&) {
                    reentered = !backend.statusSnapshot().backendType.isEmpty();
                });
        ProjectRuntimeConfig cfg;
        QString errorMessage;
        QVERIFY(!backend.configure(cfg, &errorMessage));
        QVERIFY(reentered);
    }

    void configureRejectsTcpBeforeSerialValidation()
    {
        ControllerDeviceBackend backend;
        ProjectRuntimeConfig cfg;
        cfg.transport.protocol = QStringLiteral("modbus");
        cfg.transport.mode = QStringLiteral("tcp");
        cfg.transport.parameters.insert(QStringLiteral("host"), QStringLiteral("127.0.0.1"));
        cfg.transport.parameters.insert(QStringLiteral("tcpPort"), 502);

        QString errorMessage;
        QVERIFY(!backend.configure(cfg, &errorMessage));
        QVERIFY(errorMessage.contains(QStringLiteral("Modbus TCP")));
        QCOMPARE(backend.lastError().code, CommErrorCode::InvalidConfig);
    }

    void preflightReportsSerialAndPointMapping()
    {
        ControllerDeviceBackend backend;
        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        cfg.transport.parameters.insert(QStringLiteral("baudRate"), 115200);
        cfg.transport.parameters.insert(QStringLiteral("parity"), QStringLiteral("None"));

        HardwareResourceBinding point;
        point.id = QStringLiteral("target.reg");
        point.resourceName = QStringLiteral("Target Register");
        point.resourceType = QStringLiteral("holding");
        point.channel = QStringLiteral("target.reg");
        point.metadata.insert(QStringLiteral("address"), 120);
        point.metadata.insert(QStringLiteral("unitId"), 2);
        cfg.resources.append(point);

        QVERIFY(backend.configure(cfg));
        QVariantMap report;
        QVERIFY(backend.preflight(nullptr, &report));
        QCOMPARE(report.value(QStringLiteral("valid")).toBool(), true);
        QCOMPARE(report.value(QStringLiteral("serial")).toMap().value(QStringLiteral("port")).toString(),
                 QStringLiteral("COM7"));
        QCOMPARE(report.value(QStringLiteral("pointMappings")).toMap().value(QStringLiteral("mappedHoldingPoints")).toInt(),
                 1);
    }

    void preflightWarnsWhenOpcSerialDiffersFromControllerSerial()
    {
        ControllerDeviceBackend backend;
        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        cfg.transport.parameters.insert(QStringLiteral("baudRate"), 115200);
        cfg.transport.parameters.insert(QStringLiteral("parity"), QStringLiteral("None"));
        cfg.opcServer.enabled = true;
        cfg.opcServer.deviceName = QStringLiteral("COM8");
        cfg.opcServer.serialMode = QStringLiteral("19200,N,8,1");
        cfg.opcServer.opcProgId = QStringLiteral("Matrikon.OPC.Modbus");

        QVERIFY(backend.configure(cfg));
        QVariantMap report;
        QVERIFY(backend.preflight(nullptr, &report));

        const QStringList warnings = report.value(QStringLiteral("warnings")).toStringList();
        const QString joined = warnings.join(QStringLiteral("; "));
        QVERIFY(joined.contains(QStringLiteral("OPC 设备端口")));
        QVERIFY(joined.contains(QStringLiteral("OPC 波特率")));
        QCOMPARE(report.value(QStringLiteral("opc")).toMap().value(QStringLiteral("serialModeValid")).toBool(), true);
    }

    void preflightReportsUnmappedPointReasons()
    {
        ControllerDeviceBackend backend;
        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));

        HardwareResourceBinding inputPoint;
        inputPoint.id = QStringLiteral("target.input");
        inputPoint.resourceName = QStringLiteral("Target Input");
        inputPoint.resourceType = QStringLiteral("input");
        inputPoint.channel = QStringLiteral("target.input");
        inputPoint.metadata.insert(QStringLiteral("protocol"), QStringLiteral("can"));
        inputPoint.metadata.insert(QStringLiteral("area"), QStringLiteral("holding"));
        inputPoint.metadata.insert(QStringLiteral("address"), 20);
        cfg.resources.append(inputPoint);

        HardwareResourceBinding invalidCount;
        invalidCount.id = QStringLiteral("target.invalidCount");
        invalidCount.resourceName = QStringLiteral("Invalid Count");
        invalidCount.resourceType = QStringLiteral("holding");
        invalidCount.channel = QStringLiteral("target.invalidCount");
        invalidCount.metadata.insert(QStringLiteral("area"), QStringLiteral("holding"));
        invalidCount.metadata.insert(QStringLiteral("address"), 30);
        invalidCount.metadata.insert(QStringLiteral("count"), 200);
        cfg.resources.append(invalidCount);

        QVERIFY(backend.configure(cfg));
        QVariantMap report;
        QVERIFY(backend.preflight(nullptr, &report));

        const QVariantMap mappings = report.value(QStringLiteral("pointMappings")).toMap();
        QVERIFY(mappings.value(QStringLiteral("unmappedPoints")).toInt() >= 1);
        const QVariantList details = mappings.value(QStringLiteral("unmappedPointDetails")).toList();
        QVERIFY(!details.isEmpty());
        QStringList reasons;
        for (const QVariant& detail : details) {
            reasons << detail.toMap().value(QStringLiteral("reason")).toString();
        }
        const QString joinedReasons = reasons.join(QStringLiteral("; "));
        QVERIFY(joinedReasons.contains(QStringLiteral("协议"))
                || joinedReasons.contains(QStringLiteral("数量")));
    }

    void statusSnapshotKeepsPreflightDetailsOutOfHotPath()
    {
        ControllerDeviceBackend backend;
        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        cfg.opcServer.enabled = true;
        cfg.opcServer.opcProgId = QStringLiteral("Matrikon.OPC.Modbus");
        cfg.opcServer.serialMode = QStringLiteral("19200,N,8,1");
        QVERIFY(backend.configure(cfg));

        const BackendStatusSnapshot snapshot = backend.statusSnapshot();
        QVERIFY(!snapshot.extras.contains(QStringLiteral("preflight")));
        const QVariantMap pointSummary = snapshot.extras.value(
                QStringLiteral("pointMappings")).toMap();
        QVERIFY(pointSummary.contains(QStringLiteral("mappedHoldingPoints")));
        QVERIFY(!pointSummary.contains(QStringLiteral("points")));
        QVERIFY(!pointSummary.contains(QStringLiteral("unmappedPointDetails")));

        QVariantMap report;
        QVERIFY(backend.preflight(nullptr, &report));
        QVERIFY(report.value(QStringLiteral("pointMappings")).toMap().contains(
                QStringLiteral("unmappedPointDetails")));
    }

    void connectReadsControllerStatus()
    {
        FakeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());

        const BackendStatusSnapshot snapshot = backend.statusSnapshot();
        QVERIFY(snapshot.online);
        QCOMPARE(snapshot.backendType, QStringLiteral("controller"));
        QCOMPARE(snapshot.extras.value(QStringLiteral("version")).toInt(), 105);
    }

    void debugCommandsWriteExpectedRegisters()
    {
        FakeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());

        QVERIFY(backend.pause());
        QCOMPARE(transport.lastWriteAddress, 32);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({1}));

        QVERIFY(backend.resume());
        QCOMPARE(transport.lastWriteAddress, 32);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0}));

        QVERIFY(backend.step());
        QCOMPARE(transport.lastWriteAddress, 31);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({1}));

        QVERIFY(backend.runToCursor(88));
        QCOMPARE(transport.lastWriteAddress, 28);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({88}));
    }

    void readAndWritePointFacadeUsesDebugClient()
    {
        FakeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());

        QHash<QString, QVariant> values;
        QVERIFY(backend.readPoints({QStringLiteral("controller.version")}, values));
        QCOMPARE(values.value(QStringLiteral("controller.version")).toInt(), 105);

        QHash<QString, QVariant> writes;
        writes.insert(QStringLiteral("controller.runToCursor"), 12);
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteAddress, 28);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({12}));
    }

    void mappedHoldingRegisterPointsUseProjectAddressing()
    {
        FakeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        HardwareResourceBinding point;
        point.id = QStringLiteral("target.reg");
        point.resourceName = QStringLiteral("Target Register");
        point.resourceType = QStringLiteral("holding");
        point.channel = QStringLiteral("target.reg");
        point.metadata.insert(QStringLiteral("address"), 120);
        point.metadata.insert(QStringLiteral("unitId"), 2);
        point.metadata.insert(QStringLiteral("elementCount"), 1);
        cfg.resources.append(point);
        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());

        QHash<QString, QVariant> values;
        QVERIFY(backend.readPoints({QStringLiteral("target.reg")}, values));
        QCOMPARE(values.value(QStringLiteral("target.reg")).toInt(), 77);
        QCOMPARE(transport.lastReadStation, 2);
        QCOMPARE(transport.lastReadAddress, 120);

        QHash<QString, QVariant> writes;
        writes.insert(QStringLiteral("target.reg"), 88);
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteStation, 2);
        QCOMPARE(transport.lastWriteAddress, 120);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({88}));
    }

    void metadataDeviceAliasesOverrideNormalizedAddressing()
    {
        FakeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        const auto addPoint = [&cfg](const QString& id,
                                     int address,
                                     const QVariantMap& aliases) {
            HardwareResourceBinding point;
            point.id = id;
            point.resourceName = id;
            point.resourceType = QStringLiteral("holding");
            point.channel = id;
            point.metadata.insert(QStringLiteral("address"), address);
            for (auto it = aliases.constBegin(); it != aliases.constEnd(); ++it) {
                point.metadata.insert(it.key(), it.value());
            }
            cfg.resources.append(point);
        };
        addPoint(QStringLiteral("alias.single"), 120,
                 {{QStringLiteral("slaveId"), 2}});
        addPoint(QStringLiteral("alias.same"), 38,
                 {{QStringLiteral("unitId"), 2},
                  {QStringLiteral("stationAddress"), 2},
                  {QStringLiteral("serverAddress"), 2}});
        addPoint(QStringLiteral("alias.conflict"), 100,
                 {{QStringLiteral("unitId"), 1},
                  {QStringLiteral("slaveId"), 2}});
        addPoint(QStringLiteral("alias.invalidString"), 10,
                 {{QStringLiteral("slaveId"), QStringLiteral("invalid")}});
        addPoint(QStringLiteral("alias.nonInteger"), 23,
                 {{QStringLiteral("slaveId"), 2.5}});
        addPoint(QStringLiteral("alias.outOfRange"), 26,
                 {{QStringLiteral("slaveId"), 64}});

        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());

        QHash<QString, QVariant> values;
        QVERIFY(backend.readPoints({QStringLiteral("alias.single")}, values));
        QCOMPARE(values.value(QStringLiteral("alias.single")).toInt(), 77);
        QCOMPARE(transport.lastReadStation, 2);

        values.clear();
        QVERIFY(backend.readPoints({QStringLiteral("alias.same")}, values));
        QCOMPARE(values.value(QStringLiteral("alias.same")).toInt(), 2812);
        QCOMPARE(transport.lastReadStation, 2);

        const auto expectInvalidConfig = [&backend](const QString& pointId) {
            QHash<QString, QVariant> rejectedValues;
            QHash<QString, CommError> pointErrors;
            QVERIFY(!backend.readPoints({pointId}, rejectedValues, nullptr, &pointErrors));
            QCOMPARE(pointErrors.value(pointId).code, CommErrorCode::InvalidConfig);
            QVERIFY(!rejectedValues.contains(pointId));
        };
        expectInvalidConfig(QStringLiteral("alias.conflict"));
        expectInvalidConfig(QStringLiteral("alias.invalidString"));
        expectInvalidConfig(QStringLiteral("alias.nonInteger"));
        expectInvalidConfig(QStringLiteral("alias.outOfRange"));
    }

    void mappedTypedHoldingRegisterPointsUseCodecContract()
    {
        FakeControllerTransport transport;
        transport.registers[1].insert(140, {0x4000, 0x0000});
        transport.registers[1].insert(142, {0xffff});
        transport.registers[1].insert(144, {0x0000, 0x3f80});
        transport.registers[1].insert(146, {1, 2});
        transport.registers[1].insert(158, {5, 6});
        transport.registers[1].insert(156, {3});
        transport.registers[1].insert(154, {0x7fc0, 0x0000});
        transport.registers[1].insert(162, {1});
        transport.registers[1].insert(164, {0x1234, 0x5678});
        transport.registers[1].insert(166, {0xffff, 0xfffe});
        transport.registers[1].insert(174, {0x0000, 0x803f});
        transport.registers[1].insert(176, {0x3f80, 0x0000, 0x4000, 0x0000});
        transport.registers[1].insert(180, {0xffff});
        transport.registers[1].insert(182, {0xffff, 0xfffe});
        transport.registers[1].insert(186, {11});
        transport.registers[1].insert(187, {12});
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        const auto addPoint = [&cfg](const QString& id,
                                     int address,
                                     const QString& dataType,
                                     int elementCount = 1,
                                     const QString& byteOrder = QStringLiteral("BigEndian"),
                                     const QString& wordOrder = QStringLiteral("BigEndian")) {
            HardwareResourceBinding point;
            point.id = id;
            point.resourceName = id;
            point.resourceType = QStringLiteral("holding");
            point.channel = id;
            point.metadata.insert(QStringLiteral("address"), address);
            point.metadata.insert(QStringLiteral("dataType"), dataType);
            point.metadata.insert(QStringLiteral("elementCount"), elementCount);
            point.metadata.insert(QStringLiteral("byteOrder"), byteOrder);
            point.metadata.insert(QStringLiteral("wordOrder"), wordOrder);
            cfg.resources.append(point);
        };
        addPoint(QStringLiteral("typed.real"), 140, QStringLiteral("REAL"));
        addPoint(QStringLiteral("typed.i16"), 142, QStringLiteral("INT16"));
        addPoint(QStringLiteral("typed.f32LittleWord"), 144, QStringLiteral("FLOAT32"), 1,
                 QStringLiteral("BigEndian"), QStringLiteral("LittleEndian"));
        addPoint(QStringLiteral("typed.u16Array"), 146, QStringLiteral("UINT16"), 2);
        addPoint(QStringLiteral("typed.u16LittleByte"), 148, QStringLiteral("UINT16"), 1,
                 QStringLiteral("LittleEndian"));
        addPoint(QStringLiteral("typed.bool"), 162, QStringLiteral("BOOL"));
        addPoint(QStringLiteral("typed.u32"), 164, QStringLiteral("UINT32"));
        addPoint(QStringLiteral("typed.i32"), 166, QStringLiteral("INT32"));
        addPoint(QStringLiteral("typed.realLittleBoth"), 174, QStringLiteral("REAL"), 1,
                 QStringLiteral("LittleEndian"), QStringLiteral("LittleEndian"));
        addPoint(QStringLiteral("typed.realArray"), 176, QStringLiteral("REAL"), 2);
        addPoint(QStringLiteral("typed.intAlias"), 180, QStringLiteral("INT"));
        addPoint(QStringLiteral("typed.dintAlias"), 182, QStringLiteral("DINT"));
        HardwareResourceBinding badCount;
        badCount.id = QStringLiteral("typed.badCount");
        badCount.resourceName = badCount.id;
        badCount.resourceType = QStringLiteral("holding");
        badCount.channel = badCount.id;
        badCount.metadata.insert(QStringLiteral("address"), 152);
        badCount.metadata.insert(QStringLiteral("dataType"), QStringLiteral("REAL"));
        badCount.metadata.insert(QStringLiteral("count"), 1);
        cfg.resources.append(badCount);
        HardwareResourceBinding scaled;
        scaled.id = QStringLiteral("typed.scaled");
        scaled.resourceName = scaled.id;
        scaled.resourceType = QStringLiteral("holding");
        scaled.channel = scaled.id;
        scaled.metadata.insert(QStringLiteral("address"), 156);
        scaled.metadata.insert(QStringLiteral("dataType"), QStringLiteral("UINT16"));
        scaled.metadata.insert(QStringLiteral("scale"), 2.0);
        scaled.metadata.insert(QStringLiteral("offset"), 1.0);
        cfg.resources.append(scaled);
        HardwareResourceBinding legacy;
        legacy.id = QStringLiteral("legacy.rawArray");
        legacy.resourceName = legacy.id;
        legacy.resourceType = QStringLiteral("holding");
        legacy.channel = legacy.id;
        legacy.metadata.insert(QStringLiteral("address"), 158);
        legacy.metadata.insert(QStringLiteral("elementCount"), 2);
        cfg.resources.append(legacy);
        HardwareResourceBinding badReal;
        badReal.id = QStringLiteral("typed.badReal");
        badReal.resourceName = badReal.id;
        badReal.resourceType = QStringLiteral("holding");
        badReal.channel = badReal.id;
        badReal.metadata.insert(QStringLiteral("address"), 154);
        badReal.metadata.insert(QStringLiteral("dataType"), QStringLiteral("REAL"));
        cfg.resources.append(badReal);
        HardwareResourceBinding badType;
        badType.id = QStringLiteral("typed.badType");
        badType.resourceName = badType.id;
        badType.resourceType = QStringLiteral("holding");
        badType.channel = badType.id;
        badType.metadata.insert(QStringLiteral("address"), 160);
        badType.metadata.insert(QStringLiteral("dataType"), QStringLiteral("STRING"));
        cfg.resources.append(badType);
        HardwareResourceBinding badFraction;
        badFraction.id = QStringLiteral("legacy.badFraction");
        badFraction.resourceName = badFraction.id;
        badFraction.resourceType = QStringLiteral("holding");
        badFraction.channel = badFraction.id;
        badFraction.metadata.insert(QStringLiteral("address"), 172);
        badFraction.metadata.insert(QStringLiteral("count"), 1.5);
        cfg.resources.append(badFraction);
        HardwareResourceBinding badScale;
        badScale.id = QStringLiteral("typed.badScale");
        badScale.resourceName = badScale.id;
        badScale.resourceType = QStringLiteral("holding");
        badScale.channel = badScale.id;
        badScale.metadata.insert(QStringLiteral("address"), 184);
        badScale.metadata.insert(QStringLiteral("dataType"), QStringLiteral("REAL"));
        badScale.metadata.insert(QStringLiteral("scale"), 0.0);
        cfg.resources.append(badScale);
        HardwareResourceBinding writeOnly;
        writeOnly.id = QStringLiteral("typed.writeOnly");
        writeOnly.resourceName = writeOnly.id;
        writeOnly.resourceType = QStringLiteral("holding");
        writeOnly.channel = writeOnly.id;
        writeOnly.metadata.insert(QStringLiteral("address"), 186);
        writeOnly.metadata.insert(QStringLiteral("tagAccess"), QStringLiteral("WriteOnly"));
        cfg.resources.append(writeOnly);
        HardwareResourceBinding readOnly;
        readOnly.id = QStringLiteral("typed.readOnly");
        readOnly.resourceName = readOnly.id;
        readOnly.resourceType = QStringLiteral("holding");
        readOnly.channel = readOnly.id;
        readOnly.metadata.insert(QStringLiteral("address"), 187);
        readOnly.metadata.insert(QStringLiteral("tagAccess"), QStringLiteral("ReadOnly"));
        cfg.resources.append(readOnly);
        HardwareResourceBinding offsetOnly;
        offsetOnly.id = QStringLiteral("typed.offsetOnly");
        offsetOnly.resourceName = offsetOnly.id;
        offsetOnly.resourceType = QStringLiteral("holding");
        offsetOnly.channel = offsetOnly.id;
        offsetOnly.metadata.insert(QStringLiteral("offset"), 188);
        cfg.resources.append(offsetOnly);

        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());

        QHash<QString, QVariant> values;
        QVERIFY(backend.readPoints({QStringLiteral("typed.real")}, values));
        QVERIFY(qFuzzyCompare(values.value(QStringLiteral("typed.real")).toFloat(), 2.0f));
        values.clear();
        QVERIFY(backend.readPoints({QStringLiteral("typed.f32LittleWord")}, values));
        QVERIFY(qFuzzyCompare(values.value(QStringLiteral("typed.f32LittleWord")).toFloat(), 1.0f));
        values.clear();
        QVERIFY(backend.readPoints({QStringLiteral("typed.u16Array")}, values));
        const QVariantList arrayValues = values.value(QStringLiteral("typed.u16Array")).toList();
        QCOMPARE(arrayValues.size(), 2);
        QCOMPARE(arrayValues.at(0).toInt(), 1);
        QCOMPARE(arrayValues.at(1).toInt(), 2);
        values.clear();
        QVERIFY(backend.readPoints({QStringLiteral("typed.scaled")}, values));
        QCOMPARE(values.value(QStringLiteral("typed.scaled")).toDouble(), 7.0);
        values.clear();
        QVERIFY(backend.readPoints({QStringLiteral("legacy.rawArray")}, values));
        const QVariantList legacyValues = values.value(QStringLiteral("legacy.rawArray")).toList();
        QCOMPARE(legacyValues.size(), 2);
        QCOMPARE(legacyValues.at(0).toInt(), 5);
        QCOMPARE(legacyValues.at(1).toInt(), 6);
        values.clear();
        QVERIFY(backend.readPoints({QStringLiteral("typed.bool")}, values));
        QVERIFY(values.value(QStringLiteral("typed.bool")).toBool());
        values.clear();
        QVERIFY(backend.readPoints({QStringLiteral("typed.u32")}, values));
        QCOMPARE(values.value(QStringLiteral("typed.u32")).toULongLong(), 0x12345678ULL);
        values.clear();
        QVERIFY(backend.readPoints({QStringLiteral("typed.i32")}, values));
        QCOMPARE(values.value(QStringLiteral("typed.i32")).toLongLong(), -2LL);
        values.clear();
        QVERIFY(backend.readPoints({QStringLiteral("typed.realLittleBoth")}, values));
        QVERIFY(qFuzzyCompare(values.value(QStringLiteral("typed.realLittleBoth")).toFloat(), 1.0f));
        QCOMPARE(transport.lastReadCount, 2);
        values.clear();
        QVERIFY(backend.readPoints({QStringLiteral("typed.realArray")}, values));
        const QVariantList realArrayValues = values.value(QStringLiteral("typed.realArray")).toList();
        QCOMPARE(realArrayValues.size(), 2);
        QVERIFY(qFuzzyCompare(realArrayValues.at(0).toFloat(), 1.0f));
        QVERIFY(qFuzzyCompare(realArrayValues.at(1).toFloat(), 2.0f));
        QCOMPARE(transport.lastReadCount, 4);
        values.clear();
        QVERIFY(backend.readPoints({QStringLiteral("typed.intAlias")}, values));
        QCOMPARE(values.value(QStringLiteral("typed.intAlias")).toInt(), -1);
        values.clear();
        QVERIFY(backend.readPoints({QStringLiteral("typed.dintAlias")}, values));
        QCOMPARE(values.value(QStringLiteral("typed.dintAlias")).toLongLong(), -2LL);

        QHash<QString, QVariant> writes;
        QHash<QString, CommError> pointErrors;
        writes.insert(QStringLiteral("typed.real"), QStringLiteral("2.0"));
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0x4000, 0x0000}));
        values.clear();
        QVERIFY(backend.readPoints({QStringLiteral("typed.real")}, values));
        QVERIFY(qFuzzyCompare(values.value(QStringLiteral("typed.real")).toFloat(), 2.0f));

        writes.clear();
        writes.insert(QStringLiteral("typed.f32LittleWord"), 1.0);
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0x0000, 0x3f80}));

        writes.clear();
        writes.insert(QStringLiteral("typed.bool"), true);
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({1}));

        writes.clear();
        writes.insert(QStringLiteral("typed.u32"), 0x12345678);
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0x1234, 0x5678}));

        writes.clear();
        writes.insert(QStringLiteral("typed.i32"), -2);
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0xffff, 0xfffe}));

        writes.clear();
        writes.insert(QStringLiteral("typed.i16"), QStringLiteral("-1"));
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0xffff}));

        writes.clear();
        writes.insert(QStringLiteral("typed.intAlias"), QStringLiteral("-1"));
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0xffff}));

        writes.clear();
        writes.insert(QStringLiteral("typed.dintAlias"), QStringLiteral("-2"));
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0xffff, 0xfffe}));

        writes.clear();
        writes.insert(QStringLiteral("typed.u16LittleByte"), 65535);
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0xffff}));

        writes.clear();
        writes.insert(QStringLiteral("typed.u16LittleByte"), 0x1234);
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0x3412}));

        writes.clear();
        writes.insert(QStringLiteral("typed.u16Array"), QVariantList({1, 2}));
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({1, 2}));

        writes.clear();
        writes.insert(QStringLiteral("typed.scaled"), 7.0);
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({3}));

        writes.clear();
        writes.insert(QStringLiteral("typed.scaled"), 8.0);
        QVERIFY(!backend.writePoints(writes, nullptr, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.scaled")).code,
                 CommErrorCode::InvalidParameter);

        writes.clear();
        writes.insert(QStringLiteral("legacy.rawArray"), QVariantList({7, 8}));
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({7, 8}));

        writes.clear();
        writes.insert(QStringLiteral("legacy.rawArray"), QVariantList({7}));
        QVERIFY(!backend.writePoints(writes, nullptr, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("legacy.rawArray")).code,
                 CommErrorCode::InvalidParameter);

        writes.clear();
        writes.insert(QStringLiteral("typed.u16Array"), QVariantList({1}));
        QVERIFY(!backend.writePoints(writes, nullptr, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.u16Array")).code,
                 CommErrorCode::InvalidParameter);

        writes.clear();
        writes.insert(QStringLiteral("typed.i16"), QStringLiteral("abc"));
        QVERIFY(!backend.writePoints(writes, nullptr, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.i16")).code,
                 CommErrorCode::InvalidParameter);
        QVERIFY(!pointErrors.value(QStringLiteral("typed.i16")).details.isEmpty());
        QVERIFY(!pointErrors.value(QStringLiteral("typed.i16")).details.contains(QStringLiteral("拒绝字符串")));

        writes.clear();
        writes.insert(QStringLiteral("typed.i16"), QStringLiteral("2.0abc"));
        QVERIFY(!backend.writePoints(writes, nullptr, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.i16")).code,
                 CommErrorCode::InvalidParameter);

        writes.clear();
        writes.insert(QStringLiteral("typed.i16"), QStringLiteral("   "));
        QVERIFY(!backend.writePoints(writes, nullptr, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.i16")).code,
                 CommErrorCode::InvalidParameter);

        writes.clear();
        writes.insert(QStringLiteral("typed.real"), QStringLiteral("nan"));
        QVERIFY(!backend.writePoints(writes, nullptr, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.real")).code,
                 CommErrorCode::InvalidParameter);

        writes.clear();
        writes.insert(QStringLiteral("typed.real"), QStringLiteral("inf"));
        QVERIFY(!backend.writePoints(writes, nullptr, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.real")).code,
                 CommErrorCode::InvalidParameter);

        writes.clear();
        writes.insert(QStringLiteral("typed.u16LittleByte"), 65536);
        QVERIFY(!backend.writePoints(writes, nullptr, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.u16LittleByte")).code,
                 CommErrorCode::InvalidParameter);

        writes.clear();
        writes.insert(QStringLiteral("typed.u16LittleByte"), QStringLiteral("65535"));
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0xffff}));

        writes.clear();
        writes.insert(QStringLiteral("typed.badCount"), 1.0);
        QVERIFY(!backend.writePoints(writes, nullptr, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.badCount")).code,
                 CommErrorCode::InvalidConfig);

        writes.clear();
        writes.insert(QStringLiteral("typed.badType"), QStringLiteral("abc"));
        QVERIFY(!backend.writePoints(writes, nullptr, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.badType")).code,
                 CommErrorCode::InvalidConfig);

        writes.clear();
        writes.insert(QStringLiteral("legacy.badFraction"), 1);
        QVERIFY(!backend.writePoints(writes, nullptr, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("legacy.badFraction")).code,
                 CommErrorCode::InvalidConfig);

        writes.clear();
        writes.insert(QStringLiteral("typed.badScale"), 1.0);
        QVERIFY(!backend.writePoints(writes, nullptr, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.badScale")).code,
                 CommErrorCode::InvalidConfig);

        writes.clear();
        writes.insert(QStringLiteral("typed.writeOnly"), 13);
        QVERIFY(backend.writePoints(writes));
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({13}));
        QHash<QString, QVariant> accessValues;
        QVERIFY(!backend.readPoints({QStringLiteral("typed.writeOnly")},
                                    accessValues,
                                    nullptr,
                                    &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.writeOnly")).code,
                 CommErrorCode::PermissionDenied);

        accessValues.clear();
        QVERIFY(backend.readPoints({QStringLiteral("typed.readOnly")}, accessValues));
        QCOMPARE(accessValues.value(QStringLiteral("typed.readOnly")).toInt(), 12);
        writes.clear();
        writes.insert(QStringLiteral("typed.readOnly"), 14);
        QVERIFY(!backend.writePoints(writes, nullptr, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.readOnly")).code,
                 CommErrorCode::PermissionDenied);

        accessValues.clear();
        QVERIFY(!backend.readPoints({QStringLiteral("typed.offsetOnly")},
                                    accessValues,
                                    nullptr,
                                    &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.offsetOnly")).code,
                 CommErrorCode::InvalidConfig);

        QHash<QString, QVariant> decodeValues;
        QVERIFY(!backend.readPoints({QStringLiteral("typed.badReal")},
                                    decodeValues,
                                    nullptr,
                                    &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("typed.badReal")).code,
                 CommErrorCode::InvalidParameter);
        QVERIFY(!pointErrors.value(QStringLiteral("typed.badReal")).details.isEmpty());

        RuntimePointRegisterCodecSpec realSpec;
        realSpec.dataType = QStringLiteral("REAL");
        QVariant decoded;
        QString decodeError;
        QVERIFY(!RuntimePointRegisterCodec::decode(realSpec,
                                                   QVector<quint16>({0x4000}),
                                                   &decoded,
                                                   &decodeError));
        QVERIFY(!decodeError.isEmpty());
    }

    void mappedTransportErrorsRemainSpecific()
    {
        FakeControllerTransport transport;
        transport.registers[1].insert(190, {7});
        transport.registers[1].insert(192, {8});
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        const auto addPoint = [&cfg](const QString& id, int address) {
            HardwareResourceBinding point;
            point.id = id;
            point.resourceName = id;
            point.resourceType = QStringLiteral("holding");
            point.channel = id;
            point.metadata.insert(QStringLiteral("address"), address);
            point.metadata.insert(QStringLiteral("dataType"), QStringLiteral("UINT16"));
            cfg.resources.append(point);
        };
        addPoint(QStringLiteral("mapped.readFailure"), 190);
        addPoint(QStringLiteral("mapped.writeFailure"), 192);
        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());

        transport.failReadAddress = 190;
        QHash<QString, QVariant> values;
        QHash<QString, CommError> pointErrors;
        QString errorMessage;
        QVERIFY(!backend.readPoints({QStringLiteral("mapped.readFailure")},
                                    values,
                                    &errorMessage,
                                    &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("mapped.readFailure")).code,
                 CommErrorCode::ReceiveFailed);
        QCOMPARE(pointErrors.value(QStringLiteral("mapped.readFailure")).message,
                 QStringLiteral("fake read failed"));
        QCOMPARE(pointErrors.value(QStringLiteral("mapped.readFailure")).details,
                 QStringLiteral("fake read details"));
        QCOMPARE(backend.lastError().code, CommErrorCode::ReceiveFailed);
        QCOMPARE(backend.lastError().message, QStringLiteral("fake read failed"));
        QCOMPARE(backend.lastError().details, QStringLiteral("fake read details"));
        QCOMPARE(errorMessage, QStringLiteral("fake read failed"));

        transport.failReadAddress = -1;
        transport.failWriteAddress = 192;
        QHash<QString, QVariant> writes;
        writes.insert(QStringLiteral("mapped.writeFailure"), 9);
        errorMessage.clear();
        QVERIFY(!backend.writePoints(writes, &errorMessage, &pointErrors));
        QCOMPARE(pointErrors.value(QStringLiteral("mapped.writeFailure")).code,
                 CommErrorCode::SendFailed);
        QCOMPARE(pointErrors.value(QStringLiteral("mapped.writeFailure")).message,
                 QStringLiteral("fake write failed"));
        QCOMPARE(pointErrors.value(QStringLiteral("mapped.writeFailure")).details,
                 QStringLiteral("fake write details"));
        QCOMPARE(backend.lastError().code, CommErrorCode::SendFailed);
        QCOMPARE(backend.lastError().message, QStringLiteral("fake write failed"));
        QCOMPARE(backend.lastError().details, QStringLiteral("fake write details"));
        QCOMPARE(errorMessage, QStringLiteral("fake write failed"));
    }

    void targetProbeFailureDoesNotBreakControllerConnection()
    {
        FakeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        cfg.bridge.parameters.insert(QStringLiteral("targetProbe"),
                                     QVariantMap{{QStringLiteral("slaveId"), 2},
                                                 {QStringLiteral("address"), 999},
                                                 {QStringLiteral("count"), 1}});
        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());

        const BackendStatusSnapshot snapshot = backend.statusSnapshot();
        QVERIFY(snapshot.online);
        QVERIFY(!snapshot.extras.value(QStringLiteral("targetOnline")).toBool());
        QCOMPARE(snapshot.extras.value(QStringLiteral("targetDeviceId")).toInt(), 2);
    }

    void downloadArtifactRejectsMissingProfileBeforeWrite()
    {
        FakeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());

        QTemporaryFile artifact;
        QVERIFY(artifact.open());

        QString errorMessage;
        CommError operationError;
        QVariantMap report;
        QVERIFY(!backend.dryRunDownloadArtifact(artifact.fileName(),
                                                {},
                                                &errorMessage,
                                                &operationError,
                                                &report));
        QCOMPARE(operationError.code, CommErrorCode::InvalidConfig);
        QVERIFY(errorMessage.contains(QStringLiteral("Profile")));
        QCOMPARE(report.value(QStringLiteral("valid")).toBool(), false);

        errorMessage.clear();
        operationError = CommError();
        QVERIFY(!backend.downloadArtifact(artifact.fileName(),
                                          {},
                                          &errorMessage,
                                          &operationError));
        QCOMPARE(operationError.code, CommErrorCode::InvalidConfig);
        QVERIFY(errorMessage.contains(QStringLiteral("Profile")));
        QVERIFY(transport.writes.isEmpty());
        QCOMPARE(backend.statusSnapshot().downloadPercent, 0);
    }

    void downloadArtifactCanExecuteConfiguredProfile()
    {
        FakeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());

        QTemporaryFile artifact;
        QVERIFY(artifact.open());
        artifact.write(QByteArray::fromHex("12345678"));
        artifact.flush();

        QTemporaryFile profile;
        QVERIFY(profile.open());
        profile.write(R"({
            "name": "test",
            "slaveId": 1,
            "steps": [
                {"type": "enter", "params": {"address": 200, "values": [1]}},
                {"type": "sendChunk", "params": {"dataAddress": 210, "chunkWords": 2}},
                {"type": "finalize", "params": {"address": 220, "values": [2]}}
            ]
        })");
        profile.flush();

        QVERIFY(backend.downloadArtifact(artifact.fileName(),
                                         {{QStringLiteral("downloadProfilePath"), profile.fileName()}}));
        QCOMPARE(transport.lastWriteAddress, 220);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({2}));
        QCOMPARE(backend.statusSnapshot().downloadPercent, 100);
    }

    void downloadArtifactHonorsCanonicalChunkFields()
    {
        FakeControllerTransport transport;
        ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend;
        backend.setDebugClientForTest(&client);

        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        cfg.target.parameters.insert(QStringLiteral("deviceId"), 2);
        QVERIFY(backend.configure(cfg));
        QVERIFY(backend.connectBackend());

        QTemporaryFile artifact;
        QVERIFY(artifact.open());
        artifact.write(QByteArray::fromHex("12345678"));
        artifact.flush();

        QTemporaryFile profile;
        QVERIFY(profile.open());
        profile.write(R"({
            "name": "canonical-chunk",
            "slaveId": 2,
            "steps": [
                {"type": "sendChunk", "params": {
                    "dataAddress": 210,
                    "chunkWords": 1,
                    "byteOrder": "LittleEndian",
                    "packetIndexAddress": 201,
                    "packetLengthAddress": 202,
                    "packetCrcAddress": 203,
                    "packetOffsetAddress": 204
                }}
            ]
        })");
        profile.flush();

        QVERIFY(backend.downloadArtifact(
                artifact.fileName(),
                {{QStringLiteral("downloadProfilePath"), profile.fileName()}}));
        QCOMPARE(transport.writes.size(), 10);
        QCOMPARE(transport.writes.at(0).station, 2);
        QCOMPARE(transport.writes.at(0).address, 201);
        QCOMPARE(transport.writes.at(0).values, QVector<quint16>({0}));
        QCOMPARE(transport.writes.at(1).address, 202);
        QCOMPARE(transport.writes.at(1).values, QVector<quint16>({2}));
        QCOMPARE(transport.writes.at(3).address, 204);
        QCOMPARE(transport.writes.at(3).values, QVector<quint16>({0}));
        QCOMPARE(transport.writes.at(4).address, 210);
        QCOMPARE(transport.writes.at(4).values, QVector<quint16>({0x3412}));
        QCOMPARE(transport.writes.at(5).address, 201);
        QCOMPARE(transport.writes.at(5).values, QVector<quint16>({1}));
        QCOMPARE(transport.writes.at(8).address, 204);
        QCOMPARE(transport.writes.at(8).values, QVector<quint16>({2}));
        QCOMPARE(transport.writes.at(9).address, 210);
        QCOMPARE(transport.writes.at(9).values, QVector<quint16>({0x7856}));
    }

    void downloadDryRunValidatesProfileWithoutConnection()
    {
        ControllerDeviceBackend backend;
        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));

        QTemporaryFile artifact;
        QVERIFY(artifact.open());
        artifact.write(QByteArray::fromHex("12345678"));
        artifact.flush();

        QTemporaryFile profile;
        QVERIFY(profile.open());
        profile.write(R"({
            "name": "dry-run",
            "slaveId": 1,
            "steps": [
                {"type": "enter", "params": {"address": 200, "values": [1]}},
                {"type": "sendChunk", "params": {"dataAddress": 210, "chunkWords": 2}},
                {"type": "queryResult", "params": {"address": 220, "count": 1, "expected": [0]}}
            ]
        })");
        profile.flush();

        QVariantMap report;
        QString errorMessage;
        QVERIFY(backend.dryRunDownloadArtifact(artifact.fileName(),
                                               {{QStringLiteral("downloadProfilePath"), profile.fileName()}},
                                               &errorMessage,
                                               nullptr,
                                               &report));
        QCOMPARE(report.value(QStringLiteral("valid")).toBool(), true);
        QCOMPARE(report.value(QStringLiteral("stepCount")).toInt(), 3);
        QCOMPARE(report.value(QStringLiteral("estimatedPacketCount")).toInt(), 1);

        QVERIFY(backend.downloadArtifact(artifact.fileName(),
                                         {{QStringLiteral("downloadProfilePath"), profile.fileName()},
                                          {QStringLiteral("dryRun"), true}},
                                         &errorMessage));
    }

    void downloadDryRunRejectsInvalidProfileStep()
    {
        ControllerDeviceBackend backend;
        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));

        QTemporaryFile artifact;
        QVERIFY(artifact.open());
        artifact.write(QByteArray::fromHex("1234"));
        artifact.flush();

        QTemporaryFile profile;
        QVERIFY(profile.open());
        profile.write(R"({
            "name": "bad",
            "slaveId": 1,
            "steps": [
                {"type": "sendChunk", "params": {"maxRegisters": 200}}
            ]
        })");
        profile.flush();

        QVariantMap report;
        QString errorMessage;
        QVERIFY(!backend.dryRunDownloadArtifact(artifact.fileName(),
                                                {{QStringLiteral("downloadProfilePath"), profile.fileName()}},
                                                &errorMessage,
                                                nullptr,
                                                &report));
        QVERIFY(errorMessage.contains(QStringLiteral("dataAddress")));
        QVERIFY(errorMessage.contains(QStringLiteral("chunkWords")));
    }

    void downloadDryRunRejectsAddressRangeOverflow()
    {
        ControllerDeviceBackend backend;
        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));

        QTemporaryFile artifact;
        QVERIFY(artifact.open());
        artifact.write(QByteArray::fromHex("1234"));
        artifact.flush();

        QTemporaryFile profile;
        QVERIFY(profile.open());
        profile.write(R"({"name":"range","slaveId":1,"steps":[{"type":"sendChunk","params":{"dataAddress":65535,"chunkWords":2}}]})");
        profile.flush();

        QString errorMessage;
        QVERIFY(!backend.dryRunDownloadArtifact(artifact.fileName(),
                                                {{QStringLiteral("downloadProfilePath"), profile.fileName()}},
                                                &errorMessage));
        QVERIFY(errorMessage.contains(QStringLiteral("16 位地址空间")));
    }

    void downloadDryRunAcceptsLastRegisterForOneWordChunk()
    {
        ControllerDeviceBackend backend;
        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));

        QTemporaryFile artifact;
        QVERIFY(artifact.open());
        artifact.write(QByteArray::fromHex("1234"));
        artifact.flush();

        QTemporaryFile profile;
        QVERIFY(profile.open());
        profile.write(R"({"name":"range-ok","slaveId":1,"steps":[{"type":"sendChunk","params":{"dataAddress":65535,"chunkWords":1}}]})");
        profile.flush();

        QString errorMessage;
        QVERIFY(backend.dryRunDownloadArtifact(artifact.fileName(),
                                               {{QStringLiteral("downloadProfilePath"), profile.fileName()}},
                                               &errorMessage));
    }

    void downloadDryRunRejectsMultiValueWriteAcrossAddressLimit()
    {
        ControllerDeviceBackend backend;
        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));

        QTemporaryFile artifact;
        QVERIFY(artifact.open());
        artifact.write(QByteArray::fromHex("1234"));
        artifact.flush();

        QTemporaryFile profile;
        QVERIFY(profile.open());
        profile.write(R"({"name":"write-range","slaveId":1,"steps":[{"type":"enter","params":{"address":65535,"values":[1,2]}},{"type":"sendChunk","params":{"dataAddress":20,"chunkWords":1}}]})");
        profile.flush();

        QString errorMessage;
        QVERIFY(!backend.dryRunDownloadArtifact(artifact.fileName(),
                                                {{QStringLiteral("downloadProfilePath"), profile.fileName()}},
                                                &errorMessage));
        QVERIFY(errorMessage.contains(QStringLiteral("values 长度")));
    }

    void downloadDryRunAcceptsScalarWriteValue()
    {
        ControllerDeviceBackend backend;
        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));

        QTemporaryFile artifact;
        QVERIFY(artifact.open());
        artifact.write(QByteArray::fromHex("1234"));
        artifact.flush();

        QTemporaryFile profile;
        QVERIFY(profile.open());
        profile.write(R"({"name":"scalar-write","slaveId":1,"steps":[{"type":"enter","params":{"address":200,"values":1}},{"type":"sendChunk","params":{"dataAddress":20,"chunkWords":1}}]})");
        profile.flush();

        QString errorMessage;
        QVERIFY(backend.dryRunDownloadArtifact(artifact.fileName(),
                                               {{QStringLiteral("downloadProfilePath"), profile.fileName()}},
                                               &errorMessage));
    }

    void downloadDryRunRejectsInvalidRegisterValues()
    {
        ControllerDeviceBackend backend;
        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));

        QTemporaryFile artifact;
        QVERIFY(artifact.open());
        artifact.write(QByteArray::fromHex("1234"));
        artifact.flush();

        const auto isRejected = [&backend, &artifact](const QByteArray& values) {
            QTemporaryFile profile;
            if (!profile.open()) return false;
            const QByteArray json = QByteArrayLiteral("{\"name\":\"invalid-values\",\"slaveId\":1,\"steps\":[{\"type\":\"enter\",\"params\":{\"address\":200,\"values\":")
                    + values
                    + QByteArrayLiteral("}},{\"type\":\"sendChunk\",\"params\":{\"dataAddress\":20,\"chunkWords\":1}}]}");
            if (profile.write(json) != json.size()) return false;
            profile.flush();
            QString errorMessage;
            return !backend.dryRunDownloadArtifact(
                    artifact.fileName(), {{QStringLiteral("downloadProfilePath"), profile.fileName()}}, &errorMessage);
        };

        QVERIFY(isRejected(QByteArrayLiteral("-1")));
        QVERIFY(isRejected(QByteArrayLiteral("65536")));
        QVERIFY(isRejected(QByteArrayLiteral("1.5")));
        QVERIFY(isRejected(QByteArrayLiteral("[1,65536]")));

        DownloadProfile profile;
        profile.steps = {
            {DownloadProfile::StepType::Enter,
             {{QStringLiteral("address"), 200},
              {QStringLiteral("values"), QStringList{QStringLiteral("1"), QStringLiteral("65536")}}}},
            {DownloadProfile::StepType::SendChunk,
             {{QStringLiteral("dataAddress"), 20}, {QStringLiteral("chunkWords"), 1}}}
        };
        QStringList errors;
        QVERIFY(!profile.validate(&errors));
        QVERIFY(errors.join(QStringLiteral("; ")).contains(QStringLiteral("values")));
    }

    void downloadDryRunRejectsNonTransmittingProfileAndEmptyArtifact()
    {
        ControllerDeviceBackend backend;
        ProjectRuntimeConfig cfg;
        cfg.transport.parameters.insert(QStringLiteral("port"), QStringLiteral("COM7"));
        QVERIFY(backend.configure(cfg));

        QTemporaryFile artifact;
        QVERIFY(artifact.open());
        artifact.flush();
        QTemporaryFile profile;
        QVERIFY(profile.open());
        profile.write(R"({"name":"query-only","steps":[{"type":"queryResult","params":{"address":220,"count":1}}]})");
        profile.flush();

        QString errorMessage;
        QVERIFY(!backend.dryRunDownloadArtifact(artifact.fileName(),
                                                {{QStringLiteral("downloadProfilePath"), profile.fileName()}},
                                                &errorMessage));
        QVERIFY(errorMessage.contains(QStringLiteral("sendChunk")) || errorMessage.contains(QStringLiteral("为空")));
    }
};

QTEST_MAIN(ControllerDeviceBackendTest)
#include "controller_device_backend_test.moc"
