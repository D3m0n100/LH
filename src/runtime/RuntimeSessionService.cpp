#include "RuntimeSessionService.h"
#include <stdexcept>

RuntimeSessionService::RuntimeSessionService(std::shared_ptr<RuntimeMonitorPort> monitor,
    std::shared_ptr<Monitor::IMonitorHistoryStore> history, QObject* parent)
    : QObject(parent), m_monitor(std::move(monitor)), m_history(std::move(history)),
      m_parameters(new ParameterController(this)), m_cancelled(std::make_shared<std::atomic_bool>(false))
{
    if (!m_monitor) throw std::invalid_argument("RuntimeSessionService requires an explicit monitor port");
    qRegisterMetaType<RuntimeSessionState>();
    qRegisterMetaType<DownloadState>();
}
void RuntimeSessionService::setBackend(IDeviceBackend* backend)
{
    if (m_backend == backend) { m_monitor->setDeviceBackend(backend); return; }
    cancelOperations();
    m_backend = backend;
    const auto binding = ++m_backendBindingGeneration;
    if (backend) connect(backend, &QObject::destroyed, this, [this, binding] {
        if (binding != m_backendBindingGeneration) return;
        m_backend = nullptr;
        cancelOperations();
        m_monitor->setDeviceBackend(nullptr);
        if (m_state != RuntimeSessionState::Idle) setState(RuntimeSessionState::Fault);
    });
    m_monitor->setDeviceBackend(backend);
}
void RuntimeSessionService::setParameters(ParameterController* parameters)
{
    if (m_parameters && m_parameters != parameters)
        m_parameters->cancelPendingReadback(QStringLiteral("Parameter service replaced"));
    m_parameters = parameters;
}
bool RuntimeSessionService::applyParameters(QString* errorMessage)
{
    if (!m_parameters) {
        if (errorMessage) *errorMessage = QStringLiteral("Parameter service unavailable");
        return false;
    }
    return m_parameters->applyModifiedParametersWithReadbackAsync(m_backend.data(), 1, 0, errorMessage);
}
void RuntimeSessionService::setState(RuntimeSessionState state)
{
    if (m_state == state) return;
    const auto old = m_state; m_state = state;
    emit stateChanged(old, state);
}
void RuntimeSessionService::setDownloadState(DownloadState state)
{
    if (m_downloadState == state) return;
    const auto old = m_downloadState; m_downloadState = state;
    emit downloadStateChanged(old, state);
}
void RuntimeSessionService::cancelOperations()
{
    m_cancelled->store(true);
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    ++m_generation;
    if (m_backend) m_backend->cancelDownload();
    if (m_parameters) m_parameters->cancelPendingReadback(QStringLiteral("Runtime operation cancelled"));
    if (m_history) m_history->cancelPendingRequests();
    m_monitor->stopMonitoring();
}
void RuntimeSessionService::stop()
{
    cancelOperations();
    setDownloadState(DownloadState::Idle);
    setState(RuntimeSessionState::Idle);
}
RunController::DownloadArtifactPrecheckReport RuntimeSessionService::precheck(
    const ProjectRuntimeConfig& config, const QString& projectPath,
    const QString& artifactPath, bool requirePublishedBinding) const
{
    return RunController::validateDownloadArtifact(config, projectPath, artifactPath, requirePublishedBinding);
}
