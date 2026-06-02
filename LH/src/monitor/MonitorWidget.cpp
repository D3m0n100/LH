// 文件: src/monitor/MonitorWidget.cpp
// 监控主界面控件实现（性能优化版本）
// 导出功能完善版本：
// - 支持 CSV / JSON / TSV 三种格式导出
// - 完整的通道元信息和时间戳
// - 格式选择通过文件对话框过滤器实现
#include "MonitorWidget.h"
#include "MonitorChartView.h"
#include "MonitorDataProcessor.h"
#include "MonitorManager.h"
#include "SampleDataProvider.h"
#include "MonitorExportHelper.h"
#include "MonitorTypes.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QHeaderView>
#include <QDateTime>
#include <QDebug>
#include <QPixmap>
#include <QPainter>
#include <QFileDialog>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>

MonitorWidget::MonitorWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_splitter(nullptr)
    , m_channelPanel(nullptr)
    , m_channelPanelLayout(nullptr)
    , m_channelListWidget(nullptr)
    , m_selectAllButton(nullptr)
    , m_deselectAllButton(nullptr)
    , m_channelCountLabel(nullptr)
    , m_chartView(nullptr)
    , m_rightTabWidget(nullptr)
    , m_dataTable(nullptr)
    , m_alarmTable(nullptr)
    , m_clearAlarmsButton(nullptr)
    , m_alarmCountLabel(nullptr)
    , m_maxAlarmRecords(500)
    , m_controlBar(nullptr)
    , m_controlBarLayout(nullptr)
    , m_startStopButton(nullptr)
    , m_clearButton(nullptr)
    , m_timeWindowCombo(nullptr)
    , m_fpsSpinBox(nullptr)
    , m_statsLabel(nullptr)
    , m_dataProcessor(new MonitorDataProcessor(this))
    , m_exportHelper(new MonitorExportHelper(this))
    , m_sampleDataProvider(nullptr)
    , m_isMonitoring(false)
    , m_demoModeActive(false)
    , m_updatingChannelList(false)
    , m_statsTimer(new QTimer(this))
{
    setupUI();
    setupStatsTimer();

    // 设置导出助手的父窗口
    m_exportHelper->setParentWidget(this);

    // 连接 MonitorManager 信号
    auto& manager = Monitor::MonitorManager::instance();
    connect(&manager, &Monitor::MonitorManager::sampleRecorded,
            this, &MonitorWidget::onSampleRecorded);
    connect(&manager, &Monitor::MonitorManager::thresholdExceeded,
            this, &MonitorWidget::onThresholdExceeded);
    connect(&manager, &Monitor::MonitorManager::channelsChanged,
            this, &MonitorWidget::onRefreshChannelList);

    manager.setDataProcessor(m_dataProcessor);

    // 初始化刷新通道列表
    onRefreshChannelList();
}

MonitorWidget::~MonitorWidget()
{
    stopMonitoring();
}

void MonitorWidget::setSampleDataProvider(SampleDataProvider* provider)
{
    m_sampleDataProvider = provider;
}

void MonitorWidget::setupUI()
{
    setStyleSheet(R"(
        QWidget#MonitorChannelPanel {
            background: #ffffff;
            border-right: 1px solid #d0d7de;
        }
        QWidget#MonitorControlBar {
            background: #f3f3f3;
            border-top: 1px solid #d0d7de;
        }
        QLabel#MonitorPanelTitle {
            color: #3b3b3b;
            font-weight: 700;
        }
        QLabel#MonitorMetaLabel {
            color: #5f6a72;
        }
    )");

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(6);

    setupMainContent();
    setupControlBar();
}

