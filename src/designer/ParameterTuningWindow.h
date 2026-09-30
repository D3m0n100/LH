#ifndef PARAMETER_TUNING_WINDOW_H
#define PARAMETER_TUNING_WINDOW_H

#include <QWidget>
#include <QMap>
#include "../common/ConfigTypes.h"

struct ParameterStateInfo;
class ParameterTuningPanel;
class QCloseEvent;
class QLabel;
namespace QtCharts {
class QLineSeries;
}

class ParameterTuningWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ParameterTuningWindow(QWidget* parent = nullptr);
    ~ParameterTuningWindow() override;

    void setTuningPanel(ParameterTuningPanel* panel);
    void detachTuningPanel();
    ParameterTuningPanel* tuningPanel() const { return m_panel; }
    QtCharts::QLineSeries* pidSeries() const { return m_pidSeries; }
    QLabel* pidSummaryLabel() const { return m_pidSummaryLabel; }

    void setPidParameterDetails(const QList<ParameterDefinition>& parameters);
    void setParameterDetails(const QList<ParameterDefinition>& parameters);
    void setParameterReadbackReady(const QStringList& readyParameterNames);
    void setParameterDeviationMap(const QMap<QString, double>& deviationMap);
    void setParameterStateMap(const QMap<QString, ParameterStateInfo>& stateMap);

protected:
    void closeEvent(QCloseEvent* event) override;

signals:
    void requestCompile();
    void requestRun();
    void requestOpenMonitor();
    void requestEditParameter(const QString& parameterName);
    void requestApplyParameters();
    void requestDockBack();

private:
    void loadWindowState();
    void saveWindowState() const;

private:
    ParameterTuningPanel* m_panel = nullptr;
    bool m_ownsPanel = false;
    bool m_stateLoaded = false;
    QtCharts::QLineSeries* m_pidSeries = nullptr;
    QLabel* m_pidSummaryLabel = nullptr;
};

#endif // PARAMETER_TUNING_WINDOW_H
