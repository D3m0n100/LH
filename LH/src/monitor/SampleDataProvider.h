// 文件：src/monitor/SampleDataProvider.h
// 示例数据提供器 - 用于演示和测试

#ifndef SAMPLE_DATA_PROVIDER_H
#define SAMPLE_DATA_PROVIDER_H

#include <QObject>
#include <QTimer>
#include <random>

namespace Monitor {
class MonitorManager;
}

/**
 * @brief SampleDataProvider
 * 
 * 示例数据提供器，生成模拟的伺服阀监控数据：
 * - 系统压力
 * - 系统流量
 * - 系统温度
 * - 阀芯位置
 * 
 * 【使用场景】
 * - 演示监控功能
 * - 单元测试
 * - 性能测试
 */
class SampleDataProvider : public QObject
{
    Q_OBJECT

public:
    explicit SampleDataProvider(QObject* parent = nullptr);
    ~SampleDataProvider() override;

    void start();
    void stop();
    bool isRunning() const { return m_running; }
    
    // 配置基准值
    void setBasePressure(double value) { m_basePressure = value; }
    void setBaseFlow(double value) { m_baseFlow = value; }
    void setBaseTemperature(double value) { m_baseTemperature = value; }

private slots:
    void generatePressureData();
    void generateFlowData();
    void generateTemperatureData();
    void generatePositionData();

private:
    double generateRandomValue(double base, double variation);
    void registerChannels();

private:
    Monitor::MonitorManager& m_monitorManager;
    
    QTimer* m_pressureTimer;
    QTimer* m_flowTimer;
    QTimer* m_temperatureTimer;
    QTimer* m_positionTimer;
    
    bool m_running;
    std::mt19937 m_randomEngine;
    std::uniform_real_distribution<double> m_distribution;
    
    // 基准值
    double m_basePressure;
    double m_baseFlow;
    double m_baseTemperature;
    double m_basePosition;
};

#endif // SAMPLE_DATA_PROVIDER_H
