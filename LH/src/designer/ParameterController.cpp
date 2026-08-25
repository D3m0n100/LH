// File: src/designer/ParameterController.cpp

#include "ParameterController.h"
#include "../communication/IDeviceBackend.h"

#include <QEventLoop>
#include <QMetaType>
#include <QTimer>
#include <QtGlobal>

#include <cmath>
#include <limits>

namespace {
QString canonicalParameterDataType(const QString& dataType)
{
    const QString type = dataType.trimmed().toUpper();
    if (type == QStringLiteral("INT") || type == QStringLiteral("INT16"))
        return QStringLiteral("INT16");
    if (type == QStringLiteral("DINT") || type == QStringLiteral("INT32"))
        return QStringLiteral("INT32");
    if (type == QStringLiteral("UINT") || type == QStringLiteral("WORD")
            || type == QStringLiteral("UINT16")) {
        return QStringLiteral("UINT16");
    }
    if (type == QStringLiteral("UDINT") || type == QStringLiteral("DWORD")
            || type == QStringLiteral("UINT32")) {
        return QStringLiteral("UINT32");
    }
    if (type == QStringLiteral("REAL") || type == QStringLiteral("FLOAT32"))
        return QStringLiteral("REAL");
    if (type == QStringLiteral("BOOL"))
        return QStringLiteral("BOOL");
    return type;
}

bool variantToSigned(const QVariant& value, qint64 minimum, qint64 maximum, qint64* result)
{
    if (!result || !value.isValid() || value.isNull())
        return false;

    const int type = value.userType();
    if (type == QMetaType::QString) {
        bool ok = false;
        const qint64 parsed = value.toString().trimmed().toLongLong(&ok);
        if (!ok || parsed < minimum || parsed > maximum)
            return false;
        *result = parsed;
        return true;
    }
    if (type == QMetaType::Bool)
        return false;
    if (type == QMetaType::Float || type == QMetaType::Double) {
        bool ok = false;
        const double parsed = value.toDouble(&ok);
        if (!ok || !std::isfinite(parsed) || std::trunc(parsed) != parsed
                || parsed < static_cast<double>(minimum)
                || parsed > static_cast<double>(maximum)) {
            return false;
        }
        *result = static_cast<qint64>(parsed);
        return true;
    }

    bool ok = false;
    const qint64 parsed = value.toLongLong(&ok);
    if (ok && parsed >= minimum && parsed <= maximum) {
        *result = parsed;
        return true;
    }

    const quint64 unsignedParsed = value.toULongLong(&ok);
    if (!ok || unsignedParsed > static_cast<quint64>(maximum))
        return false;
    *result = static_cast<qint64>(unsignedParsed);
    return true;
}

bool variantToUnsigned(const QVariant& value, quint64 maximum, quint64* result)
{
    if (!result || !value.isValid() || value.isNull())
        return false;

    const int type = value.userType();
    if (type == QMetaType::QString) {
        const QString text = value.toString().trimmed();
        if (text.startsWith(QLatin1Char('-')))
            return false;
        bool ok = false;
        const quint64 parsed = text.toULongLong(&ok);
        if (!ok || parsed > maximum)
            return false;
        *result = parsed;
        return true;
    }
    if (type == QMetaType::Bool)
        return false;
    if (type == QMetaType::Float || type == QMetaType::Double) {
        bool ok = false;
        const double parsed = value.toDouble(&ok);
        if (!ok || !std::isfinite(parsed) || std::trunc(parsed) != parsed
                || parsed < 0.0 || parsed > static_cast<double>(maximum)) {
            return false;
        }
        *result = static_cast<quint64>(parsed);
        return true;
    }

    bool ok = false;
    const qint64 signedParsed = value.toLongLong(&ok);
    if (ok) {
        if (signedParsed < 0 || static_cast<quint64>(signedParsed) > maximum)
            return false;
        *result = static_cast<quint64>(signedParsed);
        return true;
    }

    const quint64 parsed = value.toULongLong(&ok);
    if (!ok || parsed > maximum)
        return false;
    *result = parsed;
    return true;
}

bool variantToBool(const QVariant& value, bool* result)
{
    if (!result || !value.isValid() || value.isNull())
        return false;
    if (value.userType() == QMetaType::Bool) {
        *result = value.toBool();
        return true;
    }
    if (value.userType() == QMetaType::QString) {
        const QString text = value.toString().trimmed().toLower();
        if (text == QStringLiteral("true") || text == QStringLiteral("1")) {
            *result = true;
            return true;
        }
        if (text == QStringLiteral("false") || text == QStringLiteral("0")) {
            *result = false;
            return true;
        }
        return false;
    }

    qint64 integer = 0;
    if (value.userType() != QMetaType::Float && value.userType() != QMetaType::Double
            && variantToSigned(value, 0, 1, &integer)) {
        *result = integer != 0;
        return true;
    }
    return false;
}

bool variantToFloat32(const QVariant& value, float* result)
{
    if (!result || !value.isValid() || value.isNull()
            || value.userType() == QMetaType::Bool) {
        return false;
    }

    bool ok = false;
    const float parsed = value.userType() == QMetaType::QString
            ? value.toString().trimmed().toFloat(&ok)
            : value.toFloat(&ok);
    if (!ok || !std::isfinite(parsed))
        return false;
    *result = parsed;
    return true;
}

bool valuesMatch(const QString& dataType,
                 const QString& appliedValue,
                 const QVariant& readbackValue)
{
    if (!readbackValue.isValid())
        return false;

    const QString type = canonicalParameterDataType(dataType);
    if (type == QStringLiteral("BOOL")) {
        bool applied = false;
        bool readback = false;
        return variantToBool(QVariant(appliedValue), &applied)
                && variantToBool(readbackValue, &readback)
                && applied == readback;
    }
    if (type == QStringLiteral("INT16") || type == QStringLiteral("INT32")) {
        const qint64 minimum = type == QStringLiteral("INT16")
                ? std::numeric_limits<qint16>::min()
                : std::numeric_limits<qint32>::min();
        const qint64 maximum = type == QStringLiteral("INT16")
                ? std::numeric_limits<qint16>::max()
                : std::numeric_limits<qint32>::max();
        bool appliedOk = false;
        const qint64 applied = appliedValue.trimmed().toLongLong(&appliedOk);
        qint64 readback = 0;
        return appliedOk && applied >= minimum && applied <= maximum
                && variantToSigned(readbackValue, minimum, maximum, &readback)
                && applied == readback;
    }
    if (type == QStringLiteral("UINT16") || type == QStringLiteral("UINT32")) {
        const quint64 maximum = type == QStringLiteral("UINT16")
                ? std::numeric_limits<quint16>::max()
                : std::numeric_limits<quint32>::max();
        bool appliedOk = false;
        const QString appliedText = appliedValue.trimmed();
        const quint64 applied = appliedText.startsWith(QLatin1Char('-'))
                ? 0
                : appliedText.toULongLong(&appliedOk);
        quint64 readback = 0;
        return appliedOk && applied <= maximum
                && variantToUnsigned(readbackValue, maximum, &readback)
                && applied == readback;
    }
    if (type == QStringLiteral("REAL")) {
        bool appliedOk = false;
        const float applied = appliedValue.trimmed().toFloat(&appliedOk);
        float readback = 0.0f;
        return appliedOk && std::isfinite(applied)
                && variantToFloat32(readbackValue, &readback)
                && applied == readback;
    }

    return appliedValue.trimmed() == readbackValue.toString().trimmed();
}

QString commErrorMessage(const CommError& error)
{
    const QString message = error.message.trimmed();
    const QString details = error.details.trimmed();
    if (!message.isEmpty() && !details.isEmpty()) {
        return QStringLiteral("%1 (%2)").arg(message, details);
    }
    return message.isEmpty() ? details : message;
}
}

