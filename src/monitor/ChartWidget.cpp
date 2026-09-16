/**
 * @file ChartWidget.cpp
 * @brief 实时监控图表控件实现（性能优化版本）
 */

#include "ChartWidget.h"
#include "MonitorTypes.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QFontMetrics>
#include <QDebug>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QBrush>
#include <QPen>
#include <QtCharts/QLegendMarker>
#include <algorithm>
#include <cmath>

const QList<QColor> ChartWidget::DEFAULT_COLORS = Monitor::defaultChannelColors();

ChartWidget::ChartWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_toolbarLayout(nullptr)
    , m_toolbarWidget(nullptr)
    , m_resetZoomButton(nullptr)
    , m_autoScaleCheck(nullptr)
    , m_exportImageButton(nullptr)
    , m_channelLegendButton(nullptr)
    , m_infoLabel(nullptr)
    , m_isCompactMode(false)
    , m_userLegendVisible(true)
    , m_chartView(nullptr)
    , m_chart(new QChart())
    , m_axisX(new QDateTimeAxis())
    , m_axisY(new QValueAxis())
    , m_defaultChannelId("__default__")
    , m_updateTimer(new QTimer(this))
    , m_axisUpdatePending(false)
    , m_autoScale(true)
    , m_fixedMinY(0.0)
    , m_fixedMaxY(100.0)
    , m_timeWindowMs(30000)
    , m_maxPointsPerSeries(DEFAULT_MAX_POINTS)
    , m_nextColorIndex(0)
{
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(2);
    setMinimumHeight(120);

    setupControlButtons();
    setupChart();

    connect(m_updateTimer, &QTimer::timeout, this, &ChartWidget::onUpdateTimer);
    m_updateTimer->setInterval(UPDATE_INTERVAL);
    m_updateTimer->start();
    
    m_lastAxisUpdateTime.start();
    updateInfoLabel();
}

ChartWidget::~ChartWidget()
{
    for (auto& info : m_channels) {
        if (info.series) {
            m_chart->removeSeries(info.series);
        }
    }
    m_channels.clear();
}

void ChartWidget::setupControlButtons()
{
    m_toolbarWidget = new QWidget(this);
    m_toolbarWidget->setObjectName("ChartToolbar");
    m_toolbarWidget->setMinimumHeight(34);
    m_toolbarWidget->setStyleSheet(
        "#ChartToolbar {"
        "  background-color: #f3f3f3;"
        "  border-bottom: 1px solid #d0d7de;"
        "  padding: 2px;"
        "}"
        "#ChartToolbar QLabel { color: #5f6a72; }"
    );

    m_toolbarLayout = new QHBoxLayout(m_toolbarWidget);
    m_toolbarLayout->setContentsMargins(8, 4, 8, 4);
    m_toolbarLayout->setSpacing(10);

    // 重置缩放按钮
    m_resetZoomButton = new QPushButton("重置缩放", m_toolbarWidget);
    m_resetZoomButton->setToolTip("重置图表缩放到默认状态");
    m_resetZoomButton->setFixedHeight(26);
    connect(m_resetZoomButton, &QPushButton::clicked, this, &ChartWidget::resetZoom);
    m_toolbarLayout->addWidget(m_resetZoomButton);

    m_toolbarLayout->addSpacing(10);

    // 自动缩放复选框
    m_autoScaleCheck = new QCheckBox("自动缩放 Y 轴", m_toolbarWidget);
    m_autoScaleCheck->setToolTip("勾选后 Y 轴将根据当前可见数据自动调整范围");
    m_autoScaleCheck->setChecked(m_autoScale);
    connect(m_autoScaleCheck, &QCheckBox::stateChanged,
            this, &ChartWidget::onAutoScaleCheckChanged);
    m_toolbarLayout->addWidget(m_autoScaleCheck);

    m_toolbarLayout->addSpacing(10);

    // 导出按钮
    m_exportImageButton = new QPushButton("导出图像", m_toolbarWidget);
    m_exportImageButton->setToolTip("导出当前图表为 PNG 图像");
    m_exportImageButton->setFixedHeight(26);
    connect(m_exportImageButton, &QPushButton::clicked, this, &ChartWidget::onExportImage);
    m_toolbarLayout->addWidget(m_exportImageButton);

    // 通道图例按钮（紧凑模式折叠展示）
    m_channelLegendButton = new QPushButton(tr("通道图例"), m_toolbarWidget);
    m_channelLegendButton->setObjectName("ChartChannelLegendButton");
    m_channelLegendButton->setToolTip(tr("查看通道图例与切换可见性"));
    m_channelLegendButton->setFixedHeight(26);
    m_channelLegendButton->setVisible(false);
    connect(m_channelLegendButton, &QPushButton::clicked, this, &ChartWidget::openChannelLegendMenu);
    m_toolbarLayout->addWidget(m_channelLegendButton);

    m_toolbarLayout->addStretch();

    // 信息标签
    m_infoLabel = new QLabel(m_toolbarWidget);
    m_toolbarLayout->addWidget(m_infoLabel);

    m_mainLayout->addWidget(m_toolbarWidget);
}

