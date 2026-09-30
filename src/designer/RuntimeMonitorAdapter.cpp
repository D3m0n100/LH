#include "RuntimeMonitorAdapter.h"
#include "../monitor/MonitorManager.h"
#include <QPointer>
namespace {
class MonitorAdapter final : public RuntimeMonitorPort {
public:
    explicit MonitorAdapter(Monitor::MonitorManager& manager) : m_manager(&manager) {}
    void setDeviceBackend(IDeviceBackend* backend) override { if (m_manager) m_manager->setDeviceBackend(backend); }
    bool applyConfiguration(const ProjectRuntimeConfig& config) override { return m_manager && m_manager->applyConfiguration(config); }
    QStringList providerIds() const override { return m_manager ? m_manager->providerIds() : QStringList(); }
    void startMonitoring() override { if (m_manager) m_manager->startMonitoring(); }
    void stopMonitoring() override { if (m_manager) m_manager->stopMonitoring(); }
    bool isMonitoring() const override { return m_manager && m_manager->isMonitoring(); }
    void removeDemoChannels() override {
        if (!m_manager) return;
        for (const auto& name : m_manager->channelNames()) {
            if (!m_manager) break;
            if (m_manager->channelConfig(name).metadata.value(QStringLiteral("__demoMode")).toBool())
                m_manager->removeChannel(name);
        }
    }
private:
    QPointer<Monitor::MonitorManager> m_manager;
};
}
std::shared_ptr<RuntimeMonitorPort> makeRuntimeMonitorPort(Monitor::MonitorManager& manager)
{ return std::make_shared<MonitorAdapter>(manager); }
