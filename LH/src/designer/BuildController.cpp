/**
 * @file BuildController.cpp
 * @brief Build flow controller implementation
 */

#include "BuildController.h"
#include "Common.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

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
    compileCommon(BuildType::Parameters, projectPath, config);
}

void BuildController::compileCommunication(const QString& projectPath,
                                           const ProjectRuntimeConfig& config)
{
    compileCommon(BuildType::Communication, projectPath, config);
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
        emit logMessage(timestampedMessage("未找到当前项目的 DSL 脚本。"));
        emit compileFailed(type, QStringLiteral("DSL 脚本不存在。"));
        return;
    }

    // 编译类型对应的中文名称
    const char* typeNames[] = {"组态", "参数", "通信"};
    const char* typeName = typeNames[static_cast<int>(type)];

    setBusy(true);
    emit compileStarted(type);
    emit logMessage(timestampedMessage(
            QStringLiteral("开始编译%1: %2").arg(typeName).arg(QFileInfo(sourceFile).fileName())));
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
    if (!mainPath.isEmpty() && QFileInfo::exists(mainPath)) {
        return mainPath;
    }

    const QString legacyConfigPath = resolvePath(config.dslScriptPath);
    if (!legacyConfigPath.isEmpty() && QFileInfo::exists(legacyConfigPath)) {
        return legacyConfigPath;
    }

    const QString dslPath = QDir(m_currentProjectPath).absoluteFilePath(QStringLiteral("main.dsl"));
    if (QFileInfo::exists(dslPath)) {
        return dslPath;
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
