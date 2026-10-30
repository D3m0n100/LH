/**
 * @file MainWindowWorkspace.cpp
 * @brief MainWindow 工作区路由、布局持久化与多工作区管理
 */

#include "MainWindow.h"
#include "SettingsController.h"
#include "ProgramBlocksWidget.h"
#include "DeviceWorkspaceWidget.h"
#include "ui/InspectorPanel.h"
#include "ui/GlobalStatusBar.h"
#include "Common.h"

#include <QGuiApplication>
#include <QScreen>
#include <QTabWidget>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QDockWidget>
#include <QAction>

QString MainWindow::workspaceIdForTabIndex(int index) const
{
    switch (index) {
    case 0:
        return WorkspaceId::Programming;
    case 1:
        return WorkspaceId::Monitor;
    case 2:
        return WorkspaceId::Device;
    default:
        return QString();
    }
}

int MainWindow::tabIndexForWorkspaceId(const QString& workspaceId) const
{
    if (workspaceId == WorkspaceId::Programming) {
        return 0;
    }
    if (workspaceId == WorkspaceId::Monitor) {
        return 1;
    }
    if (workspaceId == WorkspaceId::Device) {
        return 2;
    }
    return -1;
}

void MainWindow::switchToWorkspace(const QString& targetWorkspaceId)
{
    if (targetWorkspaceId.isEmpty()) {
        return;
    }
    if (m_switchingWorkspace) {
        return;
    }
    m_switchingWorkspace = true;

    // System views are peers of documents; showing them does not control the backend.
    m_currentWorkspaceId = targetWorkspaceId;
    QMdiSubWindow* target = targetWorkspaceId == WorkspaceId::Monitor ? m_monitorSubWindow
            : targetWorkspaceId == WorkspaceId::Device ? m_deviceSubWindow : m_editorSubWindow;
    QDockWidget* floatingTarget = target == m_monitorSubWindow ? m_monitorFloatingDock
            : target == m_deviceSubWindow ? m_deviceFloatingDock : nullptr;
    if (target && m_mdiArea) {
        if (floatingTarget) {
            floatingTarget->show();
            floatingTarget->raise();
        } else {
            if (target->widget()) target->widget()->show();
            target->show();
            m_mdiArea->setActiveSubWindow(target);
        }
    }
    const int targetIndex = tabIndexForWorkspaceId(targetWorkspaceId);

    // 4. 更新相关视图动作 checked 状态
    if (m_actToggleDslEditor) {
        m_actToggleDslEditor->blockSignals(true);
        m_actToggleDslEditor->setChecked(targetWorkspaceId == WorkspaceId::Programming);
        m_actToggleDslEditor->blockSignals(false);
    }
    if (m_actToggleMonitorDock) {
        m_actToggleMonitorDock->blockSignals(true);
        m_actToggleMonitorDock->setChecked(targetWorkspaceId == WorkspaceId::Monitor);
        m_actToggleMonitorDock->blockSignals(false);
    }
    if (m_actToggleDownloadDock) {
        m_actToggleDownloadDock->blockSignals(true);
        m_actToggleDownloadDock->setChecked(targetWorkspaceId == WorkspaceId::Device);
        m_actToggleDownloadDock->blockSignals(false);
    }

    // 5. 更新检查面板对应的工作区名称
    if (m_inspectorPanel && m_workspaceTabs && targetIndex >= 0) {
        m_inspectorPanel->setWorkspaceName(m_workspaceTabs->tabText(targetIndex));
    }
    if (targetWorkspaceId == WorkspaceId::Device) {
        updateDeviceWorkspaceInfo();
    }
    updateToolBarForWorkspace(targetWorkspaceId);

    // 6. 按工作区更新详情展开策略（监控/设备页展开通信与采集详情，编程页收起，异常时不强制收起）
    if (m_globalStatusBar) {
        if (targetWorkspaceId == WorkspaceId::Monitor || targetWorkspaceId == WorkspaceId::Device) {
            m_globalStatusBar->setDetailsExpanded(true);
        } else if (m_lastOpcError.isEmpty()) {
            m_globalStatusBar->setDetailsExpanded(false);
        }
    }

    // 7. 记录最后活动工作区到设置
    if (m_settingsController) {
        m_settingsController->setActiveWorkspaceId(targetWorkspaceId);
    }

    m_switchingWorkspace = false;
}

