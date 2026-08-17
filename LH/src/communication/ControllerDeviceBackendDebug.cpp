// File: src/communication/ControllerDeviceBackendDebug.cpp

#include "ControllerDeviceBackend.h"

bool ControllerDeviceBackend::pause(QString* errorMessage)
{
    if (!ensureOnline(errorMessage)) {
        return false;
    }
    return executeDebugCommand(m_client->pause(), QStringLiteral("暂停控制器"), errorMessage);
}

bool ControllerDeviceBackend::resume(QString* errorMessage)
{
    if (!ensureOnline(errorMessage)) {
        return false;
    }
    return executeDebugCommand(m_client->resume(), QStringLiteral("继续控制器"), errorMessage);
}

bool ControllerDeviceBackend::step(QString* errorMessage)
{
    if (!ensureOnline(errorMessage)) {
        return false;
    }
    return executeDebugCommand(m_client->step(), QStringLiteral("单步执行"), errorMessage);
}

bool ControllerDeviceBackend::runToCursor(int lineNumber, QString* errorMessage)
{
    if (!ensureOnline(errorMessage)) {
        return false;
    }
    return executeDebugCommand(m_client->runToCursor(boundedLine(lineNumber)),
                               QStringLiteral("运行到光标"),
                               errorMessage);
}

bool ControllerDeviceBackend::setBreakpoints(int firstLine, int secondLine, QString* errorMessage)
{
    if (!ensureOnline(errorMessage)) {
        return false;
    }
    return executeDebugCommand(m_client->setBreakpoints(boundedLine(firstLine), boundedLine(secondLine)),
                               QStringLiteral("设置断点"),
                               errorMessage);
}

bool ControllerDeviceBackend::executeDebugCommand(bool ok, const QString& action, QString* errorMessage)
{
    if (ok) {
        clearFailure();
        return true;
    }

    const CommError err = currentDebugError(QStringLiteral("%1失败。").arg(action));
    if (errorMessage) {
        *errorMessage = err.message;
    }
    setFailure(err.code, err.message, err.details);
    return false;
}
