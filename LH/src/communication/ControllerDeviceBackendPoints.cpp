// File: src/communication/ControllerDeviceBackendPoints.cpp

#include "ControllerDeviceBackend.h"

#include "ControllerDebugProtocol.h"

#include <QMutexLocker>
#include <QtGlobal>

namespace {
bool readPositiveInt(const QVariantMap& map, const QStringList& keys, int* value)
{
    for (const QString& key : keys) {
        bool ok = false;
        const int candidate = map.value(key).toInt(&ok);
        if (ok && candidate > 0) {
            *value = candidate;
            return true;
        }
    }
    return false;
}

bool readNonNegativeInt(const QVariantMap& map, const QStringList& keys, int* value)
{
    for (const QString& key : keys) {
        bool ok = false;
        const int candidate = map.value(key).toInt(&ok);
        if (ok && candidate >= 0) {
            *value = candidate;
            return true;
        }
    }
    return false;
}

bool isHoldingArea(const QString& area, const QString& registerType)
{
    const QString normalizedArea = area.trimmed().toLower();
    const QString normalizedType = registerType.trimmed().toLower();
    return normalizedArea == QStringLiteral("holding")
            || normalizedArea == QStringLiteral("holdingregister")
            || normalizedArea == QStringLiteral("holdingregisters")
            || normalizedArea == QStringLiteral("holding-register")
            || normalizedArea == QStringLiteral("holding-registers")
            || normalizedArea == QStringLiteral("hr")
            || normalizedType == QStringLiteral("holding")
            || normalizedType == QStringLiteral("holdingregister")
            || normalizedType == QStringLiteral("holdingregisters");
}

QVector<quint16> variantToRegisters(const QVariant& value)
{
    QVector<quint16> registers;
    if (value.type() == QVariant::List || value.type() == QVariant::StringList) {
        const QVariantList list = value.toList();
        registers.reserve(list.size());
        for (const QVariant& item : list) {
            registers.append(static_cast<quint16>(item.toUInt()));
        }
        return registers;
    }

    if (value.canConvert<QVector<quint16>>()) {
        return value.value<QVector<quint16>>();
    }

    registers.append(static_cast<quint16>(value.toUInt()));
    return registers;
}

QVariant registersToVariant(const QVector<quint16>& values)
{
    if (values.size() == 1) {
        return values.first();
    }

    QVariantList list;
    list.reserve(values.size());
    for (quint16 value : values) {
        list.append(value);
    }
    return list;
}
} // namespace

bool ControllerDeviceBackend::readPoints(const QStringList& pointIds,
                                         QHash<QString, QVariant>& values,
                                         QString* errorMessage,
                                         QHash<QString, CommError>* pointErrors)
{
    values.clear();
    if (pointErrors) {
        pointErrors->clear();
    }

    if (!ensureOnline(errorMessage)) {
        return false;
    }

    ControllerDebugStatus status;
    if (!m_client->readStatus(&status)) {
        const CommError err = currentDebugError(QStringLiteral("读取控制器状态失败。"));
        if (errorMessage) {
            *errorMessage = err.message;
        }
        setFailure(err.code, err.message, err.details);
        return false;
    }

    QHash<QString, QVariant> snapshotValues;
    snapshotValues.insert(QStringLiteral("controller.state"), status.state);
    snapshotValues.insert(QStringLiteral("controller.workMode"), status.workMode);
    snapshotValues.insert(QStringLiteral("controller.componentLine"), status.componentLine);
    snapshotValues.insert(QStringLiteral("controller.reset"), status.reset);
    snapshotValues.insert(QStringLiteral("controller.version"), status.version);

    bool hasError = false;
    CommError firstError;
    QHash<QString, CommError> localPointErrors;
    const QStringList requested = pointIds.isEmpty()
            ? QStringList(snapshotValues.keys())
            : pointIds;
    for (const QString& pointId : requested) {
        const auto it = snapshotValues.constFind(pointId);
        if (it != snapshotValues.constEnd()) {
            values.insert(pointId, it.value());
            continue;
        }

        const auto mappingIt = m_pointMappings.constFind(pointId);
        if (mappingIt != m_pointMappings.constEnd() && mappingIt.value().readable) {
            const PointMapping mapping = mappingIt.value();
            QVector<quint16> registers;
            if (m_client->readHoldingRegisters(mapping.deviceId, mapping.address, mapping.count, &registers)) {
                values.insert(pointId, registersToVariant(registers));
                continue;
            }

            CommError pointError = currentDebugError(QStringLiteral("控制器点位读取失败。"));
            localPointErrors.insert(pointId, pointError);
            if (!hasError) {
                firstError = pointError;
                hasError = true;
            }
            continue;
        }

        CommError pointError(CommProtocolType::ModbusRTU,
                             CommErrorCode::InvalidAddress,
                             QStringLiteral("控制器点位不存在：%1").arg(pointId));
        localPointErrors.insert(pointId, pointError);
        if (!hasError) {
            firstError = pointError;
            hasError = true;
        }
    }

    if (pointErrors) {
        *pointErrors = localPointErrors;
    }
    {
        QMutexLocker lock(&m_mutex);
        updateStatusCache(status);
    }

    if (hasError) {
        if (errorMessage) {
            *errorMessage = firstError.message;
        }
        setFailure(firstError.code, firstError.message);
        return false;
    }

    clearFailure();
    return true;
}

