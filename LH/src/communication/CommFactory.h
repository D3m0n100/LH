#ifndef COMMFACTORY_H
#define COMMFACTORY_H

#include "ICommInterface.h"

#include <QVariantMap>

// 前向声明
class ProtocolDetector;
class ControllerBridge;

class CommFactory
{
public:
    // 旧接口：按字符串协议创建（未打开）
    static ICommInterface* create(const QString& type, QObject* parent = nullptr);

    // 新接口：按配置创建并打开；若为 Modbus 且配置了 bridge，则自动 handshake/probe。
    // outBridge 可选：返回已挂载到 interface（parent=interface）的 ControllerBridge 指针。
    static ICommInterface* createAndOpen(const QVariantMap& config, QObject* parent = nullptr, ControllerBridge** outBridge = nullptr);

    static ProtocolDetector* createProtocolDetector(QObject* parent = nullptr);
    static QStringList getSupportedProtocols();
};

#endif // COMMFACTORY_H
