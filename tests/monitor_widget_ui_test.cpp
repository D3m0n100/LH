// tests/monitor_widget_ui_test.cpp
#include <QApplication>
#include <QTest>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QListWidget>
#include <QTableWidget>
#include <QTabWidget>

#include "MonitorWidget.h"
#include "MonitorManager.h"
#include "MonitorTypes.h"
#include "SampleDataProvider.h"
#include "MonitorDataProcessor.h"
#include "MonitorChartView.h"
#include "MonitorExportHelper.h"
#include "ChartWidget.h"
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>

class MonitorWidgetUiTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void channelFilterAndSelectionPreservationTest();
    void detailsPanelCollapseAndAlarmBadgeTest();
    void clearDisplayActionTest();
    void monitoringStartStopWordingTest();
    void viewDestructionKeepsMonitoringAndProcessor();
    void demoChannelsAreIsolatedAndRetained();
    void exportMarksChannelSource();
    void hiddenViewKeepsRecordingWithBoundedDisplay();
    void hiddenViewDefersRenderingUntilShown();
    void sustainedRecordingKeepsBoundedCaches();
    void sampleTimeAndStoppedStateAreVisible();
    void reopenedViewRestoresLastSampleAndMarksBadQuality();
    void twoViewsReceiveTheSameSamples();
    void sampleBecomesStaleAfterConfiguredPeriods();
};

void MonitorWidgetUiTest::sampleBecomesStaleAfterConfiguredPeriods()
{
    auto& manager = Monitor::MonitorManager::instance();
    manager.stopMonitoring();
    Monitor::ProviderConfig provider; provider.id = QStringLiteral("freshness.provider");
    provider.channelName = QStringLiteral("freshness.channel"); provider.periodMs = 20;
    provider.sampler = [] { return 1.0; };
    QVERIFY(manager.registerProvider(provider));
    MonitorWidget widget;
    widget.syncMonitoringState(true);
    emit manager.sampleRecorded(provider.channelName, 3.0, QStringLiteral("V"), QDateTime::currentDateTimeUtc());
    auto* table = widget.findChild<QTableWidget*>(QStringLiteral("MonitorDataTable")); QVERIFY(table);
    int row = -1;
    for (int i = 0; i < table->rowCount(); ++i)
        if (table->item(i, 0)->text() == provider.channelName) row = i;
    QVERIFY(row >= 0);
    QTest::qWait(90);
    QVERIFY(QMetaObject::invokeMethod(&widget, "onUpdateStats", Qt::DirectConnection));
    QVERIFY(table->item(row, 4)->text().contains(QStringLiteral("超过三个采样周期")));
    QCOMPARE(table->item(row, 1)->text(), QStringLiteral("3"));
    widget.syncMonitoringState(false);
    QVERIFY(manager.unregisterProvider(provider.id));
    manager.removeChannel(provider.channelName);
}

void MonitorWidgetUiTest::twoViewsReceiveTheSameSamples()
{
    MonitorDataProcessor processor;
    MonitorChartView first, second;
    first.setDataProcessor(&processor); second.setDataProcessor(&processor);
    first.addChannel(QStringLiteral("shared")); second.addChannel(QStringLiteral("shared"));
    first.addChannelSeries(QStringLiteral("shared")); second.addChannelSeries(QStringLiteral("shared"));
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    processor.appendPoint(QStringLiteral("shared"), QPointF(now, 1));
    processor.appendPoint(QStringLiteral("shared"), QPointF(now + 1, 2));
    QVERIFY(QMetaObject::invokeMethod(&first, "onRenderTimer", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&second, "onRenderTimer", Qt::DirectConnection));
    for (auto* view : {&first, &second}) {
        auto* chart = view->chartWidget()->findChild<QtCharts::QChartView*>(); QVERIFY(chart);
        QtCharts::QLineSeries* shared = nullptr;
        for (auto* series : chart->chart()->series())
            if (series->name() == QStringLiteral("shared")) shared = qobject_cast<QtCharts::QLineSeries*>(series);
        QVERIFY(shared);
        QCOMPARE(shared->count(), 2);
        QCOMPARE(shared->at(0).y(), 1.0);
        QCOMPARE(shared->at(1).y(), 2.0);
    }
}

