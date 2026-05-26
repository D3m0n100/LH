#include "DSLCompilerInterface.h"
#include "TextEncoding.h"

#include <QCoreApplication>

// 静态成员定义：Python 解释器路径缓存
QString DSLCompilerInterface::s_cachedPythonInterpreter;
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>
#include <QStringList>
#include <QRegularExpression>
#include <QHash>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

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

QString injectMissingInstanceDeclarations(const QString& sourceText,
                                          const QStringList& instanceOrder,
                                          const QHash<QString, QString>& instanceTypeMap)
{
    if (instanceOrder.isEmpty()) {
        return sourceText;
    }

    QStringList lines = sourceText.split(QLatin1Char('\n'));
    QSet<QString> declaredInstances;
    const QRegularExpression declarationRe(
        QStringLiteral(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*:)"));

    int varLine = -1;
    int endVarLine = -1;
    int programLine = -1;

    for (int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines.at(i).trimmed();
        if (programLine < 0 && trimmed.startsWith(QStringLiteral("PROGRAM "), Qt::CaseInsensitive)) {
            programLine = i;
        }
        if (varLine < 0 && trimmed.compare(QStringLiteral("VAR"), Qt::CaseInsensitive) == 0) {
            varLine = i;
        } else if (varLine >= 0
                   && endVarLine < 0
                   && trimmed.compare(QStringLiteral("END_VAR"), Qt::CaseInsensitive) == 0) {
            endVarLine = i;
        }

        const QRegularExpressionMatch declarationMatch = declarationRe.match(lines.at(i));
        if (declarationMatch.hasMatch()) {
            declaredInstances.insert(declarationMatch.captured(1));
        }
    }

    QStringList missingDeclarations;
    for (const QString& instanceName : instanceOrder) {
        if (!declaredInstances.contains(instanceName)) {
            missingDeclarations << QStringLiteral("    %1 : %2;")
                                       .arg(instanceName, instanceTypeMap.value(instanceName));
        }
    }
    if (missingDeclarations.isEmpty()) {
        return sourceText;
    }

    if (varLine >= 0 && endVarLine > varLine) {
        for (int i = missingDeclarations.size() - 1; i >= 0; --i) {
            lines.insert(endVarLine, missingDeclarations.at(i));
        }
        return lines.join(QLatin1Char('\n'));
    }

    if (programLine >= 0) {
        QStringList varBlock;
        varBlock << QStringLiteral("VAR");
        varBlock << missingDeclarations;
        varBlock << QStringLiteral("END_VAR");
        varBlock << QString();
        for (int i = varBlock.size() - 1; i >= 0; --i) {
            lines.insert(programLine + 1, varBlock.at(i));
        }
    }

    return lines.join(QLatin1Char('\n'));
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
        return injectMissingInstanceDeclarations(transformedBody, instanceOrder, instanceTypeMap);
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

QString extractProgramStatements(const QString& sourceText,
                                 const QString& sourcePath,
                                 QStringList* varDeclarations,
                                 QString* errorMessage)
{
    if (!hasProgramEnvelope(sourceText)) {
        return sourceText.trimmed();
    }

    const QString normalized = normalizeLegacyDslSource(sourceText, QFileInfo(sourcePath).completeBaseName());
    const QStringList lines = normalized.split(QLatin1Char('\n'));
    int programLine = -1;
    int endProgramLine = -1;
    bool inVarBlock = false;
    QStringList statements;

    for (int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines.at(i).trimmed();
        if (programLine < 0 && trimmed.startsWith(QStringLiteral("PROGRAM "), Qt::CaseInsensitive)) {
            programLine = i;
            continue;
        }
        if (trimmed.compare(QStringLiteral("END_PROGRAM"), Qt::CaseInsensitive) == 0) {
            endProgramLine = i;
            break;
        }
        if (programLine < 0) {
            continue;
        }

        if (trimmed.compare(QStringLiteral("VAR"), Qt::CaseInsensitive) == 0) {
            inVarBlock = true;
            continue;
        }
        if (trimmed.compare(QStringLiteral("END_VAR"), Qt::CaseInsensitive) == 0) {
            inVarBlock = false;
            continue;
        }

        if (inVarBlock) {
            if (!trimmed.isEmpty()) {
                if (varDeclarations) {
                    varDeclarations->append(lines.at(i));
                }
            }
            continue;
        }

        statements << lines.at(i);
    }

    if (programLine < 0 || endProgramLine < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Program envelope is incomplete: %1").arg(sourcePath);
        }
        return QString();
    }

    return statements.join(QLatin1Char('\n')).trimmed();
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
    // 使用缓存的解释器路径（首次解析后缓存，避免每次编译都重新探测）
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

static QString readTextFile(const QString& filePath, QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to read source file: %1").arg(filePath);
        }
        return QString();
    }
    return TextEncoding::decodeUtf8WithLocalFallback(file.readAll());
}

