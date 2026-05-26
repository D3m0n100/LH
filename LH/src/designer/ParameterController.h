// File: src/designer/ParameterController.h
#ifndef PARAMETERCONTROLLER_H
#define PARAMETERCONTROLLER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVariant>
#include <QList>

#include "common/ConfigTypes.h"

class IDeviceBackend;

enum class ParameterState {
    Clean,
    Modified,
    PendingApply,
    Applying,
    PendingReadback,
    Confirmed,
    Mismatch,
    ApplyFailed
};

struct ParameterStateInfo
{
    QString pointId;
    QString name;
    ParameterState state = ParameterState::Clean;
    QString definitionValue;
    QString editedValue;
    QString appliedValue;
    QString readbackValue;
    QString lastError;
    bool onlineEditable = false;
};

class ParameterController : public QObject
{
    Q_OBJECT
public:
    explicit ParameterController(QObject* parent = nullptr);

    void loadDefinitions(const QList<ParameterDefinition>& definitions);
    void clear();

    bool editParameter(const QString& name, const QString& value);
    bool applyModifiedParameters(IDeviceBackend* backend);
    bool applyModifiedParametersWithReadback(IDeviceBackend* backend,
                                             int maxReadbackRetries = 1,
                                             int readbackRetryIntervalMs = 0,
                                             QString* errorMessage = nullptr);
    void onReadbackValues(const QHash<QString, QVariant>& readbackValues);

    ParameterStateInfo parameterState(const QString& name) const;
    QList<ParameterStateInfo> parameterStates() const;
    QStringList parameterNamesByState(ParameterState state) const;
    bool hasModifiedParameters() const;

signals:
    void stateChanged(const QString& name, ParameterState oldState, ParameterState newState);
    void statesChanged();

private:
    QStringList pendingReadbackPointIds() const;
    void setPendingReadbackError(const QString& errorMessage);

    QMap<QString, ParameterStateInfo> m_states;
};

#endif // PARAMETERCONTROLLER_H
