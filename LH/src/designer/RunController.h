#ifndef RUNCONTROLLER_H
#define RUNCONTROLLER_H

#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "common/ConfigTypes.h"
#include "compiler/DSLCompilerInterface.h"

class RunController
{
public:
    struct DownloadArtifactPrecheckReport {
        bool valid = true;
        QStringList errors;
        QStringList warnings;
        QVariantMap details;
    };

    static bool usesModbusTransport(const ProjectRuntimeConfig& config);
    static QString findArtifactPathFromCompileResult(const CompileResult& compileResult);

    static QString resolveDownloadArtifactPath(const ProjectRuntimeConfig& config,
                                               const QString& projectPath);

    static QString findDownloadArtifactPath(const ProjectRuntimeConfig& config,
                                            const QString& projectPath,
                                            const CompileResult& compileResult = CompileResult());

    static bool writeDownloadArtifact(ProjectRuntimeConfig& config,
                                      const QString& projectPath,
                                      const CompileResult& compileResult);

    static DownloadArtifactPrecheckReport validateDownloadArtifact(
            const ProjectRuntimeConfig& config,
            const QString& projectPath,
            const QString& artifactPath);
};

#endif // RUNCONTROLLER_H
