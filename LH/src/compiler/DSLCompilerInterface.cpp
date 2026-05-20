#include "DSLCompilerInterface.h"
#include "TextEncoding.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>
#include <QStringList>
#include <QRegularExpression>
#include <QHash>

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

bool hasProgramEnvelope(const QString& sourceText)
{
    static const QRegularExpression programRe(
        QStringLiteral("\\bPROGRAM\\b"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression endProgramRe(
        QStringLiteral("\\bEND_PROGRAM\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return programRe.match(sourceText).hasMatch() && endProgramRe.match(sourceText).hasMatch();
}

QString sanitizeProgramName(const QString& baseName)
{
    QString name = baseName.trimmed();
    if (name.isEmpty()) {
        return QStringLiteral("Main");
    }

    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]")),
                 QStringLiteral("_"));
    if (name.isEmpty()) {
        return QStringLiteral("Main");
    }

    if (name.at(0).isDigit()) {
        name.prepend(QStringLiteral("P_"));
    }
    return name;
}

QString normalizeLegacyDslSource(const QString& sourceText, const QString& sourceBaseName)
{
    const QRegularExpression legacyCallRe(
        QStringLiteral(R"(^(\s*)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([_A-Za-z][_A-Za-z0-9_]*)\s*\()"));
    const QRegularExpression paramAssignRe(
        QStringLiteral(R"(^(\s*[A-Za-z_][A-Za-z0-9_]*\s*)=(\s*.+)$)"));

    const QStringList lines = sourceText.split(QLatin1Char('\n'));
    QStringList transformed;
    transformed.reserve(lines.size());

    QStringList instanceOrder;
    QHash<QString, QString> instanceTypeMap;

    int parenDepth = 0;
    for (const QString& rawLine : lines) {
        QString line = rawLine;
        const QString trimmed = line.trimmed();
        const bool commentOnly = trimmed.startsWith(QStringLiteral("//"));

        const QRegularExpressionMatch callMatch = legacyCallRe.match(line);
        if (callMatch.hasMatch()) {
            const QString indentation = callMatch.captured(1);
            const QString instanceName = callMatch.captured(2);
            const QString blockType = callMatch.captured(3);

            if (!instanceTypeMap.contains(instanceName)) {
                instanceOrder.append(instanceName);
                instanceTypeMap.insert(instanceName, blockType);
            }

            line = indentation + instanceName + QStringLiteral("(");
        } else if (parenDepth > 0
                   && !commentOnly
                   && !trimmed.contains(QStringLiteral(":="))
                   && !trimmed.contains(QStringLiteral("=>"))) {
            const QRegularExpressionMatch paramMatch = paramAssignRe.match(line);
            if (paramMatch.hasMatch()) {
                line = paramMatch.captured(1) + QStringLiteral(":=") + paramMatch.captured(2);
            }
        }

        transformed.append(line);

        parenDepth += line.count(QLatin1Char('('));
        parenDepth -= line.count(QLatin1Char(')'));
        if (parenDepth < 0) {
            parenDepth = 0;
        }
    }

    const QString transformedBody = transformed.join(QLatin1Char('\n'));
    if (hasProgramEnvelope(transformedBody)) {
        return transformedBody;
    }

    QStringList wrapped;
    wrapped << (QStringLiteral("PROGRAM ") + sanitizeProgramName(sourceBaseName));
    wrapped << QStringLiteral("VAR");
    for (const QString& instanceName : instanceOrder) {
        wrapped << QStringLiteral("    %1 : %2;")
                       .arg(instanceName, instanceTypeMap.value(instanceName));
    }
    wrapped << QStringLiteral("END_VAR");
    wrapped << QString();

    const QString trimmedBody = transformedBody.trimmed();
    if (!trimmedBody.isEmpty()) {
        wrapped << trimmedBody;
        wrapped << QString();
    }

    wrapped << QStringLiteral("END_PROGRAM");
    return wrapped.join(QLatin1Char('\n'));
}
} // namespace

DSLCompilerInterface::DSLCompilerInterface(QObject* parent)
    : QObject(parent)
{
}

// third_party/custom_dsp_language/compile 目录（已替换为最新 lm-compiler）
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

// 新编译器入口：lmc.py
QString DSLCompilerInterface::compilerEntryScript() const
{
    return QDir(compilerWorkingDir()).absoluteFilePath(QStringLiteral("lmc.py"));
}

QString DSLCompilerInterface::resolvePythonInterpreter() const
{
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
            return cmd;
        }
        qWarning() << "[DSLCompilerInterface] Python candidate is not suitable for DSL compile:"
                   << cmd << "|" << probeDetails;
    }

    qWarning() << "[DSLCompilerInterface] No Python interpreter found."
               << "Please install Python 3, then install requirements.txt for the DSL compiler.";
    return QString();
}