ParameterController::ParameterController(QObject* parent)
    : QObject(parent)
{
}

void ParameterController::loadDefinitions(const QList<ParameterDefinition>& definitions)
{
    bool invalidatesPendingReadback = m_pendingReadbackActive;
    if (invalidatesPendingReadback && definitions.size() == m_states.size()) {
        invalidatesPendingReadback = false;
        for (const auto& def : definitions) {
            const auto it = m_states.constFind(def.name);
            if (it == m_states.constEnd() || it->pointId != def.id) {
                invalidatesPendingReadback = true;
                break;
            }
        }
    }
    if (invalidatesPendingReadback) {
        cancelPendingReadback(QStringLiteral("参数定义已刷新，回读已取消"));
    }

    QMap<QString, ParameterStateInfo> newStates;

    for (const auto& def : definitions) {
        ParameterStateInfo info;
        info.pointId = def.id;
        info.name = def.name;
        info.dataType = canonicalParameterDataType(def.dataType);
        info.onlineEditable = def.onlineEditable;
        info.definitionValue = def.currentValue.isEmpty() ? def.defaultValue : def.currentValue;

        auto it = m_states.find(def.name);
        if (it != m_states.end()) {
            info.state = it->state;
            info.editedValue = it->editedValue;
            info.appliedValue = it->appliedValue;
            info.readbackValue = it->readbackValue;
            info.lastError = it->lastError;
            info.lastWriteTime = it->lastWriteTime;
            info.lastReadbackTime = it->lastReadbackTime;
            info.readbackAttempts = it->readbackAttempts;
        }

        newStates.insert(def.name, info);
    }

    m_states = newStates;
    emit statesChanged();
}

