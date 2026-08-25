// File: src/designer/ParameterController.h
#ifndef PARAMETERCONTROLLER_H
#define PARAMETERCONTROLLER_H

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QHash>
#include <QVariant>
#include <QList>
#include <QPointer>
#include <QtGlobal>

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
    QString dataType;
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
    bool applyParameterByPointIdWithReadbackAsync(IDeviceBackend* backend,
                                                  const QString& pointId,
                                                  int maxReadbackRetries = 1,
                                                  int readbackRetryIntervalMs = 0,
                                                  QString* errorMessage = nullptr);
    void cancelPendingReadback(const QString& message = QString());
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
    enum class ReadbackDecision {
        Success,
        Continue,
        Failure
    };

    bool applyModifiedParametersForTargets(IDeviceBackend* backend,
                                           const QStringList& targetPointIds,
                                           QStringList* batchTargetPointIds,
                                           QString* errorMessage);
    bool applyModifiedParametersWithReadbackAsyncForTargets(
            IDeviceBackend* backend,
            const QStringList& targetPointIds,
            int maxReadbackRetries,
            int readbackRetryIntervalMs,
            QString* errorMessage);
    QStringList pendingReadbackPointIds(const QStringList& targetPointIds) const;
    void applyReadbackValues(const QHash<QString, QVariant>& readbackValues,
                             const QStringList& targetPointIds);
    ReadbackDecision evaluateReadback(const QStringList& targetPointIds,
                                      QString* message) const;
    void setPendingReadbackError(const QString& errorMessage,
                                 const QStringList& targetPointIds);
    void pollReadbackAttempt();
    void finishReadback(bool success, const QString& message);

    QMap<QString, ParameterStateInfo> m_states;
    QPointer<IDeviceBackend> m_pendingReadbackBackend;
    int m_pendingReadbackMaxRetries = 0;
    int m_pendingReadbackRetryIntervalMs = 0;
    int m_pendingReadbackAttempt = 0;
    bool m_pendingReadbackActive = false;
    QStringList m_pendingReadbackTargetPointIds;
    QString m_pendingReadbackMessage;
    quint64 m_pendingReadbackGeneration = 0;
};

#endif // PARAMETERCONTROLLER_H
