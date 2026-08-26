#include "DSLCompilerInterface.h"
#include "DSLCompilerInternal.h"
#include "TextEncoding.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>

quint64 DSLCompilerInterface::allocateOperationGeneration(quint64 requestedGeneration)
{
    if (requestedGeneration != 0) {
        if (requestedGeneration > m_nextOperationGeneration) {
            m_nextOperationGeneration = requestedGeneration;
        }
        return requestedGeneration;
    }
    return ++m_nextOperationGeneration;
}

void DSLCompilerInterface::emitCompileFailedToStart(quint64 operationGeneration,
                                                     const QString& errorString)
{
    emit compileFailedToStartForGeneration(operationGeneration, errorString);
    emit compileFailedToStart(errorString);
}

void DSLCompilerInterface::resetFinishedProcess()
{
    if (!m_process) {
        return;
    }

    QProcess* process = m_process;
    m_process = nullptr;
    m_asyncOperationGeneration = 0;
    QObject::disconnect(process, nullptr, this, nullptr);
    process->deleteLater();
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
                                                     const QString& compilerInputFile,
                                                     const QString& projectPath,
                                                     const QString& generationId,
                                                     quint64 operationGeneration)
{
    m_process = new QProcess(this);
    m_asyncOperationGeneration = allocateOperationGeneration(operationGeneration);
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
    m_process->setProperty("projectPath", projectPath);
    m_process->setProperty("generationId", generationId);
    m_process->setProperty("operationGeneration", QVariant::fromValue(m_asyncOperationGeneration));
    m_process->setProperty("projectCompile", !projectPath.isEmpty());
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
                                               const QString& projectName,
                                               quint64 operationGeneration)
{
    const quint64 generation = allocateOperationGeneration(operationGeneration);

    if (m_process && m_process->state() != QProcess::NotRunning) {
        const QString msg =
                QStringLiteral("DSL compile request ignored: previous compile is still running.");
        LOG_WARN(msg);
        emitCompileFailedToStart(generation, msg);
        return;
    }

    if (m_process) {
        resetFinishedProcess();
    }
    m_asyncProjectConfig.clear();

    const QString python = resolvePythonInterpreter();
    if (python.isEmpty()) {
        const QString msg = QStringLiteral(
                "No suitable Python interpreter found for DSL compile. "
                "Recreate third_party/custom_dsp_language/compile/venv "
                "and install requirements.txt, or install Python 3 plus antlr4-python3-runtime in PATH.");
        LOG_ERROR(msg);
        emitCompileFailedToStart(generation, msg);
        return;
    }

    const QString workDir = compilerWorkingDir();
    const QString entryScript = compilerEntryScript();
    if (!QFileInfo::exists(entryScript)) {
        const QString msg =
                QStringLiteral("Compiler entry script not found: %1").arg(entryScript);
        LOG_ERROR(msg);
        emitCompileFailedToStart(generation, msg);
        return;
    }

    QDir dir;
    if (!dir.mkpath(outputDir)) {
        const QString msg =
                QStringLiteral("Failed to create output directory: %1").arg(outputDir);
        LOG_ERROR(msg);
        emitCompileFailedToStart(generation, msg);
        return;
    }

    QString inputError;
    const QString compilerInputFile = prepareCompilerInput(sourceFile, outputDir, &inputError);
    if (compilerInputFile.isEmpty()) {
        qWarning() << "[DSLCompilerInterface]" << inputError;
        emitCompileFailedToStart(generation, inputError);
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
                              compilerInputFile,
                              QString(),
                              QString(),
                              generation);
}