static bool writeTextFile(const QString& filePath, const QString& text, QString* errorMessage)
{
    QFileInfo info(filePath);
    QDir dir;
    if (!dir.mkpath(info.absolutePath())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create staging directory: %1")
                                .arg(info.absolutePath());
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write staging file: %1").arg(filePath);
        }
        return false;
    }
    file.write(text.toUtf8());
    return true;
}

static QString compilerStagingDir(const QString& outputDir)
{
    return QDir(outputDir).absoluteFilePath(QStringLiteral(".compiler_staging"));
}

static QString runDslCompilerProcess(const QString& python,
                                     const QString& entryScript,
                                     const QString& compilerInputFile,
                                     const QString& outputFile,
                                     const QString& workDir,
                                     QString* compilerStdout,
                                     QString* compilerStderr)
{
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
        if (compilerStderr) {
            *compilerStderr = QStringLiteral("DSL compile process timeout.");
        }
        qWarning() << "[DSLCompilerInterface] compile process timeout.";
        return QStringLiteral("timeout");
    }

    const QString stdOut =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardOutput());
    const QString stdErr =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardError());

    if (compilerStdout) {
        *compilerStdout = stdOut;
    }
    if (compilerStderr) {
        *compilerStderr = stdErr;
    }

    if (proc.exitStatus() == QProcess::NormalExit
            && proc.exitCode() == 0
            && QFileInfo::exists(outputFile)) {
        return QString();
    }

    return QStringLiteral("exitCode=%1 outputFile=%2")
            .arg(proc.exitCode())
            .arg(outputFile);
}

static QString resolveScriptPath(const QString& projectPath, const QString& path)
{
    if (path.isEmpty()) {
        return QString();
    }
    if (QFileInfo(path).isAbsolute()) {
        return QFileInfo(path).absoluteFilePath();
    }
    return QDir(projectPath).absoluteFilePath(path);
}

static QString resolveProjectMainScriptPath(const QString& projectPath,
                                            const ProjectRuntimeConfig& config)
{
    const QString mainPath = resolveScriptPath(projectPath, config.mainScriptPath);
    if (!mainPath.isEmpty() && QFileInfo::exists(mainPath)) {
        return mainPath;
    }

    const QString legacyPath = resolveScriptPath(projectPath, config.dslScriptPath);
    if (!legacyPath.isEmpty() && QFileInfo::exists(legacyPath)) {
        return legacyPath;
    }

    const QString dslPath = QDir(projectPath).absoluteFilePath(QStringLiteral("main.dsl"));
    if (QFileInfo::exists(dslPath)) {
        return dslPath;
    }

    return QString();
}

static QStringList normalizeProjectScriptFiles(const QString& projectPath,
                                               const ProjectRuntimeConfig& config,
                                               const QString& mainScriptFile)
{
    QStringList normalized;
    for (const QString& script : config.scriptFiles) {
        const QString path = resolveScriptPath(projectPath, script);
        if (!path.isEmpty() && !normalized.contains(path)) {
            normalized.append(path);
        }
    }

    if (!mainScriptFile.isEmpty()) {
        normalized.removeAll(mainScriptFile);
        normalized.prepend(mainScriptFile);
    }

    return normalized;
}

