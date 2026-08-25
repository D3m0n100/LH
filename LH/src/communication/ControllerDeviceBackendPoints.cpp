// File: src/communication/ControllerDeviceBackendPoints.cpp

#include "ControllerDeviceBackend.h"

#include "ControllerDebugProtocol.h"

#include <QMetaType>
#include <QMutexLocker>
#include <QtGlobal>

#include <cmath>
#include <limits>

namespace {
class BackendOperationGuard
{
public:
    explicit BackendOperationGuard(QMutex* mutex)
        : m_mutex(mutex)
    {
    }

    ~BackendOperationGuard()
    {
        if (m_locked) {
            m_mutex->unlock();
        }
    }

    bool tryLock()
    {
        m_locked = m_mutex && m_mutex->tryLock();
        return m_locked;
    }

private:
    QMutex* m_mutex = nullptr;
    bool m_locked = false;
};

QString busyMessage()
{
    return QStringLiteral("控制器后端正忙，点位操作被拒绝。");
}

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

bool variantToStrictPositiveInt(const QVariant& value, int* result)
{
    if (value.userType() == QMetaType::QString) {
        const QString text = value.toString().trimmed();
        if (text.isEmpty()) {
            return false;
        }
        bool ok = false;
        const double number = text.toDouble(&ok);
        if (!ok || !std::isfinite(number) || std::trunc(number) != number
                || number <= 0.0 || number > std::numeric_limits<int>::max()) {
            return false;
        }
        *result = static_cast<int>(number);
        return true;
    }
    switch (value.userType()) {
    case QMetaType::Char:
    case QMetaType::UChar:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Float:
    case QMetaType::Double:
        break;
    default:
        return false;
    }
    bool ok = false;
    const double number = value.toDouble(&ok);
    if (!ok || !std::isfinite(number) || std::trunc(number) != number
            || number <= 0.0 || number > std::numeric_limits<int>::max()) {
        return false;
    }
    *result = static_cast<int>(number);
    return true;
}

bool readExplicitDeviceId(const QVariantMap& metadata,
                          int* value,
                          bool* present,
                          QString* errorMessage)
{
    *present = false;
    QString firstPresentKey;
    const QStringList keys {
        QStringLiteral("unitId"),
        QStringLiteral("slaveId"),
        QStringLiteral("stationAddress"),
        QStringLiteral("serverAddress")
    };
    for (const QString& key : keys) {
        if (!metadata.contains(key)) {
            continue;
        }
        int candidate = 0;
        if (!variantToStrictPositiveInt(metadata.value(key), &candidate)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("显式装置号 %1 必须为严格正整数").arg(key);
            }
            return false;
        }
        if (!ControllerDebugProtocol::canReadDeviceId(candidate)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("显式装置号 %1 超出当前可读范围").arg(key);
            }
            return false;
        }
        if (*present && *value != candidate) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("显式装置号别名冲突：%1=%2 与 %3=%4")
                        .arg(firstPresentKey)
                        .arg(*value)
                        .arg(key)
                        .arg(candidate);
            }
            return false;
        }
        if (!*present) {
            firstPresentKey = key;
        }
        *value = candidate;
        *present = true;
    }
    return true;
}

bool readOptionalPositiveInt(const QVariantMap& map,
                             const QStringList& keys,
                             int* value,
                             bool* present,
                             QString* errorMessage)
{
    *present = false;
    QString firstPresentKey;
    for (const QString& key : keys) {
        if (!map.contains(key)) {
            continue;
        }
        int candidate = 0;
        if (!variantToStrictPositiveInt(map.value(key), &candidate)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("%1 必须为正整数").arg(key);
            }
            return false;
        }
        if (*present && *value != candidate) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("寄存器数量字段冲突：%1=%2 与 %3=%4")
                        .arg(firstPresentKey)
                        .arg(*value)
                        .arg(key)
                        .arg(candidate);
            }
            return false;
        }
        if (!*present) {
            firstPresentKey = key;
        }
        *value = candidate;
        *present = true;
    }
    return true;
}

CommError codecPointError(const QString& pointId,
                          const QString& action,
                          const QString& details)
{
    return CommError(CommProtocolType::ModbusRTU,
                     CommErrorCode::InvalidParameter,
                     QStringLiteral("控制器点位%1失败：%2").arg(action, pointId),
                     details);
}

