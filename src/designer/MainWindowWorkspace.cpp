/**
 * @file MainWindowWorkspace.cpp
 * @brief MainWindow 工作区路由、布局持久化与多工作区管理
 */

#include "MainWindow.h"
#include "SettingsController.h"
#include "ProgramBlocksWidget.h"
#include "ui/InspectorPanel.h"
#include "ui/GlobalStatusBar.h"
#include "Common.h"

#include <QGuiApplication>
#include <QScreen>
#include <QTabWidget>
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

    // 1. 保存当前工作区布局
    if (!m_currentWorkspaceId.isEmpty()) {
        saveWorkspaceLayout(m_currentWorkspaceId);
    }

    // 2. 切换 Tab 页面
    m_currentWorkspaceId = targetWorkspaceId;
    const int targetIndex = tabIndexForWorkspaceId(targetWorkspaceId);
    if (m_workspaceTabs && targetIndex >= 0 && m_workspaceTabs->currentIndex() != targetIndex) {
        m_workspaceTabs->setCurrentIndex(targetIndex);
    }

    // 3. 恢复目标工作区布局状态
    restoreWorkspaceLayout(targetWorkspaceId);

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

    if (targetWorkspaceId != WorkspaceId::Monitor && m_tuningDock) {
        m_tuningDock->setVisible(false);
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
            resizeDocks({m_explorerDock}, {230}, Qt::Horizontal);
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
        m_settingsController->setWorkspaceLayoutState(m_currentWorkspaceId, QByteArray());
    }
    applyDefaultWorkspaceLayout(m_currentWorkspaceId);

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
    if (m_settingsController) {
        m_settingsController->clearWorkspaceLayouts();
    }

    // 恢复所有工作区的默认布局状态，并应用到当前工作区
    applyDefaultWorkspaceLayout(m_currentWorkspaceId);

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
    if (m_workspaceTabs && targetIndex >= 0) {
        m_switchingWorkspace = true;
        m_workspaceTabs->setCurrentIndex(targetIndex);
        m_switchingWorkspace = false;
    }

    restoreWorkspaceLayout(targetWorkspace);

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
}

void MainWindow::saveWindowGeometryAndWorkspaces()
{
    if (!m_settingsController) {
        return;
    }
    m_settingsController->setWindowGeometry(saveGeometry());
    m_settingsController->setActiveWorkspaceId(m_currentWorkspaceId);
    saveWorkspaceLayout(m_currentWorkspaceId);
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
