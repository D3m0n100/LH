// 文件：src/monitor/MonitorChartView.cpp
// 监控图表视图实现（性能优化版本）

#include "MonitorChartView.h"
#include "ChartWidget.h"
#include "MonitorDataProcessor.h"
#include "MonitorTypes.h"

#include <QVBoxLayout>
#include <QDebug>
#include <QPixmap>
#include <QSvgGenerator>
#include <QPainter>

// 默认颜色列表
const QList<QColor> MonitorChartView::DEFAULT_COLORS = {
    QColor("#007ACC"),  // VS Code blue
    QColor("#00A3A3"),  // teal
    QColor("#16825D"),  // green
    QColor("#D16D00"),  // orange
    QColor("#8661C5"),  // purple
    QColor("#C586C0"),  // pink
    QColor("#C42B1C"),  // red
    QColor("#7A8F00"),  // olive
    QColor("#3B6EA8"),  // steel blue
    QColor("#6F6F6F")   // neutral
};

MonitorChartView::MonitorChartView(QWidget* parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_chartWidget(nullptr)
    , m_dataProcessor(nullptr)
    , m_timeWindowMs(Monitor::Limits::DEFAULT_TIME_WINDOW_MS)
    , m_renderTimer(new QTimer(this))
    , m_pendingRender(false)
    , m_frameCount(0)
    , m_nextColorIndex(0)
{
    // 初始化渲染配置
    m_renderConfig.targetFps = Monitor::Limits::DEFAULT_RENDER_FPS;
    m_renderConfig.renderIntervalMs = Monitor::Limits::DEFAULT_RENDER_INTERVAL_MS;
    m_renderConfig.enableThrottling = true;
    m_renderConfig.enableBatchUpdate = true;
    m_renderConfig.maxPointsPerSeries = Monitor::Limits::MAX_SERIES_POINTS;
    m_renderConfig.enableIncrementalRender = true;
    
    setupUI();
    
    // 配置渲染定时器
    m_renderTimer->setTimerType(Qt::PreciseTimer);
    m_renderTimer->setInterval(m_renderConfig.renderIntervalMs);
    connect(m_renderTimer, &QTimer::timeout, this, &MonitorChartView::onRenderTimer);
    
    // 连接 ChartWidget 信号
    connect(m_chartWidget, &ChartWidget::channelVisibilityChanged,
            this, &MonitorChartView::onChartChannelVisibilityChanged);
    
    m_lastRenderTime.start();
    m_fpsTimer.start();
    
    // 启动渲染定时器
    m_renderTimer->start();
}

MonitorChartView::~MonitorChartView() = default;

void MonitorChartView::setupUI()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    m_chartWidget = new ChartWidget(this);
    m_chartWidget->setTimeWindow(m_timeWindowMs);

    m_layout->addWidget(m_chartWidget);
}

// ============================================================================
// 渲染配置
// ============================================================================

void MonitorChartView::setRenderConfig(const RenderConfig& config)
{
    m_renderConfig = config;
    m_renderTimer->setInterval(config.renderIntervalMs);
}

void MonitorChartView::setTargetFps(int fps)
{
    if (fps <= 0) fps = 1;
    if (fps > 60) fps = 60;
    
    m_renderConfig.targetFps = fps;
    m_renderConfig.renderIntervalMs = 1000 / fps;
    m_renderTimer->setInterval(m_renderConfig.renderIntervalMs);
}

void MonitorChartView::setRenderInterval(int intervalMs)
{
    if (intervalMs < Monitor::Limits::MIN_RENDER_INTERVAL_MS) {
        intervalMs = Monitor::Limits::MIN_RENDER_INTERVAL_MS;
    }
    if (intervalMs > Monitor::Limits::MAX_RENDER_INTERVAL_MS) {
        intervalMs = Monitor::Limits::MAX_RENDER_INTERVAL_MS;
    }
    
    m_renderConfig.renderIntervalMs = intervalMs;
    m_renderConfig.targetFps = 1000 / intervalMs;
    m_renderTimer->setInterval(intervalMs);
}

void MonitorChartView::setIncrementalRenderEnabled(bool enabled)
{
    m_renderConfig.enableIncrementalRender = enabled;
}