bool ControllerDeviceBackend::writePoints(const QHash<QString, QVariant>& writes,
                                          QString* errorMessage,
                                          QHash<QString, CommError>* pointErrors)
{
    if (pointErrors) {
        pointErrors->clear();
    }
    if (!ensureOnline(errorMessage)) {
        return false;
    }

    QHash<QString, QVariant> changed;
    QHash<QString, CommError> localPointErrors;
    CommError firstError;
    bool hasError = false;

    for (auto it = writes.constBegin(); it != writes.constEnd(); ++it) {
        bool ok = true;
        if (it.key() == QStringLiteral("controller.pause")) {
            ok = it.value().toBool() ? m_client->pause() : m_client->resume();
        } else if (it.key() == QStringLiteral("controller.step")) {
            ok = m_client->step();
        } else if (it.key() == QStringLiteral("controller.runToCursor")) {
            ok = m_client->runToCursor(boundedLine(it.value().toInt()));
        } else {
            const auto mappingIt = m_pointMappings.constFind(it.key());
            if (mappingIt != m_pointMappings.constEnd() && mappingIt.value().writable) {
                const PointMapping mapping = mappingIt.value();
                const QVector<quint16> registers = variantToRegisters(it.value());
                if (registers.isEmpty() || registers.size() > mapping.count) {
                    CommError pointError(CommProtocolType::ModbusRTU,
                                         CommErrorCode::InvalidParameter,
                                         QStringLiteral("控制器点位写入值数量不匹配：%1").arg(it.key()));
                    localPointErrors.insert(it.key(), pointError);
                    if (!hasError) {
                        firstError = pointError;
                        hasError = true;
                    }
                    continue;
                }
                ok = m_client->writeHoldingRegisters(mapping.deviceId, mapping.address, registers);
                if (ok) {
                    changed.insert(it.key(), it.value());
                    continue;
                }
            }

            CommError pointError(CommProtocolType::ModbusRTU,
                                 CommErrorCode::InvalidAddress,
                                 QStringLiteral("控制器点位不可写：%1").arg(it.key()));
            localPointErrors.insert(it.key(), pointError);
            if (!hasError) {
                firstError = pointError;
                hasError = true;
            }
            continue;
        }

        if (ok) {
            changed.insert(it.key(), it.value());
            continue;
        }

        CommError pointError = currentDebugError(QStringLiteral("控制器点位写入失败。"));
        localPointErrors.insert(it.key(), pointError);
        if (!hasError) {
            firstError = pointError;
            hasError = true;
        }
    }

    if (pointErrors) {
        *pointErrors = localPointErrors;
    }
    if (!changed.isEmpty()) {
        emit pointsChanged(changed);
    }
    if (hasError) {
        if (errorMessage) {
            *errorMessage = firstError.message;
        }
        setFailure(firstError.code, firstError.message, firstError.details);
        return false;
    }

    clearFailure();
    return true;
}