void MonitorWidget::setupChannelPanel()
{
    m_channelPanel = new QWidget();
    m_channelPanel->setObjectName("MonitorChannelPanel");
    m_channelPanel->setMinimumWidth(180);
    m_channelPanel->setMaximumWidth(280);

    m_channelPanelLayout = new QVBoxLayout(m_channelPanel);
    m_channelPanelLayout->setContentsMargins(8, 8, 8, 8);
    m_channelPanelLayout->setSpacing(8);

    // 鏍囬
    QLabel* titleLabel = new QLabel(tr("监控通道"));
    titleLabel->setObjectName("MonitorPanelTitle");
    m_channelPanelLayout->addWidget(titleLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(4);

    m_selectAllButton = new QPushButton(tr("全选"));
    m_selectAllButton->setFixedHeight(24);
    connect(m_selectAllButton, &QPushButton::clicked,
            this, &MonitorWidget::onSelectAllClicked);
    buttonLayout->addWidget(m_selectAllButton);

    m_deselectAllButton = new QPushButton(tr("全不选"));
    m_deselectAllButton->setFixedHeight(24);
    connect(m_deselectAllButton, &QPushButton::clicked,
            this, &MonitorWidget::onDeselectAllClicked);
    buttonLayout->addWidget(m_deselectAllButton);

    m_channelPanelLayout->addLayout(buttonLayout);

    // 通道列表
    m_channelListWidget = new QListWidget();
    m_channelListWidget->setSelectionMode(QAbstractItemView::NoSelection);
    connect(m_channelListWidget, &QListWidget::itemChanged,
            this, &MonitorWidget::onChannelItemChanged);
    connect(m_channelListWidget, &QListWidget::itemDoubleClicked,
            this, &MonitorWidget::onChannelItemDoubleClicked);
    m_channelPanelLayout->addWidget(m_channelListWidget);

    // 通道计数标签
    m_channelCountLabel = new QLabel(tr("已选择: 0/0"));
    m_channelCountLabel->setObjectName("MonitorMetaLabel");
    m_channelPanelLayout->addWidget(m_channelCountLabel);
}

void MonitorWidget::setupMainContent()
{
    m_splitter = new QSplitter(Qt::Horizontal);

    // 左侧通道面板
    setupChannelPanel();
    m_splitter->addWidget(m_channelPanel);

    // 中央图表视图
    m_chartView = new MonitorChartView(this);
    m_chartView->setDataProcessor(m_dataProcessor);
    m_splitter->addWidget(m_chartView);

    // 右侧：数据 / 告警 Tab
    m_rightTabWidget = new QTabWidget();
    m_rightTabWidget->setMaximumWidth(320);

    // ---- 数据 Tab ----
    QWidget* dataTab = new QWidget();
    QVBoxLayout* dataLayout = new QVBoxLayout(dataTab);
    dataLayout->setContentsMargins(0, 0, 0, 0);
    dataLayout->setSpacing(4);

    m_dataTable = new QTableWidget();
    m_dataTable->setColumnCount(3);
    m_dataTable->setHorizontalHeaderLabels({tr("通道"), tr("当前值"), tr("单位")});
    m_dataTable->horizontalHeader()->setStretchLastSection(true);
    m_dataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    dataLayout->addWidget(m_dataTable);
    m_rightTabWidget->addTab(dataTab, tr("数据"));

    // ---- 告警 Tab ----
    QWidget* alarmTab = new QWidget();
    QVBoxLayout* alarmLayout = new QVBoxLayout(alarmTab);
    alarmLayout->setContentsMargins(0, 0, 0, 0);
    alarmLayout->setSpacing(4);

    QHBoxLayout* alarmTopBar = new QHBoxLayout();
    alarmTopBar->setContentsMargins(0, 0, 0, 0);
    alarmTopBar->setSpacing(6);

    m_alarmCountLabel = new QLabel(tr("告警: 0"));
    m_alarmCountLabel->setObjectName("MonitorMetaLabel");
    alarmTopBar->addWidget(m_alarmCountLabel);
    alarmTopBar->addStretch();

    m_clearAlarmsButton = new QPushButton(tr("清空告警"));
    m_clearAlarmsButton->setFixedHeight(24);
    connect(m_clearAlarmsButton, &QPushButton::clicked,
            this, &MonitorWidget::onClearAlarmsClicked);
    alarmTopBar->addWidget(m_clearAlarmsButton);

    alarmLayout->addLayout(alarmTopBar);

    m_alarmTable = new QTableWidget();
    m_alarmTable->setColumnCount(4);
    m_alarmTable->setHorizontalHeaderLabels({tr("时间"), tr("通道"), tr("当前值"), tr("阈值")});
    m_alarmTable->horizontalHeader()->setStretchLastSection(true);
    m_alarmTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_alarmTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_alarmTable->setSelectionMode(QAbstractItemView::SingleSelection);
    alarmLayout->addWidget(m_alarmTable);

    m_rightTabWidget->addTab(alarmTab, tr("告警"));

    m_splitter->addWidget(m_rightTabWidget);

    // 设置分割比例
    m_splitter->setStretchFactor(0, 1);  // 通道面板
    m_splitter->setStretchFactor(1, 4);  // 图表
    m_splitter->setStretchFactor(2, 1);  // 右侧 Tab

    m_mainLayout->addWidget(m_splitter);
}

void MonitorWidget::onThresholdExceeded(const QString& channelName, double value, double thresholdValue)
{
    if (!m_alarmTable) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();

    int row = m_alarmTable->rowCount();
    m_alarmTable->insertRow(row);

    auto* timeItem = new QTableWidgetItem(now.toString("yyyy-MM-dd HH:mm:ss.zzz"));
    auto* chItem = new QTableWidgetItem(channelName);
    auto* valItem = new QTableWidgetItem(QString::number(value, 'f', 3));
    auto* thrItem = new QTableWidgetItem(QString::number(thresholdValue, 'f', 3));

    m_alarmTable->setItem(row, 0, timeItem);
    m_alarmTable->setItem(row, 1, chItem);
    m_alarmTable->setItem(row, 2, valItem);
    m_alarmTable->setItem(row, 3, thrItem);

    // 限制条目数，防止内存膨胀
    while (m_alarmTable->rowCount() > m_maxAlarmRecords) {
        m_alarmTable->removeRow(0);
    }

    if (m_alarmCountLabel) {
        m_alarmCountLabel->setText(tr("告警: %1").arg(m_alarmTable->rowCount()));
    }
}

void MonitorWidget::onClearAlarmsClicked()
{
    if (!m_alarmTable) {
        return;
    }
    m_alarmTable->setRowCount(0);
    if (m_alarmCountLabel) {
        m_alarmCountLabel->setText(tr("告警: 0"));
    }
}

void MonitorWidget::setupControlBar()
{
    m_controlBar = new QWidget();
    m_controlBarLayout = new QHBoxLayout(m_controlBar);
    m_controlBar->setObjectName("MonitorControlBar");
    m_controlBarLayout->setContentsMargins(8, 6, 8, 6);
    m_controlBarLayout->setSpacing(8);

    m_startStopButton = new QPushButton(tr("开始监控"));
    m_startStopButton->setFixedWidth(100);
    connect(m_startStopButton, &QPushButton::clicked,
            this, &MonitorWidget::onStartStopClicked);
    m_controlBarLayout->addWidget(m_startStopButton);

    m_clearButton = new QPushButton(tr("清空数据"));
    m_clearButton->setFixedWidth(80);
    connect(m_clearButton, &QPushButton::clicked,
            this, &MonitorWidget::onClearClicked);
    m_controlBarLayout->addWidget(m_clearButton);

    m_controlBarLayout->addSpacing(20);

    QLabel* windowLabel = new QLabel(tr("时间窗口:"));
    m_controlBarLayout->addWidget(windowLabel);

    m_timeWindowCombo = new QComboBox();
    m_timeWindowCombo->addItem(tr("10 秒"), 10000);
    m_timeWindowCombo->addItem(tr("30 秒"), 30000);
    m_timeWindowCombo->addItem(tr("1 分钟"), 60000);
    m_timeWindowCombo->addItem(tr("5 分钟"), 300000);
    m_timeWindowCombo->setCurrentIndex(1);
    connect(m_timeWindowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MonitorWidget::onTimeWindowChanged);
    m_controlBarLayout->addWidget(m_timeWindowCombo);

    m_controlBarLayout->addSpacing(20);

    QLabel* fpsLabel = new QLabel(tr("刷新率:"));
    m_controlBarLayout->addWidget(fpsLabel);

    m_fpsSpinBox = new QSpinBox();
    m_fpsSpinBox->setRange(1, 60);
    m_fpsSpinBox->setValue(25);
    m_fpsSpinBox->setSuffix(" FPS");
    connect(m_fpsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MonitorWidget::onFpsChanged);
    m_controlBarLayout->addWidget(m_fpsSpinBox);

    m_controlBarLayout->addStretch();

    m_statsLabel = new QLabel(tr("就绪"));
    m_statsLabel->setObjectName("MonitorMetaLabel");
    m_controlBarLayout->addWidget(m_statsLabel);

    m_mainLayout->addWidget(m_controlBar);
}

void MonitorWidget::setupStatsTimer()
{
    connect(m_statsTimer, &QTimer::timeout, this, &MonitorWidget::onUpdateStats);
    m_statsTimer->setInterval(1000);
}

// ============================================================================
// 通道选择相关
// ============================================================================

QStringList MonitorWidget::selectedChannels() const
{
    QStringList channels;
    for (int i = 0; i < m_channelListWidget->count(); ++i) {
        QListWidgetItem* item = m_channelListWidget->item(i);
        if (item->checkState() == Qt::Checked) {
            channels.append(item->data(Qt::UserRole).toString());
        }
    }
    return channels;
}

void MonitorWidget::setSelectedChannels(const QStringList& channelIds)
{
    m_updatingChannelList = true;
    for (int i = 0; i < m_channelListWidget->count(); ++i) {
        QListWidgetItem* item = m_channelListWidget->item(i);
        QString channelId = item->data(Qt::UserRole).toString();
        item->setCheckState(channelIds.contains(channelId) ? Qt::Checked : Qt::Unchecked);
    }
    m_updatingChannelList = false;
    syncChannelSelectionToChart();
    updateChannelCountLabel();
}

void MonitorWidget::showOnlyChannel(const QString& channelId)
{
    m_updatingChannelList = true;
    for (int i = 0; i < m_channelListWidget->count(); ++i) {
        QListWidgetItem* item = m_channelListWidget->item(i);
        QString id = item->data(Qt::UserRole).toString();
        item->setCheckState(id == channelId ? Qt::Checked : Qt::Unchecked);
    }
    m_updatingChannelList = false;
    syncChannelSelectionToChart();
    updateChannelCountLabel();
}

void MonitorWidget::selectAllChannels()
{
    m_updatingChannelList = true;
    for (int i = 0; i < m_channelListWidget->count(); ++i) {
        m_channelListWidget->item(i)->setCheckState(Qt::Checked);
    }
    m_updatingChannelList = false;
    syncChannelSelectionToChart();
    updateChannelCountLabel();
}

void MonitorWidget::deselectAllChannels()
{
    m_updatingChannelList = true;
    for (int i = 0; i < m_channelListWidget->count(); ++i) {
        m_channelListWidget->item(i)->setCheckState(Qt::Unchecked);
    }
    m_updatingChannelList = false;
    syncChannelSelectionToChart();
    updateChannelCountLabel();
}

// ============================================================================
// 监控控制
// ============================================================================

void MonitorWidget::startMonitoring()
{
    if (m_isMonitoring) {
        return;
    }

    auto& manager = Monitor::MonitorManager::instance();

    QStringList providers = manager.providerIds();
    if (!providers.isEmpty()) {
        if (m_demoModeActive || (m_sampleDataProvider && m_sampleDataProvider->isRunning())) {
            qInfo() << "[MonitorWidget] Real providers detected, stopping demo data.";
            if (m_sampleDataProvider && m_sampleDataProvider->isRunning()) {
                m_sampleDataProvider->stop();
            }
            m_demoModeActive = false;

            const QStringList channels = manager.channelNames();
            for (const QString& ch : channels) {
                const auto cfg = manager.channelConfig(ch);
                if (cfg.metadata.value("__demoMode").toBool()) {
                    manager.removeChannel(ch);
                }
            }
        }
    } else {
        bool hasDemoChannels = false;
        const QStringList channels = manager.channelNames();
        for (const QString& ch : channels) {
            const auto cfg = manager.channelConfig(ch);
            if (cfg.metadata.value("__demoMode").toBool()) {
                hasDemoChannels = true;
                break;
            }
        }

        if (hasDemoChannels) {
            qInfo() << "[MonitorWidget] Demo channels detected.";
            m_demoModeActive = true;
            onRefreshChannelList();
        } else if (m_sampleDataProvider) {
            if (!m_sampleDataProvider->isRunning()) {
                qInfo() << "[MonitorWidget] Starting sample data provider.";
                m_sampleDataProvider->start();
            }
            m_demoModeActive = true;
            onRefreshChannelList();
        } else {
            qWarning() << "[MonitorWidget] No providers or demo data available.";
            QMessageBox::information(
                this,
                tr("无法开始监控"),
                tr("当前没有可用采集器。\n\n"
                   "请先应用运行时配置以注册监控采集器，"
                   "或启用演示数据模式。"));
            return;
        }
    }

    // 若用户尚未选择通道，默认全选，避免图表空白导致误以为未采样
    if (selectedChannels().isEmpty() && m_channelListWidget && m_channelListWidget->count() > 0) {
        selectAllChannels();
    }

    manager.startMonitoring();

    // 保持现有 statsTimer 与 UI 行为不变
    m_isMonitoring = true;
    m_startStopButton->setText(tr("停止监控"));
    m_statsTimer->start();

    emit monitoringStarted();
    qDebug() << "[MonitorWidget] 监控已启动，providers=" << providers.size();
}

void MonitorWidget::stopMonitoring()
{
    if (!m_isMonitoring) {
        return;
    }

    auto& manager = Monitor::MonitorManager::instance();
    manager.stopMonitoring();

    // Demo Mode：停止演示数据提供器，避免后台继续运行
    if (m_demoModeActive && m_sampleDataProvider && m_sampleDataProvider->isRunning()) {
        m_sampleDataProvider->stop();
        m_demoModeActive = false;
    }

    m_isMonitoring = false;
    m_startStopButton->setText(tr("开始监控"));
    m_statsTimer->stop();

    emit monitoringStopped();
    qDebug() << "[MonitorWidget] Monitoring stopped.";
}

void MonitorWidget::setTimeWindow(qint64 windowMs)
{
    if (m_chartView) {
        m_chartView->setTimeWindow(windowMs);
    }
    if (m_dataProcessor) {
        m_dataProcessor->setTimeWindow(windowMs);
    }
}

qint64 MonitorWidget::timeWindow() const
{
    if (m_chartView) {
        return m_chartView->timeWindow();
    }
    return 30000;
}

void MonitorWidget::setTargetFps(int fps)
{
    if (m_chartView) {
        m_chartView->setTargetFps(fps);
    }
}

int MonitorWidget::targetFps() const
{
    if (m_chartView) {
        return m_chartView->targetFps();
    }
    return 25;
}

// ============================================================================
// 数据导出（核心功能）
// ============================================================================

void MonitorWidget::onExportData()
{
    qDebug() << "[MonitorWidget] 开始导出数据...";
    
    // 1. 获取选中的通道
    QStringList channels = selectedChannels();
    if (channels.isEmpty()) {
        QMessageBox::warning(this, tr("导出失败"),
                            tr("请至少选择一个监控通道"));
        qWarning() << "[MonitorWidget] 导出失败: 没有选中通道";
        return;
    }
    
    bool fromDatabase = false;
    if (Monitor::MonitorManager::instance().isDatabaseHistoryAvailable()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("选择数据源"));
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setText(tr("请选择导出数据来源:"));
        QAbstractButton* memBtn = msgBox.addButton(tr("内存（当前会话）"), QMessageBox::AcceptRole);
        QAbstractButton* dbBtn  = msgBox.addButton(tr("数据库（历史）"), QMessageBox::ActionRole);
        QAbstractButton* cancelBtn = msgBox.addButton(QMessageBox::Cancel);
        msgBox.setDefaultButton(qobject_cast<QPushButton*>(memBtn));
        msgBox.exec();
        QAbstractButton* clicked = msgBox.clickedButton();
        if (clicked == cancelBtn) {
            qDebug() << "[MonitorWidget] 用户取消导出";
            return;
        }
        fromDatabase = (clicked == dbBtn);
    }

    ExportDataPackage package = buildExportDataPackage(channels, fromDatabase);
    
    if (!package.isValid()) {
        QMessageBox::warning(this, tr("导出失败"),
                            tr("选中的通道没有可导出的数据"));
        qWarning() << "[MonitorWidget] 导出失败: 没有数据";
        return;
    }
    
    QString defaultFileName = m_exportHelper->generateMultiChannelFileName("csv");
    QString filter = MonitorExportHelper::dataFormatFilter();
    
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("导出监控数据"),
        QDir::homePath() + "/" + defaultFileName,
        filter
    );
    
    if (filePath.isEmpty()) {
        qDebug() << "[MonitorWidget] 用户取消导出";
        return;
    }
    
    // 4. 根据文件扩展名选择导出格式
    ExportResult result = m_exportHelper->exportPackageAuto(package, filePath);
    
    // 5. 显示导出结果
    if (result.success) {
        QString message = tr("数据导出成功！\n\n"
                            "文件: %1\n"
                            "格式: %2\n"
                            "通道数: %3\n"
                            "样本数: %4\n"
                            "文件大小: %5 字节")
            .arg(result.filePath)
            .arg(result.exportFormat)
            .arg(package.channelInfos.size())
            .arg(result.exportedCount)
            .arg(result.fileSizeBytes);
        
        QMessageBox::information(this, tr("导出成功"), message);
        qDebug() << "[MonitorWidget] 导出成功:" << result.filePath;
    } else {
        QMessageBox::critical(this, tr("导出失败"),
                             tr("导出失败: %1").arg(result.errorMessage));
        qWarning() << "[MonitorWidget] 导出失败:" << result.errorMessage;
    }
}