void ParameterController::clear()
{
    cancelPendingReadback(QStringLiteral("参数状态已清空，回读已取消"));
    m_states.clear();
    emit statesChanged();
}

bool ParameterController::editParameter(const QString& name, const QString& value)
{
    auto it = m_states.find(name);
    if (it == m_states.end() || !it->onlineEditable)
        return false;

    const ParameterState oldState = it->state;
    it->editedValue = value;
    it->state = ParameterState::Modified;
    it->lastError.clear();

    emit stateChanged(name, oldState, ParameterState::Modified);
    emit statesChanged();
    return true;
}

bool ParameterController::editParameterByPointId(const QString& pointId, const QString& value)
{
    if (pointId.trimmed().isEmpty()) {
        return false;
    }

    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (it->pointId == pointId) {
            return editParameter(it->name, value);
        }
    }

    return false;
}

bool ParameterController::applyModifiedParametersForTargets(
        IDeviceBackend* backend,
        const QStringList& targetPointIds,
        QStringList* batchTargetPointIds,
        QString* errorMessage)
{
    if (batchTargetPointIds)
        batchTargetPointIds->clear();
    if (!backend) {
        if (errorMessage)
            *errorMessage = QStringLiteral("参数下发失败：后端为空");
        return false;
    }

    QHash<QString, QVariant> writes;
    QStringList modifiedNames;
    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (it->state != ParameterState::Modified || it->pointId.isEmpty())
            continue;
        if (!targetPointIds.isEmpty() && !targetPointIds.contains(it->pointId))
            continue;

        writes.insert(it->pointId, it->editedValue);
        modifiedNames.append(it->name);
        if (batchTargetPointIds && !batchTargetPointIds->contains(it->pointId))
            batchTargetPointIds->append(it->pointId);
    }

    if (writes.isEmpty())
        return true;

    for (const auto& name : modifiedNames) {
        auto& info = m_states[name];
        const ParameterState oldState = info.state;
        info.state = ParameterState::PendingApply;
        emit stateChanged(name, oldState, ParameterState::PendingApply);
    }
    emit statesChanged();

    for (const auto& name : modifiedNames) {
        auto& info = m_states[name];
        const ParameterState oldState = info.state;
        info.state = ParameterState::Applying;
        emit stateChanged(name, oldState, ParameterState::Applying);
    }
    emit statesChanged();

    QString overallError;
    QHash<QString, CommError> pointErrors;
    const bool overallOk = backend->writePoints(writes, &overallError, &pointErrors);
    QString firstPointError;
    const QDateTime writeTime = QDateTime::currentDateTimeUtc();

    for (const auto& name : modifiedNames) {
        auto& info = m_states[name];
        const auto pointErrorIt = pointErrors.constFind(info.pointId);
        const bool pointFailed = pointErrorIt != pointErrors.constEnd()
                || (!overallOk && pointErrors.isEmpty());
        const ParameterState oldState = info.state;

        if (pointFailed) {
            QString pointError;
            if (pointErrorIt != pointErrors.constEnd())
                pointError = commErrorMessage(pointErrorIt.value());
            if (pointError.isEmpty())
                pointError = overallError.trimmed();
            if (pointError.isEmpty())
                pointError = QStringLiteral("参数下发失败");
            if (firstPointError.isEmpty())
                firstPointError = pointError;
            info.state = ParameterState::ApplyFailed;
            info.lastError = pointError;
            emit stateChanged(name, oldState, ParameterState::ApplyFailed);
            continue;
        }

        info.state = ParameterState::PendingReadback;
        info.appliedValue = info.editedValue;
        info.lastError.clear();
        info.lastWriteTime = writeTime;
        info.lastReadbackTime = QDateTime();
        info.readbackAttempts = 0;
        emit stateChanged(name, oldState, ParameterState::PendingReadback);
    }
    emit statesChanged();

    if (errorMessage) {
        *errorMessage = overallError.trimmed().isEmpty()
                ? firstPointError
                : overallError.trimmed();
    }
    return overallOk && pointErrors.isEmpty();
}

