#include "DSLCompilerInterface.h"
#include "DSLCompilerInternal.h"
#include "TextEncoding.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>

void DSLCompilerInterface::resetFinishedProcess()
{
    if (!m_process) {
        return;
    }

    m_process->deleteLater();
    m_process = nullptr;
}

void DSLCompilerInterface::ensureCompileTimeoutTimer()
{
    if (m_compileTimeoutTimer) {
        return;
    }

    m_compileTimeoutTimer = new QTimer(this);
    m_compileTimeoutTimer->setSingleShot(true);
    connect(m_compileTimeoutTimer, &QTimer::timeout,
            this, &DSLCompilerInterface::onCompileTimeout);
}

void DSLCompilerInterface::startAsyncCompilerProcess(const QString& logPrefix,
                                                     const QString& python,
                                                     const QStringList& args,
                                                     const QString& workDir,
                                                     const QString& sourceFile,
                                                     const QString& mainScriptFile,
                                                     const QStringList& scriptFiles,
                                                     const QString& outputDir,
                                                     const QString& projectName,
                                                     const QString& outputFile,
                                                     const QString& compilerInputFile)
{
    m_process = new QProcess(this);
    m_asyncStdOut.clear();
    m_asyncStdErr.clear();

    m_process->setProgram(python);
    m_process->setArguments(args);
    m_process->setWorkingDirectory(workDir);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->setProperty("sourceFile", sourceFile);
    m_process->setProperty("mainScriptFile", mainScriptFile);
    m_process->setProperty("scriptFiles", scriptFiles);
    m_process->setProperty("outputDir", outputDir);
    m_process->setProperty("projectName", projectName);
    m_process->setProperty("expectedOutputFile", outputFile);
    m_process->setProperty("compilerInputFile", compilerInputFile);
    m_process->setProperty("compileTimedOut", false);

    connect(m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            &DSLCompilerInterface::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &DSLCompilerInterface::onProcessErrorOccurred);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &DSLCompilerInterface::onProcessReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &DSLCompilerInterface::onProcessReadyReadStandardError);

    ensureCompileTimeoutTimer();

    LOG_INFO(QStringLiteral("%1: %2 %3, cwd = %4")
             .arg(logPrefix,
                  python,
                  args.join(QLatin1Char(' ')),
                  workDir));

    m_process->start();
    m_compileTimeoutTimer->start(120 * 1000);
}

void DSLCompilerInterface::compileDslFileAsync(const QString& sourceFile,
                                               const QString& outputDir,
                                               const QString& projectName)
{
    m_asyncProjectConfig.clear();

    if (m_process && m_process->state() != QProcess::NotRunning) {
        const QString msg =
                QStringLiteral("DSL compile request ignored: previous compile is still running.");
        LOG_WARN(msg);
        emit compileFailedToStart(msg);
        return;
    }

    if (m_process) {
        resetFinishedProcess();
    }

    const QString python = resolvePythonInterpreter();
    if (python.isEmpty()) {
        const QString msg = QStringLiteral(
                "No suitable Python interpreter found for DSL compile. "
                "Recreate third_party/custom_dsp_language/compile/venv "
                "and install requirements.txt, or install Python 3 plus antlr4-python3-runtime in PATH.");
        LOG_ERROR(msg);
        emit compileFailedToStart(msg);
        return;
    }

    const QString workDir = compilerWorkingDir();
    const QString entryScript = compilerEntryScript();
    if (!QFileInfo::exists(entryScript)) {
        const QString msg =
                QStringLiteral("Compiler entry script not found: %1").arg(entryScript);
        LOG_ERROR(msg);
        emit compileFailedToStart(msg);
        return;
    }

    QDir dir;
    if (!dir.mkpath(outputDir)) {
        const QString msg =
                QStringLiteral("Failed to create output directory: %1").arg(outputDir);
        LOG_ERROR(msg);
        emit compileFailedToStart(msg);
        return;
    }

    QString inputError;
    const QString compilerInputFile = prepareCompilerInput(sourceFile, outputDir, &inputError);
    if (compilerInputFile.isEmpty()) {
        qWarning() << "[DSLCompilerInterface]" << inputError;
        emit compileFailedToStart(inputError);
        return;
    }

    const QString outputFile = DSLCompilerInternal::defaultOutputFileForSource(sourceFile, outputDir);
    const QStringList args = DSLCompilerInternal::buildCompilerProcessArgs(entryScript, compilerInputFile, outputFile);
    startAsyncCompilerProcess(QStringLiteral("[DSLCompilerInterface] Running (async)"),
                              python,
                              args,
                              workDir,
                              sourceFile,
                              sourceFile,
                              QStringList{sourceFile},
                              outputDir,
                              projectName,
                              outputFile,
                              compilerInputFile);
}