void MonitorWidget::exportCurrentChartImage()
{
    qDebug() << "[MonitorWidget] 开始导出图表图像...";
    
    if (!m_chartView) {
        QMessageBox::warning(this, tr("导出失败"), tr("图表视图不可用"));
        return;
    }
    
    QString activeChannel = m_chartView->activeChannel();
    QString defaultFileName = m_exportHelper->generateDefaultFileName(
        activeChannel.isEmpty() ? "chart" : activeChannel, "png");
    
    QString filter = MonitorExportHelper::imageFormatFilter();
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("导出图表图像"),
        QDir::homePath() + "/" + defaultFileName,
        filter
    );
    
    if (filePath.isEmpty()) {
        qDebug() << "[MonitorWidget] 用户取消导出";
        return;
    }
    
    // 根据扩展名选择导出格式
    QString ext = QFileInfo(filePath).suffix().toLower();
    ExportResult result;
    
    if (ext == "svg") {
        result = m_exportHelper->exportChartAsSvgToFile(m_chartView, filePath);
    } else if (ext == "jpg" || ext == "jpeg") {
        result = m_exportHelper->exportChartAsJpgToFile(m_chartView, filePath);
    } else {
        result = m_exportHelper->exportChartAsPngToFile(m_chartView, filePath);
    }
    
    // 显示结果
    if (result.success) {
        QMessageBox::information(this, tr("导出成功"),
                                tr("图表已导出到:\n%1").arg(result.filePath));
    } else {
        QMessageBox::critical(this, tr("导出失败"),
                             tr("导出失败: %1").arg(result.errorMessage));
    }
}