bool ParameterController::applyModifiedParameters(IDeviceBackend* backend)
{
    if (m_pendingReadbackActive)
        return false;

    QStringList targetPointIds;
    return applyModifiedParametersForTargets(backend, {}, &targetPointIds, nullptr);
}

bool ParameterController::applyModifiedParametersWithReadback(IDeviceBackend* backend,
                                                              int maxReadbackRetries,
                                                              int readbackRetryIntervalMs,
                                                              QString* errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    const QPointer<IDeviceBackend> safeBackend = backend;
    if (!safeBackend) {
        if (errorMessage)
            *errorMessage = QStringLiteral("参数下发失败：后端为空");
        return false;
    }
    if (m_pendingReadbackActive) {
        if (errorMessage)
            *errorMessage = QStringLiteral("参数回读正在进行中");
        return false;
    }

    QStringList targetPointIds;
    QString writeError;
    applyModifiedParametersForTargets(safeBackend.data(), {}, &targetPointIds, &writeError);
    if (targetPointIds.isEmpty())
        return true;

    const int retryCount = qMax(1, maxReadbackRetries);
    QString readbackError;
    QHash<QString, QVariant> readbackValues;

    QString decisionMessage;
    ReadbackDecision decision = evaluateReadback(targetPointIds, &decisionMessage);
    if (decision == ReadbackDecision::Failure) {
        if (decisionMessage.isEmpty())
            decisionMessage = writeError;
        setPendingReadbackError(decisionMessage, targetPointIds);
        if (errorMessage)
            *errorMessage = decisionMessage;
        return false;
    }

    for (int attempt = 0; attempt < retryCount; ++attempt) {
        const QStringList pointIds = pendingReadbackPointIds(targetPointIds);
        if (pointIds.isEmpty()) {
            decision = evaluateReadback(targetPointIds, &decisionMessage);
            if (decision == ReadbackDecision::Success)
                return true;
            if (decision == ReadbackDecision::Failure) {
                setPendingReadbackError(decisionMessage, targetPointIds);
                if (errorMessage)
                    *errorMessage = decisionMessage;
                return false;
            }
            break;
        }

        for (auto it = m_states.begin(); it != m_states.end(); ++it) {
            if (it->state == ParameterState::PendingReadback
                    && targetPointIds.contains(it->pointId)) {
                ++it->readbackAttempts;
            }
        }

        readbackValues.clear();
        readbackError.clear();
        if (!safeBackend) {
            const QString failure = QStringLiteral("参数回读失败：后端已销毁");
            setPendingReadbackError(failure, targetPointIds);
            if (errorMessage)
                *errorMessage = failure;
            return false;
        }
        safeBackend->readPoints(pointIds, readbackValues, &readbackError);
        if (!readbackValues.isEmpty()) {
            applyReadbackValues(readbackValues, targetPointIds);
            decision = evaluateReadback(targetPointIds, &decisionMessage);
            if (decision == ReadbackDecision::Success) {
                return true;
            }
            if (decision == ReadbackDecision::Failure) {
                setPendingReadbackError(decisionMessage, targetPointIds);
                if (errorMessage)
                    *errorMessage = decisionMessage;
                return false;
            }
        }

        if (attempt + 1 < retryCount && readbackRetryIntervalMs > 0) {
            QEventLoop loop;
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            timer.start(readbackRetryIntervalMs);
            loop.exec();
        }
    }

    const QString timeoutMessage = readbackError.isEmpty()
            ? QStringLiteral("参数回读超时")
            : QStringLiteral("参数回读失败：%1").arg(readbackError);
    setPendingReadbackError(timeoutMessage, targetPointIds);
    if (errorMessage) {
        *errorMessage = timeoutMessage;
    }
    return false;
}

