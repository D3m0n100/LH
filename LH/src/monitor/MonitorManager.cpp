// 文件：src/monitor/MonitorManager.cpp
// 监控管理器实现（性能优化版本）
#include "MonitorManager.h"
#include "MonitorChannel.h"
#include "MonitorDataProcessor.h"
#include "MonitorDataLogger.h"
#include "communication/RuntimePointQualityMapper.h"
#include "core/DataManager.h"

// 运行时配置类型（避免在头文件中引入，减少依赖）
#include "../common/RuntimePointTypes.h"
#include "../communication/IDeviceBackend.h"

#include <QReadLocker>
#include <QWriteLocker>
#include <QDebug>
#include <QCoreApplication>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace Monitor {

namespace {

// 用于标记 applyConfiguration() 生成/管理的资源，便于幂等清理
constexpr const char* kRuntimeManagedKey = "__runtimeManaged";
struct DemoSamplerState
{
    std::mt19937 rng;
    std::normal_distribution<double> noise;
    double minValue = 0.0;
    double maxValue = 100.0;
    double value = 0.0;
    double phase = 0.0;
    bool isDigital = false;
    int digitalValue = 0;
    int tick = 0;

    DemoSamplerState(uint32_t seed, double minV, double maxV, bool digital)
        : rng(seed)
        , noise(0.0, 1.0)
        , minValue(minV)
        , maxValue(maxV)
        , isDigital(digital)
    {
        constexpr double kPi = 3.14159265358979323846;
        const double mid = (minValue + maxValue) * 0.5;
        value = mid;
        phase = (static_cast<double>(seed % 360) / 180.0) * kPi;
        digitalValue = (seed % 2) ? 1 : 0;
    }
};

static QString normalizeUnit(QString unit)
{
    unit = unit.trimmed();
    unit.replace(QStringLiteral("℃"), QStringLiteral("°C"));
    return unit;
}

static bool isDigitalSnippetOrUnit(const QString& snippetId, const QString& unit, const QVariantMap& metadata)
{
    const QString sid = snippetId.trimmed().toLower();
    const QString u = normalizeUnit(unit).trimmed().toLower();

    if (sid.contains("digital")) {
        return true;
    }
    if (metadata.value("type").toString().toLower().contains("digital")) {
        return true;
    }
    if (metadata.value("digital").toBool()) {
        return true;
    }
    if (u.isEmpty() && (sid.contains("input") || sid.contains("output"))) {
        // 仍然需要避免把 analog_* 误判为数字点
        if (!sid.contains("analog")) {
            return true;
        }
    }
    if (u == "bool" || u == "boolean" || u == "on/off" || u == "onoff") {
        return true;
    }
    return false;
}

static void inferRangeFromUnitAndMetadata(const QString& unit,
                                         const QVariantMap& metadata,
                                         double& outMin,
                                         double& outMax)
{
    const bool hasMin = metadata.contains("min_value") || metadata.contains("min");
    const bool hasMax = metadata.contains("max_value") || metadata.contains("max");
    if (hasMin && hasMax) {
        outMin = metadata.value("min_value", metadata.value("min")).toDouble();
        outMax = metadata.value("max_value", metadata.value("max")).toDouble();
        if (outMax < outMin) {
            std::swap(outMin, outMax);
        }
        return;
    }

    const QString u = normalizeUnit(unit).toLower();

    if (u.contains("%")) {
        outMin = 0.0;
        outMax = 100.0;
    } else if (u.contains("bar")) {
        outMin = 0.0;
        outMax = 100.0;
    } else if (u.contains("mpa")) {
        outMin = 0.0;
        outMax = 10.0;
    } else if (u.contains("掳c") || u.contains("c")) {
        outMin = 0.0;
        outMax = 150.0;
    } else if (u.contains("ma")) {
        outMin = 0.0;
        outMax = 20.0;
    } else if (u == "v" || u.endsWith(" v") || u.contains("volt")) {
        outMin = 0.0;
        outMax = 10.0;
    } else if (u.contains("rpm")) {
        outMin = 0.0;
        outMax = 3000.0;
    } else if (u.contains("l/min") || u.contains("lpm")) {
        outMin = 0.0;
        outMax = 200.0;
    } else if (u.contains("ms")) {
        outMin = 0.0;
        outMax = 5000.0;
    } else {
        outMin = 0.0;
        outMax = 100.0;
    }
}

static std::function<double()> makeDemoSampler(const QString& providerId,
                                               const QString& channelName,
                                               const QString& unit,
                                               const QString& snippetId,
                                               const QVariantMap& metadata)
{
    const uint32_t seed = static_cast<uint32_t>(qHash(providerId + "|" + channelName + "|" + unit + "|" + snippetId));
    const bool digital = isDigitalSnippetOrUnit(snippetId, unit, metadata);

    double minV = 0.0;
    double maxV = 100.0;
    inferRangeFromUnitAndMetadata(unit, metadata, minV, maxV);

    if (digital) {
        minV = 0.0;
        maxV = 1.0;
    }


    auto state = std::make_shared<DemoSamplerState>(seed, minV, maxV, digital);

    return [state]() -> double {
        state->tick++;

        if (state->isDigital) {
            if (state->tick % 25 == 0) {
                const int r = static_cast<int>(state->rng() % 4); // 25% 机会翻转
                if (r == 0) {
                    state->digitalValue = 1 - state->digitalValue;
                }
            }
            return static_cast<double>(state->digitalValue);
        }

        const double range = std::max(1e-9, state->maxValue - state->minValue);
        const double mid = (state->minValue + state->maxValue) * 0.5;
        const double amplitude = range * 0.25;

        state->phase += 0.08; // 控制变化速度
        const double target = mid + amplitude * std::sin(state->phase);

        const double noiseStd = range * 0.01;
        const double n = state->noise(state->rng) * noiseStd;

        state->value = state->value * 0.92 + target * 0.08 + n;

        // 限幅
        if (state->value < state->minValue) state->value = state->minValue;
        if (state->value > state->maxValue) state->value = state->maxValue;
        return state->value;
    };
}

static QString qualityToString(RuntimePointQuality q)
{
    return runtimePointQualityToString(q);
}

} // namespace

