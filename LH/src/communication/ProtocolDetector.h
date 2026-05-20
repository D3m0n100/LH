#ifndef PROTOCOL_DETECTOR_H
#define PROTOCOL_DETECTOR_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include "ICommInterface.h"

/**
 * @brief 协议自动检测器
 * 智能识别通信协议类型，支持多种工业协议
 */
class ProtocolDetector : public QObject
{
    Q_OBJECT

public:
    enum class ProtocolType {
        Unknown,
        ModbusRTU,
        ModbusTCP,
        CANOpen,
        J1939,
        RawCAN,
        Custom
    };

    struct ProtocolInfo {
        ProtocolType type;
        QString name;
        QString description;
        QVariantMap detectedParams;
        int confidence; // 0-100
    };

    explicit ProtocolDetector(QObject* parent = nullptr);
    ~ProtocolDetector() override;

    // 协议检测
    ProtocolInfo detectProtocol(ICommInterface* interface, int timeout_ms = 5000);
    
    // 批量检测
    QList<ProtocolInfo> detectAllProtocols(ICommInterface* interface, int timeout_ms = 5000);
    
    // 验证协议
    bool verifyProtocol(ICommInterface* interface, ProtocolType type, int timeout_ms = 2000);

    // 获取支持的协议列表
    static QList<ProtocolType> getSupportedProtocols();
    static QString protocolTypeToString(ProtocolType type);
    static ProtocolType stringToProtocolType(const QString& str);

signals:
    void detectionProgress(int percentage);
    void protocolDetected(const ProtocolInfo& info);
    void detectionFailed(const QString& error);

private slots:
    void onDetectionTimeout();

private:
    // 各种协议的检测函数
    ProtocolInfo detectModbusRTU(ICommInterface* interface);
    ProtocolInfo detectModbusTCP(ICommInterface* interface);
    ProtocolInfo detectCANOpen(ICommInterface* interface);
    ProtocolInfo detectJ1939(ICommInterface* interface);
    ProtocolInfo detectRawCAN(ICommInterface* interface);
    
    // 协议特征分析
    bool analyzeModbusRTUFrame(const QByteArray& data);
    bool analyzeModbusTCPFrame(const QByteArray& data);
    bool analyzeCANOpenFrame(const QByteArray& data);
    bool analyzeJ1939Frame(const QByteArray& data);
    
    // 辅助函数
    quint16 calculateCRC16(const QByteArray& data);
    bool validateCANChecksum(const QByteArray& data);
    
private:
    QTimer* m_detectionTimer;
    QElapsedTimer m_elapsedTimer;
    ICommInterface* m_currentInterface;
    
    // 检测参数
    int m_timeout;
    bool m_detectionActive;
    
    // 统计信息
    int m_totalTests;
    int m_successfulTests;
};

#endif // PROTOCOL_DETECTOR_H
