#ifndef RUNTIME_POINT_QUALITY_MAPPER_H
#define RUNTIME_POINT_QUALITY_MAPPER_H

#include "CommTypes.h"
#include "common/RuntimePointTypes.h"

inline RuntimePointQuality runtimePointQualityFromBackendError(const CommError& error,
                                                               bool backendOnline)
{
    if (!error.isError()) {
        return backendOnline ? RuntimePointQuality::Good : RuntimePointQuality::Offline;
    }

    switch (error.code) {
    case CommErrorCode::ConnectionTimeout:
    case CommErrorCode::ReceiveTimeout:
        return backendOnline ? RuntimePointQuality::Stale : RuntimePointQuality::Offline;
    case CommErrorCode::ConnectionLost:
    case CommErrorCode::ConnectionFailed:
    case CommErrorCode::DeviceNotFound:
    case CommErrorCode::DeviceBusy:
        return RuntimePointQuality::Offline;
    case CommErrorCode::PermissionDenied:
    case CommErrorCode::InvalidAddress:
    case CommErrorCode::InvalidParameter:
    case CommErrorCode::UnsupportedProtocol:
    case CommErrorCode::ProtocolError:
    case CommErrorCode::InvalidResponse:
    case CommErrorCode::AddressMismatch:
    case CommErrorCode::ResourceError:
    case CommErrorCode::InternalError:
        return RuntimePointQuality::Bad;
    default:
        return backendOnline ? RuntimePointQuality::Bad : RuntimePointQuality::Offline;
    }
}

#endif // RUNTIME_POINT_QUALITY_MAPPER_H
