// File: src/monitor/MonitorManager.h
#ifndef MONITOR_MANAGER_H
#define MONITOR_MANAGER_H

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QMetaObject>
#include <QReadWriteLock>
#include <QTimer>

#include <atomic>
#include <memory>

#include "MonitorTypes.h"

class MonitorDataProcessor;
class IDeviceBackend;
struct ProjectRuntimeConfig;

namespace Monitor {

class MonitorChannel;
class MonitorDataLogger;

class MonitorManager : public QObject
{
    Q_OBJECT

public:
    static MonitorManager& instance();

    void setDataProcessor(MonitorDataProcessor* processor);
    MonitorDataProcessor* dataProcessor() const { return m_dataProcessor.load(std::memory_order_acquire); }

    void setDeviceBackend(IDeviceBackend* backend);
    IDeviceBackend* deviceBackend() const { return m_backend; }

    bool registerChannel(const ChannelConfig& config);
    bool removeChannel(const QString& name);
    bool hasChannel(const QString& name) const;
    QStringList channelNames() const;

    std::shared_ptr<MonitorChannel> channel(const QString& name) const;
    ChannelConfig channelConfig(const QString& name) const;
    void updateChannelConfig(const QString& name, const ChannelConfig& config);

    void recordSample(const QString& channelName,
                      double value,
                      const QString& unit = QString(),
                      const QVariantMap& metadata = {});
    void recordSample(const Sample& sample);
    void recordSamples(const QString& channelName, const QList<Sample>& samples);

    QList<Sample> history(const QString& channelName, int count = 100) const;
    QList<Sample> history(const QString& channelName,
                          const QDateTime& start,
                          const QDateTime& end) const;

    QList<Sample> historyFromDatabase(const QString& channelName, int count = 100) const;
    QList<Sample> historyFromDatabase(const QString& channelName,
                                      const QDateTime& start,
                                      const QDateTime& end) const;

    bool registerProvider(const ProviderConfig& config);
    bool unregisterProvider(const QString& providerId);
    bool hasProvider(const QString& providerId) const;
    QStringList providerIds() const;

    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const { return m_isMonitoring.load(std::memory_order_acquire); }

    void setDataRetentionDays(int days);
    int dataRetentionDays() const { return m_dataRetentionDays; }

    void clearChannelData(const QString& channelName);
    void clearAllData();

    bool applyConfiguration(const ProjectRuntimeConfig& config);

    void setDatabaseLoggingEnabled(bool enabled);
    bool isDatabaseLoggingEnabled() const;
    bool isDatabaseHistoryAvailable() const;
    void flushDatabaseLogging();

signals:
    void channelRegistered(const QString& channelName);
    void channelRemoved(const QString& channelName);
    void channelsChanged();
    void sampleRecorded(const QString& channelName,
                        double value,
                        const QString& unit,
                        const QDateTime& timestamp);
    void samplesRecorded(const QString& channelName, int count);
    void thresholdExceeded(const QString& channelName,
                           double value,
                           double thresholdValue);

private slots:
    void onCleanupTimeout();
    void onProviderTimeout();
    void onBackendPollTimeout();

private:
    MonitorManager();
    ~MonitorManager();

    MonitorManager(const MonitorManager&) = delete;
    MonitorManager& operator=(const MonitorManager&) = delete;

    void setupCleanupTimer();
    void connectChannelSignals(const std::shared_ptr<MonitorChannel>& channel);
    void dispatchToProcessor(const QString& channelName, const Sample& sample);
    void dispatchToProcessor(const QString& channelName, const QList<Sample>& samples);

private:
    QMap<QString, std::shared_ptr<MonitorChannel>> m_channels;
    mutable QReadWriteLock m_channelLock;

    QMap<QString, ProviderConfig> m_providers;
    QMap<QString, QTimer*> m_providerTimers;
    mutable QReadWriteLock m_providerLock;

    std::atomic<MonitorDataProcessor*> m_dataProcessor{nullptr};

    IDeviceBackend* m_backend = nullptr;
    QTimer* m_backendPollTimer = nullptr;
    QStringList m_backendPointIds;
    QHash<QString, QString> m_pointIdToChannel;
    QMetaObject::Connection m_backendPointsChangedConnection;
    QMetaObject::Connection m_backendConnectionStateConnection;

    std::unique_ptr<MonitorDataLogger> m_dataLogger;

    QTimer* m_cleanupTimer = nullptr;
    int m_dataRetentionDays = 0;

    std::atomic_bool m_isMonitoring{false};

    static const int DEFAULT_DATA_RETENTION_DAYS = 7;
    static const int DEFAULT_CLEANUP_INTERVAL_MS = 3600000;
    static constexpr bool DEFAULT_DB_LOGGING_ENABLED = true;
};

} // namespace Monitor

#endif // MONITOR_MANAGER_H