void DSLCompilerInterface::compileProjectAsync(const QString& projectPath,
                                               const ProjectRuntimeConfig& config,
                                               const QString& outputDir,
                                               const QString& projectName)
{
    const QString mainScriptFile = DSLCompilerInternal::resolveProjectMainScriptPath(projectPath, config);
    if (mainScriptFile.isEmpty()) {
        emit compileFailedToStart(QStringLiteral("Main script not found for project compile."));
        return;
    }

    const QStringList scriptFiles = DSLCompilerInternal::normalizeProjectScriptFiles(projectPath, config, mainScriptFile);

    QString assemblyError;
    const QString compilerInputFile = DSLCompilerInternal::assembleProjectCompilerInput(projectPath,
                                                                                       outputDir,
                                                                                       mainScriptFile,
                                                                                       scriptFiles,
                                                                                       &assemblyError);
    if (compilerInputFile.isEmpty()) {
        emit compileFailedToStart(assemblyError);
        return;
    }

    const QString python = resolvePythonInterpreter();
    if (python.isEmpty()) {
        emit compileFailedToStart(QStringLiteral(
                "No suitable Python interpreter found for DSL compile. "
                "Recreate third_party/custom_dsp_language/compile/venv "
                "and install requirements.txt, or install Python 3 plus antlr4-python3-runtime in PATH."));
        return;
    }

    const QString workDir = compilerWorkingDir();
    const QString entryScript = compilerEntryScript();
    if (!QFileInfo::exists(entryScript)) {
        emit compileFailedToStart(QStringLiteral("Compiler entry script not found: %1").arg(entryScript));
        return;
    }

    const QString outputFile = DSLCompilerInternal::defaultOutputFileForSource(mainScriptFile, outputDir);
    m_asyncProjectConfig = config;
    const QStringList args = DSLCompilerInternal::buildCompilerProcessArgs(entryScript, compilerInputFile, outputFile);
    startAsyncCompilerProcess(QStringLiteral("[DSLCompilerInterface] Running project compile (async)"),
                              python,
                              args,
                              workDir,
                              mainScriptFile,
                              mainScriptFile,
                              scriptFiles,
                              outputDir,
                              projectName,
                              outputFile,
                              compilerInputFile);
}

void DSLCompilerInterface::cancelCurrentCompile()
{
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        return;
    }

    m_process->terminate();
    if (!m_process->waitForFinished(3000)) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void DSLCompilerInterface::onProcessReadyReadStandardOutput()
{
    if (!m_process)
        return;
    m_asyncStdOut.append(
            TextEncoding::decodeUtf8WithLocalFallback(m_process->readAllStandardOutput()));
}

void DSLCompilerInterface::onProcessReadyReadStandardError()
{
    if (!m_process)
        return;
    m_asyncStdErr.append(
            TextEncoding::decodeUtf8WithLocalFallback(m_process->readAllStandardError()));
}

