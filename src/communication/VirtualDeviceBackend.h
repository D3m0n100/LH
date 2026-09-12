// File: src/communication/VirtualDeviceBackend.h
// 虚拟设备后端 — 用于无真实硬件时的开发/测试/仿真

#ifndef VIRTUALDEVICEBACKEND_H
#define VIRTUALDEVICEBACKEND_H

#include "IDeviceBackend.h"
#include "common/RuntimePointTypes.h"
#include "common/RuntimePointTable.h"

#include <QHash>
#include <QMutex>
#include <QVariant>

class VirtualDeviceBackend : public IDeviceBackend
{
    Q_OBJECT
public:
    explicit VirtualDeviceBackend(QObject* parent = nullptr);
    ~VirtualDeviceBackend() override = default;

    // ── 点定义管理 ───────────────────────────────────────

    void loadPointDefinitions(const QList<RuntimePointDefinition>& defs);
    void loadPointDefinitions(const RuntimePointTable& table);
    void clearPoints();

    // ── IDeviceBackend 接口 ──────────────────────────────

    bool connectBackend() override;
    void disconnectBackend() override;
    bool isOnline() const override;

    bool readPoints(const QStringList& pointIds,
                    QHash<QString, QVariant>& values,
                    QString* errorMessage,
                    QHash<QString, CommError>* pointErrors = nullptr) override;

    bool writePoints(const QHash<QString, QVariant>& writes,
                     QString* errorMessage,
                     QHash<QString, CommError>* pointErrors = nullptr) override;

    bool downloadArtifact(const QString& artifactPath,
                          const QVariantMap& options,
                          QString* errorMessage,
                          CommError* operationError = nullptr) override;

    BackendStatusSnapshot statusSnapshot() const override;

    // ── 故障注入（测试用）────────────────────────────────

    void setFaultInjection(bool readFail, bool writeFail, bool downloadFail);
    void setSimulatedLatencyMs(int ms);
    void setDownloadFaultInjection(int attempts,
                                   CommErrorCode code = CommErrorCode::InternalError,
                                   const QString& message = QString());
    void clearDownloadFaultInjection();

private:
    bool checkOnline(QString* errorMessage) const;
    bool checkReadFault(QString* errorMessage) const;
    bool checkWriteFault(QString* errorMessage) const;
    bool checkDownloadFault(QString* errorMessage) const;

    mutable QMutex m_mutex;
    bool m_online = false;

    QHash<QString, RuntimePointDefinition> m_definitions;
    QHash<QString, QVariant> m_values;

    // 下载状态
    bool m_downloading = false;
    int m_downloadPercent = 0;
    QString m_lastDownloadError;

    // 故障注入
    bool m_faultRead = false;
    bool m_faultWrite = false;
    bool m_faultDownload = false;
    int m_simulatedLatencyMs = 0;
    bool m_lastOperationPartialSuccess = false;
    int m_downloadFailureAttemptsRemaining = 0;
    CommErrorCode m_downloadFailureCode = CommErrorCode::InternalError;
    QString m_downloadFailureMessage;
};

#endif // VIRTUALDEVICEBACKEND_H
