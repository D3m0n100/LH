// 文件：src/communication/J1939Interface.h
// SAE J1939 协议接口

#ifndef J1939INTERFACE_H
#define J1939INTERFACE_H

#include "CANInterface.h"
#include "CommTypes.h"
#include <optional>

/**
 * @brief SAE J1939 协议通信接口
 * 
 * 继承自 CANInterface，提供 J1939 协议特定功能：
 * - 29-bit 扩展帧标识符处理
 * - PGN (参数组编号) 管理
 * - 优先级和地址处理
 * - PDU1/PDU2 格式支持
 */
class J1939Interface : public CANInterface
{
    Q_OBJECT

public:
    explicit J1939Interface(QObject* parent = nullptr);

    // ========================================================================
    // ICommInterface 重写
    // ========================================================================
    
    bool open(const QVariantMap& config) override;
    int send(const QByteArray& data) override;
    QByteArray receive(int timeout_ms) override;
    
    // ========================================================================
    // 强类型配置接口
    // ========================================================================
    
    bool open(const J1939Config& config);
    J1939Config currentJ1939Config() const { return m_j1939Config; }
    
    // ========================================================================
    // J1939 特有接口
    // ========================================================================
    
    bool sendJ1939Frame(const J1939Frame& frame);
    bool sendToPGN(quint32 pgn, const QByteArray& data, 
                   std::optional<quint8> destAddr = std::nullopt);
    
    void setSourceAddress(quint8 addr) { m_j1939Config.sourceAddress = addr; }
    quint8 sourceAddress() const { return m_j1939Config.sourceAddress; }
    
    void setPriority(quint8 priority) { m_j1939Config.priority = priority & 0x07; }
    quint8 priority() const { return m_j1939Config.priority; }
    
    void setDefaultPGN(quint32 pgn) { m_j1939Config.pgn = pgn; }
    quint32 defaultPGN() const { return m_j1939Config.pgn; }

signals:
    void j1939FrameReceived(const J1939Frame& frame);

private slots:
    void onRawFrameReceived(const CANMessage& frame);

private:
    J1939Config m_j1939Config;
};

#endif // J1939INTERFACE_H
