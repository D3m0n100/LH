#include "ClassicOpcServer.h"

#include "ModbusInterface.h"

#include <QRegularExpression>
#include <QDateTime>
#include <initializer_list>
#include <utility>

ClassicOpcServer::ClassicOpcServer(QObject* parent)
    : IOpcServer(parent)
    , m_modbus(new ModbusInterface(this))
{
    m_pollTimer.setSingleShot(false);
    connect(&m_pollTimer, &QTimer::timeout, this, &ClassicOpcServer::pollDevice);
}

bool ClassicOpcServer::applyConfig(const OpcServerConfig& config, QString* errorMessage)
{
    if (!isConfigValid(config, errorMessage)) {
        m_lastErrorCode = CommErrorCode::InvalidConfig;
        m_lastErrorMessage = errorMessage ? *errorMessage : QStringLiteral("Classic OPC config invalid");
        emit errorOccurred(m_lastErrorMessage);
        return false;
    }

    m_config = config;
    m_lastErrorCode = CommErrorCode::NoError;
    m_lastErrorMessage.clear();

    if (m_running) {
        if (m_modbus) {
            m_modbus->close();
        }
        QString reopenError;
        if (!ensureModbusOpen(&reopenError)) {
            m_lastErrorCode = CommErrorCode::ConnectionFailed;
            m_lastErrorMessage = reopenError;
            emit errorOccurred(m_lastErrorMessage);
            return false;
        }
        startPolling();
    }
    return true;
}

bool ClassicOpcServer::start(QString* errorMessage)
{
    if (!isConfigValid(m_config, errorMessage)) {
        m_lastErrorCode = CommErrorCode::InvalidConfig;
        m_lastErrorMessage = errorMessage ? *errorMessage : QStringLiteral("Classic OPC config invalid");
        emit errorOccurred(m_lastErrorMessage);
        return false;
    }

    if (m_running) {
        return true;
    }

    if (!ensureModbusOpen(errorMessage)) {
        m_lastErrorCode = CommErrorCode::ConnectionFailed;
        m_lastErrorMessage = errorMessage ? *errorMessage : QStringLiteral("Classic OPC failed to open Modbus backend");
        emit errorOccurred(m_lastErrorMessage);
        return false;
    }

    m_running = true;
    m_lastErrorCode = CommErrorCode::NoError;
    m_lastErrorMessage.clear();
    m_lastStatusChangeTime = QDateTime::currentDateTimeUtc();
    rebuildMappings();
    startPolling();
    emit runningStateChanged(true);
    return true;
}

void ClassicOpcServer::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    stopPolling();
    if (m_modbus) {
        m_modbus->close();
    }
    m_lastStatusChangeTime = QDateTime::currentDateTimeUtc();
    emit runningStateChanged(false);
}

void ClassicOpcServer::setRuntimePoints(const QList<RuntimePointDefinition>& points)
{
    m_points = points;
    m_pointById.clear();
    m_pointToNodePath.clear();
    m_nodePathToPoint.clear();
    m_pointAddressing.clear();
    m_addressedPointCount = 0;
    m_unresolvedPointCount = 0;

    for (const RuntimePointDefinition& point : m_points) {
        if (point.id.trimmed().isEmpty()) {
            continue;
        }
        const auto tag = RuntimePointConverter::runtimePointToOpcTag(point);
        const QString nodePath = nodePathForTag(tag);
        const AddressingInfo addressing = parseAddressing(point);
        m_pointById.insert(point.id, point);
        m_pointToNodePath.insert(point.id, nodePath);
        m_nodePathToPoint.insert(nodePath, point.id);
        m_pointAddressing.insert(point.id, addressing);
        if (addressing.address >= 0) {
            ++m_addressedPointCount;
        } else {
            ++m_unresolvedPointCount;
        }
    }

    if (m_running) {
        pollDevice();
    }
}

void ClassicOpcServer::setOpcTags(const QList<OpcTagDefinition>& tags)
{
    m_tags = tags;
    rebuildMappings();
}

void ClassicOpcServer::updatePointValues(const QList<RuntimePointValue>& values)
{
    for (const RuntimePointValue& value : values) {
        if (!value.pointId.isEmpty()) {
            m_values.insert(value.pointId, value);
        }
    }
}

