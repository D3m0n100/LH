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
    Q_UNUSED(config);

    if (m_busy) {
        emit logMessage(timestampedMessage("编译器正在忙碌，请稍候。"));
        return;
    }

    if (projectPath.isEmpty()) {
        emit logMessage(timestampedMessage("请先打开或创建项目。"));
        emit compileFailed(BuildType::Configuration, QStringLiteral("项目路径为空。"));
        return;
    }

    emit saveRequired();

    QStringList errors;
    bool valid = true;
    emit validationRequired(BuildType::Configuration, errors, valid);
    if (!valid) {
        const QString errorMessage = errors.isEmpty()
                ? QStringLiteral("构建前校验失败。")
                : errors.join('\n');
        emit logMessage(timestampedMessage(errorMessage));
        emit compileFailed(BuildType::Configuration, errorMessage);
        return;
    }

    m_currentBuildType = BuildType::Configuration;
    m_currentProjectPath = projectPath;

    const QString sourceFile = currentDslScriptPath();
    if (sourceFile.isEmpty() || !QFileInfo::exists(sourceFile)) {
        emit logMessage(timestampedMessage("未找到当前项目的 DSL 脚本。"));
        emit compileFailed(BuildType::Configuration, QStringLiteral("DSL 脚本不存在。"));
        return;
    }

    setBusy(true);
    emit compileStarted(BuildType::Configuration);
    emit logMessage(timestampedMessage(
            QStringLiteral("开始编译组态: %1").arg(QFileInfo(sourceFile).fileName())));
    emit logMessage(timestampedMessage(
            QStringLiteral("输出目录: %1").arg(buildOutputDirectory(BuildType::Configuration))));
    emit compileProgress(10);

    m_dslCompiler->compileDslFileAsync(sourceFile,
                                       buildOutputDirectory(BuildType::Configuration),
                                       QFileInfo(projectPath).fileName());
}

void BuildController::compileParameters(const QString& projectPath)
{
    if (m_busy) {
        emit logMessage(timestampedMessage("编译器正在忙碌，请稍候。"));
        return;
    }

    if (projectPath.isEmpty()) {
        emit logMessage(timestampedMessage("请先打开或创建项目。"));
        emit compileFailed(BuildType::Parameters, QStringLiteral("项目路径为空。"));
        return;
    }

    emit saveRequired();

    QStringList errors;
    bool valid = true;
    emit validationRequired(BuildType::Parameters, errors, valid);
    if (!valid) {
        const QString errorMessage = errors.isEmpty()
                ? QStringLiteral("构建前校验失败。")
                : errors.join('\n');
        emit logMessage(timestampedMessage(errorMessage));
        emit compileFailed(BuildType::Parameters, errorMessage);
        return;
    }

    m_currentBuildType = BuildType::Parameters;
    m_currentProjectPath = projectPath;

    const QString sourceFile = currentDslScriptPath();
    if (sourceFile.isEmpty() || !QFileInfo::exists(sourceFile)) {
        emit logMessage(timestampedMessage("未找到当前项目的 DSL 脚本。"));
        emit compileFailed(BuildType::Parameters, QStringLiteral("DSL 脚本不存在。"));
        return;
    }

    setBusy(true);
    emit compileStarted(BuildType::Parameters);
    emit logMessage(timestampedMessage(
            QStringLiteral("开始编译参数: %1").arg(QFileInfo(sourceFile).fileName())));
    emit logMessage(timestampedMessage(
            QStringLiteral("输出目录: %1").arg(buildOutputDirectory(BuildType::Parameters))));
    emit compileProgress(10);

    m_dslCompiler->compileDslFileAsync(sourceFile,
                                       buildOutputDirectory(BuildType::Parameters),
                                       QFileInfo(projectPath).fileName());
}

void BuildController::compileCommunication(const QString& projectPath)
{
    if (m_busy) {
        emit logMessage(timestampedMessage("编译器正在忙碌，请稍候。"));
        return;
    }

    if (projectPath.isEmpty()) {
        emit logMessage(timestampedMessage("请先打开或创建项目。"));
        emit compileFailed(BuildType::Communication, QStringLiteral("项目路径为空。"));
        return;
    }

    emit saveRequired();

    QStringList errors;
    bool valid = true;
    emit validationRequired(BuildType::Communication, errors, valid);
    if (!valid) {
        const QString errorMessage = errors.isEmpty()
                ? QStringLiteral("构建前校验失败。")
                : errors.join('\n');
        emit logMessage(timestampedMessage(errorMessage));
        emit compileFailed(BuildType::Communication, errorMessage);
        return;
    }

    m_currentBuildType = BuildType::Communication;
    m_currentProjectPath = projectPath;

    const QString sourceFile = currentDslScriptPath();
    if (sourceFile.isEmpty() || !QFileInfo::exists(sourceFile)) {
        emit logMessage(timestampedMessage("未找到当前项目的 DSL 脚本。"));
        emit compileFailed(BuildType::Communication, QStringLiteral("DSL 脚本不存在。"));
        return;
    }

    setBusy(true);
    emit compileStarted(BuildType::Communication);
    emit logMessage(timestampedMessage(
            QStringLiteral("开始编译通信: %1").arg(QFileInfo(sourceFile).fileName())));
    emit logMessage(timestampedMessage(
            QStringLiteral("输出目录: %1").arg(buildOutputDirectory(BuildType::Communication))));
    emit compileProgress(10);

    m_dslCompiler->compileDslFileAsync(sourceFile,
                                       buildOutputDirectory(BuildType::Communication),
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

QString BuildController::currentDslScriptPath() const
{
    if (m_currentProjectPath.isEmpty()) {
        return QString();
    }

    const QString lmPath = QDir(m_currentProjectPath).absoluteFilePath(QStringLiteral("main.lm"));
    if (QFileInfo::exists(lmPath)) {
        return lmPath;
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
