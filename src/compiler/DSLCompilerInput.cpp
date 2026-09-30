#include "DSLCompilerInterface.h"
#include "DSLCompilerInternal.h"
#include "TextEncoding.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QCryptographicHash>

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

struct MappedLine {
    QString text;
    QString filePath;
    int sourceLine = 0;
    bool exactColumn = false;
};
using MappedLines = QList<MappedLine>;

MappedLine synthetic(const QString& text) { return {text, QString(), 0, false}; }
QString joined(const MappedLines& lines)
{
    QStringList text;
    for (const auto& line : lines) text.append(line.text);
    return text.join(QLatin1Char('\n'));
}
MappedLines sourceLines(const QString& text, const QString& path)
{
    MappedLines result;
    const QStringList lines = text.split(QLatin1Char('\n'));
    const QString canonical = QFileInfo(path).canonicalFilePath();
    for (int i = 0; i < lines.size(); ++i) result.append({lines.at(i), canonical, i + 1, true});
    return result;
}
void trimBlankLines(MappedLines& lines)
{
    while (!lines.isEmpty() && lines.first().text.trimmed().isEmpty()) lines.removeFirst();
    while (!lines.isEmpty() && lines.last().text.trimmed().isEmpty()) lines.removeLast();
}
void insertLines(MappedLines& target, int at, const MappedLines& lines)
{
    for (int i = lines.size() - 1; i >= 0; --i) target.insert(at, lines.at(i));
}