// ============================================================================
// 数据处理器连接
// ============================================================================

void MonitorChartView::setDataProcessor(MonitorDataProcessor* processor)
{
    // 断开旧连接
    if (m_dataProcessor) {
        disconnect(m_dataProcessor, nullptr, this, nullptr);
    }
    
    m_dataProcessor = processor;
    
    // 建立新连接
    if (m_dataProcessor) {
        connect(m_dataProcessor, &MonitorDataProcessor::deltaDataReady,
                this, &MonitorChartView::onDeltaDataReady);
    }
}

// ============================================================================
// 通道管理
// ============================================================================

void MonitorChartView::addChannel(const QString& channelId, const ChannelStyle& style)
{
    if (m_channelStyles.contains(channelId)) {
        return;
    }

    ChannelStyle actualStyle = style;

    if (!actualStyle.color.isValid() || actualStyle.color == Qt::blue) {
        actualStyle.color = nextDefaultColor();
    }

    if (actualStyle.displayName.isEmpty()) {
        actualStyle.displayName = channelId;
    }

    m_channelStyles[channelId] = actualStyle;

    if (m_activeChannel.isEmpty()) {
        setActiveChannel(channelId);
    }

    emit channelAdded(channelId);
    qDebug() << "[MonitorChartView] 注册通道:" << channelId;
}

void MonitorChartView::removeChannel(const QString& channelId)
{
    if (!m_channelStyles.contains(channelId)) {
        return;
    }

    if (m_chartWidget->hasChannel(channelId)) {
        m_chartWidget->removeChannelSeries(channelId);
    }
    
    m_channelStyles.remove(channelId);
    m_visibleChannelSet.remove(channelId);
    m_pendingDeltas.remove(channelId);

    if (m_activeChannel == channelId) {
        if (!m_channelStyles.isEmpty()) {
            setActiveChannel(m_channelStyles.keys().first());
        } else {
            m_activeChannel.clear();
        }
    }

    emit channelRemoved(channelId);
    emit visibleChannelsChanged(visibleChannels());
}

bool MonitorChartView::hasChannel(const QString& channelId) const
{
    return m_channelStyles.contains(channelId);
}

QStringList MonitorChartView::channelNames() const
{
    return m_channelStyles.keys();
}

// ============================================================================
// 多通道显示控制
// ============================================================================

void MonitorChartView::addChannelSeries(const QString& channelId)
{
    ensureChannel(channelId);
    
    const ChannelStyle& style = m_channelStyles[channelId];
    
    if (!m_chartWidget->hasChannel(channelId)) {
        m_chartWidget->addChannelSeries(channelId, style.displayName, style.color);
        m_chartWidget->setChannelLineWidth(channelId, style.lineWidth);
        m_chartWidget->setChannelVisible(channelId, style.visible);
    }
    
    if (style.visible) {
        m_visibleChannelSet.insert(channelId);
    }
}

void MonitorChartView::removeChannelSeries(const QString& channelId)
{
    if (m_chartWidget->hasChannel(channelId)) {
        m_chartWidget->removeChannelSeries(channelId);
    }
    m_visibleChannelSet.remove(channelId);
    m_pendingDeltas.remove(channelId);
    
    emit visibleChannelsChanged(visibleChannels());
}

void MonitorChartView::toggleChannelVisible(const QString& channelId, bool visible)
{
    if (!m_channelStyles.contains(channelId)) {
        return;
    }

    m_channelStyles[channelId].visible = visible;
    
    if (visible) {
        if (!m_chartWidget->hasChannel(channelId)) {
            addChannelSeries(channelId);
        }
        m_chartWidget->setChannelVisible(channelId, true);
        m_visibleChannelSet.insert(channelId);
    } else {
        m_chartWidget->setChannelVisible(channelId, false);
        m_visibleChannelSet.remove(channelId);
    }

    emit channelVisibilityChanged(channelId, visible);
    emit visibleChannelsChanged(visibleChannels());
}

