#include "ParameterTuningWindow.h"

#include "ui/InspectorPanel.h"
#include "monitor/MonitorManager.h"

#include <QtCharts/QChart>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <QCloseEvent>
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QFont>
#include <QLabel>
#include <QMargins>
#include <QPen>
#include <QPainter>
#include <QPointF>
#include <QTabBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

bool containsToken(const QString& text, const QString& token)
{
    if (text.isEmpty() || token.isEmpty()) {
        return false;
    }

    const QString lower = text.toLower();
    int index = lower.indexOf(token);
    while (index >= 0) {
        const int beforeIndex = index - 1;
        const int afterIndex = index + token.size();
        const bool beforeOk = beforeIndex < 0 || !lower.at(beforeIndex).isLetterOrNumber();
        const bool afterOk = afterIndex >= lower.size() || !lower.at(afterIndex).isLetterOrNumber();
        if (beforeOk && afterOk) {
            return true;
        }
        index = lower.indexOf(token, index + 1);
    }
    return false;
}

bool isPidLikeParameter(const ParameterDefinition& parameter)
{
    const QString name = parameter.name.trimmed().toLower();
    const QString dataType = parameter.dataType.trimmed().toLower();
    const QString unit = parameter.unit.trimmed().toLower();
    const QString kind = parameter.metadata.value("kind").toString().trimmed().toLower();
    const QString role = parameter.metadata.value("role").toString().trimmed().toLower();
    const QString category = parameter.metadata.value("category").toString().trimmed().toLower();

    const QString combined = QStringList{ name, dataType, unit, kind, role, category }.join(' ');
    if (combined.contains("pid")) {
        return true;
    }

    if (containsToken(combined, "kp") ||
        containsToken(combined, "ki") ||
        containsToken(combined, "kd")) {
        return true;
    }

    return false;
}

QString pidGroupForParameterName(const QString& name)
{
    const QString lower = name.trimmed().toLower();
    if (lower.contains("kp")) {
        return QStringLiteral("kp");
    }
    if (lower.contains("ki")) {
        return QStringLiteral("ki");
    }
    if (lower.contains("kd")) {
        return QStringLiteral("kd");
    }
    return QStringLiteral("other");
}

QString pidGroupLabelForKey(const QString& groupKey)
{
    if (groupKey == QStringLiteral("kp")) {
        return QStringLiteral("Kp");
    }
    if (groupKey == QStringLiteral("ki")) {
        return QStringLiteral("Ki");
    }
    if (groupKey == QStringLiteral("kd")) {
        return QStringLiteral("Kd");
    }
    if (groupKey == QStringLiteral("other")) {
        return QStringLiteral("其他");
    }
    return QStringLiteral("全部");
}

QString makePidLabel(const ParameterDefinition& parameter)
{
    if (parameter.unit.trimmed().isEmpty()) {
        return parameter.name;
    }
    return QStringLiteral("%1 [%2]").arg(parameter.name, parameter.unit);
}

} // namespace

