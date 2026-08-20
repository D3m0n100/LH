#include "DSLCompilerInterface.h"
#include "DSLCompilerInternal.h"
#include "TextEncoding.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

QString DSLCompilerInterface::s_cachedPythonInterpreter;

namespace {

bool canExecutePython(const QString& program, QString* details = nullptr)
{
    QProcess proc;
    proc.start(program,
               QStringList()
                   << QStringLiteral("-c")
                   << QStringLiteral("import sys; "
                                     "raise SystemExit(0 if sys.version_info[0] >= 3 else 1)"));
    if (!proc.waitForStarted(3000)) {
        if (details) {
            *details = QStringLiteral("Failed to start: %1").arg(proc.errorString());
        }
        return false;
    }

    if (!proc.waitForFinished(5000)) {
        proc.kill();
        proc.waitForFinished(1000);
        if (details) {
            *details = QStringLiteral("Timeout while probing interpreter.");
        }
        return false;
    }

    const QString stdOut =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardOutput()).trimmed();
    const QString stdErr =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardError()).trimmed();
    const bool ok = proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;

    if (!ok && details) {
        QStringList parts;
        parts << QStringLiteral("Python 3 is required. Exit code: %1").arg(proc.exitCode());
        if (!stdErr.isEmpty())
            parts << stdErr;
        if (!stdOut.isEmpty())
            parts << stdOut;
        *details = parts.join(QStringLiteral(" | "));
    }

    return ok;
}

bool hasDslRuntimeSupport(const QString& program,
                          const QString& workingDirectory,
                          QString* details = nullptr)
{
    QProcess proc;
    proc.setWorkingDirectory(workingDirectory);
    proc.start(program,
               QStringList()
                   << QStringLiteral("-c")
                   << QStringLiteral("import antlr4"));

    if (!proc.waitForStarted(3000)) {
        if (details) {
            *details = QStringLiteral("Failed to start dependency probe: %1")
                               .arg(proc.errorString());
        }
        return false;
    }

    if (!proc.waitForFinished(5000)) {
        proc.kill();
        proc.waitForFinished(1000);
        if (details) {
            *details = QStringLiteral("Timeout while probing DSL runtime dependencies.");
        }
        return false;
    }

    const QString stdOut =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardOutput()).trimmed();
    const QString stdErr =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardError()).trimmed();
    const bool ok = proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;

    if (!ok && details) {
        QStringList parts;
        parts << QStringLiteral("Missing required module 'antlr4-python3-runtime'");
        if (!stdErr.isEmpty())
            parts << stdErr;
        if (!stdOut.isEmpty())
            parts << stdOut;
        *details = parts.join(QStringLiteral(" | "));
    }

    return ok;
}

} // namespace

DSLCompilerInterface::DSLCompilerInterface(QObject* parent)
    : QObject(parent)
{
}

QString DSLCompilerInterface::compilerWorkingDir() const
{
    const QString relativePath = QStringLiteral("third_party/custom_dsp_language/compile");
    QStringList candidates;

#ifdef LX_PROJECT_SOURCE_DIR
    candidates << QDir(QStringLiteral(LX_PROJECT_SOURCE_DIR)).absoluteFilePath(relativePath);
#endif

    QDir dir(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 5; ++depth) {
        candidates << dir.absoluteFilePath(relativePath);
        candidates << dir.absoluteFilePath(
                QStringLiteral("LX_platform_recent_folder_blank_and_stm32_prompt/")
                + relativePath);
        if (!dir.cdUp()) {
            break;
        }
    }

    for (const QString& candidate : candidates) {
        const QString cleanCandidate = QDir::cleanPath(candidate);
        if (QFileInfo::exists(QDir(cleanCandidate).absoluteFilePath(QStringLiteral("lmc.py")))) {
            return cleanCandidate;
        }
    }

    return QDir::cleanPath(candidates.isEmpty()
            ? QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(relativePath)
            : candidates.constFirst());
}

QString DSLCompilerInterface::compilerEntryScript() const
{
    return QDir(compilerWorkingDir()).absoluteFilePath(QStringLiteral("lmc.py"));
}

