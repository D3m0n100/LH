/**
 * @file BuildController.cpp
 * @brief Build flow controller implementation
 */

#include "BuildController.h"
#include "Common.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QUuid>
#include <QXmlStreamWriter>

namespace {

QString buildTypeName(BuildType type)
{
    switch (type) {
    case BuildType::Configuration: return QStringLiteral("组态");
    case BuildType::Parameters: return QStringLiteral("参数");
    case BuildType::Communication: return QStringLiteral("通信");
    }
    return QStringLiteral("编译");
}

QString artifactBaseName(const QString& projectPath, const ProjectRuntimeConfig& config)
{
    QString name = config.projectName.trimmed();
    if (name.isEmpty())
        name = QFileInfo(projectPath).fileName();
    if (name.isEmpty())
        name = QStringLiteral("lh_project");

    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|\\s]+")), QStringLiteral("_"));
    name.replace(QRegularExpression(QStringLiteral("^_+|_+$")), QString());
    return name.isEmpty() ? QStringLiteral("lh_project") : name;
}

QString artifactPath(const QString& outputDir,
                     const QString& projectPath,
                     const ProjectRuntimeConfig& config,
                     const QString& suffix)
{
    return QDir(outputDir).absoluteFilePath(artifactBaseName(projectPath, config) + suffix);
}

bool writeTextFile(const QString& filePath, const QString& text, QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法写入文件: %1").arg(filePath);
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << text;
    if (out.status() != QTextStream::Ok || !file.flush() || file.error() != QFileDevice::NoError) {
        if (errorMessage)
            *errorMessage = QStringLiteral("写入产物失败: %1").arg(filePath);
        return false;
    }
    return true;
}

QString sha256ForFile(const QString& filePath);

struct StagedArtifact
{
    QString finalPath;
    QString stagedPath;
    QString checksum;
};

struct PublishedArtifact
{
    StagedArtifact staged;
    QString backupPath;
    bool backupCreated = false;
    bool published = false;
};

bool validateArtifactTarget(const QString& finalPath, QString* errorMessage)
{
    const QFileInfo targetInfo(finalPath);
    if (targetInfo.exists() && !targetInfo.isFile()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("产物目标不是普通文件: %1").arg(finalPath);
        }
        return false;
    }

    const QFileInfo directoryInfo(targetInfo.absolutePath());
    if (!directoryInfo.exists() || !directoryInfo.isDir()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("产物输出目录不存在: %1").arg(directoryInfo.absoluteFilePath());
        }
        return false;
    }
    if (!directoryInfo.isWritable()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("产物输出目录不可写: %1").arg(directoryInfo.absoluteFilePath());
        }
        return false;
    }
    if (targetInfo.exists() && !targetInfo.isWritable()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("旧产物不可替换: %1").arg(finalPath);
        }
        return false;
    }
    return true;
}

void appendCleanupFailure(const QString& path, QStringList* failures)
{
    if (failures) {
        failures->append(QStringLiteral("无法清理残留: %1").arg(path));
    }
}

void cleanupStagedArtifacts(const QList<StagedArtifact>& staged,
                            QStringList* failures)
{
    for (const auto& artifact : staged) {
        if (QFileInfo::exists(artifact.stagedPath)
                && !QFile::remove(artifact.stagedPath)) {
            appendCleanupFailure(artifact.stagedPath, failures);
        }
    }
}

bool stageTextArtifacts(const QList<QPair<QString, QString>>& files,
                        QList<StagedArtifact>* staged,
                        QString* errorMessage)
{
    if (!staged) {
        if (errorMessage)
            *errorMessage = QStringLiteral("内部错误：暂存产物列表为空。");
        return false;
    }

    QSet<QString> finalPaths;
    for (const auto& file : files) {
        if (finalPaths.contains(file.first)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("产物目标路径重复: %1").arg(file.first);
            }
            return false;
        }
        finalPaths.insert(file.first);

        QString targetError;
        if (!validateArtifactTarget(file.first, &targetError)) {
            if (errorMessage)
                *errorMessage = targetError;
            return false;
        }
    }

    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    for (int index = 0; index < files.size(); ++index) {
        const QString finalPath = files.at(index).first;
        StagedArtifact artifact;
        artifact.finalPath = finalPath;
        artifact.stagedPath = finalPath
                + QStringLiteral(".lh-stage-")
                + token
                + QStringLiteral("-")
                + QString::number(index);

        QString writeError;
        if (!writeTextFile(artifact.stagedPath, files.at(index).second, &writeError)) {
            QStringList cleanupFailures;
            cleanupStagedArtifacts(*staged, &cleanupFailures);
            if (QFileInfo::exists(artifact.stagedPath)) {
                if (!QFile::remove(artifact.stagedPath))
                    appendCleanupFailure(artifact.stagedPath, &cleanupFailures);
            }
            if (errorMessage) {
                *errorMessage = writeError;
                if (!cleanupFailures.isEmpty()) {
                    *errorMessage += QStringLiteral("；清理失败: ")
                            + cleanupFailures.join(QStringLiteral("；"));
                }
            }
            return false;
        }

        const QFileInfo stagedInfo(artifact.stagedPath);
        artifact.checksum = sha256ForFile(artifact.stagedPath);
        if (!stagedInfo.exists() || !stagedInfo.isFile() || artifact.checksum.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("暂存产物校验失败: %1").arg(artifact.stagedPath);
            }
            staged->append(artifact);
            QStringList cleanupFailures;
            cleanupStagedArtifacts(*staged, &cleanupFailures);
            if (errorMessage && !cleanupFailures.isEmpty()) {
                *errorMessage += QStringLiteral("；清理失败: ")
                        + cleanupFailures.join(QStringLiteral("；"));
            }
            return false;
        }
        staged->append(artifact);
    }
    return true;
}

