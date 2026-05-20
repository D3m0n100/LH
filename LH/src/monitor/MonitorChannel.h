// 文件：src/monitor/MonitorChannel.h
// 监控通道类

#ifndef MONITOR_CHANNEL_H
#define MONITOR_CHANNEL_H

#include <QObject>
#include <QList>
#include <QDateTime>
#include <QMutex>
#include <deque>

#include "MonitorTypes.h"

namespace Monitor {

/**
 * @brief MonitorChannel
 * 
 * 单个监控通道，负责：
 * - 存储采样历史数据
 * - 阈值检测和告警
 * - 数据淘汰
 */
class MonitorChannel : public QObject
{
    Q_OBJECT

public:
    explicit MonitorChannel(const ChannelConfig& config, QObject* parent = nullptr);
    ~MonitorChannel() override;

    // ===== 配置 =====
    QString name() const { return m_config.name; }
    QString displayName() const { return m_config.displayName; }
    QString unit() const { return m_config.unit; }
    ChannelConfig config() const { return m_config; }
    void updateConfig(const ChannelConfig& config);

    // ===== 数据操作 =====
    void appendSample(const Sample& sample);
    void appendSamples(const QList<Sample>& samples);
    Sample latestSample() const;
    QList<Sample> history(int count) const;
    QList<Sample> history(const QDateTime& start, const QDateTime& end) const;
    int sampleCount() const;
    void clear();
    void purgeOlderThan(const QDateTime& cutoff);

    // ===== 阈值 =====
    void addThreshold(const Threshold& threshold);
    void removeThreshold(const QString& name);
    QList<Threshold> thresholds() const { return m_config.thresholds; }

signals:
    void sampleAdded(const Sample& sample);
    void thresholdExceeded(const QString& channelName, double value,
                           const QString& unit, double thresholdValue,
                           ThresholdMode mode);

private:
    void checkThresholds(const Sample& sample);
    void enforceMaxSamples();

private:
    ChannelConfig m_config;
    std::deque<Sample> m_samples;
    mutable QMutex m_mutex;
};

} // namespace Monitor

#endif // MONITOR_CHANNEL_H