bool ParameterController::applyModifiedParametersWithReadbackAsync(IDeviceBackend* backend,
                                                                   int maxReadbackRetries,
                                                                   int readbackRetryIntervalMs,
                                                                   QString* errorMessage)
{
    return applyModifiedParametersWithReadbackAsyncForTargets(
            backend, {}, maxReadbackRetries, readbackRetryIntervalMs, errorMessage);
}

bool ParameterController::applyParameterByPointIdWithReadbackAsync(
        IDeviceBackend* backend,
        const QString& pointId,
        int maxReadbackRetries,
        int readbackRetryIntervalMs,
        QString* errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    const ParameterStateInfo info = parameterStateByPointId(pointId);
    if (pointId.trimmed().isEmpty() || info.name.isEmpty()
            || info.state != ParameterState::Modified) {
        if (errorMessage)
            *errorMessage = QStringLiteral("参数点位不可应用：%1").arg(pointId);
        return false;
    }

    return applyModifiedParametersWithReadbackAsyncForTargets(
            backend,
            {pointId},
            maxReadbackRetries,
            readbackRetryIntervalMs,
            errorMessage);
}

bool ParameterController::applyModifiedParametersWithReadbackAsyncForTargets(
        IDeviceBackend* backend,
        const QStringList& targetPointIds,
        int maxReadbackRetries,
        int readbackRetryIntervalMs,
        QString* errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    if (!backend) {
        if (errorMessage)
            *errorMessage = QStringLiteral("参数下发失败：后端为空");
        return false;
    }
    if (m_pendingReadbackActive) {
        if (errorMessage)
            *errorMessage = QStringLiteral("参数回读正在进行中");
        return false;
    }

    QStringList batchTargetPointIds;
    QString writeError;
    applyModifiedParametersForTargets(backend,
                                      targetPointIds,
                                      &batchTargetPointIds,
                                      &writeError);
    if (batchTargetPointIds.isEmpty()) {
        emit readbackFinished(true, QString());
        return true;
    }

    QString decisionMessage;
    if (evaluateReadback(batchTargetPointIds, &decisionMessage)
            == ReadbackDecision::Failure) {
        if (errorMessage)
            *errorMessage = decisionMessage.isEmpty()
                    ? (writeError.isEmpty() ? QStringLiteral("参数下发失败") : writeError)
                    : decisionMessage;
        return false;
    }

    m_pendingReadbackBackend = backend;
    m_pendingReadbackTargetPointIds = batchTargetPointIds;
    m_pendingReadbackMaxRetries = qMax(1, maxReadbackRetries);
    m_pendingReadbackRetryIntervalMs = qMax(0, readbackRetryIntervalMs);
    m_pendingReadbackAttempt = 0;
    m_pendingReadbackMessage.clear();
    m_pendingReadbackActive = true;
    const quint64 generation = ++m_pendingReadbackGeneration;

    connect(backend, &QObject::destroyed, this, [this, generation]() {
        if (m_pendingReadbackActive && m_pendingReadbackGeneration == generation)
            finishReadback(false, QStringLiteral("参数回读失败：后端已销毁"));
    });

    QTimer::singleShot(0, this, [this, generation]() {
        if (m_pendingReadbackActive && m_pendingReadbackGeneration == generation)
            pollReadbackAttempt();
    });
    return true;
}

void ParameterController::onReadbackValues(const QHash<QString, QVariant>& readbackValues)
{
    applyReadbackValues(readbackValues,
                        m_pendingReadbackActive ? m_pendingReadbackTargetPointIds : QStringList());
}

void ParameterController::applyReadbackValues(const QHash<QString, QVariant>& readbackValues,
                                              const QStringList& targetPointIds)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();

    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (it->state != ParameterState::PendingReadback)
            continue;
        if (!targetPointIds.isEmpty() && !targetPointIds.contains(it->pointId))
            continue;

        auto rvIt = readbackValues.constFind(it->pointId);
        if (rvIt == readbackValues.constEnd())
            rvIt = readbackValues.constFind(it->name);
        if (rvIt == readbackValues.constEnd())
            continue;

        const ParameterState oldState = it->state;
        it->readbackValue = rvIt->toString();
        it->lastReadbackTime = now;
        it->state = valuesMatch(it->dataType, it->appliedValue, *rvIt)
                ? ParameterState::Confirmed
                : ParameterState::Mismatch;
        emit stateChanged(it->name, oldState, it->state);
    }

    emit statesChanged();
}

