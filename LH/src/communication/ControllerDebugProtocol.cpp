// File: src/communication/ControllerDebugProtocol.cpp

#include "ControllerDebugProtocol.h"

int ControllerDebugProtocol::address(ControllerSystemRegister reg)
{
    return static_cast<int>(reg);
}

bool ControllerDebugProtocol::isValidDeviceId(int deviceId)
{
    return deviceId >= minDeviceId() && deviceId <= maxDeviceId();
}

bool ControllerDebugProtocol::canReadDeviceId(int deviceId)
{
    return isValidDeviceId(deviceId) && deviceId != broadcastDeviceId();
}

QVariantMap ControllerDebugProtocol::defaultRtuConfig(const QString& portName, int deviceId)
{
    return {
        {QStringLiteral("protocol"), QStringLiteral("MODBUS")},
        {QStringLiteral("mode"), QStringLiteral("RTU")},
        {QStringLiteral("type"), QStringLiteral("Master")},
        {QStringLiteral("address"), deviceId},
        {QStringLiteral("port"), portName},
        {QStringLiteral("baudRate"), ControllerSerialDefaults::baudRate},
        {QStringLiteral("dataBits"), ControllerSerialDefaults::dataBits},
        {QStringLiteral("parity"), QString::fromLatin1(ControllerSerialDefaults::parity)},
        {QStringLiteral("stopBits"), ControllerSerialDefaults::stopBits},
        {QStringLiteral("responseTimeout"), ControllerSerialDefaults::responseTimeoutMs},
        {QStringLiteral("retryCount"), ControllerSerialDefaults::retryCount}
    };
}

QVector<quint16> ControllerDebugProtocol::breakpointValues(quint16 breakPoint1, quint16 breakPoint2)
{
    return {breakPoint1, breakPoint2};
}
