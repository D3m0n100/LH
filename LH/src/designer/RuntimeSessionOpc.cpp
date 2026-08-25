// File: src/designer/RuntimeSessionOpc.cpp

#include "RuntimeSessionController.h"

#include "ParameterController.h"
#include "ProjectController.h"
#include "../communication/ControllerDebugProtocol.h"
#include "../communication/IOpcServer.h"
#include "../common/RuntimePointTypes.h"

#include <QDateTime>
#include <QSet>
#include <utility>

namespace {

QString controllerOpcItemId(int address)
{
    return QStringLiteral("CommPort.InitDevParamnt.4:%1").arg(address);
}

RuntimePointDefinition makeControllerOpcPoint(const QString& pointId,
                                              const QString& name,
                                              ControllerSystemRegister reg,
                                              int unitId)
{
    const int address = ControllerDebugProtocol::address(reg);
    const int effectiveUnitId = ControllerDebugProtocol::canReadDeviceId(unitId) ? unitId : 1;

    RuntimePointDefinition point;
    point.id = pointId;
    point.name = name;
    point.kind = RuntimePointKind::Status;
    point.dataType = QStringLiteral("UINT16");
    point.access = RuntimePointAccess::ReadOnly;
    point.sourceModule = QStringLiteral("controller");
    point.defaultValue = 0;
    point.metadata.insert(QStringLiteral("controllerRegister"), address);
    point.metadata.insert(QStringLiteral("unitId"), effectiveUnitId);
    point.metadata.insert(QStringLiteral("opcItemId"), controllerOpcItemId(address));

    RuntimePointAddressing addressing;
    addressing.role = QStringLiteral("controller");
    addressing.protocol = QStringLiteral("modbus");
    addressing.mode = QStringLiteral("poll");
    addressing.area = QStringLiteral("holding");
    addressing.address = address;
    addressing.elementCount = 1;
    addressing.dataType = point.dataType;
    addressing.unitId = effectiveUnitId;
    addressing.channel = pointId;
    addressing.sourceModule = point.sourceModule;
    addressing.kind = QStringLiteral("Status");
    addressing.server = QStringLiteral("LH");
    addressing.item = QStringLiteral("4:%1").arg(address);
    addressing.ioGroup = QStringLiteral("controller");
    addressing.tagName = pointId;
    addressing.tagAccess = runtimePointAccessToString(point.access);
    addressing.tagDescription = name;
    addressing.extras = point.metadata;
    addressing.extras.insert(QStringLiteral("opcItemId"), controllerOpcItemId(address));
    addressing.extras.insert(QStringLiteral("opcChannel"), QStringLiteral("CommPort"));
    addressing.extras.insert(QStringLiteral("opcDevice"), QStringLiteral("InitDevParamnt"));
    point.addressing = addressing.toVariantMap();

    point.opcTagName = pointId;
    point.opcTagDescription = name;
    point.opcTagGroup = QStringLiteral("Controller");
    point.opcServerName = QStringLiteral("LH");
    point.opcItemName = QStringLiteral("4:%1").arg(address);
    point.opcIoGroup = QStringLiteral("controller");
    point.opcTagAccess = runtimePointAccessToString(point.access);
    point.opcMetadata = point.metadata;
    return point;
}

QList<RuntimePointDefinition> controllerOpcPoints(const ProjectRuntimeConfig& cfg)
{
    const int unitId = cfg.controller.modbusSlaveId;
    return {
        makeControllerOpcPoint(QStringLiteral("controller.state"),
                               QStringLiteral("Controller State"),
                               ControllerSystemRegister::State,
                               unitId),
        makeControllerOpcPoint(QStringLiteral("controller.workMode"),
                               QStringLiteral("Controller Work Mode"),
                               ControllerSystemRegister::WorkMode,
                               unitId),
        makeControllerOpcPoint(QStringLiteral("controller.componentLine"),
                               QStringLiteral("Controller Component Line"),
                               ControllerSystemRegister::ComponentLine,
                               unitId),
        makeControllerOpcPoint(QStringLiteral("controller.reset"),
                               QStringLiteral("Controller Reset State"),
                               ControllerSystemRegister::Reset,
                               unitId),
        makeControllerOpcPoint(QStringLiteral("controller.version"),
                               QStringLiteral("Controller Version"),
                               ControllerSystemRegister::Version,
                               unitId)
    };
}

} // namespace

