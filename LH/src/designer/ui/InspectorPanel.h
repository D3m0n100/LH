#ifndef INSPECTOR_PANEL_H
#define INSPECTOR_PANEL_H

#include <QWidget>
#include <QMap>

#include "common/ConfigTypes.h"

class QLabel;
class QTableWidget;
class QTableWidgetItem;
class QPushButton;
class QToolButton;

class InspectorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit InspectorPanel(QWidget* parent = nullptr);

    void setProjectPath(const QString& projectPath);
    void setCurrentFile(const QString& filePath);
    void setWorkspaceName(const QString& workspaceName);
    void setRuntimeState(const QString& runtimeState);
    void setBuildState(const QString& buildState);
    void setMonitoringState(const QString& monitoringState);
    void setVariableSummary(const QString& summary);
    void setParameterSummary(const QString& summary);
    void setResourceSummary(const QString& summary);
    void setParameterDetails(const QList<ParameterDefinition>& parameters);
    void setPidParameterDetails(const QList<ParameterDefinition>& parameters);
    void setParameterReadbackReady(const QStringList& readyParameterNames);
    void setParameterDeviationMap(const QMap<QString, double>& deviationMap);

signals:
    void requestCompile();
    void requestRun();
    void requestOpenMonitor();
    void requestEditParameter(const QString& parameterName);
    void requestApplyParameters();

private:
    void applyStateStyle(QLabel* label, const QString& state);
    void refreshParameterTable();
    void refreshPidParameterTable();
    void onParameterItemDoubleClicked(QTableWidgetItem* item);
    QString readbackStateFor(const ParameterDefinition& parameter) const;
    QString deviationStateFor(const ParameterDefinition& parameter) const;

    QLabel* m_projectPathValue = nullptr;
    QLabel* m_currentFileValue = nullptr;
    QLabel* m_workspaceValue = nullptr;
    QLabel* m_runtimeValue = nullptr;
    QLabel* m_buildValue = nullptr;
    QLabel* m_monitorValue = nullptr;
    QLabel* m_variableValue = nullptr;
    QLabel* m_parameterValue = nullptr;
    QLabel* m_resourceValue = nullptr;
    QTableWidget* m_parameterTable = nullptr;
    QToolButton* m_parameterEditButton = nullptr;
    QList<ParameterDefinition> m_parameterData;
    QTableWidget* m_pidParameterTable = nullptr;
    QList<ParameterDefinition> m_pidParameterData;
    QStringList m_readbackReadyParameters;
    QMap<QString, double> m_parameterDeviationMap;
};

#endif // INSPECTOR_PANEL_H
