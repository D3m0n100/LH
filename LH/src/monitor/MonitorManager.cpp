// 文件：src/monitor/MonitorManager.cpp
// 监控管理器实现（性能优化版本）

#include "MonitorManager.h"
#include "MonitorChannel.h"
#include "MonitorDataProcessor.h"
#include "MonitorDataLogger.h"
#include "core/DataManager.h"

// 运行时配置类型（避免在头文件中引入，减少依赖）
#include "../common/ConfigTypes.h"

#include <QReadLocker>
#include <QWriteLocker>
#include <QDebug>
#include <QCoreApplication>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <random>

namespace Monitor {

namespace {

// 标记由 applyConfiguration() 生成/管理的资源，便于幂等清理。
// 注意：这是内部约定，不影响外部 API。
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
        // 初值取中间值附近，保持稳定。
        const double mid = (minValue + maxValue) * 0.5;
        value = mid;
        phase = (static_cast<double>(seed % 360) / 180.0) * kPi;
        digitalValue = (seed % 2) ? 1 : 0;
    }
};

static QString normalizeUnit(QString unit)
{
    unit = unit.trimmed();
    // 统一一些常见符号
    unit.replace(QString::fromUtf8("℃"), QString::fromUtf8("°C"));
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
    // unit 为空时，很多数字量输入默认不带单位。
    if (u.isEmpty() && (sid.contains("input") || sid.contains("output"))) {
        // 仍然需要避免把 analog_* 误判
        if (!sid.contains("analog")) {
            return true;
        }
    }
    // 典型布尔/开关单位
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
    // 优先使用 metadata 中的显式范围（与 snippets 模板字段对齐）
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
    } else if (u.contains("°c") || u.contains("c")) {
        // 以温度的常见范围为默认
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

    // 对 digital 明确锁定 0/1
    if (digital) {
        minV = 0.0;
        maxV = 1.0;
    }

    auto state = std::make_shared<DemoSamplerState>(seed, minV, maxV, digital);

    return [state]() -> double {
        state->tick++;

        if (state->isDigital) {
            // 每隔一段时间随机翻转一次
            if (state->tick % 25 == 0) {
                const int r = static_cast<int>(state->rng() % 4); // 25% 机会翻转
                if (r == 0) {
                    state->digitalValue = 1 - state->digitalValue;
                }
            }
            return static_cast<double>(state->digitalValue);
        }

        // 稳定曲线 + 小幅随机游走：target 用低频正弦，value 做惯性跟随。
        const double range = std::max(1e-9, state->maxValue - state->minValue);
        const double mid = (state->minValue + state->maxValue) * 0.5;
        const double amplitude = range * 0.25;

        state->phase += 0.08; // 控制变化速度
        const double target = mid + amplitude * std::sin(state->phase);

        // 噪声标准差随量程缩放，避免太“跳”
        const double noiseStd = range * 0.01;
        const double n = state->noise(state->rng) * noiseStd;

        // 惯性滤波：更平滑
        state->value = state->value * 0.92 + target * 0.08 + n;

        // 限幅
        if (state->value < state->minValue) state->value = state->minValue;
        if (state->value > state->maxValue) state->value = state->maxValue;
        return state->value;
    };
}

} // namespace

// ============================================================================
// 单例与构造/析构
// ============================================================================

MonitorManager& MonitorManager::instance()
{
    static MonitorManager s_instance;
    return s_instance;
}

MonitorManager::MonitorManager()
    : QObject(nullptr)
    , m_dataProcessor(nullptr)
    , m_cleanupTimer(new QTimer(this))
    , m_dataRetentionDays(DEFAULT_DATA_RETENTION_DAYS)
    , m_isMonitoring(false)
{
    setupCleanupTimer();

    // 监控采样落库：默认启用（可通过 setDatabaseLoggingEnabled 关闭）
    m_dataLogger = std::make_unique<MonitorDataLogger>(this);
    m_dataLogger->setEnabled(DEFAULT_DB_LOGGING_ENABLED);

    // 程序退出前尽量 flush，避免静态析构顺序导致 DataManager 已销毁
    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                this, [this]() { if (m_dataLogger) { m_dataLogger->shutdown(); } });
    }
}

