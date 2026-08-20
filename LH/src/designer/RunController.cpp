#include "RunController.h"

#include "common/RuntimePointTypes.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace {

QString sha256ForFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
        hash.addData(file.read(64 * 1024));
    return QString::fromLatin1(hash.result().toHex());
}

QVariantList stringListToVariantList(const QStringList& values)
{
    QVariantList list;
    for (const QString& value : values)
        list.append(value);
    return list;
}

bool hasParentTraversal(const QString& path)
{
    const QStringList parts = QDir::fromNativeSeparators(path).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        if (part == QStringLiteral(".."))
            return true;
    }
    return false;
}

bool safeManifestRelativePath(const QString& path)
{
    return !path.trimmed().isEmpty()
            && !QDir::isAbsolutePath(path)
            && !hasParentTraversal(path);
}

bool pathWithinRoot(const QString& root, const QString& path)
{
    const QString relative = QDir(root).relativeFilePath(path);
    return relative != QStringLiteral("..")
            && !relative.startsWith(QStringLiteral("../"))
            && !relative.startsWith(QStringLiteral("..\\"))
            && !QDir::isAbsolutePath(relative);
}

QString comparablePath(const QString& path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(QFileInfo(path).absoluteFilePath()) : canonical;
}

bool resolveContainedPath(const QString& root,
                          const QString& configured,
                          QString* resolved,
                          QString* errorMessage,
                          bool requireRelative = false)
{
    const QString trimmed = configured.trimmed();
    if (trimmed.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("路径为空。");
        return false;
    }
    if (requireRelative && !safeManifestRelativePath(trimmed)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("manifest 路径必须是安全的相对路径：%1").arg(configured);
        return false;
    }
    if (QFileInfo(trimmed).isRelative() && hasParentTraversal(trimmed)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("路径禁止包含 '..'：%1").arg(configured);
        return false;
    }

    const QString absolute = QFileInfo(trimmed).isRelative()
            ? QDir(root).absoluteFilePath(trimmed)
            : QFileInfo(trimmed).absoluteFilePath();
    const QString clean = QDir::cleanPath(absolute);
    if (!pathWithinRoot(root, clean)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("路径越出允许目录：%1").arg(configured);
        return false;
    }

    const QFileInfo info(clean);
    if (info.exists()) {
        const QString canonical = info.canonicalFilePath();
        if (canonical.isEmpty() || !pathWithinRoot(root, canonical)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("符号链接越出允许目录：%1").arg(configured);
            return false;
        }
    } else {
        QDir parent = info.dir();
        while (!parent.exists() && parent.absolutePath() != parent.dir().absolutePath())
            parent = parent.dir();
        const QString canonicalParent = parent.canonicalPath();
        if (canonicalParent.isEmpty() || !pathWithinRoot(root, canonicalParent)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("父目录越出允许目录：%1").arg(configured);
            return false;
        }
    }

    if (resolved)
        *resolved = clean;
    return true;
}

bool readJsonDocument(const QString& path, QJsonDocument* document, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法读取文件：%1").arg(path);
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage)
            *errorMessage = QStringLiteral("JSON 解析失败：%1").arg(parseError.errorString());
        return false;
    }
    if (document)
        *document = parsed;
    return true;
}

bool legacyManifestPathMatches(const QString& candidatePath,
                               const QFileInfo& artifactInfo,
                               const QString& artifactDirectory)
{
    const QFileInfo candidateInfo(candidatePath);
    const QString candidateAbsolute = candidateInfo.isRelative()
            ? QDir(artifactDirectory).absoluteFilePath(candidatePath)
            : candidateInfo.absoluteFilePath();
    return !candidateAbsolute.isEmpty()
            && comparablePath(candidateAbsolute) == comparablePath(artifactInfo.absoluteFilePath());
}

QString metadataPath(const ProjectRuntimeConfig& config, const QString& key)
{
    return config.downloadArtifact.metadata.value(key).toString().trimmed();
}

QString firstMetadataPath(const ProjectRuntimeConfig& config, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString value = metadataPath(config, key);
        if (!value.isEmpty())
            return value;
    }
    return QString();
}

struct BundleInfo
{
    QString projectRoot;
    QString generationDir;
    QString codePath;
    QString profilePath;
    QString pointsPath;
    QString manifestPath;
    QString generationId;
    QJsonObject manifest;
};