static QString assembleProjectCompilerInput(const QString& projectPath,
                                            const QString& outputDir,
                                            const QString& mainScriptFile,
                                            const QStringList& scriptFiles,
                                            QString* errorMessage)
{
    if (scriptFiles.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Project script file list is empty.");
        }
        return QString();
    }

    QString mainSourceText;
    QStringList childFragments;
    QStringList childVarDeclarations;
    const QString normalizedMainPath = QFileInfo(mainScriptFile).absoluteFilePath();

    for (int i = 0; i < scriptFiles.size(); ++i) {
        const QString& scriptFile = scriptFiles.at(i);
        const QFileInfo info(scriptFile);
        if (!info.exists() || !info.isFile()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Project script file not found: %1").arg(scriptFile);
            }
            return QString();
        }
        if (info.suffix().toLower() != QStringLiteral("dsl")) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Unsupported project script suffix: %1")
                                    .arg(info.fileName());
            }
            return QString();
        }

        QString readError;
        const QString sourceText = readTextFile(info.absoluteFilePath(), &readError);
        if (!readError.isEmpty()) {
            if (errorMessage) {
                *errorMessage = readError;
            }
            return QString();
        }
        const bool isMainFile = info.absoluteFilePath() == normalizedMainPath || i == 0;

        const QString relativePath = QDir(projectPath).relativeFilePath(info.absoluteFilePath());
        QStringList decoratedFragment;
        decoratedFragment << QStringLiteral("// BEGIN %1").arg(relativePath);
        QString fragmentText = sourceText.trimmed();
        if (!isMainFile) {
            QString fragmentError;
            fragmentText = extractProgramStatements(sourceText,
                                                    info.absoluteFilePath(),
                                                    &childVarDeclarations,
                                                    &fragmentError);
            if (!fragmentError.isEmpty()) {
                if (errorMessage) {
                    *errorMessage = fragmentError;
                }
                return QString();
            }
        }
        decoratedFragment << fragmentText;
        decoratedFragment << QStringLiteral("// END %1").arg(relativePath);
        decoratedFragment << QString();

        if (isMainFile) {
            mainSourceText = decoratedFragment.join(QLatin1Char('\n'));
        } else {
            childFragments << decoratedFragment;
        }
    }

    const QFileInfo mainInfo(mainScriptFile);
    QString assembledSource = mainSourceText;
    const QString childSourceText = childFragments.join(QLatin1Char('\n'));
    if (!childSourceText.trimmed().isEmpty()) {
        if (hasProgramEnvelope(mainSourceText)) {
            QStringList mainLines = mainSourceText.split(QLatin1Char('\n'));
            int varLine = -1;
            int endVarLine = -1;
            int endProgramLine = -1;
            for (int i = 0; i < mainLines.size(); ++i) {
                const QString trimmed = mainLines.at(i).trimmed();
                if (varLine < 0 && trimmed.compare(QStringLiteral("VAR"), Qt::CaseInsensitive) == 0) {
                    varLine = i;
                } else if (varLine >= 0
                           && endVarLine < 0
                           && trimmed.compare(QStringLiteral("END_VAR"), Qt::CaseInsensitive) == 0) {
                    endVarLine = i;
                }
                if (trimmed.compare(
                        QStringLiteral("END_PROGRAM"), Qt::CaseInsensitive) == 0) {
                    endProgramLine = i;
                }
            }
            if (endProgramLine < 0) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Main script PROGRAM envelope is missing END_PROGRAM: %1")
                                        .arg(mainScriptFile);
                }
                return QString();
            }
            if (!childVarDeclarations.isEmpty()) {
                if (varLine >= 0 && endVarLine > varLine) {
                    for (int i = childVarDeclarations.size() - 1; i >= 0; --i) {
                        mainLines.insert(endVarLine, childVarDeclarations.at(i));
                    }
                    endProgramLine += childVarDeclarations.size();
                } else {
                    QStringList varBlock;
                    varBlock << QStringLiteral("VAR");
                    varBlock << childVarDeclarations;
                    varBlock << QStringLiteral("END_VAR");
                    varBlock << QString();
                    const int insertLine = qMax(0, endProgramLine);
                    for (int i = varBlock.size() - 1; i >= 0; --i) {
                        mainLines.insert(insertLine, varBlock.at(i));
                    }
                    endProgramLine += varBlock.size();
                }
            }
            mainLines.insert(endProgramLine, childSourceText.trimmed());
            mainLines.insert(endProgramLine + 1, QString());
            assembledSource = mainLines.join(QLatin1Char('\n'));
        } else {
            QStringList fragmentParts;
            fragmentParts << mainSourceText;
            if (!childVarDeclarations.isEmpty()) {
                fragmentParts << childVarDeclarations.join(QLatin1Char('\n'));
            }
            fragmentParts << childSourceText;
            assembledSource = fragmentParts.join(QLatin1Char('\n'));
        }
    }

    const QString assembledText = normalizeLegacyDslSource(assembledSource, mainInfo.completeBaseName());
    const QString assembledPath = QDir(compilerStagingDir(outputDir)).absoluteFilePath(
        mainInfo.completeBaseName() + QStringLiteral("_assembled.dsl"));

    QString writeError;
    if (!writeTextFile(assembledPath, assembledText, &writeError)) {
        if (errorMessage) {
            *errorMessage = writeError;
        }
        return QString();
    }
    return assembledPath;
}

