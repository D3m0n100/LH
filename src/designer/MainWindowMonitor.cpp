#include "MainWindow.h"

#include "MonitorWidget.h"
#include "ProjectController.h"
#include "RuntimeSessionController.h"
#include "ui/GlobalStatusBar.h"

#include <QAction>
#include <QDateTime>
#include <QDockWidget>
#include <QTabWidget>

void MainWindow::startDemoModeIfNeeded(const QString& reason)
{
    m_sessionController->startDemoMode(reason);
    m_demoModeActive = m_sessionController->isDemoMode();
}

void MainWindow::stopDemoMode(const QString& reason)
{
    m_sessionController->stopDemoMode(reason);
    m_demoModeActive = m_sessionController->isDemoMode();
}

bool MainWindow::applyRuntimeConfigToMonitor()
{
    // 更新 GlobalStatusBar 协议和采样率
    if (m_projectController && m_globalStatusBar) {
        const auto& cfg = m_projectController->runtimeConfig();
        m_globalStatusBar->setProtocolName(cfg.protocol);
        int maxHz = 0;
        for (const auto& provider : cfg.providers) {
            if (provider.periodMs > 0) {
                const int hz = qMax(1, 1000 / provider.periodMs);
                maxHz = qMax(maxHz, hz);
            }
        }
        m_globalStatusBar->setSamplingRateHz(maxHz);
    }

    return m_sessionController->applyRuntimeConfig();
}

void MainWindow::onToggleMonitorDock(bool checked)
{
    switchToWorkspace(checked ? WorkspaceId::Monitor : WorkspaceId::Programming);
}

void MainWindow::onOpenMonitor()
{
    switchToWorkspace(WorkspaceId::Monitor);
    refreshInspectorPanel();
}

void MainWindow::onStartMonitoring()
{
    if (m_projectController && m_projectController->hasOpenProject()) {
        m_sessionController->applyRuntimeConfigAsync([this](bool ok) {
            if (ok) m_sessionController->startMonitoring();
        });
        return;
    } else {
        appendOutput(QString("[%1] 未打开项目，无法启动真实监控。请先打开项目并配置采集器。")
                         .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    }
    refreshInspectorPanel();
}

void MainWindow::onStopMonitoring()
{
    if (m_sessionController->isMonitoring()) {
        m_sessionController->stopMonitoring();
    } else if (m_monitorWidget) {
        m_monitorWidget->stopMonitoring();
    }

    // 若处于 Demo Mode，则停止演示数据采集
    stopDemoMode(QStringLiteral("停止监控"));
    refreshInspectorPanel();
}

void MainWindow::onExportMonitorData()
{
    if (m_monitorWidget) {
        m_monitorWidget->onExportData();
    }
}

void MainWindow::onExportMonitorImage()
{
    if (m_monitorWidget) {
        m_monitorWidget->exportCurrentChartImage();
    }
}

void MainWindow::onMonitorThresholdExceeded(const QString& channelName, double value, double thresholdValue)
{
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    appendOutput(QString("[%1] [ALARM] 阈值超限: %2 当前值=%3 阈值=%4")
                    .arg(ts)
                    .arg(channelName)
                     .arg(value, 0, 'f', 3)
                     .arg(thresholdValue, 0, 'f', 3));
    addProblem("error", "监控", QString("%1 value=%2 threshold=%3")
               .arg(channelName)
               .arg(value, 0, 'f', 3)
               .arg(thresholdValue, 0, 'f', 3));
}
