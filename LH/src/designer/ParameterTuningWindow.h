#ifndef PARAMETER_TUNING_WINDOW_H
#define PARAMETER_TUNING_WINDOW_H

#include <QWidget>
#include <QMap>

#include "common/ConfigTypes.h"

class InspectorPanel;
class QComboBox;
class QGroupBox;
class QLabel;
class QCloseEvent;
class QTabBar;
class QTimer;

namespace QtCharts {
class QChart;
class QChartView;
class QDateTimeAxis;
class QAreaSeries;
class QLineSeries;
class QValueAxis;
}

class ParameterTuningWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ParameterTuningWindow(QWidget* parent = nullptr);

    void setPidParameterDetails(const QList<ParameterDefinition>& parameters);
    void setParameterDetails(const QList<ParameterDefinition>& parameters);
    void setParameterReadbackReady(const QStringList& readyParameterNames);
    void setParameterDeviationMap(const QMap<QString, double>& deviationMap);

private:
    void loadWindowState();
    void saveWindowState() const;
    void rebuildPidTabs();
    void rebuildPidSelector();
    void refreshPidTrend();
    QList<ParameterDefinition> filterPidParameters(const QList<ParameterDefinition>& parameters) const;
    QList<ParameterDefinition> filterPidParametersByGroup(const QList<ParameterDefinition>& parameters, const QString& groupKey) const;
    QString selectedPidParameterName() const;
    bool hasSelectedPidParameter() const;
    QString selectedPidGroupKey() const;
    void setSelectedPidGroupKey(const QString& groupKey);
    QString pidGroupForParameter(const ParameterDefinition& parameter) const;
    QString pidGroupLabel(const QString& groupKey) const;
    bool parseRangeValue(const QString& text, double& value) const;
    bool parseParameterRange(const ParameterDefinition& parameter, double& minValue, double& maxValue) const;
    bool computeReasonableRange(const ParameterDefinition& parameter, double& minValue, double& maxValue) const;

protected:
    void closeEvent(QCloseEvent* event) override;

signals:
    void requestCompile();
    void requestRun();
    void requestOpenMonitor();
    void requestEditParameter(const QString& parameterName);
    void requestApplyParameters();

private:
    QTabBar* m_pidGroupTabs = nullptr;
    QGroupBox* m_chartGroup = nullptr;
    QComboBox* m_pidSelector = nullptr;
    QLabel* m_pidSummaryLabel = nullptr;
    QtCharts::QChartView* m_pidChartView = nullptr;
    QtCharts::QChart* m_pidChart = nullptr;
    QtCharts::QLineSeries* m_pidSeries = nullptr;
    QtCharts::QLineSeries* m_pidDefaultLine = nullptr;
    QtCharts::QLineSeries* m_pidCurrentLine = nullptr;
    QtCharts::QLineSeries* m_pidRangeUpperLine = nullptr;
    QtCharts::QLineSeries* m_pidRangeLowerLine = nullptr;
    QtCharts::QAreaSeries* m_pidReasonableBand = nullptr;
    QtCharts::QDateTimeAxis* m_pidAxisX = nullptr;
    QtCharts::QValueAxis* m_pidAxisY = nullptr;
    QTimer* m_pidRefreshTimer = nullptr;
    InspectorPanel* m_panel = nullptr;
    QList<ParameterDefinition> m_allPidParameters;
    QList<ParameterDefinition> m_pidParameters;
    QString m_activePidGroup = QStringLiteral("all");
    bool m_stateLoaded = false;
};

#endif // PARAMETER_TUNING_WINDOW_H