ParameterTuningWindow::ParameterTuningWindow(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ParameterTuningWindow"));
    setWindowTitle(QStringLiteral("PID 调参窗口"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMinimumSize(580, 500);
    resize(680, 560);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    m_chartGroup = new QGroupBox(QStringLiteral("PID 曲线"), this);
    m_chartGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_chartGroup->setMinimumHeight(248);
    m_chartGroup->setMaximumHeight(288);

    auto* chartGroupLayout = new QVBoxLayout(m_chartGroup);
    chartGroupLayout->setContentsMargins(10, 12, 10, 10);
    chartGroupLayout->setSpacing(6);

    m_pidGroupTabs = new QTabBar(m_chartGroup);
    m_pidGroupTabs->setDrawBase(false);
    m_pidGroupTabs->setExpanding(false);
    m_pidGroupTabs->addTab(QStringLiteral("全部"));
    m_pidGroupTabs->addTab(QStringLiteral("Kp"));
    m_pidGroupTabs->addTab(QStringLiteral("Ki"));
    m_pidGroupTabs->addTab(QStringLiteral("Kd"));
    m_pidGroupTabs->addTab(QStringLiteral("其他"));
    chartGroupLayout->addWidget(m_pidGroupTabs);

    auto* selectorRow = new QHBoxLayout();
    selectorRow->setContentsMargins(0, 0, 0, 0);
    selectorRow->setSpacing(8);

    auto* selectorLabel = new QLabel(QStringLiteral("查看参数"), m_chartGroup);
    selectorLabel->setMinimumWidth(56);
    selectorRow->addWidget(selectorLabel);

    m_pidSelector = new QComboBox(m_chartGroup);
    m_pidSelector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    selectorRow->addWidget(m_pidSelector, 1);

    m_pidSummaryLabel = new QLabel(QStringLiteral("等待 PID 参数数据"), m_chartGroup);
    m_pidSummaryLabel->setStyleSheet("QLabel { color: #57606a; }");
    selectorRow->addWidget(m_pidSummaryLabel, 1);
    chartGroupLayout->addLayout(selectorRow);

    m_pidChart = new QtCharts::QChart();
    m_pidChart->legend()->hide();
    m_pidChart->setAnimationOptions(QtCharts::QChart::NoAnimation);
    m_pidChart->setMargins(QMargins(0, 0, 0, 0));
    m_pidChart->setBackgroundBrush(QBrush(QColor("#ffffff")));
    m_pidChart->setTitle(QStringLiteral("PID 参数趋势"));
    m_pidChart->setTitleFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10, QFont::DemiBold));

    m_pidRangeUpperLine = new QtCharts::QLineSeries();
    m_pidRangeLowerLine = new QtCharts::QLineSeries();
    m_pidReasonableBand = new QtCharts::QAreaSeries(m_pidRangeUpperLine, m_pidRangeLowerLine);
    m_pidReasonableBand->setBrush(QColor(46, 160, 67, 36));
    QPen bandPen(QColor(46, 160, 67, 90));
    bandPen.setWidth(1);
    m_pidReasonableBand->setPen(bandPen);
    m_pidReasonableBand->setName(QStringLiteral("合理区间"));
    m_pidChart->addSeries(m_pidReasonableBand);

    m_pidSeries = new QtCharts::QLineSeries(m_pidChart);
    QPen seriesPen(QColor("#0969da"));
    seriesPen.setWidth(2);
    m_pidSeries->setPen(seriesPen);
    m_pidSeries->setName(QStringLiteral("参数值"));
    m_pidChart->addSeries(m_pidSeries);

    m_pidDefaultLine = new QtCharts::QLineSeries(m_pidChart);
    QPen defaultPen(QColor("#8c959f"));
    defaultPen.setStyle(Qt::DashLine);
    defaultPen.setWidth(1);
    m_pidDefaultLine->setPen(defaultPen);
    m_pidDefaultLine->setName(QStringLiteral("默认值"));
    m_pidChart->addSeries(m_pidDefaultLine);

    m_pidCurrentLine = new QtCharts::QLineSeries(m_pidChart);
    QPen currentPen(QColor("#d1242f"));
    currentPen.setStyle(Qt::DotLine);
    currentPen.setWidth(1);
    m_pidCurrentLine->setPen(currentPen);
    m_pidCurrentLine->setName(QStringLiteral("当前值"));
    m_pidChart->addSeries(m_pidCurrentLine);

    m_pidAxisX = new QtCharts::QDateTimeAxis();
    m_pidAxisX->setFormat(QStringLiteral("HH:mm:ss"));
    m_pidAxisX->setTitleText(QStringLiteral("时间"));
    m_pidAxisX->setLabelsFont(QFont(QStringLiteral("Microsoft YaHei UI"), 8));

    m_pidAxisY = new QtCharts::QValueAxis();
    m_pidAxisY->setTitleText(QStringLiteral("数值"));
    m_pidAxisY->setLabelFormat(QStringLiteral("%.3f"));
    m_pidAxisY->setLabelsFont(QFont(QStringLiteral("Microsoft YaHei UI"), 8));

    m_pidChart->addAxis(m_pidAxisX, Qt::AlignBottom);
    m_pidChart->addAxis(m_pidAxisY, Qt::AlignLeft);
    m_pidReasonableBand->attachAxis(m_pidAxisX);
    m_pidReasonableBand->attachAxis(m_pidAxisY);
    m_pidSeries->attachAxis(m_pidAxisX);
    m_pidSeries->attachAxis(m_pidAxisY);
    m_pidDefaultLine->attachAxis(m_pidAxisX);
    m_pidDefaultLine->attachAxis(m_pidAxisY);
    m_pidCurrentLine->attachAxis(m_pidAxisX);
    m_pidCurrentLine->attachAxis(m_pidAxisY);

    m_pidChartView = new QtCharts::QChartView(m_pidChart, m_chartGroup);
    m_pidChartView->setRenderHint(QPainter::Antialiasing, true);
    m_pidChartView->setRubberBand(QtCharts::QChartView::NoRubberBand);
    m_pidChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_pidChartView->setMinimumHeight(180);
    chartGroupLayout->addWidget(m_pidChartView, 1);

    layout->addWidget(m_chartGroup);

    m_panel = new InspectorPanel(this);
    m_panel->setPanelMode(InspectorPanel::PanelMode::Tuning);
    m_panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_panel, 1);

    connect(m_panel, &InspectorPanel::requestCompile, this, &ParameterTuningWindow::requestCompile);
    connect(m_panel, &InspectorPanel::requestRun, this, &ParameterTuningWindow::requestRun);
    connect(m_panel, &InspectorPanel::requestOpenMonitor, this, &ParameterTuningWindow::requestOpenMonitor);
    connect(m_panel, &InspectorPanel::requestEditParameter, this, &ParameterTuningWindow::requestEditParameter);
    connect(m_panel, &InspectorPanel::requestApplyParameters, this, &ParameterTuningWindow::requestApplyParameters);
    connect(m_pidSelector, &QComboBox::currentTextChanged, this, &ParameterTuningWindow::refreshPidTrend);
    connect(m_pidGroupTabs, &QTabBar::currentChanged, this, [this](int index) {
        if (!m_pidGroupTabs || index < 0 || index >= m_pidGroupTabs->count()) {
            return;
        }
        setSelectedPidGroupKey(m_pidGroupTabs->tabData(index).toString());
    });

    m_pidRefreshTimer = new QTimer(this);
    m_pidRefreshTimer->setInterval(1500);
    connect(m_pidRefreshTimer, &QTimer::timeout, this, &ParameterTuningWindow::refreshPidTrend);
    m_pidRefreshTimer->start();

    loadWindowState();
    refreshPidTrend();
}