void MonitorWidgetUiTest::reopenedViewRestoresLastSampleAndMarksBadQuality()
{
    auto& manager = Monitor::MonitorManager::instance();
    manager.stopMonitoring();
    Monitor::ChannelConfig cfg; cfg.name = QStringLiteral("restore-test");
    QVERIFY(manager.registerChannel(cfg));
    const QDateTime time = QDateTime::currentDateTimeUtc();
    manager.recordSample(Monitor::Sample(cfg.name, 7.25, QStringLiteral("V"), time));
    MonitorWidget widget;
    auto* table = widget.findChild<QTableWidget*>(QStringLiteral("MonitorDataTable"));
    QVERIFY(table);
    int row = -1;
    for (int i = 0; i < table->rowCount(); ++i)
        if (table->item(i, 0)->text() == cfg.name) row = i;
    QVERIFY(row >= 0);
    QCOMPARE(table->item(row, 1)->text(), QStringLiteral("7.25"));
    QVERIFY(table->item(row, 4)->text().contains(QStringLiteral("陈旧")));
    emit widget.dataProcessor()->channelQualityUpdated(cfg.name, RuntimePointQuality::Bad, false);
    QVERIFY(table->item(row, 4)->text().contains(QStringLiteral("质量异常")));
    manager.removeChannel(cfg.name);
}

void MonitorWidgetUiTest::sampleTimeAndStoppedStateAreVisible()
{
    MonitorWidget widget;
    auto* table = widget.findChild<QTableWidget*>(QStringLiteral("MonitorDataTable"));
    QVERIFY(table);
    const QDateTime time = QDateTime::fromString(QStringLiteral("2026-10-28T10:15:30.123Z"), Qt::ISODateWithMs);
    widget.syncMonitoringState(true);
    emit Monitor::MonitorManager::instance().sampleRecorded(QStringLiteral("time-test"), 12.5, QStringLiteral("V"), time);
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("12.5"));
    QCOMPARE(table->item(0, 3)->text(), time.toLocalTime().toString(Qt::ISODateWithMs));
    QVERIFY(table->item(0, 4)->text().contains(QStringLiteral("新鲜度未知")));
    emit Monitor::MonitorManager::instance().sampleRecorded(QStringLiteral("time-test"), 99.0, QStringLiteral("V"), time.addSecs(-1));
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("12.5"));
    widget.syncMonitoringState(false);
    QVERIFY(table->item(0, 4)->text().contains(QStringLiteral("陈旧")));
}

void MonitorWidgetUiTest::demoChannelsAreIsolatedAndRetained()
{
    auto& manager = Monitor::MonitorManager::instance();
    Monitor::ChannelConfig real;
    real.name = QStringLiteral("系统压力");
    real.displayName = real.name;
    QVERIFY(manager.registerChannel(real));
    SampleDataProvider demo;
    demo.start();
    QVERIFY(demo.isRunning());
    const QString demoId = QStringLiteral("__lh_demo__/pressure");
    QVERIFY(manager.hasChannel(real.name));
    QVERIFY(manager.hasChannel(demoId));
    QVERIFY(manager.channelConfig(demoId).metadata.value(QStringLiteral("__demoMode")).toBool());
    QVERIFY(manager.channelConfig(demoId).displayName.contains(QStringLiteral("演示数据")));
    demo.stop();
    QVERIFY(manager.hasChannel(real.name));
    QVERIFY(manager.hasChannel(demoId));
    manager.removeChannel(real.name);
    for (const auto& id : {demoId, QStringLiteral("__lh_demo__/flow"),
                           QStringLiteral("__lh_demo__/temperature"), QStringLiteral("__lh_demo__/position")})
        manager.removeChannel(id);
}

