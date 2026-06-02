#ifndef DIAGNOSTICSNAPSHOTSERVICE_H
#define DIAGNOSTICSNAPSHOTSERVICE_H

#include <QString>
#include <QVariantMap>

#include "common/ConfigTypes.h"

class DiagnosticSnapshotService
{
public:
    static bool exportSnapshot(const QString& baseDir,
                               const ProjectRuntimeConfig& config,
                               bool opcRunning,
                               const QString& opcLastError,
                               const QVariantMap& opcExtras,
                               QString* outFilePath = nullptr,
                               QString* outError = nullptr);
};

#endif // DIAGNOSTICSNAPSHOTSERVICE_H