bool manifestPathFor(const QJsonObject& manifest,
                     const QString& key,
                     const QString& generationDir,
                     QString* path,
                     QStringList* errors)
{
    const QString relative = manifest.value(key).toString().trimmed();
    if (!safeManifestRelativePath(relative)) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest.%1 不是安全的相对路径。").arg(key));
        return false;
    }
    QString error;
    if (!resolveContainedPath(generationDir, relative, path, &error, true)) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest.%1 无效：%2").arg(key, error));
        return false;
    }
    return true;
}

void validateSourcePaths(const QJsonObject& manifest, QStringList* errors)
{
    const QStringList requiredScalarKeys = {
        QStringLiteral("mainScriptPath"), QStringLiteral("dslScriptPath")
    };
    for (const QString& key : requiredScalarKeys) {
        if (!manifest.contains(key) || !safeManifestRelativePath(manifest.value(key).toString())) {
            if (errors)
                errors->append(QStringLiteral("runtime_manifest.%1 必须是 project-relative 路径。").arg(key));
        }
    }
    const QString sourceFile = manifest.value(QStringLiteral("sourceFile")).toString();
    if (manifest.contains(QStringLiteral("sourceFile"))
            && !safeManifestRelativePath(sourceFile)) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest.sourceFile 必须是 project-relative 路径。"));
    }
    const QJsonValue scripts = manifest.value(QStringLiteral("scriptFiles"));
    if (!scripts.isArray()) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest.scriptFiles 缺失或不是数组。"));
    } else {
        for (const QJsonValue& value : scripts.toArray()) {
            if (!value.isString() || !safeManifestRelativePath(value.toString())) {
                if (errors)
                    errors->append(QStringLiteral("runtime_manifest.scriptFiles 含有不安全路径。"));
                break;
            }
        }
    }
    const QJsonValue sourcePaths = manifest.value(QStringLiteral("sourcePaths"));
    if (!sourcePaths.isObject()) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest.sourcePaths 缺失或不是对象。"));
    } else {
        const QJsonObject object = sourcePaths.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (!it.value().isString() || !safeManifestRelativePath(it.value().toString())) {
                if (errors)
                    errors->append(QStringLiteral("runtime_manifest.sourcePaths 含有不安全路径。"));
                break;
            }
        }
    }
    const QJsonValue sourceFileCount = manifest.value(QStringLiteral("sourceFileCount"));
    const QJsonValue scriptFileCount = manifest.value(QStringLiteral("scriptFileCount"));
    if (!sourceFileCount.isDouble() || sourceFileCount.toInt(-1) < 0
            || !scriptFileCount.isDouble() || scriptFileCount.toInt(-1) < 0) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest source path count 无效。"));
    } else if (!scripts.isArray()
               || sourceFileCount.toInt() != scripts.toArray().size()
               || scriptFileCount.toInt() != scripts.toArray().size()) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest source path count 与 scriptFiles 不一致。"));
    }
}