void MonitorWidgetUiTest::exportMarksChannelSource()
{
    ExportDataPackage package;
    ExportChannelInfo demo(QStringLiteral("__lh_demo__/pressure"),
                           QStringLiteral("系统压力（演示数据）"), QStringLiteral("bar"), 500);
    demo.source = QStringLiteral("demo");
    demo.sampleCount = 1;
    ExportChannelInfo real(QStringLiteral("real.pressure"),
                           QStringLiteral("系统压力"), QStringLiteral("bar"), 100);
    real.source = QStringLiteral("real");
    real.sampleCount = 1;
    package.channelInfos = {demo, real};
    package.channelSamples.insert(demo.channelId,
                                  {Monitor::Sample(demo.channelId, 1.0, QStringLiteral("bar"),
                                                   QDateTime::currentDateTimeUtc())});
    package.channelSamples.insert(real.channelId,
                                  {Monitor::Sample(real.channelId, 2.0, QStringLiteral("bar"),
                                                   QDateTime::currentDateTimeUtc())});
    package.metadata.totalChannels = 2;
    package.metadata.totalSamples = 2;

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    MonitorExportHelper helper;
    ExportConfig config;
    config.alignMultiChannelByTime = false;
    helper.setConfig(config);

    const QString csvPath = dir.filePath(QStringLiteral("source.csv"));
    QVERIFY(helper.exportPackageAsCsv(package, csvPath).success);
    QFile csv(csvPath);
    QVERIFY(csv.open(QIODevice::ReadOnly));
    const QString csvText = QString::fromUtf8(csv.readAll());
    QVERIFY(csvText.contains(QStringLiteral("Source: demo")));
    QVERIFY(csvText.contains(QStringLiteral("Source: real")));
    QVERIFY(csvText.contains(QStringLiteral("channel_name,source,value")));
    QVERIFY(csvText.contains(QStringLiteral("__lh_demo__/pressure")));
    QVERIFY(csvText.contains(QStringLiteral(",demo,")));
    QVERIFY(csvText.contains(QStringLiteral(",real,")));

    const QString jsonPath = dir.filePath(QStringLiteral("source.json"));
    QVERIFY(helper.exportPackageAsJson(package, jsonPath).success);
    QFile json(jsonPath);
    QVERIFY(json.open(QIODevice::ReadOnly));
    const QJsonArray channels = QJsonDocument::fromJson(json.readAll())
                                    .object().value(QStringLiteral("channels")).toArray();
    QCOMPARE(channels.size(), 2);
    QCOMPARE(channels.at(0).toObject().value(QStringLiteral("source")).toString(),
             QStringLiteral("demo"));
    QCOMPARE(channels.at(1).toObject().value(QStringLiteral("source")).toString(),
             QStringLiteral("real"));

    QCOMPARE(exportSourceFromChannelMetadata({{QStringLiteral("__demoMode"), true}}),
             QStringLiteral("demo"));
    QCOMPARE(exportSourceFromChannelMetadata({}), QStringLiteral("real"));

    ExportPageProvider provider = [](const QString& channelId, const ExportCursor&, int) {
        ExportPage page;
        page.samples = {Monitor::Sample(channelId, 3.0, QStringLiteral("bar"),
                                        QDateTime::currentDateTimeUtc())};
        page.hasMore = false;
        return page;
    };
    ExportMetadata pagedMetadata;
    pagedMetadata.totalChannels = 1;
    pagedMetadata.totalSamples = 1;
    const QString pagedCsvPath = dir.filePath(QStringLiteral("paged.csv"));
    QVERIFY(helper.exportPackagePaged({demo}, pagedMetadata, provider, pagedCsvPath).success);
    QFile pagedCsv(pagedCsvPath);
    QVERIFY(pagedCsv.open(QIODevice::ReadOnly));
    const QString pagedCsvText = QString::fromUtf8(pagedCsv.readAll());
    QVERIFY(pagedCsvText.contains(QStringLiteral("Source: demo")));
    QVERIFY(pagedCsvText.contains(QStringLiteral("channel_name,source,value")));
    QVERIFY(pagedCsvText.contains(QStringLiteral(",demo,")));

    const QString pagedJsonPath = dir.filePath(QStringLiteral("paged.json"));
    QVERIFY(helper.exportPackagePaged({demo}, pagedMetadata, provider, pagedJsonPath).success);
    QFile pagedJson(pagedJsonPath);
    QVERIFY(pagedJson.open(QIODevice::ReadOnly));
    const QJsonArray pagedChannels = QJsonDocument::fromJson(pagedJson.readAll())
                                         .object().value(QStringLiteral("channels")).toArray();
    QCOMPARE(pagedChannels.size(), 1);
    QCOMPARE(pagedChannels.at(0).toObject().value(QStringLiteral("source")).toString(),
             QStringLiteral("demo"));
}

