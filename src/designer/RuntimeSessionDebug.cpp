// File: src/designer/RuntimeSessionDebug.cpp

#include "RuntimeSessionController.h"

#include "ProjectController.h"
#include "../communication/ControllerDeviceBackend.h"
#include "../monitor/MonitorManager.h"
#include "../core/BusinessOperation.h"

#include <QDateTime>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>

namespace {
class ControllerOperationGuard {
public:
    ControllerOperationGuard(ControllerDeviceBackend* backend, QString* error)
        : m_backend(backend), m_locked(backend && backend->tryBeginOperation(error)) {}
    ~ControllerOperationGuard() { if (m_locked) m_backend->endOperation(); }
    bool locked() const { return m_locked; }
private:
    ControllerDeviceBackend* m_backend;
    bool m_locked;
};
}

bool RuntimeSessionController::requestControllerDebug(const QString& command,
        const QVector<quint16>& arguments, int pauseState, const QString& description)
{
    if (command != QStringLiteral("step")) stopContinuousStep();
    auto* backend = controllerBackend();
    QVariantList loggedArguments;
    for (auto argument : arguments) loggedArguments.append(argument);
    QVariantMap auditFields{{"backendGeneration", QVariant::fromValue(m_backendGeneration)},
                           {"commandGeneration", QVariant::fromValue(m_debugCommandGeneration + 1)},
                           {"command", command}, {"arguments", loggedArguments}};
    if (backend) {
        const auto config = backend->runtimeConfig();
        auditFields.insert("controllerId", config.controller.modbusSlaveId);
        auditFields.insert("port", config.transport.parameters.value("port", config.commParameters.value("port")));
    }
    auto audit = std::make_shared<Core::BusinessOperation>("controller_debug", "ControllerDeviceBackend", auditFields);
    if (m_debugCommandPending) {
        audit->finish("rejected", "Debug command is busy");
        emit runtimeError(QStringLiteral("控制器调试命令正在执行，请等待完成"));
        return false;
    }
    if (!backend) {
        if (m_demoModeActive && pauseState >= 0) {
            setPaused(pauseState == 1);
            emit logMessage(description);
            audit->finish("succeeded", "Demo mode");
            return true;
        }
        audit->finish("rejected", "Controller backend unavailable");
        emit runtimeError(QStringLiteral("控制器后端不可用，无法执行调试命令。"));
        return false;
    }
    const quint64 generation = m_backendGeneration;
    const quint64 operation = ++m_debugCommandGeneration;
    const QPointer<ControllerDeviceBackend> original(backend);
    QString error;
    const bool admitted = backend->requestDebugCommand(command, arguments, this,
        [this, generation, operation, original, pauseState, description, audit, command](bool ok, const QString& message) {
            if (generation != m_backendGeneration || operation != m_debugCommandGeneration || original != controllerBackend()) {
                audit->finish("canceled", "Session changed"); return;
            }
            audit->finish(ok ? "succeeded" : "failed", message);
            setDebugCommandPending(false);
            if (!ok) { stopContinuousStep(); emit runtimeError(message); return; }
            if (pauseState >= 0) setPaused(pauseState == 1);
            emit logMessage(QStringLiteral("[%1] %2")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), description));
            if (command == QStringLiteral("step")) scheduleContinuousStep();
        }, &error);
    if (!admitted) { audit->finish("rejected", error); emit runtimeError(error); }
    else {
        m_cancelDebugAudit = [audit, original] {
            if (original) original->cancelDebugCommand();
            audit->finish("canceled", "Session stopped or replaced");
        };
        setDebugCommandPending(true);
        emit logMessage(QStringLiteral("控制器调试命令处理中…"));
    }
    return admitted;
}

void RuntimeSessionController::setDebugCommandPending(bool pending)
{
    if (!pending && m_cancelDebugAudit) {
        auto cancel = std::move(m_cancelDebugAudit);
        m_cancelDebugAudit = {};
        cancel();
    }
    if (m_debugCommandPending == pending) return;
    m_debugCommandPending = pending;
    emit debugCommandPendingChanged(pending);
}

bool RuntimeSessionController::pauseController()
{
    return requestControllerDebug("pause", {}, 1, QStringLiteral("控制器已暂停"));
}

bool RuntimeSessionController::resumeController()
{
    return requestControllerDebug("resume", {}, 0, QStringLiteral("控制器已继续运行"));
}