void MonitorChartView::setVisibleChannels(const QStringList& channelIds)
{
    // 先隐藏所有
    for (const QString& id : m_channelStyles.keys()) {
        if (m_chartWidget->hasChannel(id)) {
            m_chartWidget->setChannelVisible(id, false);
        }
        m_channelStyles[id].visible = false;
    }
    m_visibleChannelSet.clear();

    // 显示指定通道
    for (const QString& id : channelIds) {
        if (m_channelStyles.contains(id)) {
            m_channelStyles[id].visible = true;
            
            if (!m_chartWidget->hasChannel(id)) {
                addChannelSeries(id);
            }
            m_chartWidget->setChannelVisible(id, true);
            m_visibleChannelSet.insert(id);
        }
    }

    emit visibleChannelsChanged(visibleChannels());
}

QStringList MonitorChartView::visibleChannels() const
{
    return m_visibleChannelSet.values();
}

void MonitorChartView::showOnlyChannel(const QString& channelId)
{
    if (!m_channelStyles.contains(channelId)) {
        return;
    }
    setVisibleChannels(QStringList() << channelId);
    setActiveChannel(channelId);
}

void MonitorChartView::showAllChannels()
{
    setVisibleChannels(m_channelStyles.keys());
}

void MonitorChartView::hideAllChannels()
{
    setVisibleChannels(QStringList());
}

bool MonitorChartView::isChannelVisible(const QString& channelId) const
{
    return m_visibleChannelSet.contains(channelId);
}

// ============================================================================
// 活动通道
// ============================================================================

void MonitorChartView::setActiveChannel(const QString& channelId)
{
    if (m_activeChannel == channelId) {
        return;
    }

    m_activeChannel = channelId;
    
    if (!channelId.isEmpty() && m_channelStyles.contains(channelId)) {
        toggleChannelVisible(channelId, true);
    }

    emit activeChannelChanged(channelId);
}

// ============================================================================
// 数据更新（增量优化）
// ============================================================================

void MonitorChartView::appendDeltaPoints(const QString& channelId, 
                                          const QVector<QPointF>& points)
{
    if (points.isEmpty()) {
        return;
    }
    
    ensureChannel(channelId);
    
    // 累积增量数据
    m_pendingDeltas[channelId].append(points);
    m_pendingRender = true;
    
    // 如果禁用节流，立即渲染
    if (!m_renderConfig.enableThrottling) {
        doRender();
    }
}

void MonitorChartView::updateFromProcessedData(const ProcessedChannelData& data)
{
    const QString& channelId = data.channelName;
    
    ensureChannel(channelId);
    
    if (m_channelStyles[channelId].visible && !m_chartWidget->hasChannel(channelId)) {
        addChannelSeries(channelId);
    }
    
    // 增量模式
    if (m_renderConfig.enableIncrementalRender && data.isIncremental) {
        appendDeltaPoints(channelId, data.deltaPoints);
    } else {
        // 全量模式
        if (m_chartWidget->hasChannel(channelId)) {
            m_chartWidget->updateChannelData(channelId, data.seriesPoints);
        }
    }
}

void MonitorChartView::updateMultipleChannels(const QMap<QString, QVector<QPointF>>& channelData)
{
    for (auto it = channelData.begin(); it != channelData.end(); ++it) {
        appendDeltaPoints(it.key(), it.value());
    }
}

void MonitorChartView::clearChannelData(const QString& channelId)
{
    m_pendingDeltas.remove(channelId);
    if (m_chartWidget->hasChannel(channelId)) {
        m_chartWidget->clearChannelData(channelId);
    }
}

void MonitorChartView::clearAllData()
{
    m_pendingDeltas.clear();
    m_chartWidget->clearAllData();
}

void MonitorChartView::consumeAndRender()
{
    if (!m_dataProcessor) {
        return;
    }
    
    // 从数据处理器消费所有增量数据
    QMap<QString, QVector<QPointF>> deltas = m_dataProcessor->drainAllDeltas();
    
    for (auto it = deltas.begin(); it != deltas.end(); ++it) {
        appendDeltaPoints(it.key(), it.value());
    }
}

// ============================================================================
// 时间窗口
// ============================================================================

void MonitorChartView::setTimeWindow(qint64 windowMs)
{
    m_timeWindowMs = windowMs;
    if (m_chartWidget) {
        m_chartWidget->setTimeWindow(windowMs);
    }
}

// ============================================================================
// 样式配置
// ============================================================================