void DSLCompilerInterface::compileProjectAsync(const QString& projectPath,
                                               const ProjectRuntimeConfig& config,
                                               const QString& outputDir,
                                               const QString& projectName,
                                               quint64 operationGeneration)
{
    const quint64 generation = allocateOperationGeneration(operationGeneration);

    if (m_process && m_process->state() != QProcess::NotRunning) {
        const QString msg =
                QStringLiteral("DSL compile request ignored: previous compile is still running.");
        LOG_WARN(msg);
        emitCompileFailedToStart(generation, msg);
        return;
    }

    if (m_process) {
        resetFinishedProcess();
    }
    m_asyncProjectConfig.clear();

    const QString mainScriptFile = DSLCompilerInternal::resolveProjectMainScriptPath(projectPath, config);
    if (mainScriptFile.isEmpty()) {
        emitCompileFailedToStart(generation,
                                 QStringLiteral("Main script not found for project compile."));
        return;
    }

    QString pathError;
    if (!DSLCompilerInternal::validateProjectScriptPath(projectPath,
                                                        config.mainScriptPath.isEmpty()
                                                                ? QStringLiteral("main.lh")
                                                                : config.mainScriptPath,
                                                        &pathError)) {
        emitCompileFailedToStart(generation, pathError);
        return;
    }

    for (const QString& script : config.scriptFiles) {
        if (!DSLCompilerInternal::validateProjectScriptPath(projectPath, script, &pathError)) {
            emitCompileFailedToStart(generation, pathError);
            return;
        }
    }

    const QStringList scriptFiles = DSLCompilerInternal::normalizeProjectScriptFiles(projectPath, config, mainScriptFile);
    for (const QString& scriptFile : scriptFiles) {
        if (!DSLCompilerInternal::validateProjectScriptPath(projectPath, scriptFile, &pathError)) {
            emitCompileFailedToStart(generation, pathError);
            return;
        }
    }

    QString generationId;
    QString generationError;
    const QString generationDir = DSLCompilerInternal::createProjectGeneration(projectPath,
                                                                                config,
                                                                                outputDir,
                                                                                &generationId,
                                                                                &generationError);
    if (generationDir.isEmpty()) {
        emitCompileFailedToStart(generation, generationError);
        return;
    }

    QString assemblyError;
    const QString compilerInputFile = DSLCompilerInternal::assembleProjectCompilerInput(projectPath,
                                                                                       generationDir,
                                                                                       mainScriptFile,
                                                                                       scriptFiles,
                                                                                       &assemblyError);
    if (compilerInputFile.isEmpty()) {
        emitCompileFailedToStart(generation, assemblyError);
        return;
    }

    const QString python = resolvePythonInterpreter();
    if (python.isEmpty()) {
        emitCompileFailedToStart(generation, QStringLiteral(
                "No suitable Python interpreter found for DSL compile. "
                "Recreate third_party/custom_dsp_language/compile/venv "
                "and install requirements.txt, or install Python 3 plus antlr4-python3-runtime in PATH."));
        return;
    }

    const QString workDir = compilerWorkingDir();
    const QString entryScript = compilerEntryScript();
    if (!QFileInfo::exists(entryScript)) {
        emitCompileFailedToStart(generation,
                                 QStringLiteral("Compiler entry script not found: %1").arg(entryScript));
        return;
    }

    const QString outputFile = DSLCompilerInternal::projectOutputFile(generationDir);
    m_asyncProjectConfig = config;
    const QStringList args = DSLCompilerInternal::buildCompilerProcessArgs(entryScript, compilerInputFile, outputFile);
    startAsyncCompilerProcess(QStringLiteral("[DSLCompilerInterface] Running project compile (async)"),
                              python,
                              args,
                              workDir,
                              mainScriptFile,
                              mainScriptFile,
                              scriptFiles,
                              generationDir,
                              projectName,
                              outputFile,
                              compilerInputFile,
                              projectPath,
                              generationId,
                              generation);
}

void DSLCompilerInterface::cancelCurrentCompile()
{
    QProcess* process = m_process;
    if (!process) {
        return;
    }

    if (m_compileTimeoutTimer) {
        m_compileTimeoutTimer->stop();
    }

    m_process = nullptr;
    m_asyncOperationGeneration = 0;
    QObject::disconnect(process, nullptr, this, nullptr);
    if (process->state() != QProcess::NotRunning) {
        process->terminate();
        process->kill();
    }
    process->deleteLater();
    m_asyncStdOut.clear();
    m_asyncStdErr.clear();
    m_asyncProjectConfig.clear();
}

void DSLCompilerInterface::onProcessReadyReadStandardOutput()
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process || process != m_process)
        return;
    m_asyncStdOut.append(
            TextEncoding::decodeUtf8WithLocalFallback(process->readAllStandardOutput()));
}

void DSLCompilerInterface::onProcessReadyReadStandardError()
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process || process != m_process)
        return;
    m_asyncStdErr.append(
            TextEncoding::decodeUtf8WithLocalFallback(process->readAllStandardError()));
}