void MainWindow::saveWorkspaceLayout(const QString& workspaceId)
{
    if (workspaceId.isEmpty()) {
        return;
    }
    const QByteArray state = saveState(SettingsController::CURRENT_LAYOUT_VERSION);
    if (m_settingsController) {
        m_settingsController->setWorkspaceLayoutState(workspaceId, state);
    }
}

void MainWindow::restoreWorkspaceLayout(const QString& workspaceId)
{
    QByteArray state;
    if (m_settingsController) {
        if (m_settingsController->layoutVersion() == SettingsController::CURRENT_LAYOUT_VERSION) {
            state = m_settingsController->workspaceLayoutState(workspaceId);
        }
    }

    bool restored = false;
    if (!state.isEmpty()) {
        restored = restoreState(state, SettingsController::CURRENT_LAYOUT_VERSION);
    }

    if (!restored) {
        applyDefaultWorkspaceLayout(workspaceId);
    }

    // 无论恢复还是默认，同步 Dock Action 勾选状态
    if (m_actToggleExplorerDock && m_explorerDock) {
        m_actToggleExplorerDock->blockSignals(true);
        m_actToggleExplorerDock->setChecked(m_explorerDock->isVisible());
        m_actToggleExplorerDock->blockSignals(false);
    }
    if (m_actToggleInspectorDock && m_inspectorDock) {
        m_actToggleInspectorDock->blockSignals(true);
        m_actToggleInspectorDock->setChecked(m_inspectorDock->isVisible());
        m_actToggleInspectorDock->blockSignals(false);
    }
    if (m_actToggleOutputDock && m_logDock) {
        m_actToggleOutputDock->blockSignals(true);
        m_actToggleOutputDock->setChecked(m_logDock->isVisible());
        m_actToggleOutputDock->blockSignals(false);
    }
}

void MainWindow::applyDefaultWorkspaceLayout(const QString& workspaceId)
{
    if (workspaceId == WorkspaceId::Programming) {
        if (m_explorerDock) {
            addDockWidget(Qt::LeftDockWidgetArea, m_explorerDock);
            m_explorerDock->setFloating(false);
            m_explorerDock->setVisible(true);
        }
        if (m_inspectorDock) {
            addDockWidget(Qt::RightDockWidgetArea, m_inspectorDock);
            m_inspectorDock->setFloating(false);
            m_inspectorDock->setVisible(true);
        }
        if (m_tuningDock) {
            m_tuningDock->setFloating(false);
            m_tuningDock->setVisible(false);
        }
        if (m_logDock) {
            addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
            m_logDock->setFloating(false);
            m_logDock->setVisible(true);
            resizeDocks({m_logDock}, {220}, Qt::Vertical);
        }
        if (m_explorerDock && m_inspectorDock) {
            resizeDocks({m_explorerDock, m_inspectorDock}, {145, 145}, Qt::Horizontal);
        }
    } else if (workspaceId == WorkspaceId::Monitor) {
        // 监控工作区：隐藏编程侧栏，隐藏检查面板与输出
        if (m_explorerDock) {
            m_explorerDock->setFloating(false);
            m_explorerDock->setVisible(false);
        }
        if (m_inspectorDock) {
            addDockWidget(Qt::RightDockWidgetArea, m_inspectorDock);
            m_inspectorDock->setFloating(false);
            m_inspectorDock->setVisible(false);
            resizeDocks({m_inspectorDock}, {300}, Qt::Horizontal);
        }
        if (m_tuningDock) {
            m_tuningDock->setFloating(false);
            m_tuningDock->setVisible(false);
        }
        if (m_logDock) {
            addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
            m_logDock->setFloating(false);
            m_logDock->setVisible(false);
            resizeDocks({m_logDock}, {220}, Qt::Vertical);
        }
    } else if (workspaceId == WorkspaceId::Device) {
        // 设备工作区：默认无侧栏
        if (m_explorerDock) {
            m_explorerDock->setFloating(false);
            m_explorerDock->setVisible(false);
        }
        if (m_inspectorDock) {
            addDockWidget(Qt::RightDockWidgetArea, m_inspectorDock);
            m_inspectorDock->setFloating(false);
            m_inspectorDock->setVisible(false);
            resizeDocks({m_inspectorDock}, {300}, Qt::Horizontal);
        }
        if (m_tuningDock) {
            m_tuningDock->setFloating(false);
            m_tuningDock->setVisible(false);
        }
        if (m_logDock) {
            addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
            m_logDock->setFloating(false);
            m_logDock->setVisible(false);
            resizeDocks({m_logDock}, {220}, Qt::Vertical);
        }
    }

    if (m_dslEditor) {
        m_dslEditor->setFunctionListVisible(false);
    }
}

