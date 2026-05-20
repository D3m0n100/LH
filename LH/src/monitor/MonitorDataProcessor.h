// 文件：src/monitor/MonitorDataProcessor.h
// 监控数据处理器（性能优化版本）
// 支持增量数据处理、环形缓冲区、限频渲染

#ifndef MONITOR_DATA_PROCESSOR_H
#define MONITOR_DATA_PROCESSOR_H

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QDateTime>
#include <QMap>
#include <QMutex>
#include <QReadWriteLock>
#include <QElapsedTimer>
#include <optional>
#include <memory>
#include <atomic>
#include <map>          // 添加：std::map 支持 unique_ptr

#include "MonitorTypes.h"
#include "RingBuffer.h"

namespace Monitor {
struct Sample;
class MonitorChannel;
}

/**
 * @brief 单通道的增量数据缓冲
 */
struct ChannelDeltaBuffer
{
    QVector<QPointF> deltaPoints;       ///< 增量数据点
    qint64 lastConsumeTimeMs = 0;       ///< 上次消费时间
    int totalAppended = 0;              ///< 总追加数量（统计用）
    int totalConsumed = 0;              ///< 总消费数量（统计用）
    
    void clear() {
        deltaPoints.clear();
    }
    
    bool hasData() const {
        return !deltaPoints.isEmpty();
    }
    
    int size() const {
        return deltaPoints.size();
    }
};

/**
 * @brief 处理后的通道数据
 */
struct ProcessedChannelData
{
    QString channelName;
    QString unit;
    QVector<QPointF> seriesPoints;      ///< 图表数据点 (timestamp_ms, value)
    double currentValue = 0.0;
    std::optional<double> minValue;
    std::optional<double> maxValue;
    std::optional<double> avgValue;
    QDateTime lastUpdateTime;
    int rawSampleCount = 0;
    int processedCount = 0;
    
    // 增量相关
    QVector<QPointF> deltaPoints;       ///< 本次增量数据
    bool isIncremental = false;         ///< 是否为增量更新
};

/**
 * @brief 数据处理配置
 */
struct DataProcessorConfig
{
    // ===== 窗口配置 =====
    int maxDisplaySamples = 200;            ///< 最大显示样本数
    qint64 timeWindowMs = 30000;            ///< 时间窗口（毫秒）
    int ringBufferCapacity = 5000;          ///< 环形缓冲区容量
    
    // ===== 处理配置 =====
    bool enableSmoothing = false;
    int smoothingWindow = 3;
    bool enableDownsampling = false;
    int downsampleFactor = 2;
    std::optional<double> minClamp;
    std::optional<double> maxClamp;
    
    // ===== 增量处理配置 =====
    bool enableIncrementalMode = true;      ///< 启用增量模式
    int batchProcessThreshold = 50;         ///< 批量处理阈值
    int maxDeltaBufferSize = 1000;          ///< 增量缓冲区最大容量
    
    // ===== 自适应降采样 =====
    bool enableAdaptiveDownsample = true;   ///< 启用自适应降采样
    int adaptiveDownsampleThreshold = 3000; ///< 自适应降采样触发阈值
};

/**
 * @brief MonitorDataProcessor
 * 
 * 监控数据处理器（性能优化版本），负责：
 * - 使用环形缓冲区管理每个通道的数据
 * - 支持增量数据追加和消费
 * - 时间窗口裁剪
 * - 限幅、平滑、降采样
 * - 统计值计算
 * - 自适应降采样（高负载时自动启用）
 * 
 * 【性能优化要点】
 * - 增量追加：只处理新增数据，避免全量重算
 * - 环形缓冲区：O(1) 淘汰旧数据，内存使用固定
 * - 批量处理：累积一定量增量后批量处理
 * - 自适应降采样：数据量大时自动降低分辨率
 * 
 * 【线程安全】
 * - 使用读写锁保护缓冲区
 * - appendSamples 可在任意线程调用
 * - drainDelta 应在 UI 线程调用
 */
class MonitorDataProcessor : public QObject
{
    Q_OBJECT

public:
    explicit MonitorDataProcessor(QObject* parent = nullptr);
    ~MonitorDataProcessor() override;

    // =========================================================================
    // 配置接口
    // =========================================================================
    
    DataProcessorConfig config() const { return m_config; }
    void setConfig(const DataProcessorConfig& config);
    
    void setTimeWindow(qint64 windowMs);
    qint64 timeWindow() const { return m_config.timeWindowMs; }
    
    void setMaxDisplaySamples(int maxSamples);
    int maxDisplaySamples() const { return m_config.maxDisplaySamples; }
    
    void setRingBufferCapacity(int capacity);
    int ringBufferCapacity() const { return m_config.ringBufferCapacity; }
    
    void setClampRange(std::optional<double> minVal, std::optional<double> maxVal);
    void setSmoothing(bool enabled, int windowSize = 3);
    void setDownsampling(bool enabled, int factor = 2);
    void setIncrementalMode(bool enabled);

    // =========================================================================
    // 增量数据接口（核心优化）
    // =========================================================================
    
    /**
     * @brief 追加采样数据（线程安全）
     * @param channelId 通道 ID
     * @param sample 采样数据
     */
    void appendSample(const QString& channelId, const Monitor::Sample& sample);
    
    /**
     * @brief 批量追加采样数据（线程安全）
     * @param channelId 通道 ID
     * @param samples 采样数据列表
     */
    void appendSamples(const QString& channelId, const QList<Monitor::Sample>& samples);
    
