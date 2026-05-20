#include <QtTest/QtTest>

#include <functional>

#include "communication/ProtocolDetector.h"

class FakeCommInterface : public ICommInterface
{
    Q_OBJECT

public:
    explicit FakeCommInterface(QObject* parent = nullptr)
        : ICommInterface(parent)
    {
    }

    bool open(const QVariantMap& config) override
    {
        Q_UNUSED(config);
        m_connected = true;
        return true;
    }

    void close() override
    {
        m_connected = false;
    }

    int send(const QByteArray& data) override
    {
        m_lastSent = data;
        if (m_responseProvider) {
            m_pendingResponse = m_responseProvider(data);
        }
        return data.size();
    }

    QByteArray receive(int timeout_ms = 1000) override
    {
        Q_UNUSED(timeout_ms);
        const QByteArray response = m_pendingResponse;
        m_pendingResponse.clear();
        return response;
    }

    bool isConnected() const override
    {
        return m_connected;
    }

    void setResponseProvider(const std::function<QByteArray(const QByteArray&)>& provider)
    {
        m_responseProvider = provider;
    }

    QByteArray lastSent() const
    {
        return m_lastSent;
    }

private:
    bool m_connected = true;
    QByteArray m_lastSent;
    QByteArray m_pendingResponse;
    std::function<QByteArray(const QByteArray&)> m_responseProvider;
};

class ProtocolDetectorTest : public QObject
{
    Q_OBJECT

private slots:
    void testProtocolTypeRoundTrip()
    {
        const QList<ProtocolDetector::ProtocolType> protocols = {
            ProtocolDetector::ProtocolType::ModbusRTU,
            ProtocolDetector::ProtocolType::ModbusTCP,
            ProtocolDetector::ProtocolType::CANOpen,
            ProtocolDetector::ProtocolType::J1939,
            ProtocolDetector::ProtocolType::RawCAN
        };

        for (ProtocolDetector::ProtocolType protocol : protocols) {
            const QString text = ProtocolDetector::protocolTypeToString(protocol);
            QCOMPARE(ProtocolDetector::stringToProtocolType(text), protocol);
            QCOMPARE(ProtocolDetector::stringToProtocolType(text.toLower()), protocol);
        }

        QCOMPARE(ProtocolDetector::protocolTypeToString(ProtocolDetector::ProtocolType::Unknown),
                 QStringLiteral("Unknown"));
        QCOMPARE(ProtocolDetector::stringToProtocolType(QStringLiteral("not-a-protocol")),
                 ProtocolDetector::ProtocolType::Unknown);
    }

    void testSupportedProtocols()
    {
        const QList<ProtocolDetector::ProtocolType> protocols =
                ProtocolDetector::getSupportedProtocols();

        QVERIFY(protocols.contains(ProtocolDetector::ProtocolType::ModbusRTU));
        QVERIFY(protocols.contains(ProtocolDetector::ProtocolType::ModbusTCP));
        QVERIFY(protocols.contains(ProtocolDetector::ProtocolType::CANOpen));
        QVERIFY(protocols.contains(ProtocolDetector::ProtocolType::J1939));
        QVERIFY(protocols.contains(ProtocolDetector::ProtocolType::RawCAN));
        QVERIFY(!protocols.contains(ProtocolDetector::ProtocolType::Unknown));
    }

    void testNullInterfaceHandling()
    {
        ProtocolDetector detector;
        QCOMPARE(detector.verifyProtocol(nullptr, ProtocolDetector::ProtocolType::ModbusTCP, 1),
                 false);

        const auto info = detector.detectProtocol(nullptr, 1);
        QCOMPARE(info.type, ProtocolDetector::ProtocolType::Unknown);
        QCOMPARE(info.confidence, 0);
    }

    void testDetectModbusTcpWithFakeInterface()
    {
        FakeCommInterface fake;
        fake.setResponseProvider([](const QByteArray& request) {
            if (request.size() >= 12
                    && static_cast<quint8>(request[0]) == 0x00
                    && static_cast<quint8>(request[1]) == 0x01
                    && static_cast<quint8>(request[2]) == 0x00
                    && static_cast<quint8>(request[3]) == 0x00) {
                QByteArray response;
                response.append(char(0x00));
                response.append(char(0x01));
                response.append(char(0x00));
                response.append(char(0x00));
                response.append(char(0x00));
                response.append(char(0x06));
                response.append(char(0x01));
                response.append(char(0x03));
                response.append(char(0x02));
                response.append(char(0x00));
                response.append(char(0x01));
                response.append(char(0x00));
                return response;
            }
            return QByteArray();
        });

        ProtocolDetector detector;
        const auto info = detector.detectProtocol(&fake, 100);

        QCOMPARE(info.type, ProtocolDetector::ProtocolType::ModbusTCP);
        QCOMPARE(info.confidence, 90);
        QCOMPARE(info.detectedParams.value(QStringLiteral("transactionId")).toInt(), 1);
        QCOMPARE(info.detectedParams.value(QStringLiteral("protocolId")).toInt(), 0);
        QCOMPARE(info.detectedParams.value(QStringLiteral("length")).toInt(), 6);
        QCOMPARE(info.detectedParams.value(QStringLiteral("unitId")).toInt(), 1);
    }
};

QTEST_MAIN(ProtocolDetectorTest)
#include "protocol_detector_test.moc"