QString DSLCompilerInterface::resolvePythonInterpreter() const
{
    if (!s_cachedPythonInterpreter.isEmpty()) {
        return s_cachedPythonInterpreter;
    }

    QDir workDir(compilerWorkingDir());

#ifdef Q_OS_WIN
    const QString venvPython =
            workDir.absoluteFilePath(QStringLiteral("venv/Scripts/python.exe"));
#else
    const QString venvPython =
            workDir.absoluteFilePath(QStringLiteral("venv/bin/python3"));
#endif

    if (QFileInfo::exists(venvPython)) {
        QString probeDetails;
        if (canExecutePython(venvPython, &probeDetails)
                && hasDslRuntimeSupport(venvPython, workDir.absolutePath(), &probeDetails)) {
            qInfo() << "[DSLCompilerInterface] Using venv Python interpreter:" << venvPython;
            s_cachedPythonInterpreter = venvPython;
            return venvPython;
        }

        qWarning() << "[DSLCompilerInterface] Venv Python is not suitable for DSL compile:"
                   << venvPython << "|" << probeDetails;
    }

    QString envPython = qEnvironmentVariable("PYTHON");
    QStringList candidates;
    if (!envPython.isEmpty())
        candidates << envPython;

#ifdef Q_OS_WIN
    candidates << QStringLiteral("py")
               << QStringLiteral("python")
               << QStringLiteral("python3");
#else
    candidates << QStringLiteral("python3")
               << QStringLiteral("python");
#endif

    for (const QString& cmd : candidates) {
        if (cmd.trimmed().isEmpty())
            continue;

        QString probeDetails;
        if (canExecutePython(cmd, &probeDetails)
                && hasDslRuntimeSupport(cmd, workDir.absolutePath(), &probeDetails)) {
            qInfo() << "[DSLCompilerInterface] Detected Python interpreter:" << cmd;
            s_cachedPythonInterpreter = cmd;
            return cmd;
        }
        qWarning() << "[DSLCompilerInterface] Python candidate is not suitable for DSL compile:"
                   << cmd << "|" << probeDetails;
    }

    qWarning() << "[DSLCompilerInterface] No Python interpreter found."
               << "Please install Python 3, then install requirements.txt for the DSL compiler.";
    return QString();
}

bool DSLCompilerInterface::compileDslFile(const QString& sourceFile,
                                          const QString& outputDir,
                                          const QString& projectName,
                                          QString* compilerStdout,
                                          QString* compilerStderr)
{
    const QString python = resolvePythonInterpreter();
    if (python.isEmpty()) {
        if (compilerStderr)
            *compilerStderr = QStringLiteral(
                    "No suitable Python interpreter found for DSL compile. "
                    "Recreate third_party/custom_dsp_language/compile/venv "
                    "and install requirements.txt, or install Python 3 plus antlr4-python3-runtime in PATH.");
        return false;
    }

    const QString workDir = compilerWorkingDir();
    const QString entryScript = compilerEntryScript();
    if (!QFileInfo::exists(entryScript)) {
        const QString msg =
                QStringLiteral("Compiler entry script not found: %1").arg(entryScript);
        qWarning() << "[DSLCompilerInterface]" << msg;
        if (compilerStderr)
            *compilerStderr = msg;
        return false;
    }

    if (!outputDir.isEmpty()) {
        QDir dir;
        if (!dir.mkpath(outputDir)) {
            const QString msg =
                    QStringLiteral("Failed to create output directory: %1").arg(outputDir);
            qWarning() << "[DSLCompilerInterface]" << msg;
            if (compilerStderr)
                *compilerStderr = msg;
            return false;
        }
    }

    QString inputError;
    const QString compilerInputFile = prepareCompilerInput(sourceFile, outputDir, &inputError);
    if (compilerInputFile.isEmpty()) {
        qWarning() << "[DSLCompilerInterface]" << inputError;
        if (compilerStderr)
            *compilerStderr = inputError;
        return false;
    }

    const QString outputFile = DSLCompilerInternal::defaultOutputFileForSource(sourceFile, outputDir);

    QString stdOut;
    QString stdErr;
    const QString compileError = DSLCompilerInternal::runDslCompilerProcess(python,
                                                                           entryScript,
                                                                           compilerInputFile,
                                                                           outputFile,
                                                                           workDir,
                                                                           &stdOut,
                                                                           &stdErr);
    if (compilerStdout)
        *compilerStdout = stdOut;
    if (compilerStderr)
        *compilerStderr = stdErr;

    bool ok = compileError.isEmpty();
    if (ok) {
        QString headerError;
        ok = DSLCompilerInternal::prependCodeMetadataHeader(outputFile,
                                                            sourceFile,
                                                            projectName,
                                                            compilerInputFile,
                                                            &headerError);
        if (!ok) {
            stdErr.append(headerError + QLatin1Char('\n'));
        }
    }

    m_lastCompileResult = buildCompileResult(sourceFile,
                                             outputDir,
                                             projectName,
                                             sourceFile,
                                             QStringList{sourceFile},
                                             ok,
                                             stdOut,
                                             stdErr);

    if (!ok) {
        qWarning() << "[DSLCompilerInterface] compileDslFile failed." << compileError;
    }

    return ok;
}

CompileResult DSLCompilerInterface::compileDslFileWithResult(const QString& sourceFile,
                                                             const QString& outputDir,
                                                             const QString& projectName)
{
    QString stdOut;
    QString stdErr;
    const bool ok = compileDslFile(sourceFile, outputDir, projectName, &stdOut, &stdErr);
    m_lastCompileResult = buildCompileResult(sourceFile,
                                             outputDir,
                                             projectName,
                                             sourceFile,
                                             QStringList{sourceFile},
                                             ok,
                                             stdOut,
                                             stdErr);
    return m_lastCompileResult;
}

