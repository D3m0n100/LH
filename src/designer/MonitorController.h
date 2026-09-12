/**
 * @file MonitorController.h
 * @brief 监控控制器
 *
 * 封装监控相关的所有逻辑，包括：
 * - 监控的启动/停止
 * - 与 MonitorManager 的交互
 * - 与 MonitorWidget 的协调
 * - 配置应用
 * - 数据导出
 *
 * 此类从 MainWindow 中抽离，降低 MainWindow 的复杂度。
 */

#ifndef MONITORCONTROLLER_H
#define MONITORCONTROLLER_H

#include <QObject>

class QDockWidget;
class QAction;
class MonitorWidget;
struct ProjectRuntimeConfig;

/**
 * @brief 监控控制器
 *
 * 使用方式：
 * @code
 * QDockWidget* dock = new QDockWidget("监控", this);
 * m_monitorController = new MonitorController(dock, this);
 * 
 * // 启动监控
 * m_monitorController->startMonitoring();
 * @endcode
 */
class MonitorController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param dockWidget 承载监控窗口的 Dock 控件
     * @param parent 父对象
     */
    explicit MonitorController(QDockWidget* dockWidget, QObject* parent = nullptr);
    ~MonitorController() override;

    /**
     * @brief 获取 MonitorWidget
     * @return MonitorWidget 指针
     */
    MonitorWidget* widget() const { return m_monitorWidget; }

    /**
     * @brief 获取 Dock 控件
     * @return Dock 控件指针
     */
    QDockWidget* dockWidget() const { return m_dockWidget; }

    /**
     * @brief 检查是否正在监控
     * @return 是否正在监控
     */
    bool isMonitoring() const;

    // ===== 监控控制 =====

    /**
     * @brief 应用运行时配置到监控系统
     * @param config 项目运行时配置
     * @return 是否应用成功
     */
    bool applyConfiguration(const ProjectRuntimeConfig& config);

    /**
     * @brief 启动监控
     * @return 是否启动成功
     */
    bool startMonitoring();

    /**
     * @brief 停止监控
     */
    void stopMonitoring();

    /**
     * @brief 显示监控窗口
     */
    void showMonitorPane();

    /**
     * @brief 隐藏监控窗口
     */
    void hideMonitorPane();

    /**
     * @brief 切换监控窗口可见性
     */
    void toggleMonitorPane();

    // ===== 数据导出 =====

    /**
     * @brief 导出监控数据
     */
    void exportData();

    /**
     * @brief 导出当前图表为图像
     */
    void exportChartImage();

signals:
    /**
     * @brief 监控启动信号
     */
    void monitoringStarted();

    /**
     * @brief 监控停止信号
     */
    void monitoringStopped();

    /**
     * @brief 可见性变化信号
     * @param visible 是否可见
     */
    void visibilityChanged(bool visible);

    /**
     * @brief 状态消息信号
     * @param message 消息内容
     */
    void statusMessage(const QString& message);

    /**
     * @brief 错误消息信号
     * @param message 错误内容
     */
    void errorMessage(const QString& message);

private:
    /**
     * @brief 初始化 UI
     */
    void setupUI();

    /**
     * @brief 连接信号
     */
    void connectSignals();

private:
    QDockWidget* m_dockWidget;          ///< Dock 控件
    MonitorWidget* m_monitorWidget;     ///< 监控窗口控件
};

#endif // MONITORCONTROLLER_H
