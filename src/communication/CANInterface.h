// 文件：src/communication/CANInterface.h
// CAN 通信接口 - 原始 CAN 通信实现

#ifndef CANINTERFACE_H
#define CANINTERFACE_H

#include "ICommInterface.h"
#include "CANCommon.h"
#include "CommTypes.h"
#include <QCanBusDevice>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>
#include <optional>

/**
 * @brief CAN 总线通信接口
 * 
 * 提供原始 CAN 帧的收发功能，支持标准帧和扩展帧。
 * 子类 CANOpenInterface 和 J1939Interface 提供协议特定功能。
 */
class CANInterface : public ICommInterface
{
    Q_OBJECT
    
public:
    explicit CANInterface(QObject *parent = nullptr);
    ~CANInterface() override;
    
    // ========================================================================
    // ICommInterface 实现
    // ========================================================================
    
    bool open(const QVariantMap& config) override;
    void close() override;
    int send(const QByteArray& data) override;
    QByteArray receive(int timeout_ms) override;
    bool isConnected() const override;
    
    // ========================================================================
    // 强类型配置接口
    // ========================================================================
    
    /**
     * @brief 使用强类型配置打开连接
     */
    bool open(const CanConfig& config);
    
    /**
     * @brief 获取当前配置
     */
    CanConfig currentConfig() const;

signals:
    /**
     * @brief CAN 帧接收信号
     * @param frame 接收到的 CAN 帧
     */
    void frameReceived(const CANMessage& frame);

protected:
    // ========================================================================
    // 子类可用方法
    // ========================================================================
    
    /**
     * @brief 发送 CAN 帧
     * @param frame 要发送的帧
     * @return 成功返回 true
     */
    bool sendFrame(const CANMessage& frame);
    
    /**
     * @brief 从接收缓冲区取出一帧（带超时）
     * @param timeout_ms 超时时间
     * @return 接收到的帧，超时返回 nullopt
     */
    std::optional<CANMessage> takeFrame(int timeout_ms);
    
    /**
     * @brief 检查缓冲区是否有数据
     */
    bool hasFrameAvailable() const;
    
private slots:
    void onFramesReceived();
    void onDeviceError(QCanBusDevice::CanBusError error);
    void onDeviceStateChanged(QCanBusDevice::CanBusDeviceState state);
    
private:
    QCanBusDevice* m_device;
    QQueue<CANMessage> m_receiveBuffer;
    mutable QMutex m_receiveMutex;
    QWaitCondition m_frameAvailable;
    
    CanConfig m_config;
    
    static constexpr int MAX_BUFFER_SIZE = 1000;
};

#endif // CANINTERFACE_H
