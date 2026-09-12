// 文件：src/monitor/MonitorDataProcessor.cpp
// 监控数据处理器实现（性能优化版本）

#include "MonitorDataProcessor.h"
#include "MonitorChannel.h"
#include "MonitorTypes.h"

#include <QDebug>
#include <QReadLocker>
#include <QWriteLocker>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <limits>
#include <vector>

MonitorDataProcessor::MonitorDataProcessor(QObject* parent)
    : QObject(parent)
{
    // 初始化配置
    m_config.maxDisplaySamples = Monitor::Limits::DEFAULT_DISPLAY_SAMPLES;
    m_config.timeWindowMs = Monitor::Limits::DEFAULT_TIME_WINDOW_MS;
    m_config.ringBufferCapacity = Monitor::Limits::DEFAULT_RING_BUFFER_CAPACITY;
    m_config.enableIncrementalMode = true;
    m_config.batchProcessThreshold = Monitor::Limits::BATCH_PROCESS_THRESHOLD;
    m_config.maxDeltaBufferSize = Monitor::Limits::DEFAULT_RING_BUFFER_CAPACITY;
    m_config.enableAdaptiveDownsample = true;
    m_config.adaptiveDownsampleThreshold = Monitor::Limits::ADAPTIVE_DOWNSAMPLE_THRESHOLD;
    
    m_statsTimer.start();
}

MonitorDataProcessor::~MonitorDataProcessor() = default;

// ============================================================================
// 配置接口
// ============================================================================

DataProcessorConfig MonitorDataProcessor::config() const
{
    return snapshotConfig();
}

void MonitorDataProcessor::setConfig(const DataProcessorConfig& config)
{
    updateConfig([config](DataProcessorConfig& current) {
        current = config;
    });
}

void MonitorDataProcessor::setTimeWindow(qint64 windowMs)
{
    updateConfig([windowMs](DataProcessorConfig& config) {
        config.timeWindowMs = windowMs;
    });
}

qint64 MonitorDataProcessor::timeWindow() const
{
    return snapshotConfig().timeWindowMs;
}

void MonitorDataProcessor::setMaxDisplaySamples(int maxSamples)
{
    updateConfig([maxSamples](DataProcessorConfig& config) {
        config.maxDisplaySamples = maxSamples;
    });
}

int MonitorDataProcessor::maxDisplaySamples() const
{
    return snapshotConfig().maxDisplaySamples;
}

void MonitorDataProcessor::setRingBufferCapacity(int capacity)
{
    updateConfig([capacity](DataProcessorConfig& config) {
        config.ringBufferCapacity = capacity;
    });
}

int MonitorDataProcessor::ringBufferCapacity() const
{
    return snapshotConfig().ringBufferCapacity;
}

void MonitorDataProcessor::setClampRange(std::optional<double> minVal, 
                                          std::optional<double> maxVal)
{
    updateConfig([minVal, maxVal](DataProcessorConfig& config) {
        config.minClamp = minVal;
        config.maxClamp = maxVal;
    });
}

void MonitorDataProcessor::setSmoothing(bool enabled, int windowSize)
{
    updateConfig([enabled, windowSize](DataProcessorConfig& config) {
        config.enableSmoothing = enabled;
        config.smoothingWindow = windowSize;
    });
}

void MonitorDataProcessor::setDownsampling(bool enabled, int factor)
{
    updateConfig([enabled, factor](DataProcessorConfig& config) {
        config.enableDownsampling = enabled;
        config.downsampleFactor = factor;
    });
}

void MonitorDataProcessor::setIncrementalMode(bool enabled)
{
    updateConfig([enabled](DataProcessorConfig& config) {
        config.enableIncrementalMode = enabled;
    });
}

// ============================================================================
// 增量数据接口（核心优化）
// ============================================================================

