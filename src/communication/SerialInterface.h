// 文件：src/communication/SerialInterface.h
// 串口通信接口

#ifndef SERIALINTERFACE_H
#define SERIALINTERFACE_H

#include "ICommInterface.h"
#include "CommTypes.h"
#include <QSerialPort>
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>
#include <QWaitCondition>

/**
 * @brief 串口通信接口
 * 
 * 支持 RS232/RS485 通信，提供：
 * - 波特率自动检测
 * - 多种协议模式 (Raw / ModbusRTU / Custom)
 * - RS485 方向控制
 * - 异步数据接收
 */
class SerialInterface : public ICommInterface
{
    Q_OBJECT

public:
    enum class SerialProtocol {
        Raw,
        ModbusRTU,
        Custom
    };

    explicit SerialInterface(QObject* parent = nullptr);
    ~SerialInterface() override;

    // ========================================================================
    // ICommInterface 实现
    // ========================================================================
    
    bool open(const QVariantMap& config) override;
    void close() override;
    int send(const QByteArray& data) override;
    QByteArray receive(int timeout_ms = 1000) override;
    bool isConnected() const override;

    // ========================================================================
    // 强类型配置接口
    // ========================================================================
    
    bool open(const SerialConfig& config);
    SerialConfig currentConfig() const { return m_serialConfig; }

    // ========================================================================
    // 串口特有接口
    // ========================================================================
    
    bool setBaudRate(int baudRate);
    bool setDataBits(int dataBits);
    bool setParity(QSerialPort::Parity parity);
    bool setStopBits(QSerialPort::StopBits stopBits);
    bool setFlowControl(QSerialPort::FlowControl flowControl);
    
    bool autoDetectBaudRate();
    QList<int> getSupportedBaudRates() const;
    
    void setProtocol(SerialProtocol protocol) { m_protocol = protocol; }
    SerialProtocol getProtocol() const { return m_protocol; }

    void setRs485Mode(bool enable) { m_serialConfig.rs485Mode = enable; }
    bool isRs485Mode() const { return m_serialConfig.rs485Mode; }
    
    void setRs485DirectionControl(bool enable) { m_serialConfig.rs485DirectionControl = enable; }
    bool hasRs485DirectionControl() const { return m_serialConfig.rs485DirectionControl; }

    qint64 bytesReceived() const { return m_bytesReceived; }
    qint64 bytesSent() const { return m_bytesSent; }
    int frameErrors() const { return m_frameErrors; }

signals:
    void baudRateDetected(int baudRate);
    void baudRateAutoDetectionFailed();
    void dataFramed(const QByteArray& frame);

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);
    void onBaudRateDetectionTimeout();
    void processReceivedData();

private:
    bool tryBaudRate(int baudRate);
    QByteArray extractFrame(const QByteArray& data);
    bool validateFrame(const QByteArray& frame);
    QSerialPort::Parity parityFromString(const QString& str) const;
    QSerialPort::FlowControl flowControlFromString(const QString& str) const;
    
    QSerialPort* m_serialPort;
    QTimer* m_baudRateDetectionTimer;
    QTimer* m_frameProcessingTimer;
    
    SerialConfig m_serialConfig;
    SerialProtocol m_protocol;
    
    QList<int> m_baudRateCandidates;
    int m_currentBaudRateIndex;
    QByteArray m_detectionBuffer;
    QElapsedTimer m_detectionTimer;
    
    QByteArray m_receiveBuffer;
    mutable QMutex m_bufferMutex;
    QWaitCondition m_receiveWaitCondition;
    
    QByteArray m_frameBuffer;
    
    qint64 m_bytesReceived;
    qint64 m_bytesSent;
    int m_frameErrors;
    
    static constexpr int MAX_BUFFER_SIZE = 65536;
    static constexpr int BAUD_DETECTION_TIMEOUT = 2000;
};

#endif // SERIALINTERFACE_H
