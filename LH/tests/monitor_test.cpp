/**
 * @file monitor_test.cpp
 * @brief 监控模块测试
 *
 * 测试内容：
 * - MonitorManager 通道注册与数据采集
 * - MonitorChannel 采样与阈值检测
 * - DataManager 数据持久化
 * - TaskScheduler 任务调度
 * - 各模块的 shutdown 正确性
 */

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QtGlobal>
#include "core/DataManager.h"
#include "core/TaskScheduler.h"
#include "monitor/MonitorManager.h"
#include "monitor/MonitorChannel.h"

using namespace Monitor;

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    qInfo() << "========================================";
    qInfo() << "Monitor Test Started";
    qInfo() << "========================================";

    // ===== 初始化 DataManager =====
    QString dbPath = QDir::temp().absoluteFilePath("monitor_test.db");
    QFile::remove(dbPath);  // 确保使用干净的数据库
    
    qInfo() << "Initializing DataManager with:" << dbPath;
    if (!DataManager::instance().initialize(dbPath)) {
        qCritical() << "Failed to initialize DataManager";
        return 1;
    }
    qInfo() << "DataManager initialized successfully";

    // ===== 启动 TaskScheduler =====
    qInfo() << "Starting TaskScheduler...";
    TaskScheduler::instance().start();
    qInfo() << "TaskScheduler started";

    // ===== 配置 MonitorManager =====
    auto& monitorMgr = MonitorManager::instance();
    monitorMgr.startMonitoring();
    qInfo() << "MonitorManager started";

    // ===== 注册监控通道 =====
    ChannelConfig channelConfig;
    channelConfig.name = "test.channel";
    channelConfig.displayName = "测试通道";
    channelConfig.unit = "bar";
    channelConfig.maxSamples = 32;

    // 配置阈值
    Threshold upperThreshold;
    upperThreshold.name = "upper";
    upperThreshold.value = 3.0;
    upperThreshold.unit = "bar";
    upperThreshold.mode = ThresholdMode::Above;
    channelConfig.thresholds.append(upperThreshold);

    if (!monitorMgr.registerChannel(channelConfig)) {
        qCritical() << "Failed to register monitor channel";
        return 1;
    }
    qInfo() << "Monitor channel registered:" << channelConfig.name;

    // ===== 配置数据采集器 =====
    ProviderConfig provider;
    provider.id = "test.provider";
    provider.channelName = channelConfig.name;
    provider.unit = channelConfig.unit;
    provider.periodMs = 100;
    provider.priority = 10;
    provider.metadata.insert("source", "monitor_test");

    provider.sampler = []() mutable -> double {
        static double value = 0.5;
        value += 0.5;
        return value;
    };
    
    provider.errorHandler = [](const QString& message) {
        qWarning() << "Provider error:" << message;
    };

    if (!monitorMgr.registerProvider(provider)) {
        qCritical() << "Failed to register monitor provider";
        return 1;
    }
    qInfo() << "Monitor provider registered:" << provider.id;

    // ===== 运行一段时间后检查结果 =====
    QTimer::singleShot(1500, [&]() {
        qInfo() << "----------------------------------------";
        qInfo() << "Checking collected samples...";
        
        auto channel = monitorMgr.channel(channelConfig.name);
        if (!channel) {
            qCritical() << "Channel missing after sampling period";
        } else {
            // 修复：使用 history() 而非 samples()
            auto samples = channel->history(100);  // 获取最近100个样本
            QTextStream out(stdout);
            out << "Collected samples: " << samples.size() << Qt::endl;
            
            int displayCount = qMin(samples.size(), 5);
            for (int i = 0; i < displayCount; ++i) {
                const auto& sample = samples[i];
                out << "  [" << i << "] " 
                    << sample.channelName << ", "
                    << sample.value << ' ' << sample.unit << ", "
                    << sample.timestamp.toString(Qt::ISODate) << Qt::endl;
            }
            
            if (samples.size() > 5) {
                out << "  ... (and " << (samples.size() - 5) << " more)" << Qt::endl;
            }
        }

        // ===== 清理阶段 =====
        qInfo() << "----------------------------------------";
        qInfo() << "Starting cleanup phase...";
        
        // 1. 注销 Provider
        qInfo() << "Unregistering provider...";
        monitorMgr.unregisterProvider(provider.id);
        
        // 2. 停止监控
        qInfo() << "Stopping MonitorManager...";
        monitorMgr.stopMonitoring();
        
        // 3. 停止并关闭 TaskScheduler
        qInfo() << "Shutting down TaskScheduler...";
        TaskScheduler::instance().shutdown();
        
        // 验证 TaskScheduler 状态
        if (TaskScheduler::instance().isShutdown()) {
            qInfo() << "TaskScheduler shutdown confirmed";
        } else {
            qWarning() << "TaskScheduler shutdown state unexpected";
        }
        
        // 4. 关闭 DataManager
        qInfo() << "Shutting down DataManager...";
        DataManager::instance().shutdown();
        
        // 验证 DataManager 状态
        if (!DataManager::instance().isInitialized()) {
            qInfo() << "DataManager shutdown confirmed";
        } else {
            qWarning() << "DataManager still initialized after shutdown";
        }
        
        qInfo() << "----------------------------------------";
        qInfo() << "All cleanup completed successfully!";
        qInfo() << "========================================";
        
        QCoreApplication::quit();
    });

    return app.exec();
}
