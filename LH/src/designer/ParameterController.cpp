// File: src/designer/ParameterController.cpp

#include "ParameterController.h"
#include "../communication/IDeviceBackend.h"

#include <QThread>
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

    const QStringList pointIds = pendingReadbackPointIds();
    if (pointIds.isEmpty()) {
        return true;
    }

    const int retryCount = qMax(1, maxReadbackRetries);
    QString readbackError;
    QHash<QString, QVariant> readbackValues;

    for (int attempt = 0; attempt < retryCount; ++attempt) {
        readbackValues.clear();
        readbackError.clear();
        if (backend->readPoints(pointIds, readbackValues, &readbackError)) {
            onReadbackValues(readbackValues);
            return true;
        }

        if (attempt + 1 < retryCount && readbackRetryIntervalMs > 0) {
            QThread::msleep(static_cast<unsigned long>(readbackRetryIntervalMs));
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

void ParameterController::onReadbackValues(const QHash<QString, QVariant>& readbackValues)
{
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
    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        if (it->state == ParameterState::PendingReadback) {
            it->lastError = errorMessage;
        }
    }
    emit statesChanged();
}