void MonitorDataProcessor::appendSample(const QString& channelId, 
                                         const Monitor::Sample& sample)
{
    QWriteLocker locker(&m_bufferLock);
    ChannelBuffer& buffer = getOrCreateBuffer(channelId);
    buffer.unit = sample.unit;
    buffer.quality = sample.quality;
    buffer.valueValid = sample.valueValid;
    buffer.lastTimestampMs = sample.timestampMs();
    int deltaCount = 0;
    if (sample.valueValid) {
        const QPointF point = sample.toPoint();
        buffer.ringBuffer.push_back(point);
        buffer.lastValue = point.y();
        if (m_config.enableIncrementalMode) {
            buffer.deltaBuffer.deltaPoints.append(point);
            buffer.deltaBuffer.totalAppended++;
            if (buffer.deltaBuffer.deltaPoints.size() > m_config.maxDeltaBufferSize) {
                buffer.deltaBuffer.deltaPoints.remove(0, buffer.deltaBuffer.deltaPoints.size() - m_config.maxDeltaBufferSize);
            }
            deltaCount = buffer.deltaBuffer.size();
        }
        m_totalAppendCount++;
    }
    const bool emitDelta = deltaCount >= m_config.batchProcessThreshold;
    locker.unlock();
    emit channelQualityUpdated(channelId, sample.quality, sample.valueValid);
    if (emitDelta) emit deltaDataReady(channelId, deltaCount);
}

void MonitorDataProcessor::appendSamples(const QString& channelId, 
                                          const QList<Monitor::Sample>& samples)
{
    if (samples.isEmpty()) {
        return;
    }
    
    QVector<QPointF> points;
    points.reserve(samples.size());
    for (const auto& sample : samples) {
        if (sample.valueValid) {
            points.append(sample.toPoint());
        }
    }

    const Monitor::Sample& lastSample = samples.last();
    QWriteLocker locker(&m_bufferLock);
    ChannelBuffer& buffer = getOrCreateBuffer(channelId);
    buffer.unit = lastSample.unit;
    buffer.quality = lastSample.quality;
    buffer.valueValid = lastSample.valueValid;
    buffer.lastTimestampMs = lastSample.timestampMs();
    if (!points.isEmpty()) {
        for (const QPointF& point : points) buffer.ringBuffer.push_back(point);
        const QPointF& lastPoint = points.last();
        buffer.lastValue = lastPoint.y();
        if (m_config.enableIncrementalMode) {
            buffer.deltaBuffer.deltaPoints.append(points);
            buffer.deltaBuffer.totalAppended += points.size();
            if (buffer.deltaBuffer.deltaPoints.size() > m_config.maxDeltaBufferSize) {
                buffer.deltaBuffer.deltaPoints.remove(0, buffer.deltaBuffer.deltaPoints.size() - m_config.maxDeltaBufferSize);
            }
        }
        m_totalAppendCount += points.size();
    }
    const int deltaCount = points.size();
    locker.unlock();
    emit channelQualityUpdated(channelId, lastSample.quality, lastSample.valueValid);
    if (deltaCount > 0) emit deltaDataReady(channelId, deltaCount);
}

void MonitorDataProcessor::appendPoint(const QString& channelId, const QPointF& point)
{
    QWriteLocker locker(&m_bufferLock);
    
    ChannelBuffer& buffer = getOrCreateBuffer(channelId);
    
    // 追加到环形缓冲区
    buffer.ringBuffer.push_back(point);
    buffer.lastValue = point.y();
    buffer.lastTimestampMs = static_cast<qint64>(point.x());
    buffer.quality = RuntimePointQuality::Good;
    buffer.valueValid = true;
    
    // 追加到增量缓冲区
    if (m_config.enableIncrementalMode) {
        buffer.deltaBuffer.deltaPoints.append(point);
        buffer.deltaBuffer.totalAppended++;
        
        // 如果增量缓冲区过大，丢弃最老的数据
        if (buffer.deltaBuffer.deltaPoints.size() > m_config.maxDeltaBufferSize) {
            int dropCount = buffer.deltaBuffer.deltaPoints.size() - m_config.maxDeltaBufferSize;
            buffer.deltaBuffer.deltaPoints.remove(0, dropCount);
        }
    }
    
    m_totalAppendCount++;
    
    // 检查是否需要发送信号（批量阈值）
    if (buffer.deltaBuffer.size() >= m_config.batchProcessThreshold) {
        const int deltaCount = buffer.deltaBuffer.size();
        locker.unlock();
        emit deltaDataReady(channelId, deltaCount);
    }
}

