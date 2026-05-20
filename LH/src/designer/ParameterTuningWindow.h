#ifndef PARAMETER_TUNING_WINDOW_H
#define PARAMETER_TUNING_WINDOW_H

#include <QWidget>
#include <QMap>

#include "common/ConfigTypes.h"

class InspectorPanel;
class QCloseEvent;

class ParameterTuningWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ParameterTuningWindow(QWidget* parent = nullptr);

    void setParameterDetails(const QList<ParameterDefinition>& parameters);
    void setPidParameterDetails(const QList<ParameterDefinition>& parameters);
    void setParameterReadbackReady(const QStringList& readyParameterNames);
    void setParameterDeviationMap(const QMap<QString, double>& deviationMap);

private:
    void loadWindowState();
    void saveWindowState() const;

protected:
    void closeEvent(QCloseEvent* event) override;

signals:
    void requestCompile();
    void requestRun();
    void requestOpenMonitor();
    void requestEditParameter(const QString& parameterName);
    void requestApplyParameters();

private:
    InspectorPanel* m_panel = nullptr;
    bool m_stateLoaded = false;
};

#endif // PARAMETER_TUNING_WINDOW_H
