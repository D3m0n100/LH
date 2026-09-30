// File: src/designer/ParameterController.cpp

#include "ParameterController.h"
#include "../communication/IDeviceBackend.h"
#include "../core/AppLogging.h"

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

    const QString writeOpId = QStringLiteral("op-param-write-%1").arg(QDateTime::currentMSecsSinceEpoch());
    AppLogging::writeBusinessEvent(
        QStringLiteral("parameter_write_started"),
        QtInfoMsg,
        writeOpId,
        QStringLiteral("parameter_write"),
        QStringLiteral("started"),
        0,
        modifiedNames.join(QLatin1Char(',')),
        {{QStringLiteral("count"), modifiedNames.size()}});

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

    const bool writeSuccess = overallOk && pointErrors.isEmpty();
    if (writeSuccess) {
        AppLogging::writeBusinessEvent(
            QStringLiteral("parameter_write_succeeded"),
            QtInfoMsg,
            writeOpId,
            QStringLiteral("parameter_write"),
            QStringLiteral("succeeded"),
            0,
            modifiedNames.join(QLatin1Char(',')),
            {{QStringLiteral("count"), modifiedNames.size()}});
    } else {
        AppLogging::writeBusinessEvent(
            QStringLiteral("parameter_write_failed"),
            QtWarningMsg,
            writeOpId,
            QStringLiteral("parameter_write"),
            QStringLiteral("failed"),
            -1,
            modifiedNames.join(QLatin1Char(',')),
            {{QStringLiteral("error"), overallError.trimmed().isEmpty() ? firstPointError : overallError.trimmed()},
             {QStringLiteral("failedPoints"), pointErrors.keys().join(QLatin1Char(','))}});
    }
    return writeSuccess;
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
            loop.exec(QEventLoop::ExcludeUserInputEvents);
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
    QHash<QString, QVariant> writes;
    for (auto it = m_states.cbegin(); it != m_states.cend(); ++it) {
        if (it->state == ParameterState::Modified && !it->pointId.isEmpty()
                && (targetPointIds.isEmpty() || targetPointIds.contains(it->pointId))) {
            writes.insert(it->pointId, it->editedValue);
            if (!batchTargetPointIds.contains(it->pointId)) batchTargetPointIds.append(it->pointId);
        }
    }
    if (batchTargetPointIds.isEmpty()) {
        emit readbackFinished(true, QString());
        return true;
    }

    if (!backend->supportsAsyncWrite() || !backend->supportsAsyncRead()) {
        if (errorMessage) *errorMessage = QStringLiteral("后端不支持异步参数写入/回读；请使用支持此能力的后端");
        return false;
    }

    m_pendingReadbackBackend = backend;
    const QPointer<IDeviceBackend> asyncBackend(backend);
    m_pendingReadbackTargetPointIds = batchTargetPointIds;
    m_pendingReadbackMaxRetries = qMax(1, maxReadbackRetries);
    m_pendingReadbackRetryIntervalMs = qMax(0, readbackRetryIntervalMs);
    m_pendingReadbackAttempt = 0;
    m_pendingReadbackMessage.clear();
    m_pendingReadbackActive = true;
    m_pendingReadbackOpId = QStringLiteral("op-param-rb-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const quint64 generation = ++m_pendingReadbackGeneration;
    m_pendingCancelled = std::make_shared<std::atomic_bool>(false);
    m_pendingDeadline.setRemainingTime(15000);
    QHash<QString, ParameterState> applyingStates;
    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (writes.contains(it->pointId)) {
            applyingStates.insert(it.key(), it->state);
            it->state = ParameterState::Applying;
        }
    }
    for (auto it = applyingStates.cbegin(); it != applyingStates.cend(); ++it) {
        emit stateChanged(it.key(), it.value(), ParameterState::Applying);
        if (!m_pendingReadbackActive || generation != m_pendingReadbackGeneration) return true;
    }
    emit statesChanged();
    if (!m_pendingReadbackActive || generation != m_pendingReadbackGeneration) return true;
    if (!asyncBackend) { finishReadback(false, QStringLiteral("参数写入失败：后端已销毁")); return true; }

    AppLogging::writeBusinessEvent(
        QStringLiteral("parameter_readback_started"),
        QtInfoMsg,
        m_pendingReadbackOpId,
        QStringLiteral("readback"),
        QStringLiteral("started"),
        0,
        batchTargetPointIds.join(QLatin1Char(',')),
        {{QStringLiteral("maxRetries"), maxReadbackRetries},
         {QStringLiteral("retryIntervalMs"), readbackRetryIntervalMs}});

    connect(asyncBackend.data(), &QObject::destroyed, this, [this, generation]() {
        if (m_pendingReadbackActive && m_pendingReadbackGeneration == generation)
            finishReadback(false, QStringLiteral("参数回读失败：后端已销毁"));
    });

    QTimer::singleShot(15000, this, [this, generation]() {
        if (m_pendingReadbackActive && m_pendingReadbackGeneration == generation)
            finishReadback(false, QStringLiteral("参数写入/回读总时限已到"));
    });
    asyncBackend->writePointsAsync(writes, static_cast<int>(m_pendingDeadline.remainingTime()), m_pendingCancelled, this,
        [this, generation, writes](bool ok, const QString& error, const QHash<QString, CommError>& errors) {
        if (!m_pendingReadbackActive || generation != m_pendingReadbackGeneration) return;
        QHash<QString, QPair<ParameterState, ParameterState>> changed;
        for (auto it = m_states.begin(); it != m_states.end(); ++it) {
            if (!writes.contains(it->pointId)) continue;
            const auto previous = it->state;
            const bool failed = errors.contains(it->pointId) || (!ok && errors.isEmpty());
            it->state = failed ? ParameterState::ApplyFailed : ParameterState::PendingReadback;
            it->lastError = failed ? (errors.contains(it->pointId) ? commErrorMessage(errors.value(it->pointId)) : error) : QString();
            if (!failed) {
                it->appliedValue = writes.value(it->pointId).toString();
                it->lastWriteTime = QDateTime::currentDateTimeUtc();
                it->lastReadbackTime = QDateTime();
                it->readbackAttempts = 0;
            }
            changed.insert(it.key(), qMakePair(previous, it->state));
        }
        for (auto it = changed.cbegin(); it != changed.cend(); ++it) {
            emit stateChanged(it.key(), it.value().first, it.value().second);
            if (!m_pendingReadbackActive || generation != m_pendingReadbackGeneration) return;
        }
        emit statesChanged();
        if (!m_pendingReadbackActive || generation != m_pendingReadbackGeneration) return;
        if (m_pendingDeadline.hasExpired()) { finishReadback(false, QStringLiteral("参数写入/回读总时限已到")); return; }
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
    QHash<QString, ParameterState> timedOutNames;
    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (it->state != ParameterState::PendingReadback && it->state != ParameterState::Applying
                && it->state != ParameterState::PendingApply)
            continue;
        if (!targetPointIds.isEmpty() && !targetPointIds.contains(it->pointId))
            continue;
        it->lastError = errorMessage;
        timedOutNames.insert(it.key(), it->state);
        it->state = ParameterState::Timeout;
    }
    for (auto it = timedOutNames.cbegin(); it != timedOutNames.cend(); ++it) {
        emit stateChanged(it.key(), it.value(), ParameterState::Timeout);
    }
    if (!timedOutNames.isEmpty())
        emit statesChanged();
}