void RuntimeSessionController::syncOpcRuntimePoints()
{
    if (!m_opcServer || !m_projectController) {
        return;
    }

    const auto& cfg = m_projectController->runtimeConfig();
    QList<RuntimePointDefinition> points = RuntimePointConverter::fromProjectConfig(cfg);
    QSet<QString> pointIds;
    for (const RuntimePointDefinition& point : std::as_const(points)) {
        pointIds.insert(point.id);
    }
    for (const RuntimePointDefinition& point : controllerOpcPoints(cfg)) {
        if (!pointIds.contains(point.id)) {
            points.append(point);
            pointIds.insert(point.id);
        }
    }
    m_opcServer->setRuntimePoints(points);
    m_opcServer->setOpcTags(RuntimePointConverter::runtimePointsToOpcTags(points));
}

void RuntimeSessionController::startOpcServerIfEnabled()
{
    if (!m_opcServer || !m_projectController) {
        return;
    }

    const auto& opcConfig = m_projectController->runtimeConfig().opcServer;
    if (!opcConfig.enabled) {
        stopOpcServer();
        return;
    }

    QString errorMessage;
    if (!m_opcServer->applyConfig(opcConfig, &errorMessage)) {
        emit runtimeError(QStringLiteral("OPC 服务配置失败：%1").arg(errorMessage));
        return;
    }

    if (!m_opcServer->start(&errorMessage)) {
        emit runtimeError(QStringLiteral("OPC 服务启动失败：%1").arg(errorMessage));
        return;
    }

    const BackendStatusSnapshot status = m_opcServer->statusSnapshot();
    emit logMessage(QStringLiteral("[%1] OPC DA 服务已启动：progId=%2 channel=%3 device=%4 mode=%5")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         opcConfig.opcProgId,
                         opcConfig.channelName,
                         opcConfig.deviceName,
                         opcConfig.serialMode));
    emit logMessage(QStringLiteral("[%1] OPC DA 点位状态：items=%2 active=%3 matched=%4 unmatched=%5 config=%6")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         QString::number(status.extras.value(QStringLiteral("itemCount")).toInt()),
                         QString::number(status.extras.value(QStringLiteral("successfulItemCount")).toInt()),
                         QString::number(status.extras.value(QStringLiteral("matchedItemCount")).toInt()),
                         QString::number(status.extras.value(QStringLiteral("unmatchedItemCount")).toInt()),
                         status.extras.value(QStringLiteral("configurationProbeMessage")).toString()));
    emit logMessage(QStringLiteral("[%1] OPC DA 读探针：attempted=%2 ok=%3 item=%4 value=%5 quality=%6 message=%7")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         status.extras.value(QStringLiteral("readProbeAttempted")).toBool() ? QStringLiteral("true") : QStringLiteral("false"),
                         status.extras.value(QStringLiteral("readProbeOk")).toBool() ? QStringLiteral("true") : QStringLiteral("false"),
                         status.extras.value(QStringLiteral("readProbeItemId")).toString(),
                         status.extras.value(QStringLiteral("readProbeValue")).toString(),
                         status.extras.value(QStringLiteral("readProbeQualityText")).toString(),
                         status.extras.value(QStringLiteral("readProbeMessage")).toString()));
}

