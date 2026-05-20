#include "CommFactory.h"
#include "CANInterface.h"
#include "CANOpenInterface.h"
#include "EthernetInterface.h"
#include "J1939Interface.h"
#include "ModbusInterface.h"
#include "SerialInterface.h"
#include "ProtocolDetector.h"
#include "Common.h"
#include "Communication.h"

ICommInterface* CommFactory::create(const QString& type, QObject* parent)
{
    QString lowerType = type.toLower();
    
    if (lowerType == "can") {
        return new CANInterface(parent);
    } else if (lowerType == "canopen") {
        return new CANOpenInterface(parent);
    } else if (lowerType == "j1939") {
        return new J1939Interface(parent);
    } else if (lowerType == "ethernet" || lowerType == "tcp") {
        return new EthernetInterface(parent);
    } else if (lowerType == "modbusrtu") {
        return new ModbusInterface(parent);
    } else if (lowerType == "modbustcp") {
        return new ModbusInterface(parent);
    } else if (lowerType == "serial" || lowerType == "rs232" || lowerType == "rs485") {
        return new SerialInterface(parent);
    } else if (lowerType == "auto") {
        // 避免调用方收到空指针导致崩溃，默认返回串口接口作为自动检测入口。
        LOG_WARN("Protocol type 'auto' is routed to SerialInterface. Use createProtocolDetector() for full detection flow.");
        return new SerialInterface(parent);
    }
    
    LOG_WARN(QString("Unknown communication protocol type: %1").arg(type));
    return nullptr;
}

ICommInterface* CommFactory::createAndOpen(const QVariantMap& config, QObject* parent, ControllerBridge** outBridge)
{
    ICommInterface* iface = Communication::createAndOpen(config, parent);
    if (outBridge) {
        *outBridge = Communication::getControllerBridge(iface);
    }
    return iface;
}


// 创建协议检测器实例
ProtocolDetector* CommFactory::createProtocolDetector(QObject* parent)
{
    return new ProtocolDetector(parent);
}

// 获取所有支持的协议类型
QStringList CommFactory::getSupportedProtocols()
{
    return {
        "CAN",
        "CANOpen",
        "J1939",
        "Ethernet",
        "TCP",
        "ModbusRTU",
        "ModbusTCP",
        "Serial",
        "RS232",
        "RS485",
        "Auto"
    };
}