bool shouldInjectPublishFailure(const QObject* owner)
{
#ifdef LH_BUILD_CONTROLLER_TESTING
    return owner && owner->property("lh_test_inject_publish_failure_after_first").toBool();
#else
    Q_UNUSED(owner);
    return false;
#endif
}

bool rollbackPublishedArtifacts(QList<PublishedArtifact>* published,
                                QStringList* failures)
{
    if (!published)
        return true;

    bool success = true;
    for (int index = published->size() - 1; index >= 0; --index) {
        PublishedArtifact& artifact = (*published)[index];
        if (artifact.published) {
            if (QFileInfo::exists(artifact.staged.finalPath)
                    && !QFile::remove(artifact.staged.finalPath)) {
                appendCleanupFailure(artifact.staged.finalPath, failures);
                success = false;
            } else {
                artifact.published = false;
            }
        }

        if (artifact.backupCreated) {
            if (!QFile::rename(artifact.backupPath, artifact.staged.finalPath)) {
                if (failures) {
                    failures->append(QStringLiteral("无法恢复旧产物: %1 -> %2")
                                             .arg(artifact.backupPath, artifact.staged.finalPath));
                }
                success = false;
            } else {
                artifact.backupCreated = false;
            }
        }
    }
    return success;
}

bool publishStagedArtifacts(const QList<StagedArtifact>& staged,
                            QString* errorMessage,
                            bool injectFailureAfterFirstPublish)
{
#ifndef LH_BUILD_CONTROLLER_TESTING
    Q_UNUSED(injectFailureAfterFirstPublish);
#endif
    QList<PublishedArtifact> published;
    published.reserve(staged.size());

    for (const auto& stagedArtifact : staged) {
        PublishedArtifact artifact;
        artifact.staged = stagedArtifact;

        if (QFileInfo::exists(stagedArtifact.finalPath)) {
            artifact.backupPath = stagedArtifact.finalPath
                    + QStringLiteral(".lh-backup-")
                    + QUuid::createUuid().toString(QUuid::WithoutBraces);
            if (!QFile::rename(stagedArtifact.finalPath, artifact.backupPath)) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("发布前备份失败: %1").arg(stagedArtifact.finalPath);
                }
                published.append(artifact);
                QStringList rollbackFailures;
                const bool rollbackOk = rollbackPublishedArtifacts(&published, &rollbackFailures);
                cleanupStagedArtifacts(staged, &rollbackFailures);
                if (!rollbackOk || !rollbackFailures.isEmpty()) {
                    if (errorMessage) {
                        *errorMessage += QStringLiteral("；回滚/清理失败: ")
                                + rollbackFailures.join(QStringLiteral("；"));
                    }
                }
                return false;
            }
            artifact.backupCreated = true;
        }

        if (!QFile::rename(stagedArtifact.stagedPath, stagedArtifact.finalPath)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("发布产物失败: %1").arg(stagedArtifact.finalPath);
            }
            published.append(artifact);
            QStringList rollbackFailures;
            const bool rollbackOk = rollbackPublishedArtifacts(&published, &rollbackFailures);
            cleanupStagedArtifacts(staged, &rollbackFailures);
            if (!rollbackOk || !rollbackFailures.isEmpty()) {
                if (errorMessage) {
                    *errorMessage += QStringLiteral("；回滚/清理失败: ")
                            + rollbackFailures.join(QStringLiteral("；"));
                }
            }
            return false;
        }
        artifact.published = true;
        published.append(artifact);

#ifdef LH_BUILD_CONTROLLER_TESTING
        if (injectFailureAfterFirstPublish && published.size() == 1
                && published.size() < staged.size()) {
            // 让下一个正式 rename 进入真实失败分支；只在测试宏下编译。
            QFile::remove(staged.at(1).stagedPath);
        }
