// ProjectController stub for runtime_session_controller_test
// Only provides methods that are NOT inline in the header

#include "ProjectController.h"

ProjectController::ProjectController(QObject* parent) : QObject(parent) {}
ProjectController::~ProjectController() = default;

void ProjectController::setModified(bool modified) { m_modified = modified; }

bool ProjectController::validateConfiguration(QStringList& errors)
{
    Q_UNUSED(errors);
    return true;
}