void DSLCompilerInterface::onProcessErrorOccurred(QProcess::ProcessError error)
{
    if (!m_process)
        return;

    if (error == QProcess::FailedToStart) {
        if (m_compileTimeoutTimer) {
            m_compileTimeoutTimer->stop();
        }
        const QString msg =
                QStringLiteral("DSL compiler process failed to start: %1")
                .arg(m_process->errorString());
        LOG_ERROR(msg);
        emit compileFailedToStart(msg);
        m_process->deleteLater();
        m_process = nullptr;
        m_asyncStdOut.clear();
        m_asyncStdErr.clear();
        return;
    }

    const QString msg =
            QStringLiteral("DSL compiler process error: %1")
            .arg(m_process->errorString());
    LOG_WARN(msg);
    m_asyncStdErr.append(msg + QLatin1Char('\n'));
}

void DSLCompilerInterface::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_process)
        return;

    if (m_compileTimeoutTimer) {
        m_compileTimeoutTimer->stop();
    }

    m_asyncStdOut.append(
            TextEncoding::decodeUtf8WithLocalFallback(m_process->readAllStandardOutput()));
    m_asyncStdErr.append(
            TextEncoding::decodeUtf8WithLocalFallback(m_process->readAllStandardError()));

    const bool normalExit = (exitStatus == QProcess::NormalExit);
    const QString expectedOutput = m_process->property("expectedOutputFile").toString();
    bool outputExists = expectedOutput.isEmpty() || QFileInfo::exists(expectedOutput);

    if (m_process->property("compileTimedOut").toBool()) {
        m_asyncStdErr.append(QStringLiteral("DSL compile process timeout.\n"));
    }

    bool success = (exitCode == 0 && normalExit && outputExists);
    if (success) {
        QString headerError;
        success = DSLCompilerInternal::prependCodeMetadataHeader(expectedOutput,
                                                                 m_process->property("sourceFile").toString(),
                                                                 m_process->property("projectName").toString(),
                                                                 m_process->property("compilerInputFile").toString(),
                                                                 &headerError);
        if (!success) {
            outputExists = false;
            m_asyncStdErr.append(headerError + QLatin1Char('\n'));
        }
    }

    if (success) {
        LOG_INFO("[DSLCompilerInterface] async compile succeeded.");
    } else {
        if (!outputExists) {
            m_asyncStdErr.append(QStringLiteral("Expected output file was not generated: %1\n").arg(expectedOutput));
        }
        LOG_WARN(QStringLiteral("[DSLCompilerInterface] async compile failed. exitCode=%1, normalExit=%2")
                 .arg(exitCode)
                 .arg(normalExit));
    }

    if (!m_asyncProjectConfig.isEmpty()) {
        m_lastCompileResult = buildCompileResult(m_process->property("sourceFile").toString(),
                                                 m_process->property("outputDir").toString(),
                                                 m_process->property("projectName").toString(),
                                                 m_process->property("mainScriptFile").toString(),
                                                 m_process->property("scriptFiles").toStringList(),
                                                 success,
                                                 m_asyncStdOut,
                                                 m_asyncStdErr,
                                                 m_asyncProjectConfig);
        m_asyncProjectConfig.clear();
    } else {
        m_lastCompileResult = buildCompileResult(m_process->property("sourceFile").toString(),
                                                 m_process->property("outputDir").toString(),
                                                 m_process->property("projectName").toString(),
                                                 m_process->property("mainScriptFile").toString(),
                                                 m_process->property("scriptFiles").toStringList(),
                                                 success,
                                                 m_asyncStdOut,
                                                 m_asyncStdErr);
    }

    emit compileFinished(success ? 0 : exitCode, normalExit && success, m_asyncStdOut, m_asyncStdErr);

    m_process->deleteLater();
    m_process = nullptr;
    m_asyncStdOut.clear();
    m_asyncStdErr.clear();
}

void DSLCompilerInterface::onCompileTimeout()
{
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        return;
    }

    LOG_WARN("[DSLCompilerInterface] async compile timeout, killing process.");
    m_process->setProperty("compileTimedOut", true);
    m_process->kill();
}
