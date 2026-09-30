#include "ProjectCommandCoordinator.h"
#include <QScopedValueRollback>

ProjectCommandCoordinator::Result ProjectCommandCoordinator::save(
    bool allDocuments, const SavePorts& ports)
{
    if (m_active) return Result::Busy;
    QScopedValueRollback<bool> active(m_active, true);
    if (!ports.saveDocuments || !ports.saveDocuments(allDocuments))
        return Result::DocumentsRejected;
    if (!ports.saveProject || !ports.saveProject())
        return Result::ProjectRejected;
    return Result::Completed;
}
