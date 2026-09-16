#include "ProfileResolution.h"

#include <QDir>
#include <QFileInfo>

namespace {
QString firstPath(const QVariantMap& values, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString value = values.value(key).toString().trimmed();
        if (!value.isEmpty())
            return value;
    }
    return QString();
}

QString absolutePath(const QString& path, const QString& projectPath)
{
    if (path.trimmed().isEmpty())
        return QString();
    const QFileInfo info(path);
    return QDir::cleanPath(info.isRelative() && !projectPath.trimmed().isEmpty()
                                   ? QDir(projectPath).absoluteFilePath(path)
                                   : info.absoluteFilePath());
}

QString comparablePath(const QString& path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(QFileInfo(path).absoluteFilePath()) : canonical;
}
}

QVariantMap resolveDownloadProfileOptions(const QVariantMap& options,
                                          const ProjectRuntimeConfig& config,
                                          const QString& projectPath)
{
    QVariantMap resolved = options;
    const QStringList profileKeys = {QStringLiteral("downloadProfilePath"),
                                     QStringLiteral("profileJsonPath"),
                                     QStringLiteral("profilePath")};
    const QStringList sourceKeys = {QStringLiteral("downloadProfileSourcePath"),
                                    QStringLiteral("profileSourcePath"),
                                    QStringLiteral("sourceProfilePath")};
    const QString optionPath = firstPath(options, profileKeys);
    QString configuredPath = firstPath(config.downloadArtifact.metadata, profileKeys);
    const bool publishedBinding = !config.downloadArtifact.metadata.value(QStringLiteral("generationId")).toString().trimmed().isEmpty()
            || !config.downloadArtifact.metadata.value(QStringLiteral("runtimeManifestPath")).toString().trimmed().isEmpty();
    if (configuredPath.isEmpty() && !publishedBinding)
        configuredPath = firstPath(config.downloadArtifact.metadata, sourceKeys);

    const QString configuredAbsolute = absolutePath(configuredPath, projectPath);
    const QString optionAbsolute = absolutePath(optionPath, projectPath);
    if (publishedBinding && !optionAbsolute.isEmpty()) {
        if (configuredAbsolute.isEmpty()
                || comparablePath(optionAbsolute) != comparablePath(configuredAbsolute)) {
            resolved.insert(QStringLiteral("profileOverrideConflict"), true);
            resolved.insert(QStringLiteral("profileOverrideError"),
                            QStringLiteral("下载 Profile override 与已发布 generation 不一致。"));
        }
    }
    const QString effective = !configuredAbsolute.isEmpty() ? configuredAbsolute : optionAbsolute;
    if (!effective.isEmpty()) {
        resolved.insert(QStringLiteral("downloadProfilePath"), effective);
        for (const QString& key : profileKeys) {
            if (key != QStringLiteral("downloadProfilePath"))
                resolved.remove(key);
        }
    }
    return resolved;
}
