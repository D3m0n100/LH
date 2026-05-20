// File: src/communication/DownloadManager.cpp

#include "DownloadManager.h"

#include "Communication.h"
#include "ControllerBridge.h"
#include "DownloadProfile.h"
#include "ICommInterface.h"

#include <QFile>
#include <QMetaObject>
#include <QPointer>

#include <atomic>

class DownloadWorker : public QObject
{
    Q_OBJECT
public:
    explicit DownloadWorker(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    void setConfig(const QVariantMap& cfg) { m_cfg = cfg; }

public slots:
    void requestCancel()
    {
        m_cancelRequested = true;
        if (m_bridge) {
            m_bridge->requestAbort();
        }
    }

    void doConnectProbe()
    {
        m_cancelRequested = false;
        emit statusChanged(DownloadManager::State::ConnectSerial, "Opening serial(ModbusRTU)...");

        ICommInterface* iface = Communication::createAndOpen(m_cfg.value("comm").toMap(), nullptr);
        m_iface = iface;
        ControllerBridge* bridge = Communication::getControllerBridge(iface);
        m_bridge = bridge;

        if (!iface) {
            emit errorOccurred(DownloadManager::ErrorCode::SERIAL_OPEN_FAIL,
                               "Open communication failed",
                               "Communication::createAndOpen returned nullptr");
            emit finished();
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
            delete iface;
            m_iface = nullptr;
            m_bridge = nullptr;
            emit finished();
            return;
        }

        QObject::connect(bridge, &ControllerBridge::logLine, this, &DownloadWorker::logLine);
        QObject::connect(bridge, &ControllerBridge::downloadProgress, this, &DownloadWorker::progressChanged);

        if (m_cancelRequested) {
            emit errorOccurred(DownloadManager::ErrorCode::DOWNLOAD_NACK, "Cancelled", "Cancelled before handshake");
            delete iface;
            m_iface = nullptr;
            m_bridge = nullptr;
            emit finished();
            return;
        }

        emit statusChanged(DownloadManager::State::HandshakeController, "Handshaking controller...");
        if (!bridge->handshake()) {
            const CommError last = iface->lastError();
            emit errorOccurred(DownloadManager::ErrorCode::CONTROLLER_NO_RESPONSE,
                               "Controller no response (serial OK but controller offline)",
                               QString("lastError=%1 %2").arg(int(last.code)).arg(last.message));
            delete iface;
            m_iface = nullptr;
            m_bridge = nullptr;
            emit finished();
            return;
        }

        emit statusChanged(DownloadManager::State::ProbeTarget, "Probing target via controller->CAN...");
        if (!bridge->probeTarget()) {
            const CommError last = iface->lastError();
            emit errorOccurred(DownloadManager::ErrorCode::TARGET_NO_RESPONSE,
                               "Target no response (controller online but target offline)",
                               QString("lastError=%1 %2").arg(int(last.code)).arg(last.message));
            delete iface;
            m_iface = nullptr;
            m_bridge = nullptr;
            emit finished();
            return;
        }

        emit statusChanged(DownloadManager::State::Finish, "Connect/Probe OK");
        delete iface;
        m_iface = nullptr;
        m_bridge = nullptr;
        emit finished();
    }

    void doDownload(const QString& profileJsonPath, const QString& payloadFilePath)
    {
        m_cancelRequested = false;

        DownloadProfile profile;
        QString perr;
        if (!DownloadProfile::fromJsonFile(profileJsonPath, profile, &perr)) {
            emit errorOccurred(DownloadManager::ErrorCode::INVALID_CONFIG, "Failed to load profile", perr);
            emit finished();
            return;
        }

        QFile pf(payloadFilePath);
        if (!pf.open(QIODevice::ReadOnly)) {
            emit errorOccurred(DownloadManager::ErrorCode::INVALID_CONFIG, "Failed to load payload", pf.errorString());
            emit finished();
            return;
        }
        const QByteArray payload = pf.readAll();

        emit statusChanged(DownloadManager::State::ConnectSerial, "Opening serial(ModbusRTU)...");
        ICommInterface* iface = Communication::createAndOpen(m_cfg.value("comm").toMap(), nullptr);
        m_iface = iface;
        ControllerBridge* bridge = Communication::getControllerBridge(iface);
        m_bridge = bridge;

        if (!iface) {
            emit errorOccurred(DownloadManager::ErrorCode::SERIAL_OPEN_FAIL,
                               "Open communication failed",
                               "Communication::createAndOpen returned nullptr");
            emit finished();
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
            delete iface;
            m_iface = nullptr;
            m_bridge = nullptr;
            emit finished();
            return;
        }

        QObject::connect(bridge, &ControllerBridge::logLine, this, &DownloadWorker::logLine);
        QObject::connect(bridge, &ControllerBridge::downloadProgress, this, &DownloadWorker::progressChanged);

        if (m_cancelRequested) {
            emit errorOccurred(DownloadManager::ErrorCode::DOWNLOAD_NACK, "Cancelled", "Cancelled before transfer");
            delete iface;
            m_iface = nullptr;
            m_bridge = nullptr;
            emit finished();
            return;
        }

        emit statusChanged(DownloadManager::State::HandshakeController, "Handshaking controller...");
        if (!bridge->handshake()) {
            const CommError last = iface->lastError();
            emit errorOccurred(DownloadManager::ErrorCode::CONTROLLER_NO_RESPONSE,
                               "Controller no response (serial OK but controller offline)",
                               QString("lastError=%1 %2").arg(int(last.code)).arg(last.message));
            delete iface;
            m_iface = nullptr;
            m_bridge = nullptr;
            emit finished();
            return;
        }

        emit statusChanged(DownloadManager::State::ProbeTarget, "Probing target via controller->CAN...");
        if (!bridge->probeTarget()) {
            const CommError last = iface->lastError();
            emit errorOccurred(DownloadManager::ErrorCode::TARGET_NO_RESPONSE,
                               "Target no response (controller online but target offline)",
                               QString("lastError=%1 %2").arg(int(last.code)).arg(last.message));
            delete iface;
            m_iface = nullptr;
            m_bridge = nullptr;
            emit finished();
            return;
        }

        emit statusChanged(DownloadManager::State::Transfer, "Downloading...");
        if (!bridge->download(profile, payload)) {
            const CommError last = iface->lastError();
            const DownloadManager::ErrorCode code =
                (last.code == CommErrorCode::OperationCancelled)
                    ? DownloadManager::ErrorCode::DOWNLOAD_VERIFY_FAIL
                    : DownloadManager::ErrorCode::DOWNLOAD_NACK;
            emit errorOccurred(code,
                               "Download failed during transfer",
                               QString("lastError=%1 %2").arg(int(last.code)).arg(last.message));
            delete iface;
            m_iface = nullptr;
            m_bridge = nullptr;
            emit finished();
            return;
        }

        emit statusChanged(DownloadManager::State::Finish, "Download success");
        delete iface;
        m_iface = nullptr;
        m_bridge = nullptr;
        emit finished();
    }

signals:
    void statusChanged(DownloadManager::State st, const QString& msg);
    void progressChanged(int percent, int sentBytes, int totalBytes, int pktIndex, int pktCount);
    void errorOccurred(DownloadManager::ErrorCode code, const QString& msg, const QString& details);
    void logLine(const QString& line);
    void finished();

private:
    QVariantMap m_cfg;
    QPointer<ICommInterface> m_iface;
    QPointer<ControllerBridge> m_bridge;
    std::atomic_bool m_cancelRequested{false};
};

DownloadManager::DownloadManager(QObject* parent)
    : QObject(parent)
{
}

DownloadManager::~DownloadManager()
{
    m_thread.quit();
    m_thread.wait();
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
    m_activeWorker = worker;
    worker->setConfig(m_cfg);
    worker->moveToThread(&m_thread);

    connect(&m_thread, &QThread::started, worker, &DownloadWorker::doConnectProbe);
    connect(worker, &DownloadWorker::statusChanged, this, &DownloadManager::statusChanged);
    connect(worker, &DownloadWorker::progressChanged, this, &DownloadManager::progressChanged);
    connect(worker, &DownloadWorker::errorOccurred, this, &DownloadManager::errorOccurred);
    connect(worker, &DownloadWorker::logLine, this, &DownloadManager::logLine);

    connect(worker, &DownloadWorker::finished, &m_thread, &QThread::quit);
    connect(worker, &DownloadWorker::finished, worker, &QObject::deleteLater);
    connect(worker, &DownloadWorker::finished, this, [this]() { m_activeWorker = nullptr; });

    m_thread.start();
}

void DownloadManager::startDownload(const QString& profileJsonPath, const QString& payloadFilePath)
{
    if (m_thread.isRunning()) {
        return;
    }

    auto* worker = new DownloadWorker();
    m_activeWorker = worker;
    worker->setConfig(m_cfg);
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
    connect(worker, &DownloadWorker::finished, this, [this]() { m_activeWorker = nullptr; });

    m_thread.start();
}

void DownloadManager::cancel()
{
    if (!m_activeWorker) {
        return;
    }
    QMetaObject::invokeMethod(m_activeWorker, "requestCancel", Qt::QueuedConnection);
}

#include "DownloadManager.moc"
