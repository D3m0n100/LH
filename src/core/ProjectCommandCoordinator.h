#pragma once
#include <functional>

// The composition root supplies document/project operations. No editor ownership.
class ProjectCommandCoordinator
{
public:
    enum class Result { Completed, Busy, DocumentsRejected, ProjectRejected };
    struct SavePorts {
        std::function<bool(bool)> saveDocuments;
        std::function<bool()> saveProject;
    };
    Result save(bool allDocuments, const SavePorts& ports);
private:
    bool m_active = false;
};