MonitorManager::~MonitorManager()
{
    // 尽力将最后一批监控样本落库
    flushDatabaseLogging();
    if (m_dataLogger) {
        m_dataLogger->shutdown();
    }
    stopMonitoring();
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
    m_dataProcessor = processor;

    // 确保处理器中的通道与当前注册的通道同步
    if (m_dataProcessor) {
        QReadLocker locker(&m_channelLock);
        for (const QString& channelName : m_channels.keys()) {
            m_dataProcessor->ensureChannel(channelName);
        }
    }

    qDebug() << "[MonitorManager] 数据处理器已设置:"
             << (processor ? "启用增量分发" : "禁用增量分发");
}

// ============================================================================
// 监控采样落库开关
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
    if (!m_dataLogger || !m_dataLogger->isEnabled()) {
        return false;
    }
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
    if (m_dataProcessor) {
        m_dataProcessor->ensureChannel(config.name);
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

    // 清理数据处理器中的缓存
    if (m_dataProcessor) {
        m_dataProcessor->clearChannelCache(name);
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

    if (ch) {
        ch->appendSample(sample);
    }

    // 可选：落库（批量写入）
    if (m_dataLogger) {
        m_dataLogger->enqueueSample(sample);
    }

    // 增量分发到数据处理器
    dispatchToProcessor(sample.channelName, sample);

    // 发送信号
    emit sampleRecorded(sample.channelName, sample.value,
                        sample.unit, sample.timestamp);
}

void MonitorManager::recordSamples(const QString& channelName,
                                  const QList<Sample>& samples)
{
    if (samples.isEmpty()) {
        return;
    }

    // 规范化：确保样本的 channelName 与参数一致（便于落库/回放）
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
            ch->appendSample(sample);
        }
    }

    // 可选：落库（批量写入）
    if (m_dataLogger) {
        m_dataLogger->enqueueSamples(normalizedSamples);
    }

    // 批量增量分发
    dispatchToProcessor(channelName, normalizedSamples);

    // 发送信号
    emit samplesRecorded(channelName, normalizedSamples.size());
}

QList<Sample> MonitorManager::history(const QString& channelName, int count) const
{
    QReadLocker locker(&m_channelLock);
    auto ch = m_channels.value(channelName);
    if (!ch) {
        return {};
    }
    return ch->history(count);
}

QList<Sample> MonitorManager::history(const QString& channelName,
                                      const QDateTime& start,
                                      const QDateTime& end) const
{
    QReadLocker locker(&m_channelLock);
    auto ch = m_channels.value(channelName);
    if (!ch) {
        return {};
    }
    return ch->history(start, end);
}

QList<Sample> MonitorManager::historyFromDatabase(const QString& channelName, int count) const
{
    if (count <= 0) {
        return {};
    }

    auto& dm = DataManager::instance();
    if (!dm.isInitialized()) {
        return {};
    }

    QList<RuntimeRecord> records = dm.getLatestRecords(channelName, count);
    QList<Sample> out;

    for (const auto& r : records) {
        Sample s;
        s.channelName = channelName;
        s.value = r.value;
        s.unit = r.unit;
        s.timestamp = r.timestamp.isValid() ? r.timestamp : QDateTime();
        out.append(s);
    }

    // getLatestRecords 按 DESC 返回，这里统一转为 ASC
    std::reverse(out.begin(), out.end());
    return out;
}

QList<Sample> MonitorManager::historyFromDatabase(const QString& channelName,
                                                  const QDateTime& start,
                                                  const QDateTime& end) const
{
    auto& dm = DataManager::instance();
    if (!dm.isInitialized()) {
        return {};
    }

    const QDateTime s = start.isValid() ? start.toUTC() : QDateTime::fromMSecsSinceEpoch(0, Qt::UTC);
    const QDateTime e = end.isValid() ? end.toUTC() : QDateTime::currentDateTimeUtc();

    QList<RuntimeRecord> records = dm.queryHistory(channelName, s, e);
    QList<Sample> out;

    for (const auto& r : records) {
        Sample srec;
        srec.channelName = channelName;
        srec.value = r.value;
        srec.unit = r.unit;
        srec.timestamp = r.timestamp.isValid() ? r.timestamp : QDateTime();
        out.append(srec);
    }
    return out;
}


