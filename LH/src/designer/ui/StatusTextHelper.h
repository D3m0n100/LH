#ifndef STATUS_TEXT_HELPER_H
#define STATUS_TEXT_HELPER_H

#include <QString>

#include "designer/ParameterController.h"

inline QString runtimeStateText(bool running)
{
    return running ? QStringLiteral("运行中") : QStringLiteral("已停止");
}

inline QString buildStateText(bool busy, bool success)
{
    if (busy) {
        return QStringLiteral("编译中");
    }
    if (success) {
        return QStringLiteral("成功");
    }
    return QStringLiteral("空闲");
}

inline QString monitoringStateText(bool monitoring)
{
    return monitoring ? QStringLiteral("活动") : QStringLiteral("未活动");
}

inline QString parameterStateText(ParameterState state)
{
    switch (state) {
    case ParameterState::Clean:           return QStringLiteral("未修改");
    case ParameterState::Modified:        return QStringLiteral("已修改");
    case ParameterState::PendingApply:    return QStringLiteral("待下发");
    case ParameterState::Applying:        return QStringLiteral("下发中");
    case ParameterState::PendingReadback:  return QStringLiteral("待回读");
    case ParameterState::Confirmed:       return QStringLiteral("已确认");
    case ParameterState::Mismatch:        return QStringLiteral("不匹配");
    case ParameterState::Timeout:         return QStringLiteral("超时");
    case ParameterState::ApplyFailed:     return QStringLiteral("下发失败");
    default:                              return QStringLiteral("未知");
    }
}

#endif // STATUS_TEXT_HELPER_H