void ChartWidget::setupChart()
{
    m_axisX->setFormat("hh:mm:ss");
    m_axisX->setTitleText("时间");

    m_axisY->setTitleText("数值");
    updateTicksAndPrecision();

    const QPen gridPen(QColor("#e5e5e5"));
    const QPen axisPen(QColor("#8c959f"));
    const QBrush labelBrush(QColor("#3b3b3b"));
    m_axisX->setGridLinePen(gridPen);
    m_axisY->setGridLinePen(gridPen);
    m_axisX->setLinePen(axisPen);
    m_axisY->setLinePen(axisPen);
    m_axisX->setLabelsBrush(labelBrush);
    m_axisY->setLabelsBrush(labelBrush);
    m_axisX->setTitleBrush(labelBrush);
    m_axisY->setTitleBrush(labelBrush);

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    m_chart->setAnimationOptions(QChart::NoAnimation);
    m_chart->setMargins(QMargins(16, 6, 12, 6));
    m_chart->setBackgroundBrush(QBrush(QColor("#ffffff")));
    m_chart->setPlotAreaBackgroundBrush(QBrush(QColor("#ffffff")));
    m_chart->setPlotAreaBackgroundVisible(true);

    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);
    m_chart->legend()->setFont(QFont("Microsoft YaHei UI", 9));
    m_chart->legend()->setLabelColor(QColor("#3b3b3b"));

    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setRubberBand(QChartView::RectangleRubberBand);

    m_mainLayout->addWidget(m_chartView, 1);

    QDateTime now = QDateTime::currentDateTime();
    m_axisX->setRange(now.addMSecs(-m_timeWindowMs), now);
    m_axisY->setRange(m_fixedMinY, m_fixedMaxY);
    updateResponsiveLayout();
}

// ============================================================================
// 多通道管理
// ============================================================================

bool ChartWidget::addChannelSeries(const QString& channelId,
                                    const QString& displayName,
                                    const QColor& color)
{
    if (m_channels.contains(channelId)) {
        return false;
    }

    ChannelSeriesInfo info;
    info.series = new QLineSeries();
    info.displayName = displayName.isEmpty() ? channelId : displayName;
    info.color = color.isValid() ? color : allocateDefaultColor();
    info.lineWidth = 2;
    info.visible = true;
    info.pointCount = 0;

    QPen pen(info.color);
    pen.setWidth(info.lineWidth);
    info.series->setPen(pen);
    info.series->setName(info.displayName);
    
    // 使用 OpenGL 加速（如果可用）
    info.series->setUseOpenGL(true);

    m_chart->addSeries(info.series);
    info.series->attachAxis(m_axisX);
    info.series->attachAxis(m_axisY);

    connect(info.series, &QLineSeries::clicked, 
            this, &ChartWidget::handleSeriesClicked);

    m_channels.insert(channelId, info);

    // 连接图例标记点击
    const auto markers = m_chart->legend()->markers(info.series);
    for (QLegendMarker* marker : markers) {
        connect(marker, &QLegendMarker::clicked,
                this, &ChartWidget::onLegendMarkerClicked);
    }

    if (m_isCompactMode && m_userLegendVisible) {
        if (m_channelLegendButton) {
            m_channelLegendButton->setVisible(true);
        }
    }

    updateInfoLabel();
    return true;
}

