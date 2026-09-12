#ifndef CLASSICOPCSERVER_H
#define CLASSICOPCSERVER_H

#include "IOpcServer.h"

#include <QHash>
#include <QTimer>

class ModbusInterface;

class ClassicOpcServer : public IOpcServer
{
    Q_OBJECT
public:
    explicit ClassicOpcServer(QObject* parent = nullptr);

    bool applyConfig(const OpcServerConfig& config, QString* errorMessage = nullptr) override;
    bool start(QString* errorMessage = nullptr) override;
    void stop() override;
    bool isRunning() const override { return m_running; }

    void setRuntimePoints(const QList<RuntimePointDefinition>& points) override;
    void setOpcTags(const QList<OpcTagDefinition>& tags) override;
    void updatePointValues(const QList<RuntimePointValue>& values) override;
    void recordWriteResult(const QString& pointId, bool success, const QString& message) override;
    BackendStatusSnapshot statusSnapshot() const override;

private:
    struct AddressingInfo
    {
        QString area;
        int address = -1;
        int unitId = 1;
        int bitOffset = 0;
        int elementCount = 1;
        QString mode;
    };

    void rebuildMappings();
    void startPolling();
    void stopPolling();
    void pollDevice();
    bool refreshPointValue(const RuntimePointDefinition& point, QString* errorMessage = nullptr);
    bool writePointValue(const RuntimePointDefinition& point, const QVariant& value, QString* errorMessage = nullptr);
    bool applyPointValue(const RuntimePointDefinition& point, const QVariant& value, QString* errorMessage = nullptr);
    bool ensureModbusOpen(QString* errorMessage = nullptr);
    ModbusConfig toModbusConfig() const;
    static bool parseSerialMode(const QString& serialMode,
                                int* baudRate,
                                int* dataBits,
                                int* stopBits,
                                QString* parity,
                                QString* errorMessage = nullptr);
    static AddressingInfo parseAddressing(const RuntimePointDefinition& point);
    QString nodePathForTag(const OpcTagDefinition& tag) const;
    bool routeWriteRequest(const QString& nodePath, const QVariant& value, QString* errorMessage = nullptr);
    static bool isConfigValid(const OpcServerConfig& config, QString* errorMessage = nullptr);

    OpcServerConfig m_config;
    bool m_running = false;
    ModbusInterface* m_modbus = nullptr;
    QTimer m_pollTimer;
    QList<RuntimePointDefinition> m_points;
    QList<OpcTagDefinition> m_tags;
    QHash<QString, RuntimePointDefinition> m_pointById;
    QHash<QString, QString> m_pointToNodePath;
    QHash<QString, QString> m_nodePathToPoint;
    QHash<QString, AddressingInfo> m_pointAddressing;
    QHash<QString, RuntimePointValue> m_values;
    QDateTime m_lastPollTime;
    QDateTime m_lastSuccessfulPollTime;
    QDateTime m_lastStatusChangeTime;
    CommErrorCode m_lastErrorCode = CommErrorCode::NoError;
    QString m_lastErrorMessage;
    QString m_lastWriteNodePath;
    QString m_lastWritePointId;
    QVariant m_lastWriteValue;
    QDateTime m_lastWriteTime;
    bool m_lastWriteSuccess = false;
    QString m_lastWriteMessage;
    QDateTime m_lastSuccessfulWriteTime;
    QDateTime m_lastFailedWriteTime;
    QString m_lastSuccessfulWriteMessage;
    QString m_lastFailedWriteMessage;
    int m_successfulPollCount = 0;
    int m_failedPollCount = 0;
    int m_successfulWriteCount = 0;
    int m_failedWriteCount = 0;
    int m_addressedPointCount = 0;
    int m_unresolvedPointCount = 0;
};

#endif // CLASSICOPCSERVER_H
