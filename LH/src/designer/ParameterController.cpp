// File: src/designer/ParameterController.cpp

#include "ParameterController.h"
#include "../communication/IDeviceBackend.h"

#include <QEventLoop>
#include <QTimer>
#include <QtGlobal>

namespace {
bool valuesMatch(const QString& appliedValue, const QString& readbackValue)
{
    bool appliedOk = false;
    bool readbackOk = false;
    const double applied = appliedValue.toDouble(&appliedOk);
    const double readback = readbackValue.toDouble(&readbackOk);

    if (appliedOk && readbackOk) {
        constexpr double kTolerance = 1e-6;
        return qAbs(applied - readback) < kTolerance;
    }

    return appliedValue.trimmed() == readbackValue.trimmed();
}
}

ParameterController::ParameterController(QObject* parent)
    : QObject(parent)
{
}

void ParameterController::loadDefinitions(const QList<ParameterDefinition>& definitions)
{
    QMap<QString, ParameterStateInfo> newStates;

    for (const auto& def : definitions) {
        ParameterStateInfo info;
        info.pointId = def.id;
        info.name = def.name;
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

bool ParameterController::applyModifiedParameters(IDeviceBackend* backend)
{
    if (!backend)
        return false;

    QHash<QString, QVariant> writes;
    QStringList modifiedNames;
    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (it->state != ParameterState::Modified)
            continue;
        if (it->pointId.isEmpty())
            continue;

        writes.insert(it->pointId, it->editedValue);
        modifiedNames.append(it->name);
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

    QString errorMsg;
    const bool ok = backend->writePoints(writes, &errorMsg);

    if (ok) {
        for (const auto& name : modifiedNames) {
            auto& info = m_states[name];
            const ParameterState oldState = info.state;
            info.state = ParameterState::PendingReadback;
            info.appliedValue = info.editedValue;
            info.lastError.clear();
            info.lastWriteTime = QDateTime::currentDateTimeUtc();
            info.lastReadbackTime = QDateTime();
            info.readbackAttempts = 0;
            emit stateChanged(name, oldState, ParameterState::PendingReadback);
        }
        emit statesChanged();
        return true;
    }

    for (const auto& name : modifiedNames) {
        auto& info = m_states[name];
        const ParameterState oldState = info.state;
        info.state = ParameterState::ApplyFailed;
        info.lastError = errorMsg;
        emit stateChanged(name, oldState, ParameterState::ApplyFailed);
    }
    emit statesChanged();
    return false;
}

bool ParameterController::applyModifiedParametersWithReadback(IDeviceBackend* backend,
                                                              int maxReadbackRetries,
                                                              int readbackRetryIntervalMs,
                                                              QString* errorMessage)
{
    if (!applyModifiedParameters(backend)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("参数下发失败");
        }
        return false;
    }

    const int retryCount = qMax(1, maxReadbackRetries);
    QString readbackError;
    QHash<QString, QVariant> readbackValues;

    for (int attempt = 0; attempt < retryCount; ++attempt) {
        const QStringList pointIds = pendingReadbackPointIds();
        if (pointIds.isEmpty()) {
            return true;
        }

        for (auto it = m_states.begin(); it != m_states.end(); ++it) {
            if (it->state == ParameterState::PendingReadback && !it->pointId.isEmpty()) {
                ++it->readbackAttempts;
            }
        }

        readbackValues.clear();
        readbackError.clear();
        const bool readOk = backend->readPoints(pointIds, readbackValues, &readbackError);
        if (!readbackValues.isEmpty()) {
            onReadbackValues(readbackValues);
            if (pendingReadbackPointIds().isEmpty()) {
                return true;
            }
        }

        if (readOk && pendingReadbackPointIds().isEmpty()) {
            return true;
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

    if (readbackError.isEmpty()) {
        readbackError = QStringLiteral("参数回读失败");
    }
    setPendingReadbackError(readbackError);
    if (errorMessage) {
        *errorMessage = readbackError;
    }
    return false;
}

bool ParameterController::applyModifiedParametersWithReadbackAsync(IDeviceBackend* backend,
                                                                   int maxReadbackRetries,
                                                                   int readbackRetryIntervalMs,
                                                                   QString* errorMessage)
{
    if (!backend) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("参数下发失败：后端为空");
        }
        return false;
    }

    if (m_pendingReadbackActive) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("参数回读正在进行中");
        }
        return false;
    }

    if (!applyModifiedParameters(backend)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("参数下发失败");
        }
        return false;
    }

    if (pendingReadbackPointIds().isEmpty()) {
        emit readbackFinished(true, QString());
        return true;
    }

    m_pendingReadbackBackend = backend;
    m_pendingReadbackMaxRetries = qMax(1, maxReadbackRetries);
    m_pendingReadbackRetryIntervalMs = qMax(0, readbackRetryIntervalMs);
    m_pendingReadbackAttempt = 0;
    m_pendingReadbackMessage.clear();
    m_pendingReadbackActive = true;

    QTimer::singleShot(0, this, [this]() {
        pollReadbackAttempt();
    });
    return true;
}

