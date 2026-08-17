/**
 * @file controller_device_backend_test.cpp
 * @brief ControllerDeviceBackend unit tests.
 */

#include <QtTest/QtTest>
#include <QTemporaryFile>

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
        if (!connected || failRead) {
            error = CommError(CommProtocolType::ModbusRTU,
                              CommErrorCode::ReceiveFailed,
                              QStringLiteral("fake read failed"));
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
        if (!connected || failWrite) {
            error = CommError(CommProtocolType::ModbusRTU,
                              CommErrorCode::SendFailed,
                              QStringLiteral("fake write failed"));
            return false;
        }
        registers[station].insert(address, values);
        return true;
    }

    CommError lastError() const override { return error; }

    bool connected = true;
    bool failRead = false;
    bool failWrite = false;
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