void ParameterTuningWindow::closeEvent(QCloseEvent* event)
{
    saveWindowState();
    hide();
    if (event) {
        event->ignore();
    }
}

void ParameterTuningWindow::loadWindowState()
{
    if (m_stateLoaded) {
        return;
    }

    QSettings settings("ServoValve", "ControlPlatform");
    settings.beginGroup("ParameterTuningWindow");
    const QByteArray geometry = settings.value("geometry").toByteArray();
    settings.endGroup();

    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    m_stateLoaded = true;
}

void ParameterTuningWindow::saveWindowState() const
{
    QSettings settings("ServoValve", "ControlPlatform");
    settings.beginGroup("ParameterTuningWindow");
    settings.setValue("geometry", saveGeometry());
    settings.endGroup();
}

void ParameterTuningWindow::rebuildPidTabs()
{
    if (!m_pidGroupTabs) {
        return;
    }

    const QString previousGroup = selectedPidGroupKey();
    QSignalBlocker blocker(m_pidGroupTabs);

    auto countForGroup = [this](const QString& groupKey) -> int {
        if (groupKey == QStringLiteral("all")) {
            return m_allPidParameters.size();
        }
        int count = 0;
        for (const auto& parameter : m_allPidParameters) {
            if (pidGroupForParameter(parameter) == groupKey) {
                ++count;
            }
        }
        return count;
    };

    const QStringList groupKeys = {
        QStringLiteral("all"),
        QStringLiteral("kp"),
        QStringLiteral("ki"),
        QStringLiteral("kd"),
        QStringLiteral("other")
    };

    for (int i = 0; i < m_pidGroupTabs->count() && i < groupKeys.size(); ++i) {
        const QString& key = groupKeys[i];
        const int count = countForGroup(key);
        const QString label = QStringLiteral("%1(%2)").arg(pidGroupLabelForKey(key)).arg(count);
        m_pidGroupTabs->setTabText(i, label);
        m_pidGroupTabs->setTabData(i, key);
    }

    int selectedIndex = 0;
    for (int i = 0; i < m_pidGroupTabs->count(); ++i) {
        if (m_pidGroupTabs->tabData(i).toString() == previousGroup) {
            selectedIndex = i;
            break;
        }
    }
    m_pidGroupTabs->setCurrentIndex(selectedIndex);
}

