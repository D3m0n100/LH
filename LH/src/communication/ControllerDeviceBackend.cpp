// File: src/communication/ControllerDeviceBackend.cpp

#include "ControllerDeviceBackend.h"

#include "ControllerDebugProtocol.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMutexLocker>
#include <QtGlobal>

namespace {
bool readPositiveInt(const QVariantMap& map, const QStringList& keys, int* value)
{
    for (const QString& key : keys) {
        bool ok = false;
        const int candidate = map.value(key).toInt(&ok);
        if (ok && candidate > 0) {
            *value = candidate;
            return true;
        }
    }
    return false;
}
} // namespace

ControllerDeviceBackend::ControllerDeviceBackend(QObject* parent)
    : IDeviceBackend(parent)
    , m_ownedClient(new ControllerDebugClient(this))
    , m_client(m_ownedClient.data())
{
}

ControllerDeviceBackend::~ControllerDeviceBackend()
{
    disconnectBackend();
}

bool ControllerDeviceBackend::configure(const ProjectRuntimeConfig& config, QString* errorMessage)
{
    QMutexLocker lock(&m_mutex);
    m_config = config;
    m_configured = true;
    m_lastDownloadError.clear();
    m_targetOnline = false;
    m_targetDeviceId = targetDeviceId();
    m_lastTargetProbe.clear();
    loadPointDefinitions(RuntimePointConverter::fromProjectConfig(config));

    QString transportProtocol = config.transport.protocol.trimmed();
    QString transportMode = config.transport.mode.trimmed();
    const QString legacyProtocol = (config.protocol.trimmed().isEmpty()
                                    ? config.commParameters.value(QStringLiteral("protocol")).toString()
                                    : config.protocol).trimmed();
    const QString legacyProtocolToken = legacyProtocol.toLower();
    const bool placeholderTransport = transportProtocol.compare(QStringLiteral("modbus"), Qt::CaseInsensitive) == 0
            && transportMode.compare(QStringLiteral("rtu"), Qt::CaseInsensitive) == 0
            && !legacyProtocol.isEmpty()
            && legacyProtocolToken != QStringLiteral("modbus")
            && legacyProtocolToken != QStringLiteral("modbusrtu")
            && legacyProtocolToken != QStringLiteral("rtu");
    if (placeholderTransport) {
        transportProtocol = legacyProtocol;
        const QString legacyMode = config.commParameters.value(QStringLiteral("mode")).toString().trimmed();
        if (!legacyMode.isEmpty()) transportMode = legacyMode;
    }
    const bool isModbus = transportProtocol.isEmpty()
            || transportProtocol.compare(QStringLiteral("modbus"), Qt::CaseInsensitive) == 0;
    const bool isRtu = transportMode.isEmpty()
            || transportMode.compare(QStringLiteral("rtu"), Qt::CaseInsensitive) == 0;
    if (!isModbus || !isRtu) {
        m_configured = false;
        const QString message = transportMode.compare(QStringLiteral("tcp"), Qt::CaseInsensitive) == 0
                ? QStringLiteral("当前控制器后端仅支持 Modbus RTU，已拒绝 Modbus TCP 配置。")
                : QStringLiteral("当前控制器后端仅支持 Modbus RTU，传输配置无效。");
        if (errorMessage) {
            *errorMessage = message;
        }
        lock.unlock();
        setFailure(CommErrorCode::InvalidConfig,
                   message,
                   QStringLiteral("protocol=%1, mode=%2")
                           .arg(transportProtocol, transportMode));
        return false;
    }

    QStringList preflightErrors;
    QStringList preflightWarnings;
    buildPreflightReport(&preflightErrors, &preflightWarnings);
    if (!preflightErrors.isEmpty()) {
        m_configured = false;
        const QString message = QStringLiteral("控制器后端配置预检失败：%1")
                .arg(preflightErrors.join(QStringLiteral("; ")));
        if (errorMessage) {
            *errorMessage = message;
        }
        lock.unlock();
        setFailure(CommErrorCode::InvalidConfig, message);
        return false;
    }

    const QVariantMap transport = config.transport.parameters;
    const QString configuredPortName = firstNonEmptyString(transport,
                                                           config.commParameters,
                                                           {QStringLiteral("port"),
                                                            QStringLiteral("portName"),
                                                            QStringLiteral("deviceName")});
    if (configuredPortName.trimmed().isEmpty()) {
        m_configured = false;
        const QString message = QStringLiteral("控制器后端配置失败：未指定串口端口。");
        if (errorMessage) {
            *errorMessage = message;
        }
        lock.unlock();
        setFailure(CommErrorCode::InvalidConfig, message);
        return false;
    }

    const int id = deviceId();
    if (!ControllerDebugProtocol::canReadDeviceId(id)) {
        m_configured = false;
        const QString message = QStringLiteral("控制器后端配置失败：装置号必须为 1..63。");
        if (errorMessage) {
            *errorMessage = message;
        }
        lock.unlock();
        setFailure(CommErrorCode::InvalidAddress, message, QStringLiteral("deviceId=%1").arg(id));
        return false;
    }

    lock.unlock();
    clearFailure();
    return true;
}