// ============================================================================
// 采集器管理
// ============================================================================

bool MonitorManager::registerProvider(const ProviderConfig& config)
{
    if (config.id.isEmpty() || config.channelName.isEmpty()) {
        qWarning() << "[MonitorManager] 无法注册采集器：ID 或通道名为空";
        return false;
    }

    // 确保通道存在
    if (!hasChannel(config.channelName)) {
        ChannelConfig channelConfig;
        channelConfig.name = config.channelName;
        channelConfig.unit = config.unit;
        if (config.channelOverride.has_value()) {
            channelConfig = config.channelOverride.value();
        }
        registerChannel(channelConfig);
    }

    {
        QWriteLocker locker(&m_providerLock);

        // 如果已存在，先停止旧的定时器
        if (m_providerTimers.contains(config.id)) {
            m_providerTimers[config.id]->stop();
            m_providerTimers[config.id]->deleteLater();
            m_providerTimers.remove(config.id);
        }

        m_providers[config.id] = config;

        // 创建采样定时器
        if (config.sampler && config.periodMs > 0) {
            QTimer* timer = new QTimer(this);
            timer->setProperty("providerId", config.id);
            connect(timer, &QTimer::timeout, this, &MonitorManager::onProviderTimeout);
            timer->setInterval(config.periodMs);
            m_providerTimers[config.id] = timer;

            if (m_isMonitoring) {
                timer->start();
            }
        }
    }

    qDebug() << "[MonitorManager] 注册采集器:" << config.id
             << "-> 通道:" << config.channelName;
    return true;
}

bool MonitorManager::unregisterProvider(const QString& providerId)
{
    QWriteLocker locker(&m_providerLock);

    if (!m_providers.contains(providerId)) {
        return false;
    }

    if (m_providerTimers.contains(providerId)) {
        m_providerTimers[providerId]->stop();
        m_providerTimers[providerId]->deleteLater();
        m_providerTimers.remove(providerId);
    }

    m_providers.remove(providerId);

    qDebug() << "[MonitorManager] 注销采集器:" << providerId;
    return true;
}

bool MonitorManager::hasProvider(const QString& providerId) const
{
    QReadLocker locker(&m_providerLock);
    return m_providers.contains(providerId);
}

QStringList MonitorManager::providerIds() const
{
    QReadLocker locker(&m_providerLock);
    return m_providers.keys();
}

// ============================================================================
// 监控控制
// ============================================================================

void MonitorManager::startMonitoring()
{
    if (m_isMonitoring) {
        return;
    }

    m_isMonitoring = true;

    // 启动所有采集器定时器
    {
        QReadLocker locker(&m_providerLock);
        for (QTimer* timer : m_providerTimers) {
            timer->start();
        }
    }

    // 启动清理定时器
    m_cleanupTimer->start();

    qDebug() << "[MonitorManager] 监控已启动";
}

void MonitorManager::stopMonitoring()
{
    if (!m_isMonitoring) {
        return;
    }

    m_isMonitoring = false;

    // 停止所有采集器定时器
    {
        QReadLocker locker(&m_providerLock);
        for (QTimer* timer : m_providerTimers) {
            timer->stop();
        }
    }

    // 停止清理定时器
    m_cleanupTimer->stop();

    // 停止监控时尽力 flush 一次（避免丢数据）
    flushDatabaseLogging();

    qDebug() << "[MonitorManager] 监控已停止";
}

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

    if (m_dataProcessor) {
        m_dataProcessor->clearChannelCache(channelName);
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

    if (m_dataProcessor) {
        m_dataProcessor->clearAllCache();
    }
}

