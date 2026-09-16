#include "ParameterTuningWindow.h"
#include "ParameterTuningPanel.h"

#include <QVBoxLayout>
#include <QCloseEvent>
#include <QSettings>

ParameterTuningWindow::ParameterTuningWindow(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ParameterTuningWindow"));
    setWindowTitle(QStringLiteral("PID 调参窗口"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_QuitOnClose, false);
    setMinimumSize(580, 500);
    resize(680, 560);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 默认创建自有 panel，若后续宿主注入共享 panel 则接管
    auto* defaultPanel = new ParameterTuningPanel(this);
    m_ownsPanel = true;
    setTuningPanel(defaultPanel);

    loadWindowState();
}

ParameterTuningWindow::~ParameterTuningWindow()
{
    if (m_ownsPanel && m_panel) {
        delete m_panel;
        m_panel = nullptr;
    }
}

void ParameterTuningWindow::setTuningPanel(ParameterTuningPanel* panel)
{
    if (m_panel == panel) {
        return;
    }

    if (m_ownsPanel && m_panel && m_panel != panel) {
        delete m_panel;
        m_ownsPanel = false;
        m_panel = nullptr;
    }

    m_panel = panel;
    if (!m_panel) {
        m_pidSeries = nullptr;
        m_pidSummaryLabel = nullptr;
        return;
    }

    m_pidSeries = m_panel->pidSeries();
    m_pidSummaryLabel = m_panel->pidSummaryLabel();

    m_panel->setParent(this);
    m_panel->setStandaloneMode(true);
    m_panel->show();
    if (layout()) {
        layout()->addWidget(m_panel);
    }

    connect(m_panel, &ParameterTuningPanel::requestCompile, this, &ParameterTuningWindow::requestCompile, Qt::UniqueConnection);
    connect(m_panel, &ParameterTuningPanel::requestRun, this, &ParameterTuningWindow::requestRun, Qt::UniqueConnection);
    connect(m_panel, &ParameterTuningPanel::requestOpenMonitor, this, &ParameterTuningWindow::requestOpenMonitor, Qt::UniqueConnection);
    connect(m_panel, &ParameterTuningPanel::requestEditParameter, this, &ParameterTuningWindow::requestEditParameter, Qt::UniqueConnection);
    connect(m_panel, &ParameterTuningPanel::requestApplyParameters, this, &ParameterTuningWindow::requestApplyParameters, Qt::UniqueConnection);
    connect(m_panel, &ParameterTuningPanel::requestDockBack, this, [this]() {
        hide();
        emit requestDockBack();
    }, Qt::UniqueConnection);
}

void ParameterTuningWindow::detachTuningPanel()
{
    if (m_panel && layout()) {
        layout()->removeWidget(m_panel);
    }
    m_panel = nullptr;
    m_ownsPanel = false;
    m_pidSeries = nullptr;
    m_pidSummaryLabel = nullptr;
}

void ParameterTuningWindow::setPidParameterDetails(const QList<ParameterDefinition>& parameters)
{
    if (m_panel) {
        m_panel->setPidParameterDetails(parameters);
        m_pidSeries = m_panel->pidSeries();
        m_pidSummaryLabel = m_panel->pidSummaryLabel();
    }
}

void ParameterTuningWindow::setParameterDetails(const QList<ParameterDefinition>& parameters)
{
    if (m_panel) {
        m_panel->setParameterDetails(parameters);
    }
}

void ParameterTuningWindow::setParameterReadbackReady(const QStringList& readyParameterNames)
{
    if (m_panel) {
        m_panel->setParameterReadbackReady(readyParameterNames);
    }
}

void ParameterTuningWindow::setParameterDeviationMap(const QMap<QString, double>& deviationMap)
{
    if (m_panel) {
        m_panel->setParameterDeviationMap(deviationMap);
    }
}

void ParameterTuningWindow::setParameterStateMap(const QMap<QString, ParameterStateInfo>& stateMap)
{
    if (m_panel) {
        m_panel->setParameterStateMap(stateMap);
    }
}

void ParameterTuningWindow::closeEvent(QCloseEvent* event)
{
    saveWindowState();
    hide();
    emit requestDockBack();
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