void MonitorWidgetUiTest::hiddenViewKeepsRecordingWithBoundedDisplay()
{
    auto& manager = Monitor::MonitorManager::instance();
    manager.stopMonitoring();
    Monitor::ChannelConfig config;
    config.name = QStringLiteral("hidden.channel");
    config.displayName = config.name;
    config.unit = QStringLiteral("V");
    config.maxSamples = 50;
    QVERIFY(manager.registerChannel(config));

    MonitorWidget widget;
    widget.hide();
    auto* processor = widget.dataProcessor();
    QVERIFY(processor);
    processor->setMaxDisplaySamples(10);
    processor->setRingBufferCapacity(20);

    manager.startMonitoring();
    for (int i = 0; i < 100; ++i) {
        manager.recordSample(config.name, i, config.unit);
    }

    QVERIFY(manager.isMonitoring());
    const QList<Monitor::Sample> history = manager.history(config.name, 1000);
    QCOMPARE(history.size(), 50);
    QCOMPARE(history.last().value, 99.0);

    const QVector<QPointF> display = processor->processedSnapshot(config.name);
    QVERIFY(display.size() <= 10);
    QVERIFY(!display.isEmpty());
    QCOMPARE(display.last().y(), 99.0);

    manager.stopMonitoring();
    QVERIFY(manager.removeChannel(config.name));
}

void MonitorWidgetUiTest::hiddenViewDefersRenderingUntilShown()
{
    MonitorDataProcessor processor;
    MonitorChartView view;
    view.setDataProcessor(&processor);
    view.addChannel(QStringLiteral("deferred"));
    view.resize(400, 300);
    view.show();
    QTest::qWait(50);
    view.hide();

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    processor.appendPoint(QStringLiteral("deferred"), QPointF(now, 1));
    processor.appendPoint(QStringLiteral("deferred"), QPointF(now + 1, 2));
    QVERIFY(QMetaObject::invokeMethod(&view, "onRenderTimer", Qt::DirectConnection));

    auto seriesCount = [&view]() {
        auto* chart = view.chartWidget()->findChild<QtCharts::QChartView*>();
        if (!chart) return -1;
        for (auto* series : chart->chart()->series()) {
            if (series->name() == QStringLiteral("deferred")) {
                auto* line = qobject_cast<QtCharts::QLineSeries*>(series);
                return line ? line->count() : 0;
            }
        }
        return 0;
    };
    QCOMPARE(seriesCount(), 0);

    view.show();
    QTest::qWait(20);
    QVERIFY(QMetaObject::invokeMethod(&view, "onRenderTimer", Qt::DirectConnection));
    QCOMPARE(seriesCount(), 2);
}

void MonitorWidgetUiTest::sustainedRecordingKeepsBoundedCaches()
{
    auto& manager = Monitor::MonitorManager::instance();
    manager.stopMonitoring();
    QStringList channels;
    for (int i = 0; i < 4; ++i) {
        Monitor::ChannelConfig config;
        config.name = QStringLiteral("load.ch%1").arg(i);
        config.displayName = config.name;
        config.unit = QStringLiteral("V");
        config.maxSamples = 500;
        QVERIFY(manager.registerChannel(config));
        channels << config.name;
    }

    MonitorWidget widget;
    auto* processor = widget.dataProcessor();
    QVERIFY(processor);
    processor->setMaxDisplaySamples(50);
    processor->setRingBufferCapacity(1000);
    manager.startMonitoring();

    const int samplesPerChannel = 2000;
    QElapsedTimer timer;
    timer.start();
    for (int n = 0; n < samplesPerChannel; ++n) {
        for (const QString& channel : channels) {
            manager.recordSample(channel, n, QStringLiteral("V"));
        }
    }
    const qint64 elapsedMs = timer.elapsed();

    for (const QString& channel : channels) {
        const QList<Monitor::Sample> history = manager.history(channel, samplesPerChannel);
        QCOMPARE(history.size(), 500);
        QCOMPARE(history.last().value, double(samplesPerChannel - 1));
        const QVector<QPointF> display = processor->processedSnapshot(channel);
        QVERIFY(display.size() <= 50);
        QVERIFY(!display.isEmpty());
        QCOMPARE(display.last().y(), double(samplesPerChannel - 1));
    }
    qInfo() << "load-config channels=" << channels.size()
            << "samplesPerChannel=" << samplesPerChannel
            << "channelMaxSamples=500 displayMax=50 ringCapacity=1000"
            << "elapsedMs=" << elapsedMs;

    manager.stopMonitoring();
    for (const QString& channel : channels) QVERIFY(manager.removeChannel(channel));
}

