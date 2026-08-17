#include "RunController.h"

#include "common/RuntimePointTypes.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QString absolutePathFromBase(const QString& path, const QString& basePath)
{
    if (path.trimmed().isEmpty()) {
        return QString();
    }

    QFileInfo info(path);
    if (info.isRelative()) {
        info = QFileInfo(QDir(basePath).absoluteFilePath(path));
    }
    return info.absoluteFilePath();
}

QString comparablePath(const QString& path)
{
    const QFileInfo info(path);
    QString value = info.canonicalFilePath();
    if (value.isEmpty()) {
        value = info.absoluteFilePath();
    }
    return QDir::cleanPath(value).toLower();
}

bool pathMatches(const QString& candidatePath,
                 const QFileInfo& artifactInfo,
                 const QString& artifactDir)
{
    const QString candidateAbsolute = absolutePathFromBase(candidatePath, artifactDir);
    if (!candidateAbsolute.isEmpty()
            && comparablePath(candidateAbsolute) == comparablePath(artifactInfo.absoluteFilePath())) {
        return true;
    }
    return QFileInfo(candidatePath).fileName().compare(artifactInfo.fileName(), Qt::CaseInsensitive) == 0;
}

QString sha256ForFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        hash.addData(file.read(64 * 1024));
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool readJsonDocument(const QString& path, QJsonDocument* document, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取文件：%1").arg(path);
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("JSON 解析失败：%1").arg(parseError.errorString());
        }
        return false;
    }

    if (document) {
        *document = parsed;
    }
    return true;
}

QVariantList stringListToVariantList(const QStringList& values)
{
    QVariantList list;
    list.reserve(values.size());
    for (const QString& value : values) {
        list.append(value);
    }
    return list;
}
} // namespace

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
            QVariantMap mergedMetadata = config.downloadArtifact.metadata;
            for (auto it = artifact.metadata.constBegin(); it != artifact.metadata.constEnd(); ++it) {
                mergedMetadata.insert(it.key(), it.value());
            }
            config.downloadArtifact.metadata = mergedMetadata;
            config.downloadArtifact.filePath = QDir(projectPath).relativeFilePath(artifact.path);
            return true;
        }
    }

    return false;
}