QList<ParameterDefinition> ParameterTuningWindow::filterPidParameters(const QList<ParameterDefinition>& parameters) const
{
    return filterPidParametersByGroup(parameters, selectedPidGroupKey());
}

QList<ParameterDefinition> ParameterTuningWindow::filterPidParametersByGroup(const QList<ParameterDefinition>& parameters,
                                                                              const QString& groupKey) const
{
    QList<ParameterDefinition> pidParameters;
    for (const auto& parameter : parameters) {
        if (!parameter.onlineEditable || !isPidLikeParameter(parameter)) {
            continue;
        }
        const QString pidGroup = pidGroupForParameter(parameter);
        if (groupKey == QStringLiteral("all") || pidGroup == groupKey) {
            pidParameters.append(parameter);
        }
    }
    return pidParameters;
}

QString ParameterTuningWindow::selectedPidGroupKey() const
{
    if (!m_pidGroupTabs || m_pidGroupTabs->currentIndex() < 0) {
        return m_activePidGroup;
    }
    const QString key = m_pidGroupTabs->tabData(m_pidGroupTabs->currentIndex()).toString();
    return key.isEmpty() ? m_activePidGroup : key;
}

void ParameterTuningWindow::setSelectedPidGroupKey(const QString& groupKey)
{
    const QString normalized = groupKey.isEmpty() ? QStringLiteral("all") : groupKey;
    m_activePidGroup = normalized;
    m_pidParameters = filterPidParametersByGroup(m_allPidParameters, normalized);

    if (m_pidGroupTabs) {
        QSignalBlocker blocker(m_pidGroupTabs);
        for (int i = 0; i < m_pidGroupTabs->count(); ++i) {
            if (m_pidGroupTabs->tabData(i).toString() == normalized) {
                m_pidGroupTabs->setCurrentIndex(i);
                break;
            }
        }
    }

    rebuildPidSelector();
    refreshPidTrend();
}

QString ParameterTuningWindow::pidGroupForParameter(const ParameterDefinition& parameter) const
{
    const QString pidGroup = pidGroupForParameterName(parameter.name);
    if (pidGroup != QStringLiteral("other")) {
        return pidGroup;
    }
    return pidGroup;
}

QString ParameterTuningWindow::pidGroupLabel(const QString& groupKey) const
{
    return pidGroupLabelForKey(groupKey);
}

bool ParameterTuningWindow::parseRangeValue(const QString& text, double& value) const
{
    bool ok = false;
    value = text.trimmed().toDouble(&ok);
    return ok && std::isfinite(value);
}

