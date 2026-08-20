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

#include <QHash>
#include <QMutex>
#include <QMutexLocker>

/**
 * @brief 通信模块命名空间
 * 
 * 提供工厂方法创建通信接口实例
 */
namespace Communication {

// 进程内 RTU 端口 owner。ponytail: one process-wide mutex; per-port locks if throughput matters.
inline QString normalizedRtuPortName(QString portName)
{
    portName = portName.trimmed();
#ifdef Q_OS_WIN
    return portName.toLower();
#else
    return portName;
#endif
}

inline QMutex& rtuPortClaimMutex()
{
    static QMutex mutex;
    return mutex;
}

inline QHash<QString, QString>& rtuPortClaims()
{
    static QHash<QString, QString> claims;
    return claims;
}

inline bool tryClaimRtuPort(const QString& portName,
                            const QString& ownerToken,
                            QString* currentOwner = nullptr)
{
    const QString port = normalizedRtuPortName(portName);
    const QString owner = ownerToken.trimmed();
    if (currentOwner) {
        currentOwner->clear();
    }
    if (port.isEmpty() || owner.isEmpty()) {
        return false;
    }

    QMutexLocker lock(&rtuPortClaimMutex());
    const auto it = rtuPortClaims().constFind(port);
    if (it != rtuPortClaims().constEnd() && it.value() != owner) {
        if (currentOwner) {
            *currentOwner = it.value();
        }
        return false;
    }
    rtuPortClaims().insert(port, owner);
    return true;
}

inline void releaseRtuPort(const QString& portName, const QString& ownerToken)
{
    const QString port = normalizedRtuPortName(portName);
    const QString owner = ownerToken.trimmed();
    if (port.isEmpty() || owner.isEmpty()) {
        return;
    }

    QMutexLocker lock(&rtuPortClaimMutex());
    const auto it = rtuPortClaims().constFind(port);
    if (it != rtuPortClaims().constEnd() && it.value() == owner) {
        rtuPortClaims().erase(it);
    }
}

inline QString currentRtuPortOwner(const QString& portName)
{
    QMutexLocker lock(&rtuPortClaimMutex());
    return rtuPortClaims().value(normalizedRtuPortName(portName));
}

struct ResolvedCommConfig
{
    CommProtocolType type = CommProtocolType::Unknown;
    QVariantMap parameters;