void ClassicOpcServer::recordWriteResult(const QString& pointId, bool success, const QString& message)
{
    m_lastWritePointId = pointId;
    m_lastWriteSuccess = success;
    m_lastWriteMessage = message;
    m_lastWriteTime = QDateTime::currentDateTimeUtc();
    m_lastStatusChangeTime = m_lastWriteTime;
    if (success) {
        m_lastSuccessfulWriteTime = m_lastWriteTime;
        m_lastSuccessfulWriteMessage = message;
        ++m_successfulWriteCount;
    } else {
        m_lastFailedWriteTime = m_lastWriteTime;
        m_lastFailedWriteMessage = message;
        ++m_failedWriteCount;
    }
}

BackendStatusSnapshot ClassicOpcServer::statusSnapshot() const
{
    BackendStatusSnapshot snapshot;
    snapshot.online = m_running;
    snapshot.backendType = QStringLiteral("classic-modbus");
    snapshot.downloading = false;
    snapshot.downloadPercent = 0;
    snapshot.lastErrorCode = m_lastErrorCode;
    snapshot.lastErrorMessage = m_lastErrorMessage;
    snapshot.partialSuccess = false;
    snapshot.timestamp = QDateTime::currentDateTimeUtc();
    snapshot.extras.insert(QStringLiteral("pointCount"), m_points.size());
    snapshot.extras.insert(QStringLiteral("tagCount"), m_tags.size());
    snapshot.extras.insert(QStringLiteral("mappedNodeCount"), m_nodePathToPoint.size());
    snapshot.extras.insert(QStringLiteral("valueCount"), m_values.size());
    snapshot.extras.insert(QStringLiteral("polling"), m_pollTimer.isActive());
    snapshot.extras.insert(QStringLiteral("modbusConnected"), m_modbus && m_modbus->isConnected());
    snapshot.extras.insert(QStringLiteral("successfulPollCount"), m_successfulPollCount);
    snapshot.extras.insert(QStringLiteral("failedPollCount"), m_failedPollCount);
    snapshot.extras.insert(QStringLiteral("successfulWriteCount"), m_successfulWriteCount);
    snapshot.extras.insert(QStringLiteral("failedWriteCount"), m_failedWriteCount);
    snapshot.extras.insert(QStringLiteral("addressedPointCount"), m_addressedPointCount);
    snapshot.extras.insert(QStringLiteral("unresolvedPointCount"), m_unresolvedPointCount);
    snapshot.extras.insert(QStringLiteral("lastPollTime"),
                           m_lastPollTime.isValid() ? m_lastPollTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("lastSuccessfulPollTime"),
                           m_lastSuccessfulPollTime.isValid() ? m_lastSuccessfulPollTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("lastStatusChangeTime"),
                           m_lastStatusChangeTime.isValid() ? m_lastStatusChangeTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("enabled"), m_config.enabled);
    snapshot.extras.insert(QStringLiteral("channelName"), m_config.channelName);
    snapshot.extras.insert(QStringLiteral("deviceName"), m_config.deviceName);
    snapshot.extras.insert(QStringLiteral("serialMode"), m_config.serialMode);
    snapshot.extras.insert(QStringLiteral("timeoutMs"), m_config.timeoutMs);
    snapshot.extras.insert(QStringLiteral("reconnectDelayMs"), m_config.reconnectDelayMs);
    snapshot.extras.insert(QStringLiteral("retries"), m_config.retries);
    snapshot.extras.insert(QStringLiteral("maxRegistersPerRequest"), m_config.maxRegistersPerRequest);
    snapshot.extras.insert(QStringLiteral("rootDescription"), m_config.rootDescription);
    snapshot.extras.insert(QStringLiteral("classicServerName"), m_config.classicServerName);
    snapshot.extras.insert(QStringLiteral("exposeTagTable"), m_config.exposeTagTable);
    snapshot.extras.insert(QStringLiteral("lastWriteNodePath"), m_lastWriteNodePath);
    snapshot.extras.insert(QStringLiteral("lastWritePointId"), m_lastWritePointId);
    snapshot.extras.insert(QStringLiteral("lastWriteValue"), m_lastWriteValue);
    snapshot.extras.insert(QStringLiteral("lastWriteTime"),
                           m_lastWriteTime.isValid() ? m_lastWriteTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("lastWriteSuccess"), m_lastWriteSuccess);
    snapshot.extras.insert(QStringLiteral("lastWriteMessage"), m_lastWriteMessage);
    snapshot.extras.insert(QStringLiteral("lastSuccessfulWriteTime"),
                           m_lastSuccessfulWriteTime.isValid() ? m_lastSuccessfulWriteTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("lastFailedWriteTime"),
                           m_lastFailedWriteTime.isValid() ? m_lastFailedWriteTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("lastSuccessfulWriteMessage"), m_lastSuccessfulWriteMessage);
    snapshot.extras.insert(QStringLiteral("lastFailedWriteMessage"), m_lastFailedWriteMessage);
    snapshot.extras.insert(QStringLiteral("impl"), QStringLiteral("classic-modbus"));
    return snapshot;
}

