#ifndef OPCSERVERFACTORY_H
#define OPCSERVERFACTORY_H

#include <QObject>

#include "common/ConfigTypes.h"

class IOpcServer;

class OpcServerFactory
{
public:
    static IOpcServer* createDefault(QObject* parent = nullptr);
    static IOpcServer* createForConfig(const OpcServerConfig& config, QObject* parent = nullptr);
};

#endif // OPCSERVERFACTORY_H