static QString defaultOutputFileForSource(const QString& sourceFile, const QString& outputDir)
{
    const QFileInfo sourceInfo(sourceFile);
    QDir dir(outputDir);
    return dir.absoluteFilePath(sourceInfo.completeBaseName() + QStringLiteral(".code"));
}

static QString sha256ForFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}

CompileResult DSLCompilerInterface::buildCompileResult(const QString& sourceFile,
                                                       const QString& outputDir,
                                                       const QString& projectName,
                                                       bool success,
                                                       const QString& stdOut,
                                                       const QString& stdErr) const
{
    CompileResult result;
    result.success = success;
    result.projectName = projectName;
    result.stdOut = stdOut;
    result.stdErr = stdErr;

    if (!stdErr.trimmed().isEmpty() && !success) {
        result.errors = stdErr.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                     Qt::SkipEmptyParts);
    }

    const QString outputFile = defaultOutputFileForSource(sourceFile, outputDir);
    if (QFileInfo::exists(outputFile)) {
        CompileArtifact artifact;
        artifact.type = QStringLiteral("download");
        artifact.path = outputFile;
        artifact.format = QStringLiteral("dsl_custom");
        artifact.checksum = sha256ForFile(outputFile);
        artifact.metadata.insert(QStringLiteral("sourceFile"), sourceFile);
        artifact.metadata.insert(QStringLiteral("outputDir"), outputDir);
        result.artifacts.append(artifact);
    }

    return result;
}

QString DSLCompilerInterface::prepareCompilerInput(const QString& sourceFile,
                                                   const QString& outputDir,
                                                   QString* errorMessage) const
{
    const QFileInfo sourceInfo(sourceFile);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Source file not found: %1").arg(sourceFile);
        }
        return QString();
    }

    const QString suffix = sourceInfo.suffix().toLower();
    if (suffix == QStringLiteral("lm")) {
        return sourceInfo.absoluteFilePath();
    }

    QFile src(sourceInfo.absoluteFilePath());
    if (!src.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to read source file: %1").arg(sourceInfo.absoluteFilePath());
        }
        return QString();
    }

    const QString stagingDir = QDir(outputDir).absoluteFilePath(QStringLiteral(".compiler_staging"));
    QDir().mkpath(stagingDir);

    const QString stagedLmFile = QDir(stagingDir).absoluteFilePath(sourceInfo.completeBaseName() + QStringLiteral(".lm"));
    QFile staged(stagedLmFile);
    if (!staged.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create staged LM file: %1").arg(stagedLmFile);
        }
        return QString();
    }

    const QString sourceText = TextEncoding::decodeUtf8WithLocalFallback(src.readAll());
    const QString normalizedText = normalizeLegacyDslSource(sourceText, sourceInfo.completeBaseName());
    staged.write(normalizedText.toUtf8());
    staged.close();
    src.close();

    return stagedLmFile;
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

    const QString outputFile = defaultOutputFileForSource(sourceFile, outputDir);

    QStringList args;
    args << entryScript
         << QDir::toNativeSeparators(compilerInputFile)
         << QStringLiteral("-o")
         << QDir::toNativeSeparators(outputFile)
         << QStringLiteral("-v");

    QProcess proc;
    proc.setProgram(python);
    proc.setArguments(args);
    proc.setWorkingDirectory(workDir);

    qInfo() << "[DSLCompilerInterface] Running:" << python << args
            << "cwd =" << workDir;

    proc.start();
    if (!proc.waitForFinished(120 * 1000)) {
        proc.kill();
        if (compilerStderr)
            *compilerStderr = QStringLiteral("DSL compile process timeout.");
        qWarning() << "[DSLCompilerInterface] compileDslFile timeout.";
        return false;
    }

    const QString stdOut =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardOutput());
    const QString stdErr =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardError());

    if (compilerStdout)
        *compilerStdout = stdOut;
    if (compilerStderr)
        *compilerStderr = stdErr;

    const bool ok = proc.exitStatus() == QProcess::NormalExit
                 && proc.exitCode() == 0
                 && QFileInfo::exists(outputFile);

    m_lastCompileResult = buildCompileResult(sourceFile, outputDir, projectName, ok, stdOut, stdErr);

    if (!ok) {
        qWarning() << "[DSLCompilerInterface] compileDslFile failed. exitCode="
                   << proc.exitCode() << "outputFile=" << outputFile;
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
    m_lastCompileResult = buildCompileResult(sourceFile, outputDir, projectName, ok, stdOut, stdErr);
    return m_lastCompileResult;
}