// ============================================================================
// 私有槽函数
// ============================================================================

void MonitorManager::onCleanupTimeout()
{
    QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-m_dataRetentionDays);

    QReadLocker locker(&m_channelLock);
    for (auto& ch : m_channels) {
        ch->purgeOlderThan(cutoff);
    }

    qDebug() << "[MonitorManager] 数据清理完成，截止时间:" << cutoff;
}

void MonitorManager::onProviderTimeout()
{
    QTimer* timer = qobject_cast<QTimer*>(sender());
    if (!timer) {
        return;
    }

    QString providerId = timer->property("providerId").toString();

    ProviderConfig config;
    {
        QReadLocker locker(&m_providerLock);
        if (!m_providers.contains(providerId)) {
            return;
        }
        config = m_providers[providerId];
    }

    if (!config.sampler) {
        return;
    }

    try {
        double value = config.sampler();
        recordSample(config.channelName, value, config.unit);
    } catch (const std::exception& e) {
        qWarning() << "[MonitorManager] 采集器异常:" << providerId << e.what();
        if (config.errorHandler) {
            config.errorHandler(QString::fromStdString(e.what()));
        }
    }
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
    if (!m_dataProcessor) {
        return;
    }

    // 增量追加到数据处理器
    m_dataProcessor->appendSample(channelName, sample);
}

void MonitorManager::dispatchToProcessor(const QString& channelName,
                                         const QList<Sample>& samples)
{
    if (!m_dataProcessor || samples.isEmpty()) {
        return;
    }

    // 批量增量追加到数据处理器
    m_dataProcessor->appendSamples(channelName, samples);
}