QVariantMap ControllerDeviceBackend::pointMappingSummary() const
{
    int readable = 0;
    int writable = 0;
    int targetMapped = 0;
    QVariantList mappedPoints;
    mappedPoints.reserve(m_pointMappings.size());
    QVariantList unmappedPoints;
    unmappedPoints.reserve(m_unmappedPointReasons.size());

    for (const PointMapping& mapping : m_pointMappings) {
        if (mapping.readable) {
            ++readable;
        }
        if (mapping.writable) {
            ++writable;
        }
        if (mapping.deviceId != deviceId()) {
            ++targetMapped;
        }

        QVariantMap item;
        item.insert(QStringLiteral("pointId"), mapping.pointId);
        item.insert(QStringLiteral("deviceId"), mapping.deviceId);
        item.insert(QStringLiteral("address"), mapping.address);
        item.insert(QStringLiteral("count"), mapping.count);
        item.insert(QStringLiteral("readable"), mapping.readable);
        item.insert(QStringLiteral("writable"), mapping.writable);
        mappedPoints.append(item);
    }

    for (auto it = m_unmappedPointReasons.constBegin(); it != m_unmappedPointReasons.constEnd(); ++it) {
        const RuntimePointDefinition point = m_pointDefinitions.value(it.key());
        QVariantMap item;
        item.insert(QStringLiteral("pointId"), it.key());
        item.insert(QStringLiteral("name"), point.name);
        item.insert(QStringLiteral("reason"), it.value());
        unmappedPoints.append(item);
    }

    QVariantMap summary;
    summary.insert(QStringLiteral("totalPoints"), m_pointDefinitions.size());
    summary.insert(QStringLiteral("mappedHoldingPoints"), m_pointMappings.size());
    summary.insert(QStringLiteral("readableMappedPoints"), readable);
    summary.insert(QStringLiteral("writableMappedPoints"), writable);
    summary.insert(QStringLiteral("targetMappedPoints"), targetMapped);
    summary.insert(QStringLiteral("unmappedPoints"), qMax(0, m_pointDefinitions.size() - m_pointMappings.size()));
    summary.insert(QStringLiteral("points"), mappedPoints);
    summary.insert(QStringLiteral("unmappedPointDetails"), unmappedPoints);
    return summary;
}

void ControllerDeviceBackend::loadPointDefinitions(const QList<RuntimePointDefinition>& points)
{
    m_pointDefinitions.clear();
    m_pointMappings.clear();
    m_unmappedPointReasons.clear();

    for (const RuntimePointDefinition& point : points) {
        if (point.id.isEmpty()) {
            continue;
        }

        m_pointDefinitions.insert(point.id, point);
        PointMapping mapping;
        QString rejectReason;
        if (parsePointMapping(point, &mapping, &rejectReason)) {
            m_pointMappings.insert(point.id, mapping);
        } else {
            m_unmappedPointReasons.insert(point.id, rejectReason);
        }
    }
}

bool ControllerDeviceBackend::parsePointMapping(const RuntimePointDefinition& point,
                                                PointMapping* mapping,
                                                QString* rejectReason) const
{
    if (!mapping) {
        return false;
    }

    QVariantMap address = point.addressing;
    for (auto it = point.metadata.constBegin(); it != point.metadata.constEnd(); ++it) {
        address.insert(it.key(), it.value());
    }

    const QString protocol = address.value(QStringLiteral("protocol")).toString().trimmed().toLower();
    if (!protocol.isEmpty()
            && protocol != QStringLiteral("none")
            && protocol != QStringLiteral("modbus")) {
        if (rejectReason) {
            *rejectReason = QStringLiteral("协议不是 modbus");
        }
        return false;
    }

    if (!isHoldingArea(address.value(QStringLiteral("area")).toString(),
                       address.value(QStringLiteral("registerType")).toString())) {
        if (rejectReason) {
            *rejectReason = QStringLiteral("不是 holding register 点位");
        }
        return false;
    }

    int regAddress = -1;
    if (!readNonNegativeInt(address,
                            {QStringLiteral("address"),
                             QStringLiteral("regAddress"),
                             QStringLiteral("registerAddress"),
                             QStringLiteral("offset")},
                             &regAddress)) {
        if (rejectReason) {
            *rejectReason = QStringLiteral("缺少寄存器地址");
        }
        return false;
    }

    int count = 1;
    readPositiveInt(address,
                    {QStringLiteral("elementCount"),
                     QStringLiteral("length"),
                     QStringLiteral("count"),
                     QStringLiteral("registerCount")},
                     &count);
    if (count <= 0 || count > 125) {
        if (rejectReason) {
            *rejectReason = QStringLiteral("寄存器数量超出 1..125");
        }
        return false;
    }

    int unitId = deviceId();
    readPositiveInt(address,
                    {QStringLiteral("unitId"),
                     QStringLiteral("slaveId"),
                     QStringLiteral("stationAddress"),
                     QStringLiteral("serverAddress")},
                     &unitId);
    if (!ControllerDebugProtocol::canReadDeviceId(unitId)) {
        if (rejectReason) {
            *rejectReason = QStringLiteral("装置号必须为 1..63");
        }
        return false;
    }

    mapping->pointId = point.id;
    mapping->deviceId = unitId;
    mapping->address = regAddress;
    mapping->count = count;
    mapping->readable = point.access != RuntimePointAccess::WriteOnly;
    mapping->writable = point.access != RuntimePointAccess::ReadOnly;
    return true;
}