bool ChartWidget::removeChannelSeries(const QString& channelId)
{
    if (!m_channels.contains(channelId)) {
        return false;
    }

    ChannelSeriesInfo& info = m_channels[channelId];
    if (info.series) {
        m_chart->removeSeries(info.series);
    }

    m_channels.remove(channelId);
    if (m_channels.isEmpty() && m_channelLegendButton) {
        m_channelLegendButton->setVisible(false);
    }
    updateInfoLabel();
    return true;
}

bool ChartWidget::hasChannel(const QString& channelId) const
{
    return m_channels.contains(channelId);
}

QStringList ChartWidget::channelIds() const
{
    return m_channels.keys();
}

void ChartWidget::setChannelVisible(const QString& channelId, bool visible)
{
    if (!m_channels.contains(channelId)) {
        return;
    }

    ChannelSeriesInfo& info = m_channels[channelId];
    if (info.visible == visible) {
        return;
    }

    info.visible = visible;
    if (info.series) {
        info.series->setVisible(visible);
        
        const auto markers = m_chart->legend()->markers(info.series);
        for (QLegendMarker* marker : markers) {
            marker->setVisible(true);
            marker->setLabelBrush(QBrush(visible ? Qt::black : Qt::gray));
        }
    }

    requestAxisUpdate();
    updateInfoLabel();
    emit channelVisibilityChanged(channelId, visible);
}

bool ChartWidget::isChannelVisible(const QString& channelId) const
{
    if (!m_channels.contains(channelId)) {
        return false;
    }
    return m_channels[channelId].visible;
}

void ChartWidget::setChannelColor(const QString& channelId, const QColor& color)
{
    if (!m_channels.contains(channelId)) {
        return;
    }

    ChannelSeriesInfo& info = m_channels[channelId];
    info.color = color;
    if (info.series) {
        QPen pen = info.series->pen();
        pen.setColor(color);
        info.series->setPen(pen);
    }
}

void ChartWidget::setChannelLineWidth(const QString& channelId, int width)
{
    if (!m_channels.contains(channelId)) {
        return;
    }

    ChannelSeriesInfo& info = m_channels[channelId];
    info.lineWidth = width;
    if (info.series) {
        QPen pen = info.series->pen();
        pen.setWidth(width);
        info.series->setPen(pen);
    }
}

void ChartWidget::setChannelDisplayName(const QString& channelId, 
                                         const QString& displayName)
{
    if (!m_channels.contains(channelId)) {
        return;
    }

    ChannelSeriesInfo& info = m_channels[channelId];
    info.displayName = displayName;
    if (info.series) {
        info.series->setName(displayName);
    }
}

// ============================================================================
// 增量数据更新（核心优化）
// ============================================================================

void ChartWidget::appendPoint(const QString& channelId, const QPointF& point)
{
    if (!m_channels.contains(channelId)) {
        addChannelSeries(channelId);
    }

    ChannelSeriesInfo& info = m_channels[channelId];
    if (!info.series) {
        return;
    }

    // 直接追加到 series
    info.series->append(point);
    info.pointCount++;
    
    const qint64 timestamp = static_cast<qint64>(point.x());
    if (info.oldestTimestampMs == 0 || timestamp < info.oldestTimestampMs) {
        info.oldestTimestampMs = timestamp;
    }
    if (timestamp > info.newestTimestampMs) {
        info.newestTimestampMs = timestamp;
    }
    
    if (info.pointCount > m_maxPointsPerSeries) {
        enforcePointLimit(channelId);
    }
    
    requestAxisUpdate();
}