void MonitorWidgetUiTest::viewDestructionKeepsMonitoringAndProcessor()
{
    auto& manager = Monitor::MonitorManager::instance();
    manager.stopMonitoring();
    auto* first = new MonitorWidget;
    auto* processor = first->dataProcessor();
    QVERIFY(processor);
    manager.startMonitoring();
    delete first;
    QVERIFY(manager.isMonitoring());
    QCOMPARE(manager.dataProcessor(), processor);
    MonitorWidget second;
    QCOMPARE(second.dataProcessor(), processor);
    QVERIFY(manager.isMonitoring());
    manager.stopMonitoring();
}

void MonitorWidgetUiTest::initTestCase()
{
}

void MonitorWidgetUiTest::cleanupTestCase()
{
}

void MonitorWidgetUiTest::channelFilterAndSelectionPreservationTest()
{
    auto& manager = Monitor::MonitorManager::instance();
    for (const QString& name : manager.channelNames()) {
        manager.removeChannel(name);
    }
    manager.clearAllData();

    // 注册 4 个测试通道
    Monitor::ChannelConfig c1; c1.name = QStringLiteral("ch1_v"); c1.displayName = QStringLiteral("CH1_Voltage");
    Monitor::ChannelConfig c2; c2.name = QStringLiteral("ch2_i"); c2.displayName = QStringLiteral("CH2_Current");
    Monitor::ChannelConfig c3; c3.name = QStringLiteral("motor_spd"); c3.displayName = QStringLiteral("Motor_Speed");
    Monitor::ChannelConfig c4; c4.name = QStringLiteral("temp_s"); c4.displayName = QStringLiteral("Temp_Sensor");
    manager.registerChannel(c1);
    manager.registerChannel(c2);
    manager.registerChannel(c3);
    manager.registerChannel(c4);

    MonitorWidget widget;
    widget.resize(800, 600);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    auto* searchEdit = widget.channelSearchEdit();
    auto* onlySelectedBox = widget.onlySelectedCheckBox();
    auto* listWidget = widget.findChild<QListWidget*>();

    QVERIFY(searchEdit != nullptr);
    QVERIFY(onlySelectedBox != nullptr);
    QVERIFY(listWidget != nullptr);
    QCOMPARE(listWidget->count(), 4);

    // 默认所有通道选中
    widget.selectAllChannels();
    QCOMPARE(widget.selectedChannels().size(), 4);

    // 1. 搜索过滤测试：输入 "motor"
    searchEdit->setText(QStringLiteral("motor"));
    QTest::qWait(20);

    // 验证只有 motor_spd 可见，其他隐藏
    int visibleCount = 0;
    for (int i = 0; i < listWidget->count(); ++i) {
        auto* item = listWidget->item(i);
        if (!item->isHidden()) {
            visibleCount++;
            QVERIFY(item->text().contains(QStringLiteral("Motor"), Qt::CaseInsensitive));
        }
    }
    QCOMPARE(visibleCount, 1);

    // 关键契约：过滤绝不改变选中集合！所有 4 个通道依然保持选中
    QCOMPARE(widget.selectedChannels().size(), 4);

    // 清空搜索框
    searchEdit->clear();
    QTest::qWait(20);
    visibleCount = 0;
    for (int i = 0; i < listWidget->count(); ++i) {
        if (!listWidget->item(i)->isHidden()) {
            visibleCount++;
        }
    }
    QCOMPARE(visibleCount, 4);

    // 2. “仅显示已选”过滤测试
    // 取消选中 ch1_v
    for (int i = 0; i < listWidget->count(); ++i) {
        auto* item = listWidget->item(i);
        if (item->data(Qt::UserRole).toString() == QStringLiteral("ch1_v")) {
            item->setCheckState(Qt::Unchecked);
            break;
        }
    }
    QCOMPARE(widget.selectedChannels().size(), 3);

    // 勾选“仅显示已选”
    onlySelectedBox->setChecked(true);
    QTest::qWait(20);

    // 验证只有已选中的 3 个通道可见，未选中的 ch1_v 隐藏
    visibleCount = 0;
    for (int i = 0; i < listWidget->count(); ++i) {
        auto* item = listWidget->item(i);
        if (!item->isHidden()) {
            visibleCount++;
            QCOMPARE(item->checkState(), Qt::Checked);
        } else {
            QCOMPARE(item->data(Qt::UserRole).toString(), QStringLiteral("ch1_v"));
        }
    }
    QCOMPARE(visibleCount, 3);
    QCOMPARE(widget.selectedChannels().size(), 3);

    // 恢复
    onlySelectedBox->setChecked(false);
    for (const QString& name : manager.channelNames()) {
        manager.removeChannel(name);
    }
    manager.clearAllData();
}