// ============================================================================
// 单例与构造
// ============================================================================

MonitorManager& MonitorManager::instance()
{
    static MonitorManager s_instance;
    return s_instance;
}

MonitorManager::MonitorManager()
    : QObject(nullptr)
    , m_backendPollTimer(new QTimer(this))
    , m_cleanupTimer(new QTimer(this))
    , m_dataRetentionDays(DEFAULT_DATA_RETENTION_DAYS)
{
    m_backendPollClock.start();
    setupCleanupTimer();

    connect(m_backendPollTimer, &QTimer::timeout,
            this, &MonitorManager::onBackendPollTimeout);

    m_dataLogger = std::make_unique<MonitorDataLogger>(this);
    m_dataLogger->setEnabled(DEFAULT_DB_LOGGING_ENABLED);

    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                this, [this]() { shutdown(); });
    }
}

MonitorManager::~MonitorManager()
{
    shutdown();
    m_dataLogger.reset();
}

void MonitorManager::shutdown()
{
    stopMonitoring();

    if (m_backendPointsChangedConnection) {
        disconnect(m_backendPointsChangedConnection);
        m_backendPointsChangedConnection = {};
    }
    if (m_backendConnectionStateConnection) {
        disconnect(m_backendConnectionStateConnection);
        m_backendConnectionStateConnection = {};
    }
    if (m_backendDestroyedConnection) {
        disconnect(m_backendDestroyedConnection);
        m_backendDestroyedConnection = {};
    }
    if (m_dataProcessorDestroyedConnection) {
        disconnect(m_dataProcessorDestroyedConnection);
        m_dataProcessorDestroyedConnection = {};
    }

    m_backend = nullptr;
    m_dataProcessor = nullptr;
    m_backendPointIds.clear();
    m_pointIdToChannel.clear();
    m_backendPointPeriodsMs.clear();
    m_backendPointNextDueMs.clear();

    if (m_dataLogger) {
        m_dataLogger->shutdown();
    }
}

void MonitorManager::setupCleanupTimer()
{
    connect(m_cleanupTimer, &QTimer::timeout,
            this, &MonitorManager::onCleanupTimeout);
    m_cleanupTimer->setInterval(DEFAULT_CLEANUP_INTERVAL_MS);
}

// ============================================================================
// 数据处理器连接
// ============================================================================

void MonitorManager::setDataProcessor(MonitorDataProcessor* processor)
{
    if (m_dataProcessor.data() == processor) {
        return;
    }

    if (m_dataProcessorDestroyedConnection) {
        disconnect(m_dataProcessorDestroyedConnection);
        m_dataProcessorDestroyedConnection = {};
    }

    m_dataProcessor = processor;
    if (processor) {
        m_dataProcessorDestroyedConnection = connect(
            processor, &QObject::destroyed, this, [this]() {
                m_dataProcessor = nullptr;
                m_dataProcessorDestroyedConnection = {};
            });
    }

    // 确保处理器中的通道与当前注册通道同步
    if (processor) {
        QReadLocker locker(&m_channelLock);
        for (const QString& channelName : m_channels.keys()) {
            processor->ensureChannel(channelName);
        }
    }

    qDebug() << "[MonitorManager] 数据处理器已设置:"
             << (processor ? "启用增量分发" : "禁用增量分发");
}

MonitorDataProcessor* MonitorManager::dataProcessor() const
{
    return m_dataProcessor.data();
}

// ============================================================================
// 设备后端连接
// ============================================================================


// ============================================================================
// 监控采样日志开关
// ============================================================================
void MonitorManager::setDatabaseLoggingEnabled(bool enabled)
{
    if (m_dataLogger) {
        m_dataLogger->setEnabled(enabled);
    }
}

bool MonitorManager::isDatabaseLoggingEnabled() const
{
    return m_dataLogger ? m_dataLogger->isEnabled() : false;
}

bool MonitorManager::isDatabaseHistoryAvailable() const
{
    return DataManager::instance().isInitialized();
}

void MonitorManager::flushDatabaseLogging()
{
    if (m_dataLogger) {
        m_dataLogger->flush();
    }
}