ParameterStateInfo ParameterController::parameterState(const QString& name) const
{
    return m_states.value(name);
}

ParameterStateInfo ParameterController::parameterStateByPointId(const QString& pointId) const
{
    for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it) {
        if (it->pointId == pointId) {
            return it.value();
        }
    }
    return ParameterStateInfo();
}

QList<ParameterStateInfo> ParameterController::parameterStates() const
{
    return m_states.values();
}

QStringList ParameterController::parameterNamesByState(ParameterState state) const
{
    QStringList names;
    for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it) {
        if (it->state == state)
            names.append(it.key());
    }
    return names;
}

bool ParameterController::hasModifiedParameters() const
{
    for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it) {
        if (it->state == ParameterState::Modified)
            return true;
    }
    return false;
}

QStringList ParameterController::pendingReadbackPointIds(const QStringList& targetPointIds) const
{
    QStringList pointIds;
    for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it) {
        if (it->state != ParameterState::PendingReadback || it->pointId.isEmpty())
            continue;
        if (!targetPointIds.isEmpty() && !targetPointIds.contains(it->pointId))
            continue;
        if (!pointIds.contains(it->pointId))
            pointIds.append(it->pointId);
    }
    return pointIds;
}

ParameterController::ReadbackDecision ParameterController::evaluateReadback(
        const QStringList& targetPointIds,
        QString* message) const
{
    if (message)
        message->clear();
    if (targetPointIds.isEmpty())
        return ReadbackDecision::Success;

    QStringList pendingNames;
    QStringList mismatchNames;
    QStringList timeoutNames;
    QStringList failedNames;
    bool allConfirmed = true;

    for (const auto& pointId : targetPointIds) {
        const ParameterStateInfo info = parameterStateByPointId(pointId);
        if (info.name.isEmpty()) {
            allConfirmed = false;
            failedNames.append(pointId);
            continue;
        }
        switch (info.state) {
        case ParameterState::Confirmed:
            break;
        case ParameterState::PendingReadback:
            allConfirmed = false;
            pendingNames.append(info.name);
            break;
        case ParameterState::Mismatch:
            allConfirmed = false;
            mismatchNames.append(info.name);
            break;
        case ParameterState::Timeout:
            allConfirmed = false;
            timeoutNames.append(info.name);
            break;
        case ParameterState::ApplyFailed:
            allConfirmed = false;
            failedNames.append(info.lastError.isEmpty()
                                       ? info.name
                                       : QStringLiteral("%1 (%2)").arg(info.name, info.lastError));
            break;
        default:
            allConfirmed = false;
            failedNames.append(info.name);
            break;
        }
    }

    if (!mismatchNames.isEmpty()) {
        if (message)
            *message = QStringLiteral("参数回读不匹配：%1").arg(mismatchNames.join(QStringLiteral(", ")));
        return ReadbackDecision::Failure;
    }
    if (!timeoutNames.isEmpty()) {
        if (message)
            *message = QStringLiteral("参数回读超时：%1").arg(timeoutNames.join(QStringLiteral(", ")));
        return ReadbackDecision::Failure;
    }
    if (!pendingNames.isEmpty())
        return ReadbackDecision::Continue;
    if (!failedNames.isEmpty()) {
        if (message)
            *message = QStringLiteral("参数下发失败：%1").arg(failedNames.join(QStringLiteral(", ")));
        return ReadbackDecision::Failure;
    }
    if (allConfirmed)
        return ReadbackDecision::Success;

    if (message)
        *message = QStringLiteral("参数回读未完成");
    return ReadbackDecision::Failure;
}

void ParameterController::setPendingReadbackError(const QString& errorMessage,
                                                  const QStringList& targetPointIds)
{
    QStringList timedOutNames;
    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (it->state != ParameterState::PendingReadback)
            continue;
        if (!targetPointIds.isEmpty() && !targetPointIds.contains(it->pointId))
            continue;
        it->lastError = errorMessage;
        it->state = ParameterState::Timeout;
        timedOutNames.append(it.key());
    }
    for (const auto& name : timedOutNames) {
        emit stateChanged(name, ParameterState::PendingReadback, ParameterState::Timeout);
    }
    if (!timedOutNames.isEmpty())
        emit statesChanged();
}