CompileResult DSLCompilerInterface::buildCompileResult(const QString& sourceFile,
                                                       const QString& outputDir,
                                                       const QString& projectName,
                                                       const QString& mainScriptFile,
                                                       const QStringList& scriptFiles,
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
        artifact.metadata.insert(QStringLiteral("mainScriptFile"), mainScriptFile.isEmpty() ? sourceFile : mainScriptFile);
        artifact.metadata.insert(QStringLiteral("scriptFiles"), scriptFiles);
        artifact.metadata.insert(QStringLiteral("artifactScope"), QStringLiteral("project"));
        artifact.metadata.insert(QStringLiteral("outputDir"), outputDir);
        result.artifacts.append(artifact);
    }

    return result;
}

CompileResult DSLCompilerInterface::buildCompileResult(const QString& sourceFile,
                                                       const QString& outputDir,
                                                       const QString& projectName,
                                                       const QString& mainScriptFile,
                                                       const QStringList& scriptFiles,
                                                       bool success,
                                                       const QString& stdOut,
                                                       const QString& stdErr,
                                                       const ProjectRuntimeConfig& config) const
{
    CompileResult result = buildCompileResult(sourceFile, outputDir, projectName,
                                              mainScriptFile, scriptFiles,
                                              success, stdOut, stdErr);

    if (result.success) {
        CompileArtifact pointsArtifact = generateRuntimePointsJson(outputDir, config);
        if (!pointsArtifact.path.isEmpty())
            result.artifacts.append(pointsArtifact);

        const auto points = RuntimePointConverter::fromProjectConfig(config);
        int parameterCount = 0;
        for (const auto& pt : points) {
            if (pt.kind == RuntimePointKind::Parameter)
                parameterCount++;
        }

        CompileArtifact manifestArtifact = generateRuntimeManifestJson(
            outputDir, projectName, mainScriptFile, scriptFiles,
            points.size(), parameterCount);
        if (!manifestArtifact.path.isEmpty())
            result.artifacts.append(manifestArtifact);
    }

    return result;
}

CompileArtifact DSLCompilerInterface::generateRuntimePointsJson(
    const QString& outputDir, const ProjectRuntimeConfig& config) const
{
    const auto points = RuntimePointConverter::fromProjectConfig(config);

    QJsonArray arr;
    for (const auto& pt : points) {
        QJsonObject obj;
        obj[QStringLiteral("id")] = pt.id;
        obj[QStringLiteral("name")] = pt.name;

        QString kindStr;
        switch (pt.kind) {
        case RuntimePointKind::Variable:  kindStr = QStringLiteral("Variable");  break;
        case RuntimePointKind::Parameter: kindStr = QStringLiteral("Parameter"); break;
        case RuntimePointKind::Status:    kindStr = QStringLiteral("Status");    break;
        case RuntimePointKind::Alarm:     kindStr = QStringLiteral("Alarm");     break;
        case RuntimePointKind::Resource:  kindStr = QStringLiteral("Resource");  break;
        }
        obj[QStringLiteral("kind")] = kindStr;
        obj[QStringLiteral("dataType")] = pt.dataType;
        obj[QStringLiteral("unit")] = pt.unit;

        QString accessStr;
        switch (pt.access) {
        case RuntimePointAccess::ReadOnly:  accessStr = QStringLiteral("ReadOnly");  break;
        case RuntimePointAccess::WriteOnly: accessStr = QStringLiteral("WriteOnly"); break;
        case RuntimePointAccess::ReadWrite: accessStr = QStringLiteral("ReadWrite"); break;
        }
        obj[QStringLiteral("access")] = accessStr;
        obj[QStringLiteral("sourceModule")] = pt.sourceModule;
        obj[QStringLiteral("bindingPath")] = pt.bindingPath;
        if (!pt.addressing.isEmpty())
            obj[QStringLiteral("addressing")] = QJsonObject::fromVariantMap(pt.addressing);
        if (!pt.defaultValue.isNull())
            obj[QStringLiteral("defaultValue")] = QJsonValue::fromVariant(pt.defaultValue);
        if (!pt.metadata.isEmpty())
            obj[QStringLiteral("metadata")] = QJsonObject::fromVariantMap(pt.metadata);

        arr.append(obj);
    }

    const QString filePath = QDir(outputDir).absoluteFilePath(
        QStringLiteral("runtime_points.json"));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "[DSLCompilerInterface] Failed to write" << filePath;
        return {};
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    file.close();

    CompileArtifact artifact;
    artifact.type = QString::fromLatin1(CompileArtifactType::RuntimePoints);
    artifact.path = filePath;
    artifact.format = QStringLiteral("json");
    artifact.checksum = sha256ForFile(filePath);
    artifact.metadata.insert(QStringLiteral("pointCount"), arr.size());
    return artifact;
}