// ============================================================================
// 通道管理
// ============================================================================

bool MonitorManager::registerChannel(const ChannelConfig& config)
{
    if (config.name.isEmpty()) {
        qWarning() << "[MonitorManager] 无法注册通道：名称为空";
        return false;
    }

    {
        QWriteLocker locker(&m_channelLock);

        if (m_channels.contains(config.name)) {
            qDebug() << "[MonitorManager] 通道已存在，更新配置:" << config.name;
            m_channels[config.name]->updateConfig(config);
            return true;
        }

        auto channel = std::make_shared<MonitorChannel>(config);
        connectChannelSignals(channel);
        m_channels.insert(config.name, channel);
    }

    // 同步到数据处理器
    {
        QPointer<MonitorDataProcessor> proc = m_dataProcessor;
        if (proc) {
            proc->ensureChannel(config.name);
        }
    }

    qDebug() << "[MonitorManager] 注册通道:" << config.name;
    emit channelRegistered(config.name);
    emit channelsChanged();

    return true;
}

bool MonitorManager::removeChannel(const QString& name)
{
    {
        QWriteLocker locker(&m_channelLock);

        if (!m_channels.contains(name)) {
            return false;
        }

        m_channels.remove(name);
    }

    {
        QPointer<MonitorDataProcessor> proc = m_dataProcessor;
        if (proc) {
            proc->clearChannelCache(name);
        }
    }

    qDebug() << "[MonitorManager] 移除通道:" << name;
    emit channelRemoved(name);
    emit channelsChanged();

    return true;
}

bool MonitorManager::hasChannel(const QString& name) const
{
    QReadLocker locker(&m_channelLock);
    return m_channels.contains(name);
}

QStringList MonitorManager::channelNames() const
{
    QReadLocker locker(&m_channelLock);
    return m_channels.keys();
}

std::shared_ptr<MonitorChannel> MonitorManager::channel(const QString& name) const
{
    QReadLocker locker(&m_channelLock);
    return m_channels.value(name);
}

ChannelConfig MonitorManager::channelConfig(const QString& name) const
{
    QReadLocker locker(&m_channelLock);
    auto ch = m_channels.value(name);
    return ch ? ch->config() : ChannelConfig();
}

void MonitorManager::updateChannelConfig(const QString& name, const ChannelConfig& config)
{
    QReadLocker locker(&m_channelLock);
    auto ch = m_channels.value(name);
    if (ch) {
        ch->updateConfig(config);
    }
}

// ============================================================================
// 数据记录（增量分发）
// ============================================================================

void MonitorManager::recordSample(const QString& channelName,
                                  double value,
                                  const QString& unit,
                                  const QVariantMap& metadata)
{
    Sample sample(channelName, value, unit, QDateTime::currentDateTimeUtc(), metadata);
    recordSample(sample);
}

void MonitorManager::recordSample(const Sample& sample)
{
    std::shared_ptr<MonitorChannel> ch;

    {
        QReadLocker locker(&m_channelLock);
        ch = m_channels.value(sample.channelName);
    }

    if (!ch) {
        // 自动注册通道
        ChannelConfig config;
        config.name = sample.channelName;
        config.unit = sample.unit;
        registerChannel(config);

        QReadLocker locker(&m_channelLock);
        ch = m_channels.value(sample.channelName);
    }

    if (ch && sample.valueValid) {
        ch->appendSample(sample);
    }

    // 可选：落库（批量写入）
    if (m_dataLogger) {
        m_dataLogger->enqueueSample(sample);
    }

    // 增量分发到数据处理器
    dispatchToProcessor(sample.channelName, sample);

    if (sample.valueValid) {
        emit sampleRecorded(sample.channelName, sample.value,
                            sample.unit, sample.timestamp);
    }
}

void MonitorManager::recordSamples(const QString& channelName,
                                  const QList<Sample>& samples)
{
    if (samples.isEmpty()) {
        return;
    }

    QList<Sample> normalizedSamples = samples;
    for (Sample& s : normalizedSamples) {
        if (s.channelName.isEmpty() || s.channelName != channelName) {
            s.channelName = channelName;
        }
    }

    std::shared_ptr<MonitorChannel> ch;

    {
        QReadLocker locker(&m_channelLock);
        ch = m_channels.value(channelName);
    }

    if (!ch) {
        // 自动注册通道
        ChannelConfig config;
        config.name = channelName;
        if (!normalizedSamples.isEmpty()) {
            config.unit = normalizedSamples.first().unit;
        }
        registerChannel(config);

        QReadLocker locker(&m_channelLock);
        ch = m_channels.value(channelName);
    }

    if (ch) {
        for (const Sample& sample : normalizedSamples) {
            if (sample.valueValid) {
                ch->appendSample(sample);
            }
        }
    }

    // 可选：落库（批量写入）
    if (m_dataLogger) {
        m_dataLogger->enqueueSamples(normalizedSamples);
    }

    // 批量增量分发
    dispatchToProcessor(channelName, normalizedSamples);

    const int validCount = std::count_if(normalizedSamples.cbegin(), normalizedSamples.cend(),
                                         [](const Sample& sample) { return sample.valueValid; });
    if (validCount > 0) {
        emit samplesRecorded(channelName, validCount);
    }
}

