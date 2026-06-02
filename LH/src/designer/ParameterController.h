// File: src/designer/ParameterController.h
#ifndef PARAMETERCONTROLLER_H
#define PARAMETERCONTROLLER_H

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QStringList>
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
    Timeout,
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
    QDateTime lastWriteTime;
    QDateTime lastReadbackTime;
    int readbackAttempts = 0;
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
    bool editParameterByPointId(const QString& pointId, const QString& value);
    bool applyModifiedParameters(IDeviceBackend* backend);
    bool applyModifiedParametersWithReadback(IDeviceBackend* backend,
                                             int maxReadbackRetries = 1,
                                             int readbackRetryIntervalMs = 0,
                                             QString* errorMessage = nullptr);
    bool applyModifiedParametersWithReadbackAsync(IDeviceBackend* backend,
                                                  int maxReadbackRetries = 1,
                                                  int readbackRetryIntervalMs = 0,
                                                  QString* errorMessage = nullptr);
    void onReadbackValues(const QHash<QString, QVariant>& readbackValues);

    ParameterStateInfo parameterState(const QString& name) const;
    ParameterStateInfo parameterStateByPointId(const QString& pointId) const;
    QList<ParameterStateInfo> parameterStates() const;
    QStringList parameterNamesByState(ParameterState state) const;
    bool hasModifiedParameters() const;

signals:
    void stateChanged(const QString& name, ParameterState oldState, ParameterState newState);
    void statesChanged();
    void readbackFinished(bool success, const QString& message);

private:
    QStringList pendingReadbackPointIds() const;
    void setPendingReadbackError(const QString& errorMessage);
    void pollReadbackAttempt();
    void finishReadback(bool success, const QString& message);

    QMap<QString, ParameterStateInfo> m_states;
    IDeviceBackend* m_pendingReadbackBackend = nullptr;
    int m_pendingReadbackMaxRetries = 0;
    int m_pendingReadbackRetryIntervalMs = 0;
    int m_pendingReadbackAttempt = 0;
    bool m_pendingReadbackActive = false;
    QString m_pendingReadbackMessage;
};

#endif // PARAMETERCONTROLLER_H