void ChartWidget::appendPoints(const QString& channelId, const QVector<QPointF>& points)
{
    if (points.isEmpty()) {
        return;
    }

    if (!m_channels.contains(channelId)) {
        addChannelSeries(channelId);
    }

    ChannelSeriesInfo& info = m_channels[channelId];
    if (!info.series) {
        return;
    }

    // 批量追加
    QVector<QPointF> retained;
    const int incomingCount = qMin(points.size(), m_maxPointsPerSeries);
    const int oldCount = qMin(info.series->count(), m_maxPointsPerSeries - incomingCount);
    retained.reserve(oldCount + incomingCount);
    for (int i = info.series->count() - oldCount; i < info.series->count(); ++i)
        retained.append(info.series->at(i));
    for (int i = points.size() - incomingCount; i < points.size(); ++i)
        retained.append(points.at(i));
    info.series->replace(retained);
    info.pointCount = retained.size();
    info.oldestTimestampMs = retained.isEmpty() ? 0 : static_cast<qint64>(retained.first().x());
    info.newestTimestampMs = info.oldestTimestampMs;
    for (const auto& point : retained) {
        info.oldestTimestampMs = qMin(info.oldestTimestampMs, static_cast<qint64>(point.x()));
        info.newestTimestampMs = qMax(info.newestTimestampMs, static_cast<qint64>(point.x()));
    }

    requestAxisUpdate();
}

void ChartWidget::addSampleToChannel(const QString& channelId, 
                                      const Monitor::Sample& sample)
{
    appendPoint(channelId, sample.toPoint());
}

void ChartWidget::addSamplesToChannel(const QString& channelId,
                                       const QList<Monitor::Sample>& samples)
{
    QVector<QPointF> points;
    points.reserve(samples.size());
    for (const auto& sample : samples) {
        points.append(sample.toPoint());
    }
    appendPoints(channelId, points);
}

// ============================================================================
// 全量数据更新（兼容旧接口）
// ============================================================================

void ChartWidget::updateChannelData(const QString& channelId,
                                     const QList<Monitor::Sample>& samples)
{
    QVector<QPointF> points;
    points.reserve(samples.size());
    for (const auto& sample : samples) {
        points.append(sample.toPoint());
    }
    updateChannelData(channelId, points);
}

void ChartWidget::updateChannelData(const QString& channelId,
                                     const QVector<QPointF>& points)
{
    if (!m_channels.contains(channelId)) {
        addChannelSeries(channelId);
    }

    ChannelSeriesInfo& info = m_channels[channelId];
    if (!info.series) {
        return;
    }

    // 全量替换
    info.series->replace(points);
    info.pointCount = points.size();
    
    if (!points.isEmpty()) {
        info.oldestTimestampMs = static_cast<qint64>(points.first().x());
        info.newestTimestampMs = static_cast<qint64>(points.last().x());
    } else {
        info.oldestTimestampMs = 0;
        info.newestTimestampMs = 0;
    }
    
    requestAxisUpdate();
}

void ChartWidget::clearChannelData(const QString& channelId)
{
    if (!m_channels.contains(channelId)) {
        return;
    }

    ChannelSeriesInfo& info = m_channels[channelId];
    if (info.series) {
        info.series->clear();
    }
    info.pointCount = 0;
    info.oldestTimestampMs = 0;
    info.newestTimestampMs = 0;
    
    updateInfoLabel();
}

void ChartWidget::clearAllData()
{
    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        if (it.value().series) {
            it.value().series->clear();
        }
        it.value().pointCount = 0;
        it.value().oldestTimestampMs = 0;
        it.value().newestTimestampMs = 0;
    }
    updateInfoLabel();
}

void ChartWidget::updateData(const QList<Monitor::Sample>& samples)
{
    updateChannelData(m_defaultChannelId, samples);
}

void ChartWidget::clearData()
{
    clearAllData();
}

void ChartWidget::setChannelName(const QString& name)
{
    if (!m_channels.contains(m_defaultChannelId)) {
        addChannelSeries(m_defaultChannelId, name);
    } else {
        setChannelDisplayName(m_defaultChannelId, name);
    }
}