void MonitorDataProcessor::appendPoints(const QString& channelId, 
                                          const QVector<QPointF>& points)
{
    if (points.isEmpty()) {
        return;
    }
    
    QWriteLocker locker(&m_bufferLock);
    
    ChannelBuffer& buffer = getOrCreateBuffer(channelId);
    
    // 批量追加到环形缓冲区
    for (const QPointF& point : points) {
        buffer.ringBuffer.push_back(point);
    }
    
    // 更新最新值
    const QPointF& lastPoint = points.last();
    buffer.lastValue = lastPoint.y();
    buffer.lastTimestampMs = static_cast<qint64>(lastPoint.x());
    buffer.quality = RuntimePointQuality::Good;
    buffer.valueValid = true;
    
    // 追加到增量缓冲区
    if (m_config.enableIncrementalMode) {
        buffer.deltaBuffer.deltaPoints.append(points);
        buffer.deltaBuffer.totalAppended += points.size();
        
        // 如果增量缓冲区过大，丢弃最老的数据
        if (buffer.deltaBuffer.deltaPoints.size() > m_config.maxDeltaBufferSize) {
            int dropCount = buffer.deltaBuffer.deltaPoints.size() - m_config.maxDeltaBufferSize;
            buffer.deltaBuffer.deltaPoints.remove(0, dropCount);
        }
    }
    
    m_totalAppendCount += points.size();
    
    locker.unlock();
    emit deltaDataReady(channelId, points.size());
}

QVector<QPointF> MonitorDataProcessor::drainDelta(const QString& channelId)
{
    QWriteLocker locker(&m_bufferLock);
    
    auto it = m_channelBuffers.find(channelId);
    if (it == m_channelBuffers.end()) {
        return {};
    }
    
    ChannelBuffer& buffer = *it->second;
    
    // 获取增量数据
    QVector<QPointF> delta = std::move(buffer.deltaBuffer.deltaPoints);
    buffer.deltaBuffer.deltaPoints.clear();
    buffer.deltaBuffer.lastConsumeTimeMs = QDateTime::currentMSecsSinceEpoch();
    buffer.deltaBuffer.totalConsumed += delta.size();
    
    m_totalConsumeCount += delta.size();
    
    return delta;
}

QMap<QString, QVector<QPointF>> MonitorDataProcessor::drainAllDeltas()
{
    QWriteLocker locker(&m_bufferLock);
    
    QMap<QString, QVector<QPointF>> result;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    
    for (auto& [channelId, bufferPtr] : m_channelBuffers) {
        ChannelBuffer& buffer = *bufferPtr;
        
        if (!buffer.deltaBuffer.deltaPoints.isEmpty()) {
            result[channelId] = std::move(buffer.deltaBuffer.deltaPoints);
            buffer.deltaBuffer.deltaPoints.clear();
            buffer.deltaBuffer.lastConsumeTimeMs = now;
            buffer.deltaBuffer.totalConsumed += result[channelId].size();
            m_totalConsumeCount += result[channelId].size();
        }
    }
    
    return result;
}

bool MonitorDataProcessor::hasPendingDelta(const QString& channelId) const
{
    QReadLocker locker(&m_bufferLock);
    
    auto it = m_channelBuffers.find(channelId);
    if (it == m_channelBuffers.end()) {
        return false;
    }
    
    return !it->second->deltaBuffer.deltaPoints.isEmpty();
}

