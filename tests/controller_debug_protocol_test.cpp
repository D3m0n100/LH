/**
 * @file controller_debug_protocol_test.cpp
 * @brief Controller debug protocol tests.
 */

#include <QtTest/QtTest>

#include "communication/ControllerDebugClient.h"
#include "communication/ControllerDebugProtocol.h"

class FakeControllerDebugTransport : public IControllerDebugTransport
{
public:
    bool connected = true;
    int lastReadAddress = -1;
    int lastReadCount = 0;
    int lastWriteAddress = -1;
    QVector<quint16> lastWriteValues;
    QHash<int, QVector<quint16>> registers;
    CommError error;

    bool isConnected() const override { return connected; }
    int stationAddress() const override { return station; }
    void setStationAddress(int deviceId) override { station = deviceId; }

    bool readHoldingRegisters(int address, int count) override
    {
        lastReadAddress = address;
        lastReadCount = count;
        return connected && registers.contains(address);
    }

    QVector<quint16> holdingRegisterValues(int address) const override
    {
        return registers.value(address);
    }

    bool writeMultipleRegisters(int address, const QVector<quint16>& values) override
    {
        lastWriteAddress = address;
        lastWriteValues = values;
        if (!connected) {
            error = CommError(CommProtocolType::ModbusRTU,
                              CommErrorCode::ConnectionLost,
                              QStringLiteral("offline"));
            return false;
        }
        registers.insert(address, values);
        error = CommError();
        return true;
    }

    CommError lastError() const override { return error; }
    int station = 1;
};

class ControllerDebugProtocolTest : public QObject
{
    Q_OBJECT

private slots:
    void registerMapMatchesManual()
    {
        QCOMPARE(ControllerDebugProtocol::address(ControllerSystemRegister::State), 10);
        QCOMPARE(ControllerDebugProtocol::address(ControllerSystemRegister::ConfSave), 21);
        QCOMPARE(ControllerDebugProtocol::address(ControllerSystemRegister::ReadPassword), 22);
        QCOMPARE(ControllerDebugProtocol::address(ControllerSystemRegister::Reset), 23);
        QCOMPARE(ControllerDebugProtocol::address(ControllerSystemRegister::WorkMode), 26);
        QCOMPARE(ControllerDebugProtocol::address(ControllerSystemRegister::ComponentLine), 27);
        QCOMPARE(ControllerDebugProtocol::address(ControllerSystemRegister::RunToCursor), 28);
        QCOMPARE(ControllerDebugProtocol::address(ControllerSystemRegister::BreakPoint1), 29);
        QCOMPARE(ControllerDebugProtocol::address(ControllerSystemRegister::BreakPoint2), 30);
        QCOMPARE(ControllerDebugProtocol::address(ControllerSystemRegister::DebugStep), 31);
        QCOMPARE(ControllerDebugProtocol::address(ControllerSystemRegister::DebugPause), 32);
        QCOMPARE(ControllerDebugProtocol::address(ControllerSystemRegister::Version), 38);
    }

    void defaultRtuConfigUsesControllerDefaults()
    {
        const QVariantMap cfg = ControllerDebugProtocol::defaultRtuConfig(QStringLiteral("COM7"), 3);
        QCOMPARE(cfg.value(QStringLiteral("protocol")).toString(), QStringLiteral("MODBUS"));
        QCOMPARE(cfg.value(QStringLiteral("mode")).toString(), QStringLiteral("RTU"));
        QCOMPARE(cfg.value(QStringLiteral("type")).toString(), QStringLiteral("Master"));
        QCOMPARE(cfg.value(QStringLiteral("address")).toInt(), 3);
        QCOMPARE(cfg.value(QStringLiteral("port")).toString(), QStringLiteral("COM7"));
        QCOMPARE(cfg.value(QStringLiteral("baudRate")).toInt(), 115200);
        QCOMPARE(cfg.value(QStringLiteral("dataBits")).toInt(), 8);
        QCOMPARE(cfg.value(QStringLiteral("parity")).toString(), QStringLiteral("None"));
        QCOMPARE(cfg.value(QStringLiteral("stopBits")).toInt(), 1);
        QCOMPARE(cfg.value(QStringLiteral("retryCount")).toInt(), 3);
    }