    /**
     * @brief 追加数据点（线程安全）
     * @param channelId 通道 ID
     * @param point 数据点 (x=时间戳ms, y=值)
     */
    void appendPoint(const QString& channelId, const QPointF& point);
    
    /**
     * @brief 批量追加数据点（线程安全）
     * @param channelId 通道 ID
     * @param points 数据点列表
     */
    void appendPoints(const QString& channelId, const QVector<QPointF>& points);
    
    /**
     * @brief 消费增量数据（获取并清空）
     * @param channelId 通道 ID
     * @return 增量数据点列表
     */
    QVector<QPointF> drainDelta(const QString& channelId);
    
    /**
     * @brief 消费所有通道的增量数据
     * @return 通道ID -> 增量数据点 的映射
     */
    QMap<QString, QVector<QPointF>> drainAllDeltas();
    
    /**
     * @brief 检查通道是否有待消费的增量数据
     */
    bool hasPendingDelta(const QString& channelId) const;
    
    /**
     * @brief 获取待消费增量数据的数量
     */
    int pendingDeltaCount(const QString& channelId) const;

    // =========================================================================
    // 全量数据接口（兼容旧接口）
    // =========================================================================
    
    /**
     * @brief 从 MonitorChannel 处理数据（全量）
     */
    ProcessedChannelData processChannel(
        const std::shared_ptr<Monitor::MonitorChannel>& channel);
    
    /**
     * @brief 处理原始样本列表（全量）
     */
    ProcessedChannelData processSamples(
        const QString& channelName,
        const QList<Monitor::Sample>& samples);
    
    /**
     * @brief 获取通道的完整数据（从环形缓冲区）
     */
    QVector<QPointF> getChannelData(const QString& channelId) const;
    
    /**
     * @brief 获取通道最近 N 个点
     */
    QVector<QPointF> getRecentPoints(const QString& channelId, int count) const;
    
    /**
     * @brief 获取通道在时间窗口内的数据
     */
    QVector<QPointF> getPointsInTimeWindow(const QString& channelId) const;

    // =========================================================================
    // 缓存管理
    // =========================================================================
    
    /// 获取缓存的处理结果
    ProcessedChannelData getCachedData(const QString& channelName) const;
    
    /// 检查是否有缓存数据
    bool hasCachedData(const QString& channelName) const;
    
    /// 清除指定通道缓存
    void clearChannelCache(const QString& channelName);
    
    /// 清除所有缓存
    void clearAllCache();
    
    /// 确保通道存在（如不存在则创建）
    void ensureChannel(const QString& channelId);

    // =========================================================================
    // 性能统计
    // =========================================================================
    
    /// 获取性能统计信息
    Monitor::PerformanceStats getPerformanceStats() const;
    
    /// 重置性能统计
    void resetPerformanceStats();

    // =========================================================================
    // 静态工具方法
    // =========================================================================
    
    static QVector<QPointF> samplesToPoints(const QList<Monitor::Sample>& samples);
    static std::optional<double> calculateMin(const QVector<QPointF>& points);
    static std::optional<double> calculateMax(const QVector<QPointF>& points);
    static std::optional<double> calculateAvg(const QVector<QPointF>& points);

signals:
    void configChanged();
    void channelDataUpdated(const QString& channelId);
    void deltaDataReady(const QString& channelId, int count);

private:
    /// 通道缓冲区数据
    struct ChannelBuffer {
        Monitor::RingBuffer<QPointF> ringBuffer;    ///< 环形缓冲区
        ChannelDeltaBuffer deltaBuffer;             ///< 增量缓冲
        QString unit;
        double lastValue = 0.0;
        qint64 lastTimestampMs = 0;
        
        ChannelBuffer(int capacity = 5000) : ringBuffer(capacity) {}
    };

    /// 获取或创建通道缓冲区
    ChannelBuffer& getOrCreateBuffer(const QString& channelId);
    
    /// 应用处理管道
    QVector<QPointF> applyProcessingPipeline(const QVector<QPointF>& points) const;
    QVector<QPointF> applyClamp(const QVector<QPointF>& points) const;
    QVector<QPointF> applySmoothing(const QVector<QPointF>& points) const;
    QVector<QPointF> applyDownsampling(const QVector<QPointF>& points) const;
    QVector<QPointF> limitPointCount(const QVector<QPointF>& points) const;
    
    /// 时间窗口裁剪
    QVector<QPointF> applyTimeWindow(const QVector<QPointF>& points) const;
    
    /// 自适应降采样
    QVector<QPointF> applyAdaptiveDownsample(const QVector<QPointF>& points) const;

private:
    DataProcessorConfig m_config;
    
    // 通道缓冲区（channelId -> ChannelBuffer）
    // 修复：使用 std::map 替代 QMap，因为 QMap 不支持 unique_ptr（需要可拷贝类型）
    mutable std::map<QString, std::unique_ptr<ChannelBuffer>> m_channelBuffers;
    mutable QReadWriteLock m_bufferLock;
    
    // 处理结果缓存
    mutable QMap<QString, ProcessedChannelData> m_cachedData;
    mutable QMutex m_cacheMutex;
    
    // 性能统计
    mutable std::atomic<int> m_totalAppendCount{0};
    mutable std::atomic<int> m_totalConsumeCount{0};
    QElapsedTimer m_statsTimer;
    mutable Monitor::PerformanceStats m_perfStats;
};

#endif // MONITOR_DATA_PROCESSOR_H