void MonitorChartView::setChannelStyle(const QString& channelId, const ChannelStyle& style)
{
    ensureChannel(channelId);
    m_channelStyles[channelId] = style;

    if (m_chartWidget->hasChannel(channelId)) {
        m_chartWidget->setChannelColor(channelId, style.color);
        m_chartWidget->setChannelLineWidth(channelId, style.lineWidth);
        m_chartWidget->setChannelDisplayName(channelId, style.displayName);
        m_chartWidget->setChannelVisible(channelId, style.visible);
    }
}

ChannelStyle MonitorChartView::channelStyle(const QString& channelId) const
{
    return m_channelStyles.value(channelId);
}

void MonitorChartView::setChannelVisible(const QString& channelId, bool visible)
{
    toggleChannelVisible(channelId, visible);
}

void MonitorChartView::setChannelColor(const QString& channelId, const QColor& color)
{
    if (m_channelStyles.contains(channelId)) {
        m_channelStyles[channelId].color = color;
        if (m_chartWidget->hasChannel(channelId)) {
            m_chartWidget->setChannelColor(channelId, color);
        }
    }
}

void MonitorChartView::setChannelLineWidth(const QString& channelId, int width)
{
    if (m_channelStyles.contains(channelId)) {
        m_channelStyles[channelId].lineWidth = width;
        if (m_chartWidget->hasChannel(channelId)) {
            m_chartWidget->setChannelLineWidth(channelId, width);
        }
    }
}

void MonitorChartView::setChannelDisplayName(const QString& channelId, 
                                              const QString& displayName)
{
    if (m_channelStyles.contains(channelId)) {
        m_channelStyles[channelId].displayName = displayName;
        if (m_chartWidget->hasChannel(channelId)) {
            m_chartWidget->setChannelDisplayName(channelId, displayName);
        }
    }
}

// ============================================================================
// 坐标轴
// ============================================================================

void MonitorChartView::setAutoScale(bool enabled)
{
    if (m_chartWidget) {
        m_chartWidget->setAutoScale(enabled);
    }
}

bool MonitorChartView::isAutoScale() const
{
    return m_chartWidget ? m_chartWidget->isAutoScale() : true;
}

void MonitorChartView::setYAxisRange(double min, double max)
{
    if (m_chartWidget) {
        m_chartWidget->setYAxisRange(min, max);
    }
}

void MonitorChartView::resetZoom()
{
    if (m_chartWidget) {
        m_chartWidget->resetZoom();
    }
}

// ============================================================================
// 图例控制
// ============================================================================

void MonitorChartView::setLegendVisible(bool visible)
{
    if (m_chartWidget) {
        m_chartWidget->setLegendVisible(visible);
    }
}

bool MonitorChartView::isLegendVisible() const
{
    return m_chartWidget ? m_chartWidget->isLegendVisible() : true;
}

// ============================================================================
// 导出功能
// ============================================================================

bool MonitorChartView::exportAsPng(const QString& filePath, int width, int height)
{
    if (!m_chartWidget) {
        return false;
    }
    
    QPixmap pixmap;
    if (width > 0 && height > 0) {
        pixmap = m_chartWidget->grab().scaled(width, height, 
                                               Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation);
    } else {
        pixmap = m_chartWidget->grab();
    }
    
    return pixmap.save(filePath, "PNG");
}

bool MonitorChartView::exportAsJpg(const QString& filePath, int quality)
{
    if (!m_chartWidget) {
        return false;
    }
    return m_chartWidget->grab().save(filePath, "JPG", quality);
}

bool MonitorChartView::exportAsSvg(const QString& filePath)
{
    if (!m_chartWidget) {
        return false;
    }
    
    QSvgGenerator generator;
    generator.setFileName(filePath);
    generator.setSize(m_chartWidget->size());
    generator.setViewBox(m_chartWidget->rect());
    
    QPainter painter(&generator);
    m_chartWidget->render(&painter);
    
    return true;
}

QPixmap MonitorChartView::grabChartPixmap() const
{
    return m_chartWidget ? m_chartWidget->grab() : QPixmap();
}

// ============================================================================
// 性能监控
// ============================================================================

