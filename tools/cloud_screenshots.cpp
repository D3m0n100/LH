// Cloud-only capture harness; all displayed samples are simulated.
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QLabel>
#include <QPixmap>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QtCharts/QXYSeries>
#include "designer/DeviceWorkspaceWidget.h"
#include "designer/ui/ThemeManager.h"
#include "monitor/MonitorWidget.h"
#include "monitor/MonitorChartView.h"
#include "monitor/ChartWidget.h"
#include "monitor/MonitorManager.h"
#include "monitor/SampleDataProvider.h"
#include "core/DataManager.h"

static bool capture(QWidget* content, const QString& title, const QString& file) {
    QWidget frame;
    auto* layout = new QVBoxLayout(&frame);
    auto* label = new QLabel(title, &frame);
    label->setWordWrap(true);
    layout->addWidget(label);
    layout->addWidget(content);
    frame.resize(1600, 960);
    frame.show();
    QApplication::processEvents();
    const bool saved = frame.grab().save(file);
    layout->removeWidget(content);
    content->setParent(nullptr);
    return saved;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("LH-cloud-screenshots");
    if (app.arguments().size() != 2) return 1;
    QDir output(app.arguments().at(1));
    if (!output.exists() && !QDir().mkpath(output.absolutePath())) return 2;
    QTemporaryDir temp;
    if (!temp.isValid()) return 3;
    auto& db = DataManager::instance();
    if (!db.initialize(temp.filePath("demo.sqlite"))) return 4;
    auto& manager = Monitor::MonitorManager::instance();
    manager.setDatabaseLoggingEnabled(false);
    ThemeManager::applyTheme(&app, ThemeManager::ThemeMode::Light);
    int result = 0;
    {
        DeviceWorkspaceWidget device;
        device.setTargetInfo("F2812", "Example configuration", "COM7 (example; not connected)",
                             "F2812", "download_profile.json");
        device.setConnectionStatus(false, QStringLiteral("未连接 / 无硬件"));
        device.setExpertDiagnosticVisible(true);
        if (!capture(&device, QStringLiteral("LH 原生设备与通信界面｜示例配置，未连接真实硬件"),
                     output.filePath("01-communication.png"))) result = 5;

        MonitorWidget monitor;
        SampleDataProvider provider;
        monitor.setSampleDataProvider(&provider);
        provider.start();
        monitor.selectAllChannels();
        monitor.setDetailsPanelVisible(true);
        monitor.setTimeWindow(30000);
        monitor.show();
        QEventLoop sampling;
        QTimer::singleShot(15000, &sampling, &QEventLoop::quit);
        sampling.exec();
        provider.stop();
        for (auto* series : monitor.chartView()->chartWidget()->chart()->series())
            series->setUseOpenGL(false);
        monitor.chartView()->consumeAndRender();
        monitor.chartView()->forceRefresh();
        int sampleCount = 0;
        QList<QVariantMap> records;
        for (const auto& name : manager.channelNames()) {
            const auto samples = manager.history(name, 5000);
            sampleCount += samples.size();
            for (const auto& sample : samples) {
                QVariantMap record;
                record["varName"] = name;
                record["value"] = sample.value;
                record["unit"] = sample.unit;
                record["timestamp"] = sample.timestamp;
                records.append(record);
            }
        }
        if (manager.channelNames().size() < 4 || sampleCount < 100) result = 6;
        if (!capture(&monitor, QStringLiteral("LH 原生多通道监控｜模拟数据：压力、流量、温度、阀芯位置"),
                     output.filePath("02-multichannel-monitor.png"))) result = 7;
        if (!db.logRuntimeDataBatch(records).success) result = 8;
        QFile csv(output.filePath("03-history-query.csv"));
        if (!csv.open(QIODevice::WriteOnly | QIODevice::Text)) result = 9;
        else {
            QTextStream stream(&csv);
            stream.setCodec("UTF-8");
            stream << "timestamp,variable,value,unit\n";
            int count = 0;
            for (const auto& name : db.getAllVariableNames()) {
                const auto rows = db.queryHistory(name, QDateTime::currentDateTime().addSecs(-120),
                                                  QDateTime::currentDateTime().addSecs(1));
                for (const auto& row : rows) {
                    stream << row.timestamp.toString(Qt::ISODateWithMs) << "," << name << ","
                           << row.value << "," << row.unit << "\n";
                    ++count;
                }
            }
            if (count != sampleCount) result = 10;
        }
        monitor.hide();
        monitor.setSampleDataProvider(nullptr);
        manager.setDataProcessor(nullptr);
    }
    manager.shutdown();
    db.shutdown();
    QFile notes(output.filePath("README.txt"));
    if (!notes.open(QIODevice::WriteOnly | QIODevice::Text)) return 11;
    notes.write("Native Qt widget captures. All values are simulated; no hardware connection.\n"
                "01: actual DeviceWorkspaceWidget, disconnected example configuration.\n"
                "02: actual MonitorWidget using the existing SampleDataProvider.\n"
                "03: actual SQLite history query results (CSV). The repository has no standalone history query page, so no third screenshot is fabricated.\n");
    return result;
}