void ParameterController::pollReadbackAttempt()
{
    if (!m_pendingReadbackActive) {
        return;
    }

    if (m_pendingDeadline.hasExpired()) {
        finishReadback(false, QStringLiteral("参数写入/回读总时限已到"));
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
    AppLogging::writeBusinessEvent(
        QStringLiteral("parameter_readback_attempt"),
        QtInfoMsg,
        m_pendingReadbackOpId,
        QStringLiteral("readback"),
        QStringLiteral("polling"),
        0,
        pointIds.join(QLatin1Char(',')),
        {{QStringLiteral("attempt"), m_pendingReadbackAttempt},
         {QStringLiteral("maxRetries"), m_pendingReadbackMaxRetries}},
        m_pendingReadbackAttempt);

    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (it->state == ParameterState::PendingReadback
                && m_pendingReadbackTargetPointIds.contains(it->pointId)) {
            ++it->readbackAttempts;
        }
    }

    const quint64 generation = m_pendingReadbackGeneration;
    backend->readPointsAsync(pointIds, static_cast<int>(m_pendingDeadline.remainingTime()), m_pendingCancelled, this,
        [this, generation](bool, const QHash<QString, QVariant>& readbackValues, const QString& readbackError,
                           const QHash<QString, CommError>&, const BackendStatusSnapshot&) {
    if (!m_pendingReadbackActive || generation != m_pendingReadbackGeneration) return;
    if (m_pendingDeadline.hasExpired()) { finishReadback(false, QStringLiteral("参数写入/回读总时限已到")); return; }
    QString decisionMessage;

    if (!readbackValues.isEmpty()) {
        applyReadbackValues(readbackValues, m_pendingReadbackTargetPointIds);
    }
    if (!m_pendingReadbackActive || generation != m_pendingReadbackGeneration) return;

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

    QTimer::singleShot(m_pendingReadbackRetryIntervalMs, this, [this, generation]() {
        if (m_pendingReadbackActive && m_pendingReadbackGeneration == generation)
            pollReadbackAttempt();
    });
    });
}

