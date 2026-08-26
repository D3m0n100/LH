// File: src/communication/ControllerDeviceBackend.h
// LH controller device backend built on the existing Modbus debug client.

#ifndef CONTROLLERDEVICEBACKEND_H
#define CONTROLLERDEVICEBACKEND_H

#include "ControllerDebugClient.h"
#include "DownloadProfile.h"
#include "IDeviceBackend.h"
#include "RuntimePointRegisterCodec.h"

#include "common/ConfigTypes.h"
#include "common/RuntimePointTypes.h"

#include <QHash>
#include <QMutex>
#include <QScopedPointer>
#include <QStringList>
#include <QVariant>

struct ControllerConnectionDiagnostic
{
    bool controllerOnline = false;
    bool targetOnline = false;
    int deviceId = 1;
    int targetDeviceId = 0;
    quint16 state = 0;
    quint16 workMode = 0;
    quint16 componentLine = 0;
    quint16 reset = 0;
    quint16 version = 0;
    QString portName;
    QString message;
    QVariantMap extras;
};

class ControllerDeviceBackend : public IDeviceBackend
{
    Q_OBJECT
public:
    explicit ControllerDeviceBackend(QObject* parent = nullptr);
    ~ControllerDeviceBackend() override;

    bool configure(const ProjectRuntimeConfig& config, QString* errorMessage = nullptr);
    ProjectRuntimeConfig runtimeConfig() const;

    void setDebugClientForTest(ControllerDebugClient* client);

    bool connectBackend() override;
    void disconnectBackend() override;
    bool isOnline() const override;

    bool readPoints(const QStringList& pointIds,
                    QHash<QString, QVariant>& values,
                    QString* errorMessage = nullptr,
                    QHash<QString, CommError>* pointErrors = nullptr) override;

    bool writePoints(const QHash<QString, QVariant>& writes,
                     QString* errorMessage = nullptr,
                     QHash<QString, CommError>* pointErrors = nullptr) override;

    bool downloadArtifact(const QString& artifactPath,
                          const QVariantMap& options,
                          QString* errorMessage = nullptr,
                          CommError* operationError = nullptr) override;

    BackendStatusSnapshot statusSnapshot() const override;

    bool pause(QString* errorMessage = nullptr);
    bool resume(QString* errorMessage = nullptr);
    bool step(QString* errorMessage = nullptr);
    bool runToCursor(int lineNumber, QString* errorMessage = nullptr);
    bool setBreakpoints(int firstLine, int secondLine, QString* errorMessage = nullptr);
    bool testConnection(ControllerConnectionDiagnostic* diagnostic = nullptr,
                        QString* errorMessage = nullptr);
    bool tryBeginOperation(QString* errorMessage = nullptr);
    void endOperation();
    bool preflight(QString* errorMessage = nullptr, QVariantMap* report = nullptr) const;
    bool dryRunDownloadArtifact(const QString& artifactPath,
                                const QVariantMap& options,
                                QString* errorMessage = nullptr,
                                CommError* operationError = nullptr,
                                QVariantMap* report = nullptr) const;

private:
    struct PointMapping {
        QString pointId;
        int deviceId = 1;
        int address = -1;
        int count = 1;
        RuntimePointRegisterCodecSpec codec;
        bool readable = true;
        bool writable = false;
    };

    QVariantMap buildRtuConfig() const;
    int deviceId() const;
    int targetDeviceId() const;
    QString portName() const;
    QVariantMap pointMappingSummary(bool includeDetails = true) const;
    QVariantMap buildPreflightReport(QStringList* errors, QStringList* warnings) const;
    bool validateDownloadProfilePlan(const DownloadProfile& profile,
                                     const QByteArray& payload,
                                     QVariantMap* report,
                                     QStringList* errors) const;
    bool ensureConfigured(QString* errorMessage) const;
    bool ensureOnline(QString* errorMessage) const;
    bool executeDebugCommand(bool ok, const QString& action, QString* errorMessage);
    void updateStatusCache(const ControllerDebugStatus& status);
    void loadPointDefinitions(const QList<RuntimePointDefinition>& points);
    bool parsePointMapping(const RuntimePointDefinition& point,
                           PointMapping* mapping,
                           QString* rejectReason = nullptr) const;
    bool probeTarget(QString* errorMessage = nullptr);
    QVariantMap targetProbeConfig() const;
    bool executeDownloadProfile(const DownloadProfile& profile,
                                const QByteArray& payload,
                                QString* errorMessage,
                                CommError* operationError);
    bool executeDownloadStep(const DownloadProfile& profile,
                             const DownloadProfile::Step& step,
                             const QByteArray& payload,
                             int stepIndex,
                             QString* errorMessage,
                             CommError* operationError);
    bool setOperationError(CommErrorCode code,
                           const QString& message,
                           CommError* operationError,
                           QString* errorMessage,
                           const QString& details = QString());
    CommError currentDebugError(const QString& fallbackMessage) const;
    void setFailure(CommErrorCode code, const QString& message, const QString& details = QString());
    void clearFailure();

    static QString firstNonEmptyString(const QVariantMap& primary,
                                       const QVariantMap& secondary,
                                       const QStringList& keys);
    static int firstPositiveInt(const QVariantMap& primary,
                                const QVariantMap& secondary,
                                const QStringList& keys,
                                int fallback);
    static quint16 boundedLine(int lineNumber);

private:
    mutable QMutex m_mutex;
    QMutex m_operationMutex;
    ProjectRuntimeConfig m_config;
    bool m_configured = false;
    bool m_online = false;
    bool m_downloading = false;
    int m_downloadPercent = 0;
    ControllerDebugStatus m_status;
    QString m_lastDownloadError;
    bool m_targetOnline = false;
    int m_targetDeviceId = 0;
    QVariantMap m_lastTargetProbe;
    QHash<QString, RuntimePointDefinition> m_pointDefinitions;
    QHash<QString, PointMapping> m_pointMappings;
    QHash<QString, QString> m_unmappedPointReasons;

    QScopedPointer<ControllerDebugClient> m_ownedClient;
    ControllerDebugClient* m_client = nullptr;
    QString m_portOwnerToken;
    QString m_claimedPortName;
    bool m_portClaimed = false;
};

#endif // CONTROLLERDEVICEBACKEND_H
