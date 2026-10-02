// File: src/communication/ControllerDeviceBackendDebug.cpp

#include "ControllerDeviceBackend.h"
#include "BackendOperationGuard.h"
#include <QPointer>
#include <QTimer>
#include <QDeadlineTimer>

bool ControllerDeviceBackend::requestDebugCommand(const QString& command,
        const QVector<quint16>& arguments, QObject* context,
        std::function<void(bool, QString)> completed, QString* errorMessage, int budgetMs)
{
    if (!context || !completed || budgetMs <= 0 || !ensureConfigured(errorMessage)) return false;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_online) {
            if (errorMessage) *errorMessage = QStringLiteral("控制器后端未连接。");
            return false;
        }
    }
    const int argumentCount = command == "cursor" ? 1 : command == "breakpoints" ? 2 : 0;
    if ((command != "pause" && command != "resume" && command != "step"
            && command != "cursor" && command != "breakpoints") || arguments.size() != argumentCount) {
        if (errorMessage) *errorMessage = QStringLiteral("无效的调试命令参数");
        return false;
    }
    if (m_asyncDownloadActive || m_asyncReadActive.exchange(true)) {
        if (errorMessage) *errorMessage = QStringLiteral("控制器正在处理其他请求");
        return false;
    }
    const auto cancelled = std::make_shared<std::atomic_bool>(false);
    m_debugCancelled = cancelled;
    const auto finished = std::make_shared<std::atomic_bool>(false);
    const QPointer<QObject> guard(context);
    const QDeadlineTimer deadline(budgetMs);
    QTimer::singleShot(budgetMs, context, [=] {
        if (!finished->exchange(true)) {
            cancelled->store(true);
            completed(false, QStringLiteral("调试命令超过总期限"));
        }
    });
    auto job = [this, command, arguments, cancelled, finished, guard, completed, deadline] {
        bool ok = false;
        QString message;
        Communication::BackendOperationGuard operation(&m_operationMutex);
        if (cancelled->load() || deadline.hasExpired() || !operation.tryLock()) {
            message = QStringLiteral("调试命令已取消、超时或控制器繁忙");
        } else {
            m_client->setRequestBudget(int(deadline.remainingTime()), cancelled.get());
            try {
                if (command == "pause") ok = pause(&message);
                else if (command == "resume") ok = resume(&message);
                else if (command == "step") ok = step(&message);
                else if (command == "cursor") ok = runToCursor(arguments.at(0), &message);
                else ok = setBreakpoints(arguments.at(0), arguments.at(1), &message);
            } catch (...) { message = QStringLiteral("调试命令遇到异常"); }
            m_client->setRequestBudget(-1, nullptr);
            if (cancelled->load() || deadline.hasExpired()) {
                ok = false; message = QStringLiteral("调试命令已取消或超时");
            }
        }
        m_asyncReadActive = false;
        if (guard) QMetaObject::invokeMethod(guard, [=] {
            if (!finished->exchange(true)) completed(ok, message);
        }, Qt::QueuedConnection);
    };
    if (m_client->hasWorkerThread()) m_client->runAsync(std::move(job));
    else QTimer::singleShot(0, this, std::move(job)); // Injected inline test clients have no I/O actor.
    return true;
}

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
