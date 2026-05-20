#include "ParameterTuningWindow.h"

#include "ui/InspectorPanel.h"

#include <QCloseEvent>
#include <QSettings>
#include <QVBoxLayout>

ParameterTuningWindow::ParameterTuningWindow(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ParameterTuningWindow"));
    setWindowTitle(QStringLiteral("调参窗口"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(520, 780);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(0);

    m_panel = new InspectorPanel(this);
    layout->addWidget(m_panel);

    connect(m_panel, &InspectorPanel::requestCompile, this, &ParameterTuningWindow::requestCompile);
    connect(m_panel, &InspectorPanel::requestRun, this, &ParameterTuningWindow::requestRun);
    connect(m_panel, &InspectorPanel::requestOpenMonitor, this, &ParameterTuningWindow::requestOpenMonitor);
    connect(m_panel, &InspectorPanel::requestEditParameter, this, &ParameterTuningWindow::requestEditParameter);
    connect(m_panel, &InspectorPanel::requestApplyParameters, this, &ParameterTuningWindow::requestApplyParameters);

    loadWindowState();
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

void ParameterTuningWindow::setParameterDetails(const QList<ParameterDefinition>& parameters)
{
    if (m_panel) m_panel->setParameterDetails(parameters);
}

void ParameterTuningWindow::setPidParameterDetails(const QList<ParameterDefinition>& parameters)
{
    if (m_panel) m_panel->setPidParameterDetails(parameters);
}

void ParameterTuningWindow::setParameterReadbackReady(const QStringList& readyParameterNames)
{
    if (m_panel) m_panel->setParameterReadbackReady(readyParameterNames);
}

void ParameterTuningWindow::setParameterDeviationMap(const QMap<QString, double>& deviationMap)
{
    if (m_panel) m_panel->setParameterDeviationMap(deviationMap);
}