ExportDataPackage MonitorWidget::buildExportDataPackage(const QStringList& channelIds, bool fromDatabase)
{
    ExportDataPackage package;
    
    package.metadata.exportTime = QDateTime::currentDateTime();
    package.metadata.timeWindowMs = timeWindow();
    package.metadata.softwareVersion = "1.0.0";
    package.metadata.totalChannels = channelIds.size();
    
    auto& manager = Monitor::MonitorManager::instance();
    
    for (const QString& channelId : channelIds) {
        // 获取通道配置
        Monitor::ChannelConfig config = manager.channelConfig(channelId);

        // 创建通道信息
        ExportChannelInfo info;
        info.channelId = channelId;
        info.displayName = config.displayName.isEmpty() ? channelId : config.displayName;
        info.unit = config.unit;
        bool ok = false;
        int periodMs = config.metadata.value("periodMs").toInt(&ok);
        if (!ok || periodMs <= 0) {
            periodMs = 100;
        }
        info.samplePeriodMs = periodMs;

        // 获取通道历史数据
        QList<Monitor::Sample> samples;
        const qint64 winMs = timeWindow();
        if (fromDatabase && manager.isDatabaseHistoryAvailable()) {
            const QDateTime end = QDateTime::currentDateTimeUtc();
            const QDateTime start = end.addMSecs(-winMs);
            samples = manager.historyFromDatabase(channelId, start, end);

            if (samples.isEmpty()) {
                const int estimate = static_cast<int>(winMs / qMax(1, info.samplePeriodMs));
                samples = manager.historyFromDatabase(channelId, qMax(50, estimate));
            }
        } else {
            // 内存历史（当前会话）
            const int estimate = static_cast<int>(winMs / 100); // 保持与原逻辑一致的估算
            samples = manager.history(channelId, qMax(50, estimate));
        }

        info.sampleCount = samples.size();
        package.channelInfos.append(info);
        package.channelSamples.insert(channelId, samples);
    }
    
    package.metadata.totalSamples = package.totalSampleCount();
    
    qDebug() << "[MonitorWidget] 构建导出数据包: 通道数=" << package.channelInfos.size()
             << " 样本数=" << package.metadata.totalSamples;
    
    return package;
}

