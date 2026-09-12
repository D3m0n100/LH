// 文件：src/communication/CANOpenInterface.h
// CANopen 协议接口

#ifndef CANOPENINTERFACE_H
#define CANOPENINTERFACE_H

#include "CANInterface.h"
#include "CommTypes.h"
#include <QJsonObject>

/**
 * @brief CANopen 协议通信接口
 * 
 * 继承自 CANInterface，提供 CANopen 协议特定功能：
 * - COB-ID 管理
 * - 节点 ID 和功能码处理
 * - SDO/PDO 帧解析
 */
class CANOpenInterface : public CANInterface
{
    Q_OBJECT

public:
    explicit CANOpenInterface(QObject* parent = nullptr);

    // ========================================================================
    // ICommInterface 重写
    // ========================================================================
    
    bool open(const QVariantMap& config) override;
    int send(const QByteArray& data) override;
    QByteArray receive(int timeout_ms) override;
    
    // ========================================================================
    // 强类型配置接口
    // ========================================================================
    
    bool open(const CANOpenConfig& config);
    CANOpenConfig currentCANOpenConfig() const { return m_canopenConfig; }
    
    // ========================================================================
    // CANopen 特有接口
    // ========================================================================
    
    bool sendCANOpenFrame(quint16 cobId, const QByteArray& data);
    
    void setNodeId(quint8 nodeId) { m_canopenConfig.nodeId = nodeId; }
    quint8 nodeId() const { return m_canopenConfig.nodeId; }
    
    void setFunctionCode(quint8 funcCode) { m_canopenConfig.functionCode = funcCode; }
    quint8 functionCode() const { return m_canopenConfig.functionCode; }

signals:
    void canOpenFrameReceived(quint16 cobId,
                              quint8 functionCode,
                              quint8 nodeId,
                              const QByteArray& payload);

private slots:
    void onRawFrameReceived(const CANMessage& frame);

private:
    CANOpenConfig m_canopenConfig;
    quint16 m_defaultCobId;
};

#endif // CANOPENINTERFACE_H
