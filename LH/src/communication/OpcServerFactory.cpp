#include "OpcServerFactory.h"

#include "IOpcServer.h"
#include "ClassicOpcServer.h"

IOpcServer* OpcServerFactory::createDefault(QObject* parent)
{
    auto* server = new ClassicOpcServer(parent);
    server->setObjectName(QStringLiteral("OpcServerFactory::ClassicOpcServer"));
    return server;
}

IOpcServer* OpcServerFactory::createForConfig(const OpcServerConfig& config, QObject* parent)
{
    Q_UNUSED(config);

    auto* server = new ClassicOpcServer(parent);
    server->setObjectName(QStringLiteral("OpcServerFactory::ClassicOpcServer"));
    return server;
}
