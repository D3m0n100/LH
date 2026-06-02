// File: src/communication/IDeviceBackend.h

#ifndef IDEVICEBACKEND_H
#define IDEVICEBACKEND_H

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include "CommTypes.h"

class IDeviceBackend : public QObject
{
    Q_OBJECT
public:
    explicit IDeviceBackend(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    virtual ~IDeviceBackend() = default;

    virtual bool connectBackend() = 0;
    virtual void disconnectBackend() = 0;
    virtual bool isOnline() const = 0;

    CommError lastError() const
    {
        QMutexLocker locker(&m_errorMutex);
        return m_lastError;
    }

    void clearError()
    {
        QMutexLocker locker(&m_errorMutex);
        m_lastError = CommError();
    }

    /**
     * Read point values.
     * Return true only when all requested points are read successfully.
     * Partial results may still be returned in values/pointErrors.
     */
    virtual bool readPoints(const QStringList& pointIds,
                            QHash<QString, QVariant>& values,
                            QString* errorMessage = nullptr,
                            QHash<QString, CommError>* pointErrors = nullptr) = 0;

    /**
     * Write point values.
     * Return true only when all requested points are written successfully.
     * Partial success is allowed and must be reported through pointErrors.
     */
    virtual bool writePoints(const QHash<QString, QVariant>& writes,
                             QString* errorMessage = nullptr,
                             QHash<QString, CommError>* pointErrors = nullptr) = 0;

    /**
     * Download the compiled artifact to the device.
     */
    virtual bool downloadArtifact(const QString& artifactPath,
                                  const QVariantMap& options,
                                  QString* errorMessage = nullptr,
                                  CommError* operationError = nullptr) = 0;

    /**
     * Structured backend status snapshot.
     * This is the primary status contract for runtime consumers.
     */
    virtual BackendStatusSnapshot statusSnapshot() const = 0;

signals:
    void connectionStateChanged(bool connected);
    void pointsChanged(const QHash<QString, QVariant>& updates);
    void errorOccurred(const CommError& error);
    void errorOccurred(const QString& errorMessage);

protected:
    void reportError(CommErrorCode code, const QString& message, const QString& details = QString())
    {
        CommError error(CommProtocolType::Custom, code, message, details);
        {
            QMutexLocker locker(&m_errorMutex);
            m_lastError = error;
        }

        emit errorOccurred(error);
        emit errorOccurred(error.message);
    }

    mutable QMutex m_errorMutex;
    CommError m_lastError;
};

#endif // IDEVICEBACKEND_H
