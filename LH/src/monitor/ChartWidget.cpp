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
#include <QDebug>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QBrush>
#include <QPen>
#include <QtCharts/QLegendMarker>
#include <algorithm>
#include <cmath>

const QList<QColor> ChartWidget::DEFAULT_COLORS = {
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

ChartWidget::ChartWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_toolbarLayout(nullptr)
    , m_toolbarWidget(nullptr)
    , m_resetZoomButton(nullptr)
    , m_autoScaleCheck(nullptr)
    , m_exportImageButton(nullptr)
    , m_infoLabel(nullptr)
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

    setupControlButtons();
    setupChart();

    connect(m_updateTimer, &QTimer::timeout, this, &ChartWidget::onUpdateTimer);
    m_updateTimer->setInterval(UPDATE_INTERVAL);
    m_updateTimer->start();
    
    m_lastAxisUpdateTime.start();
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
    m_axisX->setTickCount(6);

    m_axisY->setTitleText("数值");
    m_axisY->setLabelFormat("%.2f");
    m_axisY->setTickCount(6);

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
    m_chart->setMargins(QMargins(0, 0, 0, 0));
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
    for (const QPointF& pt : points) {
        info.series->append(pt);
        
        qint64 timestamp = static_cast<qint64>(pt.x());
        if (info.oldestTimestampMs == 0 || timestamp < info.oldestTimestampMs) {
            info.oldestTimestampMs = timestamp;
        }
        if (timestamp > info.newestTimestampMs) {
            info.newestTimestampMs = timestamp;
        }
    }
    
    info.pointCount += points.size();
    
    if (info.pointCount > m_maxPointsPerSeries) {
        enforcePointLimit(channelId);
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
    
    QList<QPointF> allPoints = info.series->points();
    int removeCount = 0;
    
    for (int i = 0; i < allPoints.size(); ++i) {
        if (allPoints[i].x() >= cutoffTime) {
            removeCount = i;
            break;
        }
    }
    
    if (removeCount > 0) {
        // 移除旧点
        info.pointCount -= removeCount;
        
        // 更新最老时间戳
        if (info.pointCount > 0) {
            QList<QPointF> remaining = info.series->points();
            if (!remaining.isEmpty()) {
                info.oldestTimestampMs = static_cast<qint64>(remaining.first().x());
            }
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
    m_chart->legend()->setVisible(visible);
}

bool ChartWidget::isLegendVisible() const
{
    return m_chart->legend()->isVisible();
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

        QList<QPointF> points = info.series->points();
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

    m_infoLabel->setText(QString("通道: %1/%2 | 数据点: %3")
                         .arg(totalChannels)
                         .arg(totalPoints));
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
    QList<QPointF> remaining = info.series->points();
    if (!remaining.isEmpty()) {
        info.oldestTimestampMs = static_cast<qint64>(remaining.first().x());
    }
}