bool loadPublishedBundle(const QString& projectPath,
                         const ProjectRuntimeConfig& config,
                         const QString& artifactPath,
                         BundleInfo* bundle,
                         QStringList* errors,
                         QVariantMap* details)
{
    const QString projectRoot = QFileInfo(projectPath).canonicalFilePath();
    if (projectRoot.isEmpty()) {
        if (errors)
            errors->append(QStringLiteral("项目根目录无法解析。"));
        return false;
    }

    QString codePath;
    QString error;
    if (!resolveContainedPath(projectRoot, artifactPath, &codePath, &error)) {
        if (errors)
            errors->append(QStringLiteral("下载产物路径无效：%1").arg(error));
        return false;
    }
    const QFileInfo codeInfo(codePath);
    if (!codeInfo.exists() || !codeInfo.isFile() || !codeInfo.isReadable()) {
        if (errors)
            errors->append(QStringLiteral("下载产物不存在或不可读：%1").arg(codePath));
        return false;
    }
    if (codeInfo.suffix().compare(QStringLiteral("code"), Qt::CaseInsensitive) != 0) {
        if (errors)
            errors->append(QStringLiteral("下载产物必须是 .code 文件。"));
        return false;
    }

    const QString generationDir = codeInfo.absolutePath();
    const QDir generation(generationDir);
    if (generation.dirName().isEmpty() || generation.dir().dirName() != QStringLiteral("generations")) {
        if (errors)
            errors->append(QStringLiteral("下载产物不在 generation 目录中。"));
        return false;
    }

    QString manifestPath;
    const QString configuredManifest = firstMetadataPath(
            config, {QStringLiteral("runtimeManifestPath"), QStringLiteral("manifestPath")});
    if (!configuredManifest.isEmpty()) {
        if (!resolveContainedPath(projectRoot, configuredManifest, &manifestPath, &error)) {
            if (errors)
                errors->append(QStringLiteral("runtime_manifest 路径无效：%1").arg(error));
            return false;
        }
    } else {
        manifestPath = generation.filePath(QStringLiteral("runtime_manifest.json"));
    }
    if (comparablePath(QFileInfo(manifestPath).absolutePath())
            != comparablePath(generation.absolutePath())) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest 与 code 不属于同一 generation。"));
        return false;
    }

    QJsonDocument manifestDocument;
    if (!readJsonDocument(manifestPath, &manifestDocument, &error) || !manifestDocument.isObject()) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest.json 无效：%1").arg(error));
        return false;
    }
    const QJsonObject manifest = manifestDocument.object();
    const QString generationId = manifest.value(QStringLiteral("generationId")).toString().trimmed();
    if (!manifest.value(QStringLiteral("complete")).toBool(false) || generationId.isEmpty()
            || generationId != generation.dirName()) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest 未标记为当前 generation 的完整发布。"));
        return false;
    }

    const QString configuredGeneration = metadataPath(config, QStringLiteral("generationId"));
    if (!configuredGeneration.isEmpty() && configuredGeneration != generationId) {
        if (errors)
            errors->append(QStringLiteral("generationId 与项目配置不一致。"));
        return false;
    }

    QString manifestCode;
    QString manifestProfile;
    QString manifestPoints;
    bool mandatoryPathsOk = true;
    mandatoryPathsOk &= manifestPathFor(manifest, QStringLiteral("codePath"), generationDir, &manifestCode, errors);
    mandatoryPathsOk &= manifestPathFor(manifest, QStringLiteral("downloadProfilePath"), generationDir, &manifestProfile, errors);
    mandatoryPathsOk &= manifestPathFor(manifest, QStringLiteral("runtimePointsPath"), generationDir, &manifestPoints, errors);
    if (!mandatoryPathsOk)
        return false;
    if (comparablePath(manifestCode) != comparablePath(codeInfo.absoluteFilePath())) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest.codePath 与当前下载产物不一致。"));
        return false;
    }
    QString manifestFromManifest;
    if (!manifestPathFor(manifest,
                         QStringLiteral("runtimeManifestPath"),
                         generationDir,
                         &manifestFromManifest,
                         errors)
            || comparablePath(manifestFromManifest) != comparablePath(manifestPath)) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest.runtimeManifestPath 与当前 manifest 不一致。"));
        return false;
    }

    const QJsonArray artifactPaths = manifest.value(QStringLiteral("artifactPaths")).toArray();
    if (artifactPaths.isEmpty()) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest.artifactPaths 为空。"));
        return false;
    }
    QSet<QString> listed;
    const QJsonObject checksums = manifest.value(QStringLiteral("artifactChecksums")).toObject();
    for (const QJsonValue& value : artifactPaths) {
        const QString relative = value.toString().trimmed();
        if (!safeManifestRelativePath(relative)) {
            if (errors)
                errors->append(QStringLiteral("runtime_manifest.artifactPaths 含有不安全路径。"));
            return false;
        }
        QString path;
        if (!resolveContainedPath(generationDir, relative, &path, &error, true)
                || !QFileInfo(path).exists() || !QFileInfo(path).isFile()) {
            if (errors)
                errors->append(QStringLiteral("runtime_manifest artifact 不可用：%1").arg(relative));
            return false;
        }
        listed.insert(relative);
        const QString expected = checksums.value(relative).toString().trimmed();
        if (expected.isEmpty() || sha256ForFile(path).compare(expected, Qt::CaseInsensitive) != 0) {
            if (errors)
                errors->append(QStringLiteral("runtime_manifest artifact checksum 不一致：%1").arg(relative));
            return false;
        }
    }

    const QString codeRelative = manifest.value(QStringLiteral("codePath")).toString();
    const QString profileRelative = manifest.value(QStringLiteral("downloadProfilePath")).toString();
    const QString pointsRelative = manifest.value(QStringLiteral("runtimePointsPath")).toString();
    if (!listed.contains(codeRelative) || !listed.contains(profileRelative) || !listed.contains(pointsRelative)) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest 未列出全部 mandatory artifact。"));
        return false;
    }

    const QString codeChecksum = sha256ForFile(codePath);
    const QString profileChecksum = sha256ForFile(manifestProfile);
    const QString pointsChecksum = sha256ForFile(manifestPoints);
    const QString manifestChecksum = sha256ForFile(manifestPath);
    if (codeChecksum.isEmpty()
            || codeChecksum.compare(manifest.value(QStringLiteral("codeChecksum")).toString(), Qt::CaseInsensitive) != 0
            || profileChecksum.compare(manifest.value(QStringLiteral("downloadProfileChecksum")).toString(), Qt::CaseInsensitive) != 0
            || pointsChecksum.compare(manifest.value(QStringLiteral("runtimePointsChecksum")).toString(), Qt::CaseInsensitive) != 0) {
        if (errors)
            errors->append(QStringLiteral("mandatory artifact checksum 不一致。"));
        return false;
    }

    const QString configuredProfileChecksum = metadataPath(
            config, QStringLiteral("downloadProfileChecksum"));
    const QString configuredPointsChecksum = metadataPath(
            config, QStringLiteral("runtimePointsChecksum"));
    const QString configuredManifestChecksum = metadataPath(
            config, QStringLiteral("runtimeManifestChecksum"));
    if ((!configuredProfileChecksum.isEmpty()
         && profileChecksum.compare(configuredProfileChecksum, Qt::CaseInsensitive) != 0)
            || (!configuredPointsChecksum.isEmpty()
                && pointsChecksum.compare(configuredPointsChecksum, Qt::CaseInsensitive) != 0)
            || (!configuredManifestChecksum.isEmpty()
                && manifestChecksum.compare(configuredManifestChecksum, Qt::CaseInsensitive) != 0)) {
        if (errors)
            errors->append(QStringLiteral("项目配置 checksum 与已发布 generation 不一致。"));
        return false;
    }

    const QString configProfile = firstMetadataPath(
            config, {QStringLiteral("downloadProfilePath"), QStringLiteral("profileJsonPath"), QStringLiteral("profilePath")});
    if (!configProfile.isEmpty()) {
        QString resolvedConfigProfile;
        if (!resolveContainedPath(projectRoot, configProfile, &resolvedConfigProfile, &error)
                || comparablePath(resolvedConfigProfile) != comparablePath(manifestProfile)) {
            if (errors)
                errors->append(QStringLiteral("项目配置 Profile 与 manifest 不一致。"));
            return false;
        }
    }

    QJsonDocument profileDocument;
    bool profileValid = readJsonDocument(manifestProfile, &profileDocument, &error)
            && profileDocument.isObject()
            && profileDocument.object().value(QStringLiteral("steps")).isArray()
            && !profileDocument.object().value(QStringLiteral("steps")).toArray().isEmpty();
    if (profileValid) {
        const QJsonArray profileSteps = profileDocument.object()
                .value(QStringLiteral("steps")).toArray();
        bool hasSendChunk = false;
        for (const QJsonValue& step : profileSteps) {
            const QString type = step.isObject()
                    ? step.toObject().value(QStringLiteral("type")).toString().trimmed().toLower()
                    : QString();
            if (type == QStringLiteral("sendchunk"))
                hasSendChunk = true;
            if (!step.isObject()
                    || !step.toObject().value(QStringLiteral("params")).isObject()
                    || (type != QStringLiteral("enter")
                    && type != QStringLiteral("sendchunk")
                    && type != QStringLiteral("poll")
                    && type != QStringLiteral("finalize")
                    && type != QStringLiteral("queryresult"))) {
                profileValid = false;
                break;
            }
        }
        profileValid = profileValid && hasSendChunk;
    }
    if (!profileValid) {
        if (errors)
            errors->append(QStringLiteral("generation download_profile.json 无效。"));
        return false;
    }
    QJsonDocument pointsDocument;
    if (!readJsonDocument(manifestPoints, &pointsDocument, &error) || !pointsDocument.isArray()) {
        if (errors)
            errors->append(QStringLiteral("generation runtime_points.json 无效：%1").arg(error));
        return false;
    }
    if (manifest.contains(QStringLiteral("pointCount"))
            && manifest.value(QStringLiteral("pointCount")).toInt(-1) != pointsDocument.array().size()) {
        if (errors)
            errors->append(QStringLiteral("runtime_manifest pointCount 与 runtime_points.json 不一致。"));
        return false;
    }

    const int sourceErrorCount = errors ? errors->size() : 0;
    validateSourcePaths(manifest, errors);
    if (errors && errors->size() != sourceErrorCount)
        return false;
    if (details) {
        details->insert(QStringLiteral("generationId"), generationId);
        details->insert(QStringLiteral("manifestPath"), manifestPath);
        details->insert(QStringLiteral("runtimePointsPath"), manifestPoints);
        details->insert(QStringLiteral("downloadProfilePath"), manifestProfile);
        details->insert(QStringLiteral("manifestArtifactCount"), artifactPaths.size());
        details->insert(QStringLiteral("manifestContainsArtifact"), true);
        details->insert(QStringLiteral("artifactBytes"), codeInfo.size());
        details->insert(QStringLiteral("checksumActual"), codeChecksum);
        details->insert(QStringLiteral("runtimePointCount"), pointsDocument.array().size());
        details->insert(QStringLiteral("manifestPointCount"), manifest.value(QStringLiteral("pointCount")).toInt(-1));
    }

    if (bundle) {
        bundle->projectRoot = projectRoot;
        bundle->generationDir = generationDir;
        bundle->codePath = codePath;
        bundle->profilePath = manifestProfile;
        bundle->pointsPath = manifestPoints;
        bundle->manifestPath = manifestPath;
        bundle->generationId = generationId;
        bundle->manifest = manifest;
    }
    return true;
}