CommError pointAccessError(const QString& pointId, const QString& action)
{
    return CommError(CommProtocolType::ModbusRTU,
                     CommErrorCode::PermissionDenied,
                     QStringLiteral("控制器点位%1权限不足：%2").arg(action, pointId));
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

    BackendOperationGuard operation(&m_operationMutex);
    if (!operation.tryLock()) {
        const QString message = busyMessage();
        if (errorMessage) {
            *errorMessage = message;
        }
        setFailure(CommErrorCode::DeviceBusy, message);
        if (pointErrors) {
            const CommError error(CommProtocolType::ModbusRTU,
                                  CommErrorCode::DeviceBusy,
                                  message);
            for (const QString& pointId : pointIds) {
                pointErrors->insert(pointId, error);
            }
        }
        return false;
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
        if (mappingIt != m_pointMappings.constEnd()) {
            const PointMapping mapping = mappingIt.value();
            if (!mapping.readable) {
                const CommError pointError = pointAccessError(pointId, QStringLiteral("读取"));
                localPointErrors.insert(pointId, pointError);
                if (!hasError) {
                    firstError = pointError;
                    hasError = true;
                }
                continue;
            }
            QVector<quint16> registers;
            if (m_client->readHoldingRegisters(mapping.deviceId, mapping.address, mapping.count, &registers)) {
                QVariant decoded;
                QString codecError;
                if (RuntimePointRegisterCodec::decode(mapping.codec,
                                                      registers,
                                                      &decoded,
                                                      &codecError)) {
                    values.insert(pointId, decoded);
                    continue;
                }
                const CommError pointError = codecPointError(
                        pointId,
                        QStringLiteral("读取解码"),
                        QStringLiteral("address=%1 registerCount=%2; %3")
                                .arg(mapping.address)
                                .arg(mapping.count)
                                .arg(codecError));
                localPointErrors.insert(pointId, pointError);
                if (!hasError) {
                    firstError = pointError;
                    hasError = true;
                }
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

        const QString rejectReason = m_unmappedPointReasons.value(pointId);
        const bool invalidConfig = m_pointDefinitions.contains(pointId);
        const CommError pointError(
                CommProtocolType::ModbusRTU,
                invalidConfig ? CommErrorCode::InvalidConfig : CommErrorCode::InvalidAddress,
                invalidConfig
                        ? QStringLiteral("控制器点位映射配置无效：%1").arg(pointId)
                        : QStringLiteral("控制器点位不存在：%1").arg(pointId),
                invalidConfig ? rejectReason : QString());
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
        setFailure(firstError.code, firstError.message, firstError.details);
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
    BackendOperationGuard operation(&m_operationMutex);
    if (!operation.tryLock()) {
        const QString message = busyMessage();
        if (errorMessage) {
            *errorMessage = message;
        }
        setFailure(CommErrorCode::DeviceBusy, message);
        if (pointErrors) {
            const CommError error(CommProtocolType::ModbusRTU,
                                  CommErrorCode::DeviceBusy,
                                  message);
            for (auto it = writes.constBegin(); it != writes.constEnd(); ++it) {
                pointErrors->insert(it.key(), error);
            }
        }
        return false;
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
            if (mappingIt != m_pointMappings.constEnd()) {
                const PointMapping mapping = mappingIt.value();
                if (!mapping.writable) {
                    const CommError pointError = pointAccessError(it.key(), QStringLiteral("写入"));
                    localPointErrors.insert(it.key(), pointError);
                    if (!hasError) {
                        firstError = pointError;
                        hasError = true;
                    }
                    continue;
                }
                QVector<quint16> registers;
                QString codecError;
                if (!RuntimePointRegisterCodec::encode(mapping.codec,
                                                       it.value(),
                                                       &registers,
                                                       &codecError)) {
                    const CommError pointError = codecPointError(
                            it.key(),
                            QStringLiteral("写入编码"),
                            QStringLiteral("address=%1 registerCount=%2; %3")
                                    .arg(mapping.address)
                                    .arg(mapping.count)
                                    .arg(codecError));
                    localPointErrors.insert(it.key(), pointError);
                    if (!hasError) {
                        firstError = pointError;
                        hasError = true;
                    }
                    continue;
                }
                if (registers.size() != mapping.count) {
                    const CommError pointError = codecPointError(
                            it.key(),
                            QStringLiteral("写入编码"),
                            QStringLiteral("寄存器数量不匹配：expected=%1 got=%2")
                                    .arg(mapping.count)
                                    .arg(registers.size()));
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
                const CommError pointError = currentDebugError(QStringLiteral("控制器点位写入失败。"));
                localPointErrors.insert(it.key(), pointError);
                if (!hasError) {
                    firstError = pointError;
                    hasError = true;
                }
                continue;
            }

            const QString rejectReason = m_unmappedPointReasons.value(it.key());
            const bool invalidConfig = m_pointDefinitions.contains(it.key());
            const CommError pointError(
                    CommProtocolType::ModbusRTU,
                    invalidConfig ? CommErrorCode::InvalidConfig : CommErrorCode::InvalidAddress,
                    invalidConfig
                            ? QStringLiteral("控制器点位映射配置无效或不可写：%1").arg(it.key())
                            : QStringLiteral("控制器点位不可写：%1").arg(it.key()),
                    invalidConfig ? rejectReason : QString());
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

    RuntimePointRegisterCodecSpec codec;
    QString codecReason;
    if (!RuntimePointRegisterCodec::buildSpec(point, &codec, &codecReason)) {
        if (rejectReason) {
            *rejectReason = QStringLiteral("codec: %1").arg(codecReason);
        }
        return false;
    }

    int regAddress = -1;
    QStringList addressKeys {
        QStringLiteral("address"),
        QStringLiteral("regAddress"),
        QStringLiteral("registerAddress")
    };
    if (!readNonNegativeInt(address, addressKeys, &regAddress)) {
        if (rejectReason) {
            *rejectReason = QStringLiteral("缺少寄存器地址");
        }
        return false;
    }

    int count = codec.registerCount;
    if (codec.isTyped()) {
        int explicitRegisterCount = 0;
        bool hasExplicitRegisterCount = false;
        QString countReason;
        if (!readOptionalPositiveInt(address,
                                     {QStringLiteral("registerCount"),
                                      QStringLiteral("count"),
                                      QStringLiteral("length")},
                                     &explicitRegisterCount,
                                     &hasExplicitRegisterCount,
                                     &countReason)) {
            if (rejectReason) {
                *rejectReason = QStringLiteral("codec: %1").arg(countReason);
            }
            return false;
        }
        if (hasExplicitRegisterCount && explicitRegisterCount != count) {
            if (rejectReason) {
                *rejectReason = QStringLiteral("codec: 显式寄存器数量与类型宽度冲突：expected=%1 got=%2")
                        .arg(count)
                        .arg(explicitRegisterCount);
            }
            return false;
        }
    } else {
        int explicitRegisterCount = 0;
        bool hasExplicitRegisterCount = false;
        QString countReason;
        if (!readOptionalPositiveInt(address,
                                     {QStringLiteral("registerCount"),
                                      QStringLiteral("count"),
                                      QStringLiteral("length")},
                                     &explicitRegisterCount,
                                     &hasExplicitRegisterCount,
                                     &countReason)) {
            if (rejectReason) {
                *rejectReason = QStringLiteral("codec: %1").arg(countReason);
            }
            return false;
        }
        if (hasExplicitRegisterCount) {
            count = explicitRegisterCount;
        }
        codec.registerCount = count;
    }
    if (count <= 0 || count > 125) {
        if (rejectReason) {
            *rejectReason = QStringLiteral("codec: 寄存器数量超出 1..125");
        }
        return false;
    }
    if (regAddress > 0xffff || count > 0x10000 - regAddress) {
        if (rejectReason) {
            *rejectReason = QStringLiteral("寄存器地址范围越界：address=%1 count=%2")
                    .arg(regAddress)
                    .arg(count);
        }
        return false;
    }

    int unitId = 0;
    bool hasExplicitUnitId = false;
    QString deviceIdReason;
    if (!readExplicitDeviceId(point.metadata, &unitId, &hasExplicitUnitId, &deviceIdReason)) {
        if (rejectReason) {
            *rejectReason = QStringLiteral("deviceId: %1").arg(deviceIdReason);
        }
        return false;
    }
    if (!hasExplicitUnitId) {
        unitId = deviceId();
        readPositiveInt(address,
                        {QStringLiteral("unitId"),
                         QStringLiteral("slaveId"),
                         QStringLiteral("stationAddress"),
                         QStringLiteral("serverAddress")},
                        &unitId);
    }
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
    mapping->codec = codec;
    bool readable = point.access != RuntimePointAccess::WriteOnly;
    bool writable = point.access != RuntimePointAccess::ReadOnly;
    const QString tagAccess = address.value(QStringLiteral("tagAccess")).toString().trimmed();
    if (!tagAccess.isEmpty()) {
        const QString normalizedTagAccess = tagAccess.toLower();
        if (normalizedTagAccess == QStringLiteral("readonly")
                || normalizedTagAccess == QStringLiteral("read-only")) {
            writable = false;
        } else if (normalizedTagAccess == QStringLiteral("writeonly")
                   || normalizedTagAccess == QStringLiteral("write-only")) {
            readable = false;
        } else if (normalizedTagAccess != QStringLiteral("readwrite")
                   && normalizedTagAccess != QStringLiteral("read-write")) {
            if (rejectReason) {
                *rejectReason = QStringLiteral("tagAccess 无效：%1").arg(tagAccess);
            }
            return false;
        }
    }
    mapping->readable = readable;
    mapping->writable = writable;
    return true;
}