RunController::DownloadArtifactPrecheckReport RunController::validateDownloadArtifact(
        const ProjectRuntimeConfig& config,
        const QString& projectPath,
        const QString& artifactPath)
{
    DownloadArtifactPrecheckReport report;
    QVariantMap details;

    const QString trimmedPath = artifactPath.trimmed();
    details.insert(QStringLiteral("artifactPath"), trimmedPath);
    if (trimmedPath.isEmpty()) {
        report.errors << QStringLiteral("产物路径为空。");
        report.valid = false;
        details.insert(QStringLiteral("errors"), stringListToVariantList(report.errors));
        details.insert(QStringLiteral("warnings"), stringListToVariantList(report.warnings));
        report.details = details;
        return report;
    }

    const QFileInfo artifactInfo(trimmedPath);
    details.insert(QStringLiteral("artifactAbsolutePath"), artifactInfo.absoluteFilePath());
    details.insert(QStringLiteral("artifactExists"), artifactInfo.exists());
    details.insert(QStringLiteral("artifactSuffix"), artifactInfo.suffix());

    if (!artifactInfo.exists() || !artifactInfo.isFile()) {
        report.errors << QStringLiteral("产物文件不存在：%1").arg(trimmedPath);
    } else {
        details.insert(QStringLiteral("artifactBytes"), artifactInfo.size());
        if (!artifactInfo.isReadable()) {
            report.errors << QStringLiteral("产物文件不可读：%1").arg(artifactInfo.absoluteFilePath());
        }
        if (artifactInfo.suffix().compare(QStringLiteral("code"), Qt::CaseInsensitive) != 0) {
            report.warnings << QStringLiteral("产物后缀不是 .code：%1").arg(artifactInfo.fileName());
        }
    }

    const QString configuredPath = resolveDownloadArtifactPath(config, projectPath);
    details.insert(QStringLiteral("configuredArtifactPath"), configuredPath);
    if (!configuredPath.isEmpty()
            && !artifactInfo.absoluteFilePath().isEmpty()
            && comparablePath(configuredPath) != comparablePath(artifactInfo.absoluteFilePath())) {
        report.warnings << QStringLiteral("当前下载产物和项目记录的产物路径不一致。");
    }

    if (artifactInfo.exists() && artifactInfo.isFile() && artifactInfo.isReadable()) {
        const QString actualChecksum = sha256ForFile(artifactInfo.absoluteFilePath());
        const QString expectedChecksum = config.downloadArtifact.checksum.trimmed();
        details.insert(QStringLiteral("checksumActual"), actualChecksum);
        details.insert(QStringLiteral("checksumExpected"), expectedChecksum);
        if (expectedChecksum.isEmpty()) {
            report.warnings << QStringLiteral("项目配置未记录产物 checksum，无法确认是否为最新编译产物。");
        } else if (actualChecksum.compare(expectedChecksum, Qt::CaseInsensitive) != 0) {
            report.errors << QStringLiteral("产物 checksum 不一致，当前文件可能不是最近一次编译结果。");
        }
    }

    const QString artifactDir = artifactInfo.absoluteDir().absolutePath();
    const QString manifestPath = QDir(artifactDir).absoluteFilePath(QStringLiteral("runtime_manifest.json"));
    const QString runtimePointsPath = QDir(artifactDir).absoluteFilePath(QStringLiteral("runtime_points.json"));
    details.insert(QStringLiteral("manifestPath"), manifestPath);
    details.insert(QStringLiteral("runtimePointsPath"), runtimePointsPath);

    const int expectedPointCount = RuntimePointConverter::fromProjectConfig(config).size();
    const int expectedParameterCount = config.parameters.size();
    details.insert(QStringLiteral("expectedRuntimePointCount"), expectedPointCount);
    details.insert(QStringLiteral("expectedParameterCount"), expectedParameterCount);

    const QFileInfo manifestInfo(manifestPath);
    details.insert(QStringLiteral("manifestExists"), manifestInfo.exists());
    if (!manifestInfo.exists()) {
        report.warnings << QStringLiteral("未找到 runtime_manifest.json，无法校验产物清单。");
    } else {
        QJsonDocument manifestDoc;
        QString jsonError;
        if (!readJsonDocument(manifestPath, &manifestDoc, &jsonError) || !manifestDoc.isObject()) {
            report.errors << QStringLiteral("runtime_manifest.json 无效：%1").arg(jsonError);
        } else {
            const QJsonObject manifest = manifestDoc.object();
            const QJsonArray artifactPaths = manifest.value(QStringLiteral("artifactPaths")).toArray();
            details.insert(QStringLiteral("manifestArtifactCount"), artifactPaths.size());
            bool containsArtifact = false;
            for (const QJsonValue& value : artifactPaths) {
                if (pathMatches(value.toString(), artifactInfo, artifactDir)) {
                    containsArtifact = true;
                    break;
                }
            }
            details.insert(QStringLiteral("manifestContainsArtifact"), containsArtifact);
            if (!artifactPaths.isEmpty() && !containsArtifact) {
                report.errors << QStringLiteral("runtime_manifest.json 未包含当前下载产物。");
            }

            if (manifest.contains(QStringLiteral("pointCount"))) {
                const int manifestPointCount = manifest.value(QStringLiteral("pointCount")).toInt(-1);
                details.insert(QStringLiteral("manifestPointCount"), manifestPointCount);
                if (manifestPointCount >= 0 && manifestPointCount != expectedPointCount) {
                    report.warnings << QStringLiteral("runtime_manifest.json 点位数量和当前项目配置不一致：%1/%2")
                                      .arg(manifestPointCount)
                                      .arg(expectedPointCount);
                }
            }

            if (manifest.contains(QStringLiteral("parameterCount"))) {
                const int manifestParameterCount = manifest.value(QStringLiteral("parameterCount")).toInt(-1);
                details.insert(QStringLiteral("manifestParameterCount"), manifestParameterCount);
                if (manifestParameterCount >= 0 && manifestParameterCount != expectedParameterCount) {
                    report.warnings << QStringLiteral("runtime_manifest.json 参数数量和当前项目配置不一致：%1/%2")
                                      .arg(manifestParameterCount)
                                      .arg(expectedParameterCount);
                }
            }
        }
    }

    const QFileInfo runtimePointsInfo(runtimePointsPath);
    details.insert(QStringLiteral("runtimePointsExists"), runtimePointsInfo.exists());
    if (!runtimePointsInfo.exists()) {
        report.warnings << QStringLiteral("未找到 runtime_points.json，无法校验点位映射文件。");
    } else {
        QJsonDocument pointsDoc;
        QString jsonError;
        if (!readJsonDocument(runtimePointsPath, &pointsDoc, &jsonError) || !pointsDoc.isArray()) {
            report.errors << QStringLiteral("runtime_points.json 无效：%1").arg(jsonError);
        } else {
            const int runtimePointCount = pointsDoc.array().size();
            details.insert(QStringLiteral("runtimePointCount"), runtimePointCount);
            if (runtimePointCount != expectedPointCount) {
                report.warnings << QStringLiteral("runtime_points.json 点位数量和当前项目配置不一致：%1/%2")
                                  .arg(runtimePointCount)
                                  .arg(expectedPointCount);
            }
        }
    }

    report.valid = report.errors.isEmpty();
    details.insert(QStringLiteral("valid"), report.valid);
    details.insert(QStringLiteral("errors"), stringListToVariantList(report.errors));
    details.insert(QStringLiteral("warnings"), stringListToVariantList(report.warnings));
    report.details = details;
    return report;
}
