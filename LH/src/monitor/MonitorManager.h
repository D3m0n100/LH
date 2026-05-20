// 文件：src/monitor/MonitorManager.h
// 监控管理器（性能优化版本）
// 支持增量数据分发 + 监控采样落库（DataManager）

#ifndef MONITOR_MANAGER_H
#define MONITOR_MANAGER_H

#include <QObject>
#include <QMap>
#include <QReadWriteLock>
#include <QTimer>
#include <memory>
#include "MonitorTypes.h"

// 前向声明
class MonitorDataProcessor;
struct ProjectRuntimeConfig;

namespace Monitor {

class MonitorChannel;
class MonitorDataLogger;

/**
 * @brief MonitorManager
 *
 * 监控管理器单例类（性能优化版本），职责：
 * - 通道的注册、查询、移除
 * - 采集器的生命周期管理
 * - 数据记录与增量分发
 * - 监控采样落库（可选开关）
 *
 * 【线程安全】
 * - 通道相关操作使用 m_channelLock 保护
 * - recordSample() 可在任意线程调用
 */
class MonitorManager : public QObject
{
    Q_OBJECT

public:
    static MonitorManager& instance();

    // =========================================================================
    // 数据处理器连接（新增）
    // =========================================================================

    /**
     * @brief 设置数据处理器
     * 设置后，所有记录的样本会自动增量分发到处理器
     */
    void setDataProcessor(MonitorDataProcessor* processor);
    MonitorDataProcessor* dataProcessor() const { return m_dataProcessor; }

    // =========================================================================
    // 通道管理
    // =========================================================================

    bool registerChannel(const ChannelConfig& config);
    bool removeChannel(const QString& name);
    bool hasChannel(const QString& name) const;
    QStringList channelNames() const;

    std::shared_ptr<MonitorChannel> channel(const QString& name) const;
    ChannelConfig channelConfig(const QString& name) const;
    void updateChannelConfig(const QString& name, const ChannelConfig& config);

    // =========================================================================
    // 数据记录（增量分发 + 可选落库）
    // =========================================================================

    /**
     * @brief 记录采样数据（线程安全）
     * 会同时：
     * 1) 追加到对应通道（内存 history，不破坏旧行为）
     * 2) 增量分发到 MonitorDataProcessor（如果已设置）
     * 3) 可选：写入 DataManager（监控采样落库，支持批量写入）
     */
    void recordSample(const QString& channelName,
                      double value,
                      const QString& unit = QString(),
                      const QVariantMap& metadata = {});

    /// 记录采样数据（完整版本）
    void recordSample(const Sample& sample);

    /// 批量记录采样数据
    void recordSamples(const QString& channelName, const QList<Sample>& samples);

    /// 获取通道的历史数据（内存）
    QList<Sample> history(const QString& channelName, int count = 100) const;

    /// 获取通道的时间范围内数据（内存）
    QList<Sample> history(const QString& channelName,
                          const QDateTime& start,
                          const QDateTime& end) const;

    // =========================================================================
    // 最小历史查询能力（数据库）
    // =========================================================================

    /**
     * @brief 从 DataManager 查询最近 N 条记录
     * @note variableName 使用 channelName
     */
    QList<Sample> historyFromDatabase(const QString& channelName, int count = 100) const;

    /**
     * @brief 从 DataManager 查询指定时间窗口记录
     */
    QList<Sample> historyFromDatabase(const QString& channelName,
                                      const QDateTime& start,
                                      const QDateTime& end) const;

    // =========================================================================
    // 采集器管理
    // =========================================================================

    bool registerProvider(const ProviderConfig& config);
    bool unregisterProvider(const QString& providerId);
    bool hasProvider(const QString& providerId) const;
    QStringList providerIds() const;

    // =========================================================================
    // 监控控制
    // =========================================================================

    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const { return m_isMonitoring; }

    // =========================================================================
    // 数据清理
    // =========================================================================

    void setDataRetentionDays(int days);
    int dataRetentionDays() const { return m_dataRetentionDays; }

    void clearChannelData(const QString& channelName);
    void clearAllData();

    // =========================================================================
    // 配置应用（新增）
    // =========================================================================

    /**
     * @brief 应用运行时配置
     * @param config 项目运行时配置
     * @return 是否成功应用
     */
    bool applyConfiguration(const ProjectRuntimeConfig& config);

    // =========================================================================
    // 监控采样落库开关
    // =========================================================================

    /**
     * @brief 启用/禁用监控采样落库
     * @note 关闭后仍保留内存 history 行为
     */
    void setDatabaseLoggingEnabled(bool enabled);
    bool isDatabaseLoggingEnabled() const;

    /**
     * @brief 当前是否具备从数据库查询历史的条件
     * DataManager 已初始化且落库未被关闭
     */
    bool isDatabaseHistoryAvailable() const;

    /// 立即 flush（用于 stopMonitoring / aboutToQuit）
    void flushDatabaseLogging();

signals:
    /// 通道注册
    void channelRegistered(const QString& channelName);

    /// 通道移除
    void channelRemoved(const QString& channelName);

    /// 通道列表改变
    void channelsChanged();

    /// 采样数据记录（用于 UI 更新）
    void sampleRecorded(const QString& channelName,
                        double value,
                        const QString& unit,
                        const QDateTime& timestamp);

    /// 批量采样数据记录
    void samplesRecorded(const QString& channelName, int count);

    /// 阈值超限
    void thresholdExceeded(const QString& channelName,
                           double value,
                           double thresholdValue);

private slots:
    void onCleanupTimeout();
    void onProviderTimeout();

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
    // 通道存储
    QMap<QString, std::shared_ptr<MonitorChannel>> m_channels;
    mutable QReadWriteLock m_channelLock;

    // 采集器存储
    QMap<QString, ProviderConfig> m_providers;
    QMap<QString, QTimer*> m_providerTimers;
    mutable QReadWriteLock m_providerLock;

    // 数据处理器（增量分发目标）
    MonitorDataProcessor* m_dataProcessor = nullptr;

    // 监控采样落库（批量写入）
    std::unique_ptr<MonitorDataLogger> m_dataLogger;

    // 清理定时器
    QTimer* m_cleanupTimer;
    int m_dataRetentionDays;

    // 状态
    bool m_isMonitoring;

    // 常量
    static const int DEFAULT_DATA_RETENTION_DAYS = 7;
    static const int DEFAULT_CLEANUP_INTERVAL_MS = 3600000;  // 1 小时
    static constexpr bool DEFAULT_DB_LOGGING_ENABLED = true;
};

} // namespace Monitor

#endif // MONITOR_MANAGER_H