void ParameterController::pollReadbackAttempt()
{
    if (!m_pendingReadbackActive) {
        return;
    }

    const QPointer<IDeviceBackend> backend = m_pendingReadbackBackend;
    if (!backend) {
        finishReadback(false, QStringLiteral("参数回读失败：后端不可用"));
        return;
    }

    QString decisionMessage;
    const ReadbackDecision initialDecision = evaluateReadback(
            m_pendingReadbackTargetPointIds, &decisionMessage);
    if (initialDecision == ReadbackDecision::Success) {
        finishReadback(true, QString());
        return;
    }
    if (initialDecision == ReadbackDecision::Failure) {
        finishReadback(false, decisionMessage);
        return;
    }

    const QStringList pointIds = pendingReadbackPointIds(m_pendingReadbackTargetPointIds);
    if (pointIds.isEmpty()) {
        finishReadback(false, QStringLiteral("参数回读未完成"));
        return;
    }

    ++m_pendingReadbackAttempt;
    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (it->state == ParameterState::PendingReadback
                && m_pendingReadbackTargetPointIds.contains(it->pointId)) {
            ++it->readbackAttempts;
        }
    }

    QHash<QString, QVariant> readbackValues;
    QString readbackError;
    backend->readPoints(pointIds, readbackValues, &readbackError);

    if (!readbackValues.isEmpty()) {
        applyReadbackValues(readbackValues, m_pendingReadbackTargetPointIds);
    }

    const ReadbackDecision decision = evaluateReadback(
            m_pendingReadbackTargetPointIds, &decisionMessage);
    if (decision == ReadbackDecision::Failure) {
        finishReadback(false, decisionMessage);
        return;
    }
    if (decision == ReadbackDecision::Success) {
        finishReadback(true, QString());
        return;
    }

    if (m_pendingReadbackAttempt >= m_pendingReadbackMaxRetries) {
        const QString finalMessage = readbackError.isEmpty()
                ? QStringLiteral("参数回读超时")
                : QStringLiteral("参数回读失败：%1").arg(readbackError);
        finishReadback(false, finalMessage);
        return;
    }

    const quint64 generation = m_pendingReadbackGeneration;
    QTimer::singleShot(m_pendingReadbackRetryIntervalMs, this, [this, generation]() {
        if (m_pendingReadbackActive && m_pendingReadbackGeneration == generation)
            pollReadbackAttempt();
    });
}

void ParameterController::finishReadback(bool success, const QString& message)
{
    const QStringList targetPointIds = m_pendingReadbackTargetPointIds;
    const QString finalMessage = message.isEmpty() && !success
            ? QStringLiteral("参数回读失败")
            : message;
    m_pendingReadbackActive = false;
    m_pendingReadbackBackend = nullptr;
    m_pendingReadbackTargetPointIds.clear();
    m_pendingReadbackMaxRetries = 0;
    m_pendingReadbackRetryIntervalMs = 0;
    m_pendingReadbackAttempt = 0;
    m_pendingReadbackMessage = finalMessage;
    ++m_pendingReadbackGeneration;

    if (!success) {
        setPendingReadbackError(finalMessage, targetPointIds);
    }

    emit readbackFinished(success, finalMessage);
}

void ParameterController::cancelPendingReadback(const QString& message)
{
    if (!m_pendingReadbackActive)
        return;

    const QString finalMessage = message.isEmpty()
            ? QStringLiteral("参数回读已取消")
            : message;
    const QStringList targetPointIds = m_pendingReadbackTargetPointIds;
    m_pendingReadbackActive = false;
    m_pendingReadbackBackend = nullptr;
    m_pendingReadbackTargetPointIds.clear();
    m_pendingReadbackMaxRetries = 0;
    m_pendingReadbackRetryIntervalMs = 0;
    m_pendingReadbackAttempt = 0;
    m_pendingReadbackMessage = finalMessage;
    ++m_pendingReadbackGeneration;

    setPendingReadbackError(finalMessage, targetPointIds);
    emit readbackFinished(false, finalMessage);
}
