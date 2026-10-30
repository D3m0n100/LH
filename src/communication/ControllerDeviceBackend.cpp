// File: src/communication/ControllerDeviceBackend.cpp

#include "ControllerDeviceBackend.h"

#include "Communication.h"
#include "ControllerDebugProtocol.h"
#include "ProtocolUtils.h"

#include "BackendOperationGuard.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMutexLocker>
#include <QtGlobal>
#include <QDeadlineTimer>
#include <QTimer>
#include <QPointer>
#include <QScopeGuard>

using Communication::BackendOperationGuard;

namespace {

using Communication::readPositiveInt;
} // namespace

ControllerDeviceBackend::ControllerDeviceBackend(QObject* parent)
    : IDeviceBackend(parent)
    , m_ownedClient(new ControllerDebugClient(this))
    , m_client(m_ownedClient.data())
    , m_portOwnerToken(QStringLiteral("formal-backend-%1")
                               .arg(QString::number(reinterpret_cast<quintptr>(this), 16)))
{
    m_ownedClient->enableWorkerThread();
}

ControllerDeviceBackend::~ControllerDeviceBackend()
{
    if (m_connectCancelled) m_connectCancelled->store(true);
    if (m_debugCancelled) m_debugCancelled->store(true);
    cancelDownload();
    if (m_client) m_client->waitForIdle();
    disconnectBackend();
    if (m_client) m_client->waitForIdle();
}

bool ControllerDeviceBackend::configure(const ProjectRuntimeConfig& config, QString* errorMessage)
{
    BackendOperationGuard operation(&m_operationMutex);
    if (m_asyncConnectActive || m_asyncDownloadActive || m_asyncReadActive || !operation.tryLock()) {
        if (errorMessage) *errorMessage = QStringLiteral("Controller is busy; configuration was not changed");
        return false;
    }
    QMutexLocker lock(&m_mutex);
    if (m_online || m_portClaimed || m_disconnectPending) {
        if (m_configured && m_config.toJson() == config.toJson()) return true;
        if (errorMessage) *errorMessage = QStringLiteral("请先断开控制器，再应用新的连接配置。");
        return false;
    }
    const ProjectRuntimeConfig previousConfig = m_config;
    const bool previouslyConfigured = m_configured;
    const auto rollback = [&]() {
        m_config = previousConfig;
        m_configured = previouslyConfigured;
        loadPointDefinitions(RuntimePointConverter::fromProjectConfig(previousConfig));
    };
    m_config = config;
    m_configured = true;
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
        rollback();
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
        rollback();
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
        rollback();
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
        rollback();
        const QString message = QStringLiteral("控制器后端配置失败：装置号必须为 1..63。");
        if (errorMessage) {
            *errorMessage = message;
        }
        lock.unlock();
        setFailure(CommErrorCode::InvalidAddress, message, QStringLiteral("deviceId=%1").arg(id));
        return false;
    }

    m_lastDownloadError.clear();
    m_targetOnline = false;
    m_targetDeviceId = targetDeviceId();
    m_lastTargetProbe.clear();
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
    return connectBackendWithBudget(3000, nullptr, true);
}

void ControllerDeviceBackend::connectBackendAsync(int budgetMs, std::shared_ptr<std::atomic_bool> cancelled,
        QObject* context, std::function<void(bool, QString)> completed,
        bool publishConnectionOnSuccess)
{
    if (!context || !completed) return;
    if (!cancelled) cancelled = std::make_shared<std::atomic_bool>(false);
    if (budgetMs <= 0 || m_asyncConnectActive.exchange(true)) {
        completed(false, QStringLiteral("控制器正在连接或连接期限无效")); return;
    }
    emit asyncOperationBusyChanged(true);
    m_connectCancelled = cancelled;
    auto finished = std::make_shared<std::atomic_bool>(false);
    QPointer<QObject> guard(context);
    QPointer<ControllerDeviceBackend> self(this);
    const QDeadlineTimer deadline(budgetMs);
    QTimer::singleShot(budgetMs, context, [=] {
        if (!finished->exchange(true)) {
            cancelled->store(true);
            completed(false, QStringLiteral("控制器连接超过总期限"));
        }
    });
    auto job = [=] {
        if (!guard) cancelled->store(true);
        bool ok = !cancelled->load() && !deadline.hasExpired()
            && connectBackendWithBudget(int(deadline.remainingTime()), cancelled.get(), false);
        QString message = ok ? QString() : lastError().message;
        if (!guard) cancelled->store(true);
        if (cancelled->load() || deadline.hasExpired()) {
            ok = false;
            message = QStringLiteral("控制器连接已取消或超时");
            // Close on the I/O actor; the GUI never waits for native close.
            m_asyncConnectActive = false;
            disconnectBackend();
        }
        if (guard) QMetaObject::invokeMethod(guard, [=] {
            if (self) {
                self->m_asyncConnectActive = false;
                emit self->asyncOperationBusyChanged(self->hasAsyncOperationInFlight());
            }
            if (finished->exchange(true)) return;
            if (!self) { completed(false, QStringLiteral("控制器后端已释放")); return; }
            if (cancelled->load()) {
                self->disconnectBackend(); completed(false, QStringLiteral("控制器连接已取消")); return;
            }
            if (ok && publishConnectionOnSuccess) emit self->connectionStateChanged(true);
            completed(ok, message);
        }, Qt::QueuedConnection);
    };
    if (m_client && m_client->hasWorkerThread()) m_client->runAsync(std::move(job));
    else QTimer::singleShot(0, this, std::move(job));
}

