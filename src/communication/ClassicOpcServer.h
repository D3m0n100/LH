#ifndef CLASSICOPCSERVER_H
#define CLASSICOPCSERVER_H

#include "IOpcServer.h"
#include "OpcWriteResultState.h"

#include <QHash>
#include <QTimer>
#include <QThread>
#include <atomic>
#include <memory>

class ClassicOpcPollWorker;
class ModbusInterface;

class ClassicOpcServer : public IOpcServer
{
    Q_OBJECT
public:
    explicit ClassicOpcServer(QObject* parent = nullptr);
    ClassicOpcServer(QObject* parent, std::unique_ptr<ModbusInterface> transport);
    ~ClassicOpcServer() override;

    bool applyConfig(const OpcServerConfig& config, QString* errorMessage = nullptr) override;
    // Accepts a configured server session. Serial connection completes on I/O
    // thread; statusSnapshot.modbusConnected and errorOccurred report its outcome.
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
        bool valid = true;
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
    QThread m_ioThread;
    ClassicOpcPollWorker* m_worker = nullptr;
    bool m_modbusConnected = false;
    bool m_pollInFlight = false;
    quint64 m_pollGeneration = 0;
    std::shared_ptr<std::atomic_bool> m_pollCancelled;
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
    OpcWriteResultState m_writeResult;
    QString m_lastWriteNodePath;
    QVariant m_lastWriteValue;
    int m_successfulPollCount = 0;
    int m_failedPollCount = 0;
    int m_addressedPointCount = 0;
    int m_unresolvedPointCount = 0;
};

#endif // CLASSICOPCSERVER_H