// ============================================================================
// 函数实现
// ============================================================================
void MonitorWidget::onStartStopClicked()
{
    if (m_isMonitoring) {
        stopMonitoring();
    } else {
        startMonitoring();
    }
}

void MonitorWidget::onClearClicked()
{
    if (m_chartView) {
        m_chartView->clearAllData();
    }
    if (m_dataProcessor) {
        m_dataProcessor->clearAllCache();
    }
    qDebug() << "[MonitorWidget] Data cleared.";
}

void MonitorWidget::onChannelItemChanged(QListWidgetItem* item)
{
    if (m_updatingChannelList) {
        return;
    }
    
    syncChannelSelectionToChart();
    updateChannelCountLabel();
    
    QString channelId = item->data(Qt::UserRole).toString();
    bool visible = (item->checkState() == Qt::Checked);
    
    emit visibleChannelsChanged(selectedChannels());
    qDebug() << "[MonitorWidget] 通道可见性变化:" << channelId << visible;
}

void MonitorWidget::onChannelItemDoubleClicked(QListWidgetItem* item)
{
    QString channelId = item->data(Qt::UserRole).toString();
    showOnlyChannel(channelId);
}

void MonitorWidget::onSelectAllClicked()
{
    selectAllChannels();
}

void MonitorWidget::onDeselectAllClicked()
{
    deselectAllChannels();
}