    bool isValid() const { return type != CommProtocolType::Unknown; }
};

namespace Detail {

inline QString normalizedToken(QString value)
{
    value = value.trimmed().toUpper();
    value.remove(QLatin1Char('-'));
    value.remove(QLatin1Char('_'));
    value.remove(QLatin1Char(' '));
    return value;
}

inline void mergeMissing(QVariantMap& target, const QVariantMap& source)
{
    for (auto it = source.constBegin(); it != source.constEnd(); ++it) {
        if (!target.contains(it.key())) {
            target.insert(it.key(), it.value());
        }
    }
}

} // namespace Detail

/**
 * @brief 将历史配置、强类型配置和扁平配置解析为统一的后端配置。
 *
 * 顶层明确协议优先于字段推断；嵌套 serial/modbus/ethernet 参数只补充缺失值。
 */
inline ResolvedCommConfig resolveConfig(const QVariantMap& config)
{
    ResolvedCommConfig resolved;
    resolved.parameters = config;

    const QVariantMap parameters = config.value(QStringLiteral("parameters")).toMap();
    const QVariantMap commParameters = config.value(QStringLiteral("commParameters")).toMap();
    const QVariantMap serial = config.value(QStringLiteral("serial")).toMap();
    const QVariantMap modbus = config.value(QStringLiteral("modbus")).toMap();
    const QVariantMap ethernet = config.value(QStringLiteral("ethernet")).toMap();
    Detail::mergeMissing(resolved.parameters, parameters);
    Detail::mergeMissing(resolved.parameters, commParameters);
    Detail::mergeMissing(resolved.parameters, serial);
    Detail::mergeMissing(resolved.parameters, modbus);
    Detail::mergeMissing(resolved.parameters, ethernet);

    if (!resolved.parameters.contains(QStringLiteral("port"))) {
        const QString port = serial.value(
                QStringLiteral("port"),
                serial.value(QStringLiteral("portName"),
                             resolved.parameters.value(QStringLiteral("portName")))).toString();
        if (!port.isEmpty()) {
            resolved.parameters.insert(QStringLiteral("port"), port);
        }
    }
    if (!resolved.parameters.contains(QStringLiteral("responseTimeout"))) {
        const QVariant timeout = modbus.value(QStringLiteral("responseTimeout"),
                                              modbus.value(
                                                      QStringLiteral("timeoutMs"),
                                                      resolved.parameters.value(
                                                              QStringLiteral("timeoutMs"))));
        if (timeout.isValid()) {
            resolved.parameters.insert(QStringLiteral("responseTimeout"), timeout);
        }
    }

    const QString protocol = Detail::normalizedToken(
            resolved.parameters.value(QStringLiteral("protocol")).toString());
    QString mode = Detail::normalizedToken(
            resolved.parameters.value(QStringLiteral("mode")).toString());

    if (protocol == QStringLiteral("MODBUSRTU") || protocol == QStringLiteral("RTU")) {
        resolved.type = CommProtocolType::ModbusRTU;
        mode = QStringLiteral("RTU");
    } else if (protocol == QStringLiteral("MODBUSTCP")) {
        resolved.type = CommProtocolType::ModbusTCP;
        mode = QStringLiteral("TCP");
    } else if (protocol == QStringLiteral("MODBUS")) {
        resolved.type = mode == QStringLiteral("TCP")
                ? CommProtocolType::ModbusTCP
                : CommProtocolType::ModbusRTU;
        mode = resolved.type == CommProtocolType::ModbusTCP
                ? QStringLiteral("TCP")
                : QStringLiteral("RTU");
    } else if (protocol == QStringLiteral("SERIAL")
               || protocol == QStringLiteral("RS232")
               || protocol == QStringLiteral("RS485")) {
        if (mode == QStringLiteral("MODBUS") || mode == QStringLiteral("RTU")) {
            resolved.type = CommProtocolType::ModbusRTU;
            mode = QStringLiteral("RTU");
        } else {
            resolved.type = CommProtocolType::Serial;
        }
    } else if (protocol == QStringLiteral("ETHERNET")) {
        resolved.type = mode == QStringLiteral("UDP")
                ? CommProtocolType::EthernetUDP
                : CommProtocolType::EthernetTCP;
    } else if (protocol == QStringLiteral("TCP")) {
        resolved.type = CommProtocolType::EthernetTCP;
        mode = QStringLiteral("TCP");
    } else if (protocol == QStringLiteral("UDP")) {
        resolved.type = CommProtocolType::EthernetUDP;
        mode = QStringLiteral("UDP");
    } else if (protocol == QStringLiteral("CAN")) {
        resolved.type = CommProtocolType::CAN;
    } else if (protocol == QStringLiteral("CANOPEN")) {
        resolved.type = CommProtocolType::CANOpen;
    } else if (protocol == QStringLiteral("J1939")) {
        resolved.type = CommProtocolType::J1939;
    } else if (protocol == QStringLiteral("AUTO")) {
        resolved.type = CommProtocolType::Serial;
    } else if (protocol == QStringLiteral("RAW") || protocol == QStringLiteral("CUSTOM")) {
        resolved.type = CommProtocolType::Serial;
    }

    if (resolved.type == CommProtocolType::Unknown && protocol.isEmpty()) {
        const bool hasTcpHint = mode == QStringLiteral("TCP")
                || resolved.parameters.contains(QStringLiteral("tcpPort"));
        if (!modbus.isEmpty()
                || mode == QStringLiteral("RTU")
                || mode == QStringLiteral("MODBUS")
                || resolved.parameters.contains(QStringLiteral("tcpPort"))) {
            resolved.type = hasTcpHint
                    ? CommProtocolType::ModbusTCP
                    : CommProtocolType::ModbusRTU;
            mode = resolved.type == CommProtocolType::ModbusTCP
                    ? QStringLiteral("TCP")
                    : QStringLiteral("RTU");
        } else if (resolved.parameters.contains(QStringLiteral("interface"))) {
            resolved.type = CommProtocolType::CAN;
        } else if (resolved.parameters.contains(QStringLiteral("baudRate"))) {
            resolved.type = CommProtocolType::Serial;
        } else if (resolved.parameters.contains(QStringLiteral("host"))) {
            resolved.type = mode == QStringLiteral("UDP")
                    ? CommProtocolType::EthernetUDP
                    : CommProtocolType::EthernetTCP;
        }
    }

    switch (resolved.type) {
    case CommProtocolType::ModbusRTU:
        resolved.parameters.insert(QStringLiteral("protocol"), QStringLiteral("MODBUS"));
        resolved.parameters.insert(QStringLiteral("mode"), QStringLiteral("RTU"));
        break;
    case CommProtocolType::ModbusTCP:
        resolved.parameters.insert(QStringLiteral("protocol"), QStringLiteral("MODBUS"));
        resolved.parameters.insert(QStringLiteral("mode"), QStringLiteral("TCP"));
        break;
    case CommProtocolType::Serial:
        resolved.parameters.insert(QStringLiteral("protocol"), QStringLiteral("SERIAL"));
        break;
    case CommProtocolType::EthernetUDP:
        resolved.parameters.insert(QStringLiteral("protocol"), QStringLiteral("UDP"));
        resolved.parameters.insert(QStringLiteral("mode"), QStringLiteral("UDP"));
        break;
    case CommProtocolType::EthernetTCP:
        resolved.parameters.insert(QStringLiteral("protocol"), QStringLiteral("TCP"));
        resolved.parameters.insert(QStringLiteral("mode"), QStringLiteral("TCP"));
        break;
    default:
        break;
    }

    return resolved;
}

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
    const ResolvedCommConfig resolved = resolveConfig(config);
    if (!resolved.isValid()) {
        return nullptr;
    }

