#ifndef PARAMETER_TUNING_PANEL_H
#define PARAMETER_TUNING_PANEL_H

#include <QWidget>
#include <QMap>
#include <QList>

#include "../common/ConfigTypes.h"

struct ParameterStateInfo;
class InspectorPanel;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
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

class ParameterTuningPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ParameterTuningPanel(QWidget* parent = nullptr);
    ~ParameterTuningPanel() override;

    void setPidParameterDetails(const QList<ParameterDefinition>& parameters);
    void setParameterDetails(const QList<ParameterDefinition>& parameters);
    void setParameterReadbackReady(const QStringList& readyParameterNames);
    void setParameterDeviationMap(const QMap<QString, double>& deviationMap);
    void setParameterStateMap(const QMap<QString, ParameterStateInfo>& stateMap);

    void setStandaloneMode(bool standalone);
    bool isStandaloneMode() const { return m_isStandalone; }

    void refreshPidTrend();
    InspectorPanel* inspectorPanel() const { return m_panel; }
    QPushButton* popOutButton() const { return m_popOutButton; }
    QtCharts::QLineSeries* pidSeries() const { return m_pidSeries; }
    QLabel* pidSummaryLabel() const { return m_pidSummaryLabel; }

signals:
    void requestCompile();
    void requestRun();
    void requestOpenMonitor();
    void requestEditParameter(const QString& parameterName);
    void requestApplyParameters();
    void requestPopOutWindow();
    void requestDockBack();

private:
    void rebuildPidTabs();
    void rebuildPidSelector();
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

private:
    QWidget* m_topToolBar = nullptr;
    QPushButton* m_popOutButton = nullptr;

    QGroupBox* m_chartGroup = nullptr;
    QTabBar* m_pidGroupTabs = nullptr;
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

    InspectorPanel* m_panel = nullptr;

    QList<ParameterDefinition> m_allPidParameters;
    QList<ParameterDefinition> m_pidParameters;
    QString m_activePidGroup = QStringLiteral("all");
    bool m_isStandalone = false;
    quint64 m_pidHistoryGeneration = 0;
};

#endif // PARAMETER_TUNING_PANEL_H
