// File: src/monitor/MonitorManager.h
#ifndef MONITOR_MANAGER_H
#define MONITOR_MANAGER_H

#include <QObject>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QMap>
#include <QMetaObject>
#include <QMutex>
#include <QPointer>
#include <QReadWriteLock>
#include <QTimer>

#include <atomic>
#include <memory>

#include "MonitorTypes.h"
#include "IMonitorHistoryStore.h"
#include "core/DataManager.h"
#include "MonitorHistoryService.h"

namespace Core { class AsyncDatabaseWorker; }
class MonitorDataProcessor;
class IDeviceBackend;
struct ProjectRuntimeConfig;

namespace Monitor {

class MonitorChannel;
class MonitorDataLogger;

/**
 * @brief 数据库历史分页转换结果
 *
 * MonitorManager 只负责把 RuntimeRecord 转成 Sample；每次调用只持有
 * DataManager 的单页锁，调用方可以在页与页之间执行耗时的文件写入。
 */


class MonitorManager : public QObject
{
    Q_OBJECT

public:
    static MonitorManager& instance();
    explicit MonitorManager(QObject* parent = nullptr);
    ~MonitorManager() override;

    /// 注入/配置历史数据存储后端；传入 nullptr 时清空存储后端
    void setHistoryStore(std::shared_ptr<IMonitorHistoryStore> store);
    std::shared_ptr<IMonitorHistoryStore> historyStore() const;

    bool startDatabaseService(const QString& databasePath);
    void startDatabaseServiceAsync(const QString& databasePath);
    Core::AsyncDatabaseWorker* databaseWorker() const { return m_databaseWorker.get(); }
    void requestDatabaseHistoryPage(QObject* context, const QString& channel, const QDateTime& start,
        const QDateTime& end, int size, const RuntimeHistoryCursor& cursor, int latestCount,
        std::function<void(DatabaseHistoryPage)> completed);
    void setDataProcessor(MonitorDataProcessor* processor);
    MonitorDataProcessor* dataProcessor() const;

    void setDeviceBackend(IDeviceBackend* backend);
    IDeviceBackend* deviceBackend() const;

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

    /**
     * @brief 数据库历史的 keyset 分页转换接口
     */
    DatabaseHistoryPage historyFromDatabasePage(
        const QString& channelName,
        const QDateTime& start,
        const QDateTime& end,
        int pageSize,
        const RuntimeHistoryCursor& cursor = {}) const;

    /**
     * @brief 时间窗无数据时使用的最近 maxCount 条升序分页接口
     */
    DatabaseHistoryPage historyFromDatabaseLatestPage(
        const QString& channelName,
        int maxCount,
        int pageSize,
        const RuntimeHistoryCursor& cursor = {},
        const QDateTime& end = QDateTime()) const;

    /// 轻量统计时间窗内数据库历史记录数，不构造完整列表；maxRecordId >= 0 时限定记录 ID 上界，-1 表示不限制。
    RuntimeHistoryCount historyFromDatabaseCount(
        const QString& channelName,
        const QDateTime& start,
        const QDateTime& end,
        qint64 maxRecordId = -1) const;

    /// 轻量统计最近 maxCount 条数据库历史记录数；end 有效时限定时间上界，maxRecordId >= 0 时限定记录 ID 上界，-1 表示不限制。
    RuntimeHistoryCount historyFromDatabaseLatestCount(
        const QString& channelName,
        int maxCount,
        const QDateTime& end = QDateTime(),
        qint64 maxRecordId = -1) const;

    bool registerProvider(const ProviderConfig& config);
    bool unregisterProvider(const QString& providerId);
    bool hasProvider(const QString& providerId) const;
    QStringList providerIds() const;

    void startMonitoring();
    void stopMonitoring();
    void shutdown();
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
    void databaseServiceStarted(bool success, const QString& error);
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
    void thresholdCleared(const QString& channelName,
                          double value,
                          double thresholdValue);

private slots:
    void onCleanupTimeout();
    void onProviderTimeout();
    void onBackendPollTimeout();

private:
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

    QPointer<MonitorDataProcessor> m_dataProcessor;
    QMetaObject::Connection m_dataProcessorDestroyedConnection;

    QPointer<IDeviceBackend> m_backend;
    QTimer* m_backendPollTimer = nullptr;
    QStringList m_backendPointIds;
    QHash<QString, QString> m_pointIdToChannel;
    QHash<QString, int> m_backendPointPeriodsMs;
    QHash<QString, qint64> m_backendPointNextDueMs;
    QElapsedTimer m_backendPollClock;
    quint64 m_backendPollGeneration = 0;
    bool m_backendPollPending = false;
    std::shared_ptr<std::atomic_bool> m_backendPollCancelled;
    QMetaObject::Connection m_backendPointsChangedConnection;
    QMetaObject::Connection m_backendConnectionStateConnection;
    QMetaObject::Connection m_backendDestroyedConnection;

    std::unique_ptr<Core::AsyncDatabaseWorker> m_databaseWorker;
    quint64 m_databaseServiceGeneration = 0;
    bool m_databaseServiceStarting = false;
    std::unique_ptr<MonitorDataLogger> m_dataLogger;
    std::shared_ptr<IMonitorHistoryStore> m_historyStore;

    QTimer* m_cleanupTimer = nullptr;
    int m_dataRetentionDays = 0;

    std::atomic_bool m_isMonitoring{false};

    struct AlarmRateLimit {
        qint64 lastLogTimeUtcMs = 0;
        int suppressedCount = 0;
    };
    QMap<QString, AlarmRateLimit> m_alarmRateLimits;
    mutable QMutex m_alarmMutex;

    static const int DEFAULT_DATA_RETENTION_DAYS = 7;
    static const int DEFAULT_CLEANUP_INTERVAL_MS = 3600000;
    static constexpr bool DEFAULT_DB_LOGGING_ENABLED = true;
};

} // namespace Monitor

#endif // MONITOR_MANAGER_H