    void deviceIdValidationMatchesSixBitDeviceAddressing()
    {
        QVERIFY(ControllerDebugProtocol::isValidDeviceId(0));
        QVERIFY(ControllerDebugProtocol::isValidDeviceId(63));
        QVERIFY(!ControllerDebugProtocol::isValidDeviceId(-1));
        QVERIFY(!ControllerDebugProtocol::isValidDeviceId(64));

        QVERIFY(!ControllerDebugProtocol::canReadDeviceId(0));
        QVERIFY(ControllerDebugProtocol::canReadDeviceId(1));
        QVERIFY(ControllerDebugProtocol::canReadDeviceId(63));
    }

    void readVersionUsesHoldingRegister()
    {
        FakeControllerDebugTransport transport;
        transport.registers.insert(38, {0x1234});
        ControllerDebugClient client(&transport);

        quint16 version = 0;
        QVERIFY(client.readVersion(&version));
        QCOMPARE(version, quint16(0x1234));
        QCOMPARE(transport.lastReadAddress, 38);
        QCOMPARE(transport.lastReadCount, 1);
    }

    void readStatusReadsControllerStatusRegisters()
    {
        FakeControllerDebugTransport transport;
        transport.registers.insert(10, {1});
        transport.registers.insert(26, {2});
        transport.registers.insert(27, {345});
        transport.registers.insert(23, {15});
        transport.registers.insert(38, {105});
        ControllerDebugClient client(&transport);

        ControllerDebugStatus status;
        QVERIFY(client.readStatus(&status));
        QCOMPARE(status.state, quint16(1));
        QCOMPARE(status.workMode, quint16(2));
        QCOMPARE(status.componentLine, quint16(345));
        QCOMPARE(status.reset, quint16(15));
        QCOMPARE(status.version, quint16(105));
    }

    void debugCommandsWriteExpectedRegisters()
    {
        FakeControllerDebugTransport transport;
        ControllerDebugClient client(&transport);

        QVERIFY(client.pause());
        QCOMPARE(transport.lastWriteAddress, 32);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({1}));

        QVERIFY(client.resume());
        QCOMPARE(transport.lastWriteAddress, 32);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({0}));

        QVERIFY(client.step());
        QCOMPARE(transport.lastWriteAddress, 31);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({1}));

        QVERIFY(client.runToCursor(99));
        QCOMPARE(transport.lastWriteAddress, 28);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({99}));

        QVERIFY(client.setBreakpoints(10, 20));
        QCOMPARE(transport.lastWriteAddress, 29);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({10, 20}));
    }

    void resetAndSaveUseDocumentedSystemRegisters()
    {
        FakeControllerDebugTransport transport;
        ControllerDebugClient client(&transport);

        QVERIFY(client.reset(ControllerResetCode::MonitorCommand));
        QCOMPARE(transport.lastWriteAddress, 23);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({1}));

        QVERIFY(client.saveConfiguration());
        QCOMPARE(transport.lastWriteAddress, 21);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({1}));
    }

    void disconnectedTransportFailsWithConnectionError()
    {
        FakeControllerDebugTransport transport;
        transport.connected = false;
        ControllerDebugClient client(&transport);

        QVERIFY(!client.pause());
        QCOMPARE(client.lastError().code, CommErrorCode::ConnectionLost);
    }

    void rawHoldingRegisterAccessTemporarilySwitchesDeviceId()
    {
        FakeControllerDebugTransport transport;
        transport.station = 1;
        transport.registers.insert(60, {7, 8});
        ControllerDebugClient client(&transport);

        QVector<quint16> values;
        QVERIFY(client.readHoldingRegisters(5, 60, 2, &values));
        QCOMPARE(values, QVector<quint16>({7, 8}));
        QCOMPARE(transport.station, 1);

        QVERIFY(client.writeHoldingRegisters(6, 70, {9}));
        QCOMPARE(transport.lastWriteAddress, 70);
        QCOMPARE(transport.lastWriteValues, QVector<quint16>({9}));
        QCOMPARE(transport.station, 1);
    }
};

QTEST_MAIN(ControllerDebugProtocolTest)
#include "controller_debug_protocol_test.moc"
