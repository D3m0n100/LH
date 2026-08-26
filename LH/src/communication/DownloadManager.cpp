// File: src/communication/DownloadManager.cpp

#include "DownloadManager.h"

#include "Communication.h"
#include "ControllerBridge.h"
#include "DownloadProfile.h"
#include "ICommInterface.h"

#include <QFile>
#include <QMutexLocker>
#include <QPointer>

void DownloadCancellationHandle::request()
{
    requested.store(true, std::memory_order_release);
    QMutexLocker lock(&bridgeMutex);
    if (bridge)
        bridge->requestAbort();
}

void DownloadCancellationHandle::setBridge(ControllerBridge* value)
{
    QMutexLocker lock(&bridgeMutex);
    bridge = value;
    if (bridge && requested.load(std::memory_order_acquire))
        bridge->requestAbort();
}

void DownloadCancellationHandle::clearBridge()
{
    QMutexLocker lock(&bridgeMutex);
    bridge = nullptr;
}

class DownloadWorker : public QObject
{
    Q_OBJECT
public:
    explicit DownloadWorker(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    void setConfig(const QVariantMap& cfg) { m_cfg = cfg; }
    void setCancellationState(const std::shared_ptr<DownloadCancellationHandle>& state)
    {
        m_cancelState = state;
    }

public slots:
    void requestCancel()
    {
        if (m_cancelState)
            m_cancelState->request();
    }

    void doConnectProbe()
    {
        if (!claimRtuPort()) {
            emit errorOccurred(DownloadManager::ErrorCode::DEVICE_BUSY,
                               "RTU port busy",
                               m_claimErrorDetails);
            finish();
            return;
        }
        emit statusChanged(DownloadManager::State::ConnectSerial, "Opening serial(ModbusRTU)...");

        ICommInterface* iface = Communication::createAndOpen(m_cfg.value("comm").toMap(), nullptr);
        m_iface = iface;
        ControllerBridge* bridge = Communication::getControllerBridge(iface);
        if (m_cancelState)
            m_cancelState->setBridge(bridge);

        if (!iface) {
            emit errorOccurred(DownloadManager::ErrorCode::SERIAL_OPEN_FAIL,
                               "Open communication failed",
                               "Communication::createAndOpen returned nullptr");
            finish();
            return;
        }

        QObject::connect(iface,
                         static_cast<void (ICommInterface::*)(const CommError&)>(&ICommInterface::errorOccurred),
                         this,
                         [this](const CommError& e) {
                             emit logLine(QString("[COMM_ERR] code=%1 msg=%2 details=%3")
                                              .arg(int(e.code))
                                              .arg(e.message)
                                              .arg(e.details));
                         });

        if (!bridge) {
            emit errorOccurred(DownloadManager::ErrorCode::INVALID_CONFIG,
                               "ControllerBridge not available",
                               "Ensure comm.enableBridge=true and comm.bridge.* configured");
            destroyInterface();
            finish();
            return;
        }

        QObject::connect(bridge, &ControllerBridge::logLine, this, &DownloadWorker::logLine);
        QObject::connect(bridge, &ControllerBridge::downloadProgress, this, &DownloadWorker::progressChanged);

        if (isCancelRequested()) {
            emit errorOccurred(DownloadManager::ErrorCode::CANCELLED, "Cancelled", "Cancelled before handshake");
            destroyInterface();
            finish();
            return;
        }

        emit statusChanged(DownloadManager::State::HandshakeController, "Handshaking controller...");
        if (!bridge->handshake()) {
            const CommError last = iface->lastError();
            emit errorOccurred(DownloadManager::ErrorCode::CONTROLLER_NO_RESPONSE,
                               "Controller no response (serial OK but controller offline)",
                               QString("lastError=%1 %2").arg(int(last.code)).arg(last.message));
            destroyInterface();
            finish();
            return;
        }

        emit statusChanged(DownloadManager::State::ProbeTarget, "Probing target via controller->CAN...");
        if (!bridge->probeTarget()) {
            const CommError last = iface->lastError();
            emit errorOccurred(DownloadManager::ErrorCode::TARGET_NO_RESPONSE,
                               "Target no response (controller online but target offline)",
                               QString("lastError=%1 %2").arg(int(last.code)).arg(last.message));
            destroyInterface();
            finish();
            return;
        }

        emit statusChanged(DownloadManager::State::Finish, "Connect/Probe OK");
        destroyInterface();
        finish();
    }

    void doDownload(const QString& profileJsonPath, const QString& payloadFilePath)
    {
        if (!claimRtuPort()) {
            emit errorOccurred(DownloadManager::ErrorCode::DEVICE_BUSY,
                               "RTU port busy",
                               m_claimErrorDetails);
            finish();
            return;
        }

        DownloadProfile profile;
        QString perr;
        if (!DownloadProfile::fromJsonFile(profileJsonPath, profile, &perr)) {
            emit errorOccurred(DownloadManager::ErrorCode::INVALID_CONFIG, "Failed to load profile", perr);
            finish();
            return;
        }

        QFile pf(payloadFilePath);
        if (!pf.open(QIODevice::ReadOnly)) {
            emit errorOccurred(DownloadManager::ErrorCode::INVALID_CONFIG, "Failed to load payload", pf.errorString());
            finish();
            return;
        }
        const QByteArray payload = pf.readAll();

        emit statusChanged(DownloadManager::State::ConnectSerial, "Opening serial(ModbusRTU)...");
        ICommInterface* iface = Communication::createAndOpen(m_cfg.value("comm").toMap(), nullptr);
        m_iface = iface;
        ControllerBridge* bridge = Communication::getControllerBridge(iface);
        if (m_cancelState)
            m_cancelState->setBridge(bridge);

        if (!iface) {
            emit errorOccurred(DownloadManager::ErrorCode::SERIAL_OPEN_FAIL,
                               "Open communication failed",
                               "Communication::createAndOpen returned nullptr");
            finish();
            return;
        }

        QObject::connect(iface,
                         static_cast<void (ICommInterface::*)(const CommError&)>(&ICommInterface::errorOccurred),
                         this,
                         [this](const CommError& e) {
                             emit logLine(QString("[COMM_ERR] code=%1 msg=%2 details=%3")
                                              .arg(int(e.code))
                                              .arg(e.message)
                                              .arg(e.details));
                         });

        if (!bridge) {
            emit errorOccurred(DownloadManager::ErrorCode::INVALID_CONFIG,
                               "ControllerBridge not available",
                               "Ensure comm.enableBridge=true and comm.bridge.* configured");
            destroyInterface();
            finish();
            return;
        }

        QObject::connect(bridge, &ControllerBridge::logLine, this, &DownloadWorker::logLine);
        QObject::connect(bridge, &ControllerBridge::downloadProgress, this, &DownloadWorker::progressChanged);

        if (isCancelRequested()) {
            emit errorOccurred(DownloadManager::ErrorCode::CANCELLED, "Cancelled", "Cancelled before transfer");
            destroyInterface();
            finish();
            return;
        }

        emit statusChanged(DownloadManager::State::HandshakeController, "Handshaking controller...");
        if (!bridge->handshake()) {
            const CommError last = iface->lastError();
            emit errorOccurred(DownloadManager::ErrorCode::CONTROLLER_NO_RESPONSE,
                               "Controller no response (serial OK but controller offline)",
                               QString("lastError=%1 %2").arg(int(last.code)).arg(last.message));
            destroyInterface();
            finish();
            return;
        }

        emit statusChanged(DownloadManager::State::ProbeTarget, "Probing target via controller->CAN...");
        if (!bridge->probeTarget()) {
            const CommError last = iface->lastError();
            emit errorOccurred(DownloadManager::ErrorCode::TARGET_NO_RESPONSE,
                               "Target no response (controller online but target offline)",
                               QString("lastError=%1 %2").arg(int(last.code)).arg(last.message));
            destroyInterface();
            finish();
            return;
        }

        emit statusChanged(DownloadManager::State::Transfer, "Downloading...");
        if (isCancelRequested()) {
            emit errorOccurred(DownloadManager::ErrorCode::CANCELLED, "Cancelled", "Cancelled before transfer");
            destroyInterface();
            finish();
            return;
        }
        if (!bridge->download(profile, payload)) {
            const CommError last = iface->lastError();
            const DownloadManager::ErrorCode code =
                (last.code == CommErrorCode::OperationCancelled)
                    ? DownloadManager::ErrorCode::CANCELLED
                    : DownloadManager::ErrorCode::DOWNLOAD_NACK;
            emit errorOccurred(code,
                               "Download failed during transfer",
                               QString("lastError=%1 %2").arg(int(last.code)).arg(last.message));
            destroyInterface();
            finish();
            return;
        }

        emit statusChanged(DownloadManager::State::Finish, "Download success");
        destroyInterface();
        finish();
    }

signals:
    void statusChanged(DownloadManager::State st, const QString& msg);
    void progressChanged(int percent, int sentBytes, int totalBytes, int pktIndex, int pktCount);
    void errorOccurred(DownloadManager::ErrorCode code, const QString& msg, const QString& details);
    void logLine(const QString& line);
    void finished();

private:
    bool claimRtuPort()
    {
        const Communication::ResolvedCommConfig resolved =
                Communication::resolveConfig(m_cfg.value("comm").toMap());
        if (resolved.type != CommProtocolType::ModbusRTU) {
            return true;
        }

        const QString port = resolved.parameters.value(QStringLiteral("port"),
                                                        resolved.parameters.value(QStringLiteral("portName")))
                                      .toString().trimmed();
        if (port.isEmpty()) {
            return true;
        }

        m_claimOwner = QStringLiteral("diagnostic-download-%1")
                .arg(QString::number(reinterpret_cast<quintptr>(this), 16));
        QString currentOwner;
        if (!Communication::tryClaimRtuPort(port, m_claimOwner, &currentOwner)) {
            m_claimErrorDetails = QStringLiteral("port=%1 owner=%2")
                    .arg(port, currentOwner);
            return false;
        }
        m_claimedPort = port;
        m_portClaimed = true;
        return true;
    }

    void destroyInterface()
    {
        QPointer<ICommInterface> iface;
        if (m_cancelState)
            m_cancelState->clearBridge();
        iface = m_iface;
        m_iface = nullptr;
        delete iface.data();
    }

    bool isCancelRequested() const
    {
        return m_cancelState && m_cancelState->isRequested();
    }

    void finish()
    {
        if (m_portClaimed) {
            Communication::releaseRtuPort(m_claimedPort, m_claimOwner);
            m_portClaimed = false;
        }
        emit finished();
    }

    QVariantMap m_cfg;
    QPointer<ICommInterface> m_iface;
    std::shared_ptr<DownloadCancellationHandle> m_cancelState;
    QString m_claimedPort;
    QString m_claimOwner;
    QString m_claimErrorDetails;
    bool m_portClaimed = false;
};

DownloadManager::DownloadManager(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<DownloadManager::ErrorCode>("DownloadManager::ErrorCode");
}

DownloadManager::~DownloadManager()
{
    if (m_activeCancelState)
        m_activeCancelState->request();
    m_thread.quit();
    m_thread.wait();
    m_activeWorker.clear();
    m_activeCancelState.reset();
}

void DownloadManager::setConfig(const QVariantMap& cfg)
{
    m_cfg = cfg;
}

void DownloadManager::startConnectProbe()
{
    if (m_thread.isRunning()) {
        return;
    }

    auto* worker = new DownloadWorker();
    const auto cancelState = std::make_shared<DownloadCancellationHandle>();
    m_activeWorker = worker;
    m_activeCancelState = cancelState;
    worker->setConfig(m_cfg);
    worker->setCancellationState(cancelState);
    worker->moveToThread(&m_thread);

    connect(&m_thread, &QThread::started, worker, &DownloadWorker::doConnectProbe);
    connect(worker, &DownloadWorker::statusChanged, this, &DownloadManager::statusChanged);
    connect(worker, &DownloadWorker::progressChanged, this, &DownloadManager::progressChanged);
    connect(worker, &DownloadWorker::errorOccurred, this, &DownloadManager::errorOccurred);
    connect(worker, &DownloadWorker::logLine, this, &DownloadManager::logLine);

    connect(worker, &DownloadWorker::finished, &m_thread, &QThread::quit);
    connect(worker, &DownloadWorker::finished, worker, &QObject::deleteLater);
    connect(&m_thread, &QThread::finished, worker, &QObject::deleteLater);
    const QPointer<QObject> workerGuard(worker);
    connect(worker, &DownloadWorker::finished, this, [this, workerGuard, cancelState]() {
        if (m_activeWorker.data() == workerGuard.data())
            m_activeWorker.clear();
        if (m_activeCancelState == cancelState)
            m_activeCancelState.reset();
    });
    connect(&m_thread, &QThread::finished, this, [this, workerGuard, cancelState]() {
        if (m_activeWorker.data() == workerGuard.data())
            m_activeWorker.clear();
        if (m_activeCancelState == cancelState)
            m_activeCancelState.reset();
    });

    m_thread.start();
}

void DownloadManager::startDownload(const QString& profileJsonPath, const QString& payloadFilePath)
{
    if (m_thread.isRunning()) {
        return;
    }

    auto* worker = new DownloadWorker();
    const auto cancelState = std::make_shared<DownloadCancellationHandle>();
    m_activeWorker = worker;
    m_activeCancelState = cancelState;
    worker->setConfig(m_cfg);
    worker->setCancellationState(cancelState);
    worker->moveToThread(&m_thread);

    connect(&m_thread, &QThread::started, worker, [worker, profileJsonPath, payloadFilePath]() {
        worker->doDownload(profileJsonPath, payloadFilePath);
    }, Qt::QueuedConnection);

    connect(worker, &DownloadWorker::statusChanged, this, &DownloadManager::statusChanged);
    connect(worker, &DownloadWorker::progressChanged, this, &DownloadManager::progressChanged);
    connect(worker, &DownloadWorker::errorOccurred, this, &DownloadManager::errorOccurred);
    connect(worker, &DownloadWorker::logLine, this, &DownloadManager::logLine);

    connect(worker, &DownloadWorker::finished, &m_thread, &QThread::quit);
    connect(worker, &DownloadWorker::finished, worker, &QObject::deleteLater);
    connect(&m_thread, &QThread::finished, worker, &QObject::deleteLater);
    const QPointer<QObject> workerGuard(worker);
    connect(worker, &DownloadWorker::finished, this, [this, workerGuard, cancelState]() {
        if (m_activeWorker.data() == workerGuard.data())
            m_activeWorker.clear();
        if (m_activeCancelState == cancelState)
            m_activeCancelState.reset();
    });
    connect(&m_thread, &QThread::finished, this, [this, workerGuard, cancelState]() {
        if (m_activeWorker.data() == workerGuard.data())
            m_activeWorker.clear();
        if (m_activeCancelState == cancelState)
            m_activeCancelState.reset();
    });

    m_thread.start();
}

void DownloadManager::cancel()
{
    if (m_activeCancelState)
        m_activeCancelState->request();
}

#include "DownloadManager.moc"