int MonitorDataProcessor::pendingDeltaCount(const QString& channelId) const
{
    QReadLocker locker(&m_bufferLock);
    
    auto it = m_channelBuffers.find(channelId);
    if (it == m_channelBuffers.end()) {
        return 0;
    }
    
    return it->second->deltaBuffer.deltaPoints.size();
}

// ============================================================================
// 全量数据接口（兼容旧接口）
// ============================================================================

ProcessedChannelData MonitorDataProcessor::processChannel(
    const std::shared_ptr<Monitor::MonitorChannel>& channel)
{
    ProcessedChannelData result;
    
    if (!channel) {
        return result;
    }
    
    result.channelName = channel->name();
    result.lastUpdateTime = QDateTime::currentDateTime();
    quint64 configGeneration = 0;
    const DataProcessorConfig processingConfig = snapshotConfig(&configGeneration);
    
    // 获取最新样本信息
    auto currentSample = channel->latestSample();
    if (!currentSample.channelName.isEmpty()) {
        result.quality = currentSample.quality;
        result.valueValid = currentSample.valueValid;
        if (currentSample.valueValid) {
            result.currentValue = currentSample.value;
        }
        result.unit = currentSample.unit;
    }
    
    // 从环形缓冲区获取数据
    QVector<QPointF> allPoints = getChannelData(result.channelName);
    result.rawSampleCount = allPoints.size();
    
    // 应用时间窗口
    result.seriesPoints = applyTimeWindow(allPoints, processingConfig);
    
    // 应用处理管道
    result.seriesPoints = applyProcessingPipeline(result.seriesPoints, processingConfig);
    
    result.processedCount = result.seriesPoints.size();
    
    // 计算统计值
    result.minValue = calculateMin(result.seriesPoints);
    result.maxValue = calculateMax(result.seriesPoints);
    result.avgValue = calculateAvg(result.seriesPoints);
    
    // 缓存结果
    {
        QReadLocker configLocker(&m_bufferLock);
        if (m_configGeneration == configGeneration) {
            QMutexLocker cacheLocker(&m_cacheMutex);
            m_cachedData[result.channelName] = result;
        }
    }
    
    return result;
}

ProcessedChannelData MonitorDataProcessor::processSamples(
    const QString& channelName,
    const QList<Monitor::Sample>& samples)
{
    ProcessedChannelData result;
    result.channelName = channelName;
    result.lastUpdateTime = QDateTime::currentDateTime();
    result.rawSampleCount = samples.size();
    const DataProcessorConfig processingConfig = snapshotConfig();
    
    if (!samples.isEmpty()) {
        const Monitor::Sample& lastSample = samples.last();
        result.quality = lastSample.quality;
        result.valueValid = lastSample.valueValid;
        result.unit = lastSample.unit;
        for (auto it = samples.crbegin(); it != samples.crend(); ++it) {
            if (it->valueValid) {
                result.currentValue = it->value;
                break;
            }
        }
    }
    
    // 转换为点序列
    result.seriesPoints = samplesToPoints(samples);
    
    // 应用时间窗口
    result.seriesPoints = applyTimeWindow(result.seriesPoints, processingConfig);
    
    // 应用处理管道
    result.seriesPoints = applyProcessingPipeline(result.seriesPoints, processingConfig);
    
    result.processedCount = result.seriesPoints.size();
    
    // 计算统计值
    result.minValue = calculateMin(result.seriesPoints);
    result.maxValue = calculateMax(result.seriesPoints);
    result.avgValue = calculateAvg(result.seriesPoints);
    
    return result;
}

QVector<QPointF> MonitorDataProcessor::getChannelData(const QString& channelId) const
{
    QReadLocker locker(&m_bufferLock);
    
    auto it = m_channelBuffers.find(channelId);
    if (it == m_channelBuffers.end()) {
        return {};
    }
    
    const Monitor::RingBuffer<QPointF>& ringBuffer = it->second->ringBuffer;
    
    QVector<QPointF> result;
    result.reserve(ringBuffer.size());
    for (size_t i = 0; i < ringBuffer.size(); ++i) {
        result.append(ringBuffer[i]);
    }
    
    return result;
}