bool MonitorManager::applyConfiguration(const ProjectRuntimeConfig& config)
{
    qDebug() << "[MonitorManager] applyConfiguration 开始"
             << "project=" << config.projectName
             << "providers=" << config.providers.size()
             << "mappings=" << config.dslMappings.size();

    // applyConfiguration 不应隐式改变外部“是否在监控”的语义；但如果当前正在采样，
    // 为避免定时器在变更过程中触发，这里临时停止并在最后恢复。
    const bool wasMonitoring = m_isMonitoring;
    if (wasMonitoring) {
        stopMonitoring();
    }

    bool success = true;
    bool channelsTouched = false;

    // ---------------------------------------------------------------------
    // 1) 清理旧的 runtime-managed provider/channel（幂等）
    // ---------------------------------------------------------------------

    QStringList providersToRemove;
    {
        QReadLocker locker(&m_providerLock);
        for (auto it = m_providers.constBegin(); it != m_providers.constEnd(); ++it) {
            if (it.value().metadata.value(kRuntimeManagedKey).toBool()) {
                providersToRemove.push_back(it.key());
            }
        }
    }
    for (const QString& id : providersToRemove) {
        if (!unregisterProvider(id)) {
            // 理论上不会失败（因为列表来自现存项），但仍保留日志。
            qWarning() << "[MonitorManager] 清理旧采集器失败:" << id;
            success = false;
        }
    }

    QStringList channelsToRemove;
    {
        QReadLocker locker(&m_channelLock);
        for (auto it = m_channels.constBegin(); it != m_channels.constEnd(); ++it) {
            const auto& ch = it.value();
            if (!ch) {
                continue;
            }
            if (ch->config().metadata.value(kRuntimeManagedKey).toBool()) {
                channelsToRemove.push_back(it.key());
            }
        }
    }
    for (const QString& name : channelsToRemove) {
        if (!removeChannel(name)) {
            // 同上：尽量给出清晰日志
            qWarning() << "[MonitorManager] 清理旧通道失败:" << name;
            success = false;
        } else {
            channelsTouched = true;
        }
    }

    // ---------------------------------------------------------------------
    // 2) 基于 dslMappings/providers 注册通道
    // ---------------------------------------------------------------------

    QSet<QString> registeredChannelNames;
    QHash<QString, DslMappingEntry> mappingById;
    QHash<QString, DslMappingEntry> mappingByChannel;
    mappingById.reserve(config.dslMappings.size());
    mappingByChannel.reserve(config.dslMappings.size());

    for (const auto& entry : config.dslMappings) {
        mappingById.insert(entry.id, entry);
        if (!entry.channelName.isEmpty() && !mappingByChannel.contains(entry.channelName)) {
            mappingByChannel.insert(entry.channelName, entry);
        }

        if (entry.channelName.trimmed().isEmpty()) {
            qWarning() << "[MonitorManager] dslMappings 条目缺少 channelName，id=" << entry.id
                       << "snippetId=" << entry.snippetId << "line=" << entry.lineNumber;
            success = false;
            continue;
        }

        ChannelConfig chCfg;
        chCfg.name = entry.channelName.trimmed();
        chCfg.unit = entry.unit;
        chCfg.displayName = !entry.snippetName.trimmed().isEmpty()
                                ? entry.snippetName.trimmed()
                                : chCfg.name;
        chCfg.maxSamples = Limits::DEFAULT_RING_BUFFER_CAPACITY;

        // 填充一些可追溯元数据，便于 UI/导出/调试。
        chCfg.metadata = entry.metadata;
        chCfg.metadata["mappingId"] = entry.id;
        chCfg.metadata["snippetId"] = entry.snippetId;
        chCfg.metadata["snippetName"] = entry.snippetName;
        chCfg.metadata["signalPath"] = entry.signalPath;
        chCfg.metadata["lineNumber"] = entry.lineNumber;
        chCfg.metadata["periodMs"] = entry.periodMs;
        chCfg.metadata["projectName"] = config.projectName;
        chCfg.metadata[kRuntimeManagedKey] = true;

        if (!registerChannel(chCfg)) {
            qWarning() << "[MonitorManager] 注册通道失败:" << chCfg.name;
            success = false;
            continue;
        }
        registeredChannelNames.insert(chCfg.name);
        channelsTouched = true;
    }

    // providers 中如果引用了额外通道，也一并注册（保证 UI 能看到通道）
    for (const auto& p : config.providers) {
        const QString chName = p.channelName.trimmed();
        if (chName.isEmpty()) {
            qWarning() << "[MonitorManager] providers 条目缺少 channelName，id=" << p.id;
            success = false;
            continue;
        }
        if (registeredChannelNames.contains(chName)) {
            continue;
        }

        ChannelConfig chCfg;
        chCfg.name = chName;
        chCfg.unit = p.unit;
        chCfg.displayName = chName;
        chCfg.maxSamples = Limits::DEFAULT_RING_BUFFER_CAPACITY;
        chCfg.metadata = p.metadata;
        chCfg.metadata["projectName"] = config.projectName;
        chCfg.metadata["periodMs"] = p.periodMs;
        chCfg.metadata[kRuntimeManagedKey] = true;

        // 若能匹配到 mapping，则补齐显示名/额外元数据
        if (mappingById.contains(p.id)) {
            const auto& m = mappingById[p.id];
            if (!m.snippetName.trimmed().isEmpty()) {
                chCfg.displayName = m.snippetName.trimmed();
            }
            chCfg.metadata["mappingId"] = m.id;
            chCfg.metadata["snippetId"] = m.snippetId;
            chCfg.metadata["snippetName"] = m.snippetName;
            chCfg.metadata["signalPath"] = m.signalPath;
            chCfg.metadata["lineNumber"] = m.lineNumber;
        } else if (mappingByChannel.contains(chName)) {
            const auto& m = mappingByChannel[chName];
            if (!m.snippetName.trimmed().isEmpty()) {
                chCfg.displayName = m.snippetName.trimmed();
            }
            chCfg.metadata["mappingId"] = m.id;
            chCfg.metadata["snippetId"] = m.snippetId;
            chCfg.metadata["snippetName"] = m.snippetName;
            chCfg.metadata["signalPath"] = m.signalPath;
            chCfg.metadata["lineNumber"] = m.lineNumber;
        }

        if (!registerChannel(chCfg)) {
            qWarning() << "[MonitorManager] 注册通道失败:" << chCfg.name;
            success = false;
            continue;
        }
        registeredChannelNames.insert(chCfg.name);
        channelsTouched = true;
    }

    // ---------------------------------------------------------------------
    // 3) 基于 providers 注册采集器（必须有可运行 sampler）
    // ---------------------------------------------------------------------

    QList<MonitorProviderRuntimeConfig> providers = config.providers;

    // providers 为空但 mappings 非空时，从 mappings 派生 providers
    if (providers.isEmpty() && !config.dslMappings.isEmpty()) {
        providers.reserve(config.dslMappings.size());
        for (const auto& entry : config.dslMappings) {
            if (entry.channelName.trimmed().isEmpty()) {
                continue;
            }
            MonitorProviderRuntimeConfig p;
            p.id = entry.id.trimmed().isEmpty() ? entry.channelName.trimmed() : entry.id.trimmed();
            p.channelName = entry.channelName.trimmed();
            p.unit = entry.unit;
            p.periodMs = entry.periodMs > 0 ? entry.periodMs : 20;
            p.priority = 128;
            p.metadata = entry.metadata;
            // 便于 sampler 推断行为
            p.metadata["snippetId"] = entry.snippetId;
            p.metadata["snippetName"] = entry.snippetName;
            p.metadata["mappingId"] = entry.id;
            p.metadata["signalPath"] = entry.signalPath;
            providers.push_back(p);
        }
    }

    for (const auto& p : providers) {
        if (p.id.trimmed().isEmpty()) {
            qWarning() << "[MonitorManager] provider 条目缺少 id，channel=" << p.channelName;
            success = false;
            continue;
        }
        if (p.channelName.trimmed().isEmpty()) {
            qWarning() << "[MonitorManager] provider 条目缺少 channelName，id=" << p.id;
            success = false;
            continue;
        }

        ProviderConfig pc;
        pc.id = p.id.trimmed();
        pc.channelName = p.channelName.trimmed();
        pc.unit = p.unit;
        pc.periodMs = (p.periodMs > 0) ? p.periodMs : 20;
        pc.priority = p.priority;
        pc.metadata = p.metadata;
        pc.metadata["projectName"] = config.projectName;
        pc.metadata[kRuntimeManagedKey] = true;

        // 尝试关联到 mapping，以便 demo sampler 根据 snippetId 做出更合理行为
        QString snippetId;
        if (mappingById.contains(pc.id)) {
            const auto& m = mappingById[pc.id];
            snippetId = m.snippetId;
            pc.metadata["snippetId"] = m.snippetId;
            pc.metadata["snippetName"] = m.snippetName;
            pc.metadata["mappingId"] = m.id;
            pc.metadata["signalPath"] = m.signalPath;
            pc.metadata["lineNumber"] = m.lineNumber;
        } else if (mappingByChannel.contains(pc.channelName)) {
            const auto& m = mappingByChannel[pc.channelName];
            snippetId = m.snippetId;
            pc.metadata["snippetId"] = m.snippetId;
            pc.metadata["snippetName"] = m.snippetName;
            pc.metadata["mappingId"] = m.id;
            pc.metadata["signalPath"] = m.signalPath;
            pc.metadata["lineNumber"] = m.lineNumber;
        } else {
            // fallback: 尝试从 metadata 读取
            snippetId = pc.metadata.value("snippetId").toString();
        }

        pc.sampler = makeDemoSampler(pc.id, pc.channelName, pc.unit, snippetId, pc.metadata);
        pc.errorHandler = [pid = pc.id](const QString& err) {
            qWarning() << "[MonitorManager] provider error:" << pid << err;
        };

        if (!registerProvider(pc)) {
            qWarning() << "[MonitorManager] 注册采集器失败:" << pc.id
                       << "->" << pc.channelName;
            success = false;
            continue;
        }
    }

    // ---------------------------------------------------------------------
    // 3.5) 将变量 / 参数 / 资源作为可监控对象挂到监控系统
    // ---------------------------------------------------------------------

    auto registerObjectChannel = [&](const QString& channelName,
                                     const QString& displayName,
                                     const QString& unit,
                                     MonitorObjectKind kind,
                                     const QString& sourceName,
                                     bool editable,
                                     const QVariantMap& extraMeta) {
        if (channelName.trimmed().isEmpty()) {
            return false;
        }

        ChannelConfig chCfg;
        chCfg.name = channelName.trimmed();
        chCfg.displayName = displayName.trimmed().isEmpty() ? chCfg.name : displayName.trimmed();
        chCfg.unit = unit;
        chCfg.maxSamples = Limits::DEFAULT_RING_BUFFER_CAPACITY;
        chCfg.objectKind = kind;
        chCfg.sourceName = sourceName;
        chCfg.editable = editable;
        chCfg.defaultVisible = false;
        chCfg.metadata = extraMeta;
        chCfg.metadata["projectName"] = config.projectName;
        chCfg.metadata["objectKind"] = static_cast<int>(kind);
        chCfg.metadata["sourceName"] = sourceName;
        chCfg.metadata["editable"] = editable;
        chCfg.metadata[kRuntimeManagedKey] = true;
        return registerChannel(chCfg);
    };

    for (const auto& v : config.variables) {
        QVariantMap meta = v.metadata;
        meta["variableId"] = v.id;
        meta["variableName"] = v.name;
        meta["dataType"] = v.dataType;
        meta["scope"] = v.scope;
        meta["binding"] = v.binding;
        meta["defaultValue"] = v.defaultValue;
        if (!registerObjectChannel(QStringLiteral("var::%1").arg(v.name),
                                   v.name,
                                   QStringLiteral("var"),
                                   MonitorObjectKind::Variable,
                                   v.name,
                                   !v.readOnly,
                                   meta)) {
            success = false;
        } else {
            channelsTouched = true;
        }
    }

    for (const auto& p : config.parameters) {
        QVariantMap meta = p.metadata;
        meta["parameterId"] = p.id;
        meta["parameterName"] = p.name;
        meta["dataType"] = p.dataType;
        meta["defaultValue"] = p.defaultValue;
        meta["minValue"] = p.minValue;
        meta["maxValue"] = p.maxValue;
        meta["unit"] = p.unit;
        meta["onlineEditable"] = p.onlineEditable;
        if (!registerObjectChannel(QStringLiteral("param::%1").arg(p.name),
                                   p.name,
                                   p.unit,
                                   MonitorObjectKind::Parameter,
                                   p.name,
                                   p.onlineEditable,
                                   meta)) {
            success = false;
        } else {
            channelsTouched = true;
        }
    }

    for (const auto& r : config.resources) {
        QVariantMap meta = r.metadata;
        meta["resourceId"] = r.id;
        meta["resourceType"] = r.resourceType;
        meta["resourceName"] = r.resourceName;
        meta["channel"] = r.channel;
        meta["owner"] = r.owner;
        meta["exclusive"] = r.exclusive;
        if (!registerObjectChannel(QStringLiteral("res::%1").arg(r.channel.isEmpty() ? r.resourceName : r.channel),
                                   r.resourceName.isEmpty() ? r.channel : r.resourceName,
                                   r.resourceType,
                                   MonitorObjectKind::Resource,
                                   r.resourceName.isEmpty() ? r.channel : r.resourceName,
                                   false,
                                   meta)) {
            success = false;
        } else {
            channelsTouched = true;
        }
    }

    // ---------------------------------------------------------------------
    // 4) 通知 UI 刷新 & 恢复监控状态
    // ---------------------------------------------------------------------

    if (channelsTouched) {
        emit channelsChanged();
    }

    if (wasMonitoring) {
        startMonitoring();
    }

    qDebug() << "[MonitorManager] applyConfiguration 结束"
             << "success=" << success
             << "channels=" << channelNames().size()
             << "providers=" << providerIds().size();

    return success;
}

} // namespace Monitor
