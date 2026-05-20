// 文件：src/monitor/MonitorWidget.h
// 监控主界面控件（性能优化版本）
//
// 导出功能完善版本：
// - 支持 CSV / JSON / TSV 三种格式导出
// - 完整的通道元信息和时间戳语义

#ifndef MONITOR_WIDGET_H
#define MONITOR_WIDGET_H

#include <QWidget>
#include <QSet>
#include <QTimer>
#include <QDateTime>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QHBoxLayout;
class QSplitter;
class QTabWidget;
class QListWidget;
class QListWidgetItem;
class QTableWidget;
class QPushButton;
class QLabel;
class QComboBox;
class QSpinBox;
QT_END_NAMESPACE

class MonitorChartView;
class MonitorDataProcessor;
class MonitorExportHelper;
struct ExportDataPackage;
class SampleDataProvider;

namespace Monitor {
class MonitorManager;
}

/**
 * @brief MonitorWidget
 * 
 * 监控模块主界面，包含：
 * - 左侧通道选择面板
 * - 中央图表视图
 * - 右侧数据表格
 * - 底部控制栏
 * 
 * 【性能优化】
 * - 集成 MonitorDataProcessor 进行增量数据处理
 * - 使用限频渲染的 MonitorChartView
 * - 提供 FPS 和渲染统计显示
 * 
 * 【导出功能】
 * - 支持 CSV / JSON / TSV 三种数据格式
 * - 支持 PNG / JPG / SVG 三种图像格式
 * - 完整的通道元信息和时间戳语义
 */
class MonitorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorWidget(QWidget* parent = nullptr);
    ~MonitorWidget() override;

    // ===== 数据处理器 =====
    MonitorDataProcessor* dataProcessor() const { return m_dataProcessor; }
    MonitorChartView* chartView() const { return m_chartView; }
    MonitorExportHelper* exportHelper() const { return m_exportHelper; }

    // ===== 演示数据模式 =====
    /// 注入演示数据提供器（无硬件时兜底）
    void setSampleDataProvider(SampleDataProvider* provider);

    // ===== 通道选择 =====
    QStringList selectedChannels() const;
    void setSelectedChannels(const QStringList& channelIds);
    void showOnlyChannel(const QString& channelId);
    void selectAllChannels();
    void deselectAllChannels();

    // ===== 监控控制 =====
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const { return m_isMonitoring; }

    // ===== 配置 =====
    void setTimeWindow(qint64 windowMs);
    qint64 timeWindow() const;
    void setTargetFps(int fps);
    int targetFps() const;

public slots:
    // ===== 数据导出 =====
    
    /**
     * @brief 导出监控数据
     * 
     * 弹出文件选择对话框，用户可选择导出格式：
     * - CSV: 逗号分隔，适合 Excel 和通用数据交换
     * - JSON: 结构化数据，适合程序间交换，包含完整元数据
     * - TSV: 制表符分隔，适合直接用 Excel 打开
     * 
     * 导出的数据包含：
     * - 通道元信息（ID、名称、单位、采样周期）
     * - 完整时间戳（ISO 8601 格式 + 毫秒时间戳）
     * - 采样值序列
     */
    void onExportData();
    
    /**
     * @brief 导出当前图表为图片
     * 
     * 弹出文件选择对话框，用户可选择导出格式：
     * - PNG: 位图格式，通用性好
     * - JPG: 压缩位图格式，文件较小
     * - SVG: 矢量格式，可无损缩放
     */
    void exportCurrentChartImage();

signals:
    void monitoringStarted();
    void monitoringStopped();
    void visibleChannelsChanged(const QStringList& channelIds);

private slots:
    void onStartStopClicked();
    void onClearClicked();
    void onChannelItemChanged(QListWidgetItem* item);
    void onChannelItemDoubleClicked(QListWidgetItem* item);
    void onSelectAllClicked();
    void onDeselectAllClicked();
    void onSampleRecorded(const QString& channelName, double value,
                          const QString& unit, const QDateTime& timestamp);
    void onThresholdExceeded(const QString& channelName, double value, double thresholdValue);
    void onRefreshChannelList();
    void onUpdateStats();
    void onTimeWindowChanged(int index);
    void onFpsChanged(int value);
    void onClearAlarmsClicked();

private:
    void setupUI();
    void setupChannelPanel();
    void setupMainContent();
    void setupControlBar();
    void setupStatsTimer();
    void refreshChannelList();
    void updateChannelCountLabel();
    void updateStatsDisplay();
    void syncChannelSelectionToChart();
    void updateDataTableRow(const QString& channelName, double value, const QString& unit);
    
    /**
     * @brief 构建导出数据包
     * @param channelIds 要导出的通道 ID 列表
     * @param fromDatabase 是否从数据库历史拉取（否则使用内存 history）
     * @return 包含完整元数据和样本数据的导出数据包
     */
    ExportDataPackage buildExportDataPackage(const QStringList& channelIds, bool fromDatabase = false);

private:
    // 布局
    QVBoxLayout* m_mainLayout;
    QSplitter* m_splitter;

    // 左侧通道面板
    QWidget* m_channelPanel;
    QVBoxLayout* m_channelPanelLayout;
    QListWidget* m_channelListWidget;
    QPushButton* m_selectAllButton;
    QPushButton* m_deselectAllButton;
    QLabel* m_channelCountLabel;

    // 中央图表
    MonitorChartView* m_chartView;

    // 右侧 Tab（数据/告警）
    QTabWidget* m_rightTabWidget;

    // 数据表格
    QTableWidget* m_dataTable;

    // 告警表格
    QTableWidget* m_alarmTable;
    QPushButton* m_clearAlarmsButton;
    QLabel* m_alarmCountLabel;
    int m_maxAlarmRecords;

    // 底部控制栏
    QWidget* m_controlBar;
    QHBoxLayout* m_controlBarLayout;
    QPushButton* m_startStopButton;
    QPushButton* m_clearButton;
    QComboBox* m_timeWindowCombo;
    QSpinBox* m_fpsSpinBox;
    QLabel* m_statsLabel;

    // 数据处理
    MonitorDataProcessor* m_dataProcessor;
    
    // 导出助手
    MonitorExportHelper* m_exportHelper;

    // 演示数据提供器（Demo Mode）
    SampleDataProvider* m_sampleDataProvider;

    // 状态
    bool m_isMonitoring;
    bool m_demoModeActive;
    bool m_updatingChannelList;
    QTimer* m_statsTimer;
};

#endif // MONITOR_WIDGET_H