bool RuntimeSessionController::stepController()
{
    return requestControllerDebug("step", {}, 1, QStringLiteral("控制器单步执行"));
}

bool RuntimeSessionController::startContinuousStep(int intervalMs)
{
    if (intervalMs < 50 || intervalMs > 60000 || m_debugCommandPending || m_continuousStepActive) return false;
    m_continuousStepIntervalMs = intervalMs;
    m_continuousStepActive = true;
    emit continuousStepChanged(true);
    if (!stepController()) { stopContinuousStep(); return false; }
    // Demo completion is synchronous; real backend completion schedules the timer.
    if (!m_debugCommandPending) scheduleContinuousStep();
    return true;
}

void RuntimeSessionController::scheduleContinuousStep()
{
    if (!m_continuousStepActive) return;
    if (!m_continuousStepTimer) {
        m_continuousStepTimer = new QTimer(this);
        m_continuousStepTimer->setSingleShot(true);
        connect(m_continuousStepTimer, &QTimer::timeout, this, [this] {
            if (!m_continuousStepActive || m_debugCommandPending) return;
            if (!stepController()) { stopContinuousStep(); return; }
            if (!m_debugCommandPending) scheduleContinuousStep();
        });
    }
    m_continuousStepTimer->start(m_continuousStepIntervalMs);
}

void RuntimeSessionController::stopContinuousStep()
{
    if (m_continuousStepTimer) m_continuousStepTimer->stop();
    if (!m_continuousStepActive) return;
    m_continuousStepActive = false;
    emit continuousStepChanged(false);
}

bool RuntimeSessionController::runControllerToCursor(int lineNumber)
{
    if (lineNumber < 0 || lineNumber > 65535) {
        emit runtimeError(QStringLiteral("调试行号超出范围")); return false;
    }
    return requestControllerDebug("cursor", {quint16(lineNumber)}, -1,
        QStringLiteral("控制器运行到组件位置 %1").arg(lineNumber));
}

bool RuntimeSessionController::setControllerBreakpoints(int firstLine, int secondLine)
{
    if (firstLine < 0 || firstLine > 65535 || secondLine < 0 || secondLine > 65535) {
        emit runtimeError(QStringLiteral("断点行号超出范围")); return false;
    }
    return requestControllerDebug("breakpoints", {quint16(firstLine), quint16(secondLine)}, -1,
        QStringLiteral("控制器组件断点已设置：%1, %2").arg(firstLine).arg(secondLine));
}