CompileArtifact DSLCompilerInterface::generateRuntimeManifestJson(
    const QString& outputDir,
    const QString& projectName,
    const QString& mainScriptFile,
    const QStringList& scriptFiles,
    int pointCount,
    int parameterCount) const
{
    QJsonObject manifest;
    manifest[QStringLiteral("projectName")] = projectName;
    manifest[QStringLiteral("mainScriptPath")] = mainScriptFile;

    QJsonArray scriptsArr;
    for (const auto& s : scriptFiles)
        scriptsArr.append(s);
    manifest[QStringLiteral("scriptFiles")] = scriptsArr;

    QJsonArray artifactPathsArr;
    const QString codeFile = defaultOutputFileForSource(mainScriptFile, outputDir);
    if (QFileInfo::exists(codeFile))
        artifactPathsArr.append(codeFile);
    const QString pointsPath = QDir(outputDir).absoluteFilePath(
        QStringLiteral("runtime_points.json"));
    if (QFileInfo::exists(pointsPath))
        artifactPathsArr.append(pointsPath);
    manifest[QStringLiteral("artifactPaths")] = artifactPathsArr;

    manifest[QStringLiteral("generatedAt")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    manifest[QStringLiteral("pointCount")] = pointCount;
    manifest[QStringLiteral("parameterCount")] = parameterCount;

    const QString filePath = QDir(outputDir).absoluteFilePath(
        QStringLiteral("runtime_manifest.json"));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "[DSLCompilerInterface] Failed to write" << filePath;
        return {};
    }
    file.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    file.close();

    CompileArtifact artifact;
    artifact.type = QString::fromLatin1(CompileArtifactType::RuntimeManifest);
    artifact.path = filePath;
    artifact.format = QStringLiteral("json");
    artifact.checksum = sha256ForFile(filePath);
    return artifact;
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
    if (suffix != QStringLiteral("dsl")) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unsupported source file suffix: %1").arg(sourceInfo.suffix());
        }
        return QString();
    }

    QString readError;
    const QString sourceText = readTextFile(sourceInfo.absoluteFilePath(), &readError);
    if (!readError.isEmpty()) {
        if (errorMessage) {
            *errorMessage = readError;
        }
        return QString();
    }

    const QString stagedText = normalizeLegacyDslSource(sourceText, sourceInfo.completeBaseName());
    const QString stagedPath = QDir(compilerStagingDir(outputDir)).absoluteFilePath(
        sourceInfo.completeBaseName() + QStringLiteral(".dsl"));

    QString writeError;
    if (!writeTextFile(stagedPath, stagedText, &writeError)) {
        if (errorMessage) {
            *errorMessage = writeError;
        }
        return QString();
    }

    return stagedPath;
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

    QString stdOut;
    QString stdErr;
    const QString compileError = runDslCompilerProcess(python,
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

    const bool ok = compileError.isEmpty();

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
    const QString mainScriptFile = resolveProjectMainScriptPath(projectPath, config);
    if (mainScriptFile.isEmpty()) {
        CompileResult result;
        result.success = false;
        result.projectName = projectName;
        result.errors.append(QStringLiteral("Main script not found for project compile."));
        return result;
    }

    const QStringList scriptFiles = normalizeProjectScriptFiles(projectPath, config, mainScriptFile);
    QString assemblyError;
    const QString compilerInputFile = assembleProjectCompilerInput(projectPath,
                                                                   outputDir,
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
    const QString outputFile = defaultOutputFileForSource(mainScriptFile, outputDir);
    const QString compileError = runDslCompilerProcess(python,
                                                       entryScript,
                                                       compilerInputFile,
                                                       outputFile,
                                                       workDir,
                                                       &stdOut,
                                                       &stdErr);
    const bool ok = compileError.isEmpty();
    m_lastCompileResult = buildCompileResult(mainScriptFile,
                                             outputDir,
                                             projectName,
                                             mainScriptFile,
                                             scriptFiles,
                                             ok,
                                             stdOut,
                                             stdErr,
                                             config);
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

QString DSLCompilerInterface::describeComponents(QString* compilerStderr)
{
    const QString python = resolvePythonInterpreter();
    if (python.isEmpty()) {
        if (compilerStderr)
            *compilerStderr = QStringLiteral(
                    "No suitable Python interpreter found for DSL compile. "
                    "Recreate third_party/custom_dsp_language/compile/venv "
                    "and install requirements.txt, or install Python 3 plus antlr4-python3-runtime in PATH.");
        return QString();
    }

    const QString workDir = compilerWorkingDir();
    const QString entryScript = compilerEntryScript();
    if (!QFileInfo::exists(entryScript)) {
        const QString msg =
                QStringLiteral("Compiler entry script not found: %1").arg(entryScript);
        qWarning() << "[DSLCompilerInterface]" << msg;
        if (compilerStderr)
            *compilerStderr = msg;
        return QString();
    }

    QStringList args;
    args << entryScript << QStringLiteral("--describe-blocks");

    QProcess proc;
    proc.setProgram(python);
    proc.setArguments(args);
    proc.setWorkingDirectory(workDir);

    qInfo() << "[DSLCompilerInterface] describeComponents:" << python << args
            << "cwd =" << workDir;

    proc.start();
    if (!proc.waitForFinished(30 * 1000)) {
        proc.kill();
        if (compilerStderr)
            *compilerStderr = QStringLiteral("describe-blocks timeout.");
        qWarning() << "[DSLCompilerInterface] describeComponents timeout.";
        return QString();
    }

    const QString stdOut =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardOutput());
    const QString stdErr =
            TextEncoding::decodeUtf8WithLocalFallback(proc.readAllStandardError());

    if (compilerStderr)
        *compilerStderr = stdErr;

    const bool ok = proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;

    if (!ok) {
        qWarning() << "[DSLCompilerInterface] describeComponents failed. exitCode="
                   << proc.exitCode();
        return QString();
    }

    return stdOut.trimmed();
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
    m_process->setProperty("mainScriptFile", sourceFile);
    m_process->setProperty("scriptFiles", QStringList{sourceFile});
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

void DSLCompilerInterface::compileProjectAsync(const QString& projectPath,
                                               const ProjectRuntimeConfig& config,
                                               const QString& outputDir,
                                               const QString& projectName)
{
    const QString mainScriptFile = resolveProjectMainScriptPath(projectPath, config);
    if (mainScriptFile.isEmpty()) {
        emit compileFailedToStart(QStringLiteral("Main script not found for project compile."));
        return;
    }

    const QStringList scriptFiles = normalizeProjectScriptFiles(projectPath, config, mainScriptFile);

    QString assemblyError;
    const QString compilerInputFile = assembleProjectCompilerInput(projectPath,
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

    const QString outputFile = defaultOutputFileForSource(mainScriptFile, outputDir);

    QStringList args;
    args << entryScript
         << QDir::toNativeSeparators(compilerInputFile)
         << QStringLiteral("-o")
         << QDir::toNativeSeparators(outputFile)
         << QStringLiteral("-v");

    m_process = new QProcess(this);
    m_asyncStdOut.clear();
    m_asyncStdErr.clear();
    m_asyncProjectConfig = config;

    m_process->setProgram(python);
    m_process->setArguments(args);
    m_process->setWorkingDirectory(workDir);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->setProperty("sourceFile", mainScriptFile);
    m_process->setProperty("mainScriptFile", mainScriptFile);
    m_process->setProperty("scriptFiles", scriptFiles);
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

    LOG_INFO(QStringLiteral("[DSLCompilerInterface] Running project compile (async): %1 %2, cwd = %3")
             .arg(python,
                  args.join(QLatin1Char(' ')),
                  workDir));

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