#endif
    }

    for (const auto& artifact : published) {
        const QString checksum = sha256ForFile(artifact.staged.finalPath);
        if (checksum.isEmpty() || checksum != artifact.staged.checksum) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("发布后产物校验失败: %1")
                        .arg(artifact.staged.finalPath);
            }
            QStringList rollbackFailures;
            const bool rollbackOk = rollbackPublishedArtifacts(&published, &rollbackFailures);
            cleanupStagedArtifacts(staged, &rollbackFailures);
            if (!rollbackOk || !rollbackFailures.isEmpty()) {
                if (errorMessage) {
                    *errorMessage += QStringLiteral("；回滚/清理失败: ")
                            + rollbackFailures.join(QStringLiteral("；"));
                }
            }
            return false;
        }
    }

    QStringList cleanupFailures;
    for (const auto& artifact : published) {
        if (artifact.backupCreated
                && QFileInfo::exists(artifact.backupPath)
                && !QFile::remove(artifact.backupPath)) {
            appendCleanupFailure(artifact.backupPath, &cleanupFailures);
        }
    }
    cleanupStagedArtifacts(staged, &cleanupFailures);
    if (!cleanupFailures.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("产物已发布但清理失败: ")
                    + cleanupFailures.join(QStringLiteral("；"));
        }
        return false;
    }
    return true;
}

QString sha256ForFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}

QString artifactHeader(const QString& title, const QString& columns)
{
    return QStringLiteral("# %1\n# GeneratedBy\tLH\n# GeneratedAt\t%2\n# Columns\t%3\n")
            .arg(title, QDateTime::currentDateTime().toString(Qt::ISODate), columns);
}

QString variantText(const QVariant& value)
{
    if (!value.isValid() || value.isNull())
        return QString();
    if (value.type() == QVariant::Bool)
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return value.toString();
}

QString parameterEffectiveValue(const ParameterDefinition& parameter)
{
    if (!parameter.currentValue.trimmed().isEmpty())
        return parameter.currentValue.trimmed();
    return variantText(parameter.defaultValue);
}

QVariantMap artifactMetadata(const QString& projectPath,
                             const QString& outputDir,
                             const QString& projectName)
{
    QVariantMap metadata;
    metadata.insert(QStringLiteral("projectPath"), projectPath);
    metadata.insert(QStringLiteral("outputDir"), outputDir);
    metadata.insert(QStringLiteral("projectName"), projectName);
    metadata.insert(QStringLiteral("artifactScope"), QStringLiteral("project"));
    return metadata;
}

CompileArtifact makeArtifact(const QString& type,
                             const QString& path,
                             const QString& format,
                             const QVariantMap& metadata)
{
    CompileArtifact artifact;
    artifact.type = type;
    artifact.path = path;
    artifact.format = format;
    artifact.checksum = sha256ForFile(path);
    artifact.metadata = metadata;
    return artifact;
}

QString tagLine(const OpcTagDefinition& tag)
{
    return QStringList{
        tag.tagName,
        tag.dataType,
        tag.item,
        tag.tagGroup,
        tag.tagAccess,
        tag.server,
        tag.ioGroup,
        tag.unit
    }.join(QLatin1Char('\t'));
}

QString compactJson(const QJsonObject& obj);

QString runtimePointAccessText(const RuntimePointDefinition& point)
{
    return runtimePointAccessToString(point.access);
}

bool isReadablePoint(const RuntimePointDefinition& point)
{
    return point.access == RuntimePointAccess::ReadOnly
            || point.access == RuntimePointAccess::ReadWrite;
}

bool isWritablePoint(const RuntimePointDefinition& point)
{
    return point.access == RuntimePointAccess::WriteOnly
            || point.access == RuntimePointAccess::ReadWrite;
}

QString pointKindText(const RuntimePointDefinition& point)
{
    switch (point.kind) {
    case RuntimePointKind::Variable: return QStringLiteral("Variable");
    case RuntimePointKind::Parameter: return QStringLiteral("Parameter");
    case RuntimePointKind::Status: return QStringLiteral("Status");
    case RuntimePointKind::Alarm: return QStringLiteral("Alarm");
    case RuntimePointKind::Resource: return QStringLiteral("Resource");
    }
    return QStringLiteral("Unknown");
}

QString pointLine(const RuntimePointDefinition& point, const OpcTagDefinition& tag)
{
    return QStringList{
        point.id,
        point.name,
        pointKindText(point),
        point.dataType,
        runtimePointAccessText(point),
        tag.item,
        tag.tagName,
        compactJson(QJsonObject::fromVariantMap(point.addressing))
    }.join(QLatin1Char('\t'));
}

QString engineeringLine(const RuntimePointDefinition& point, const OpcTagDefinition& tag)
{
    const QVariantMap metadata = point.metadata;
    const QVariantMap addressing = point.addressing;
    return QStringList{
        point.id,
        point.name,
        point.dataType,
        point.unit,
        variantText(point.defaultValue),
        metadata.value(QStringLiteral("minValue")).toString(),
        metadata.value(QStringLiteral("maxValue")).toString(),
        variantText(addressing.value(QStringLiteral("scale"), 1.0)),
        variantText(addressing.value(QStringLiteral("offset"), 0.0)),
        tag.tagName
    }.join(QLatin1Char('\t'));
}