void MainWindow::onResetCurrentWorkspaceLayout()
{
    if (m_settingsController) {
        m_settingsController->setWorkspaceLayoutState(WorkspaceId::Programming, QByteArray());
    }
    applyDefaultWorkspaceLayout(WorkspaceId::Programming);

    if (m_actToggleExplorerDock && m_explorerDock) {
        m_actToggleExplorerDock->setChecked(m_explorerDock->isVisible());
    }
    if (m_actToggleInspectorDock && m_inspectorDock) {
        m_actToggleInspectorDock->setChecked(m_inspectorDock->isVisible());
    }
    if (m_actToggleOutputDock && m_logDock) {
        m_actToggleOutputDock->setChecked(m_logDock->isVisible());
    }
}

void MainWindow::onResetAllWorkspaceLayouts()
{
    // Rehost the existing frontends before discarding layout state. Otherwise a
    // later save would persist the still-open floating docks again.
    setSystemViewFloating(WorkspaceId::Monitor, false);
    setSystemViewFloating(WorkspaceId::Device, false);
    if (m_settingsController) {
        m_settingsController->clearWorkspaceLayouts();
    }

    // 恢复所有工作区的默认布局状态，并应用到当前工作区
    applyDefaultWorkspaceLayout(WorkspaceId::Programming);

    if (m_actToggleExplorerDock && m_explorerDock) {
        m_actToggleExplorerDock->setChecked(m_explorerDock->isVisible());
    }
    if (m_actToggleInspectorDock && m_inspectorDock) {
        m_actToggleInspectorDock->setChecked(m_inspectorDock->isVisible());
    }
    if (m_actToggleOutputDock && m_logDock) {
        m_actToggleOutputDock->setChecked(m_logDock->isVisible());
    }
    if (m_actToggleFunctionList) {
        m_actToggleFunctionList->setChecked(false);
    }
}