bool ParameterTuningWindow::parseParameterRange(const ParameterDefinition& parameter, double& minValue, double& maxValue) const
{
    bool okMin = false;
    bool okMax = false;
    const double parsedMin = parameter.minValue.trimmed().toDouble(&okMin);
    const double parsedMax = parameter.maxValue.trimmed().toDouble(&okMax);

    if (okMin && okMax) {
        minValue = parsedMin;
        maxValue = parsedMax;
        if (maxValue < minValue) {
            std::swap(minValue, maxValue);
        }
        return std::isfinite(minValue) && std::isfinite(maxValue);
    }

    return false;
}

bool ParameterTuningWindow::computeReasonableRange(const ParameterDefinition& parameter, double& minValue, double& maxValue) const
{
    if (parseParameterRange(parameter, minValue, maxValue) && maxValue > minValue) {
        return true;
    }

    double defaultValue = 0.0;
    double currentValue = 0.0;
    const bool hasDefault = parseRangeValue(parameter.defaultValue, defaultValue);
    const bool hasCurrent = parseRangeValue(parameter.currentValue, currentValue);

    if (hasDefault && hasCurrent) {
        minValue = std::min(defaultValue, currentValue);
        maxValue = std::max(defaultValue, currentValue);
        if (std::abs(maxValue - minValue) <= 1e-9) {
            const double padding = std::max(std::abs(defaultValue) * 0.1, 1.0);
            minValue -= padding;
            maxValue += padding;
        }
        return true;
    }

    if (hasDefault || hasCurrent) {
        const double center = hasCurrent ? currentValue : defaultValue;
        const double padding = std::max(std::abs(center) * 0.1, 1.0);
        minValue = center - padding;
        maxValue = center + padding;
        return true;
    }

    return false;
}

QString ParameterTuningWindow::selectedPidParameterName() const
{
    if (!m_pidSelector) {
        return QString();
    }
    return m_pidSelector->currentData().toString();
}

bool ParameterTuningWindow::hasSelectedPidParameter() const
{
    return !selectedPidParameterName().trimmed().isEmpty();
}

void ParameterTuningWindow::rebuildPidSelector()
{
    if (!m_pidSelector) {
        return;
    }

    const QString previousSelection = selectedPidParameterName();
    QSignalBlocker blocker(m_pidSelector);
    m_pidSelector->clear();

    for (const auto& parameter : m_pidParameters) {
        m_pidSelector->addItem(makePidLabel(parameter), parameter.name);
    }

    if (m_pidParameters.isEmpty()) {
        m_pidSelector->addItem(QStringLiteral("未识别到 PID 参数"), QString());
        m_pidSelector->setEnabled(false);
        if (m_pidSummaryLabel) {
            m_pidSummaryLabel->setText(QStringLiteral("%1 组没有可调 PID 参数").arg(pidGroupLabel(selectedPidGroupKey())));
        }
        return;
    }

    m_pidSelector->setEnabled(true);
    int index = m_pidSelector->findData(previousSelection);
    if (index < 0) {
        index = 0;
    }
    m_pidSelector->setCurrentIndex(index);
}

