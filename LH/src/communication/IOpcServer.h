#ifndef IOPCSERVER_H
#define IOPCSERVER_H

#include <QObject>
#include <QList>
#include <QString>

#include "../common/ConfigTypes.h"
#include "../common/RuntimePointTypes.h"
#include "CommTypes.h"

class IOpcServer : public QObject
{
    Q_OBJECT
public:
    explicit IOpcServer(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    ~IOpcServer() override = default;

    virtual bool applyConfig(const OpcServerConfig& config, QString* errorMessage = nullptr) = 0;
    virtual bool start(QString* errorMessage = nullptr) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;

    virtual void setRuntimePoints(const QList<RuntimePointDefinition>& points) = 0;
    virtual void setOpcTags(const QList<OpcTagDefinition>& tags) = 0;
    virtual void updatePointValues(const QList<RuntimePointValue>& values) = 0;
    virtual void recordWriteResult(const QString& pointId, bool success, const QString& message) = 0;
    virtual BackendStatusSnapshot statusSnapshot() const = 0;

signals:
    void runningStateChanged(bool running);
    void errorOccurred(const QString& message);
    void writeRequestReceived(const QString& pointId, const QVariant& value);
};

#endif // IOPCSERVER_H
