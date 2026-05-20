/**
 * @file ChartWidget.h
 * @brief 实时监控图表控件（性能优化版本）
 *
 * 功能增强：
 * - 多通道曲线叠加显示
 * - 增量数据追加（高性能）
 * - 滑动窗口自动裁剪
 * - 限频渲染支持
 */

#ifndef CHART_WIDGET_H
#define CHART_WIDGET_H

#include <QWidget>
#include <QTimer>
#include <QHash>
#include <QElapsedTimer>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLegend>

QT_CHARTS_USE_NAMESPACE

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QCheckBox;
class QLabel;
QT_END_NAMESPACE

namespace Monitor {
struct Sample;
}

/**
 * @brief 单个通道的曲线信息
 */
struct ChannelSeriesInfo {
    QLineSeries* series = nullptr;
    QString displayName;
    QColor color;
    int lineWidth = 2;
    bool visible = true;
    int pointCount = 0;                     ///< 当前点数
    qint64 oldestTimestampMs = 0;           ///< 最老数据点时间戳
    qint64 newestTimestampMs = 0;           ///< 最新数据点时间戳
};

/**
 * @brief ChartWidget
 * 
 * 实时监控图表控件（性能优化版本），支持：
 * - 多通道曲线叠加显示
 * - 增量数据追加（避免全量替换）
 * - 滑动窗口自动裁剪旧数据
 * - 限频 UI 更新
 * - 图例显示与交互
 */
class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartWidget(QWidget* parent = nullptr);
    ~ChartWidget() override;

    // ===== 多通道管理 =====
    
    bool addChannelSeries(const QString& channelId, 
                          const QString& displayName = QString(),
                          const QColor& color = QColor());
    bool removeChannelSeries(const QString& channelId);
    bool hasChannel(const QString& channelId) const;
    QStringList channelIds() const;
    
    void setChannelVisible(const QString& channelId, bool visible);
    bool isChannelVisible(const QString& channelId) const;
    void setChannelColor(const QString& channelId, const QColor& color);
    void setChannelLineWidth(const QString& channelId, int width);
    void setChannelDisplayName(const QString& channelId, const QString& displayName);

    // ===== 增量数据更新（核心优化）=====
    
    /**
     * @brief 追加单个数据点到指定通道（增量）
     * 这是高性能接口，只追加不替换
     */
    void appendPoint(const QString& channelId, const QPointF& point);
    
    /**
     * @brief 批量追加数据点（增量）
     */
    void appendPoints(const QString& channelId, const QVector<QPointF>& points);
    
    /**
     * @brief 追加采样数据（增量）
     */
    void addSampleToChannel(const QString& channelId, const Monitor::Sample& sample);
    
    /**
     * @brief 批量追加采样数据（增量）
     */
    void addSamplesToChannel(const QString& channelId, 
                              const QList<Monitor::Sample>& samples);

    // ===== 全量数据更新（兼容旧接口）=====
    
    void updateChannelData(const QString& channelId, 
                           const QList<Monitor::Sample>& samples);
    void updateChannelData(const QString& channelId, const QVector<QPointF>& points);
    void clearChannelData(const QString& channelId);
    void clearAllData();
    
    // 旧接口兼容
    void updateData(const QList<Monitor::Sample>& samples);
    void clearData();
    void setChannelName(const QString& name);
    void addSample(const Monitor::Sample& sample);

    // ===== 滑动窗口控制 =====
    
    /**
     * @brief 裁剪指定通道超出时间窗口的旧数据
     * @return 裁剪的点数
     */
    int trimOldPoints(const QString& channelId);
    
    /**
     * @brief 裁剪所有通道的旧数据
     * @return 总裁剪点数
     */
    int trimAllOldPoints();
    
    /**
     * @brief 设置每条曲线的最大点数
     */
    void setMaxPointsPerSeries(int maxPoints);
    int maxPointsPerSeries() const { return m_maxPointsPerSeries; }

    // ===== 坐标轴控制 =====
    
    void setYAxisRange(double min, double max);
    void setAutoScale(bool autoScale);
    bool isAutoScale() const { return m_autoScale; }
    void setTimeWindow(qint64 ms);
    qint64 timeWindow() const { return m_timeWindowMs; }
    void getYAxisRange(double& min, double& max) const;

    // ===== 图例控制 =====
    
    void setLegendVisible(bool visible);
    bool isLegendVisible() const;
    void setLegendAlignment(Qt::Alignment alignment);

    // ===== 导出功能 =====
    
    bool exportAsPng(const QString& filePath);

    // ===== 性能统计 =====
    
    int totalPointCount() const;
    int channelPointCount(const QString& channelId) const;

public slots:
    void resetZoom();
    void setAutoScaleEnabled(bool enabled);
    void onExportImage();
    
    /**
     * @brief 请求更新坐标轴（可节流调用）
     */
    void requestAxisUpdate();

signals:
    void chartClicked(const QPointF& point);
    void rangeChanged(qint64 minTime, qint64 maxTime);
    void autoScaleChanged(bool enabled);
    void channelVisibilityChanged(const QString& channelId, bool visible);

private slots:
    void onUpdateTimer();
    void handleSeriesClicked(const QPointF& point);
    void onAutoScaleCheckChanged(int state);
    void onLegendMarkerClicked();

private:
    void setupChart();
    void setupControlButtons();
    void updateAxisRanges();
    void calculateVisibleYRange(double& minY, double& maxY) const;
    QColor allocateDefaultColor();
    void updateInfoLabel();
    
    /**
     * @brief 限制曲线点数（LTTB 降采样）
     */
    void enforcePointLimit(const QString& channelId);

private:
    // 布局
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_toolbarLayout;
    QWidget*     m_toolbarWidget;

    // 工具条
    QPushButton* m_resetZoomButton;
    QCheckBox*   m_autoScaleCheck;
    QPushButton* m_exportImageButton;
    QLabel*      m_infoLabel;

    // 图表组件
    QChartView*    m_chartView;
    QChart*        m_chart;
    QDateTimeAxis* m_axisX;
    QValueAxis*    m_axisY;

    // 多通道数据
    QHash<QString, ChannelSeriesInfo> m_channels;
    QString m_defaultChannelId;

    // 定时器
    QTimer* m_updateTimer;
    QElapsedTimer m_lastAxisUpdateTime;
    bool m_axisUpdatePending;

    // 配置
    bool m_autoScale;
    double m_fixedMinY;
    double m_fixedMaxY;
    qint64 m_timeWindowMs;
    int m_maxPointsPerSeries;

    // 颜色分配
    int m_nextColorIndex;
    static const QList<QColor> DEFAULT_COLORS;

    // 常量
    static const int UPDATE_INTERVAL = 100;
    static const int DEFAULT_MAX_POINTS = 2000;
    static const int AXIS_UPDATE_THROTTLE_MS = 50;
};

#endif // CHART_WIDGET_H