ProjectRuntimeConfig ControllerDeviceBackend::runtimeConfig() const
{
    QMutexLocker lock(&m_mutex);
    return m_config;
}

void ControllerDeviceBackend::setDebugClientForTest(ControllerDebugClient* client)
{
    QMutexLocker lock(&m_mutex);
    m_ownedClient.reset();
    m_client = client;
}

bool ControllerDeviceBackend::connectBackend()
{
    ControllerDebugClient* client = nullptr;
    QVariantMap cfg;
    int id = 1;
    {
        QMutexLocker lock(&m_mutex);
        if (m_online) {
            return true;
        }
        if (!m_configured) {
            const QString message = QStringLiteral("控制器后端尚未配置。");
            lock.unlock();
            setFailure(CommErrorCode::InvalidConfig, message);
            return false;
        }
        client = m_client;
        cfg = buildRtuConfig();
        id = deviceId();
    }

    if (!client) {
        const QString message = QStringLiteral("控制器调试客户端不可用。");
        setFailure(CommErrorCode::InvalidConfig, message);
        return false;
    }

    if (!client->isConnected() && !client->openRtu(cfg, id)) {
        const CommError err = currentDebugError(QStringLiteral("控制器连接失败。"));
        setFailure(err.code, err.message, err.details);
        return false;
    }

    ControllerDebugStatus status;
    if (!client->readStatus(&status)) {
        const CommError err = currentDebugError(QStringLiteral("控制器状态读取失败。"));
        setFailure(err.code, err.message, err.details);
        client->close();
        return false;
    }

    QString targetProbeError;
    const bool targetOk = probeTarget(&targetProbeError);

    {
        QMutexLocker lock(&m_mutex);
        m_online = true;
        m_targetOnline = targetOk;
        updateStatusCache(status);
        m_lastDownloadError = targetOk ? QString() : targetProbeError;
    }
    clearFailure();
    emit connectionStateChanged(true);
    return true;
}

void ControllerDeviceBackend::disconnectBackend()
{
    ControllerDebugClient* client = nullptr;
    bool wasOnline = false;
    {
        QMutexLocker lock(&m_mutex);
        client = m_client;
        wasOnline = m_online;
        m_online = false;
        m_targetOnline = false;
        m_downloading = false;
        m_downloadPercent = 0;
    }

    if (client) {
        client->close();
    }
    if (wasOnline) {
        emit connectionStateChanged(false);
    }
}

bool ControllerDeviceBackend::isOnline() const
{
    QMutexLocker lock(&m_mutex);
    return m_online && m_client && m_client->isConnected();
}

