// 文件：src/monitor/MonitorChannel.cpp
// 监控通道类实现

#include "MonitorChannel.h"
#include <QMutexLocker>
#include <algorithm>

namespace Monitor {

MonitorChannel::MonitorChannel(const ChannelConfig& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
{
    if (m_config.maxSamples <= 0) {
        m_config.maxSamples = Limits::DEFAULT_MAX_SAMPLES;
    }
}

MonitorChannel::~MonitorChannel() = default;

void MonitorChannel::updateConfig(const ChannelConfig& config)
{
    QMutexLocker locker(&m_mutex);
    m_config = config;
    if (m_config.maxSamples <= 0) {
        m_config.maxSamples = Limits::DEFAULT_MAX_SAMPLES;
    }
    enforceMaxSamples();
}

void MonitorChannel::appendSample(const Sample& sample)
{
    {
        QMutexLocker locker(&m_mutex);
        m_samples.push_back(sample);
        enforceMaxSamples();
    }
    
    checkThresholds(sample);
    emit sampleAdded(sample);
}

void MonitorChannel::appendSamples(const QList<Sample>& samples)
{
    if (samples.isEmpty()) {
        return;
    }
    
    {
        QMutexLocker locker(&m_mutex);
        for (const Sample& sample : samples) {
            m_samples.push_back(sample);
        }
        enforceMaxSamples();
    }
    
    // 检查最后一个样本的阈值
    checkThresholds(samples.last());
    emit sampleAdded(samples.last());
}

Sample MonitorChannel::latestSample() const
{
    QMutexLocker locker(&m_mutex);
    if (m_samples.empty()) {
        return Sample();
    }
    return m_samples.back();
}

QList<Sample> MonitorChannel::history(int count) const
{
    QMutexLocker locker(&m_mutex);
    
    QList<Sample> result;
    int startIdx = std::max(0, static_cast<int>(m_samples.size()) - count);
    for (size_t i = startIdx; i < m_samples.size(); ++i) {
        result.append(m_samples[i]);
    }
    return result;
}

QList<Sample> MonitorChannel::history(const QDateTime& start, 
                                       const QDateTime& end) const
{
    QMutexLocker locker(&m_mutex);
    
    QList<Sample> result;
    for (const Sample& sample : m_samples) {
        if (sample.timestamp >= start && sample.timestamp <= end) {
            result.append(sample);
        }
    }
    return result;
}

int MonitorChannel::sampleCount() const
{
    QMutexLocker locker(&m_mutex);
    return static_cast<int>(m_samples.size());
}

void MonitorChannel::clear()
{
    QMutexLocker locker(&m_mutex);
    m_samples.clear();
}

void MonitorChannel::purgeOlderThan(const QDateTime& cutoff)
{
    QMutexLocker locker(&m_mutex);
    
    while (!m_samples.empty() && m_samples.front().timestamp < cutoff) {
        m_samples.pop_front();
    }
}

void MonitorChannel::addThreshold(const Threshold& threshold)
{
    QMutexLocker locker(&m_mutex);
    
    // 移除同名阈值
    m_config.thresholds.erase(
        std::remove_if(m_config.thresholds.begin(), m_config.thresholds.end(),
                       [&](const Threshold& t) { return t.name == threshold.name; }),
        m_config.thresholds.end());
    
    m_config.thresholds.append(threshold);
}

void MonitorChannel::removeThreshold(const QString& name)
{
    QMutexLocker locker(&m_mutex);
    
    m_config.thresholds.erase(
        std::remove_if(m_config.thresholds.begin(), m_config.thresholds.end(),
                       [&](const Threshold& t) { return t.name == name; }),
        m_config.thresholds.end());
}

void MonitorChannel::checkThresholds(const Sample& sample)
{
    for (const Threshold& threshold : m_config.thresholds) {
        if (!threshold.enabled) {
            continue;
        }
        
        bool exceeded = false;
        if (threshold.mode == ThresholdMode::Above) {
            exceeded = sample.value > threshold.value;
        } else {
            exceeded = sample.value < threshold.value;
        }
        
        if (exceeded) {
            emit thresholdExceeded(m_config.name, sample.value, 
                                   sample.unit, threshold.value, threshold.mode);
        }
    }
}

void MonitorChannel::enforceMaxSamples()
{
    while (static_cast<int>(m_samples.size()) > m_config.maxSamples) {
        m_samples.pop_front();
    }
}

} // namespace Monitor
