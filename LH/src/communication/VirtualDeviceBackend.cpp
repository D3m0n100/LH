// File: src/communication/VirtualDeviceBackend.cpp

#include "VirtualDeviceBackend.h"
#include "Common.h"

VirtualDeviceBackend::VirtualDeviceBackend(QObject* parent)
    : IDeviceBackend(parent)
{
}

// ── 点定义管理 ───────────────────────────────────────────

void VirtualDeviceBackend::loadPointDefinitions(const QList<RuntimePointDefinition>& defs)
{
    QMutexLocker lock(&m_mutex);
    for (const auto& def : defs) {
        m_definitions.insert(def.id, def);
        if (!m_values.contains(def.id))
            m_values.insert(def.id, def.defaultValue);
    }
}

void VirtualDeviceBackend::loadPointDefinitions(const RuntimePointTable& table)
{
    const auto snapshots = table.allSnapshots();
    QMutexLocker lock(&m_mutex);
    for (const auto& snap : snapshots) {
        m_definitions.insert(snap.definition.id, snap.definition);
        if (!m_values.contains(snap.definition.id))
            m_values.insert(snap.definition.id, snap.current.value);
    }
}

void VirtualDeviceBackend::clearPoints()
{
    QMutexLocker lock(&m_mutex);
    m_definitions.clear();
    m_values.clear();
}

// ── 连接生命周期 ─────────────────────────────────────────

bool VirtualDeviceBackend::connectBackend()
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_online)
            return true;
        m_online = true;
    }
    emit connectionStateChanged(true);
    LOG_INFO("[VirtualDeviceBackend] connected");
    return true;
}

void VirtualDeviceBackend::disconnectBackend()
{
    {
        QMutexLocker lock(&m_mutex);
        if (!m_online)
            return;
        m_online = false;
    }
    emit connectionStateChanged(false);
    LOG_INFO("[VirtualDeviceBackend] disconnected");
}

bool VirtualDeviceBackend::isOnline() const
{
    QMutexLocker lock(&m_mutex);
    return m_online;
}

// ── 点位读写 ─────────────────────────────────────────────

bool VirtualDeviceBackend::readPoints(const QStringList& pointIds,
                                      QHash<QString, QVariant>& values,
                                      QString* errorMessage)
{
    if (!checkOnline(errorMessage))
        return false;
    if (!checkReadFault(errorMessage))
        return false;

    QMutexLocker lock(&m_mutex);
    for (const auto& id : pointIds) {
        auto it = m_values.constFind(id);
        if (it != m_values.constEnd()) {
            values.insert(id, it.value());
        } else {
            if (errorMessage)
                *errorMessage = QStringLiteral("Point not found: %1").arg(id);
            return false;
        }
    }
    return true;
}

bool VirtualDeviceBackend::writePoints(const QHash<QString, QVariant>& writes,
                                       QString* errorMessage)
{
    if (!checkOnline(errorMessage))
        return false;
    if (!checkWriteFault(errorMessage))
        return false;

    QHash<QString, QVariant> changed;
    {
        QMutexLocker lock(&m_mutex);
        for (auto it = writes.constBegin(); it != writes.constEnd(); ++it) {
            auto defIt = m_definitions.constFind(it.key());
            if (defIt == m_definitions.constEnd()) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("Point not found: %1").arg(it.key());
                return false;
            }
            if (defIt->access == RuntimePointAccess::ReadOnly) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("Point is read-only: %1").arg(it.key());
                return false;
            }
            m_values.insert(it.key(), it.value());
            changed.insert(it.key(), it.value());
        }
    }

    if (!changed.isEmpty())
        emit pointsChanged(changed);
    return true;
}

// ── 下载 ─────────────────────────────────────────────────

bool VirtualDeviceBackend::downloadArtifact(const QString& artifactPath,
                                            const QVariantMap& options,
                                            QString* errorMessage)
{
    Q_UNUSED(options)

    if (!checkOnline(errorMessage))
        return false;
    if (!checkDownloadFault(errorMessage))
        return false;

    {
        QMutexLocker lock(&m_mutex);
        m_downloading = true;
        m_downloadPercent = 0;
        m_lastDownloadError.clear();
    }

    // 虚拟下载：直接标记完成
    {
        QMutexLocker lock(&m_mutex);
        m_downloading = false;
        m_downloadPercent = 100;
    }

    LOG_INFO(QStringLiteral("[VirtualDeviceBackend] download simulated: %1").arg(artifactPath));
    return true;
}

// ── 状态查询 ─────────────────────────────────────────────

QVariantMap VirtualDeviceBackend::queryStatus() const
{
    QMutexLocker lock(&m_mutex);
    QVariantMap status;
    status[QStringLiteral("online")] = m_online;
    status[QStringLiteral("backend")] = QStringLiteral("virtual");
    status[QStringLiteral("pointCount")] = m_definitions.size();
    status[QStringLiteral("downloading")] = m_downloading;
    status[QStringLiteral("downloadPercent")] = m_downloadPercent;
    if (!m_lastDownloadError.isEmpty())
        status[QStringLiteral("lastDownloadError")] = m_lastDownloadError;
    status[QStringLiteral("faultRead")] = m_faultRead;
    status[QStringLiteral("faultWrite")] = m_faultWrite;
    status[QStringLiteral("faultDownload")] = m_faultDownload;
    return status;
}

// ── 故障注入 ─────────────────────────────────────────────

void VirtualDeviceBackend::setFaultInjection(bool readFail, bool writeFail, bool downloadFail)
{
    QMutexLocker lock(&m_mutex);
    m_faultRead = readFail;
    m_faultWrite = writeFail;
    m_faultDownload = downloadFail;
}

void VirtualDeviceBackend::setSimulatedLatencyMs(int ms)
{
    QMutexLocker lock(&m_mutex);
    m_simulatedLatencyMs = ms;
}

// ── 内部辅助 ─────────────────────────────────────────────

bool VirtualDeviceBackend::checkOnline(QString* errorMessage) const
{
    QMutexLocker lock(&m_mutex);
    if (!m_online) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Device is offline");
        return false;
    }
    return true;
}

bool VirtualDeviceBackend::checkReadFault(QString* errorMessage) const
{
    QMutexLocker lock(&m_mutex);
    if (m_faultRead) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Injected read fault");
        return false;
    }
    return true;
}

bool VirtualDeviceBackend::checkWriteFault(QString* errorMessage) const
{
    QMutexLocker lock(&m_mutex);
    if (m_faultWrite) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Injected write fault");
        return false;
    }
    return true;
}

bool VirtualDeviceBackend::checkDownloadFault(QString* errorMessage) const
{
    QMutexLocker lock(&m_mutex);
    if (m_faultDownload) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Injected download fault");
        return false;
    }
    return true;
}