void ChartWidget::addSample(const Monitor::Sample& sample)
{
    addSampleToChannel(m_defaultChannelId, sample);
}

// ============================================================================
// 滑动窗口控制
// ============================================================================

int ChartWidget::trimOldPoints(const QString& channelId)
{
    if (!m_channels.contains(channelId)) {
        return 0;
    }

    ChannelSeriesInfo& info = m_channels[channelId];
    if (!info.series || info.pointCount == 0) {
        return 0;
    }

    qint64 cutoffTime = QDateTime::currentMSecsSinceEpoch() - m_timeWindowMs;
    
    const auto& allPoints = info.series->pointsVector();
    int removeCount = 0;
    
    for (int i = 0; i < allPoints.size(); ++i) {
        if (allPoints[i].x() >= cutoffTime) {
            removeCount = i;
            break;
        }
    }
    
    if (removeCount > 0) {
        // 移除旧点
        info.series->removePoints(0, removeCount);
        info.pointCount -= removeCount;
        
        // 更新最老时间戳
        if (info.pointCount > 0 && info.series->count() > 0) {
            info.oldestTimestampMs = static_cast<qint64>(info.series->at(0).x());
        } else {
            info.oldestTimestampMs = 0;
        }
    }
    
    return removeCount;
}

int ChartWidget::trimAllOldPoints()
{
    int totalRemoved = 0;
    for (const QString& channelId : m_channels.keys()) {
        totalRemoved += trimOldPoints(channelId);
    }
    return totalRemoved;
}

void ChartWidget::setMaxPointsPerSeries(int maxPoints)
{
    m_maxPointsPerSeries = maxPoints > 100 ? maxPoints : 100;
}

// ============================================================================
// 坐标轴控制
// ============================================================================

void ChartWidget::setYAxisRange(double min, double max)
{
    m_fixedMinY = min;
    m_fixedMaxY = max;
    if (!m_autoScale) {
        m_axisY->setRange(min, max);
        updateTicksAndPrecision();
    }
}

void ChartWidget::setAutoScale(bool autoScale)
{
    if (m_autoScale != autoScale) {
        m_autoScale = autoScale;
        m_autoScaleCheck->setChecked(autoScale);
        if (!autoScale) {
            m_axisY->setRange(m_fixedMinY, m_fixedMaxY);
        }
        emit autoScaleChanged(autoScale);
    }
}

void ChartWidget::setTimeWindow(qint64 ms)
{
    m_timeWindowMs = ms;
}

void ChartWidget::getYAxisRange(double& min, double& max) const
{
    min = m_axisY->min();
    max = m_axisY->max();
}

void ChartWidget::resetZoom()
{
    m_chart->zoomReset();
    updateAxisRanges();
}

void ChartWidget::setAutoScaleEnabled(bool enabled)
{
    setAutoScale(enabled);
}

// ============================================================================
// 图例控制
// ============================================================================

void ChartWidget::setLegendVisible(bool visible)
{
    m_userLegendVisible = visible;
    if (m_isCompactMode) {
        m_chart->legend()->setVisible(false);
        if (m_channelLegendButton) {
            m_channelLegendButton->setVisible(visible && !m_channels.isEmpty());
        }
    } else {
        m_chart->legend()->setVisible(visible);
        if (m_channelLegendButton) {
            m_channelLegendButton->setVisible(false);
        }
    }
}

bool ChartWidget::isLegendVisible() const
{
    return m_userLegendVisible;
}

void ChartWidget::setLegendAlignment(Qt::Alignment alignment)
{
    m_chart->legend()->setAlignment(alignment);
}

// ============================================================================
// 导出功能
// ============================================================================

bool ChartWidget::exportAsPng(const QString& filePath)
{
    if (!m_chartView) {
        return false;
    }
    return m_chartView->grab().save(filePath, "PNG");
}

void ChartWidget::onExportImage()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("导出图表图像"),
        QString("chart_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        tr("PNG 图像 (*.png)")
    );

    if (!filePath.isEmpty() && exportAsPng(filePath)) {
        QMessageBox::information(this, tr("导出成功"),
                                 tr("图表已导出到：%1").arg(filePath));
    }
}