double MonitorChartView::actualFps() const
{
    qint64 elapsedMs = m_fpsTimer.elapsed();
    if (elapsedMs <= 0) {
        return 0.0;
    }
    return static_cast<double>(m_frameCount) * 1000.0 / elapsedMs;
}

void MonitorChartView::resetRenderStats()
{
    m_renderStats = RenderStats();
    m_frameCount = 0;
    m_fpsTimer.restart();
}

void MonitorChartView::forceRefresh()
{
    doRender();
}

// ============================================================================
// 私有槽函数
// ============================================================================

void MonitorChartView::onRenderTimer()
{
    // 如果连接了数据处理器，先消费增量数据
    if (m_dataProcessor) {
        consumeAndRender();
    }
    
    // 如果有待渲染的数据，执行渲染
    if (m_pendingRender) {
        doRender();
    }
}

void MonitorChartView::onDeltaDataReady(const QString& channelId, int count)
{
    Q_UNUSED(count);
    
    // 数据处理器有新数据就绪
    // 实际渲染会在定时器超时时进行
    m_pendingRender = true;
}

void MonitorChartView::onChartChannelVisibilityChanged(const QString& channelId, bool visible)
{
    if (m_channelStyles.contains(channelId)) {
        m_channelStyles[channelId].visible = visible;
    }
    
    if (visible) {
        m_visibleChannelSet.insert(channelId);
    } else {
        m_visibleChannelSet.remove(channelId);
    }
    
    emit channelVisibilityChanged(channelId, visible);
    emit visibleChannelsChanged(visibleChannels());
}

// ============================================================================
// 私有方法
// ============================================================================

void MonitorChartView::ensureChannel(const QString& channelId)
{
    if (!m_channelStyles.contains(channelId)) {
        ChannelStyle style;
        style.color = nextDefaultColor();
        style.displayName = channelId;
        style.lineWidth = 2;
        style.visible = true;
        m_channelStyles[channelId] = style;
        
        emit channelAdded(channelId);
    }
}

QColor MonitorChartView::nextDefaultColor()
{
    QColor color = DEFAULT_COLORS[m_nextColorIndex % DEFAULT_COLORS.size()];
    m_nextColorIndex++;
    return color;
}

void MonitorChartView::doRender()
{
    QElapsedTimer renderTimer;
    renderTimer.start();
    
    int totalPointsRendered = 0;
    
    // 处理所有待渲染的增量数据
    for (auto it = m_pendingDeltas.begin(); it != m_pendingDeltas.end(); ++it) {
        const QString& channelId = it.key();
        QVector<QPointF>& points = it.value();
        
        if (points.isEmpty()) {
            continue;
        }
        
        // 确保通道已添加到图表
        if (!m_chartWidget->hasChannel(channelId)) {
            if (m_channelStyles.contains(channelId) && m_channelStyles[channelId].visible) {
                addChannelSeries(channelId);
            } else {
                continue;
            }
        }
        
        // 增量追加数据点
        for (const QPointF& pt : points) {
            m_chartWidget->addSampleToChannel(channelId, 
                Monitor::Sample(channelId, pt.y(), "", 
                    QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(pt.x()))));
        }
        
        totalPointsRendered += points.size();
    }
    
    // 清空待渲染队列
    m_pendingDeltas.clear();
    m_pendingRender = false;
    
    // 裁剪所有可见通道的旧数据
    for (const QString& channelId : m_visibleChannelSet) {
        trimOldPoints(channelId);
    }
    
    // 更新统计
    double renderTimeMs = renderTimer.elapsed();
    m_renderStats.totalRenderCount++;
    m_renderStats.pointsRenderedLastFrame = totalPointsRendered;
    m_renderStats.lastRenderTimeMs = QDateTime::currentMSecsSinceEpoch();
    m_renderStats.averageRenderTimeMs = 
        (m_renderStats.averageRenderTimeMs * (m_renderStats.totalRenderCount - 1) + renderTimeMs) 
        / m_renderStats.totalRenderCount;
    
    m_frameCount++;
    m_lastRenderTime.restart();
    
    emit renderCompleted(totalPointsRendered, renderTimeMs);
}

void MonitorChartView::trimOldPoints(const QString& channelId)
{
    // 由 ChartWidget 内部处理时间窗口裁剪
    // 这里可以添加额外的裁剪逻辑
}
