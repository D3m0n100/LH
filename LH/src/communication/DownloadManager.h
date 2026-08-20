// File: src/communication/DownloadManager.h

#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QMutex>
#include <QPointer>
#include <QThread>
#include <QVariantMap>

#include <atomic>
#include <memory>

class ControllerBridge;

struct DownloadCancellationHandle
{
    std::atomic_bool requested{false};
    QMutex bridgeMutex;
    ControllerBridge* bridge = nullptr;

    void request();
    void setBridge(ControllerBridge* value);
    void clearBridge();
    bool isRequested() const
    {
        return requested.load(std::memory_order_acquire);
    }
};

class DownloadManager : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Idle,
        ConnectSerial,
        HandshakeController,
        ProbeTarget,
        Transfer,
        Verify,
        Finish,
        Fail
    };
    Q_ENUM(State)

    enum class ErrorCode {
        NoError = 0,
        SERIAL_OPEN_FAIL,
        SERIAL_TIMEOUT,
        CONTROLLER_NO_RESPONSE,
        CONTROLLER_PROTOCOL_ERROR,
        TARGET_NO_RESPONSE,
        CAN_TIMEOUT,
        DOWNLOAD_NACK,
        DOWNLOAD_VERIFY_FAIL,
        INVALID_CONFIG,
        DEVICE_BUSY,
        CANCELLED
    };
    Q_ENUM(ErrorCode)

    explicit DownloadManager(QObject* parent = nullptr);
    ~DownloadManager() override;

    void setConfig(const QVariantMap& cfg);

public slots:
    void startConnectProbe();
    void startDownload(const QString& profileJsonPath, const QString& payloadFilePath);
    void cancel();

signals:
    void statusChanged(State st, const QString& msg);
    void progressChanged(int percent, int sentBytes, int totalBytes, int pktIndex, int pktCount);
    void errorOccurred(ErrorCode code, const QString& msg, const QString& details);
    void logLine(const QString& line);

private:
    QVariantMap m_cfg;
    QThread m_thread;
    QPointer<QObject> m_activeWorker;
    std::shared_ptr<DownloadCancellationHandle> m_activeCancelState;
};

#endif // DOWNLOADMANAGER_H
