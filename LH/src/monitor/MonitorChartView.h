// 文件：src/monitor/MonitorChartView.h
// 监控图表视图（性能优化版本）
// 支持增量更新、限频渲染

#ifndef MONITOR_CHART_VIEW_H
#define MONITOR_CHART_VIEW_H

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QMap>
#include <QColor>
#include <QTimer>
#include <QElapsedTimer>
#include <QSet>

#include "MonitorDataProcessor.h"
#include "MonitorTypes.h"

QT_BEGIN_NAMESPACE
class QVBoxLayout;
QT_END_NAMESPACE

class ChartWidget;

namespace Monitor {
struct Sample;
}

/**
 * @brief 曲线样式配置
 */
struct ChannelStyle
{
    QColor color = Qt::blue;
    int lineWidth = 2;
    bool visible = true;
    QString displayName;
    Monitor::LineStyle lineStyle = Monitor::LineStyle::Solid;
};

/**
 * @brief 渲染性能配置
 */
struct RenderConfig
{
    int targetFps = 25;                     ///< 目标帧率
    int renderIntervalMs = 40;              ///< 渲染间隔（毫秒）
    bool enableThrottling = true;           ///< 启用节流
    bool enableBatchUpdate = true;          ///< 启用批量更新
    int maxPointsPerSeries = 2000;          ///< 每条曲线最大点数
    bool enableIncrementalRender = true;    ///< 启用增量渲染
};

/**
 * @brief MonitorChartView
 * 
 * 监控图表视图（性能优化版本），职责：
 * - 封装 ChartWidget
 * - 管理多通道曲线样式
 * - 支持增量数据更新
 * - 限频渲染（控制在目标 FPS）
 * - 提供图表导出功能
 * 
 * 【性能优化要点】
 * - 增量更新：只将新增数据点追加到 Series
 * - 限频渲染：使用定时器控制 UI 刷新频率
 * - 批量处理：累积多次数据更新后统一刷新
 * - 滑动窗口：自动移除超出时间窗口的旧数据
 * 
 * 【渲染策略】
 * - 默认 25 FPS（40ms 间隔）
 * - 数据接收频率可以很高（>1000 点/秒）
 * - 实际渲染频率由定时器控制
 */