RunController::DownloadArtifactPrecheckReport validateLegacyDownloadArtifact(
        const ProjectRuntimeConfig& config,
        const QString& projectPath,
        const QString& artifactPath)
{
    RunController::DownloadArtifactPrecheckReport report;
    QVariantMap details;
    const QString trimmedPath = artifactPath.trimmed();
    details.insert(QStringLiteral("artifactPath"), trimmedPath);
    if (trimmedPath.isEmpty()) {
        report.errors << QStringLiteral("产物路径为空。");
        report.valid = false;
        report.details = details;
        return report;
    }

    const QFileInfo artifactInfo(trimmedPath);
    details.insert(QStringLiteral("artifactAbsolutePath"), artifactInfo.absoluteFilePath());
    details.insert(QStringLiteral("artifactExists"), artifactInfo.exists());
    details.insert(QStringLiteral("artifactSuffix"), artifactInfo.suffix());
    if (!projectPath.trimmed().isEmpty()) {
        QString resolvedArtifact;
        QString containmentError;
        if (!resolveContainedPath(QFileInfo(projectPath).canonicalFilePath(),
                                  trimmedPath,
                                  &resolvedArtifact,
                                  &containmentError)) {
            report.errors << QStringLiteral("下载产物路径无效：%1").arg(containmentError);
        }
    }
    if (!artifactInfo.exists() || !artifactInfo.isFile()) {
        report.errors << QStringLiteral("产物文件不存在：%1").arg(trimmedPath);
    } else {
        details.insert(QStringLiteral("artifactBytes"), artifactInfo.size());
        if (!artifactInfo.isReadable())
            report.errors << QStringLiteral("产物文件不可读：%1").arg(artifactInfo.absoluteFilePath());
        if (artifactInfo.suffix().compare(QStringLiteral("code"), Qt::CaseInsensitive) != 0) {
            report.warnings << QStringLiteral("产物后缀不是 .code：%1").arg(artifactInfo.fileName());
        }
    }

    const QString configuredPath = config.downloadArtifact.filePath.trimmed().isEmpty()
            ? QString()
            : (QFileInfo(config.downloadArtifact.filePath).isRelative()
               ? QDir(projectPath).absoluteFilePath(config.downloadArtifact.filePath)
               : QFileInfo(config.downloadArtifact.filePath).absoluteFilePath());
    details.insert(QStringLiteral("configuredArtifactPath"), configuredPath);
    if (!configuredPath.isEmpty() && artifactInfo.exists()
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

    const QString artifactDirectory = artifactInfo.absoluteDir().absolutePath();
    const QString manifestPath = QDir(artifactDirectory).filePath(QStringLiteral("runtime_manifest.json"));
    const QString pointsPath = QDir(artifactDirectory).filePath(QStringLiteral("runtime_points.json"));
    details.insert(QStringLiteral("manifestPath"), manifestPath);
    details.insert(QStringLiteral("runtimePointsPath"), pointsPath);
    details.insert(QStringLiteral("expectedRuntimePointCount"),
                   RuntimePointConverter::fromProjectConfig(config).size());
    details.insert(QStringLiteral("expectedParameterCount"), config.parameters.size());

    const QFileInfo manifestInfo(manifestPath);
    details.insert(QStringLiteral("manifestExists"), manifestInfo.exists());
    if (!manifestInfo.exists()) {
        report.warnings << QStringLiteral("未找到 runtime_manifest.json，无法校验产物清单。");
    } else {
        QJsonDocument manifestDocument;
        QString error;
        if (!readJsonDocument(manifestPath, &manifestDocument, &error) || !manifestDocument.isObject()) {
            report.errors << QStringLiteral("runtime_manifest.json 无效：%1").arg(error);
        } else {
            const QJsonObject manifest = manifestDocument.object();
            const QJsonArray artifactPaths = manifest.value(QStringLiteral("artifactPaths")).toArray();
            details.insert(QStringLiteral("manifestArtifactCount"), artifactPaths.size());
            bool containsArtifact = false;
            for (const QJsonValue& value : artifactPaths) {
                if (value.isString()
                        && legacyManifestPathMatches(value.toString(), artifactInfo, artifactDirectory)) {
                    containsArtifact = true;
                    break;
                }
            }
            details.insert(QStringLiteral("manifestContainsArtifact"), containsArtifact);
            if (!artifactPaths.isEmpty() && !containsArtifact)
                report.errors << QStringLiteral("runtime_manifest.json 未包含当前下载产物。");
            if (manifest.contains(QStringLiteral("pointCount"))) {
                const int manifestPointCount = manifest.value(QStringLiteral("pointCount")).toInt(-1);
                details.insert(QStringLiteral("manifestPointCount"), manifestPointCount);
            }
        }
    }

    const QFileInfo pointsInfo(pointsPath);
    details.insert(QStringLiteral("runtimePointsExists"), pointsInfo.exists());
    if (!pointsInfo.exists()) {
        report.warnings << QStringLiteral("未找到 runtime_points.json，无法校验点位映射文件。");
    } else {
        QJsonDocument pointsDocument;
        QString error;
        if (!readJsonDocument(pointsPath, &pointsDocument, &error) || !pointsDocument.isArray())
            report.errors << QStringLiteral("runtime_points.json 无效：%1").arg(error);
        else
            details.insert(QStringLiteral("runtimePointCount"), pointsDocument.array().size());
    }

    report.valid = report.errors.isEmpty();
    details.insert(QStringLiteral("valid"), report.valid);
    details.insert(QStringLiteral("errors"), stringListToVariantList(report.errors));
    details.insert(QStringLiteral("warnings"), stringListToVariantList(report.warnings));
    report.details = details;
    return report;
}

QString projectRelativePath(const QString& projectPath, const QString& absolutePath)
{
    return QDir(projectPath).relativeFilePath(QFileInfo(absolutePath).absoluteFilePath());
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
    const QString root = QFileInfo(projectPath).canonicalFilePath();
    if (root.isEmpty() || config.downloadArtifact.filePath.trimmed().isEmpty())
        return QString();
    QString resolved;
    QString error;
    return resolveContainedPath(root, config.downloadArtifact.filePath, &resolved, &error)
            ? resolved
            : QString();
}

QString RunController::findDownloadArtifactPath(const ProjectRuntimeConfig& config,
                                                const QString& projectPath,
                                                const CompileResult& compileResult)
{
    Q_UNUSED(compileResult)
    const QString configuredPath = resolveDownloadArtifactPath(config, projectPath);
    if (configuredPath.isEmpty())
        return QString();
    const bool publishedBinding = !metadataPath(config, QStringLiteral("generationId")).isEmpty()
            || !metadataPath(config, QStringLiteral("runtimeManifestPath")).isEmpty();
    if (!publishedBinding)
        return QFileInfo::exists(configuredPath) ? configuredPath : QString();
    QStringList errors;
    return loadPublishedBundle(projectPath, config, configuredPath, nullptr, &errors, nullptr)
            ? configuredPath
            : QString();
}

bool RunController::writeDownloadArtifact(ProjectRuntimeConfig& config,
                                          const QString& projectPath,
                                          const CompileResult& compileResult)
{
    if (!compileResult.success)
        return false;

    QString candidate;
    QString generationId;
    for (const CompileArtifact& artifact : compileResult.artifacts) {
        if (artifact.type == QStringLiteral("download")
                && artifact.format == QStringLiteral("dsl_custom")
                && QFileInfo::exists(artifact.path)) {
            candidate = artifact.path;
            generationId = artifact.metadata.value(QStringLiteral("generationId")).toString();
            break;
        }
    }
    if (candidate.isEmpty())
        return false;

    ProjectRuntimeConfig emptyConfig;
    BundleInfo bundle;
    QStringList errors;
    if (!loadPublishedBundle(projectPath, emptyConfig, candidate, &bundle, &errors, nullptr)
            || (!generationId.isEmpty() && generationId != bundle.generationId)) {
        return false;
    }

    const QString relativeCode = projectRelativePath(bundle.projectRoot, bundle.codePath);
    const QString relativeManifest = projectRelativePath(bundle.projectRoot, bundle.manifestPath);
    const QString relativeProfile = projectRelativePath(bundle.projectRoot, bundle.profilePath);
    const QString relativePoints = projectRelativePath(bundle.projectRoot, bundle.pointsPath);
    if (relativeCode.isEmpty() || relativeManifest.isEmpty() || relativeProfile.isEmpty() || relativePoints.isEmpty()
            || !safeManifestRelativePath(relativeCode)
            || !safeManifestRelativePath(relativeManifest)
            || !safeManifestRelativePath(relativeProfile)
            || !safeManifestRelativePath(relativePoints)) {
        return false;
    }

    QString sourceProfile = compileResult.metadata
            .value(QStringLiteral("downloadProfileSourcePath")).toString().trimmed();
    if (sourceProfile.isEmpty()) {
        sourceProfile = firstMetadataPath(config,
                                          {QStringLiteral("downloadProfileSourcePath"),
                                           QStringLiteral("profileSourcePath"),
                                           QStringLiteral("sourceProfilePath")});
    }
    if (sourceProfile.isEmpty()) {
        sourceProfile = firstMetadataPath(config,
                                          {QStringLiteral("downloadProfilePath"),
                                           QStringLiteral("profileJsonPath"),
                                           QStringLiteral("profilePath")});
        const QString normalizedSource = QDir::fromNativeSeparators(sourceProfile);
        if ((normalizedSource.startsWith(QStringLiteral("generations/"))
             || normalizedSource.contains(QStringLiteral("/generations/")))
                && normalizedSource.endsWith(QStringLiteral("/download_profile.json"))) {
            sourceProfile.clear();
        }
    }
    QString relativeSourceProfile;
    if (!sourceProfile.isEmpty()) {
        const QString absoluteSourceProfile = QFileInfo(sourceProfile).isRelative()
                ? QDir(bundle.projectRoot).absoluteFilePath(sourceProfile)
                : QFileInfo(sourceProfile).absoluteFilePath();
        relativeSourceProfile = projectRelativePath(bundle.projectRoot, absoluteSourceProfile);
        if (relativeSourceProfile.isEmpty() || !safeManifestRelativePath(relativeSourceProfile))
            return false;
    }

    config.downloadArtifact.artifactType = QStringLiteral("dsl_custom");
    config.downloadArtifact.formatVersion.clear();
    config.downloadArtifact.filePath = relativeCode;
    config.downloadArtifact.checksum = sha256ForFile(bundle.codePath);
    QVariantMap metadata = config.downloadArtifact.metadata;
    metadata.remove(QStringLiteral("sourceFile"));
    metadata.remove(QStringLiteral("mainScriptFile"));
    metadata.remove(QStringLiteral("scriptFiles"));
    metadata.remove(QStringLiteral("outputDir"));
    metadata.remove(QStringLiteral("generationDirectory"));
    metadata.remove(QStringLiteral("artifactPath"));
    metadata.remove(QStringLiteral("sourceFilePath"));
    metadata.remove(QStringLiteral("mainScriptPath"));
    metadata.remove(QStringLiteral("dslScriptPath"));
    metadata.remove(QStringLiteral("profileJsonPath"));
    metadata.remove(QStringLiteral("profilePath"));
    metadata.remove(QStringLiteral("profileSourcePath"));
    metadata.remove(QStringLiteral("sourceProfilePath"));
    metadata.remove(QStringLiteral("manifestPath"));
    metadata.insert(QStringLiteral("generationId"), bundle.generationId);
    metadata.insert(QStringLiteral("runtimeManifestPath"), relativeManifest);
    metadata.insert(QStringLiteral("runtimeManifestChecksum"), sha256ForFile(bundle.manifestPath));
    metadata.insert(QStringLiteral("downloadProfilePath"), relativeProfile);
    metadata.insert(QStringLiteral("downloadProfileChecksum"), sha256ForFile(bundle.profilePath));
    if (!relativeSourceProfile.isEmpty())
        metadata.insert(QStringLiteral("downloadProfileSourcePath"), relativeSourceProfile);
    else
        metadata.remove(QStringLiteral("downloadProfileSourcePath"));
    metadata.insert(QStringLiteral("runtimePointsPath"), relativePoints);
    metadata.insert(QStringLiteral("runtimePointsChecksum"), sha256ForFile(bundle.pointsPath));
    metadata.insert(QStringLiteral("artifactScope"), QStringLiteral("project"));
    config.downloadArtifact.metadata = metadata;
    return true;
}

RunController::DownloadArtifactPrecheckReport RunController::validateDownloadArtifact(
        const ProjectRuntimeConfig& config,
        const QString& projectPath,
        const QString& artifactPath)
{
    const bool publishedBinding = !metadataPath(config, QStringLiteral("generationId")).isEmpty()
            || !metadataPath(config, QStringLiteral("runtimeManifestPath")).isEmpty();
    if (!publishedBinding)
        return validateLegacyDownloadArtifact(config, projectPath, artifactPath);

    DownloadArtifactPrecheckReport report;
    QVariantMap details;
    details.insert(QStringLiteral("artifactPath"), artifactPath.trimmed());
    details.insert(QStringLiteral("configuredArtifactPath"), resolveDownloadArtifactPath(config, projectPath));
    details.insert(QStringLiteral("expectedRuntimePointCount"), RuntimePointConverter::fromProjectConfig(config).size());
    details.insert(QStringLiteral("expectedParameterCount"), config.parameters.size());

    BundleInfo bundle;
    if (artifactPath.trimmed().isEmpty()) {
        report.errors << QStringLiteral("产物路径为空。");
    } else if (!loadPublishedBundle(projectPath, config, artifactPath, &bundle, &report.errors, &details)) {
        report.valid = false;
    } else {
        const QString expectedChecksum = config.downloadArtifact.checksum.trimmed();
        const QString actualChecksum = sha256ForFile(bundle.codePath);
        details.insert(QStringLiteral("checksumExpected"), expectedChecksum);
        if (expectedChecksum.isEmpty() || actualChecksum.compare(expectedChecksum, Qt::CaseInsensitive) != 0)
            report.errors << QStringLiteral("项目配置中的 code checksum 与已发布 generation 不一致。");

        const QString configuredPath = resolveDownloadArtifactPath(config, projectPath);
        if (!configuredPath.isEmpty() && comparablePath(configuredPath) != comparablePath(bundle.codePath))
            report.errors << QStringLiteral("当前下载产物和项目记录的产物路径不一致。");

        const QString configuredManifest = firstMetadataPath(
                config, {QStringLiteral("runtimeManifestPath"), QStringLiteral("manifestPath")});
        if (configuredManifest.isEmpty())
            report.errors << QStringLiteral("项目配置缺少 runtime_manifest 路径。");
    }

    report.valid = report.errors.isEmpty();
    details.insert(QStringLiteral("artifactExists"), !bundle.codePath.isEmpty() && QFileInfo::exists(bundle.codePath));
    details.insert(QStringLiteral("manifestExists"), !bundle.manifestPath.isEmpty() && QFileInfo::exists(bundle.manifestPath));
    details.insert(QStringLiteral("runtimePointsExists"), !bundle.pointsPath.isEmpty() && QFileInfo::exists(bundle.pointsPath));
    details.insert(QStringLiteral("valid"), report.valid);
    details.insert(QStringLiteral("errors"), stringListToVariantList(report.errors));
    details.insert(QStringLiteral("warnings"), stringListToVariantList(report.warnings));
    report.details = details;
    return report;
}