void ClassicOpcServer::rebuildMappings()
{
    m_pointToNodePath.clear();
    m_nodePathToPoint.clear();

    for (const RuntimePointDefinition& point : std::as_const(m_points)) {
        if (point.id.trimmed().isEmpty()) {
            continue;
        }
        const auto tag = RuntimePointConverter::runtimePointToOpcTag(point);
        const QString nodePath = nodePathForTag(tag);
        m_pointToNodePath.insert(point.id, nodePath);
        m_nodePathToPoint.insert(nodePath, point.id);
    }

    for (const OpcTagDefinition& tag : std::as_const(m_tags)) {
        const QString nodePath = nodePathForTag(tag);
        if (!m_nodePathToPoint.contains(nodePath) && !tag.tagName.trimmed().isEmpty()) {
            m_nodePathToPoint.insert(nodePath, tag.tagName.trimmed());
        }
    }
}

void ClassicOpcServer::startPolling()
{
    const int intervalMs = qMax(50, m_config.publishIntervalMs);
    m_pollTimer.start(intervalMs);
}

void ClassicOpcServer::stopPolling()
{
    if (m_pollTimer.isActive()) {
        m_pollTimer.stop();
    }
}

void ClassicOpcServer::pollDevice()
{
    if (!m_running || !m_modbus) {
        return;
    }

    m_lastPollTime = QDateTime::currentDateTimeUtc();
    int successCount = 0;
    int failureCount = 0;

    if (!m_modbus->isConnected()) {
        QString errorMessage;
        if (!ensureModbusOpen(&errorMessage)) {
            m_lastErrorCode = CommErrorCode::ConnectionLost;
            m_lastErrorMessage = errorMessage;
            ++m_failedPollCount;
            m_lastStatusChangeTime = QDateTime::currentDateTimeUtc();
            emit errorOccurred(errorMessage);
            return;
        }
    }

    for (const RuntimePointDefinition& point : std::as_const(m_points)) {
        QString errorMessage;
        if (!refreshPointValue(point, &errorMessage)) {
            m_lastErrorCode = CommErrorCode::ReceiveFailed;
            m_lastErrorMessage = errorMessage;
            ++failureCount;
            ++m_failedPollCount;
            m_lastStatusChangeTime = QDateTime::currentDateTimeUtc();
            continue;
        }
        ++successCount;
        ++m_successfulPollCount;
    }

    if (successCount > 0) {
        m_lastSuccessfulPollTime = QDateTime::currentDateTimeUtc();
    }
}

