/**
 * @file communication_routing_test.cpp
 * @brief 通信配置解析与后端路由回归测试。
 */

#include <QtTest/QtTest>
#include <QScopedPointer>

#include "common/ConfigTypes.h"
#include "communication/Communication.h"

class CommunicationRoutingTest : public QObject
{
    Q_OBJECT

private slots:
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