// ============================================================================
// 性能统计

int ChartWidget::totalPointCount() const
{
    int total = 0;
    for (const auto& info : m_channels) {
        total += info.pointCount;
    }
    return total;
}

int ChartWidget::channelPointCount(const QString& channelId) const
{
    if (!m_channels.contains(channelId)) {
        return 0;
    }
    return m_channels[channelId].pointCount;
}

void ChartWidget::requestAxisUpdate()
{
    m_axisUpdatePending = true;
}

// ============================================================================
// 私有槽函数
// ============================================================================

void ChartWidget::onUpdateTimer()
{
    if (m_axisUpdatePending && 
        m_lastAxisUpdateTime.elapsed() >= AXIS_UPDATE_THROTTLE_MS) {
        updateAxisRanges();
        m_axisUpdatePending = false;
        m_lastAxisUpdateTime.restart();
    }
    
    trimAllOldPoints();
    
    updateInfoLabel();
}

void ChartWidget::handleSeriesClicked(const QPointF& point)
{
    emit chartClicked(point);
}

void ChartWidget::onAutoScaleCheckChanged(int state)
{
    setAutoScale(state == Qt::Checked);
}

void ChartWidget::onLegendMarkerClicked()
{
    QLegendMarker* marker = qobject_cast<QLegendMarker*>(sender());
    if (!marker) {
        return;
    }

    QLineSeries* series = qobject_cast<QLineSeries*>(marker->series());
    if (!series) {
        return;
    }

    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        if (it.value().series == series) {
            setChannelVisible(it.key(), !it.value().visible);
            break;
        }
    }
}

// ============================================================================
// 私有方法

void ChartWidget::updateAxisRanges()
{
    QDateTime now = QDateTime::currentDateTime();
    
    if (m_timeWindowMs > 0) {
        m_axisX->setRange(now.addMSecs(-m_timeWindowMs), now);
    }

    if (m_autoScale) {
        double minY, maxY;
        calculateVisibleYRange(minY, maxY);
        
        if (minY < maxY) {
            double padding = (maxY - minY) * 0.1;
            if (padding < 0.1) padding = 0.1;
            m_axisY->setRange(minY - padding, maxY + padding);
        }
    }

    updateTicksAndPrecision();

    emit rangeChanged(now.addMSecs(-m_timeWindowMs).toMSecsSinceEpoch(),
                      now.toMSecsSinceEpoch());
}

void ChartWidget::calculateVisibleYRange(double& minY, double& maxY) const
{
    minY = std::numeric_limits<double>::max();
    maxY = std::numeric_limits<double>::lowest();
    bool hasData = false;

    qint64 cutoffTime = QDateTime::currentMSecsSinceEpoch() - m_timeWindowMs;

    for (const auto& info : m_channels) {
        if (!info.visible || !info.series || info.pointCount == 0) {
            continue;
        }

        const auto& points = info.series->pointsVector();
        for (const QPointF& pt : points) {
            if (m_timeWindowMs > 0 && pt.x() < cutoffTime) {
                continue;
            }
            minY = std::min(minY, pt.y());
            maxY = std::max(maxY, pt.y());
            hasData = true;
        }
    }

    if (!hasData) {
        minY = m_fixedMinY;
        maxY = m_fixedMaxY;
    }
}

QColor ChartWidget::allocateDefaultColor()
{
    QColor color = DEFAULT_COLORS[m_nextColorIndex % DEFAULT_COLORS.size()];
    m_nextColorIndex++;
    return color;
}

void ChartWidget::updateInfoLabel()
{
    int totalChannels = m_channels.size();
    int visibleChannels = 0;
    int totalPoints = 0;

    for (const auto& info : m_channels) {
        if (info.visible) {
            visibleChannels++;
        }
        totalPoints += info.pointCount;
    }

    if (m_isCompactMode) {
        m_infoLabel->setText(QString("通道: %1/%2 | 点数: %3 (数值/时间)")
                             .arg(visibleChannels)
                             .arg(totalChannels)
                             .arg(totalPoints));
    } else {
        m_infoLabel->setText(QString("通道: %1/%2 | 数据点: %3")
                             .arg(visibleChannels)
                             .arg(totalChannels)
                             .arg(totalPoints));
    }
}