void RuntimeSessionController::stopOpcServer()
{
    cancelPendingOpcWrite(QStringLiteral("OPC 服务已停止，写入已取消"));
    if (!m_opcServer || !m_opcServer->isRunning()) {
        return;
    }

    m_opcServer->stop();
    emit logMessage(QStringLiteral("[%1] OPC 服务已停止")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
}

void RuntimeSessionController::connectOpcServerSignals()
{
    if (!m_opcServer) {
        return;
    }

    connect(m_opcServer, &IOpcServer::runningStateChanged,
            this, &RuntimeSessionController::handleOpcRunningStateChanged,
            Qt::UniqueConnection);
    connect(m_opcServer, &IOpcServer::errorOccurred,
            this, &RuntimeSessionController::opcErrorOccurred,
            Qt::UniqueConnection);
    connect(m_opcServer, &IOpcServer::writeRequestReceived,
            this, &RuntimeSessionController::handleOpcWriteRequest,
            Qt::UniqueConnection);
}

void RuntimeSessionController::handleOpcRunningStateChanged(bool running)
{
    emit opcRunningChanged(running);
    if (!running) {
        cancelPendingOpcWrite(QStringLiteral("OPC 服务已停止，写入已取消"));
    }
}

void RuntimeSessionController::handleOpcWriteRequest(const QString& pointId, const QVariant& value)
{
    if (!m_opcServer) {
        return;
    }

    if (m_pendingOpcWriteActive) {
        const QString message = QStringLiteral("OPC 写入失败：已有参数写入正在等待回读确认");
        m_opcServer->recordWriteResult(pointId, false, message);
        emit runtimeError(message);
        return;
    }

    if (!m_parameterController) {
        const QString message = QStringLiteral("OPC 写入失败：参数控制器不可用");
        m_opcServer->recordWriteResult(pointId, false, message);
        emit runtimeError(message);
        return;
    }

    const ParameterStateInfo stateInfo = m_parameterController->parameterStateByPointId(pointId);
    if (stateInfo.name.isEmpty()) {
        const QString message = QStringLiteral("OPC 写入失败：未找到点位 %1").arg(pointId);
        m_opcServer->recordWriteResult(pointId, false, message);
        emit runtimeError(message);
        return;
    }

    if (!stateInfo.onlineEditable) {
        const QString message = QStringLiteral("OPC 写入失败：参数 %1 只读").arg(stateInfo.name);
        m_opcServer->recordWriteResult(pointId, false, message);
        emit runtimeError(message);
        return;
    }

    const QString valueText = value.toString();
    if (!m_parameterController->editParameterByPointId(pointId, valueText)) {
        const QString message = QStringLiteral("OPC 写入失败：参数 %1 编辑失败").arg(stateInfo.name);
        m_opcServer->recordWriteResult(pointId, false, message);
        emit runtimeError(message);
        return;
    }

    m_pendingOpcWriteActive = true;
    m_pendingOpcPointId = pointId;
    m_pendingOpcParameterName = stateInfo.name;

    QString applyError;
    const bool ok = m_parameterController->applyParameterByPointIdWithReadbackAsync(
            deviceBackend(), pointId, 1, 0, &applyError);
    if (!ok) {
        const QString message = applyError.isEmpty()
                ? QStringLiteral("OPC 写入失败：参数 %1 下发失败").arg(stateInfo.name)
                : QStringLiteral("OPC 写入失败：%1").arg(applyError);
        if (m_pendingOpcWriteActive) {
            m_opcServer->recordWriteResult(pointId, false, message);
            m_pendingOpcWriteActive = false;
            m_pendingOpcPointId.clear();
            m_pendingOpcParameterName.clear();
        }
        emit runtimeError(message);
        return;
    }

    emit logMessage(QStringLiteral("[%1] OPC 写入已受理，等待回读确认：%2=%3")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                         stateInfo.name,
                         valueText));
}

void RuntimeSessionController::handleParameterReadbackFinished(bool success,
                                                                const QString& message)
{
    if (!m_pendingOpcWriteActive || !m_opcServer || !m_parameterController) {
        return;
    }

    const QString pointId = m_pendingOpcPointId;
    const QString parameterName = m_pendingOpcParameterName;
    const ParameterStateInfo stateInfo = m_parameterController->parameterStateByPointId(pointId);
    const bool confirmed = success && stateInfo.state == ParameterState::Confirmed;
    const QString finalMessage = confirmed
            ? QStringLiteral("OPC 写入成功：%1=%2").arg(parameterName, stateInfo.readbackValue)
            : (message.isEmpty()
                       ? QStringLiteral("OPC 写入失败：参数 %1 未完成回读确认").arg(parameterName)
                       : QStringLiteral("OPC 写入失败：%1").arg(message));

    if (!confirmed) {
        m_opcServer->recordWriteResult(pointId, false, finalMessage);
        emit runtimeError(finalMessage);
    } else {
        m_opcServer->recordWriteResult(pointId, true, finalMessage);
        RuntimePointValue syncedValue;
        syncedValue.pointId = pointId;
        syncedValue.value = stateInfo.readbackValue;
        syncedValue.quality = RuntimePointQuality::Good;
        syncedValue.timestamp = QDateTime::currentDateTime();
        syncedValue.origin = QStringLiteral("opc-write");
        m_opcServer->updatePointValues({syncedValue});
        emit logMessage(QStringLiteral("[%1] %2")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                             finalMessage));
    }

    m_pendingOpcWriteActive = false;
    m_pendingOpcPointId.clear();
    m_pendingOpcParameterName.clear();
}

void RuntimeSessionController::cancelPendingOpcWrite(const QString& message)
{
    if (m_pendingOpcWriteActive && m_parameterController) {
        m_parameterController->cancelPendingReadback(message);
    }

    if (!m_pendingOpcWriteActive) {
        return;
    }

    const QString pointId = m_pendingOpcPointId;
    const QString finalMessage = message.isEmpty()
            ? QStringLiteral("OPC 写入已取消")
            : QStringLiteral("OPC 写入失败：%1").arg(message);
    if (m_opcServer) {
        m_opcServer->recordWriteResult(pointId, false, finalMessage);
    }
    m_pendingOpcWriteActive = false;
    m_pendingOpcPointId.clear();
    m_pendingOpcParameterName.clear();
}