QVector<QPointF> MonitorDataProcessor::getRecentPoints(const QString& channelId, 
                                                        int count) const
{
    QReadLocker locker(&m_bufferLock);
    
    auto it = m_channelBuffers.find(channelId);
    if (it == m_channelBuffers.end()) {
        return {};
    }
    
    const Monitor::RingBuffer<QPointF>& ringBuffer = it->second->ringBuffer;
    auto vec = ringBuffer.last(static_cast<size_t>(count));
    
    return QVector<QPointF>(vec.begin(), vec.end());
}

QVector<QPointF> MonitorDataProcessor::getPointsInTimeWindow(const QString& channelId) const
{
    const DataProcessorConfig processingConfig = snapshotConfig();
    QVector<QPointF> allPoints = getChannelData(channelId);
    return applyTimeWindow(allPoints, processingConfig);
}

// ============================================================================
// 缓存管理
// ============================================================================

ProcessedChannelData MonitorDataProcessor::getCachedData(const QString& channelName) const
{
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedData.value(channelName);
}

bool MonitorDataProcessor::hasCachedData(const QString& channelName) const
{
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedData.contains(channelName);
}

void MonitorDataProcessor::clearChannelCache(const QString& channelName)
{
    {
        QWriteLocker locker(&m_bufferLock);
        auto it = m_channelBuffers.find(channelName);
        if (it != m_channelBuffers.end()) {
            it->second->ringBuffer.clear();
            it->second->deltaBuffer.clear();
        }
    }
    
    {
        QMutexLocker locker(&m_cacheMutex);
        m_cachedData.remove(channelName);
    }
}

void MonitorDataProcessor::clearAllCache()
{
    {
        QWriteLocker locker(&m_bufferLock);
        for (auto& [key, buffer] : m_channelBuffers) {
            buffer->ringBuffer.clear();
            buffer->deltaBuffer.clear();
        }
    }
    
    {
        QMutexLocker locker(&m_cacheMutex);
        m_cachedData.clear();
    }
}

void MonitorDataProcessor::ensureChannel(const QString& channelId)
{
    QWriteLocker locker(&m_bufferLock);
    getOrCreateBuffer(channelId);
}

// ============================================================================
// 性能统计
// ============================================================================

Monitor::PerformanceStats MonitorDataProcessor::getPerformanceStats() const
{
    Monitor::PerformanceStats stats;
    
    qint64 elapsedMs = m_statsTimer.elapsed();
    if (elapsedMs > 0) {
        stats.samplesPerSecond = static_cast<int>(
            static_cast<qint64>(m_totalAppendCount) * 1000 / elapsedMs);
    }
    
    stats.lastUpdateTimeMs = QDateTime::currentMSecsSinceEpoch();
    
    // 统计各通道信息
    QReadLocker locker(&m_bufferLock);
    stats.activeChannelCount = static_cast<int>(m_channelBuffers.size());
    
    int totalPoints = 0;
    for (const auto& [key, buffer] : m_channelBuffers) {
        totalPoints += static_cast<int>(buffer->ringBuffer.size());
    }
    stats.totalPointsInChart = totalPoints;
    
    return stats;
}

void MonitorDataProcessor::resetPerformanceStats()
{
    m_totalAppendCount = 0;
    m_totalConsumeCount = 0;
    m_statsTimer.restart();
}

// ============================================================================
// 静态工具方法
// ============================================================================

QVector<QPointF> MonitorDataProcessor::samplesToPoints(const QList<Monitor::Sample>& samples)
{
    QVector<QPointF> points;
    points.reserve(samples.size());
    for (const auto& sample : samples) {
        if (sample.valueValid) {
            points.append(sample.toPoint());
        }
    }
    return points;
}

