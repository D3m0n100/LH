#include "OpcServerFactory.h"

#include "IOpcServer.h"
#include "ClassicOpcServer.h"
#include "MatrikonOpcServer.h"

IOpcServer* OpcServerFactory::createDefault(QObject* parent)
{
    auto* server = new MatrikonOpcServer(parent);
    server->setObjectName(QStringLiteral("OpcServerFactory::MatrikonOpcServer"));
    return server;
}

IOpcServer* OpcServerFactory::createForConfig(const OpcServerConfig& config, QObject* parent)
{
    if (config.metadata.value(QStringLiteral("backend")).toString().compare(QStringLiteral("classic-modbus"),
                                                                            Qt::CaseInsensitive) == 0) {
        auto* server = new ClassicOpcServer(parent);
        server->setObjectName(QStringLiteral("OpcServerFactory::ClassicOpcServer"));
        return server;
    }

    auto* server = new MatrikonOpcServer(parent);
    server->setObjectName(QStringLiteral("OpcServerFactory::MatrikonOpcServer"));
    return server;
}