class MonitorChartView : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorChartView(QWidget* parent = nullptr);
    ~MonitorChartView() override;

    // =========================================================================
    // 渲染配置
    // =========================================================================
    
    RenderConfig renderConfig() const { return m_renderConfig; }
    void setRenderConfig(const RenderConfig& config);
    
    /// 设置目标帧率
    void setTargetFps(int fps);
    int targetFps() const { return m_renderConfig.targetFps; }
    
    /// 设置渲染间隔（毫秒）
    void setRenderInterval(int intervalMs);
    int renderInterval() const { return m_renderConfig.renderIntervalMs; }
    
    /// 启用/禁用增量渲染
    void setIncrementalRenderEnabled(bool enabled);
    bool isIncrementalRenderEnabled() const { return m_renderConfig.enableIncrementalRender; }

    // =========================================================================
    // 数据处理器连接
    // =========================================================================
    
    /// 设置数据处理器（用于增量数据消费）
    void setDataProcessor(MonitorDataProcessor* processor);
    MonitorDataProcessor* dataProcessor() const { return m_dataProcessor; }

    // =========================================================================
    // 通道管理
    // =========================================================================
    
    void addChannel(const QString& channelId, const ChannelStyle& style = ChannelStyle());
    void removeChannel(const QString& channelId);
    bool hasChannel(const QString& channelId) const;
    QStringList channelNames() const;

    // =========================================================================
    // 多通道显示控制
    // =========================================================================
    
    void addChannelSeries(const QString& channelId);
    void removeChannelSeries(const QString& channelId);
    void toggleChannelVisible(const QString& channelId, bool visible);
    void setVisibleChannels(const QStringList& channelIds);
    QStringList visibleChannels() const;
    void showOnlyChannel(const QString& channelId);
    void showAllChannels();
    void hideAllChannels();
    bool isChannelVisible(const QString& channelId) const;

    // =========================================================================
    // 活动通道
    // =========================================================================
    
    void setActiveChannel(const QString& channelId);
    QString activeChannel() const { return m_activeChannel; }

    // =========================================================================
    // 数据更新（增量优化）
    // =========================================================================
    
    /**
     * @brief 追加增量数据点到指定通道
     * 这是主要的数据更新接口，支持高频调用
     * 实际渲染会被限频
     */
    void appendDeltaPoints(const QString& channelId, const QVector<QPointF>& points);
    
    /**
     * @brief 使用处理后的数据更新（兼容旧接口）
     */
    void updateFromProcessedData(const ProcessedChannelData& data);
    
    /**
     * @brief 批量更新多个通道
     */
    void updateMultipleChannels(const QMap<QString, QVector<QPointF>>& channelData);
    
    /// 清除指定通道数据
    void clearChannelData(const QString& channelId);
    
    /// 清除所有通道数据
    void clearAllData();
    
    /**
     * @brief 从数据处理器消费增量数据并更新图表
     * 通常由内部定时器调用，也可手动调用
     */
    void consumeAndRender();

    // =========================================================================
    // 时间窗口
    // =========================================================================
    
    void setTimeWindow(qint64 windowMs);
    qint64 timeWindow() const { return m_timeWindowMs; }

    // =========================================================================
    // 样式配置
    // =========================================================================
    
    void setChannelStyle(const QString& channelId, const ChannelStyle& style);
    ChannelStyle channelStyle(const QString& channelId) const;
    void setChannelVisible(const QString& channelId, bool visible);
    void setChannelColor(const QString& channelId, const QColor& color);
    void setChannelLineWidth(const QString& channelId, int width);
    void setChannelDisplayName(const QString& channelId, const QString& displayName);

    // =========================================================================
    // 坐标轴
    // =========================================================================
    
    void setAutoScale(bool enabled);
    bool isAutoScale() const;
    void setYAxisRange(double min, double max);
    void resetZoom();

    // =========================================================================
    // 图例控制
    // =========================================================================
    
    void setLegendVisible(bool visible);
    bool isLegendVisible() const;

    // =========================================================================
    // 导出功能
    // =========================================================================
    
    bool exportAsPng(const QString& filePath, int width = 0, int height = 0);
    bool exportAsJpg(const QString& filePath, int quality = 90);
    bool exportAsSvg(const QString& filePath);
    QPixmap grabChartPixmap() const;

    // =========================================================================
    // 性能监控
    // =========================================================================
    
    /// 获取实际渲染 FPS
    double actualFps() const;
    
    /// 获取渲染统计
    struct RenderStats {
        int totalRenderCount = 0;
        int skippedRenderCount = 0;
        double averageRenderTimeMs = 0.0;
        int pointsRenderedLastFrame = 0;
        qint64 lastRenderTimeMs = 0;
    };
    RenderStats renderStats() const { return m_renderStats; }
    
    /// 重置渲染统计
    void resetRenderStats();

    // =========================================================================
    // 强制刷新
    // =========================================================================
    
    /// 强制立即刷新（忽略限频）
    void forceRefresh();

    // =========================================================================
    // 内部访问
    // =========================================================================
    
    ChartWidget* chartWidget() const { return m_chartWidget; }

signals:
    void channelAdded(const QString& channelId);
    void channelRemoved(const QString& channelId);
    void activeChannelChanged(const QString& channelId);
    void channelVisibilityChanged(const QString& channelId, bool visible);
    void visibleChannelsChanged(const QStringList& channelIds);
    void renderCompleted(int pointCount, double renderTimeMs);

private slots:
    /// 渲染定时器超时
    void onRenderTimer();
    
    /// 数据处理器发出增量数据就绪信号
    void onDeltaDataReady(const QString& channelId, int count);
    
    /// ChartWidget 通道可见性改变
    void onChartChannelVisibilityChanged(const QString& channelId, bool visible);

private:
    void setupUI();
    void ensureChannel(const QString& channelId);
    QColor nextDefaultColor();
    
    /// 执行实际渲染
    void doRender();
    
    /// 裁剪超出时间窗口的旧数据
    void trimOldPoints(const QString& channelId);

private:
    QVBoxLayout* m_layout;
    ChartWidget* m_chartWidget;
    MonitorDataProcessor* m_dataProcessor;

    // 通道样式配置
    QMap<QString, ChannelStyle> m_channelStyles;
    
    // 当前可见的通道集合
    QSet<QString> m_visibleChannelSet;
    
    // 活动通道
    QString m_activeChannel;

    // 时间窗口
    qint64 m_timeWindowMs;

    // ===== 渲染控制 =====
    RenderConfig m_renderConfig;
    QTimer* m_renderTimer;              ///< 渲染定时器
    QElapsedTimer m_lastRenderTime;     ///< 上次渲染时间
    bool m_pendingRender;               ///< 是否有待渲染的数据
    
    // 待渲染的增量数据（channelId -> delta points）
    QMap<QString, QVector<QPointF>> m_pendingDeltas;
    
    // 渲染统计
    RenderStats m_renderStats;
    QElapsedTimer m_fpsTimer;
    int m_frameCount;

    // 颜色分配
    int m_nextColorIndex;
    static const QList<QColor> DEFAULT_COLORS;
};

#endif // MONITOR_CHART_VIEW_H
