#include "RunController.h"

#include <QDir>
#include <QFileInfo>

bool RunController::usesModbusTransport(const ProjectRuntimeConfig& config)
{
    const QString protocol = config.transport.protocol.isEmpty()
            ? config.protocol
            : config.transport.protocol;
    return protocol.compare(QStringLiteral("modbus"), Qt::CaseInsensitive) == 0;
}

QString RunController::findArtifactPathFromCompileResult(const CompileResult& compileResult)
{
    for (const CompileArtifact& artifact : compileResult.artifacts) {
        if (artifact.type == QStringLiteral("download")
                && artifact.format == QStringLiteral("dsl_custom")
                && QFileInfo::exists(artifact.path)) {
            return artifact.path;
        }
    }
    return QString();
}

QString RunController::resolveDownloadArtifactPath(const ProjectRuntimeConfig& config,
                                                   const QString& projectPath)
{
    QString artifactPath = config.downloadArtifact.filePath;
    if (!artifactPath.isEmpty() && QFileInfo(artifactPath).isRelative()) {
        artifactPath = QDir(projectPath).absoluteFilePath(artifactPath);
    }
    return artifactPath;
}

QString RunController::findDownloadArtifactPath(const ProjectRuntimeConfig& config,
                                                const QString& projectPath,
                                                const CompileResult& compileResult)
{
    const QString compileArtifactPath = findArtifactPathFromCompileResult(compileResult);
    if (!compileArtifactPath.isEmpty()) {
        return compileArtifactPath;
    }

    const QString configuredPath = resolveDownloadArtifactPath(config, projectPath);
    if (!configuredPath.isEmpty() && QFileInfo::exists(configuredPath)) {
        return configuredPath;
    }

    return QString();
}

bool RunController::writeDownloadArtifact(ProjectRuntimeConfig& config,
                                          const QString& projectPath,
                                          const CompileResult& compileResult)
{
    for (const CompileArtifact& artifact : compileResult.artifacts) {
        if (artifact.type == QStringLiteral("download")
                && artifact.format == QStringLiteral("dsl_custom")
                && QFileInfo::exists(artifact.path)) {
            config.downloadArtifact.artifactType = QStringLiteral("dsl_custom");
            config.downloadArtifact.formatVersion.clear();
            config.downloadArtifact.checksum = artifact.checksum;
            config.downloadArtifact.metadata = artifact.metadata;
            config.downloadArtifact.filePath = QDir(projectPath).relativeFilePath(artifact.path);
            return true;
        }
    }

    return false;
}
