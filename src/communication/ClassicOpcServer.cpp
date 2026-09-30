#include "ClassicOpcServer.h"

#include "ModbusInterface.h"
#include "ClassicOpcPollWorker.h"
#include <QPointer>
#include "RuntimePointRegisterCodec.h"
#include "ModbusLimits.h"

#include <QRegularExpression>
#include <QDateTime>
#include <QtMath>
#include <initializer_list>
#include <utility>

ClassicOpcServer::ClassicOpcServer(QObject* parent)
    : IOpcServer(parent)
    , m_worker(new ClassicOpcPollWorker)
{
    m_worker->moveToThread(&m_ioThread);
    connect(&m_ioThread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_ioThread.start();
    m_pollTimer.setSingleShot(false);
    connect(&m_pollTimer, &QTimer::timeout, this, &ClassicOpcServer::pollDevice);
}

ClassicOpcServer::~ClassicOpcServer()
{
    stop();
    if (m_pollCancelled) m_pollCancelled->store(true);
    m_ioThread.quit();
    m_ioThread.wait();
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
        stop();
        return start(errorMessage);
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

    m_running = true;
    ++m_pollGeneration;
    m_pollCancelled = std::make_shared<std::atomic_bool>(false);
    m_lastErrorCode = CommErrorCode::NoError;
    m_lastErrorMessage.clear();
    m_lastStatusChangeTime = QDateTime::currentDateTimeUtc();
    rebuildMappings();
    startPolling();
    emit runningStateChanged(true);
    pollDevice();
    return true;
}

void ClassicOpcServer::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    ++m_pollGeneration;
    m_pollInFlight = false;
    m_modbusConnected = false;
    stopPolling();
    const auto cancelled = m_pollCancelled;
    if (cancelled) cancelled->store(true);
    auto worker = m_worker;
    QMetaObject::invokeMethod(worker, [worker, cancelled] { worker->closeWhenIdle(cancelled); }, Qt::QueuedConnection);
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
        if (m_pollCancelled) m_pollCancelled->store(true);
        ++m_pollGeneration;
        m_pollInFlight = false;
        m_pollCancelled = std::make_shared<std::atomic_bool>(false);
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
    snapshot.online = m_running && m_modbusConnected;
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
    snapshot.extras.insert(QStringLiteral("modbusConnected"), m_modbusConnected);
    snapshot.extras.insert(QStringLiteral("pollInFlight"), m_pollInFlight);
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
    if (!m_running || m_pollInFlight) return;
    m_pollInFlight = true;
    m_lastPollTime = QDateTime::currentDateTimeUtc();
    const auto generation = m_pollGeneration;
    const auto cancelled = m_pollCancelled;
    QList<ClassicPollPoint> requests;
    const int maxRegisters = qBound(1, m_config.maxRegistersPerRequest, ModbusLimits::ReadRegisters);
    for (const auto& point : std::as_const(m_points)) {
        ClassicPollPoint request;
        request.point = point;
        const auto info = m_pointAddressing.value(point.id, parseAddressing(point));
        request.unit = info.unitId; request.address = info.address; request.bit = info.bitOffset;
        const auto area = info.area.trimmed().toLower();
        if (area == "coil" || area == "coils") request.area = "coil";
        else if (area == "discrete" || area == "discrete-input" || area == "discrete-inputs") request.area = "discrete";
        else if (area == "input" || area == "input-register" || area == "input-registers") request.area = "input";
        else if (area == "holding" || area == "holding-register" || area == "holding-registers") request.area = "holding";
        else request.error = QStringLiteral("unknown register area");
        QString codecError;
        if (!RuntimePointRegisterCodec::buildSpec(point, &request.codec, &codecError)) request.error = codecError;
        const bool bits = request.area == "coil" || request.area == "discrete";
        const int limit = bits ? 2000 : maxRegisters;
        if (!info.valid || request.unit < 1 || request.unit > 247 || request.address < 0 || request.address > 65535 ||
                request.width() < 1 || request.width() > limit || request.address + request.width() > 65536 ||
                info.bitOffset < 0 || info.bitOffset > 15)
            request.error = QStringLiteral("invalid explicit address/unit/width/bit offset");
        if (point.access == RuntimePointAccess::WriteOnly) request.error = QStringLiteral("point is write-only");
        if (bits && !request.codec.dataType.isEmpty() && request.codec.dataType != "BOOL")
            request.error = QStringLiteral("coil/discrete point must be BOOL");
        if (info.bitOffset != 0 && (request.codec.dataType != "BOOL" || request.width() != 1 || bits))
            request.error = QStringLiteral("bitOffset requires a scalar BOOL register");
        requests.append(request);
    }
    const auto config = toModbusConfig();
    const int budgetMs = qBound(50, m_config.timeoutMs, 30000);
    QPointer<ClassicOpcServer> guard(this);
    auto completed = [guard, generation, requests](QList<RuntimePointValue> values, QString error, bool connected) {
        if (!guard) return;
        if (!connected && values.isEmpty()) {
            for (const auto& request : requests) {
                RuntimePointValue invalid;
                invalid.pointId = request.point.id;
                invalid.quality = RuntimePointQuality::Bad;
                invalid.timestamp = QDateTime::currentDateTimeUtc();
                invalid.origin = QStringLiteral("opc-poll");
                values.append(invalid);
            }
        }
        QMetaObject::invokeMethod(guard.data(), [guard, generation, values, error, connected] {
            if (!guard || !guard->m_running || generation != guard->m_pollGeneration) return;
            guard->m_pollInFlight = false;
            guard->m_modbusConnected = connected;
            for (const auto& value : values) {
                guard->m_values.insert(value.pointId, value);
                if (value.quality == RuntimePointQuality::Good) {
                    ++guard->m_successfulPollCount;
                    guard->m_lastSuccessfulPollTime = value.timestamp;
                } else ++guard->m_failedPollCount;
            }
            guard->m_lastErrorCode = error.isEmpty() ? CommErrorCode::NoError : CommErrorCode::ReceiveFailed;
            guard->m_lastErrorMessage = error;
            if (!error.isEmpty()) {
                if (values.isEmpty()) ++guard->m_failedPollCount;
                guard->m_lastStatusChangeTime = QDateTime::currentDateTimeUtc();
                emit guard->errorOccurred(error);
            }
        }, Qt::QueuedConnection);
    };
    auto worker = m_worker;
    QMetaObject::invokeMethod(worker, [worker, config, requests, budgetMs, maxRegisters, cancelled, completed] {
        try {
            worker->poll(config, requests, budgetMs, maxRegisters, cancelled, completed);
        } catch (...) {
            completed({}, QStringLiteral("Classic poll worker exception"), false);
        }
    }, Qt::QueuedConnection);
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
                const QVariant value = map.value(qKey);
                const double parsed = value.toDouble(&ok);
                if (value.type() != QVariant::Bool && ok && qIsFinite(parsed)
                        && qFloor(parsed) == parsed && parsed >= 0 && parsed <= 65535)
                    return static_cast<int>(parsed);
                info.valid = false;
                return fallback;
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
    info.elementCount = readInt({ "elementCount", "count", "length", "quantity" }, 1);
    if (info.elementCount <= 0) info.valid = false;

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
                const QVariant value = meta.value(key);
                const double parsed = value.toDouble(&ok);
                if (value.type() != QVariant::Bool && ok && qIsFinite(parsed)
                        && qFloor(parsed) == parsed && parsed >= 0 && parsed <= 65535) {
                    info.address = static_cast<int>(parsed);
                    break;
                }
                info.valid = false;
                break;
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
