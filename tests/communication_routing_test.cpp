#include <QModbusReply>
#include <QElapsedTimer>
#include <QTimer>
/**
 * @file communication_routing_test.cpp
 * @brief 通信配置解析与后端路由回归测试。
 */

#include <QtTest/QtTest>
#include <QHostAddress>
#include <QSignalSpy>
#include <QScopedPointer>
#include <QUdpSocket>
#include <QTcpServer>
#include <QTcpSocket>
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

    void inflightReplyHonorsRemainingDeadlineAndCancellation()
    {
        ModbusInterface interface;
        interface.setResponseTimeout(10000);interface.setRetryCount(10);
        std::atomic_bool cancelled{false};
        QModbusReply neverFinishes(QModbusReply::Common,1);
        interface.setRequestBudget(200,&cancelled);
        QString error;bool timedOut=false;QElapsedTimer timer;timer.start();
        QVERIFY(!interface.waitForReply(&neverFinishes,error,timedOut));QVERIFY(timedOut);
        QVERIFY(timer.elapsed()>=150);QVERIFY(timer.elapsed()<450);
        interface.setRequestBudget(5000,&cancelled);timer.restart();
        QTimer::singleShot(50,&interface,[&](){cancelled=true;});
        QVERIFY(!interface.waitForReply(&neverFinishes,error,timedOut));
        QCOMPARE(interface.classifyReplyFailure(timedOut,QModbusDevice::NoError),CommErrorCode::OperationCancelled);
        QVERIFY(timer.elapsed()<200);
        interface.setRequestBudget(-1,nullptr);
        QCOMPARE(interface.responseTimeout(),10000);QCOMPARE(interface.retryCount(),10);
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
        QVERIFY(portProbe.bind(QHostAddress(QStringLiteral("127.0.0.1")), static_cast<quint16>(0)));
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
        QSignalSpy dataSpy(&receiver, &EthernetInterface::dataReceived);
        QVERIFY(sender.writeDatagram(QByteArray(), QHostAddress(QStringLiteral("127.0.0.1")), port) >= 0);
        QVERIFY(sender.writeDatagram(QByteArrayLiteral("first"), QHostAddress(QStringLiteral("127.0.0.1")), port) >= 0);
        QVERIFY(sender.writeDatagram(QByteArrayLiteral("second"), QHostAddress(QStringLiteral("127.0.0.1")), port) >= 0);

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

    void tcpServerConnectionStateReflectsClientCount()
    {
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress(QStringLiteral("127.0.0.1")), 0));
        const quint16 port = probe.serverPort();
        probe.close();

        EthernetConfig config;
        config.protocol = EthernetConfig::Protocol::TCP;
        config.role = EthernetConfig::Role::Server;
        config.host = QStringLiteral("127.0.0.1");
        config.port = port;
        config.keepAliveInterval = 0;

        EthernetInterface server;
        QSignalSpy connSpy(&server, &EthernetInterface::connectionStateChanged);

        QVERIFY(server.open(config));
        // 刚启动监听时，没有客户端连接，isConnected() 必须为 false，且不能发射 connectionStateChanged(true)
        QVERIFY(!server.isConnected());
        QCOMPARE(connSpy.count(), 0);

        // 连入一个客户端
        QTcpSocket client;
        client.connectToHost(QHostAddress(QStringLiteral("127.0.0.1")), port);
        QVERIFY(client.waitForConnected(3000));

        // 等待 server 接收并触发 connectionStateChanged(true)
        QTRY_COMPARE(connSpy.count(), 1);
        QCOMPARE(connSpy.takeFirst().first().toBool(), true);
        QVERIFY(server.isConnected());

        // 客户端断开连接
        client.disconnectFromHost();
        QTRY_COMPARE(connSpy.count(), 1);
        QCOMPARE(connSpy.takeFirst().first().toBool(), false);
        QVERIFY(!server.isConnected());

        server.close();
    }

    void tcpServerProactiveCloseWithConnectedClients()
    {
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress(QStringLiteral("127.0.0.1")), 0));
        const quint16 port = probe.serverPort();
        probe.close();

        EthernetConfig config;
        config.protocol = EthernetConfig::Protocol::TCP;
        config.role = EthernetConfig::Role::Server;
        config.host = QStringLiteral("127.0.0.1");
        config.port = port;
        config.keepAliveInterval = 0;

        EthernetInterface server;
        QSignalSpy connSpy(&server, &EthernetInterface::connectionStateChanged);
        QSignalSpy clientDiscSpy(&server, &EthernetInterface::clientDisconnected);

        QVERIFY(server.open(config));
        QVERIFY(!server.isConnected());
        QCOMPARE(connSpy.count(), 0);

        QTcpSocket client;
        client.connectToHost(QHostAddress(QStringLiteral("127.0.0.1")), port);
        QVERIFY(client.waitForConnected(3000));

        // 等待 server 确认客户端连入
        QTRY_COMPARE(connSpy.count(), 1);
        QCOMPARE(connSpy.takeFirst().first().toBool(), true);
        QVERIFY(server.isConnected());

        // 在客户端保持连接时，服务器主动调用 close()
        server.close();

        // 验证 isConnected() 已置为 false
        QVERIFY(!server.isConnected());
        // 客户端断开通知信号正常触发
        QCOMPARE(clientDiscSpy.count(), 1);
        // connectionStateChanged(false) 仅发射一次，杜绝重复发射
        QCOMPARE(connSpy.count(), 1);
        QCOMPARE(connSpy.takeFirst().first().toBool(), false);

        client.disconnectFromHost();
    }

    void tcpReceiveBufferSizeValidation()
    {
        const QVariantMap invalidSmall{{QStringLiteral("protocol"), QStringLiteral("TCP")},
                                       {QStringLiteral("receiveBufferSize"), 0}};
        QVERIFY(!EthernetConfig::fromMap(invalidSmall).isValid());

        const QVariantMap invalidNegative{{QStringLiteral("protocol"), QStringLiteral("TCP")},
                                          {QStringLiteral("receiveBufferSize"), -1}};
        QVERIFY(!EthernetConfig::fromMap(invalidNegative).isValid());

        const QVariantMap invalidLarge{{QStringLiteral("protocol"), QStringLiteral("TCP")},
                                       {QStringLiteral("receiveBufferSize"),
                                        EthernetConfig::MAX_RECEIVE_BUFFER_SIZE + 1}};
        QVERIFY(!EthernetConfig::fromMap(invalidLarge).isValid());

        const QVariantMap validDefault{{QStringLiteral("protocol"), QStringLiteral("TCP")},
                                       {QStringLiteral("receiveBufferSize"),
                                        EthernetConfig::DEFAULT_RECEIVE_BUFFER_SIZE}};
        QVERIFY(EthernetConfig::fromMap(validDefault).isValid());

        const QVariantMap validMax{{QStringLiteral("protocol"), QStringLiteral("TCP")},
                                   {QStringLiteral("receiveBufferSize"),
                                    EthernetConfig::MAX_RECEIVE_BUFFER_SIZE}};
        QVERIFY(EthernetConfig::fromMap(validMax).isValid());

        EthernetInterface iface;
        QVERIFY(!iface.open(invalidLarge));
        QCOMPARE(iface.lastError().code, CommErrorCode::InvalidConfig);
    }

    void tcpReceiveBufferCapacityBoundScenarios()
    {
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress(QStringLiteral("127.0.0.1")), 0));
        const quint16 port = probe.serverPort();
        probe.close();

        EthernetConfig config;
        config.protocol = EthernetConfig::Protocol::TCP;
        config.role = EthernetConfig::Role::Server;
        config.host = QStringLiteral("127.0.0.1");
        config.port = port;
        config.keepAliveInterval = 0;
        config.receiveBufferSize = EthernetInterface::MAX_BUFFER_SIZE;

        EthernetInterface server;
        QVERIFY(server.open(config));

        QTcpSocket client;
        client.connectToHost(QHostAddress(QStringLiteral("127.0.0.1")), port);
        QVERIFY(client.waitForConnected(3000));
        QTRY_VERIFY(server.isConnected());

        // 场景 1: 空缓存大输入（单次输入 > 1 MB）
        {
            const int totalSend = EthernetInterface::MAX_BUFFER_SIZE + 200 * 1024;
            QByteArray sendData(totalSend, 'A');
            for (int i = 0; i < 1000; ++i) {
                sendData[totalSend - 1000 + i] = static_cast<char>('0' + (i % 10));
            }

            qint64 written = 0;
            while (written < sendData.size()) {
                const qint64 chunk = client.write(sendData.constData() + written,
                                                  qMin(qint64(64 * 1024), qint64(sendData.size() - written)));
                QVERIFY(chunk > 0);
                written += chunk;
                QVERIFY(client.waitForBytesWritten(3000));
            }

            QTRY_VERIFY_WITH_TIMEOUT(server.bytesReceived() >= totalSend, 5000);

            QByteArray received = server.receive(500);
            QCOMPARE(received.size(), EthernetInterface::MAX_BUFFER_SIZE);
            QCOMPARE(received, sendData.right(EthernetInterface::MAX_BUFFER_SIZE));
        }

        // 场景 2: 非空缓存叠加输入，累计超过上限
        {
            const qint64 baseReceived = server.bytesReceived();
            const int part1Size = 600 * 1024;
            QByteArray part1(part1Size, 'B');
            qint64 written = 0;
            while (written < part1.size()) {
                const qint64 chunk = client.write(part1.constData() + written,
                                                  qMin(qint64(64 * 1024), qint64(part1.size() - written)));
                QVERIFY(chunk > 0);
                written += chunk;
                QVERIFY(client.waitForBytesWritten(3000));
            }

            const int part2Size = 600 * 1024;
            QByteArray part2(part2Size, 'C');
            written = 0;
            while (written < part2.size()) {
                const qint64 chunk = client.write(part2.constData() + written,
                                                  qMin(qint64(64 * 1024), qint64(part2.size() - written)));
                QVERIFY(chunk > 0);
                written += chunk;
                QVERIFY(client.waitForBytesWritten(3000));
            }

            QTRY_VERIFY_WITH_TIMEOUT(server.bytesReceived() >= baseReceived + part1Size + part2Size, 5000);

            const QByteArray expectedTail = (part1 + part2).right(EthernetInterface::MAX_BUFFER_SIZE);
            QByteArray received = server.receive(500);
            QCOMPARE(received.size(), EthernetInterface::MAX_BUFFER_SIZE);
            QCOMPARE(received, expectedTail);
        }

        // 场景 3: 恰好达到上限（512 KB + 512 KB = 1024 KB）
        {
            const qint64 baseReceived = server.bytesReceived();
            const int halfSize = EthernetInterface::MAX_BUFFER_SIZE / 2;
            QByteArray partA(halfSize, 'E');
            QByteArray partB(halfSize, 'F');

            qint64 written = 0;
            while (written < partA.size()) {
                const qint64 chunk = client.write(partA.constData() + written,
                                                  qMin(qint64(64 * 1024), qint64(partA.size() - written)));
                QVERIFY(chunk > 0);
                written += chunk;
                QVERIFY(client.waitForBytesWritten(3000));
            }
            written = 0;
            while (written < partB.size()) {
                const qint64 chunk = client.write(partB.constData() + written,
                                                  qMin(qint64(64 * 1024), qint64(partB.size() - written)));
                QVERIFY(chunk > 0);
                written += chunk;
                QVERIFY(client.waitForBytesWritten(3000));
            }

            QTRY_VERIFY_WITH_TIMEOUT(server.bytesReceived() >= baseReceived + halfSize * 2, 5000);

            QByteArray received = server.receive(500);
            QCOMPARE(received.size(), EthernetInterface::MAX_BUFFER_SIZE);
            QCOMPARE(received, partA + partB);
        }

        // 场景 4: 连续小包突发累积溢出，验证上限始终不超过 1 MB
        {
            const qint64 baseReceived = server.bytesReceived();
            const int burstCount = 20;
            const int burstChunkSize = 64 * 1024;
            for (int i = 0; i < burstCount; ++i) {
                client.write(QByteArray(burstChunkSize, 'D'));
                QVERIFY(client.waitForBytesWritten(1000));
            }
            QTRY_VERIFY_WITH_TIMEOUT(server.bytesReceived() >= baseReceived + burstCount * burstChunkSize, 5000);
            QByteArray finalBuf = server.receive(200);
            QCOMPARE(finalBuf.size(), EthernetInterface::MAX_BUFFER_SIZE);
        }

        server.close();
        client.disconnectFromHost();
    }
};

QTEST_MAIN(CommunicationRoutingTest)
#include "communication_routing_test.moc"
