#include "MonitorManager.h"
#include "BackendSampler.h"
#include "communication/RuntimePointQualityMapper.h"
#include "communication/IDeviceBackend.h"

#include <QReadLocker>
#include <QWriteLocker>
#include <QDebug>
#include <QSet>
#include <QElapsedTimer>
#include <cmath>

#include <exception>
#include <utility>

namespace Monitor {

void MonitorManager::setDeviceBackend(IDeviceBackend* backend) { m_backendSampler->setDeviceBackend(backend); }
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

        // 若已存在，先停止旧定时器
        if (m_providerTimers.contains(config.id)) {
            m_providerTimers[config.id]->stop();
            m_providerTimers[config.id]->deleteLater();
            m_providerTimers.remove(config.id);
        }

        m_providers[config.id] = config;

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
void MonitorManager::startMonitoring()
{
    if (m_isMonitoring) {
        return;
    }

    m_isMonitoring = true;

    {
        QReadLocker locker(&m_providerLock);
        for (QTimer* timer : m_providerTimers) {
            timer->start();
        }
    }

    m_backendSampler->start();

    m_cleanupTimer->start();

    qDebug() << "[MonitorManager] 监控已启动";
}

void MonitorManager::stopMonitoring()
{
    const bool wasMonitoring = m_isMonitoring.load(std::memory_order_acquire);
    m_isMonitoring = false;
    m_backendSampler->stop();

    {
        QReadLocker locker(&m_providerLock);
        for (QTimer* timer : m_providerTimers) {
            timer->stop();
        }
    }


    m_cleanupTimer->stop();

    // 停止时 flush 一次，避免丢数据
    flushDatabaseLogging();

    if (wasMonitoring) {
        qDebug() << "[MonitorManager] 监控已停止";
    }
}
void MonitorManager::onBackendPollTimeout() { m_backendSampler->poll(); }
IDeviceBackend* MonitorManager::deviceBackend() const
{
    return m_backendSampler->backend();
}
void MonitorManager::onProviderTimeout()
{
    if (!m_isMonitoring.load(std::memory_order_acquire)) {
        return;
    }

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

    const auto reportProviderError = [this, &config, &providerId](const QString& error) {
        qWarning() << "[MonitorManager] 采集器异常:" << providerId << error;

        Sample sample;
        sample.channelName = config.channelName;
        sample.unit = config.unit;
        sample.timestamp = QDateTime::currentDateTimeUtc();
        sample.quality = RuntimePointQuality::Bad;
        sample.valueValid = false;
        sample.metadata[QStringLiteral("quality")] = runtimePointQualityToString(sample.quality);
        sample.metadata[QStringLiteral("valueValid")] = false;
        sample.metadata[QStringLiteral("source")] = QStringLiteral("provider");
        sample.metadata[QStringLiteral("error")] = error;
        recordSample(sample);

        if (config.errorHandler) {
            try {
                config.errorHandler(error);
            } catch (const std::exception& e) {
                qWarning() << "[MonitorManager] provider error handler failed:" << providerId << e.what();
            } catch (...) {
                qWarning() << "[MonitorManager] provider error handler failed:" << providerId;
            }
        }
    };

    try {
        double value = config.sampler();
        recordSample(config.channelName, value, config.unit);
    } catch (const std::exception& e) {
        reportProviderError(QString::fromStdString(e.what()));
    } catch (...) {
        reportProviderError(QStringLiteral("unknown exception"));
    }
}

} // namespace Monitor