std::optional<double> MonitorDataProcessor::calculateMin(const QVector<QPointF>& points)
{
    if (points.isEmpty()) {
        return std::nullopt;
    }
    
    double minVal = std::numeric_limits<double>::max();
    for (const auto& pt : points) {
        if (pt.y() < minVal) {
            minVal = pt.y();
        }
    }
    return minVal;
}

std::optional<double> MonitorDataProcessor::calculateMax(const QVector<QPointF>& points)
{
    if (points.isEmpty()) {
        return std::nullopt;
    }
    
    double maxVal = std::numeric_limits<double>::lowest();
    for (const auto& pt : points) {
        if (pt.y() > maxVal) {
            maxVal = pt.y();
        }
    }
    return maxVal;
}

std::optional<double> MonitorDataProcessor::calculateAvg(const QVector<QPointF>& points)
{
    if (points.isEmpty()) {
        return std::nullopt;
    }
    
    double sum = 0.0;
    for (const auto& pt : points) {
        sum += pt.y();
    }
    return sum / points.size();
}

// ============================================================================
// 私有方法
// ============================================================================

DataProcessorConfig MonitorDataProcessor::normalizeConfig(DataProcessorConfig config)
{
    if (config.timeWindowMs < 0) {
        config.timeWindowMs = 0;
    }
    if (config.maxDisplaySamples < 0) {
        config.maxDisplaySamples = 0;
    }
    config.ringBufferCapacity = std::max(1, config.ringBufferCapacity);
    config.smoothingWindow = std::max(1, config.smoothingWindow);
    config.downsampleFactor = std::max(1, config.downsampleFactor);
    config.batchProcessThreshold = std::max(1, config.batchProcessThreshold);
    config.adaptiveDownsampleThreshold =
        std::max(2, config.adaptiveDownsampleThreshold);
    config.maxDeltaBufferSize = std::max(1, config.maxDeltaBufferSize);
    config.maxDeltaBufferSize =
        std::max(config.maxDeltaBufferSize, config.ringBufferCapacity);

    if (config.minClamp.has_value() && !std::isfinite(config.minClamp.value())) {
        config.minClamp.reset();
    }
    if (config.maxClamp.has_value() && !std::isfinite(config.maxClamp.value())) {
        config.maxClamp.reset();
    }
    if (config.minClamp.has_value() && config.maxClamp.has_value()
        && config.minClamp.value() > config.maxClamp.value()) {
        std::swap(config.minClamp, config.maxClamp);
    }

    return config;
}

bool MonitorDataProcessor::configsEqual(const DataProcessorConfig& lhs,
                                        const DataProcessorConfig& rhs)
{
    return lhs.maxDisplaySamples == rhs.maxDisplaySamples
        && lhs.timeWindowMs == rhs.timeWindowMs
        && lhs.ringBufferCapacity == rhs.ringBufferCapacity
        && lhs.enableSmoothing == rhs.enableSmoothing
        && lhs.smoothingWindow == rhs.smoothingWindow
        && lhs.enableDownsampling == rhs.enableDownsampling
        && lhs.downsampleFactor == rhs.downsampleFactor
        && lhs.minClamp == rhs.minClamp
        && lhs.maxClamp == rhs.maxClamp
        && lhs.enableIncrementalMode == rhs.enableIncrementalMode
        && lhs.batchProcessThreshold == rhs.batchProcessThreshold
        && lhs.maxDeltaBufferSize == rhs.maxDeltaBufferSize
        && lhs.enableAdaptiveDownsample == rhs.enableAdaptiveDownsample
        && lhs.adaptiveDownsampleThreshold == rhs.adaptiveDownsampleThreshold;
}

