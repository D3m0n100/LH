#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>

#include "compiler/DSLCompilerInterface.h"

namespace {
bool writeTextFile(const QString& filePath, const QString& text)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qCritical() << "Failed to write file:" << filePath;
        return false;
    }
    QTextStream out(&file);
    out << text;
    return true;
}

QString readTextFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString runtimeRoot =
        QDir::cleanPath(QDir::currentPath() + QStringLiteral("/build/legacy_probe_runtime"));
    QDir().mkpath(runtimeRoot);

    const QString sourcePath = QDir(runtimeRoot).filePath(QStringLiteral("main.dsl"));
    const QString outputDir = QDir(runtimeRoot).filePath(QStringLiteral("build_output/parameters"));
    QDir().mkpath(outputDir);

    const QString legacyDsl = QStringLiteral(
        "drv_ao_1 = _DrvAO(\n"
        "    channel = 0,\n"
        "    value = 0\n"
        ");\n"
        "drv_do_1 = _DrvDO(\n"
        "    OutputWord = 0,\n"
        "    Port = 0,\n"
        "    Mask = 255,\n"
        "    Action = 1\n"
        ");\n");

    if (!writeTextFile(sourcePath, legacyDsl)) {
        return 1;
    }

    DSLCompilerInterface compiler;
    const CompileResult result = compiler.compileDslFileWithResult(sourcePath, outputDir, QStringLiteral("legacy_probe"));
    const bool ok = result.success;

    if (!ok) {
        const QString combinedError = result.stdErr + QLatin1Char('\n') + result.stdOut;
        if (combinedError.contains(QStringLiteral("No suitable Python interpreter found"))
                || combinedError.contains(QStringLiteral("Unable to create process"))
                || combinedError.contains(QStringLiteral("Failed to start"))
                || combinedError.contains(QStringLiteral("Missing required module 'antlr4-python3-runtime'"))) {
            qWarning() << "Legacy DSL compile probe skipped: compiler runtime is unavailable.";
            qWarning() << "stderr:" << result.stdErr;
            qWarning() << "stdout:" << result.stdOut;
            return 0;
        }
        qCritical() << "compileDslFile failed";
        qCritical() << "stdout:" << result.stdOut;
        qCritical() << "stderr:" << result.stdErr;
        return 2;
    }

    const QString outputCodePath = QDir(outputDir).filePath(QStringLiteral("main.code"));
    if (!QFileInfo::exists(outputCodePath)) {
        qCritical() << "Output .code file not found:" << outputCodePath;
        return 3;
    }

    bool hasDownloadArtifact = false;
    for (const CompileArtifact& artifact : result.artifacts) {
        if (artifact.type == QStringLiteral("download")
                && artifact.format == QStringLiteral("dsl_custom")
                && QFileInfo::exists(artifact.path)
                && !artifact.checksum.isEmpty()) {
            hasDownloadArtifact = true;
            break;
        }
    }
    if (!hasDownloadArtifact) {
        qCritical() << "CompileResult did not report a dsl_custom download artifact.";
        return 6;
    }

    const QString stagedLmPath = QDir(outputDir).filePath(QStringLiteral(".compiler_staging/main.lm"));
    const QString stagedText = readTextFile(stagedLmPath);
    if (stagedText.isEmpty()) {
        qCritical() << "Staged LM file missing or empty:" << stagedLmPath;
        return 4;
    }

    const bool wrapped = stagedText.contains(QStringLiteral("PROGRAM main"))
                      && stagedText.contains(QStringLiteral("END_PROGRAM"));
    const bool declared = stagedText.contains(QStringLiteral("drv_ao_1 : _DrvAO;"))
                       && stagedText.contains(QStringLiteral("drv_do_1 : _DrvDO;"));
    const bool callConverted = stagedText.contains(QStringLiteral("drv_ao_1("))
                            && stagedText.contains(QStringLiteral("drv_do_1("))
                            && !stagedText.contains(QStringLiteral("= _DrvAO("))
                            && !stagedText.contains(QStringLiteral("= _DrvDO("));
    const bool assignConverted = stagedText.contains(QStringLiteral("channel := 0"))
                              && stagedText.contains(QStringLiteral("value := 0"))
                              && stagedText.contains(QStringLiteral("Mask := 255"))
                              && stagedText.contains(QStringLiteral("Action := 1"));

    if (!wrapped || !declared || !callConverted || !assignConverted) {
        qCritical() << "Legacy DSL normalization assertions failed.";
        qCritical() << "staged file:" << stagedLmPath;
        qCritical() << stagedText;
        return 5;
    }

    qInfo() << "Legacy DSL compile probe passed.";
    qInfo() << "Output file:" << outputCodePath;
    qInfo() << "Staged file:" << stagedLmPath;
    return 0;
}