bool ControllerDeviceBackend::connectBackendWithBudget(int budgetMs, const std::atomic_bool* cancelled, bool publishConnection)
{
    QDeadlineTimer deadline(qMax(1, budgetMs));
    if (cancelled && cancelled->load()) return false;
    if (m_asyncReadActive || m_asyncDownloadActive || m_disconnectPending) {
        setFailure(CommErrorCode::DeviceBusy, QStringLiteral("控制器正在处理请求或断开连接，请等待完成"));
        return false;
    }
    BackendOperationGuard operation(&m_operationMutex);
    if (!operation.tryLock()) {
        const QString message = QStringLiteral("控制器后端正忙，无法连接。");
        setFailure(CommErrorCode::DeviceBusy, message);
        return false;
    }

    ControllerDebugClient* client = nullptr;
    QVariantMap cfg;
    int id = 1;
    QString port;
    {
        QMutexLocker lock(&m_mutex);
        if (m_online && m_client && m_client->isConnected()) {
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
        port = cfg.value(QStringLiteral("port")).toString().trimmed();
    }

    if (!client) {
        const QString message = QStringLiteral("控制器调试客户端不可用。");
        setFailure(CommErrorCode::InvalidConfig, message);
        return false;
    }

    QString currentOwner;
    if (!Communication::tryClaimRtuPort(port, m_portOwnerToken, &currentOwner)) {
        const QString message = currentOwner.isEmpty()
                ? QStringLiteral("控制器后端无法占用串口 %1。").arg(port)
                : QStringLiteral("串口 %1 正被其它通信 owner 占用：%2。")
                          .arg(port, currentOwner);
        setFailure(CommErrorCode::DeviceBusy,
                   message,
                   currentOwner.isEmpty()
                           ? QString()
                           : QStringLiteral("owner=%1").arg(currentOwner));
        return false;
    }
    {
        QMutexLocker lock(&m_mutex);
        m_portClaimed = true;
        m_claimedPortName = port;
    }
    const auto releaseClaim = [this, port]() {
        bool claimed = false;
        {
            QMutexLocker lock(&m_mutex);
            claimed = m_portClaimed;
            m_portClaimed = false;
            m_claimedPortName.clear();
        }
        if (claimed) {
            Communication::releaseRtuPort(port, m_portOwnerToken);
        }
    };

    if (!client->isConnected() && !client->openRtu(cfg, id)) {
        const CommError err = currentDebugError(QStringLiteral("控制器连接失败。"));
        releaseClaim();
        setFailure(err.code, err.message, err.details);
        return false;
    }

    m_client->setRequestBudget(int(deadline.remainingTime()), cancelled);
    const auto clearBudget = qScopeGuard([this] { m_client->setRequestBudget(-1, nullptr); });
    ControllerDebugStatus status;
    if (!client->readStatus(&status)) {
        const CommError err = currentDebugError(QStringLiteral("控制器状态读取失败。"));
        client->close();
        releaseClaim();
        setFailure(err.code, err.message, err.details);
        return false;
    }

    QString targetProbeError;
    const bool targetOk = probeTarget(&targetProbeError);
    if (deadline.hasExpired() || (cancelled && cancelled->load())) {
        client->close(); releaseClaim();
        setFailure(CommErrorCode::ConnectionTimeout, QStringLiteral("控制器连接已取消或超过总期限"));
        return false;
    }

    {
        QMutexLocker lock(&m_mutex);
        m_online = true;
        m_targetOnline = targetOk;
        updateStatusCache(status);
        m_lastDownloadError = targetOk ? QString() : targetProbeError;
    }
    clearFailure();
    if (publishConnection) emit connectionStateChanged(true);
    return true;
}

void ControllerDeviceBackend::testConnectionAsync(int budgetMs, QObject* context,
        std::function<void(bool, ControllerConnectionDiagnostic, QString)> completed)
{
    if (!context || !completed) return;
    if (budgetMs <= 0 || m_asyncConnectActive || m_asyncDownloadActive || m_asyncReadActive.exchange(true)) {
        completed(false, {}, QStringLiteral("控制器正在处理其他请求")); return;
    }
    const auto cancelled = std::make_shared<std::atomic_bool>(false);
    m_debugCancelled = cancelled;
    const auto finished = std::make_shared<std::atomic_bool>(false);
    const QDeadlineTimer deadline(budgetMs);
    const QPointer<QObject> guard(context);
    QTimer::singleShot(budgetMs, context, [=] {
        if (!finished->exchange(true)) {
            cancelled->store(true); completed(false, {}, QStringLiteral("控制器诊断超过总期限"));
        }
    });
    auto job = [=] {
        ControllerConnectionDiagnostic diagnostic;
        QString error;
        bool ok = false;
        {
            BackendOperationGuard operation(&m_operationMutex);
            if (!cancelled->load() && !deadline.hasExpired() && operation.tryLock()) {
                m_client->setRequestBudget(int(deadline.remainingTime()), cancelled.get());
                ok = testConnection(&diagnostic, &error);
                m_client->setRequestBudget(-1, nullptr);
            }
        }
        if (cancelled->load() || deadline.hasExpired()) { ok = false; error = QStringLiteral("控制器诊断已取消或超时"); }
        m_asyncReadActive = false;
        if (guard) QMetaObject::invokeMethod(guard, [=] {
            if (!finished->exchange(true)) completed(ok, diagnostic, error);
        }, Qt::QueuedConnection);
    };
    if (m_client && m_client->hasWorkerThread()) m_client->runAsync(std::move(job));
    else QTimer::singleShot(0, this, std::move(job));
}

void ControllerDeviceBackend::disconnectBackend()
{
    if (m_connectCancelled) m_connectCancelled->store(true);
    if (m_debugCancelled) m_debugCancelled->store(true);
    if (m_disconnectPending) return;
    if (m_client && m_client->hasWorkerThread()
        && QThread::currentThread() == thread()) {
        if (m_disconnectPending.exchange(true)) return;
        QString port;
        bool wasClaimed, wasOnline;
        {
            QMutexLocker lock(&m_mutex);
            port = m_claimedPortName;
            wasClaimed = m_portClaimed;
            wasOnline = m_online;
            m_online = m_targetOnline = m_portClaimed = false;
            m_claimedPortName.clear();
        }
        auto* client = m_client;
        const auto token = m_portOwnerToken;
        client->runAsync([this, client, port, token, wasClaimed] {
            BackendOperationGuard operation(&m_operationMutex);
            operation.lock();
            client->close();
            if (wasClaimed) Communication::releaseRtuPort(port, token);
            m_disconnectPending = false;
            m_asyncConnectActive = false;
            emit asyncOperationBusyChanged(hasAsyncOperationInFlight());
        });
        if (wasOnline) emit connectionStateChanged(false);
        return;
    }
    BackendOperationGuard operation(&m_operationMutex);
    operation.lock();

    ControllerDebugClient* client = nullptr;
    bool wasOnline = false;
    bool wasClaimed = false;
    QString port;
    {
        QMutexLocker lock(&m_mutex);
        client = m_client;
        wasOnline = m_online;
        wasClaimed = m_portClaimed;
        port = m_claimedPortName;
        m_online = false;
        m_targetOnline = false;
        m_portClaimed = false;
        m_claimedPortName.clear();
        m_downloading = false;
        m_downloadPercent = 0;
    }

    if (client) {
        client->close();
    }
    if (wasClaimed) {
        Communication::releaseRtuPort(port, m_portOwnerToken);
    }
    if (wasOnline) {
        emit connectionStateChanged(false);
    }
}

bool ControllerDeviceBackend::tryBeginOperation(QString* errorMessage)
{
    if (m_operationMutex.tryLock()) {
        return true;
    }

    const QString message = QStringLiteral("控制器后端正忙，操作被拒绝。");
    if (errorMessage) {
        *errorMessage = message;
    }
    setFailure(CommErrorCode::DeviceBusy, message);
    return false;
}

void ControllerDeviceBackend::endOperation()
{
    m_operationMutex.unlock();
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
        pointSummary = pointMappingSummary(false);
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