void ParameterController::onReadbackValues(const QHash<QString, QVariant>& readbackValues)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();

    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (it->state != ParameterState::PendingReadback)
            continue;

        auto rvIt = readbackValues.constFind(it->pointId);
        if (rvIt == readbackValues.constEnd())
            rvIt = readbackValues.constFind(it->name);
        if (rvIt == readbackValues.constEnd())
            continue;

        const ParameterState oldState = it->state;
        it->readbackValue = rvIt->toString();
        it->lastReadbackTime = now;
        it->state = valuesMatch(it->appliedValue, it->readbackValue)
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

QStringList ParameterController::pendingReadbackPointIds() const
{
    QStringList pointIds;
    for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it) {
        if (it->state == ParameterState::PendingReadback && !it->pointId.isEmpty()) {
            pointIds.append(it->pointId);
        }
    }
    return pointIds;
}

void ParameterController::setPendingReadbackError(const QString& errorMessage)
{
    QStringList timedOutNames;
    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (it->state == ParameterState::PendingReadback) {
            it->lastError = errorMessage;
            it->state = ParameterState::Timeout;
            timedOutNames.append(it.key());
        }
    }
    for (const auto& name : timedOutNames) {
        emit stateChanged(name, ParameterState::PendingReadback, ParameterState::Timeout);
    }
    emit statesChanged();
}

void ParameterController::pollReadbackAttempt()
{
    if (!m_pendingReadbackActive) {
        return;
    }

    if (!m_pendingReadbackBackend) {
        finishReadback(false, QStringLiteral("参数回读失败：后端不可用"));
        return;
    }

    const QStringList pointIds = pendingReadbackPointIds();
    if (pointIds.isEmpty()) {
        finishReadback(true, QString());
        return;
    }

    ++m_pendingReadbackAttempt;
    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (it->state == ParameterState::PendingReadback && !it->pointId.isEmpty()) {
            ++it->readbackAttempts;
        }
    }

    QHash<QString, QVariant> readbackValues;
    QString readbackError;
    m_pendingReadbackBackend->readPoints(pointIds, readbackValues, &readbackError);

    if (!readbackValues.isEmpty()) {
        onReadbackValues(readbackValues);
    }

    const QStringList mismatchNames = parameterNamesByState(ParameterState::Mismatch);
    if (!mismatchNames.isEmpty()) {
        finishReadback(false, QStringLiteral("参数回读不匹配：%1")
                                   .arg(mismatchNames.join(QStringLiteral(", "))));
        return;
    }

    if (pendingReadbackPointIds().isEmpty()) {
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

    QTimer::singleShot(m_pendingReadbackRetryIntervalMs, this, [this]() {
        pollReadbackAttempt();
    });
}

void ParameterController::finishReadback(bool success, const QString& message)
{
    m_pendingReadbackActive = false;
    m_pendingReadbackBackend = nullptr;
    m_pendingReadbackMaxRetries = 0;
    m_pendingReadbackRetryIntervalMs = 0;
    m_pendingReadbackAttempt = 0;
    m_pendingReadbackMessage = message;

    if (!success) {
        if (message.isEmpty()) {
            setPendingReadbackError(QStringLiteral("参数回读失败"));
        } else {
            setPendingReadbackError(message);
        }
    } else {
        emit statesChanged();
    }

    emit readbackFinished(success, message);
}