bool RuntimeSessionController::testControllerConnection()
{
    auto* backend = controllerBackend();
    if (!backend || !backend->isOnline()) {
        ensureControllerBackendAsync([this](bool ok) { if (ok) testControllerConnection(); });
        return true;
    }
    if (!backend) {
        emit runtimeError(QStringLiteral("控制器后端不可用，无法测试连接。"));
        return false;
    }

    if (backend->supportsAsyncRead()) {
        const quint64 generation = m_backendGeneration;
        const QPointer<ControllerDeviceBackend> original(backend);
        backend->testConnectionAsync(3000, this,
            [this, generation, original](bool ok, ControllerConnectionDiagnostic diagnostic, const QString& error) {
                if (generation != m_backendGeneration || original != controllerBackend()) return;
                if (!ok) { emit runtimeError(error); return; }
                emit logMessage(QStringLiteral("控制器连接测试：port=%1 deviceId=%2 state=%3 version=%4 target=%5")
                    .arg(diagnostic.portName).arg(diagnostic.deviceId).arg(diagnostic.state)
                    .arg(diagnostic.version).arg(diagnostic.targetOnline));
            });
        return true;
    }

    ControllerConnectionDiagnostic diagnostic;
    QString errorMessage;
    ControllerOperationGuard operation(backend, &errorMessage);
    if (!operation.locked()) {
        emit runtimeError(errorMessage);
        return false;
    }
    if (!backend->testConnection(&diagnostic, &errorMessage)) {
        const QString message = errorMessage.isEmpty()
                ? QStringLiteral("控制器连接测试失败。")
                : errorMessage;
        emit runtimeError(message);
        emit logMessage(QStringLiteral("[%1] 连接排查：先检查 OPC/串口参数，再检查下载线和控制器电源，再检查目标侧总线与装置号。")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
        return false;
    }

    emit logMessage(QStringLiteral("[%1] 控制器连接测试：port=%2 deviceId=%3 state=%4 workMode=%5 version=%6 target=%7 targetDeviceId=%8")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         diagnostic.portName,
                         QString::number(diagnostic.deviceId),
                         QString::number(diagnostic.state),
                         QString::number(diagnostic.workMode),
                         QString::number(diagnostic.version),
                         diagnostic.targetOnline ? QStringLiteral("online") : QStringLiteral("offline"),
                         QString::number(diagnostic.targetDeviceId)));
    const QVariantMap preflight = diagnostic.extras.value(QStringLiteral("preflight")).toMap();
    const QVariantList errors = preflight.value(QStringLiteral("errors")).toList();
    if (!errors.isEmpty()) {
        QStringList messages;
        for (const QVariant& error : errors) {
            messages << error.toString();
        }
        emit logMessage(QStringLiteral("[%1] 控制器预检错误：%2")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                             messages.join(QStringLiteral("; "))));
    }

    const QVariantList warnings = preflight.value(QStringLiteral("warnings")).toList();
    if (!warnings.isEmpty()) {
        QStringList messages;
        for (const QVariant& warning : warnings) {
            messages << warning.toString();
        }
        emit logMessage(QStringLiteral("[%1] 控制器预检警告：%2")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                             messages.join(QStringLiteral("; "))));
    }

    const QVariantMap serial = preflight.value(QStringLiteral("serial")).toMap();
    if (!serial.isEmpty()) {
        emit logMessage(QStringLiteral("[%1] 控制器串口参数：baud=%2 parity=%3 dataBits=%4 stopBits=%5 timeout=%6 retries=%7")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                             QString::number(serial.value(QStringLiteral("baudRate")).toInt()),
                             serial.value(QStringLiteral("parity")).toString(),
                             QString::number(serial.value(QStringLiteral("dataBits")).toInt()),
                             QString::number(serial.value(QStringLiteral("stopBits")).toInt()),
                             QString::number(serial.value(QStringLiteral("responseTimeout")).toInt()),
                             QString::number(serial.value(QStringLiteral("retryCount")).toInt())));
    }

    const QVariantMap opc = preflight.value(QStringLiteral("opc")).toMap();
    if (!opc.isEmpty()) {
        emit logMessage(QStringLiteral("[%1] OPC 预检：enabled=%2 progId=%3 registered=%4 device=%5 mode=%6")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                             opc.value(QStringLiteral("enabled")).toBool() ? QStringLiteral("true") : QStringLiteral("false"),
                             opc.value(QStringLiteral("progId")).toString(),
                             opc.value(QStringLiteral("progIdRegistered")).toBool() ? QStringLiteral("true") : QStringLiteral("false"),
                             opc.value(QStringLiteral("deviceName")).toString(),
                             opc.value(QStringLiteral("serialMode")).toString()));
    }

    const QVariantMap mappings = preflight.value(QStringLiteral("pointMappings")).toMap();
    if (!mappings.isEmpty()) {
        emit logMessage(QStringLiteral("[%1] 控制器点位映射：holding=%2 readable=%3 writable=%4 unmapped=%5 targetMapped=%6")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                             QString::number(mappings.value(QStringLiteral("mappedHoldingPoints")).toInt()),
                             QString::number(mappings.value(QStringLiteral("readableMappedPoints")).toInt()),
                             QString::number(mappings.value(QStringLiteral("writableMappedPoints")).toInt()),
                             QString::number(mappings.value(QStringLiteral("unmappedPoints")).toInt()),
                             QString::number(mappings.value(QStringLiteral("targetMappedPoints")).toInt())));
        const QVariantList unmappedDetails = mappings.value(QStringLiteral("unmappedPointDetails")).toList();
        if (!unmappedDetails.isEmpty()) {
            QStringList samples;
            const int sampleCount = qMin(5, unmappedDetails.size());
            for (int i = 0; i < sampleCount; ++i) {
                const QVariantMap item = unmappedDetails.at(i).toMap();
                samples << QStringLiteral("%1(%2)")
                           .arg(item.value(QStringLiteral("pointId")).toString(),
                                item.value(QStringLiteral("reason")).toString());
            }
            emit logMessage(QStringLiteral("[%1] 未映射点位示例：%2")
                            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                 samples.join(QStringLiteral("; "))));
        }
    }

    const QVariantMap targetProbe = diagnostic.extras.value(QStringLiteral("lastTargetProbe")).toMap();
    if (!targetProbe.isEmpty()) {
        QStringList targetProbeValues;
        const QVariantList rawTargetProbeValues = targetProbe.value(QStringLiteral("values")).toList();
        for (const QVariant& value : rawTargetProbeValues) {
            targetProbeValues << value.toString();
        }
        emit logMessage(QStringLiteral("[%1] 目标探测：slaveId=%2 address=%3 count=%4 ok=%5 values=%6")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                             QString::number(targetProbe.value(QStringLiteral("slaveId")).toInt()),
                             QString::number(targetProbe.value(QStringLiteral("address")).toInt()),
                             QString::number(targetProbe.value(QStringLiteral("count")).toInt()),
                             targetProbe.value(QStringLiteral("ok")).toBool() ? QStringLiteral("true") : QStringLiteral("false"),
                             targetProbeValues.isEmpty()
                                ? QStringLiteral("-")
                                : targetProbeValues.join(QStringLiteral(","))));
    }
    if (!diagnostic.message.isEmpty()) {
        emit logMessage(QStringLiteral("[%1] %2")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                             diagnostic.message));
    }
    return true;
}

