// File: src/communication/ControllerDebugProtocol.h
// Controller debug protocol constants and helpers.

#ifndef CONTROLLERDEBUGPROTOCOL_H
#define CONTROLLERDEBUGPROTOCOL_H

#include "CommTypes.h"

#include <QVariantMap>
#include <QVector>

enum class ControllerSystemRegister {
    State = 10,
    ConfSave = 21,
    ReadPassword = 22,
    Reset = 23,
    WorkMode = 26,
    ComponentLine = 27,
    RunToCursor = 28,
    BreakPoint1 = 29,
    BreakPoint2 = 30,
    DebugStep = 31,
    DebugPause = 32,
    Version = 38
};

enum class ControllerResetCode {
    PowerOnOrWatchdog = 0,
    MonitorCommand = 1,
    Fault = 3,
    InterruptFault = 4,
    AbnormalState = 7,
    DownloadTimeout = 8,
    DownloadSuccess = 15
};

struct ControllerSerialDefaults
{
    static constexpr int baudRate = 115200;
    static constexpr int dataBits = 8;
    static constexpr int stopBits = 1;
    static constexpr int responseTimeoutMs = 500;
    static constexpr int retryCount = 3;
    static constexpr const char* parity = "None";
};

struct ControllerDebugStatus
{
    quint16 state = 0;
    quint16 workMode = 0;
    quint16 componentLine = 0;
    quint16 reset = 0;
    quint16 version = 0;
};

class ControllerDebugProtocol
{
public:
    static constexpr int minDeviceId() { return 0; }
    static constexpr int maxDeviceId() { return 63; }
    static constexpr int broadcastDeviceId() { return 0; }

    static int address(ControllerSystemRegister reg);
    static bool isValidDeviceId(int deviceId);
    static bool canReadDeviceId(int deviceId);

    static QVariantMap defaultRtuConfig(const QString& portName, int deviceId = 1);

    static QVector<quint16> breakpointValues(quint16 breakPoint1, quint16 breakPoint2);
};

#endif // CONTROLLERDEBUGPROTOCOL_H