void MonitorDataProcessor::updateConfig(
    const std::function<void(DataProcessorConfig&)>& updater)
{
    bool changed = false;
    {
        QWriteLocker bufferLocker(&m_bufferLock);
        DataProcessorConfig candidate = m_config;
        updater(candidate);
        candidate = normalizeConfig(candidate);

        if (configsEqual(m_config, candidate)) {
            return;
        }

        if (m_config.ringBufferCapacity != candidate.ringBufferCapacity) {
            std::vector<std::pair<ChannelBuffer*, Monitor::RingBuffer<QPointF>>>
                resizedBuffers;
            resizedBuffers.reserve(m_channelBuffers.size());
            for (auto& entry : m_channelBuffers) {
                auto& buffer = entry.second;
                const auto retained = buffer->ringBuffer.last(
                    static_cast<size_t>(candidate.ringBufferCapacity));
                Monitor::RingBuffer<QPointF> resized(
                    static_cast<size_t>(candidate.ringBufferCapacity));
                resized.append(retained);
                resizedBuffers.emplace_back(buffer.get(), std::move(resized));
            }
            for (auto& resized : resizedBuffers) {
                resized.first->ringBuffer = std::move(resized.second);
            }
        }

        for (auto& entry : m_channelBuffers) {
            auto& buffer = entry.second;
            if (!candidate.enableIncrementalMode) {
                buffer->deltaBuffer.clear();
            } else if (buffer->deltaBuffer.size() > candidate.maxDeltaBufferSize) {
                buffer->deltaBuffer.deltaPoints.remove(
                    0, buffer->deltaBuffer.size() - candidate.maxDeltaBufferSize);
            }
        }

        m_config = candidate;
        ++m_configGeneration;

        // 保持 buffer -> cache 的单向锁顺序，防止旧配置处理结果重新写入。
        QMutexLocker cacheLocker(&m_cacheMutex);
        m_cachedData.clear();
        changed = true;
    }

    if (changed) {
        emit configChanged();
    }
}

DataProcessorConfig MonitorDataProcessor::snapshotConfig(quint64* generation) const
{
    QReadLocker locker(&m_bufferLock);
    if (generation) {
        *generation = m_configGeneration;
    }
    return m_config;
}

MonitorDataProcessor::ChannelBuffer& 
MonitorDataProcessor::getOrCreateBuffer(const QString& channelId)
{
    // 注意：调用此方法前必须持有写锁
    auto it = m_channelBuffers.find(channelId);
    if (it == m_channelBuffers.end()) {
        auto buffer = std::make_unique<ChannelBuffer>(m_config.ringBufferCapacity);
        // std::map 支持 emplace，可以原地构造并移动 unique_ptr
        auto result = m_channelBuffers.emplace(channelId, std::move(buffer));
        it = result.first;
    }
    return *it->second;
}

QVector<QPointF> MonitorDataProcessor::applyProcessingPipeline(
    const QVector<QPointF>& points,
    const DataProcessorConfig& config) const
{
    QVector<QPointF> result = points;
    
    // 1. 限幅
    if (config.minClamp.has_value() || config.maxClamp.has_value()) {
        result = applyClamp(result, config);
    }
    
    // 2. 平滑
    if (config.enableSmoothing && config.smoothingWindow > 1) {
        result = applySmoothing(result, config);
    }
    
    // 3. 降采样
    if (config.enableDownsampling && config.downsampleFactor > 1) {
        result = applyDownsampling(result, config);
    }
    
    // 4. 自适应降采样（高负载时）
    if (config.enableAdaptiveDownsample) {
        result = applyAdaptiveDownsample(result, config);
    }
    
    // 5. 限制点数
    result = limitPointCount(result, config);
    
    return result;
}

QVector<QPointF> MonitorDataProcessor::applyClamp(
    const QVector<QPointF>& points,
    const DataProcessorConfig& config) const
{
    QVector<QPointF> result;
    result.reserve(points.size());
    
    for (const auto& pt : points) {
        double y = pt.y();
        if (config.minClamp.has_value()) {
            y = std::max(y, config.minClamp.value());
        }
        if (config.maxClamp.has_value()) {
            y = std::min(y, config.maxClamp.value());
        }
        result.append(QPointF(pt.x(), y));
    }
    
    return result;
}