void ParameterTuningWindow::refreshPidTrend()
{
    if (!m_pidChart || !m_pidSeries || !m_pidAxisX || !m_pidAxisY || !m_pidSummaryLabel || !m_pidReasonableBand) {
        return;
    }

    if (!hasSelectedPidParameter()) {
        m_pidSeries->clear();
        if (m_pidDefaultLine) {
            m_pidDefaultLine->clear();
        }
        if (m_pidCurrentLine) {
            m_pidCurrentLine->clear();
        }
        if (m_pidRangeUpperLine) {
            m_pidRangeUpperLine->clear();
        }
        if (m_pidRangeLowerLine) {
            m_pidRangeLowerLine->clear();
        }
        m_pidChart->setTitle(QStringLiteral("PID 参数趋势"));
        m_pidSummaryLabel->setText(QStringLiteral("当前项目没有可调 PID 参数"));
        return;
    }

    const QString parameterName = selectedPidParameterName();
    const auto it = std::find_if(m_pidParameters.begin(), m_pidParameters.end(),
                                 [&parameterName](const ParameterDefinition& parameter) {
                                     return parameter.name == parameterName;
                                 });
    if (it == m_pidParameters.end()) {
        return;
    }

    const QString channelName = QStringLiteral("param::%1").arg(parameterName);
    QList<Monitor::Sample> samples = Monitor::MonitorManager::instance().history(channelName, 120);
    if (samples.isEmpty() && Monitor::MonitorManager::instance().isDatabaseHistoryAvailable()) {
        samples = Monitor::MonitorManager::instance().historyFromDatabase(channelName, 120);
    }

    m_pidSeries->clear();
    if (m_pidDefaultLine) {
        m_pidDefaultLine->clear();
    }
    if (m_pidCurrentLine) {
        m_pidCurrentLine->clear();
    }
    if (m_pidRangeUpperLine) {
        m_pidRangeUpperLine->clear();
    }
    if (m_pidRangeLowerLine) {
        m_pidRangeLowerLine->clear();
    }

    const QString unitText = it->unit.trimmed().isEmpty() ? QString() : QStringLiteral(" (%1)").arg(it->unit);
    m_pidChart->setTitle(QStringLiteral("%1 参数趋势%2").arg(it->name, unitText));

    QString summary = QStringLiteral("参数：%1").arg(it->name);
    if (!it->currentValue.isEmpty()) {
        summary += QStringLiteral(" | 当前值 %1").arg(it->currentValue);
    }
    if (!it->defaultValue.isEmpty()) {
        summary += QStringLiteral(" | 默认值 %1").arg(it->defaultValue);
    }
    summary += QStringLiteral(" | 历史样本 %1").arg(samples.size());
    m_pidSummaryLabel->setText(summary);

    double reasonableMin = 0.0;
    double reasonableMax = 0.0;
    const bool hasReasonableRange = computeReasonableRange(*it, reasonableMin, reasonableMax);

    if (samples.isEmpty()) {
        const QDateTime now = QDateTime::currentDateTime();
        m_pidAxisX->setRange(now.addSecs(-30), now);

        double fallbackMin = 0.0;
        double fallbackMax = 1.0;
        if (hasReasonableRange) {
            fallbackMin = reasonableMin;
            fallbackMax = reasonableMax;
        } else {
            bool ok = false;
            const double center = it->currentValue.toDouble(&ok);
            const double base = ok ? center : it->defaultValue.toDouble(&ok);
            if (ok) {
                const double padding = std::max(std::abs(base) * 0.1, 1.0);
                fallbackMin = base - padding;
                fallbackMax = base + padding;
            }
        }
        m_pidAxisY->setRange(fallbackMin, fallbackMax);

        const qint64 xMinMs = now.addSecs(-30).toMSecsSinceEpoch();
        const qint64 xMaxMs = now.toMSecsSinceEpoch();
        auto updateHorizontalLine = [xMinMs, xMaxMs](QtCharts::QLineSeries* series, const QString& valueText) {
            if (!series) {
                return;
            }
            bool ok = false;
            const double value = valueText.toDouble(&ok);
            series->clear();
            if (!ok) {
                return;
            }
            series->append(xMinMs, value);
            series->append(xMaxMs, value);
        };
        updateHorizontalLine(m_pidDefaultLine, it->defaultValue);
        updateHorizontalLine(m_pidCurrentLine, it->currentValue);

        if (m_pidRangeUpperLine && m_pidRangeLowerLine && hasReasonableRange) {
            m_pidRangeUpperLine->append(xMinMs, reasonableMax);
            m_pidRangeUpperLine->append(xMaxMs, reasonableMax);
            m_pidRangeLowerLine->append(xMinMs, reasonableMin);
            m_pidRangeLowerLine->append(xMaxMs, reasonableMin);
        }
        return;
    }

    QVector<QPointF> points;
    points.reserve(samples.size());

    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    QDateTime firstTimestamp;
    QDateTime lastTimestamp;

    for (const auto& sample : samples) {
        points.append(sample.toPoint());
        minY = std::min(minY, sample.value);
        maxY = std::max(maxY, sample.value);
        if (!firstTimestamp.isValid()) {
            firstTimestamp = sample.timestamp;
        }
        lastTimestamp = sample.timestamp;
    }

    m_pidSeries->replace(points);

    if (!firstTimestamp.isValid() || !lastTimestamp.isValid() || firstTimestamp >= lastTimestamp) {
        const QDateTime now = QDateTime::currentDateTime();
        m_pidAxisX->setRange(now.addSecs(-30), now.addSecs(30));
    } else {
        m_pidAxisX->setRange(firstTimestamp, lastTimestamp);
    }

    if (hasReasonableRange) {
        minY = std::min(minY, reasonableMin);
        maxY = std::max(maxY, reasonableMax);
    }

    double span = maxY - minY;
    if (!std::isfinite(span) || span <= 1e-9) {
        bool ok = false;
        const double center = it->currentValue.toDouble(&ok);
        if (!ok) {
            const double fallback = samples.last().value;
            minY = fallback - 1.0;
            maxY = fallback + 1.0;
        } else {
            const double padding = std::max(std::abs(center) * 0.1, 1.0);
            minY = center - padding;
            maxY = center + padding;
        }
    } else {
        const double padding = std::max(span * 0.15, 0.1);
        minY -= padding;
        maxY += padding;
    }
    m_pidAxisY->setRange(minY, maxY);

    const qint64 xMinMs = m_pidAxisX->min().toMSecsSinceEpoch();
    const qint64 xMaxMs = m_pidAxisX->max().toMSecsSinceEpoch();
    auto updateHorizontalLine = [xMinMs, xMaxMs](QtCharts::QLineSeries* series, const QString& valueText) {
        if (!series) {
            return;
        }
        bool ok = false;
        const double value = valueText.toDouble(&ok);
        series->clear();
        if (!ok) {
            return;
        }
        series->append(xMinMs, value);
        series->append(xMaxMs, value);
    };
    updateHorizontalLine(m_pidDefaultLine, it->defaultValue);
    updateHorizontalLine(m_pidCurrentLine, it->currentValue);

    if (m_pidRangeUpperLine && m_pidRangeLowerLine && hasReasonableRange) {
        m_pidRangeUpperLine->append(xMinMs, reasonableMax);
        m_pidRangeUpperLine->append(xMaxMs, reasonableMax);
        m_pidRangeLowerLine->append(xMinMs, reasonableMin);
        m_pidRangeLowerLine->append(xMaxMs, reasonableMin);
    }
}

void ParameterTuningWindow::setParameterDetails(const QList<ParameterDefinition>& parameters)
{
    setPidParameterDetails(parameters);
}

void ParameterTuningWindow::setPidParameterDetails(const QList<ParameterDefinition>& parameters)
{
    m_allPidParameters = filterPidParametersByGroup(parameters, QStringLiteral("all"));
    m_pidParameters = filterPidParametersByGroup(m_allPidParameters, selectedPidGroupKey());
    rebuildPidTabs();
    if (m_panel) {
        m_panel->setParameterDetails(m_pidParameters);
    }
    rebuildPidSelector();
    refreshPidTrend();
}

void ParameterTuningWindow::setParameterReadbackReady(const QStringList& readyParameterNames)
{
    if (m_panel) m_panel->setParameterReadbackReady(readyParameterNames);
}

void ParameterTuningWindow::setParameterDeviationMap(const QMap<QString, double>& deviationMap)
{
    if (m_panel) m_panel->setParameterDeviationMap(deviationMap);
}
