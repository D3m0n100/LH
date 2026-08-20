// File: src/designer/RuntimeSessionDebug.cpp

#include "RuntimeSessionController.h"

#include "ProjectController.h"
#include "../communication/ControllerDeviceBackend.h"
#include "../monitor/MonitorManager.h"

#include <QDateTime>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace {
class BackendOperationGuard
{
public:
    BackendOperationGuard(ControllerDeviceBackend* backend, QString* errorMessage)
        : m_backend(backend)
        , m_locked(m_backend && m_backend->tryBeginOperation(errorMessage))
    {
    }

    ~BackendOperationGuard()
    {
        if (m_locked) {
            m_backend->endOperation();
        }
    }

    bool locked() const { return m_locked; }

private:
    ControllerDeviceBackend* m_backend = nullptr;
    bool m_locked = false;
};
} // namespace

bool RuntimeSessionController::pauseController()
{
    auto* backend = controllerBackend();
    if (!backend) {
        emit runtimeError(QStringLiteral("控制器后端不可用，无法暂停。"));
        return false;
    }

    QString errorMessage;
    BackendOperationGuard operation(backend, &errorMessage);
    if (!operation.locked()) {
        emit runtimeError(errorMessage);
        return false;
    }
    if (!backend->pause(&errorMessage)) {
        emit runtimeError(errorMessage.isEmpty() ? QStringLiteral("控制器暂停失败。") : errorMessage);
        return false;
    }

    emit logMessage(QStringLiteral("[%1] 控制器已暂停")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
    return true;
}

bool RuntimeSessionController::resumeController()
{
    auto* backend = controllerBackend();
    if (!backend) {
        emit runtimeError(QStringLiteral("控制器后端不可用，无法继续。"));
        return false;
    }

    QString errorMessage;
    BackendOperationGuard operation(backend, &errorMessage);
    if (!operation.locked()) {
        emit runtimeError(errorMessage);
        return false;
    }
    if (!backend->resume(&errorMessage)) {
        emit runtimeError(errorMessage.isEmpty() ? QStringLiteral("控制器继续失败。") : errorMessage);
        return false;
    }

    emit logMessage(QStringLiteral("[%1] 控制器已继续运行")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
    return true;
}

bool RuntimeSessionController::stepController()
{
    auto* backend = controllerBackend();
    if (!backend) {
        emit runtimeError(QStringLiteral("控制器后端不可用，无法单步。"));
        return false;
    }

    QString errorMessage;
    BackendOperationGuard operation(backend, &errorMessage);
    if (!operation.locked()) {
        emit runtimeError(errorMessage);
        return false;
    }
    if (!backend->step(&errorMessage)) {
        emit runtimeError(errorMessage.isEmpty() ? QStringLiteral("控制器单步失败。") : errorMessage);
        return false;
    }

    emit logMessage(QStringLiteral("[%1] 控制器单步执行")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
    return true;
}

bool RuntimeSessionController::runControllerToCursor(int lineNumber)
{
    auto* backend = controllerBackend();
    if (!backend) {
        emit runtimeError(QStringLiteral("控制器后端不可用，无法运行到光标。"));
        return false;
    }

    QString errorMessage;
    BackendOperationGuard operation(backend, &errorMessage);
    if (!operation.locked()) {
        emit runtimeError(errorMessage);
        return false;
    }
    if (!backend->runToCursor(lineNumber, &errorMessage)) {
        emit runtimeError(errorMessage.isEmpty() ? QStringLiteral("控制器运行到光标失败。") : errorMessage);
        return false;
    }

    emit logMessage(QStringLiteral("[%1] 控制器运行到第 %2 行")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         QString::number(lineNumber)));
    return true;
}

bool RuntimeSessionController::setControllerBreakpoints(int firstLine, int secondLine)
{
    auto* backend = controllerBackend();
    if (!backend) {
        emit runtimeError(QStringLiteral("控制器后端不可用，无法设置断点。"));
        return false;
    }

    QString errorMessage;
    BackendOperationGuard operation(backend, &errorMessage);
    if (!operation.locked()) {
        emit runtimeError(errorMessage);
        return false;
    }
    if (!backend->setBreakpoints(firstLine, secondLine, &errorMessage)) {
        emit runtimeError(errorMessage.isEmpty() ? QStringLiteral("控制器断点设置失败。") : errorMessage);
        return false;
    }

    emit logMessage(QStringLiteral("[%1] 控制器断点已设置：%2, %3")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         QString::number(firstLine),
                         QString::number(secondLine)));
    return true;
}

bool RuntimeSessionController::testControllerConnection()
{
    auto* backend = controllerBackend();
    if (!backend || !backend->isOnline()) {
        if (!ensureControllerBackend()) {
            return false;
        }
        backend = controllerBackend();
    }
    if (!backend) {
        emit runtimeError(QStringLiteral("控制器后端不可用，无法测试连接。"));
        return false;
    }

    ControllerConnectionDiagnostic diagnostic;
    QString errorMessage;
    BackendOperationGuard operation(backend, &errorMessage);
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

    if (!m_ownedControllerBackend->isOnline() && !m_ownedControllerBackend->connectBackend()) {
        const CommError err = m_ownedControllerBackend->lastError();
        emit runtimeError(err.message.isEmpty() ? QStringLiteral("控制器后端连接失败。") : err.message);
        return false;
    }

    return true;
}

ControllerDeviceBackend* RuntimeSessionController::controllerBackend() const
{
    if (auto* backend = qobject_cast<ControllerDeviceBackend*>(m_backend)) {
        return backend;
    }
    return m_ownedControllerBackend;
}