void DSLCompilerInterface::onProcessErrorOccurred(QProcess::ProcessError error)
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process || process != m_process)
        return;

    if (error == QProcess::FailedToStart) {
        if (m_compileTimeoutTimer) {
            m_compileTimeoutTimer->stop();
        }
        const quint64 operationGeneration = m_asyncOperationGeneration;
        const QString msg =
                QStringLiteral("DSL compiler process failed to start: %1")
                .arg(process->errorString());
        LOG_ERROR(msg);
        QObject::disconnect(process, nullptr, this, nullptr);
        process->deleteLater();
        m_process = nullptr;
        m_asyncOperationGeneration = 0;
        m_asyncStdOut.clear();
        m_asyncStdErr.clear();
        m_asyncProjectConfig.clear();
        emitCompileFailedToStart(operationGeneration, msg);
        return;
    }

    const QString msg =
            QStringLiteral("DSL compiler process error: %1")
            .arg(process->errorString());
    LOG_WARN(msg);
    m_asyncStdErr.append(msg + QLatin1Char('\n'));
}

void DSLCompilerInterface::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process || process != m_process)
        return;

    if (m_compileTimeoutTimer) {
        m_compileTimeoutTimer->stop();
    }

    m_asyncStdOut.append(
            TextEncoding::decodeUtf8WithLocalFallback(process->readAllStandardOutput()));
    m_asyncStdErr.append(
            TextEncoding::decodeUtf8WithLocalFallback(process->readAllStandardError()));

    const bool normalExit = (exitStatus == QProcess::NormalExit);
    const quint64 operationGeneration = m_asyncOperationGeneration;
    const QString expectedOutput = process->property("expectedOutputFile").toString();
    bool outputExists = expectedOutput.isEmpty() || QFileInfo::exists(expectedOutput);

    if (process->property("compileTimedOut").toBool()) {
        m_asyncStdErr.append(QStringLiteral("DSL compile process timeout.\n"));
    }

    bool success = (exitCode == 0 && normalExit && outputExists);
    if (success) {
        QString headerError;
        success = DSLCompilerInternal::prependCodeMetadataHeader(expectedOutput,
                                                                 process->property("sourceFile").toString(),
                                                                 process->property("projectName").toString(),
                                                                 process->property("compilerInputFile").toString(),
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

    if (process->property("projectCompile").toBool()) {
        m_lastCompileResult = buildCompileResult(process->property("sourceFile").toString(),
                                                 process->property("outputDir").toString(),
                                                 process->property("projectName").toString(),
                                                 process->property("mainScriptFile").toString(),
                                                 process->property("scriptFiles").toStringList(),
                                                 success,
                                                 m_asyncStdOut,
                                                 m_asyncStdErr,
                                                 m_asyncProjectConfig,
                                                 process->property("projectPath").toString(),
                                                 process->property("generationId").toString());
        m_asyncProjectConfig.clear();
    } else {
        m_lastCompileResult = buildCompileResult(process->property("sourceFile").toString(),
                                                 process->property("outputDir").toString(),
                                                 process->property("projectName").toString(),
                                                 process->property("mainScriptFile").toString(),
                                                 process->property("scriptFiles").toStringList(),
                                                 success,
                                                 m_asyncStdOut,
                                                 m_asyncStdErr);
    }

    success = success && m_lastCompileResult.success;
    const int resultExitCode = success ? 0 : (exitCode == 0 ? 1 : exitCode);
    const bool resultNormalExit = normalExit && success;
    const QString resultStdOut = m_asyncStdOut;
    const QString resultStdErr = m_asyncStdErr;

    QObject::disconnect(process, nullptr, this, nullptr);
    process->deleteLater();
    m_process = nullptr;
    m_asyncOperationGeneration = 0;
    m_asyncStdOut.clear();
    m_asyncStdErr.clear();
    m_asyncProjectConfig.clear();

    emit compileFinishedForGeneration(operationGeneration,
                                      resultExitCode,
                                      resultNormalExit,
                                      resultStdOut,
                                      resultStdErr);
    emit compileFinished(resultExitCode,
                         resultNormalExit,
                         resultStdOut,
                         resultStdErr);
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
