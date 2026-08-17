#include "DSLCompilerInterface.h"
#include "DSLCompilerInternal.h"
#include "TextEncoding.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>

namespace {

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

QString normalizeFunctionBlockTypeName(QString blockType)
{
    while (blockType.startsWith(QLatin1Char('_'))) {
        blockType.remove(0, 1);
    }
    return blockType;
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
            const QString blockType = normalizeFunctionBlockTypeName(callMatch.captured(3));

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

namespace DSLCompilerInternal {

QString readTextFile(const QString& filePath, QString* errorMessage)
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

bool writeTextFile(const QString& filePath, const QString& text, QString* errorMessage)
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

QString compilerStagingDir(const QString& outputDir)
{
    return QDir(outputDir).absoluteFilePath(QStringLiteral(".compiler_staging"));
}

QString resolveScriptPath(const QString& projectPath, const QString& path)
{
    if (path.isEmpty()) {
        return QString();
    }
    if (QFileInfo(path).isAbsolute()) {
        return QFileInfo(path).absoluteFilePath();
    }
    return QDir(projectPath).absoluteFilePath(path);
}

QString resolveProjectMainScriptPath(const QString& projectPath,
                                     const ProjectRuntimeConfig& config)
{
    const QString mainPath = resolveScriptPath(projectPath, config.mainScriptPath);
    if (!mainPath.isEmpty()
            && QFileInfo::exists(mainPath)
            && QFileInfo(mainPath).suffix().compare(QStringLiteral("lh"), Qt::CaseInsensitive) == 0) {
        return mainPath;
    }

    const QString lhPath = QDir(projectPath).absoluteFilePath(QStringLiteral("main.lh"));
    if (QFileInfo::exists(lhPath)) {
        return lhPath;
    }

    return QString();
}

QStringList normalizeProjectScriptFiles(const QString& projectPath,
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

QString assembleProjectCompilerInput(const QString& projectPath,
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
        if (info.suffix().toLower() != QStringLiteral("lh")) {
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
        mainInfo.completeBaseName() + QStringLiteral("_assembled.lh"));

    QString writeError;
    if (!writeTextFile(assembledPath, assembledText, &writeError)) {
        if (errorMessage) {
            *errorMessage = writeError;
        }
        return QString();
    }
    return assembledPath;
}

} // namespace DSLCompilerInternal

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
    if (suffix != QStringLiteral("lh")) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unsupported source file suffix: %1").arg(sourceInfo.suffix());
        }
        return QString();
    }

    QString readError;
    const QString sourceText = DSLCompilerInternal::readTextFile(sourceInfo.absoluteFilePath(), &readError);
    if (!readError.isEmpty()) {
        if (errorMessage) {
            *errorMessage = readError;
        }
        return QString();
    }

    const QString stagedText = normalizeLegacyDslSource(sourceText, sourceInfo.completeBaseName());
    const QString stagedPath = QDir(DSLCompilerInternal::compilerStagingDir(outputDir)).absoluteFilePath(
        sourceInfo.completeBaseName() + QStringLiteral(".lh"));

    QString writeError;
    if (!DSLCompilerInternal::writeTextFile(stagedPath, stagedText, &writeError)) {
        if (errorMessage) {
            *errorMessage = writeError;
        }
        return QString();
    }

    return stagedPath;
}