QString communicationAddressLine(const RuntimePointDefinition& point, const OpcTagDefinition& tag)
{
    const QVariantMap addressing = point.addressing;
    return QStringList{
        point.id,
        point.name,
        pointKindText(point),
        tag.tagName,
        tag.item,
        tag.metadata.value(QStringLiteral("opcItemId")).toString(),
        addressing.value(QStringLiteral("opcItemId")).toString(),
        addressing.value(QStringLiteral("protocol")).toString(),
        addressing.value(QStringLiteral("area")).toString(),
        variantText(addressing.value(QStringLiteral("address"))),
        addressing.value(QStringLiteral("registerType")).toString(),
        variantText(addressing.value(QStringLiteral("regAddress"))),
        variantText(addressing.value(QStringLiteral("bitIndex"))),
        variantText(addressing.value(QStringLiteral("unitId")))
    }.join(QLatin1Char('\t'));
}

QString compactJson(const QJsonObject& obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

} // namespace

BuildController::BuildController(QObject* parent)
    : QObject(parent)
    , m_dslCompiler(new DSLCompilerInterface(this))
{
    connect(m_dslCompiler, &DSLCompilerInterface::compileFinished,
            this, &BuildController::onDslCompilerFinished);
    connect(m_dslCompiler, &DSLCompilerInterface::compileFailedToStart,
            this, &BuildController::onDslCompilerFailedToStart);

    LOG_DEBUG("BuildController created.");
}

BuildController::~BuildController()
{
    LOG_DEBUG("BuildController destroyed.");
}

void BuildController::compileConfiguration(const QString& projectPath,
                                           const ProjectRuntimeConfig& config)
{
    compileCommon(BuildType::Configuration, projectPath, config);
}

void BuildController::compileParameters(const QString& projectPath,
                                         const ProjectRuntimeConfig& config)
{
    compileGeneratedArtifacts(BuildType::Parameters, projectPath, config);
}

void BuildController::compileCommunication(const QString& projectPath,
                                           const ProjectRuntimeConfig& config)
{
    compileGeneratedArtifacts(BuildType::Communication, projectPath, config);
}

void BuildController::compileCommon(BuildType type,
                                    const QString& projectPath,
                                    const ProjectRuntimeConfig& config)
{
    if (m_busy) {
        emit logMessage(timestampedMessage("编译器正在忙碌，请稍候。"));
        return;
    }

    if (projectPath.isEmpty()) {
        emit logMessage(timestampedMessage("请先打开或创建项目。"));
        emit compileFailed(type, QStringLiteral("项目路径为空。"));
        return;
    }

    emit saveRequired();

    // 使用回调进行编译前校验（替代旧的引用参数信号）
    if (m_validationCallback) {
        QStringList errors;
        if (!m_validationCallback(type, errors)) {
            const QString errorMessage = errors.isEmpty()
                    ? QStringLiteral("构建前校验失败。")
                    : errors.join('\n');
            emit logMessage(timestampedMessage(errorMessage));
            emit compileFailed(type, errorMessage);
            return;
        }
    }

    m_currentBuildType = type;
    m_currentProjectPath = projectPath;

    const QString sourceFile = currentDslScriptPath(config);
    if (sourceFile.isEmpty() || !QFileInfo::exists(sourceFile)) {
        emit logMessage(timestampedMessage("未找到当前项目的 .lh 脚本。"));
        emit compileFailed(type, QStringLiteral(".lh 脚本不存在。"));
        return;
    }

    setBusy(true);
    emit compileStarted(type);
    emit logMessage(timestampedMessage(
            QStringLiteral("开始编译%1: %2").arg(buildTypeName(type), QFileInfo(sourceFile).fileName())));
    emit logMessage(timestampedMessage(
            QStringLiteral("输出目录: %1").arg(buildOutputDirectory(type))));
    emit compileProgress(10);

    m_dslCompiler->compileProjectAsync(projectPath,
                                       config,
                                       buildOutputDirectory(type),
                                       QFileInfo(projectPath).fileName());
}

void BuildController::cancelCompile()
{
    if (!m_busy) {
        return;
    }

    if (m_dslCompiler) {
        m_dslCompiler->cancelCurrentCompile();
    }

    setBusy(false);
    emit logMessage(timestampedMessage("编译已取消。"));
}