void MonitorWidget::onSampleRecorded(const QString& channelName, double value,
                                      const QString& unit, const QDateTime& timestamp)
{
    Q_UNUSED(timestamp);
    
    // 更新数据表格
    updateDataTableRow(channelName, value, unit);
}

void MonitorWidget::onRefreshChannelList()
{
    refreshChannelList();
}

void MonitorWidget::onUpdateStats()
{
    updateStatsDisplay();
}

void MonitorWidget::onTimeWindowChanged(int index)
{
    qint64 windowMs = m_timeWindowCombo->itemData(index).toLongLong();
    setTimeWindow(windowMs);
    qDebug() << "[MonitorWidget] 时间窗口变更为" << windowMs << "ms";
}

void MonitorWidget::onFpsChanged(int value)
{
    setTargetFps(value);
    qDebug() << "[MonitorWidget] 目标帧率变更为" << value << "FPS";
}

// ============================================================================
// 内部辅助方法
// ============================================================================

void MonitorWidget::refreshChannelList()
{
    m_updatingChannelList = true;
    
    QStringList previousSelected = selectedChannels();
    m_channelListWidget->clear();
    
    auto& manager = Monitor::MonitorManager::instance();
    QStringList channelNames = manager.channelNames();
    
    for (const QString& name : channelNames) {
        Monitor::ChannelConfig config = manager.channelConfig(name);
        
        QListWidgetItem* item = new QListWidgetItem(
            config.displayName.isEmpty() ? name : config.displayName);
        item->setData(Qt::UserRole, name);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(previousSelected.contains(name) || previousSelected.isEmpty() 
                           ? Qt::Checked : Qt::Unchecked);
        
        // 设置工具提示
        QString tooltip = tr("通道: %1\n单位: %2")
            .arg(name, config.unit.isEmpty() ? "-" : config.unit);
        item->setToolTip(tooltip);
        
        m_channelListWidget->addItem(item);
    }
    
    m_updatingChannelList = false;
    syncChannelSelectionToChart();
    updateChannelCountLabel();
}