bool ClassicOpcServer::refreshPointValue(const RuntimePointDefinition& point, QString* errorMessage)
{
    const AddressingInfo info = m_pointAddressing.value(point.id, parseAddressing(point));
    if (info.address < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("point %1 missing addressing.address").arg(point.id);
        }
        return false;
    }

    const int unitId = info.unitId <= 0 ? 1 : info.unitId;
    const int effectiveAddress = info.address + qMax(0, info.bitOffset);
    if (info.area.compare(QStringLiteral("coil"), Qt::CaseInsensitive) == 0
            || info.area.compare(QStringLiteral("coils"), Qt::CaseInsensitive) == 0) {
        m_modbus->setStationAddress(unitId);
        if (!m_modbus->readCoils(effectiveAddress, info.elementCount > 0 ? info.elementCount : 1)) {
            return false;
        }
        const auto values = m_modbus->coils().value(effectiveAddress);
        RuntimePointValue current;
        current.pointId = point.id;
        current.value = values.isEmpty() ? QVariant() : QVariant(values.first());
        current.quality = RuntimePointQuality::Good;
        current.timestamp = QDateTime::currentDateTimeUtc();
        current.origin = QStringLiteral("opc-poll");
        m_values.insert(point.id, current);
        return true;
    }

    if (info.area.compare(QStringLiteral("input"), Qt::CaseInsensitive) == 0
            || info.area.compare(QStringLiteral("input-register"), Qt::CaseInsensitive) == 0
            || info.area.compare(QStringLiteral("input-registers"), Qt::CaseInsensitive) == 0) {
        m_modbus->setStationAddress(unitId);
        if (!m_modbus->readInputRegisters(effectiveAddress, info.elementCount > 0 ? info.elementCount : 1)) {
            return false;
        }
        const auto values = m_modbus->inputRegisters().value(effectiveAddress);
        RuntimePointValue current;
        current.pointId = point.id;
        current.value = values.isEmpty() ? QVariant() : QVariant(values.first());
        current.quality = RuntimePointQuality::Good;
        current.timestamp = QDateTime::currentDateTimeUtc();
        current.origin = QStringLiteral("opc-poll");
        m_values.insert(point.id, current);
        return true;
    }

    m_modbus->setStationAddress(unitId);
    if (!m_modbus->readHoldingRegisters(effectiveAddress, info.elementCount > 0 ? info.elementCount : 1)) {
        return false;
    }
    const auto values = m_modbus->holdingRegisters().value(effectiveAddress);
    RuntimePointValue current;
    current.pointId = point.id;
    current.value = values.isEmpty() ? QVariant() : QVariant(values.first());
    current.quality = RuntimePointQuality::Good;
    current.timestamp = QDateTime::currentDateTimeUtc();
    current.origin = QStringLiteral("opc-poll");
    m_values.insert(point.id, current);
    return true;
}

bool ClassicOpcServer::writePointValue(const RuntimePointDefinition& point, const QVariant& value, QString* errorMessage)
{
    const AddressingInfo info = m_pointAddressing.value(point.id, parseAddressing(point));
    if (info.address < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("point %1 missing addressing.address").arg(point.id);
        }
        return false;
    }

    if (point.access == RuntimePointAccess::ReadOnly) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("point is read-only");
        }
        return false;
    }

    const int unitId = info.unitId <= 0 ? 1 : info.unitId;
    const int effectiveAddress = info.address + qMax(0, info.bitOffset);
    m_modbus->setStationAddress(unitId);

    if (info.area.compare(QStringLiteral("coil"), Qt::CaseInsensitive) == 0
            || info.area.compare(QStringLiteral("coils"), Qt::CaseInsensitive) == 0) {
        return m_modbus->writeSingleCoil(effectiveAddress, value.toBool());
    }

    if (info.elementCount > 1 || value.type() == QVariant::List) {
        QVector<quint16> registers;
        const QVariantList list = value.toList();
        if (!list.isEmpty()) {
            registers.reserve(list.size());
            for (const QVariant& item : list) {
                registers.push_back(static_cast<quint16>(item.toUInt()));
            }
        } else {
            registers.push_back(static_cast<quint16>(value.toUInt()));
        }
        return m_modbus->writeMultipleRegisters(effectiveAddress, registers);
    }

    return m_modbus->writeSingleRegister(effectiveAddress, static_cast<quint16>(value.toUInt()));
}

bool ClassicOpcServer::applyPointValue(const RuntimePointDefinition& point, const QVariant& value, QString* errorMessage)
{
    if (!m_running) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Classic OPC server is not running");
        }
        return false;
    }

    if (!m_modbus || !m_modbus->isConnected()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus backend is not connected");
        }
        return false;
    }

    const bool ok = writePointValue(point, value, errorMessage);
    if (ok) {
        m_lastWritePointId = point.id;
        m_lastWriteSuccess = true;
        m_lastWriteMessage = QStringLiteral("write ok");
        m_lastWriteValue = value;
        m_lastWriteTime = QDateTime::currentDateTimeUtc();
        m_lastSuccessfulWriteTime = m_lastWriteTime;
        m_lastSuccessfulWriteMessage = QStringLiteral("write ok");
        ++m_successfulWriteCount;
    }
    return ok;
}