BackendStatusSnapshot ControllerDeviceBackend::statusSnapshot() const
{
    bool online = false;
    bool downloading = false;
    int downloadPercent = 0;
    bool targetOnline = false;
    int targetId = 0;
    ControllerDebugStatus status;
    QString lastDownloadError;
    QVariantMap configSnapshot;
    QVariantMap pointSummary;
    QVariantMap lastTargetProbe;
    int configuredDeviceId = 1;
    {
        QMutexLocker lock(&m_mutex);
        online = m_online;
        downloading = m_downloading;
        downloadPercent = m_downloadPercent;
        targetOnline = m_targetOnline;
        targetId = m_targetDeviceId;
        status = m_status;
        lastDownloadError = m_lastDownloadError;
        configSnapshot = m_config.transport.parameters;
        configuredDeviceId = m_config.controller.modbusSlaveId;
        pointSummary = pointMappingSummary();
        lastTargetProbe = m_lastTargetProbe;
    }
    const CommError err = lastError();

    BackendStatusSnapshot snapshot;
    snapshot.online = online;
    snapshot.backendType = QStringLiteral("controller");
    snapshot.downloading = downloading;
    snapshot.downloadPercent = downloadPercent;
    snapshot.lastErrorCode = err.code;
    snapshot.lastErrorMessage = err.message;
    snapshot.lastErrorDetails = err.details;
    snapshot.timestamp = QDateTime::currentDateTimeUtc();
    snapshot.extras.insert(QStringLiteral("backend"), snapshot.backendType);
    snapshot.extras.insert(QStringLiteral("online"), snapshot.online);
    snapshot.extras.insert(QStringLiteral("deviceId"), configSnapshot.value(QStringLiteral("deviceId"), configuredDeviceId));
    snapshot.extras.insert(QStringLiteral("port"), configSnapshot.value(QStringLiteral("port")));
    snapshot.extras.insert(QStringLiteral("targetOnline"), targetOnline);
    snapshot.extras.insert(QStringLiteral("targetDeviceId"), targetId);
    snapshot.extras.insert(QStringLiteral("state"), status.state);
    snapshot.extras.insert(QStringLiteral("workMode"), status.workMode);
    snapshot.extras.insert(QStringLiteral("componentLine"), status.componentLine);
    snapshot.extras.insert(QStringLiteral("reset"), status.reset);
    snapshot.extras.insert(QStringLiteral("version"), status.version);
    snapshot.extras.insert(QStringLiteral("downloading"), downloading);
    snapshot.extras.insert(QStringLiteral("downloadPercent"), downloadPercent);
    snapshot.extras.insert(QStringLiteral("pointMappings"), pointSummary);
    snapshot.extras.insert(QStringLiteral("preflight"), buildPreflightReport(nullptr, nullptr));
    if (!lastDownloadError.isEmpty()) {
        snapshot.extras.insert(QStringLiteral("lastDownloadError"), lastDownloadError);
    }
    if (!lastTargetProbe.isEmpty()) {
        snapshot.extras.insert(QStringLiteral("lastTargetProbe"), lastTargetProbe);
    }
    return snapshot;
}

QVariantMap ControllerDeviceBackend::buildRtuConfig() const
{
    QVariantMap cfg = ControllerDebugProtocol::defaultRtuConfig(QString(), deviceId());
    const QVariantMap transport = m_config.transport.parameters;
    const QVariantMap legacy = m_config.commParameters;

    const QString configuredPortName = firstNonEmptyString(transport,
                                                           legacy,
                                                           {QStringLiteral("port"),
                                                            QStringLiteral("portName"),
                                                            QStringLiteral("deviceName")});
    cfg.insert(QStringLiteral("port"), configuredPortName);
    cfg.insert(QStringLiteral("baudRate"),
               firstPositiveInt(transport, legacy, {QStringLiteral("baudRate"), QStringLiteral("baud")}, cfg.value(QStringLiteral("baudRate")).toInt()));
    cfg.insert(QStringLiteral("responseTimeout"),
               firstPositiveInt(transport, legacy, {QStringLiteral("responseTimeout"), QStringLiteral("timeoutMs")}, cfg.value(QStringLiteral("responseTimeout")).toInt()));
    cfg.insert(QStringLiteral("retryCount"),
               firstPositiveInt(transport, legacy, {QStringLiteral("retryCount"), QStringLiteral("retries")}, cfg.value(QStringLiteral("retryCount")).toInt()));

    const QString parity = firstNonEmptyString(transport, legacy, {QStringLiteral("parity")});
    if (!parity.isEmpty()) {
        cfg.insert(QStringLiteral("parity"), parity);
    }
    cfg.insert(QStringLiteral("dataBits"),
               firstPositiveInt(transport, legacy, {QStringLiteral("dataBits")}, cfg.value(QStringLiteral("dataBits")).toInt()));
    cfg.insert(QStringLiteral("stopBits"),
               firstPositiveInt(transport, legacy, {QStringLiteral("stopBits")}, cfg.value(QStringLiteral("stopBits")).toInt()));
    cfg.insert(QStringLiteral("address"), deviceId());
    return cfg;
}

int ControllerDeviceBackend::deviceId() const
{
    const QVariantMap transport = m_config.transport.parameters;
    const QVariantMap legacy = m_config.commParameters;
    return firstPositiveInt(transport,
                            legacy,
                            {QStringLiteral("deviceId"),
                             QStringLiteral("slaveId"),
                             QStringLiteral("unitId"),
                             QStringLiteral("address")},
                            m_config.controller.modbusSlaveId);
}