    ICommInterface* interface = createInterface(resolved.type, parent);
    if (!interface) {
        return nullptr;
    }

    if (!interface->open(resolved.parameters)) {
        delete interface;
        return nullptr;
    }

    // --------------------------------------------------------------------
    // ControllerBridge 自动接入（仅对 Modbus 生效）
    // 目的：open() 后自动 handshake/probe，并通过 errorOccurred 明确区分三层问题
    // --------------------------------------------------------------------
    if (resolved.type == CommProtocolType::ModbusRTU
            || resolved.type == CommProtocolType::ModbusTCP) {
        auto* modbus = qobject_cast<ModbusInterface*>(interface);
        if (modbus) {
            QVariantMap bridgeCfg;
            if (resolved.parameters.contains("bridge")) {
                bridgeCfg = resolved.parameters.value("bridge").toMap();
            } else {
                // 兼容：允许把 handshake/targetProbe 直接放在顶层
                if (resolved.parameters.contains("handshake")) bridgeCfg.insert("handshake", resolved.parameters.value("handshake").toMap());
                if (resolved.parameters.contains("targetProbe")) bridgeCfg.insert("targetProbe", resolved.parameters.value("targetProbe").toMap());
                if (resolved.parameters.contains("enableHandshake")) bridgeCfg.insert("enableHandshake", resolved.parameters.value("enableHandshake"));
                if (resolved.parameters.contains("enableTargetProbe")) bridgeCfg.insert("enableTargetProbe", resolved.parameters.value("enableTargetProbe"));
            }

            const bool enableBridge = resolved.parameters.value("enableBridge", !bridgeCfg.isEmpty()).toBool();
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