bool ClassicOpcServer::ensureModbusOpen(QString* errorMessage)
{
    if (!m_modbus) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus backend unavailable");
        }
        return false;
    }

    ModbusConfig modbusConfig = toModbusConfig();
    if (!modbusConfig.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Classic OPC Modbus config invalid");
        }
        return false;
    }

    if (m_modbus->isConnected()) {
        return true;
    }

    if (!m_modbus->open(modbusConfig)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open Modbus backend");
        }
        m_lastStatusChangeTime = QDateTime::currentDateTimeUtc();
        return false;
    }
    m_lastStatusChangeTime = QDateTime::currentDateTimeUtc();
    return true;
}

ModbusConfig ClassicOpcServer::toModbusConfig() const
{
    ModbusConfig cfg;
    cfg.mode = ModbusConfig::Mode::RTU;
    cfg.stationType = ModbusConfig::StationType::Master;
    cfg.stationAddress = 1;
    cfg.retryCount = qMax(1, m_config.retries);
    cfg.responseTimeout = qMax(50, m_config.timeoutMs);
    cfg.portName = m_config.deviceName.trimmed();
    cfg.baudRate = 9600;
    cfg.dataBits = 8;
    cfg.stopBits = 1;
    cfg.parity = QStringLiteral("None");

    int baudRate = cfg.baudRate;
    int dataBits = cfg.dataBits;
    int stopBits = cfg.stopBits;
    QString parity = cfg.parity;
    QString parseError;
    if (parseSerialMode(m_config.serialMode, &baudRate, &dataBits, &stopBits, &parity, &parseError)) {
        cfg.baudRate = baudRate;
        cfg.dataBits = dataBits;
        cfg.stopBits = stopBits;
        cfg.parity = parity;
    }
    return cfg;
}

bool ClassicOpcServer::parseSerialMode(const QString& serialMode,
                                       int* baudRate,
                                       int* dataBits,
                                       int* stopBits,
                                       QString* parity,
                                       QString* errorMessage)
{
    const QStringList parts = serialMode.split(',', Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("serialMode is empty");
        }
        return false;
    }

    bool ok = false;
    const int parsedBaud = parts.value(0).trimmed().toInt(&ok);
    if (!ok || parsedBaud <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("serialMode baud is invalid");
        }
        return false;
    }
    if (baudRate) {
        *baudRate = parsedBaud;
    }

    if (dataBits && parts.size() > 2) {
        *dataBits = parts.value(2).trimmed().toInt(&ok);
        if (!ok || *dataBits < 5 || *dataBits > 8) {
            *dataBits = 8;
        }
    }

    if (stopBits && parts.size() > 3) {
        *stopBits = parts.value(3).trimmed().toInt(&ok);
        if (!ok || (*stopBits != 1 && *stopBits != 2)) {
            *stopBits = 1;
        }
    }

    if (parity && parts.size() > 1) {
        *parity = parts.value(1).trimmed();
    }

    return true;
}

