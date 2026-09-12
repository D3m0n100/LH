// 文件：src/monitor/SampleDataProvider.cpp
// 示例数据提供器实现

#include "SampleDataProvider.h"
#include "MonitorManager.h"
#include "MonitorTypes.h"

#include <QDateTime>
#include <cmath>

SampleDataProvider::SampleDataProvider(QObject* parent)
    : QObject(parent)
    , m_monitorManager(Monitor::MonitorManager::instance())
    , m_pressureTimer(new QTimer(this))
    , m_flowTimer(new QTimer(this))
    , m_temperatureTimer(new QTimer(this))
    , m_positionTimer(new QTimer(this))
    , m_running(false)
    , m_randomEngine(std::random_device{}())
    , m_distribution(-1.0, 1.0)
    , m_basePressure(150.0)
    , m_baseFlow(50.0)
    , m_baseTemperature(45.0)
    , m_basePosition(0.0)
{
    connect(m_pressureTimer, &QTimer::timeout, 
            this, &SampleDataProvider::generatePressureData);
    connect(m_flowTimer, &QTimer::timeout, 
            this, &SampleDataProvider::generateFlowData);
    connect(m_temperatureTimer, &QTimer::timeout, 
            this, &SampleDataProvider::generateTemperatureData);
    connect(m_positionTimer, &QTimer::timeout, 
            this, &SampleDataProvider::generatePositionData);
    
    // 不同的采样频率
    m_pressureTimer->setInterval(500);
    m_flowTimer->setInterval(1000);
    m_temperatureTimer->setInterval(2000);
    m_positionTimer->setInterval(100);
}

SampleDataProvider::~SampleDataProvider()
{
    stop();
}

void SampleDataProvider::start()
{
    if (m_running) {
        return;
    }
    
    registerChannels();
    
    m_pressureTimer->start();
    m_flowTimer->start();
    m_temperatureTimer->start();
    m_positionTimer->start();
    
    m_running = true;
}

void SampleDataProvider::stop()
{
    if (!m_running) {
        return;
    }
    
    m_pressureTimer->stop();
    m_flowTimer->stop();
    m_temperatureTimer->stop();
    m_positionTimer->stop();
    
    m_running = false;
}

void SampleDataProvider::registerChannels()
{
    // 压力通道
    Monitor::ChannelConfig pressureConfig;
    pressureConfig.name = "系统压力";
    pressureConfig.displayName = "系统压力";
    pressureConfig.unit = "bar";
    pressureConfig.maxSamples = 2000;
    m_monitorManager.registerChannel(pressureConfig);
    
    // 流量通道
    Monitor::ChannelConfig flowConfig;
    flowConfig.name = "系统流量";
    flowConfig.displayName = "系统流量";
    flowConfig.unit = "L/min";
    flowConfig.maxSamples = 1000;
    m_monitorManager.registerChannel(flowConfig);
    
    // 温度通道
    Monitor::ChannelConfig tempConfig;
    tempConfig.name = "系统温度";
    tempConfig.displayName = "系统温度";
    tempConfig.unit = "°C";
    tempConfig.maxSamples = 500;
    m_monitorManager.registerChannel(tempConfig);
    
    // 位置通道
    Monitor::ChannelConfig posConfig;
    posConfig.name = "阀芯位置";
    posConfig.displayName = "阀芯位置";
    posConfig.unit = "%";
    posConfig.maxSamples = 5000;
    m_monitorManager.registerChannel(posConfig);
}

void SampleDataProvider::generatePressureData()
{
    double value = generateRandomValue(m_basePressure, 10.0);
    m_monitorManager.recordSample("系统压力", value, "bar");
}

void SampleDataProvider::generateFlowData()
{
    double value = generateRandomValue(m_baseFlow, 5.0);
    m_monitorManager.recordSample("系统流量", value, "L/min");
}

void SampleDataProvider::generateTemperatureData()
{
    double value = generateRandomValue(m_baseTemperature, 2.0);
    m_monitorManager.recordSample("系统温度", value, "°C");
}

void SampleDataProvider::generatePositionData()
{
    // 模拟正弦波动的阀芯位置
    static double t = 0.0;
    t += 0.1;
    
    double baseValue = 50.0 + 40.0 * std::sin(t * 0.5);
    double value = generateRandomValue(baseValue, 2.0);
    
    // 限制在 0-100%
    value = std::max(0.0, std::min(100.0, value));
    
    m_monitorManager.recordSample("阀芯位置", value, "%");
}

double SampleDataProvider::generateRandomValue(double base, double variation)
{
    return base + m_distribution(m_randomEngine) * variation;
}
