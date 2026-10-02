#pragma once
#include <QString>
#include <QDateTime>

enum class ParameterState {
    Clean, Modified, PendingApply, Applying, PendingReadback,
    Confirmed, Mismatch, Timeout, ApplyFailed
};

struct ParameterStateInfo
{
    QString pointId;
    QString name;
    QString dataType;
    ParameterState state = ParameterState::Clean;
    QString definitionValue;
    QString editedValue;
    QString appliedValue;
    QString readbackValue;
    QString lastError;
    QDateTime lastWriteTime;
    QDateTime lastReadbackTime;
    int readbackAttempts = 0;
    bool onlineEditable = false;
};