ClassicOpcServer::AddressingInfo ClassicOpcServer::parseAddressing(const RuntimePointDefinition& point)
{
    AddressingInfo info;
    const QVariantMap map = point.addressing;
    auto readInt = [&](std::initializer_list<const char*> keys, int fallback) {
        for (const char* key : keys) {
            const QString qKey = QString::fromLatin1(key);
            if (map.contains(qKey)) {
                bool ok = false;
                const int parsed = map.value(qKey).toInt(&ok);
                if (ok) {
                    return parsed;
                }
            }
        }
        return fallback;
    };
    auto readString = [&](std::initializer_list<const char*> keys, const QString& fallback) {
        for (const char* key : keys) {
            const QString qKey = QString::fromLatin1(key);
            if (map.contains(qKey)) {
                const QString value = map.value(qKey).toString().trimmed();
                if (!value.isEmpty()) {
                    return value;
                }
            }
        }
        return fallback;
    };

    info.area = readString({ "area", "registerType", "tagArea", "memoryArea", "region" }, QStringLiteral("holding"));
    info.mode = readString({ "mode", "accessMode" }, QString());
    info.address = readInt({ "address", "regAddress", "register", "pointAddress", "offset" }, -1);
    info.unitId = readInt({ "unitId", "slaveId", "stationAddress", "serverAddress" }, 1);
    info.bitOffset = readInt({ "bitOffset", "bit", "bitIndex" }, 0);
    info.elementCount = qMax(1, readInt({ "elementCount", "count", "length", "quantity" }, 1));

    if (info.address < 0) {
        const QVariantMap meta = point.metadata;
        const QStringList addressKeys = {
            QStringLiteral("address"),
            QStringLiteral("regAddress"),
            QStringLiteral("register"),
            QStringLiteral("pointAddress"),
            QStringLiteral("offset")
        };
        for (const QString& key : addressKeys) {
            if (meta.contains(key)) {
                bool ok = false;
                const int parsed = meta.value(key).toInt(&ok);
                if (ok) {
                    info.address = parsed;
                    break;
                }
            }
        }
    }

    if (info.address < 0) {
        const QRegularExpression rx(QStringLiteral(R"((\d+))"));
        const auto match = rx.match(point.bindingPath);
        if (match.hasMatch()) {
            info.address = match.captured(1).toInt();
        }
    }

    if (info.area.compare(QStringLiteral("holding"), Qt::CaseInsensitive) == 0
            && point.access == RuntimePointAccess::ReadOnly) {
        info.area = QStringLiteral("input");
    }

    return info;
}

QString ClassicOpcServer::nodePathForTag(const OpcTagDefinition& tag) const
{
    const QString group = tag.tagGroup.trimmed().isEmpty() ? QStringLiteral("General") : tag.tagGroup.trimmed();
    const QString tagName = tag.tagName.trimmed().isEmpty() ? tag.item.trimmed() : tag.tagName.trimmed();
    if (!tagName.isEmpty()) {
        return QStringLiteral("Objects/LH/Classic/") + group + QStringLiteral("/") + tagName;
    }
    return QStringLiteral("Objects/LH/Classic/") + group + QStringLiteral("/Unknown");
}

bool ClassicOpcServer::routeWriteRequest(const QString& nodePath, const QVariant& value, QString* errorMessage)
{
    const auto idIt = m_nodePathToPoint.constFind(nodePath);
    if (idIt == m_nodePathToPoint.constEnd()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("node path is not mapped");
        }
        return false;
    }

    const QString pointId = idIt.value();
    const auto pointIt = m_pointById.constFind(pointId);
    if (pointIt == m_pointById.constEnd()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("mapped point not found");
        }
        return false;
    }

    const RuntimePointDefinition& point = pointIt.value();
    if (point.access == RuntimePointAccess::ReadOnly) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("point is read-only");
        }
        return false;
    }

    m_lastWriteNodePath = nodePath;
    m_lastWritePointId = pointId;
    m_lastWriteValue = value;
    m_lastWriteTime = QDateTime::currentDateTimeUtc();
    m_lastWriteSuccess = true;
    m_lastWriteMessage = QStringLiteral("write routed");
    emit writeRequestReceived(pointId, value);
    return true;
}

bool ClassicOpcServer::isConfigValid(const OpcServerConfig& config, QString* errorMessage)
{
    if (config.channelName.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Classic OPC channelName must not be empty");
        }
        return false;
    }

    if (config.deviceName.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Classic OPC deviceName must not be empty");
        }
        return false;
    }

    if (config.serialMode.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Classic OPC serialMode must not be empty");
        }
        return false;
    }

    if (config.timeoutMs <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Classic OPC timeoutMs must be > 0");
        }
        return false;
    }

    if (config.reconnectDelayMs < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Classic OPC reconnectDelayMs must be >= 0");
        }
        return false;
    }

    if (config.retries < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Classic OPC retries must be >= 0");
        }
        return false;
    }

    if (config.maxRegistersPerRequest <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Classic OPC maxRegistersPerRequest must be > 0");
        }
        return false;
    }

    return true;
}