CompileResult DSLCompilerInterface::compileProjectWithResult(const QString& projectPath,
                                                             const ProjectRuntimeConfig& config,
                                                             const QString& outputDir,
                                                             const QString& projectName)
{
    const QString mainScriptFile = DSLCompilerInternal::resolveProjectMainScriptPath(projectPath, config);
    if (mainScriptFile.isEmpty()) {
        CompileResult result;
        result.success = false;
        result.projectName = projectName;
        result.errors.append(QStringLiteral("Main script not found for project compile."));
        m_lastCompileResult = result;
        return m_lastCompileResult;
    }

    QString pathError;
    if (!DSLCompilerInternal::validateProjectScriptPath(projectPath,
                                                        config.mainScriptPath.isEmpty()
                                                                ? QStringLiteral("main.lh")
                                                                : config.mainScriptPath,
                                                        &pathError)) {
        CompileResult result;
        result.projectName = projectName;
        result.errors.append(pathError);
        result.stdErr = pathError;
        m_lastCompileResult = result;
        return m_lastCompileResult;
    }

    for (const QString& script : config.scriptFiles) {
        if (!DSLCompilerInternal::validateProjectScriptPath(projectPath, script, &pathError)) {
            CompileResult result;
            result.projectName = projectName;
            result.errors.append(pathError);
            result.stdErr = pathError;
            m_lastCompileResult = result;
            return m_lastCompileResult;
        }
    }

    const QStringList scriptFiles = DSLCompilerInternal::normalizeProjectScriptFiles(projectPath, config, mainScriptFile);
    for (const QString& scriptFile : scriptFiles) {
        if (!DSLCompilerInternal::validateProjectScriptPath(projectPath, scriptFile, &pathError)) {
            CompileResult result;
            result.projectName = projectName;
            result.errors.append(pathError);
            result.stdErr = pathError;
            m_lastCompileResult = result;
            return m_lastCompileResult;
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
        CompileResult result;
        result.projectName = projectName;
        result.errors.append(generationError);
        result.stdErr = generationError;
        m_lastCompileResult = result;
        return m_lastCompileResult;
    }

    QString assemblyError;
    const QString compilerInputFile = DSLCompilerInternal::assembleProjectCompilerInput(projectPath,
                                                                                       generationDir,
                                                                                       mainScriptFile,
                                                                                       scriptFiles,
                                                                                       &assemblyError);
    if (compilerInputFile.isEmpty()) {
        CompileResult result;
        result.success = false;
        result.projectName = projectName;
        result.errors.append(assemblyError);
        result.stdErr = assemblyError;
        m_lastCompileResult = result;
        return m_lastCompileResult;
    }

    const QString python = resolvePythonInterpreter();
    if (python.isEmpty()) {
        CompileResult result;
        result.success = false;
        result.projectName = projectName;
        result.errors.append(QStringLiteral(
                "No suitable Python interpreter found for DSL compile. "
                "Recreate third_party/custom_dsp_language/compile/venv "
                "and install requirements.txt, or install Python 3 plus antlr4-python3-runtime in PATH."));
        result.stdErr = result.errors.join(QLatin1Char('\n'));
        m_lastCompileResult = result;
        return m_lastCompileResult;
    }

    const QString workDir = compilerWorkingDir();
    const QString entryScript = compilerEntryScript();
    if (!QFileInfo::exists(entryScript)) {
        CompileResult result;
        result.success = false;
        result.projectName = projectName;
        result.errors.append(QStringLiteral("Compiler entry script not found: %1").arg(entryScript));
        result.stdErr = result.errors.join(QLatin1Char('\n'));
        m_lastCompileResult = result;
        return m_lastCompileResult;
    }

    QString stdOut;
    QString stdErr;
    const QString outputFile = DSLCompilerInternal::projectOutputFile(generationDir);
    const QString compileError = DSLCompilerInternal::runDslCompilerProcess(python,
                                                                           entryScript,
                                                                           compilerInputFile,
                                                                           outputFile,
                                                                           workDir,
                                                                           &stdOut,
                                                                           &stdErr);
    bool ok = compileError.isEmpty();
    if (ok) {
        QString headerError;
        ok = DSLCompilerInternal::prependCodeMetadataHeader(outputFile,
                                                            mainScriptFile,
                                                            projectName,
                                                            compilerInputFile,
                                                            &headerError);
        if (!ok) {
            stdErr.append(headerError + QLatin1Char('\n'));
        }
    }
    m_lastCompileResult = buildCompileResult(mainScriptFile,
                                             generationDir,
                                             projectName,
                                             mainScriptFile,
                                             scriptFiles,
                                             ok,
                                             stdOut,
                                             stdErr,
                                             config,
                                             projectPath,
                                             generationId);
    return m_lastCompileResult;
}