// ============================================================================
// 采集器管理
// ============================================================================

// ============================================================================
// 监控控制
// ============================================================================


// ============================================================================
// 数据清理
// ============================================================================

void MonitorManager::setDataRetentionDays(int days)
{
    m_dataRetentionDays = days > 0 ? days : 1;
}

void MonitorManager::clearChannelData(const QString& channelName)
{
    {
        QReadLocker locker(&m_channelLock);
        auto ch = m_channels.value(channelName);
        if (ch) {
            ch->clear();
        }
    }

    {
        QPointer<MonitorDataProcessor> proc = m_dataProcessor;
        if (proc) {
            proc->clearChannelCache(channelName);
        }
    }
}

void MonitorManager::clearAllData()
{
    {
        QReadLocker locker(&m_channelLock);
        for (auto& ch : m_channels) {
            ch->clear();
        }
    }

    {
        QPointer<MonitorDataProcessor> proc = m_dataProcessor;
        if (proc) {
            proc->clearAllCache();
        }
    }
}

// ============================================================================
// 私有函数
// ============================================================================
void MonitorManager::onCleanupTimeout()
{
    if (!m_isMonitoring.load(std::memory_order_acquire)) {
        return;
    }

    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-m_dataRetentionDays);

    {
        QReadLocker locker(&m_channelLock);
        for (auto& ch : m_channels) {
            ch->purgeOlderThan(cutoff);
        }
    }

    qDebug() << "[MonitorManager] 内存数据清理完成，截止时间:" << cutoff;

    if (!isDatabaseLoggingEnabled()) {
        return;
    }

    DataManager& dataManager = DataManager::instance();
    if (!dataManager.isInitialized()) {
        return;
    }

    const int deleted = dataManager.cleanupOldData(m_dataRetentionDays);
    if (deleted < 0) {
        qWarning() << "[MonitorManager] 持久化数据清理失败";
        return;
    }

    qDebug() << "[MonitorManager] 持久化数据清理完成，删除记录:" << deleted;
}


// ============================================================================
// 私有方法
// ============================================================================

void MonitorManager::connectChannelSignals(const std::shared_ptr<MonitorChannel>& channel)
{
    connect(channel.get(), &MonitorChannel::thresholdExceeded,
            this, [this](const QString& channelName, double value,
                         const QString& unit, double thresholdValue, ThresholdMode mode) {
                Q_UNUSED(unit);
                Q_UNUSED(mode);
                emit thresholdExceeded(channelName, value, thresholdValue);
            });
}

void MonitorManager::dispatchToProcessor(const QString& channelName, const Sample& sample)
{
    QPointer<MonitorDataProcessor> proc = m_dataProcessor;
    if (!proc) {
        return;
    }

    // 增量追加到数据处理器
    proc->appendSample(channelName, sample);
}

void MonitorManager::dispatchToProcessor(const QString& channelName,
                                         const QList<Sample>& samples)
{
    QPointer<MonitorDataProcessor> proc = m_dataProcessor;
    if (!proc || samples.isEmpty()) {
        return;
    }

    // 批量增量追加到数据处理器
    proc->appendSamples(channelName, samples);
}

