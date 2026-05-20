/**
 * @file MonitorController.cpp
 * @brief 监控控制器实现
 */

#include "MonitorController.h"
#include "MonitorWidget.h"
#include "MonitorManager.h"
#include "common/ConfigTypes.h"

#include <QDockWidget>
#include <QDateTime>

MonitorController::MonitorController(QDockWidget* dockWidget, QObject* parent)
    : QObject(parent)
    , m_dockWidget(dockWidget)
    , m_monitorWidget(nullptr)
{
    setupUI();
    connectSignals();
}

MonitorController::~MonitorController()
{
    // 停止监控
    if (isMonitoring()) {
        stopMonitoring();
    }
}

void MonitorController::setupUI()
{
    if (!m_dockWidget) {
        return;
    }

    m_monitorWidget = new MonitorWidget(m_dockWidget);
    m_dockWidget->setWidget(m_monitorWidget);
    m_dockWidget->setVisible(false);
}

void MonitorController::connectSignals()
{
    if (!m_dockWidget || !m_monitorWidget) {
        return;
    }

    // 连接可见性变化信号
    connect(m_dockWidget, &QDockWidget::visibilityChanged,
            this, &MonitorController::visibilityChanged);

    // 连接 MonitorWidget 信号
    connect(m_monitorWidget, &MonitorWidget::monitoringStarted,
            this, &MonitorController::monitoringStarted);
    connect(m_monitorWidget, &MonitorWidget::monitoringStopped,
            this, &MonitorController::monitoringStopped);
}

bool MonitorController::isMonitoring() const
{
    if (m_monitorWidget) {
        return m_monitorWidget->isMonitoring();
    }
    return false;
}

// ================= 监控控制 =================

bool MonitorController::applyConfiguration(const ProjectRuntimeConfig& config)
{
    // 应用配置到 MonitorManager
    bool success = Monitor::MonitorManager::instance().applyConfiguration(config);
    
    if (success) {
        emit statusMessage(QString("[%1] 已应用运行时配置到监控系统")
                           .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    } else {
        emit errorMessage(QString("[%1] 应用监控配置失败")
                          .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    }
    
    return success;
}

bool MonitorController::startMonitoring()
{
    if (!m_monitorWidget) {
        emit errorMessage("监控窗口未初始化");
        return false;
    }

    m_monitorWidget->startMonitoring();
    
    emit statusMessage(QString("[%1] 监控已启动")
                       .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    
    return true;
}

void MonitorController::stopMonitoring()
{
    if (m_monitorWidget) {
        m_monitorWidget->stopMonitoring();
        
        emit statusMessage(QString("[%1] 监控已停止")
                           .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    }
}

void MonitorController::showMonitorPane()
{
    if (m_dockWidget) {
        m_dockWidget->setVisible(true);
    }
}

void MonitorController::hideMonitorPane()
{
    if (m_dockWidget) {
        m_dockWidget->setVisible(false);
    }
}

void MonitorController::toggleMonitorPane()
{
    if (m_dockWidget) {
        m_dockWidget->setVisible(!m_dockWidget->isVisible());
    }
}

// ================= 数据导出 =================

void MonitorController::exportData()
{
    if (m_monitorWidget) {
        m_monitorWidget->onExportData();
    }
}

void MonitorController::exportChartImage()
{
    if (m_monitorWidget) {
        m_monitorWidget->exportCurrentChartImage();
    }
}