void MainWindow::restoreWindowGeometryAndWorkspaces()
{
    if (!m_settingsController) {
        return;
    }
    if (m_settingsController->layoutVersion() != 0
            && m_settingsController->layoutVersion() != SettingsController::CURRENT_LAYOUT_VERSION) {
        m_settingsController->clearWorkspaceLayouts();
        appendOutput(QStringLiteral("旧版工作区布局已重置，主题、字体和最近工程设置已保留。"));
    }
    if (m_monitorFloatingDock) setSystemViewFloating(WorkspaceId::Monitor, false);
    if (m_deviceFloatingDock) setSystemViewFloating(WorkspaceId::Device, false);

    // 1. 恢复窗口位置与尺寸
    const QByteArray geom = m_settingsController->windowGeometry();
    if (!geom.isEmpty()) {
        restoreGeometry(geom);

        // 处理屏幕变化（如拔除外接显示器导致窗口移出可见区域）
        bool onScreen = false;
        const QRect winRect = geometry();
        const auto screens = QGuiApplication::screens();
        for (const auto* screen : screens) {
            if (screen->availableGeometry().intersects(winRect)) {
                onScreen = true;
                break;
            }
        }
        if (!onScreen && !screens.isEmpty()) {
            move(screens.first()->availableGeometry().center() - rect().center());
        }
    }

    // 2. 恢复活动工作区
    QString targetWorkspace = m_settingsController->activeWorkspaceId();
    if (targetWorkspace.isEmpty()) {
        targetWorkspace = WorkspaceId::Programming;
    }

    // 3. 切换到目标工作区并恢复其保存的布局（不提前将初始未恢复状态覆盖保存）
    m_currentWorkspaceId = targetWorkspace;
    const int targetIndex = tabIndexForWorkspaceId(targetWorkspace);
    QMdiSubWindow* target = targetWorkspace == WorkspaceId::Monitor ? m_monitorSubWindow
            : targetWorkspace == WorkspaceId::Device ? m_deviceSubWindow : m_editorSubWindow;
    if (target && m_mdiArea && !m_settingsController->systemViewFloating(targetWorkspace)) {
        if (target->widget()) target->widget()->show();
        target->show();
        m_mdiArea->setActiveSubWindow(target);
    }

    restoreWorkspaceLayout(WorkspaceId::Programming);

    // 同步视图动作 checked 状态
    if (m_actToggleDslEditor) {
        m_actToggleDslEditor->blockSignals(true);
        m_actToggleDslEditor->setChecked(targetWorkspace == WorkspaceId::Programming);
        m_actToggleDslEditor->blockSignals(false);
    }
    if (m_actToggleMonitorDock) {
        m_actToggleMonitorDock->blockSignals(true);
        m_actToggleMonitorDock->setChecked(targetWorkspace == WorkspaceId::Monitor);
        m_actToggleMonitorDock->blockSignals(false);
    }
    if (m_actToggleDownloadDock) {
        m_actToggleDownloadDock->blockSignals(true);
        m_actToggleDownloadDock->setChecked(targetWorkspace == WorkspaceId::Device);
        m_actToggleDownloadDock->blockSignals(false);
    }
    if (m_inspectorPanel && m_workspaceTabs && targetIndex >= 0) {
        m_inspectorPanel->setWorkspaceName(m_workspaceTabs->tabText(targetIndex));
    }

    const auto restoreFloat = [this](const QString& workspaceId, QDockWidget* dock) {
        if (!dock) return;
        const QByteArray saved = m_settingsController->systemViewFloatGeometry(workspaceId);
        if (!saved.isEmpty()) dock->restoreGeometry(saved);
        const auto screens = QGuiApplication::screens();
        const QRect target = dock->frameGeometry();
        const QScreen* matching = nullptr;
        for (const QScreen* screen : screens) {
            if (screen->availableGeometry().intersects(target)) {
                matching = screen;
                break;
            }
        }
        if (!matching && !screens.isEmpty()) matching = screens.first();
        if (matching) {
            const QRect available = matching->availableGeometry();
            const int width = qMin(dock->width(), available.width());
            const int height = qMin(dock->height(), available.height());
            if (width != dock->width() || height != dock->height()) dock->resize(width, height);
            if (!available.contains(dock->frameGeometry())) {
                dock->move(available.center() - dock->rect().center());
            }
        }
        dock->show();
        dock->raise();
    };
    for (const QString& workspaceId : {WorkspaceId::Monitor, WorkspaceId::Device}) {
        if (!m_settingsController->systemViewFloating(workspaceId)) continue;
        setSystemViewFloating(workspaceId, true);
        restoreFloat(workspaceId,
                     workspaceId == WorkspaceId::Monitor ? m_monitorFloatingDock
                                                        : m_deviceFloatingDock);
    }
}

