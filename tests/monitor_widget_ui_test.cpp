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
};

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