void BuildController::onDslCompilerFinished(int exitCode,
                                            bool normalExit,
                                            const QString& stdOut,
                                            const QString& stdErr)
{
    setBusy(false);
    m_lastCompileResult = m_dslCompiler->lastCompileResult();

    if (!stdOut.trimmed().isEmpty()) {
        emit compileProgress(80);
        emit logMessage(stdOut.trimmed());
    }
    if (!stdErr.trimmed().isEmpty()) {
        emit logMessage(stdErr.trimmed());
    }

    if (exitCode == 0 && normalExit) {
        emit compileProgress(100);
        emit logMessage(timestampedMessage(
                QStringLiteral("编译完成，输出目录: %1")
                        .arg(buildOutputDirectory(m_currentBuildType))));
        emit compileSucceeded(m_currentBuildType);
        return;
    }

    const QString errorMessage = stdErr.trimmed().isEmpty()
            ? QStringLiteral("编译失败，退出码: %1").arg(exitCode)
            : stdErr.trimmed();
    emit compileProgress(100);
    emit logMessage(timestampedMessage(QStringLiteral("错误: %1").arg(errorMessage)));
    emit compileFailed(m_currentBuildType, errorMessage);
}

void BuildController::onDslCompilerFailedToStart(const QString& errorString)
{
    setBusy(false);
    emit logMessage(timestampedMessage(QStringLiteral("编译进程启动失败: %1").arg(errorString)));
    emit compileFailed(m_currentBuildType, errorString);
}

void BuildController::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged(busy);
    }
}

QString BuildController::timestampedMessage(const QString& msg) const
{
    return QString("[%1] %2")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(msg);
}

QString BuildController::currentDslScriptPath(const ProjectRuntimeConfig& config) const
{
    if (m_currentProjectPath.isEmpty()) {
        return QString();
    }

    auto resolvePath = [this](const QString& path) -> QString {
        if (path.isEmpty()) {
            return QString();
        }
        if (QFileInfo(path).isAbsolute()) {
            return path;
        }
        return QDir(m_currentProjectPath).absoluteFilePath(path);
    };

    const QString mainPath = resolvePath(config.mainScriptPath);
    if (!mainPath.isEmpty()
            && QFileInfo::exists(mainPath)
            && QFileInfo(mainPath).suffix().compare(QStringLiteral("lh"), Qt::CaseInsensitive) == 0) {
        return mainPath;
    }

    const QString lhPath = QDir(m_currentProjectPath).absoluteFilePath(QStringLiteral("main.lh"));
    if (QFileInfo::exists(lhPath)) {
        return lhPath;
    }

    return QString();
}

void BuildController::onCompileProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const bool normalExit = (exitStatus == QProcess::NormalExit);
    onDslCompilerFinished(exitCode, normalExit, QString(), QString());
}

void BuildController::onCompileProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    onDslCompilerFailedToStart(QStringLiteral("编译进程启动失败或异常退出。"));
}

QString BuildController::buildOutputDirectory(BuildType type) const
{
    const QString baseDir = QDir(m_currentProjectPath).absoluteFilePath(QStringLiteral("build_output"));
    QDir().mkpath(baseDir);

    QString subDirName;
    switch (type) {
    case BuildType::Configuration:
        subDirName = QStringLiteral("configuration");
        break;
    case BuildType::Parameters:
        subDirName = QStringLiteral("parameters");
        break;
    case BuildType::Communication:
        subDirName = QStringLiteral("communication");
        break;
    }

    const QString outputDir = QDir(baseDir).absoluteFilePath(subDirName);
    QDir().mkpath(outputDir);
    return outputDir;
}

void BuildController::compileGeneratedArtifacts(BuildType type,
                                                const QString& projectPath,
                                                const ProjectRuntimeConfig& config)
{
    if (m_busy) {
        emit logMessage(timestampedMessage("编译器正在忙碌，请稍候。"));
        return;
    }

    if (projectPath.isEmpty()) {
        emit logMessage(timestampedMessage("请先打开或创建项目。"));
        emit compileFailed(type, QStringLiteral("项目路径为空。"));
        return;
    }

    emit saveRequired();

    if (m_validationCallback) {
        QStringList errors;
        if (!m_validationCallback(type, errors)) {
            const QString errorMessage = errors.isEmpty()
                    ? QStringLiteral("构建前校验失败。")
                    : errors.join('\n');
            emit logMessage(timestampedMessage(errorMessage));
            emit compileFailed(type, errorMessage);
            return;
        }
    }

    m_currentBuildType = type;
    m_currentProjectPath = projectPath;
    const QString outputDir = buildOutputDirectory(type);

    setBusy(true);
    emit compileStarted(type);
    emit compileProgress(10);
    emit logMessage(timestampedMessage(
            QStringLiteral("开始生成%1产物: %2")
                    .arg(buildTypeName(type), artifactBaseName(projectPath, config))));
    emit logMessage(timestampedMessage(QStringLiteral("输出目录: %1").arg(outputDir)));

    if (type == BuildType::Parameters) {
        m_lastCompileResult = generateParameterArtifacts(projectPath, config, outputDir);
    } else {
        m_lastCompileResult = generateCommunicationArtifacts(projectPath, config, outputDir);
    }

    emit compileProgress(100);
    setBusy(false);

    if (m_lastCompileResult.success) {
        emit logMessage(timestampedMessage(
                QStringLiteral("%1产物生成完成，输出目录: %2")
                        .arg(buildTypeName(type), outputDir)));
        emit compileSucceeded(type);
    } else {
        const QString errorMessage = m_lastCompileResult.errors.isEmpty()
                ? QStringLiteral("%1产物生成失败").arg(buildTypeName(type))
                : m_lastCompileResult.errors.join('\n');
        emit logMessage(timestampedMessage(QStringLiteral("错误: %1").arg(errorMessage)));
        emit compileFailed(type, errorMessage);
    }
}