void ParameterController::finishReadback(bool success, const QString& message)
{
    if (m_pendingCancelled) m_pendingCancelled->store(true);
    const QStringList targetPointIds = m_pendingReadbackTargetPointIds;
    const QString finalMessage = message.isEmpty() && !success
            ? QStringLiteral("参数回读失败")
            : message;

    AppLogging::writeBusinessEvent(
        success ? QStringLiteral("parameter_readback_succeeded") : QStringLiteral("parameter_readback_failed"),
        success ? QtInfoMsg : QtWarningMsg,
        m_pendingReadbackOpId,
        QStringLiteral("readback"),
        success ? QStringLiteral("succeeded") : QStringLiteral("failed"),
        success ? 0 : -1,
        targetPointIds.join(QLatin1Char(',')),
        {{QStringLiteral("message"), finalMessage},
         {QStringLiteral("attempts"), m_pendingReadbackAttempt}},
        m_pendingReadbackAttempt);

    m_pendingReadbackActive = false;
    m_pendingReadbackBackend = nullptr;
    m_pendingReadbackTargetPointIds.clear();
    m_pendingReadbackMaxRetries = 0;
    m_pendingReadbackRetryIntervalMs = 0;
    m_pendingReadbackAttempt = 0;
    m_pendingReadbackMessage = finalMessage;
    m_pendingReadbackOpId.clear();
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
    if (m_pendingCancelled) m_pendingCancelled->store(true);

    const QString finalMessage = message.isEmpty()
            ? QStringLiteral("参数回读已取消")
            : message;
    const QStringList targetPointIds = m_pendingReadbackTargetPointIds;

    AppLogging::writeBusinessEvent(
        QStringLiteral("parameter_readback_canceled"),
        QtWarningMsg,
        m_pendingReadbackOpId,
        QStringLiteral("readback"),
        QStringLiteral("canceled"),
        0,
        targetPointIds.join(QLatin1Char(',')),
        {{QStringLiteral("message"), finalMessage}});

    m_pendingReadbackActive = false;
    m_pendingReadbackBackend = nullptr;
    m_pendingReadbackTargetPointIds.clear();
    m_pendingReadbackMaxRetries = 0;
    m_pendingReadbackRetryIntervalMs = 0;
    m_pendingReadbackAttempt = 0;
    m_pendingReadbackMessage = finalMessage;
    m_pendingReadbackOpId.clear();
    ++m_pendingReadbackGeneration;

    setPendingReadbackError(finalMessage, targetPointIds);
    emit readbackFinished(false, finalMessage);
}
