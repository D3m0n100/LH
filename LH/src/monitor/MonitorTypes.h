// 文件：src/monitor/MonitorTypes.h
// 监控模块通用类型定义
// 性能优化版本：添加增量处理相关类型

#ifndef MONITOR_TYPES_H
#define MONITOR_TYPES_H

#include <QString>
#include <QDateTime>
#include <QVariantMap>
#include <QList>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <functional>
#include <optional>

namespace Monitor {

// ============================================================================
// 阈值模式
// ============================================================================

enum class ThresholdMode {
    Above,   // 数值高于阈值触发
    Below    // 数值低于阈值触发
};

// ============================================================================
// 曲线线型枚举
// ============================================================================

enum class LineStyle {
    Solid,
    Dash,
    Dot,
    DashDot,
    DashDotDot
};

enum class MonitorObjectKind {
    Generic,
    Variable,
    Parameter,
    Resource
};

// ============================================================================
// 采样数据结构
// ============================================================================

struct Sample {
    QString channelName;
    double value = 0.0;
    QString unit;
    QDateTime timestamp;
    QVariantMap metadata;

    Sample() = default;
    Sample(const QString& name,
           double val,
           const QString& unitStr,
           const QDateTime& time,
           const QVariantMap& meta = {})
        : channelName(name)
        , value(val)
        , unit(unitStr)
        , timestamp(time)
        , metadata(meta)
    {}
    
    QPointF toPoint() const {
        return QPointF(static_cast<double>(timestamp.toMSecsSinceEpoch()), value);
    }
    
    /// 获取时间戳（毫秒）
    qint64 timestampMs() const {
        return timestamp.toMSecsSinceEpoch();
    }
};

// ============================================================================
// 阈值配置
// ============================================================================

struct Threshold {
    QString name;
    double value = 0.0;
    QString unit;
    ThresholdMode mode = ThresholdMode::Above;
    bool enabled = true;
};

// ============================================================================
// 通道配置
// ============================================================================

struct ChannelConfig {
    QString name;
    QString displayName;
    QString unit;
    int maxSamples = 1000;
    QList<Threshold> thresholds;
    QVariantMap metadata;
    MonitorObjectKind objectKind = MonitorObjectKind::Generic;
    QString sourceName;
    bool editable = false;
    
    // 可视化属性
    QColor color;
    LineStyle lineStyle = LineStyle::Solid;
    int lineWidth = 2;
    bool defaultVisible = true;
};

// ============================================================================
// 数据提供器配置
// ============================================================================

struct ProviderConfig {
    QString id;
    QString channelName;
    QString unit;
    int periodMs = 1000;
    int priority = 128;
    QVariantMap metadata;
    std::function<double()> sampler;
    std::function<void(const QString&)> errorHandler;
    std::optional<ChannelConfig> channelOverride;
};

// ============================================================================
// 常量定义（性能优化版本）
// ============================================================================

namespace Limits {
    // 基础限制
    constexpr int DEFAULT_MAX_SAMPLES = 1000;
    constexpr int DEFAULT_DISPLAY_SAMPLES = 200;
    constexpr qint64 DEFAULT_TIME_WINDOW_MS = 30000;
    constexpr int MIN_UPDATE_INTERVAL_MS = 50;
    constexpr int DEFAULT_UPDATE_INTERVAL_MS = 100;
    constexpr int MAX_VISIBLE_CHANNELS = 10;
    
    // ===== 性能优化相关常量 =====
    
    /// 环形缓冲区默认容量（每通道）
    constexpr int DEFAULT_RING_BUFFER_CAPACITY = 5000;
    
    /// 默认渲染帧率（FPS）
    constexpr int DEFAULT_RENDER_FPS = 25;
    
    /// 默认渲染间隔（毫秒） = 1000 / FPS
    constexpr int DEFAULT_RENDER_INTERVAL_MS = 40;
    
    /// 最小渲染间隔（毫秒）~30 FPS
    constexpr int MIN_RENDER_INTERVAL_MS = 33;
    
    /// 最大渲染间隔（毫秒）~10 FPS
    constexpr int MAX_RENDER_INTERVAL_MS = 100;
    
    /// 图表 Series 最大点数（超过则降采样）
    constexpr int MAX_SERIES_POINTS = 2000;
    
    /// 批量处理阈值（累积多少增量后批量处理）
    constexpr int BATCH_PROCESS_THRESHOLD = 50;
    
    /// 增量数据最大缓存量
    constexpr int MAX_DELTA_BUFFER_SIZE = 1000;
    
    /// 自适应降采样触发阈值
    constexpr int ADAPTIVE_DOWNSAMPLE_THRESHOLD = 3000;
}

// ============================================================================
// 性能统计结构
// ============================================================================

struct PerformanceStats {
    qint64 lastUpdateTimeMs = 0;        ///< 上次更新时间
    int samplesPerSecond = 0;           ///< 每秒样本数
    int rendersPerSecond = 0;           ///< 每秒渲染次数
    int droppedSamples = 0;             ///< 丢弃的样本数
    double cpuUsagePercent = 0.0;       ///< CPU 使用率估算
    int activeChannelCount = 0;         ///< 活动通道数
    int totalPointsInChart = 0;         ///< 图表中总点数
};

} // namespace Monitor

#endif // MONITOR_TYPES_H