CompileResult BuildController::generateParameterArtifacts(const QString& projectPath,
                                                          const ProjectRuntimeConfig& config,
                                                          const QString& outputDir) const
{
    CompileResult result;
    result.projectName = config.projectName;

    const QString dataPath = artifactPath(outputDir, projectPath, config, QStringLiteral(".data"));
    const QString reportPath = artifactPath(outputDir, projectPath, config, QStringLiteral(".rep"));

    QString dataText = artifactHeader(
        QStringLiteral("LH parameter data file"),
        QStringLiteral("command\tname\tdata_type\tvalue\tdefault_value\tunit\tonline_editable\tconfirmed"));
    for (const auto& parameter : config.parameters) {
        dataText += QStringList{
            QStringLiteral("SET"),
            parameter.name,
            parameter.dataType,
            parameterEffectiveValue(parameter),
            variantText(parameter.defaultValue),
            parameter.unit,
            parameter.onlineEditable ? QStringLiteral("true") : QStringLiteral("false"),
            parameter.confirmed ? QStringLiteral("true") : QStringLiteral("false")
        }.join(QLatin1Char('\t')) + QLatin1Char('\n');
    }

    QString reportText = artifactHeader(
        QStringLiteral("LH parameter compile report"),
        QStringLiteral("key\tvalue"));
    reportText += QStringLiteral("status\tsuccess\n");
    reportText += QStringLiteral("parameter_count\t%1\n").arg(config.parameters.size());
    reportText += QStringLiteral("output\t%1\n").arg(QDir::toNativeSeparators(dataPath));

    QList<QPair<QString, QString>> artifactFiles;
    artifactFiles.append(qMakePair(dataPath, dataText));
    artifactFiles.append(qMakePair(reportPath, reportText));

    QList<StagedArtifact> stagedArtifacts;
    QString errorMessage;
    if (!stageTextArtifacts(artifactFiles, &stagedArtifacts, &errorMessage)
            || !publishStagedArtifacts(stagedArtifacts,
                                       &errorMessage,
                                       shouldInjectPublishFailure(this))) {
        result.success = false;
        result.errors.append(errorMessage);
        result.stdErr = errorMessage;
        return result;
    }

    const QVariantMap metadata = artifactMetadata(projectPath, outputDir, config.projectName);
    result.success = true;
    result.artifacts.append(makeArtifact(QString::fromLatin1(CompileArtifactType::ParameterData),
                                         dataPath,
                                         QStringLiteral("lh_parameter_data"),
                                         metadata));
    result.artifacts.append(makeArtifact(QString::fromLatin1(CompileArtifactType::CompileReport),
                                         reportPath,
                                         QStringLiteral("lh_parameter_report"),
                                         metadata));
    result.stdOut = QStringLiteral("生成参数产物: %1").arg(QDir::toNativeSeparators(dataPath));
    return result;
}