void ChartWidget::enforcePointLimit(const QString& channelId)
{
    if (!m_channels.contains(channelId)) {
        return;
    }

    ChannelSeriesInfo& info = m_channels[channelId];
    if (!info.series || info.pointCount <= m_maxPointsPerSeries) {
        return;
    }

    int removeCount = info.pointCount - m_maxPointsPerSeries;
    
    info.series->removePoints(0, removeCount);
    info.pointCount = m_maxPointsPerSeries;
    
    // 更新最老时间戳
    if (info.series->count() > 0) {
        info.oldestTimestampMs = static_cast<qint64>(info.series->at(0).x());
    }
}


// ============================================================================
// 紧凑与自适应排版 (R8-01)
// ============================================================================

void ChartWidget::updateResponsiveLayout()
{
    const int h = m_chartView ? m_chartView->height() : (height() - toolbarHeight());
    if (h > 0) {
        if (h < 180) {
            if (!m_isCompactMode) {
                setCompactMode(true);
            }
        } else if (h > 200) {
            if (m_isCompactMode) {
                setCompactMode(false);
            }
        }
    }
    updateTicksAndPrecision();
}

void ChartWidget::setCompactMode(bool compact)
{
    m_isCompactMode = compact;
    if (m_isCompactMode) {
        m_chart->setMargins(QMargins(4, 2, 4, 2));
        m_axisX->setTitleText(QString());
        m_axisY->setTitleText(QString());
        m_chart->legend()->setVisible(false);
        if (m_channelLegendButton) {
            m_channelLegendButton->setVisible(m_userLegendVisible && !m_channels.isEmpty());
        }
    } else {
        m_chart->setMargins(QMargins(16, 6, 12, 6));
        m_axisX->setTitleText(tr("时间"));
        m_axisY->setTitleText(tr("数值"));
        m_chart->legend()->setVisible(m_userLegendVisible);
        if (m_channelLegendButton) {
            m_channelLegendButton->setVisible(false);
        }
    }
    updateTicksAndPrecision();
    updateInfoLabel();
}

int ChartWidget::toolbarHeight() const
{
    if (m_toolbarWidget && m_toolbarWidget->isVisible()) {
        return qMax(m_toolbarWidget->height(), m_toolbarWidget->minimumHeight());
    }
    return 0;
}

void ChartWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateResponsiveLayout();
}

void ChartWidget::openChannelLegendMenu()
{
    if (!m_channelLegendButton) {
        return;
    }
    QMenu menu(this);
    menu.setTitle(tr("通道图例与可见性"));
    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        const QString channelId = it.key();
        const ChannelSeriesInfo& info = it.value();
        QPixmap iconPix(12, 12);
        iconPix.fill(info.color);
        QAction* act = menu.addAction(QIcon(iconPix), info.displayName);
        act->setCheckable(true);
        act->setChecked(info.visible);
        connect(act, &QAction::toggled, this, [this, channelId](bool checked) {
            setChannelVisible(channelId, checked);
        });
    }
    if (menu.actions().isEmpty()) {
        QAction* emptyAct = menu.addAction(tr("（无通道）"));
        emptyAct->setEnabled(false);
    }
    menu.exec(m_channelLegendButton->mapToGlobal(QPoint(0, m_channelLegendButton->height())));
}