bool RuntimeSessionController::ensureControllerBackend()
{
    if (!m_projectController) {
        emit runtimeError(QStringLiteral("项目控制器不可用，无法配置控制器后端。"));
        return false;
    }

    if (!m_ownedControllerBackend) {
        m_ownedControllerBackend = new ControllerDeviceBackend(this);
    }

    QString errorMessage;
    const ProjectRuntimeConfig cfg = m_projectController->runtimeConfig();
    if (!m_ownedControllerBackend->configure(cfg, &errorMessage)) {
        emit runtimeError(errorMessage);
        return false;
    }

    if (m_backend != m_ownedControllerBackend) {
        setDeviceBackend(m_ownedControllerBackend);
    }

    if (!m_ownedControllerBackend->isOnline() && m_ownedControllerBackend->supportsAsyncRead()) {
        emit runtimeError(QStringLiteral("控制器尚未连接，请使用异步连接操作。"));
        return false;
    }
    if (!m_ownedControllerBackend->isOnline() && !m_ownedControllerBackend->connectBackend()) {
        const CommError err = m_ownedControllerBackend->lastError();
        emit runtimeError(err.message.isEmpty() ? QStringLiteral("控制器后端连接失败。") : err.message);
        return false;
    }

    return true;
}

void RuntimeSessionController::ensureControllerBackendAsync(std::function<void(bool)> completed)
{
    if (!completed) return;
    if (!m_projectController) { completed(false); return; }
    if (m_connectionCancelled) m_connectionCancelled->store(true);
    m_connectionCancelled.reset();
    if (!m_ownedControllerBackend) {
        m_ownedControllerBackend = new ControllerDeviceBackend(this);
    }
    auto* backend = m_ownedControllerBackend;
    const auto config = m_projectController->runtimeConfig();
    QString error;
    if (!backend->configure(config, &error)) { emit runtimeError(error); completed(false); return; }
    if (m_backend != backend) setDeviceBackend(backend);
    if (backend->isOnline()) { completed(true); return; }
    if (state() != RuntimeSessionState::Connecting)
        setState(RuntimeSessionState::Connecting);
    const auto token = std::make_shared<std::atomic_bool>(false);
    m_connectionCancelled = token;
    const auto generation = m_backendGeneration;
    const QPointer<ProjectController> project = m_projectController;
    backend->connectBackendAsync(3000, token, this,
        [this, completed, backend, generation, token, project, config](bool ok, const QString& message) {
            if (token != m_connectionCancelled || generation != m_backendGeneration || backend != m_backend
                    || !project || project != m_projectController || config.toJson() != project->runtimeConfig().toJson()) {
                token->store(true);
                if (state() == RuntimeSessionState::Connecting)
                    setState(RuntimeSessionState::Idle);
                if (token == m_connectionCancelled && backend == m_backend) backend->disconnectBackend();
                completed(false); return;
            }
            if (!ok) emit runtimeError(message);
            if (!ok && state() == RuntimeSessionState::Connecting)
                setState(RuntimeSessionState::Idle);
            if (ok) emit backend->connectionStateChanged(true);
            completed(ok);
        }, false);
}

ControllerDeviceBackend* RuntimeSessionController::controllerBackend() const
{
    if (auto* backend = qobject_cast<ControllerDeviceBackend*>(m_backend)) {
        return backend;
    }
    return m_ownedControllerBackend;
}