QVector<QPointF> MonitorDataProcessor::applySmoothing(
    const QVector<QPointF>& points,
    const DataProcessorConfig& config) const
{
    if (points.size() < config.smoothingWindow) {
        return points;
    }
    
    QVector<QPointF> result;
    result.reserve(points.size());
    
    int halfWindow = config.smoothingWindow / 2;
    
    for (int i = 0; i < points.size(); ++i) {
        double sum = 0.0;
        int count = 0;
        
        for (int j = std::max(0, i - halfWindow); 
             j <= std::min(points.size() - 1, i + halfWindow); 
             ++j) {
            sum += points[j].y();
            count++;
        }
        
        result.append(QPointF(points[i].x(), sum / count));
    }
    
    return result;
}

QVector<QPointF> MonitorDataProcessor::applyDownsampling(
    const QVector<QPointF>& points,
    const DataProcessorConfig& config) const
{
    if (points.size() <= config.downsampleFactor) {
        return points;
    }
    
    QVector<QPointF> result;
    result.reserve(points.size() / config.downsampleFactor + 1);
    
    for (int i = 0; i < points.size(); i += config.downsampleFactor) {
        result.append(points[i]);
    }
    
    // 确保最后一个点被包含
    if (!points.isEmpty() && (result.isEmpty() || result.last() != points.last())) {
        result.append(points.last());
    }
    
    return result;
}

QVector<QPointF> MonitorDataProcessor::limitPointCount(
    const QVector<QPointF>& points,
    const DataProcessorConfig& config) const
{
    if (points.size() <= config.maxDisplaySamples || config.maxDisplaySamples <= 0) {
        return points;
    }

    // maxDisplaySamples == 1 时直接返回最后一个点
    if (config.maxDisplaySamples == 1) {
        return {points.last()};
    }

    // 等间距采样
    QVector<QPointF> result;
    result.reserve(config.maxDisplaySamples);

    double step = static_cast<double>(points.size() - 1) / (config.maxDisplaySamples - 1);
    
    for (int i = 0; i < config.maxDisplaySamples - 1; ++i) {
        int idx = static_cast<int>(i * step);
        result.append(points[idx]);
    }
    
    // 确保最后一个点
    result.append(points.last());
    
    return result;
}

QVector<QPointF> MonitorDataProcessor::applyTimeWindow(
    const QVector<QPointF>& points,
    const DataProcessorConfig& config) const
{
    if (points.isEmpty() || config.timeWindowMs <= 0) {
        return points;
    }
    
    qint64 cutoffTime = QDateTime::currentMSecsSinceEpoch() - config.timeWindowMs;
    
    // 找到第一个在时间窗口内的点
    int startIdx = -1;
    for (int i = 0; i < points.size(); ++i) {
        if (points[i].x() >= cutoffTime) {
            startIdx = i;
            break;
        }
    }
    
    if (startIdx < 0) {
        return {};
    }
    if (startIdx == 0) {
        return points;
    }
    
    return points.mid(startIdx);
}

QVector<QPointF> MonitorDataProcessor::applyAdaptiveDownsample(
    const QVector<QPointF>& points,
    const DataProcessorConfig& config) const
{
    if (points.size() <= config.adaptiveDownsampleThreshold) {
        return points;
    }
    
    // 计算需要的降采样因子
    int targetCount = config.adaptiveDownsampleThreshold / 2;
    int factor = (points.size() + targetCount - 1) / targetCount;
    
    if (factor <= 1) {
        return points;
    }
    
    QVector<QPointF> result;
    result.reserve(targetCount + 1);
    
    for (int i = 0; i < points.size(); i += factor) {
        result.append(points[i]);
    }
    
    // 确保最后一个点
    if (!points.isEmpty() && (result.isEmpty() || result.last() != points.last())) {
        result.append(points.last());
    }
    
    return result;
}