int ControllerDeviceBackend::targetDeviceId() const
{
    const QVariantMap target = m_config.target.parameters;
    const QVariantMap bridge = m_config.bridge.parameters;
    const QVariantMap transport = m_config.transport.parameters;

    int id = 0;
    if (readPositiveInt(target, {QStringLiteral("deviceId"), QStringLiteral("slaveId"), QStringLiteral("unitId")}, &id)) {
        return id;
    }
    if (readPositiveInt(bridge.value(QStringLiteral("target")).toMap(),
                        {QStringLiteral("deviceId"), QStringLiteral("slaveId"), QStringLiteral("unitId")},
                        &id)) {
        return id;
    }
    if (readPositiveInt(bridge.value(QStringLiteral("targetProbe")).toMap(),
                        {QStringLiteral("slaveId"), QStringLiteral("deviceId"), QStringLiteral("unitId")},
                        &id)) {
        return id;
    }
    if (readPositiveInt(transport,
                        {QStringLiteral("targetDeviceId"), QStringLiteral("targetSlaveId"), QStringLiteral("targetUnitId")},
                        &id)) {
        return id;
    }
    bool ok = false;
    id = m_config.target.nodeId.toInt(&ok);
    return ok && id > 0 ? id : deviceId();
}

QString ControllerDeviceBackend::portName() const
{
    return firstNonEmptyString(m_config.transport.parameters,
                               m_config.commParameters,
                               {QStringLiteral("port"),
                                QStringLiteral("portName"),
                                QStringLiteral("deviceName")});
}

bool ControllerDeviceBackend::ensureConfigured(QString* errorMessage) const
{
    QMutexLocker lock(&m_mutex);
    if (m_configured) {
        return true;
    }
    const QString message = QStringLiteral("控制器后端尚未配置。");
    if (errorMessage) {
        *errorMessage = message;
    }
    return false;
}

bool ControllerDeviceBackend::ensureOnline(QString* errorMessage) const
{
    if (!ensureConfigured(errorMessage)) {
        return false;
    }
    QMutexLocker lock(&m_mutex);
    if (m_online && m_client && m_client->isConnected()) {
        return true;
    }
    const QString message = QStringLiteral("控制器后端未连接。");
    if (errorMessage) {
        *errorMessage = message;
    }
    return false;
}

void ControllerDeviceBackend::updateStatusCache(const ControllerDebugStatus& status)
{
    m_status = status;
}

bool ControllerDeviceBackend::setOperationError(CommErrorCode code,
                                                const QString& message,
                                                CommError* operationError,
                                                QString* errorMessage,
                                                const QString& details)
{
    if (errorMessage) {
        *errorMessage = message;
    }
    if (operationError) {
        *operationError = CommError(CommProtocolType::ModbusRTU, code, message, details);
    }
    setFailure(code, message, details);
    return false;
}

CommError ControllerDeviceBackend::currentDebugError(const QString& fallbackMessage) const
{
    if (m_client) {
        const CommError err = m_client->lastError();
        if (err.isError()) {
            return err;
        }
    }
    return CommError(CommProtocolType::ModbusRTU, CommErrorCode::UnknownError, fallbackMessage);
}

void ControllerDeviceBackend::setFailure(CommErrorCode code, const QString& message, const QString& details)
{
    reportError(code, message, details);
}

void ControllerDeviceBackend::clearFailure()
{
    clearError();
}

QString ControllerDeviceBackend::firstNonEmptyString(const QVariantMap& primary,
                                                     const QVariantMap& secondary,
                                                     const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString value = primary.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    for (const QString& key : keys) {
        const QString value = secondary.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return QString();
}

int ControllerDeviceBackend::firstPositiveInt(const QVariantMap& primary,
                                              const QVariantMap& secondary,
                                              const QStringList& keys,
                                              int fallback)
{
    for (const QString& key : keys) {
        bool ok = false;
        const int value = primary.value(key).toInt(&ok);
        if (ok && value > 0) {
            return value;
        }
    }
    for (const QString& key : keys) {
        bool ok = false;
        const int value = secondary.value(key).toInt(&ok);
        if (ok && value > 0) {
            return value;
        }
    }
    return fallback;
}

quint16 ControllerDeviceBackend::boundedLine(int lineNumber)
{
    return static_cast<quint16>(qBound(0, lineNumber, 0xffff));
}
