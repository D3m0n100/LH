#include <QCoreApplication>
#include <RuntimeSessionService.h>
#include <ProjectCommandCoordinator.h>
#include <AppLogging.h>
#include <DataManager.h>
#include <AsyncDatabaseWorker.h>
#include <DSLCompilerInterface.h>
#include <ClassicOpcServer.h>
#include <ControllerDeviceBackend.h>
#include <../monitor/IMonitorHistoryStore.h>

class ConsumerMonitor : public RuntimeMonitorPort {
public:
    void setDeviceBackend(IDeviceBackend*) override {}
    bool applyConfiguration(const ProjectRuntimeConfig&) override { return true; }
    QStringList providerIds() const override { return {}; }
    void startMonitoring() override {}
    void stopMonitoring() override {}
    bool isMonitoring() const override { return false; }
    void removeDemoChannels() override {}
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    ProjectCommandCoordinator coordinator;
    ProjectCommandCoordinator::SavePorts ports;
    ports.saveDocuments=[](bool) { return true; }; ports.saveProject=[] { return true; };
    if(coordinator.save(true,ports)!=ProjectCommandCoordinator::Result::Completed) return 1;
    ClassicOpcServer server;
    ControllerDeviceBackend backend;
    DSLCompilerInterface compiler;
    RuntimeSessionService runtime(std::make_shared<ConsumerMonitor>());
    runtime.setBackend(&backend);
    runtime.setState(RuntimeSessionState::Compiled);
    if(runtime.state()!=RuntimeSessionState::Compiled || runtime.backend()!=&backend || !runtime.parameters()) return 2;
    return backend.statusSnapshot().backendType.isEmpty() || server.statusSnapshot().backendType.isEmpty();
}
