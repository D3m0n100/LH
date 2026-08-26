/**
 * @file communication_routing_test.cpp
 * @brief 通信配置解析与后端路由回归测试。
 */

#include <QtTest/QtTest>
#include <QHostAddress>
#include <QSignalSpy>
#include <QScopedPointer>
#include <QUdpSocket>
#include <QtSerialBus/QModbusDevice>

#include "common/ConfigTypes.h"
#include "communication/Communication.h"

class CommunicationRoutingTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<CommError>("CommError");
    }

    void nestedModbusRtuConfigResolves()
    {
        QVariantMap config;
        config.insert(QStringLiteral("protocol"), QStringLiteral("ModbusRTU"));
        config.insert(QStringLiteral("serial"),
                      QVariantMap{{QStringLiteral("portName"), QStringLiteral("COM3")},
                                  {QStringLiteral("baudRate"), 115200}});
        config.insert(QStringLiteral("modbus"),
                      QVariantMap{{QStringLiteral("timeoutMs"), 300},
                                  {QStringLiteral("retryCount"), 3}});

        const Communication::ResolvedCommConfig resolved = Communication::resolveConfig(config);

        QVERIFY(resolved.type == CommProtocolType::ModbusRTU);
        QCOMPARE(resolved.parameters.value(QStringLiteral("protocol")).toString(),
                 QStringLiteral("MODBUS"));
        QCOMPARE(resolved.parameters.value(QStringLiteral("mode")).toString(),
                 QStringLiteral("RTU"));
        QCOMPARE(resolved.parameters.value(QStringLiteral("port")).toString(),
                 QStringLiteral("COM3"));
        QCOMPARE(resolved.parameters.value(QStringLiteral("responseTimeout")).toInt(), 300);

        QScopedPointer<ICommInterface> interface(Communication::createInterface(resolved.type));
        QVERIFY(qobject_cast<ModbusInterface*>(interface.data()));
    }

    void modbusReplyFailureClassificationNeverReportsNoError()
    {
        ModbusInterface interface;

        QCOMPARE(interface.classifyReplyFailure(true, QModbusDevice::NoError),
                 CommErrorCode::ReceiveTimeout);
        QCOMPARE(interface.classifyReplyFailure(false, QModbusDevice::ProtocolError),
                 CommErrorCode::ProtocolError);
        QCOMPARE(interface.classifyReplyFailure(false, QModbusDevice::ConnectionError),
                 CommErrorCode::ConnectionLost);
        QCOMPARE(interface.classifyReplyFailure(false, QModbusDevice::NoError),
                 CommErrorCode::InvalidResponse);

        QVERIFY(interface.classifyReplyFailure(true, QModbusDevice::NoError)
                != CommErrorCode::NoError);
        QVERIFY(interface.classifyReplyFailure(false, QModbusDevice::ProtocolError)
                != CommErrorCode::NoError);
        QVERIFY(interface.classifyReplyFailure(false, QModbusDevice::ConnectionError)
                != CommErrorCode::NoError);
        QVERIFY(interface.classifyReplyFailure(false, QModbusDevice::NoError)
                != CommErrorCode::NoError);
    }

    void typedModbusTcpConfigResolvesToModbus()
    {
        ModbusConfig config;
        config.mode = ModbusConfig::Mode::TCP;
        config.host = QStringLiteral("192.0.2.10");
        config.port = 1502;

        const Communication::ResolvedCommConfig resolved =
                Communication::resolveConfig(config.toVariantMap());

        QVERIFY(resolved.type == CommProtocolType::ModbusTCP);
        QCOMPARE(resolved.parameters.value(QStringLiteral("protocol")).toString(),
                 QStringLiteral("MODBUS"));
        QCOMPARE(resolved.parameters.value(QStringLiteral("mode")).toString(),
                 QStringLiteral("TCP"));
        QCOMPARE(resolved.parameters.value(QStringLiteral("tcpPort")).toInt(), 1502);

        QScopedPointer<ICommInterface> interface(Communication::createInterface(resolved.type));
        QVERIFY(qobject_cast<ModbusInterface*>(interface.data()));
    }

    void transportParametersAreFlattened()
    {
        const QVariantMap config{{QStringLiteral("protocol"), QStringLiteral("modbus")},
                                 {QStringLiteral("mode"), QStringLiteral("rtu")},
                                 {QStringLiteral("parameters"),
                                  QVariantMap{{QStringLiteral("port"), QStringLiteral("COM4")},
                                              {QStringLiteral("baudRate"), 57600}}}};

        const Communication::ResolvedCommConfig resolved = Communication::resolveConfig(config);

        QVERIFY(resolved.type == CommProtocolType::ModbusRTU);
        QCOMPARE(resolved.parameters.value(QStringLiteral("port")).toString(),
                 QStringLiteral("COM4"));
        QCOMPARE(resolved.parameters.value(QStringLiteral("baudRate")).toInt(), 57600);
    }

    void ethernetUdpModeIsNormalizedForBackend()
    {
        const QVariantMap config{{QStringLiteral("protocol"), QStringLiteral("ETHERNET")},
                                 {QStringLiteral("mode"), QStringLiteral("UDP")},
                                 {QStringLiteral("host"), QStringLiteral("127.0.0.1")},
                                 {QStringLiteral("port"), 9000}};

        const Communication::ResolvedCommConfig resolved = Communication::resolveConfig(config);
        const EthernetConfig backendConfig = EthernetConfig::fromMap(resolved.parameters);

        QVERIFY(resolved.type == CommProtocolType::EthernetUDP);
        QCOMPARE(resolved.parameters.value(QStringLiteral("protocol")).toString(),
                 QStringLiteral("UDP"));
        QVERIFY(backendConfig.protocol == EthernetConfig::Protocol::UDP);

        QScopedPointer<ICommInterface> interface(Communication::createInterface(resolved.type));
        QVERIFY(qobject_cast<EthernetInterface*>(interface.data()));
    }

    void explicitProtocolBeatsSerialFieldHeuristic()
    {
        const QVariantMap config{{QStringLiteral("protocol"), QStringLiteral("MODBUS")},
                                 {QStringLiteral("mode"), QStringLiteral("TCP")},
                                 {QStringLiteral("host"), QStringLiteral("192.0.2.20")},
                                 {QStringLiteral("tcpPort"), 502},
                                 {QStringLiteral("baudRate"), 9600}};

        const Communication::ResolvedCommConfig resolved = Communication::resolveConfig(config);

        QVERIFY(resolved.type == CommProtocolType::ModbusTCP);
    }

    void unknownExplicitProtocolIsRejected()
    {
        const QVariantMap config{{QStringLiteral("protocol"), QStringLiteral("not-supported")},
                                 {QStringLiteral("host"), QStringLiteral("127.0.0.1")}};

        const Communication::ResolvedCommConfig resolved = Communication::resolveConfig(config);

        QVERIFY(!resolved.isValid());
        QVERIFY(!Communication::createInterface(resolved.type));
    }

    void explicitTransportModesAreWhitelisted()
    {
        const QList<QVariantMap> invalidConfigs = {
            QVariantMap{{QStringLiteral("protocol"), QStringLiteral("MODBUS")},
                        {QStringLiteral("mode"), QStringLiteral("invalid")},
                        {QStringLiteral("port"), QStringLiteral("COM3")}},
            QVariantMap{{QStringLiteral("protocol"), QStringLiteral("ETHERNET")},
                        {QStringLiteral("mode"), QStringLiteral("invalid")},
                        {QStringLiteral("host"), QStringLiteral("127.0.0.1")},
                        {QStringLiteral("port"), 8080}},
            QVariantMap{{QStringLiteral("protocol"), QStringLiteral("SERIAL")},
                        {QStringLiteral("mode"), QStringLiteral("invalid")},
                        {QStringLiteral("port"), QStringLiteral("COM3")}},
            QVariantMap{{QStringLiteral("protocol"), QStringLiteral("MODBUSRTU")},
                        {QStringLiteral("mode"), QStringLiteral("TCP")},
                        {QStringLiteral("port"), QStringLiteral("COM3")}},
            QVariantMap{{QStringLiteral("protocol"), QStringLiteral("TCP")},
                        {QStringLiteral("mode"), QStringLiteral("UDP")},
                        {QStringLiteral("host"), QStringLiteral("127.0.0.1")},
                        {QStringLiteral("port"), 8080}}
        };

        for (const QVariantMap& config : invalidConfigs) {
            QVERIFY(!Communication::resolveConfig(config).isValid());
        }
    }

    void directConfigEnumsAndPortsAreStrict()
    {
        QVERIFY(!SerialConfig::fromMap(
                         QVariantMap{{QStringLiteral("port"), QStringLiteral("COM3")},
                                     {QStringLiteral("parity"), QStringLiteral("invalid")}})
                         .isValid());
        QVERIFY(!SerialConfig::fromMap(
                         QVariantMap{{QStringLiteral("port"), QStringLiteral("COM3")},
                                     {QStringLiteral("flowControl"), QStringLiteral("invalid")}})
                         .isValid());
        QVERIFY(!ModbusConfig::fromMap(
                         QVariantMap{{QStringLiteral("mode"), QStringLiteral("RTU")},
                                     {QStringLiteral("type"), QStringLiteral("invalid")},
                                     {QStringLiteral("port"), QStringLiteral("COM3")}})
                         .isValid());
        QVERIFY(!EthernetConfig::fromMap(
                         QVariantMap{{QStringLiteral("protocol"), QStringLiteral("TCP")},
                                     {QStringLiteral("role"), QStringLiteral("invalid")},
                                     {QStringLiteral("port"), 8080}})
                         .isValid());
        QVERIFY(!SerialConfig::fromMap(
                         QVariantMap{{QStringLiteral("port"), QStringLiteral("COM3")},
                                     {QStringLiteral("frameTimeout"), 0}})
                         .isValid());
        QVERIFY(!ModbusConfig::fromMap(
                         QVariantMap{{QStringLiteral("mode"), QStringLiteral("RTU")},
                                     {QStringLiteral("port"), QStringLiteral("COM3")},
                                     {QStringLiteral("responseTimeout"), 0}})
                         .isValid());
        QVERIFY(!EthernetConfig::fromMap(
                         QVariantMap{{QStringLiteral("protocol"), QStringLiteral("TCP")},
                                     {QStringLiteral("port"), 8080},
                                     {QStringLiteral("receiveBufferSize"), 0}})
                         .isValid());

        const QList<QVariant> invalidPorts = {
            QVariant(0), QVariant(65536), QVariant(-1),
            QVariant(QStringLiteral("not-a-number")), QVariant(QStringLiteral("502x"))
        };
        for (const QVariant& port : invalidPorts) {
            const QVariantMap ethernet{{QStringLiteral("protocol"), QStringLiteral("TCP")},
                                       {QStringLiteral("port"), port}};
            QVERIFY(!EthernetConfig::fromMap(ethernet).isValid());

            const QVariantMap modbus{{QStringLiteral("protocol"), QStringLiteral("MODBUS")},
                                     {QStringLiteral("mode"), QStringLiteral("TCP")},
                                     {QStringLiteral("host"), QStringLiteral("127.0.0.1")},
                                     {QStringLiteral("tcpPort"), port}};
            QVERIFY(!ModbusConfig::fromMap(modbus).isValid());
        }

        const EthernetConfig valid = EthernetConfig::fromMap(
                QVariantMap{{QStringLiteral("protocol"), QStringLiteral("TCP")},
                            {QStringLiteral("host"), QStringLiteral("127.0.0.1")},
                            {QStringLiteral("port"), 8080}});
        QVERIFY(valid.isValid());
    }

    void directOpenRejectsInvalidConfigsBeforeIo()
    {
        {
            EthernetInterface interface;
            QSignalSpy spy(&interface, SIGNAL(errorOccurred(CommError)));
            QVERIFY(!interface.open(QVariantMap{{QStringLiteral("protocol"), QStringLiteral("TCP")},
                                                {QStringLiteral("role"), QStringLiteral("invalid")},
                                                {QStringLiteral("port"), 8080}}));
            QVERIFY(!interface.isConnected());
            QCOMPARE(interface.lastError().code, CommErrorCode::InvalidConfig);
            QVERIFY(spy.count() >= 1);
            QCOMPARE(qvariant_cast<CommError>(spy.first().at(0)).code,
                     CommErrorCode::InvalidConfig);
        }

        {
            EthernetInterface interface;
            QSignalSpy spy(&interface, SIGNAL(errorOccurred(CommError)));
            QVERIFY(!interface.open(QVariantMap{{QStringLiteral("protocol"), QStringLiteral("TCP")},
                                                {QStringLiteral("port"), 0}}));
            QVERIFY(!interface.isConnected());
            QCOMPARE(interface.lastError().code, CommErrorCode::InvalidConfig);
            QVERIFY(spy.count() >= 1);
            QCOMPARE(qvariant_cast<CommError>(spy.first().at(0)).code,
                     CommErrorCode::InvalidConfig);
        }

        {
            ModbusInterface interface;
            QSignalSpy spy(&interface, SIGNAL(errorOccurred(CommError)));
            QVERIFY(!interface.open(QVariantMap{{QStringLiteral("protocol"), QStringLiteral("MODBUS")},
                                                {QStringLiteral("mode"), QStringLiteral("invalid")},
                                                {QStringLiteral("port"), QStringLiteral("COM3")}}));
            QVERIFY(!interface.isConnected());
            QCOMPARE(interface.lastError().code, CommErrorCode::InvalidConfig);
            QVERIFY(spy.count() >= 1);
            QCOMPARE(qvariant_cast<CommError>(spy.first().at(0)).code,
                     CommErrorCode::InvalidConfig);
        }

        {
            ModbusInterface interface;
            QSignalSpy spy(&interface, SIGNAL(errorOccurred(CommError)));
            QVERIFY(!interface.open(QVariantMap{{QStringLiteral("protocol"), QStringLiteral("MODBUS")},
                                                {QStringLiteral("mode"), QStringLiteral("TCP")},
                                                {QStringLiteral("host"), QStringLiteral("127.0.0.1")},
                                                {QStringLiteral("tcpPort"), 0}}));
            QVERIFY(!interface.isConnected());
            QCOMPARE(interface.lastError().code, CommErrorCode::InvalidConfig);
            QVERIFY(spy.count() >= 1);
            QCOMPARE(qvariant_cast<CommError>(spy.first().at(0)).code,
                     CommErrorCode::InvalidConfig);
        }
    }

    void invalidBindAddressesAreRejected()
    {
        const QList<EthernetConfig::Protocol> protocols = {
            EthernetConfig::Protocol::TCP,
            EthernetConfig::Protocol::UDP
        };

        for (const EthernetConfig::Protocol protocol : protocols) {
            EthernetConfig config;
            config.protocol = protocol;
            config.role = EthernetConfig::Role::Server;
            config.host = QStringLiteral("not-an-ip-address");
            config.keepAliveInterval = 0;

            EthernetInterface interface;
            QVERIFY(!interface.open(config));
            QCOMPARE(interface.lastError().code, CommErrorCode::InvalidConfig);
            QVERIFY(!interface.isConnected());
        }
    }

    void udpReceiveKeepsDatagramsSeparate()
    {
        QUdpSocket portProbe;
        QVERIFY(portProbe.bind(QHostAddress::LocalHost, 0));
        const quint16 port = portProbe.localPort();
        portProbe.close();

        EthernetConfig config;
        config.protocol = EthernetConfig::Protocol::UDP;
        config.role = EthernetConfig::Role::Server;
        config.host = QStringLiteral("127.0.0.1");
        config.port = port;
        config.keepAliveInterval = 0;

        EthernetInterface receiver;
        QVERIFY(receiver.open(config));

        QVERIFY(receiver.sendTo(QByteArrayLiteral("x"), QStringLiteral("not-an-ip-address"), 1) < 0);
        QCOMPARE(receiver.lastError().code, CommErrorCode::InvalidConfig);

        QUdpSocket sender;
        QSignalSpy dataSpy(&receiver, SIGNAL(dataReceived(QByteArray)));
        QVERIFY(sender.writeDatagram(QByteArray(), QHostAddress::LocalHost, port) >= 0);
        QVERIFY(sender.writeDatagram(QByteArrayLiteral("first"), QHostAddress::LocalHost, port) >= 0);
        QVERIFY(sender.writeDatagram(QByteArrayLiteral("second"), QHostAddress::LocalHost, port) >= 0);

        QTRY_COMPARE(dataSpy.count(), 3);
        QCOMPARE(receiver.receive(0), QByteArray());
        QCOMPARE(receiver.receive(0), QByteArrayLiteral("first"));
        QCOMPARE(receiver.receive(0), QByteArrayLiteral("second"));
    }

    void legacyProjectProtocolMigratesToTransport()
    {
        QJsonObject object;
        object.insert(QStringLiteral("protocol"), QStringLiteral("ModbusTCP"));
        object.insert(QStringLiteral("commParameters"),
                      QJsonObject::fromVariantMap(
                              QVariantMap{{QStringLiteral("host"), QStringLiteral("192.0.2.30")},
                                          {QStringLiteral("tcpPort"), 1502}}));

        const ProjectRuntimeConfig config = ProjectRuntimeConfig::fromJson(object);

        QCOMPARE(config.transport.protocol, QStringLiteral("modbus"));
        QCOMPARE(config.transport.mode, QStringLiteral("tcp"));
        QCOMPARE(config.transport.parameters.value(QStringLiteral("host")).toString(),
                 QStringLiteral("192.0.2.30"));
        QCOMPARE(config.transport.parameters.value(QStringLiteral("tcpPort")).toInt(), 1502);
    }

    void defaultTransportPlaceholderUsesLegacyUdp()
    {
        QJsonObject object;
        object.insert(QStringLiteral("protocol"), QStringLiteral("UDP"));
        object.insert(QStringLiteral("commParameters"), QJsonObject());
        object.insert(QStringLiteral("transport"), QJsonObject{
            {QStringLiteral("protocol"), QStringLiteral("modbus")},
            {QStringLiteral("mode"), QStringLiteral("rtu")},
            {QStringLiteral("parameters"), QJsonObject()}
        });

        const ProjectRuntimeConfig config = ProjectRuntimeConfig::fromJson(object);
        QCOMPARE(config.transport.protocol, QStringLiteral("ethernet"));
        QCOMPARE(config.transport.mode, QStringLiteral("udp"));
    }

    void defaultTransportPlaceholderUsesLegacyCan()
    {
        QJsonObject object;
        object.insert(QStringLiteral("protocol"), QStringLiteral("CAN"));
        object.insert(QStringLiteral("transport"), QJsonObject{
            {QStringLiteral("protocol"), QStringLiteral("modbus")},
            {QStringLiteral("mode"), QStringLiteral("rtu")}
        });

        const ProjectRuntimeConfig config = ProjectRuntimeConfig::fromJson(object);
        QCOMPARE(config.transport.protocol, QStringLiteral("can"));
    }

    void explicitTransportIsNotOverriddenByLegacyFields()
    {
        QJsonObject object;
        object.insert(QStringLiteral("protocol"), QStringLiteral("UDP"));
        object.insert(QStringLiteral("commParameters"),
                      QJsonObject{{QStringLiteral("host"), QStringLiteral("192.0.2.40")}});
        object.insert(QStringLiteral("transport"), QJsonObject{
            {QStringLiteral("protocol"), QStringLiteral("modbus")},
            {QStringLiteral("mode"), QStringLiteral("rtu")},
            {QStringLiteral("parameters"),
             QJsonObject{{QStringLiteral("port"), QStringLiteral("COM8")}}}
        });

        const ProjectRuntimeConfig config = ProjectRuntimeConfig::fromJson(object);
        QCOMPARE(config.transport.protocol, QStringLiteral("modbus"));
        QCOMPARE(config.transport.mode, QStringLiteral("rtu"));
        QCOMPARE(config.transport.parameters.value(QStringLiteral("port")).toString(),
                 QStringLiteral("COM8"));
    }

};

QTEST_APPLESS_MAIN(CommunicationRoutingTest)
#include "communication_routing_test.moc"