CompileResult BuildController::generateCommunicationArtifacts(const QString& projectPath,
                                                              const ProjectRuntimeConfig& config,
                                                              const QString& outputDir) const
{
    CompileResult result;
    result.projectName = config.projectName;

    const QList<RuntimePointDefinition> points = RuntimePointConverter::fromProjectConfig(config);
    const QList<OpcTagDefinition> tags = RuntimePointConverter::runtimePointsToOpcTags(points);

    const QString xmlPath = artifactPath(outputDir, projectPath, config, QStringLiteral(".xml"));
    const QString tagPath = artifactPath(outputDir, projectPath, config, QStringLiteral(".tag"));
    const QString actPath = artifactPath(outputDir, projectPath, config, QStringLiteral(".act"));
    const QString txPath = artifactPath(outputDir, projectPath, config, QStringLiteral(".tx"));
    const QString rxPath = artifactPath(outputDir, projectPath, config, QStringLiteral(".rx"));
    const QString rtPath = artifactPath(outputDir, projectPath, config, QStringLiteral(".rt"));
    const QString engPath = artifactPath(outputDir, projectPath, config, QStringLiteral(".eng"));
    const QString commPath = artifactPath(outputDir, projectPath, config, QStringLiteral(".comm"));
    const QString msgPath = artifactPath(outputDir, projectPath, config, QStringLiteral(".msg"));
    const QString reportPath = artifactPath(outputDir, projectPath, config, QStringLiteral(".rep"));

    QString xmlText;
    QXmlStreamWriter xml(&xmlText);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("lhCommunicationConfig"));
    xml.writeAttribute(QStringLiteral("generatedBy"), QStringLiteral("LH"));
    xml.writeAttribute(QStringLiteral("generatedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
    xml.writeAttribute(QStringLiteral("project"), config.projectName);
    xml.writeStartElement(QStringLiteral("opc"));
    xml.writeAttribute(QStringLiteral("enabled"), config.opcServer.enabled ? QStringLiteral("true") : QStringLiteral("false"));
    xml.writeAttribute(QStringLiteral("progId"), config.opcServer.opcProgId);
    xml.writeAttribute(QStringLiteral("channel"), config.opcServer.channelName);
    xml.writeAttribute(QStringLiteral("device"), config.opcServer.deviceName);
    xml.writeAttribute(QStringLiteral("serialMode"), config.opcServer.serialMode);
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("tags"));
    for (const auto& tag : tags) {
        xml.writeStartElement(QStringLiteral("tag"));
        xml.writeAttribute(QStringLiteral("name"), tag.tagName);
        xml.writeAttribute(QStringLiteral("dataType"), tag.dataType);
        xml.writeAttribute(QStringLiteral("group"), tag.tagGroup);
        xml.writeAttribute(QStringLiteral("access"), tag.tagAccess);
        xml.writeAttribute(QStringLiteral("server"), tag.server);
        xml.writeAttribute(QStringLiteral("item"), tag.item);
        xml.writeAttribute(QStringLiteral("ioGroup"), tag.ioGroup);
        xml.writeAttribute(QStringLiteral("unit"), tag.unit);
        xml.writeCharacters(tag.tagDescription);
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();

    QString tagText = artifactHeader(
        QStringLiteral("LH communication tag file"),
        QStringLiteral("tag_name\tdata_type\titem\tgroup\taccess\tserver\tio_group\tunit"));
    for (const auto& tag : tags)
        tagText += tagLine(tag) + QLatin1Char('\n');

    QString actText = artifactHeader(
        QStringLiteral("LH communication parameter file"),
        QStringLiteral("tag_name\titem\taddressing\tmetadata\tdescription"));
    for (const auto& tag : tags) {
        actText += QStringList{
            tag.tagName,
            tag.item,
            compactJson(QJsonObject::fromVariantMap(tag.addressing)),
            compactJson(QJsonObject::fromVariantMap(tag.metadata)),
            tag.tagDescription
        }.join(QLatin1Char('\t')) + QLatin1Char('\n');
    }

    QString txText = artifactHeader(
        QStringLiteral("LH communication transmit view"),
        QStringLiteral("point_id\tname\tkind\tdata_type\taccess\titem\ttag_name\taddressing"));
    QString rxText = artifactHeader(
        QStringLiteral("LH communication receive view"),
        QStringLiteral("point_id\tname\tkind\tdata_type\taccess\titem\ttag_name\taddressing"));
    QString rtText = artifactHeader(
        QStringLiteral("LH communication realtime view"),
        QStringLiteral("point_id\tname\tkind\tdata_type\taccess\titem\ttag_name\taddressing"));
    QString engText = artifactHeader(
        QStringLiteral("LH engineering value view"),
        QStringLiteral("point_id\tname\tdata_type\tunit\tdefault_value\tmin_value\tmax_value\tscale\toffset\ttag_name"));
    QString commText = artifactHeader(
        QStringLiteral("LH communication address view"),
        QStringLiteral("point_id\tname\tkind\ttag_name\titem\tmetadata_opc_item\taddressing_opc_item\tprotocol\tarea\taddress\tregister_type\tregister_address\tbit_index\tunit_id"));
    QString msgText = artifactHeader(
        QStringLiteral("LH communication debug index"),
        QStringLiteral("key\tvalue"));

    int txCount = 0;
    int rxCount = 0;
    int rtCount = 0;
    int missingAddressCount = 0;
    const int pairCount = qMin(points.size(), tags.size());
    for (int i = 0; i < pairCount; ++i) {
        const RuntimePointDefinition& point = points.at(i);
        const OpcTagDefinition& tag = tags.at(i);
        if (isReadablePoint(point)) {
            txText += pointLine(point, tag) + QLatin1Char('\n');
            ++txCount;
        }
        if (isWritablePoint(point)) {
            rxText += pointLine(point, tag) + QLatin1Char('\n');
            ++rxCount;
        }
        if (point.access == RuntimePointAccess::ReadWrite) {
            rtText += pointLine(point, tag) + QLatin1Char('\n');
            ++rtCount;
        }
        engText += engineeringLine(point, tag) + QLatin1Char('\n');
        commText += communicationAddressLine(point, tag) + QLatin1Char('\n');

        const QVariantMap addressing = point.addressing;
        if (!addressing.contains(QStringLiteral("address"))
                && !addressing.contains(QStringLiteral("regAddress"))
                && !addressing.contains(QStringLiteral("opcItemId"))
                && !tag.metadata.contains(QStringLiteral("opcItemId"))) {
            ++missingAddressCount;
            msgText += QStringLiteral("warning\tpoint %1 has no explicit communication address\n")
                    .arg(point.id);
        }
    }
    msgText += QStringLiteral("status\tsuccess\n");
    msgText += QStringLiteral("tx_count\t%1\n").arg(txCount);
    msgText += QStringLiteral("rx_count\t%1\n").arg(rxCount);
    msgText += QStringLiteral("rt_count\t%1\n").arg(rtCount);
    msgText += QStringLiteral("engineering_count\t%1\n").arg(points.size());
    msgText += QStringLiteral("comm_count\t%1\n").arg(pairCount);
    msgText += QStringLiteral("missing_address_count\t%1\n").arg(missingAddressCount);
    msgText += QStringLiteral("output_xml\t%1\n").arg(QDir::toNativeSeparators(xmlPath));

    QString reportText = artifactHeader(
        QStringLiteral("LH communication compile report"),
        QStringLiteral("key\tvalue"));
    reportText += QStringLiteral("status\tsuccess\n");
    reportText += QStringLiteral("runtime_point_count\t%1\n").arg(points.size());
    reportText += QStringLiteral("tag_count\t%1\n").arg(tags.size());
    reportText += QStringLiteral("tx_count\t%1\n").arg(txCount);
    reportText += QStringLiteral("rx_count\t%1\n").arg(rxCount);
    reportText += QStringLiteral("rt_count\t%1\n").arg(rtCount);
    reportText += QStringLiteral("output_xml\t%1\n").arg(QDir::toNativeSeparators(xmlPath));

    QList<QPair<QString, QString>> artifactFiles;
    artifactFiles.append(qMakePair(xmlPath, xmlText));
    artifactFiles.append(qMakePair(tagPath, tagText));
    artifactFiles.append(qMakePair(actPath, actText));
    artifactFiles.append(qMakePair(txPath, txText));
    artifactFiles.append(qMakePair(rxPath, rxText));
    artifactFiles.append(qMakePair(rtPath, rtText));
    artifactFiles.append(qMakePair(engPath, engText));
    artifactFiles.append(qMakePair(commPath, commText));
    artifactFiles.append(qMakePair(msgPath, msgText));
    artifactFiles.append(qMakePair(reportPath, reportText));

    QList<StagedArtifact> stagedArtifacts;
    QString errorMessage;
    if (!stageTextArtifacts(artifactFiles, &stagedArtifacts, &errorMessage)
            || !publishStagedArtifacts(stagedArtifacts,
                                       &errorMessage,
                                       shouldInjectPublishFailure(this))) {
        result.success = false;
        result.errors.append(errorMessage);
        result.stdErr = errorMessage;
        return result;
    }

    const QVariantMap metadata = artifactMetadata(projectPath, outputDir, config.projectName);
    result.success = true;
    result.artifacts.append(makeArtifact(QString::fromLatin1(CompileArtifactType::CommunicationXml),
                                         xmlPath,
                                         QStringLiteral("lh_communication_xml"),
                                         metadata));
    result.artifacts.append(makeArtifact(QString::fromLatin1(CompileArtifactType::CommunicationTags),
                                         tagPath,
                                         QStringLiteral("lh_communication_tags"),
                                         metadata));
    result.artifacts.append(makeArtifact(QString::fromLatin1(CompileArtifactType::CommunicationAct),
                                         actPath,
                                         QStringLiteral("lh_communication_act"),
                                         metadata));
    result.artifacts.append(makeArtifact(QString::fromLatin1(CompileArtifactType::CommunicationTx),
                                         txPath,
                                         QStringLiteral("lh_communication_tx"),
                                         metadata));
    result.artifacts.append(makeArtifact(QString::fromLatin1(CompileArtifactType::CommunicationRx),
                                         rxPath,
                                         QStringLiteral("lh_communication_rx"),
                                         metadata));
    result.artifacts.append(makeArtifact(QString::fromLatin1(CompileArtifactType::CommunicationRt),
                                         rtPath,
                                         QStringLiteral("lh_communication_rt"),
                                         metadata));
    result.artifacts.append(makeArtifact(QString::fromLatin1(CompileArtifactType::CommunicationEngineering),
                                         engPath,
                                         QStringLiteral("lh_communication_engineering"),
                                         metadata));
    result.artifacts.append(makeArtifact(QString::fromLatin1(CompileArtifactType::CommunicationComm),
                                         commPath,
                                         QStringLiteral("lh_communication_comm"),
                                         metadata));
    result.artifacts.append(makeArtifact(QString::fromLatin1(CompileArtifactType::CommunicationDebug),
                                         msgPath,
                                         QStringLiteral("lh_communication_debug"),
                                         metadata));
    result.artifacts.append(makeArtifact(QString::fromLatin1(CompileArtifactType::CompileReport),
                                         reportPath,
                                         QStringLiteral("lh_communication_report"),
                                         metadata));
    result.stdOut = QStringLiteral("生成通信产物: %1").arg(QDir::toNativeSeparators(xmlPath));
    return result;
}