void MainWindow::setSystemViewFloating(const QString& workspaceId, bool floating)
{
    const bool monitor = workspaceId == WorkspaceId::Monitor;
    QMdiSubWindow* sub = monitor ? m_monitorSubWindow : m_deviceSubWindow;
    QWidget* page = monitor ? m_workspaceMonitorPage
                            : static_cast<QWidget*>(m_deviceWorkspaceWidget);
    QDockWidget*& dock = monitor ? m_monitorFloatingDock : m_deviceFloatingDock;
    QAction* action = monitor ? m_actFloatMonitor : m_actFloatDevice;
    if (!sub || !page || !m_mdiArea) {
        return;
    }
    if (floating) {
        if (dock) {
            dock->show();
            dock->raise();
            return;
        }
        dock = new QDockWidget(monitor ? tr("监控") : tr("设备"), this);
        dock->setObjectName(monitor ? QStringLiteral("MonitorFloatingDock")
                                    : QStringLiteral("DeviceFloatingDock"));
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                          | QDockWidget::DockWidgetClosable);
        sub->setWidget(nullptr);
        dock->setWidget(page);
        dock->installEventFilter(this);
        addDockWidget(Qt::RightDockWidgetArea, dock);
        dock->setFloating(true);
        dock->resize(720, 520);
        dock->move(frameGeometry().center() - QPoint(360, 260));
        dock->show();
        dock->raise();
        sub->hide();
        if (action) {
            QSignalBlocker blocker(action);
            action->setChecked(true);
        }
        return;
    }
    if (!dock) {
        return;
    }
    dock->removeEventFilter(this);
    dock->setWidget(nullptr);
    sub->setWidget(page);
    sub->hide();
    removeDockWidget(dock);
    dock->setObjectName(QString());
    dock->hide();
    dock->deleteLater();
    dock = nullptr;
    if (action) {
        QSignalBlocker blocker(action);
        action->setChecked(false);
    }
    if (m_currentWorkspaceId == workspaceId) {
        sub->show();
        m_mdiArea->setActiveSubWindow(sub);
    }
}

void MainWindow::saveWindowGeometryAndWorkspaces()
{
    if (!m_settingsController) {
        return;
    }
    m_settingsController->setWindowGeometry(saveGeometry());
    m_settingsController->setActiveWorkspaceId(m_currentWorkspaceId);
    m_settingsController->setSystemViewFloating(WorkspaceId::Monitor, m_monitorFloatingDock != nullptr);
    m_settingsController->setSystemViewFloating(WorkspaceId::Device, m_deviceFloatingDock != nullptr);
    if (m_monitorFloatingDock) {
        m_settingsController->setSystemViewFloatGeometry(WorkspaceId::Monitor,
                                                         m_monitorFloatingDock->saveGeometry());
    }
    if (m_deviceFloatingDock) {
        m_settingsController->setSystemViewFloatGeometry(WorkspaceId::Device,
                                                         m_deviceFloatingDock->saveGeometry());
    }
    saveWorkspaceLayout(WorkspaceId::Programming);
}

void MainWindow::setSettingsStorage(const QString& organization, const QString& application)
{
    if (m_settingsController) {
        m_settingsController->setSettingsStorage(organization, application);
        m_settingsController->loadSettings();
        restoreWindowGeometryAndWorkspaces();
        // A restored layout may place the hidden tuning dock on the right,
        // which contributes its horizontal minimum even before it is opened.
        // Keep the dormant dock below the workspace until the user opens it.
        if (m_tuningDock && m_tuningDock->isHidden()) {
            addDockWidget(Qt::BottomDockWidgetArea, m_tuningDock);
            m_tuningDock->hide();
        }
    }
}

void MainWindow::onOpenDisplayBlocksWindow()
{
    if (m_explorerDock) {
        m_explorerDock->setVisible(true);
        if (m_actToggleExplorerDock) {
            m_actToggleExplorerDock->blockSignals(true);
            m_actToggleExplorerDock->setChecked(true);
            m_actToggleExplorerDock->blockSignals(false);
        }
    }

    if (m_leftTabs && m_programBlocksWidget) {
        m_leftTabs->setCurrentWidget(m_programBlocksWidget);
        m_programBlocksWidget->filterCategory(QStringLiteral("display"));
    }
}