void MonitorWidget::updateChannelCountLabel()
{
    int selected = selectedChannels().size();
    int total = m_channelListWidget->count();
    m_channelCountLabel->setText(tr("已选择: %1/%2").arg(selected).arg(total));
}

void MonitorWidget::updateStatsDisplay()
{
    if (!m_chartView || !m_dataProcessor) {
        return;
    }
    
    // 获取渲染统计
    int fps = m_chartView->targetFps();
    int channelCount = selectedChannels().size();
    
    QString statsText = tr("FPS: %1 | 通道: %2").arg(fps).arg(channelCount);
    m_statsLabel->setText(statsText);
}

void MonitorWidget::syncChannelSelectionToChart()
{
    if (!m_chartView) {
        return;
    }
    
    QStringList channels = selectedChannels();
    
    QStringList allChannels = Monitor::MonitorManager::instance().channelNames();
    for (const QString& channelId : allChannels) {
        bool visible = channels.contains(channelId);
        m_chartView->setChannelVisible(channelId, visible);
    }
}

void MonitorWidget::updateDataTableRow(const QString& channelName, 
                                        double value, const QString& unit)
{
    for (int row = 0; row < m_dataTable->rowCount(); ++row) {
        QTableWidgetItem* nameItem = m_dataTable->item(row, 0);
        if (nameItem && nameItem->text() == channelName) {
            m_dataTable->item(row, 1)->setText(QString::number(value, 'f', 2));
            return;
        }
    }
    
    // 添加新行
    int row = m_dataTable->rowCount();
    m_dataTable->insertRow(row);
    m_dataTable->setItem(row, 0, new QTableWidgetItem(channelName));
    m_dataTable->setItem(row, 1, new QTableWidgetItem(QString::number(value, 'f', 2)));
    m_dataTable->setItem(row, 2, new QTableWidgetItem(unit));
}
