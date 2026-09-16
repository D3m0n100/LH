#ifndef INSPECTOR_PANEL_H
#define INSPECTOR_PANEL_H

#include <QWidget>
#include <QMap>

#include "common/ConfigTypes.h"

struct ParameterStateInfo;

class QLabel;
class QTableWidget;
class QTableWidgetItem;
class QPushButton;
class QToolButton;
class QGroupBox;

class InspectorPanel : public QWidget
{
    Q_OBJECT

public:
    enum class PanelMode {
        Inspection,
        Tuning
    };

    explicit InspectorPanel(QWidget* parent = nullptr);
    void setPanelMode(PanelMode mode);

    void setProjectPath(const QString& projectPath);
    void setCurrentFile(const QString& filePath);
    void setWorkspaceName(const QString& workspaceName);
    void setRuntimeState(const QString& runtimeState);
    void setBuildState(const QString& buildState);
    void setMonitoringState(const QString& monitoringState);
    void setDownloadState(const QString& downloadState);
    void setOpcState(const QString& opcState);
    void setVariableSummary(const QString& summary);
    void setParameterSummary(const QString& summary);
    void setResourceSummary(const QString& summary);
    void setParameterDetails(const QList<ParameterDefinition>& parameters);
    void setParameterReadbackReady(const QStringList& readyParameterNames);
    void setParameterDeviationMap(const QMap<QString, double>& deviationMap);
    void setParameterStateMap(const QMap<QString, ParameterStateInfo>& stateMap);

    void clearSelection();
    void setSelectedObject(const QString& type, const QString& name, const QMap<QString, QString>& properties);
    bool hasSelectedObject() const { return m_hasSelection; }
    QString selectedObjectType() const { return m_selectedType; }
    QString selectedObjectName() const { return m_selectedName; }

    bool areDetailColumnsVisible() const;
    void setDetailColumnsVisible(bool visible);
    QPushButton* toggleDetailColumnsButton() const { return m_toggleDetailColumnsButton; }
    QTableWidget* parameterTable() const { return m_parameterTable; }

signals:
    void requestCompile();
    void requestRun();
    void requestOpenMonitor();
    void requestEditParameter(const QString& parameterName);
    void requestApplyParameters();

private:
    void applyStateStyle(QLabel* label, const QString& state);
    void refreshParameterTable();
    void onParameterItemDoubleClicked(QTableWidgetItem* item);
    QString readbackStateFor(const ParameterDefinition& parameter) const;
    QString deviationStateFor(const ParameterDefinition& parameter) const;
    QString issueSummaryFor(const ParameterDefinition& parameter) const;
    void onParameterSelectionChanged();

    bool m_hasSelection = false;
    QString m_selectedType;
    QString m_selectedName;
    QLabel* m_selectionHintLabel = nullptr;
    QGroupBox* m_selectedGroup = nullptr;
    QLabel* m_selectedTypeValue = nullptr;
    QLabel* m_selectedNameValue = nullptr;
    QTableWidget* m_selectedPropsTable = nullptr;

    QLabel* m_projectPathValue = nullptr;
    QLabel* m_currentFileValue = nullptr;
    QLabel* m_workspaceValue = nullptr;
    QLabel* m_runtimeValue = nullptr;
    QLabel* m_buildValue = nullptr;
    QLabel* m_monitorValue = nullptr;
    QLabel* m_downloadValue = nullptr;
    QLabel* m_opcValue = nullptr;
    QLabel* m_variableValue = nullptr;
    QLabel* m_parameterValue = nullptr;
    QLabel* m_resourceValue = nullptr;
    QLabel* m_paramHint = nullptr;
    QTableWidget* m_parameterTable = nullptr;
    QToolButton* m_parameterEditButton = nullptr;
    QToolButton* m_applyParametersButton = nullptr;
    QPushButton* m_toggleDetailColumnsButton = nullptr;
    QList<ParameterDefinition> m_parameterData;
    QStringList m_readbackReadyParameters;
    QMap<QString, double> m_parameterDeviationMap;
    QMap<QString, ParameterStateInfo> m_parameterStateMap;
    PanelMode m_panelMode = PanelMode::Tuning;
    QGroupBox* m_stateGroup = nullptr;
    QGroupBox* m_contextGroup = nullptr;
    QGroupBox* m_paramGroup = nullptr;
};

#endif // INSPECTOR_PANEL_H