bool MonitorManager::applyConfiguration(const ProjectRuntimeConfig& config)
{
    qDebug() << "[MonitorManager] applyConfiguration 开始"
             << "project=" << config.projectName
             << "providers=" << config.providers.size()
             << "mappings=" << config.dslMappings.size();

    constexpr int kMinPeriodMs = 1;
    constexpr int kMaxPeriodMs = 10000;

    struct Candidate {
        QMap<QString, ChannelConfig> channelConfigs;
        QMap<QString, ProviderConfig> providers;
        QStringList backendPointIds;
        QHash<QString, QString> pointIdToChannel;
        QHash<QString, int> backendPointPeriodsMs;
        int backendPollIntervalMs = 0;
    } candidate;

    QHash<QString, DslMappingEntry> mappingById;
    QHash<QString, DslMappingEntry> mappingByChannel;
    QSet<QString> mappingIds;
    QSet<QString> mappingChannels;
    mappingById.reserve(config.dslMappings.size());
    mappingByChannel.reserve(config.dslMappings.size());

    auto addChannelConfig = [&](const ChannelConfig& channelConfig) {
        const QString name = channelConfig.name.trimmed();
        if (name.isEmpty()) {
            qWarning() << "[MonitorManager] 候选通道名称为空";
            return false;
        }
        if (candidate.channelConfigs.contains(name)) {
            qWarning() << "[MonitorManager] 候选通道名称重复:" << name;
            return false;
        }
        ChannelConfig normalized = channelConfig;
        normalized.name = name;
        candidate.channelConfigs.insert(name, normalized);
        return true;
    };

    for (const auto& rawEntry : config.dslMappings) {
        const QString mappingId = rawEntry.id.trimmed();
        const QString channelName = rawEntry.channelName.trimmed();
        if (channelName.isEmpty()) {
            qWarning() << "[MonitorManager] dslMappings 候选缺少 channelName，id="
                       << rawEntry.id << "channel=" << rawEntry.channelName;
            return false;
        }
        if (rawEntry.periodMs < kMinPeriodMs || rawEntry.periodMs > kMaxPeriodMs) {
            qWarning() << "[MonitorManager] dslMappings 周期非法:" << rawEntry.periodMs
                       << "channel=" << channelName;
            return false;
        }
        if (rawEntry.lineNumber < -1) {
            qWarning() << "[MonitorManager] dslMappings 行号非法:" << rawEntry.lineNumber
                       << "id=" << mappingId;
            return false;
        }
        if ((!mappingId.isEmpty() && mappingIds.contains(mappingId))
                || mappingChannels.contains(channelName)) {
            qWarning() << "[MonitorManager] dslMappings 存在重复 id/channel:" << mappingId
                       << channelName;
            return false;
        }

        DslMappingEntry entry = rawEntry;
        entry.id = mappingId;
        entry.channelName = channelName;
        if (!mappingId.isEmpty()) {
            mappingIds.insert(mappingId);
        }
        mappingChannels.insert(channelName);
        if (!mappingId.isEmpty()) {
            mappingById.insert(mappingId, entry);
        }
        mappingByChannel.insert(channelName, entry);

        ChannelConfig channelConfig;
        channelConfig.name = channelName;
        channelConfig.unit = entry.unit;
        channelConfig.displayName = !entry.snippetName.trimmed().isEmpty()
                                        ? entry.snippetName.trimmed()
                                        : channelName;
        channelConfig.maxSamples = Limits::DEFAULT_RING_BUFFER_CAPACITY;
        channelConfig.metadata["mappingId"] = entry.id;
        channelConfig.metadata["snippetId"] = entry.snippetId;
        channelConfig.metadata["snippetName"] = entry.snippetName;
        channelConfig.metadata["signalPath"] = entry.signalPath;
        channelConfig.metadata["lineNumber"] = entry.lineNumber;
        channelConfig.metadata["periodMs"] = entry.periodMs;
        channelConfig.metadata["projectName"] = config.projectName;
        channelConfig.metadata[kRuntimeManagedKey] = true;
        if (!addChannelConfig(channelConfig)) {
            return false;
        }
    }

    QList<MonitorProviderRuntimeConfig> providers = config.providers;
    if (providers.isEmpty() && !config.dslMappings.isEmpty()) {
        providers.reserve(config.dslMappings.size());
        for (const auto& rawEntry : config.dslMappings) {
            const auto mappingIt = mappingByChannel.constFind(rawEntry.channelName.trimmed());
            if (mappingIt == mappingByChannel.constEnd()) {
                return false;
            }
            const DslMappingEntry& entry = mappingIt.value();
            MonitorProviderRuntimeConfig provider;
            provider.id = entry.id.isEmpty() ? entry.channelName : entry.id;
            provider.channelName = entry.channelName;
            provider.unit = entry.unit;
            provider.periodMs = entry.periodMs;
            provider.priority = 128;
            provider.metadata = entry.metadata;
            provider.metadata["snippetId"] = entry.snippetId;
            provider.metadata["snippetName"] = entry.snippetName;
            provider.metadata["mappingId"] = entry.id;
            provider.metadata["signalPath"] = entry.signalPath;
            providers.push_back(provider);
        }
    }

    QSet<QString> providerChannels;
    for (const auto& rawProvider : providers) {
        const QString providerId = rawProvider.id.trimmed();
        const QString channelName = rawProvider.channelName.trimmed();
        if (providerId.isEmpty() || channelName.isEmpty()) {
            qWarning() << "[MonitorManager] provider 候选缺少 id/channelName，id="
                       << rawProvider.id << "channel=" << rawProvider.channelName;
            return false;
        }
        if (rawProvider.periodMs < kMinPeriodMs || rawProvider.periodMs > kMaxPeriodMs) {
            qWarning() << "[MonitorManager] provider 周期非法:" << rawProvider.periodMs
                       << "id=" << providerId;
            return false;
        }
        if (rawProvider.priority < 0 || rawProvider.priority > 255) {
            qWarning() << "[MonitorManager] provider priority 非法:" << rawProvider.priority
                       << "id=" << providerId;
            return false;
        }
        if (providerChannels.contains(channelName)) {
            qWarning() << "[MonitorManager] provider 通道重复:" << channelName;
            return false;
        }
        providerChannels.insert(channelName);

        const auto mappingIt = mappingById.constFind(providerId);
        if (mappingIt != mappingById.constEnd()
                && mappingIt->channelName.trimmed() != channelName) {
            qWarning() << "[MonitorManager] provider/mapping 通道不匹配:" << providerId
                       << mappingIt->channelName << channelName;
            return false;
        }

        if (!candidate.channelConfigs.contains(channelName)) {
            ChannelConfig channelConfig;
            channelConfig.name = channelName;
            channelConfig.unit = rawProvider.unit;
            channelConfig.displayName = channelName;
            channelConfig.maxSamples = Limits::DEFAULT_RING_BUFFER_CAPACITY;
            channelConfig.metadata = rawProvider.metadata;
            channelConfig.metadata["projectName"] = config.projectName;
            channelConfig.metadata["periodMs"] = rawProvider.periodMs;
            channelConfig.metadata[kRuntimeManagedKey] = true;
            if (mappingIt != mappingById.constEnd()) {
                const auto& mapping = mappingIt.value();
                if (!mapping.snippetName.trimmed().isEmpty()) {
                    channelConfig.displayName = mapping.snippetName.trimmed();
                }
                channelConfig.metadata["mappingId"] = mapping.id;
                channelConfig.metadata["snippetId"] = mapping.snippetId;
                channelConfig.metadata["snippetName"] = mapping.snippetName;
                channelConfig.metadata["signalPath"] = mapping.signalPath;
                channelConfig.metadata["lineNumber"] = mapping.lineNumber;
            } else if (mappingByChannel.contains(channelName)) {
                const auto& mapping = mappingByChannel.value(channelName);
                if (!mapping.snippetName.trimmed().isEmpty()) {
                    channelConfig.displayName = mapping.snippetName.trimmed();
                }
                channelConfig.metadata["mappingId"] = mapping.id;
                channelConfig.metadata["snippetId"] = mapping.snippetId;
                channelConfig.metadata["snippetName"] = mapping.snippetName;
                channelConfig.metadata["signalPath"] = mapping.signalPath;
                channelConfig.metadata["lineNumber"] = mapping.lineNumber;
            }
            if (!addChannelConfig(channelConfig)) {
                return false;
            }
        }

        ProviderConfig provider;
        provider.id = providerId;
        provider.channelName = channelName;
        provider.unit = rawProvider.unit;
        provider.periodMs = rawProvider.periodMs;
        provider.priority = rawProvider.priority;
        provider.metadata = rawProvider.metadata;
        provider.metadata["projectName"] = config.projectName;
        provider.metadata[kRuntimeManagedKey] = true;
        if (mappingIt != mappingById.constEnd()) {
            const auto& mapping = mappingIt.value();
            provider.metadata["snippetId"] = mapping.snippetId;
            provider.metadata["snippetName"] = mapping.snippetName;
            provider.metadata["mappingId"] = mapping.id;
            provider.metadata["signalPath"] = mapping.signalPath;
            provider.metadata["lineNumber"] = mapping.lineNumber;
        } else if (mappingByChannel.contains(channelName)) {
            const auto& mapping = mappingByChannel.value(channelName);
            provider.metadata["snippetId"] = mapping.snippetId;
            provider.metadata["snippetName"] = mapping.snippetName;
            provider.metadata["mappingId"] = mapping.id;
            provider.metadata["signalPath"] = mapping.signalPath;
            provider.metadata["lineNumber"] = mapping.lineNumber;
        }

        if (!m_backend) {
            const QString snippetId = provider.metadata.value("snippetId").toString();
            provider.sampler = makeDemoSampler(provider.id, provider.channelName,
                                               provider.unit, snippetId, provider.metadata);
            provider.errorHandler = [pid = provider.id](const QString& error) {
                qWarning() << "[MonitorManager] provider error:" << pid << error;
            };
        }
        candidate.providers.insert(provider.id, provider);

        if (m_backend) {
            const int periodMs = qMax(kMinPeriodMs, provider.periodMs);
            if (!candidate.backendPointPeriodsMs.contains(provider.id)) {
                candidate.backendPointIds.append(provider.id);
                candidate.backendPointPeriodsMs.insert(provider.id, periodMs);
            } else {
                candidate.backendPointPeriodsMs[provider.id] =
                    qMin(candidate.backendPointPeriodsMs.value(provider.id), periodMs);
            }
            candidate.pointIdToChannel.insert(provider.id, provider.channelName);
            if (candidate.backendPollIntervalMs <= 0
                    || periodMs < candidate.backendPollIntervalMs) {
                candidate.backendPollIntervalMs = periodMs;
            }
        }
    }

    auto addObjectChannel = [&](const QString& sourceName,
                                const QString& channelName,
                                const QString& displayName,
                                const QString& unit,
                                MonitorObjectKind kind,
                                bool editable,
                                const QVariantMap& extraMetadata) {
        const QString normalizedSource = sourceName.trimmed();
        const QString normalizedChannel = channelName.trimmed();
        if (normalizedSource.isEmpty() || normalizedChannel.isEmpty()) {
            qWarning() << "[MonitorManager] 监控对象候选缺少名称:" << sourceName;
            return false;
        }
        ChannelConfig channelConfig;
        channelConfig.name = normalizedChannel;
        channelConfig.displayName = displayName.trimmed().isEmpty()
                                        ? normalizedChannel
                                        : displayName.trimmed();
        channelConfig.unit = unit;
        channelConfig.maxSamples = Limits::DEFAULT_RING_BUFFER_CAPACITY;
        channelConfig.objectKind = kind;
        channelConfig.sourceName = normalizedSource;
        channelConfig.editable = editable;
        channelConfig.defaultVisible = false;
        channelConfig.metadata = extraMetadata;
        channelConfig.metadata["projectName"] = config.projectName;
        channelConfig.metadata["objectKind"] = static_cast<int>(kind);
        channelConfig.metadata["sourceName"] = normalizedSource;
        channelConfig.metadata["editable"] = editable;
        channelConfig.metadata[kRuntimeManagedKey] = true;
        return addChannelConfig(channelConfig);
    };

    QSet<QString> variableIds;
    QSet<QString> variableNames;
    for (const auto& variable : config.variables) {
        const QString id = variable.id.trimmed();
        const QString name = variable.name.trimmed();
        if (id.isEmpty() || name.isEmpty() || variable.dataType.trimmed().isEmpty()
                || variableIds.contains(id) || variableNames.contains(name)) {
            qWarning() << "[MonitorManager] 变量候选字段非法或重复:" << id << name;
            return false;
        }
        variableIds.insert(id);
        variableNames.insert(name);
        QVariantMap metadata = variable.metadata;
        metadata["variableId"] = id;
        metadata["variableName"] = name;
        metadata["dataType"] = variable.dataType;
        metadata["scope"] = variable.scope;
        metadata["binding"] = variable.binding;
        metadata["defaultValue"] = variable.defaultValue;
        if (!addObjectChannel(name, QStringLiteral("var::%1").arg(name), name,
                              QStringLiteral("var"), MonitorObjectKind::Variable,
                              !variable.readOnly, metadata)) {
            return false;
        }
    }

    QSet<QString> parameterIds;
    QSet<QString> parameterNames;
    for (const auto& parameter : config.parameters) {
        const QString id = parameter.id.trimmed();
        const QString name = parameter.name.trimmed();
        if (id.isEmpty() || name.isEmpty() || parameter.dataType.trimmed().isEmpty()
                || parameterIds.contains(id) || parameterNames.contains(name)) {
            qWarning() << "[MonitorManager] 参数候选字段非法或重复:" << id << name;
            return false;
        }
        parameterIds.insert(id);
        parameterNames.insert(name);
        QVariantMap metadata = parameter.metadata;
        metadata["parameterId"] = id;
        metadata["parameterName"] = name;
        metadata["dataType"] = parameter.dataType;
        metadata["defaultValue"] = parameter.defaultValue;
        metadata["minValue"] = parameter.minValue;
        metadata["maxValue"] = parameter.maxValue;
        metadata["unit"] = parameter.unit;
        metadata["onlineEditable"] = parameter.onlineEditable;
        if (!addObjectChannel(name, QStringLiteral("param::%1").arg(name), name,
                              parameter.unit, MonitorObjectKind::Parameter,
                              parameter.onlineEditable, metadata)) {
            return false;
        }
    }

    QSet<QString> resourceIds;
    QSet<QString> resourceKeys;
    for (const auto& resource : config.resources) {
        const QString id = resource.id.trimmed();
        const QString resourceChannel = resource.channel.trimmed();
        const QString resourceName = resource.resourceName.trimmed();
        const QString resourceType = resource.resourceType.trimmed();
        const QString resourceKey = resourceType + QStringLiteral(":") + resourceChannel;
        if (id.isEmpty() || resourceType.isEmpty() || resourceChannel.isEmpty()
                || resourceIds.contains(id) || resourceKeys.contains(resourceKey)) {
            qWarning() << "[MonitorManager] 资源候选字段非法或重复:" << id << resourceKey;
            return false;
        }
        resourceIds.insert(id);
        resourceKeys.insert(resourceKey);
        const QString sourceName = resourceName.isEmpty() ? resourceChannel : resourceName;
        const QString channelKey = resourceChannel;
        QVariantMap metadata = resource.metadata;
        metadata["resourceId"] = id;
        metadata["resourceType"] = resourceType;
        metadata["resourceName"] = resourceName;
        metadata["channel"] = resourceChannel;
        metadata["owner"] = resource.owner;
        metadata["exclusive"] = resource.exclusive;
        if (!addObjectChannel(sourceName, QStringLiteral("res::%1").arg(channelKey),
                              sourceName, resourceType, MonitorObjectKind::Resource,
                              false, metadata)) {
            return false;
        }
    }

    QSet<QString> preservedChannelNames;
    {
        QReadLocker locker(&m_channelLock);
        for (auto it = m_channels.constBegin(); it != m_channels.constEnd(); ++it) {
            if (it.value()
                    && !it.value()->config().metadata.value(kRuntimeManagedKey).toBool()) {
                preservedChannelNames.insert(it.key());
            }
        }
    }
    for (auto it = candidate.channelConfigs.constBegin();
         it != candidate.channelConfigs.constEnd(); ++it) {
        if (preservedChannelNames.contains(it.key())) {
            qWarning() << "[MonitorManager] 候选通道覆盖非 runtime-managed 通道:" << it.key();
            return false;
        }
    }

    QSet<QString> preservedProviderIds;
    {
        QReadLocker locker(&m_providerLock);
        for (auto it = m_providers.constBegin(); it != m_providers.constEnd(); ++it) {
            if (!it.value().metadata.value(kRuntimeManagedKey).toBool()) {
                preservedProviderIds.insert(it.key());
            }
        }
    }
    for (auto it = candidate.providers.constBegin(); it != candidate.providers.constEnd(); ++it) {
        if (preservedProviderIds.contains(it.key())) {
            qWarning() << "[MonitorManager] 候选 provider 覆盖非 runtime-managed provider:" << it.key();
            return false;
        }
    }

    QMap<QString, std::shared_ptr<MonitorChannel>> candidateChannels;
    for (auto it = candidate.channelConfigs.constBegin();
         it != candidate.channelConfigs.constEnd(); ++it) {
        auto channel = std::make_shared<MonitorChannel>(it.value());
        connectChannelSignals(channel);
        candidateChannels.insert(it.key(), channel);
    }

    QMap<QString, QTimer*> candidateTimers;
    for (auto it = candidate.providers.constBegin(); it != candidate.providers.constEnd(); ++it) {
        if (!it.value().sampler || it.value().periodMs <= 0) {
            continue;
        }
        auto* timer = new QTimer(this);
        timer->setProperty("providerId", it.key());
        connect(timer, &QTimer::timeout, this, &MonitorManager::onProviderTimeout);
        timer->setInterval(it.value().periodMs);
        candidateTimers.insert(it.key(), timer);
    }

    QStringList oldRuntimeChannelNames;
    QMap<QString, std::shared_ptr<MonitorChannel>> nextChannels;
    {
        QReadLocker locker(&m_channelLock);
        nextChannels = m_channels;
        for (auto it = m_channels.constBegin(); it != m_channels.constEnd(); ++it) {
            if (it.value()
                    && it.value()->config().metadata.value(kRuntimeManagedKey).toBool()) {
                oldRuntimeChannelNames.append(it.key());
                nextChannels.remove(it.key());
            }
        }
    }
    for (auto it = candidateChannels.constBegin(); it != candidateChannels.constEnd(); ++it) {
        nextChannels.insert(it.key(), it.value());
    }

    QStringList oldRuntimeProviderIds;
    QMap<QString, ProviderConfig> nextProviders;
    QMap<QString, QTimer*> nextTimers;
    {
        QReadLocker providerLocker(&m_providerLock);
        nextProviders = m_providers;
        nextTimers = m_providerTimers;
        for (auto it = m_providers.constBegin(); it != m_providers.constEnd(); ++it) {
            if (it.value().metadata.value(kRuntimeManagedKey).toBool()) {
                oldRuntimeProviderIds.append(it.key());
                nextProviders.remove(it.key());
                nextTimers.remove(it.key());
            }
        }
    }
    for (auto it = candidate.providers.constBegin(); it != candidate.providers.constEnd(); ++it) {
        nextProviders.insert(it.key(), it.value());
    }
    for (auto it = candidateTimers.constBegin(); it != candidateTimers.constEnd(); ++it) {
        nextTimers.insert(it.key(), it.value());
    }

    const bool wasMonitoring = m_isMonitoring.load(std::memory_order_acquire);
    if (wasMonitoring) {
        stopMonitoring();
    }

    QMap<QString, std::shared_ptr<MonitorChannel>> previousChannels;
    QMap<QString, ProviderConfig> previousProviders;
    QMap<QString, QTimer*> previousTimers;
    {
        QWriteLocker channelLocker(&m_channelLock);
        m_channels.swap(nextChannels);
    }
    {
        QWriteLocker providerLocker(&m_providerLock);
        m_providers.swap(nextProviders);
        m_providerTimers.swap(nextTimers);
    }
    previousChannels.swap(nextChannels);
    previousProviders.swap(nextProviders);
    previousTimers.swap(nextTimers);

    for (const QString& providerId : oldRuntimeProviderIds) {
        if (QTimer* timer = previousTimers.take(providerId)) {
            timer->stop();
            timer->deleteLater();
        }
    }

    m_backendPointIds = candidate.backendPointIds;
    m_pointIdToChannel = candidate.pointIdToChannel;
    m_backendPointPeriodsMs = candidate.backendPointPeriodsMs;
    m_backendPointNextDueMs.clear();
    if (m_backend && !m_backendPointIds.isEmpty() && candidate.backendPollIntervalMs > 0) {
        m_backendPollTimer->setInterval(candidate.backendPollIntervalMs);
        qDebug() << "[MonitorManager] backend polling configured:"
                 << m_backendPointIds.size() << "points, interval="
                 << candidate.backendPollIntervalMs << "ms";
    } else {
        m_backendPollTimer->stop();
    }

    QPointer<MonitorDataProcessor> proc = m_dataProcessor;
    if (proc) {
        for (const QString& channelName : oldRuntimeChannelNames) {
            proc->clearChannelCache(channelName);
        }
        for (auto it = candidateChannels.constBegin(); it != candidateChannels.constEnd(); ++it) {
            proc->ensureChannel(it.key());
        }
    }

    if (!oldRuntimeChannelNames.isEmpty() || !candidateChannels.isEmpty()) {
        emit channelsChanged();
    }

    if (wasMonitoring) {
        startMonitoring();
    }

    qDebug() << "[MonitorManager] applyConfiguration 结束"
             << "success=true"
             << "channels=" << channelNames().size()
             << "providers=" << providerIds().size();

    return true;
}

} // namespace Monitor

