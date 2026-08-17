/**
 * @file serial_interface_test.cpp
 * @brief 串口接收缓冲与信号重入回归测试。
 */

#include <QtTest/QtTest>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QSerialPort>
#include <QSignalSpy>
#include <QTimer>
#include <QWaitCondition>

#include "communication/CommTypes.h"
#include "communication/ICommInterface.h"

#define private public
#include "communication/SerialInterface.h"
#undef private

class SerialInterfaceTest : public QObject
{
    Q_OBJECT

private:
    static void setBuffers(SerialInterface& interface,
                           const QByteArray& receiveData,
                           const QByteArray& frameData)
    {
        QMutexLocker locker(&interface.m_bufferMutex);
        interface.m_receiveBuffer = receiveData;
        interface.m_frameBuffer = frameData;
    }

private slots:
    void frameBufferIsConsumedBetweenEmissions()
    {
        SerialInterface interface;
        interface.setProtocol(SerialInterface::SerialProtocol::Raw);
        QSignalSpy spy(&interface, &SerialInterface::dataFramed);

        setBuffers(interface, QByteArray(), QByteArrayLiteral("first"));
        interface.processReceivedData();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toByteArray(), QByteArrayLiteral("first"));
        QVERIFY(interface.m_frameBuffer.isEmpty());

        setBuffers(interface, QByteArray(), QByteArrayLiteral("second"));
        interface.processReceivedData();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toByteArray(), QByteArrayLiteral("second"));
        QVERIFY(interface.m_frameBuffer.isEmpty());
    }

    void rawReceiveDoesNotConsumeFrameBuffer()
    {
        SerialInterface interface;
        interface.setProtocol(SerialInterface::SerialProtocol::Raw);
        QSignalSpy spy(&interface, &SerialInterface::dataFramed);
        const QByteArray payload = QByteArrayLiteral("payload");
        setBuffers(interface, payload, payload);

        QCOMPARE(interface.receive(0), payload);
        QCOMPARE(interface.m_frameBuffer, payload);

        interface.processReceivedData();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toByteArray(), payload);
        QVERIFY(interface.m_frameBuffer.isEmpty());
    }

    void dataFramedCanReenterReceive()
    {
        SerialInterface interface;
        interface.setProtocol(SerialInterface::SerialProtocol::Raw);
        const QByteArray payload = QByteArrayLiteral("reentrant");
        QByteArray receivedInSlot;
        connect(&interface,
                &SerialInterface::dataFramed,
                &interface,
                [&interface, &receivedInSlot](const QByteArray&) {
                    receivedInSlot = interface.receive(0);
                },
                Qt::DirectConnection);
        setBuffers(interface, payload, payload);

        interface.processReceivedData();

        QCOMPARE(receivedInSlot, payload);
        QVERIFY(interface.m_frameBuffer.isEmpty());
    }

    void closeClearsBothBuffersWhenAlreadyClosed()
    {
        SerialInterface interface;
        setBuffers(interface, QByteArrayLiteral("raw"), QByteArrayLiteral("frame"));

        interface.close();

        QVERIFY(interface.m_receiveBuffer.isEmpty());
        QVERIFY(interface.m_frameBuffer.isEmpty());
    }

    void receiveBuffersAreBoundedAt64KiB()
    {
        SerialInterface interface;
        const QByteArray oversized(SerialInterface::MAX_BUFFER_SIZE + 1024, 'x');
        setBuffers(interface, oversized, oversized);
        interface.setProtocol(SerialInterface::SerialProtocol::Raw);
        interface.processReceivedData();
        QVERIFY(interface.m_frameBuffer.size() <= SerialInterface::MAX_BUFFER_SIZE);
    }
};

QTEST_APPLESS_MAIN(SerialInterfaceTest)
#include "serial_interface_test.moc"
