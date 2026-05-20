// 文件：src/communication/Communication.h
// 通信模块统一头文件 - 包含所有通信接口

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

// 基础类型和配置
#include "CommTypes.h"
#include "Common.h"

// 接口基类
#include "ICommInterface.h"

// CAN 相关
#include "CANCommon.h"
#include "CANInterface.h"
#include "CANOpenInterface.h"
#include "J1939Interface.h"

// 串口
#include "SerialInterface.h"

// Modbus
#include "ModbusInterface.h"
#include "ControllerBridge.h"

// 以太网
#include "EthernetInterface.h"

/**
 * @brief 通信模块命名空间
 * 
 * 提供工厂方法创建通信接口实例
 */
namespace Communication {

/**
 * @brief 根据协议类型创建通信接口
 * @param type 协议类型
 * @param parent 父对象
 * @return 通信接口指针，失败返回 nullptr
 */
inline ICommInterface* createInterface(CommProtocolType type, QObject* parent = nullptr)
{
    switch (type) {
        case CommProtocolType::Serial:
            return new SerialInterface(parent);
            
        case CommProtocolType::ModbusRTU:
        case CommProtocolType::ModbusTCP:
            return new ModbusInterface(parent);
            
        case CommProtocolType::CAN:
            return new CANInterface(parent);
            
        case CommProtocolType::CANOpen:
            return new CANOpenInterface(parent);
            
        case CommProtocolType::J1939:
            return new J1939Interface(parent);
            
        case CommProtocolType::EthernetTCP:
        case CommProtocolType::EthernetUDP:
            return new EthernetInterface(parent);
            
        default:
            return nullptr;
    }
}

/**
 * @brief 根据配置自动创建并打开通信接口
 * @param config 配置参数
 * @param parent 父对象
 * @return 已打开的通信接口指针，失败返回 nullptr
 */
inline ICommInterface* createAndOpen(const QVariantMap& config, QObject* parent = nullptr)
{
    // 从配置中推断协议类型（尽量兼容旧配置写法）
    const QString protocol = config.value("protocol", "").toString().toUpper();
    const QString mode = config.value("mode", "").toString().toUpper();

    CommProtocolType type = CommProtocolType::Unknown;

    if (protocol == "SERIAL" || config.contains("baudRate")) {
        // 只要含有串口字段，默认按串口类；若 mode 指示 RTU/MODBUS 则按 ModbusRTU
        if (mode == "MODBUS" || mode == "RTU") {
            type = CommProtocolType::ModbusRTU;
        } else {
            type = CommProtocolType::Serial;
        }
    } else if (protocol == "MODBUS") {
        type = (mode == "TCP") ? CommProtocolType::ModbusTCP : CommProtocolType::ModbusRTU;
    } else if (protocol == "CAN") {
        type = CommProtocolType::CAN;
    } else if (protocol == "CANOPEN") {
        type = CommProtocolType::CANOpen;
    } else if (protocol == "J1939") {
        type = CommProtocolType::J1939;
    } else if (protocol == "ETHERNET") {
        type = (mode == "UDP") ? CommProtocolType::EthernetUDP : CommProtocolType::EthernetTCP;
    } else if (config.contains("interface") && !config.contains("tcpPort")) {
        // CAN 配置通常包含 interface=can0 等
        type = CommProtocolType::CAN;
    } else if (config.contains("host") || config.contains("tcpPort")) {
        type = CommProtocolType::EthernetTCP;
    }

    if (type == CommProtocolType::Unknown) {
        return nullptr;
    }

    ICommInterface* interface = createInterface(type, parent);
    if (!interface) {
        return nullptr;
    }

    if (!interface->open(config)) {
        delete interface;
        return nullptr;
    }

    // --------------------------------------------------------------------
    // ControllerBridge 自动接入（仅对 Modbus 生效）
    // 目的：open() 后自动 handshake/probe，并通过 errorOccurred 明确区分三层问题
    // --------------------------------------------------------------------
    if (type == CommProtocolType::ModbusRTU || type == CommProtocolType::ModbusTCP) {
        auto* modbus = qobject_cast<ModbusInterface*>(interface);
        if (modbus) {
            QVariantMap bridgeCfg;
            if (config.contains("bridge")) {
                bridgeCfg = config.value("bridge").toMap();
            } else {
                // 兼容：允许把 handshake/targetProbe 直接放在顶层
                if (config.contains("handshake")) bridgeCfg.insert("handshake", config.value("handshake").toMap());
                if (config.contains("targetProbe")) bridgeCfg.insert("targetProbe", config.value("targetProbe").toMap());
                if (config.contains("enableHandshake")) bridgeCfg.insert("enableHandshake", config.value("enableHandshake"));
                if (config.contains("enableTargetProbe")) bridgeCfg.insert("enableTargetProbe", config.value("enableTargetProbe"));
            }

            const bool enableBridge = config.value("enableBridge", !bridgeCfg.isEmpty()).toBool();
            if (enableBridge && !bridgeCfg.isEmpty()) {
                auto* bridge = new ControllerBridge(modbus, interface); // parent=interface，生命周期跟随
                bridge->setBridgeConfig(bridgeCfg);

                // 让 UI/上层可通过参数查询状态（无额外信号时也能读到）
                interface->setParameter("controllerOnline", false);
                interface->setParameter("targetOnline", false);

                QObject::connect(bridge, &ControllerBridge::controllerOnlineChanged, interface,
                                 [interface](bool online) { interface->setParameter("controllerOnline", online); });
                QObject::connect(bridge, &ControllerBridge::targetOnlineChanged, interface,
                                 [interface](bool online) { interface->setParameter("targetOnline", online); });

                // open 后自动握手/探测：失败则返回 nullptr（同时已通过 errorOccurred 上报语义化错误）
                if (!bridge->handshake()) {
                    // 串口已打开，但控制器离线：保持接口对象可用，便于 UI 查询 lastError/参数并允许后续重试
                    interface->setParameter("bridgeStage", "controllerOffline");
                    interface->setParameter("controllerOnline", false);
                    interface->setParameter("targetOnline", false);
                    return interface;
                }
                if (!bridge->probeTarget()) {
                    // 控制器在线，但 CAN 侧目标无响应：同样保留接口对象
                    interface->setParameter("bridgeStage", "targetOffline");
                    interface->setParameter("targetOnline", false);
                    return interface;
                }
                interface->setParameter("bridgeStage", "ok");
            }
        }
    }

    return interface;
}

/**
 * @brief 获取可用的串口列表
 */
QStringList availableSerialPorts();

/**
 * @brief 获取可用的 CAN 接口列表
 */
QStringList availableCANInterfaces();

/**
 * @brief 注册所有元类型 (在应用程序启动时调用)
 */

/**
 * @brief 从已创建的接口上获取桥接层（若已启用）
 */
inline ControllerBridge* getControllerBridge(ICommInterface* interface)
{
    return interface ? interface->findChild<ControllerBridge*>() : nullptr;
}

inline void registerMetaTypes()
{
    qRegisterMetaType<CommError>("CommError");
    qRegisterMetaType<CommErrorCode>("CommErrorCode");
    qRegisterMetaType<CommProtocolType>("CommProtocolType");
    qRegisterMetaType<CANMessage>("CANMessage");
    qRegisterMetaType<CANOpenFrame>("CANOpenFrame");
    qRegisterMetaType<J1939Frame>("J1939Frame");
}

} // namespace Communication

#endif // COMMUNICATION_H