bool DSLCompilerInterface::listComponents(QString* compilerStdout,
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

    QStringList args;
    args << QStringLiteral("-c")
         << QStringLiteral(
               "import json, sys; "
               "from pathlib import Path; "
               "sys.path.insert(0, str(Path('.').resolve() / 'src')); "
               "from lm_compiler.function_blocks.registry import FunctionBlockRegistry; "
               "registry = FunctionBlockRegistry(); registry.load_defaults(); "
               "names = sorted(list(registry.blocks.keys())); "
               "print(json.dumps({'components': names}, ensure_ascii=False))");

    QProcess proc;
    proc.setProgram(python);
    proc.setArguments(args);
    proc.setWorkingDirectory(workDir);

    qInfo() << "[DSLCompilerInterface] Running:" << python << args
            << "cwd =" << workDir;

    proc.start();
    if (!proc.waitForFinished(30 * 1000)) {
        proc.kill();
        if (compilerStderr)
            *compilerStderr = QStringLiteral("list-components timeout.");
        qWarning() << "[DSLCompilerInterface] listComponents timeout.";
        return false;
    }

    const QString stdOut =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardOutput());
    const QString stdErr =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardError());

    if (compilerStdout)
        *compilerStdout = stdOut;
    if (compilerStderr)
        *compilerStderr = stdErr;

    const bool ok = proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;

    if (!ok) {
        qWarning() << "[DSLCompilerInterface] listComponents failed. exitCode="
                   << proc.exitCode();
    }

    return ok;
}

void DSLCompilerInterface::compileDslFileAsync(const QString& sourceFile,
                                               const QString& outputDir,
                                               const QString& projectName)
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        const QString msg =
                QStringLiteral("DSL compile request ignored: previous compile is still running.");
        LOG_WARN(msg);
        emit compileFailedToStart(msg);
        return;
    }

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
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

    const QString outputFile = defaultOutputFileForSource(sourceFile, outputDir);

    QStringList args;
    args << entryScript
         << QDir::toNativeSeparators(compilerInputFile)
         << QStringLiteral("-o")
         << QDir::toNativeSeparators(outputFile)
         << QStringLiteral("-v");

    m_process = new QProcess(this);
    m_asyncStdOut.clear();
    m_asyncStdErr.clear();

    m_process->setProgram(python);
    m_process->setArguments(args);
    m_process->setWorkingDirectory(workDir);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->setProperty("sourceFile", sourceFile);
    m_process->setProperty("outputDir", outputDir);
    m_process->setProperty("projectName", projectName);
    m_process->setProperty("expectedOutputFile", outputFile);

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

    if (!m_compileTimeoutTimer) {
        m_compileTimeoutTimer = new QTimer(this);
        m_compileTimeoutTimer->setSingleShot(true);
        connect(m_compileTimeoutTimer, &QTimer::timeout,
                this, &DSLCompilerInterface::onCompileTimeout);
    }

    LOG_INFO(QStringLiteral("[DSLCompilerInterface] Running (async): %1 %2, cwd = %3")
             .arg(python,
                  args.join(QLatin1Char(' ')),
                  workDir));

    m_process->setProperty("expectedOutputFile", outputFile);
    m_process->setProperty("compileTimedOut", false);
    m_process->start();
    m_compileTimeoutTimer->start(120 * 1000);
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
    const bool outputExists = expectedOutput.isEmpty() || QFileInfo::exists(expectedOutput);

    if (m_process->property("compileTimedOut").toBool()) {
        m_asyncStdErr.append(QStringLiteral("DSL compile process timeout.\n"));
    }

    if (normalExit && exitCode == 0 && outputExists) {
        LOG_INFO("[DSLCompilerInterface] async compile succeeded.");
    } else {
        if (!outputExists) {
            m_asyncStdErr.append(QStringLiteral("Expected output file was not generated: %1\n").arg(expectedOutput));
        }
        LOG_WARN(QStringLiteral("[DSLCompilerInterface] async compile failed. exitCode=%1, normalExit=%2")
                 .arg(exitCode)
                 .arg(normalExit));
    }

    const bool success = (exitCode == 0 && normalExit && outputExists);
    m_lastCompileResult = buildCompileResult(m_process->property("sourceFile").toString(),
                                             m_process->property("outputDir").toString(),
                                             m_process->property("projectName").toString(),
                                             success,
                                             m_asyncStdOut,
                                             m_asyncStdErr);

    emit compileFinished(exitCode, normalExit && outputExists, m_asyncStdOut, m_asyncStdErr);

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