void ChartWidget::updateTicksAndPrecision()
{
    if (!m_axisX || !m_axisY) {
        return;
    }

    const int plotW = (m_chartView && m_chartView->width() > 0) ? m_chartView->width() : width();
    const int plotH = (m_chartView && m_chartView->height() > 0) ? m_chartView->height() : (height() - toolbarHeight());

    // X 轴刻度计算
    const QFontMetrics fmX(m_axisX->labelsFont());
    const int sampleXWidth = fmX.horizontalAdvance(QStringLiteral("00:00:00"));
    const int maxFittingX = qMax(2, plotW / qMax(50, sampleXWidth + 24));
    int countX = 6;
    if (m_isCompactMode) {
        countX = qBound(3, maxFittingX, 4);
    } else {
        countX = qBound(3, maxFittingX, 6);
    }
    m_axisX->setTickCount(countX);

    // Y 轴刻度计算
    const QFontMetrics fmY(m_axisY->labelsFont());
    const int labelH = qMax(12, fmY.height());
    const int maxFittingY = qMax(2, plotH / (labelH * 2 + 10));
    int countY = 6;
    if (m_isCompactMode) {
        countY = qBound(2, maxFittingY, 3);
    } else {
        countY = qBound(2, maxFittingY, 6);
    }
    m_axisY->setTickCount(countY);

    // Y 轴动态精度计算
    const double minY = m_axisY->min();
    const double maxY = m_axisY->max();
    const double rangeY = std::abs(maxY - minY);
    const double interval = (countY > 1) ? (rangeY / (countY - 1)) : rangeY;

    // Y 轴动态精度计算：根据实际刻度步长计算所需精度，移除固定 4 位小数上限，保证刻度可区分
    if (interval <= 0.0 || std::isnan(interval)) {
        m_axisY->setLabelFormat(QStringLiteral("%.1f"));
        return;
    }

    QVector<double> tickValues;
    for (int i = 0; i < countY; ++i) {
        const double val = (countY > 1) ? (minY + (maxY - minY) * i / (countY - 1.0)) : minY;
        tickValues.append(val);
    }

    auto areTicksDistinct = [&](const QString& fmt) -> bool {
        QString prev;
        for (int i = 0; i < tickValues.size(); ++i) {
            const QString curr = QString::asprintf(fmt.toLatin1().constData(), tickValues[i]);
            if (i > 0 && curr == prev) {
                return false;
            }
            prev = curr;
        }
        return true;
    };

    // 检查整数格式 (%.0f) 是否满足条件：刻度间隔 >= 1.0 且所有刻度点均为整数
    if (interval >= 1.0) {
        bool allInts = true;
        for (double v : tickValues) {
            if (std::abs(v - std::round(v)) > 1e-4) {
                allInts = false;
                break;
            }
        }
        if (allInts && areTicksDistinct(QStringLiteral("%.0f"))) {
            m_axisY->setLabelFormat(QStringLiteral("%.0f"));
            return;
        }
    }

    // 根据实际刻度步长计算所需基准小数位数
    int basePrec = 1;
    if (interval < 1.0) {
        basePrec = qMax(1, static_cast<int>(std::ceil(-std::log10(interval) - 1e-5)));
    }

    // 从基准精度开始尝试浮点定点格式，确保相邻刻度标签严格可区分
    for (int p = basePrec; p <= 8; ++p) {
        const QString fmt = QStringLiteral("%.%1f").arg(p);
        if (areTicksDistinct(fmt)) {
            // 当数值跨度极小且需要较多小数位 (p >= 6) 时，若两端数值绝对值较小且科学计数法更紧凑，尝试科学计数法
            if (p >= 6 && std::abs(minY) < 1.0 && std::abs(maxY) < 1.0) {
                for (int ep = 1; ep <= 3; ++ep) {
                    const QString eFmt = QStringLiteral("%.%1e").arg(ep);
                    if (areTicksDistinct(eFmt)) {
                        m_axisY->setLabelFormat(eFmt);
                        return;
                    }
                }
            }
            m_axisY->setLabelFormat(fmt);
            return;
        }
    }

    // 若定点格式尝试完毕仍未能区分，回退尝试科学计数法
    for (int ep = 1; ep <= 5; ++ep) {
        const QString eFmt = QStringLiteral("%.%1e").arg(ep);
        if (areTicksDistinct(eFmt)) {
            m_axisY->setLabelFormat(eFmt);
            return;
        }
    }

    m_axisY->setLabelFormat(QStringLiteral("%.6f"));
}