MappedLines normalizeLegacyDslSource(MappedLines lines, const QString& baseName)
{
    const QRegularExpression callRe(QStringLiteral(R"(^(\s*)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([_A-Za-z][_A-Za-z0-9_]*)\s*\()"));
    const QRegularExpression paramRe(QStringLiteral(R"(^(\s*[A-Za-z_][A-Za-z0-9_]*\s*)=(\s*.+)$)"));
    QStringList instanceOrder;
    QHash<QString, QString> instanceTypes;
    int depth = 0;
    for (auto& line : lines) {
        const QString original = line.text;
        const auto call = callRe.match(original);
        if (call.hasMatch()) {
            const QString instance = call.captured(2);
            if (!instanceTypes.contains(instance)) {
                instanceOrder.append(instance);
                instanceTypes.insert(instance, normalizeFunctionBlockTypeName(call.captured(3)));
            }
            // Preserve same-line arguments and the closing parenthesis.
            line.text = call.captured(1) + instance + QStringLiteral("(") + original.mid(call.capturedEnd());
        } else if (depth > 0 && !original.trimmed().startsWith(QStringLiteral("//"))
                   && !original.contains(QStringLiteral(":=")) && !original.contains(QStringLiteral("=>"))) {
            const auto parameter = paramRe.match(original);
            if (parameter.hasMatch()) line.text = parameter.captured(1) + QStringLiteral(":=") + parameter.captured(2);
        }
        if (line.text != original) line.exactColumn = false;
        depth = qMax(0, depth + line.text.count(QLatin1Char('(')) - line.text.count(QLatin1Char(')')));
    }
    const bool enveloped = hasProgramEnvelope(joined(lines));
    int program = -1, var = -1, endVar = -1;
    QSet<QString> declared;
    const QRegularExpression declaration(QStringLiteral(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*:(?!=)\s*[A-Za-z_])"));
    for (int i = 0; i < lines.size(); ++i) {
        const QString text = lines.at(i).text.trimmed();
        if (program < 0 && text.startsWith(QStringLiteral("PROGRAM "), Qt::CaseInsensitive)) program = i;
        if (var < 0 && text.compare(QStringLiteral("VAR"), Qt::CaseInsensitive) == 0) var = i;
        else if (var >= 0 && endVar < 0 && text.compare(QStringLiteral("END_VAR"), Qt::CaseInsensitive) == 0) endVar = i;
        const auto match = declaration.match(lines.at(i).text);
        if (match.hasMatch()) declared.insert(match.captured(1));
    }
    MappedLines missing;
    for (const QString& name : instanceOrder)
        if (!declared.contains(name)) missing.append(synthetic(QStringLiteral("    %1 : %2;").arg(name, instanceTypes.value(name))));
    if (enveloped) {
        if (!missing.isEmpty()) {
            if (var >= 0 && endVar > var) insertLines(lines, endVar, missing);
            else if (program >= 0) {
                missing.prepend(synthetic(QStringLiteral("VAR")));
                missing.append(synthetic(QStringLiteral("END_VAR")));
                missing.append(synthetic(QString()));
                insertLines(lines, program + 1, missing);
            }
        }
        return lines;
    }
    trimBlankLines(lines);
    MappedLines wrapped;
    wrapped.append(synthetic(QStringLiteral("PROGRAM ") + sanitizeProgramName(baseName)));
    MappedLines variables = missing;
    // Existing declarations in an envelope-free fragment stay in the VAR section.
    for (auto it = lines.begin(); it != lines.end();) {
        if (declaration.match(it->text).hasMatch()) { variables.append(*it); it = lines.erase(it); }
        else ++it;
    }
    if (!variables.isEmpty()) {
        wrapped.append(synthetic(QStringLiteral("VAR")));
        wrapped.append(variables);
        wrapped.append(synthetic(QStringLiteral("END_VAR")));
    }
    wrapped.append(synthetic(QString()));
    wrapped.append(lines);
    wrapped.append(synthetic(QString()));
    wrapped.append(synthetic(QStringLiteral("END_PROGRAM")));
    return wrapped;
}

MappedLines extractProgramStatements(const MappedLines& lines, MappedLines* declarations, QString* error)
{
    MappedLines statements;
    MappedLines variableSection;
    bool hasDeclaration = false;
    bool started = false, ended = false, inVars = false;
    for (const auto& line : lines) {
        const QString text = line.text.trimmed();
        if (!started && text.startsWith(QStringLiteral("PROGRAM "), Qt::CaseInsensitive)) { started = true; continue; }
        if (!started) continue;
        if (text.compare(QStringLiteral("END_PROGRAM"), Qt::CaseInsensitive) == 0) { ended = true; break; }
        if (text.compare(QStringLiteral("VAR"), Qt::CaseInsensitive) == 0
                || text.compare(QStringLiteral("VAR_CONSTANT"), Qt::CaseInsensitive) == 0) {
            variableSection.clear();
            variableSection.append(line);
            hasDeclaration = false;
            inVars = true; continue;
        }
        if (text.compare(QStringLiteral("END_VAR"), Qt::CaseInsensitive) == 0) {
            variableSection.append(line);
            if (hasDeclaration) declarations->append(variableSection);
            inVars = false; continue;
        }
        if (inVars) {
            variableSection.append(line);
            if (!text.isEmpty() && !text.startsWith(QStringLiteral("//"))) hasDeclaration = true;
        }
        else statements.append(line);
    }
    if (!started || !ended) {
        if (error) *error = QStringLiteral("Program envelope is incomplete");
        return {};
    }
    trimBlankLines(statements);
    return statements;
}

bool writeMappedSource(const QString& path, const MappedLines& lines, QString* error)
{
    const QString text = joined(lines);
    if (!DSLCompilerInternal::writeTextFile(path, text, error)) return false;
    QJsonObject locations;
    for (int i = 0; i < lines.size(); ++i) {
        const auto& line = lines.at(i);
        if (line.sourceLine <= 0 || line.filePath.isEmpty()) continue;
        locations.insert(QString::number(i + 1), QJsonObject{{"filePath", line.filePath},
                         {"line", line.sourceLine}, {"exactColumn", line.exactColumn}});
    }
    QJsonObject map{{"version", 1}, {"locations", locations},
                    {"normalizedSha256", QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex())}};
    QSaveFile output(path + QStringLiteral(".source-map.json"));
    const QByteArray bytes = QJsonDocument(map).toJson();
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) {
        if (error) *error = QStringLiteral("Failed to commit compiler source map: %1").arg(path);
        return false;
    }
    return true;
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

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write staging file: %1").arg(filePath);
        }
        return false;
    }
    const QByteArray bytes = text.toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        if (errorMessage) *errorMessage = QStringLiteral("Failed to commit staging file: %1").arg(filePath);
        return false;
    }
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

    MappedLines mainLines, childLines, declarations;
    const QString normalizedMain = QFileInfo(mainScriptFile).absoluteFilePath();
    bool mainFound = false;
    for (const QString& script : scriptFiles) {
        const QFileInfo info(script);
        if (!info.isFile() || info.suffix().compare(QStringLiteral("lh"), Qt::CaseInsensitive) != 0) {
            if (errorMessage) *errorMessage = QStringLiteral("Project script missing or invalid: %1").arg(script);
            return {};
        }
        QString error;
        const QString text = readTextFile(info.absoluteFilePath(), &error);
        if (!error.isEmpty()) { if (errorMessage) *errorMessage = error; return {}; }
        auto lines = normalizeLegacyDslSource(sourceLines(text, info.absoluteFilePath()), info.completeBaseName());
        if (info.absoluteFilePath() == normalizedMain) { mainLines = lines; mainFound = true; }
        else {
            auto statements = extractProgramStatements(lines, &declarations, &error);
            if (!error.isEmpty()) { if (errorMessage) *errorMessage = error + QStringLiteral(": ") + script; return {}; }
            childLines.append(synthetic(QStringLiteral("// BEGIN %1").arg(QDir(projectPath).relativeFilePath(script))));
            childLines.append(statements);
            childLines.append(synthetic(QStringLiteral("// END %1").arg(QDir(projectPath).relativeFilePath(script))));
            childLines.append(synthetic(QString()));
        }
    }
    if (!mainFound) { if (errorMessage) *errorMessage = QStringLiteral("Configured main script is not in the script list"); return {}; }
    int endProgram = -1, program = -1;
    for (int i = 0; i < mainLines.size(); ++i) {
        const QString text = mainLines.at(i).text.trimmed();
        if (program < 0 && text.startsWith(QStringLiteral("PROGRAM "), Qt::CaseInsensitive)) program = i;
        if (text.compare(QStringLiteral("END_PROGRAM"), Qt::CaseInsensitive) == 0) endProgram = i;
    }
    if (endProgram < 0) { if (errorMessage) *errorMessage = QStringLiteral("Main script missing END_PROGRAM"); return {}; }
    if (!declarations.isEmpty()) {
        insertLines(mainLines, program + 1, declarations);
        endProgram += declarations.size();
    }
    insertLines(mainLines, endProgram, childLines);
    const QString assembledPath = QDir(compilerStagingDir(outputDir)).absoluteFilePath(
        QFileInfo(mainScriptFile).completeBaseName() + QStringLiteral("_assembled.lh"));
    if (!writeMappedSource(assembledPath, mainLines, errorMessage)) return {};
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

    const MappedLines stagedLines = normalizeLegacyDslSource(sourceLines(sourceText, sourceInfo.absoluteFilePath()), sourceInfo.completeBaseName());
    const QString stagedPath = QDir(DSLCompilerInternal::compilerStagingDir(outputDir)).absoluteFilePath(
        sourceInfo.completeBaseName() + QStringLiteral(".lh"));

    QString writeError;
    if (!writeMappedSource(stagedPath, stagedLines, &writeError)) {
        if (errorMessage) {
            *errorMessage = writeError;
        }
        return QString();
    }

    return stagedPath;
}
