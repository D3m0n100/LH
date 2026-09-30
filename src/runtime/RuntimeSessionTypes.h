#pragma once
#include <QMetaType>
enum class RuntimeSessionState {
    Idle,
    Compiled,
    Connecting,
    Connected,
    Running,
    Monitoring,
    Downloading,
    Fault
};

enum class DownloadState {
    Idle,
    Precheck,
    PrecheckFailed,
    Downloading,
    Retrying,
    Verifying,
    Succeeded,
    TransportFailed,
    DeviceRejected,
    VerifyFailed,
    Failed
};

Q_DECLARE_METATYPE(DownloadState)
Q_DECLARE_METATYPE(RuntimeSessionState)