void MonitorWidgetUiTest::detailsPanelCollapseAndAlarmBadgeTest()
{
    auto& manager = Monitor::MonitorManager::instance();
    for (const QString& name : manager.channelNames()) {
        manager.removeChannel(name);
    }
    manager.clearAllData();

    MonitorWidget widget;
    widget.resize(800, 600);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    auto* toggleBtn = widget.toggleDetailsButton();
    auto* rightTab = widget.rightTabWidget();

    QVERIFY(toggleBtn != nullptr);
    QVERIFY(rightTab != nullptr);

    // 契约：右侧详情默认收起
    QVERIFY(!widget.isDetailsPanelVisible());
    QVERIFY(!rightTab->isVisible());
    QVERIFY(toggleBtn->text().contains(QStringLiteral("0")));

    // 点击展开
    toggleBtn->click();
    QTest::qWait(20);
    QVERIFY(widget.isDetailsPanelVisible());
    QVERIFY(rightTab->isVisible());

    // 再次点击折叠
    toggleBtn->click();
    QTest::qWait(20);
    QVERIFY(!widget.isDetailsPanelVisible());
    QVERIFY(!rightTab->isVisible());

    // 触发告警，验证即便折叠状态下，告警计数也在按钮上常驻可见
    Monitor::ChannelConfig testCh;
    testCh.name = QStringLiteral("test_ch");
    testCh.displayName = QStringLiteral("Test Channel");
    manager.registerChannel(testCh);
    emit manager.thresholdExceeded(QStringLiteral("test_ch"), 105.0, 100.0);
    QTest::qWait(20);

    QCOMPARE(widget.alarmCount(), 1);
    QVERIFY(toggleBtn->text().contains(QStringLiteral("1")));

    // 调用 showAlarmTab()，详情面板必须自动展开并切换到告警页
    widget.showAlarmTab();
    QTest::qWait(20);
    QVERIFY(widget.isDetailsPanelVisible());
    QVERIFY(rightTab->isVisible());
    QCOMPARE(rightTab->tabText(rightTab->currentIndex()), QStringLiteral("告警"));

    for (const QString& name : manager.channelNames()) {
        manager.removeChannel(name);
    }
    manager.clearAllData();
}

void MonitorWidgetUiTest::clearDisplayActionTest()
{
    MonitorWidget widget;
    widget.resize(800, 600);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    auto* clearBtn = widget.findChild<QPushButton*>(QStringLiteral("MonitorClearDisplayButton"));
    QVERIFY(clearBtn != nullptr);
    QCOMPARE(clearBtn->text(), QStringLiteral("清空显示"));
    QVERIFY(clearBtn->toolTip().contains(QStringLiteral("不影响已保存的数据库历史记录")) ||
             clearBtn->toolTip().contains(QStringLiteral("不影响数据库历史记录")));
}

void MonitorWidgetUiTest::monitoringStartStopWordingTest()
{
    MonitorWidget widget;
    widget.resize(800, 600);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    auto* startStopBtn = widget.findChild<QPushButton*>(QStringLiteral("MonitorStartStopButton"));
    QVERIFY(startStopBtn != nullptr);
    QCOMPARE(startStopBtn->text(), QStringLiteral("开始监控"));

    widget.syncMonitoringState(true);
    QCOMPARE(startStopBtn->text(), QStringLiteral("停止监控"));
    QVERIFY(startStopBtn->toolTip().contains(QStringLiteral("不影响控制器运行")));

    widget.syncMonitoringState(false);
    QCOMPARE(startStopBtn->text(), QStringLiteral("开始监控"));
}

int runMonitorWidgetUiTest(int argc, char* argv[])
{
    QApplication app(argc, argv);
    MonitorWidgetUiTest test;
    return QTest::qExec(&test, argc, argv);
}

int main(int argc, char* argv[])
{
    return runMonitorWidgetUiTest(argc, argv);
}

#include "monitor_widget_ui_test.moc"
