#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>
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

    ProjectRuntimeConfig missingChildConfig;
    missingChildConfig.mainScriptPath = sourcePath;
    missingChildConfig.dslScriptPath = sourcePath;
    missingChildConfig.scriptFiles = QStringList{
        sourcePath,
        QStringLiteral("missing_child.dsl")
    };
    const CompileResult missingChildResult = compiler.compileProjectWithResult(
        runtimeRoot,
        missingChildConfig,
        outputDir,
        QStringLiteral("missing_child_probe"));
    if (missingChildResult.success
            || !missingChildResult.errors.join(QLatin1Char('\n')).contains(
                QStringLiteral("Project script file not found"))) {
        qCritical() << "Missing project child script was not rejected before compile.";
        qCritical() << "errors:" << missingChildResult.errors;
        qCritical() << "stderr:" << missingChildResult.stdErr;
        return 12;
    }

    const QString projectRoot = QDir(runtimeRoot).filePath(QStringLiteral("project"));
    const QString projectOutputDir = QDir(runtimeRoot).filePath(QStringLiteral("build_output/project"));
    QDir().mkpath(projectRoot);
    QDir().mkpath(projectOutputDir);

    const QString projectMainPath = QDir(projectRoot).filePath(QStringLiteral("main.dsl"));
    const QString projectSubPath = QDir(projectRoot).filePath(QStringLiteral("valve_auto.dsl"));
    const QString projectMainDsl = QStringLiteral(
        "PROGRAM Main\n"
        "VAR\n"
        "END_VAR\n"
        "\n"
        "drv_ao_1 = _DrvAO(\n"
        "    channel = 0,\n"
        "    value = 0\n"
        ");\n"
        "\n"
        "END_PROGRAM\n");
    const QString projectSubDsl = QStringLiteral(
        "PROGRAM ValveAuto\n"
        "VAR\n"
        "    subReady : BOOL;\n"
        "END_VAR\n"
        "\n"
        "drv_do_1 = _DrvDO(\n"
        "    OutputWord = 0,\n"
        "    Port = 0,\n"
        "    Mask = 255,\n"
        "    Action = 1\n"
        ");\n"
        "\n"
        "END_PROGRAM\n");

    if (!writeTextFile(projectMainPath, projectMainDsl)
            || !writeTextFile(projectSubPath, projectSubDsl)) {
        return 7;
    }

    ProjectRuntimeConfig config;
    config.mainScriptPath = QStringLiteral("main.dsl");
    config.dslScriptPath = config.mainScriptPath;
    config.scriptFiles = QStringList{
        QStringLiteral("main.dsl"),
        QStringLiteral("valve_auto.dsl")
    };

    const CompileResult projectResult = compiler.compileProjectWithResult(
        projectRoot,
        config,
        projectOutputDir,
        QStringLiteral("project_probe"));
    const QString projectErrorText = projectResult.errors.join(QLatin1Char('\n'))
                                   + QLatin1Char('\n')
                                   + projectResult.stdErr;
    const bool projectSkippedForRuntime =
        !projectResult.success
        && projectErrorText.contains(QStringLiteral("No suitable Python interpreter found"));
    if (!projectResult.success && !projectSkippedForRuntime) {
        qCritical() << "compileProjectWithResult failed";
        qCritical() << "stdout:" << projectResult.stdOut;
        qCritical() << "stderr:" << projectResult.stdErr;
        return 8;
    }

    const QString assembledPath = QDir(projectOutputDir).filePath(
        QStringLiteral(".compiler_staging/main_assembled.dsl"));
    const QString assembledText = readTextFile(assembledPath);
    if (!assembledText.contains(QStringLiteral("drv_ao_1 : _DrvAO;"))
            || !assembledText.contains(QStringLiteral("drv_do_1 : _DrvDO;"))
            || !assembledText.contains(QStringLiteral("subReady : BOOL;"))
            || !assembledText.contains(QStringLiteral("drv_ao_1("))
            || !assembledText.contains(QStringLiteral("drv_do_1("))) {
        qCritical() << "Project assembly did not include all source fragments.";
        qCritical() << "assembled file:" << assembledPath;
        qCritical() << assembledText;
        return 10;
    }
    if (assembledText.indexOf(QStringLiteral("subReady : BOOL;"))
            > assembledText.indexOf(QStringLiteral("END_VAR"))) {
        qCritical() << "Project assembly placed child VAR declarations outside the main VAR block.";
        qCritical() << assembledText;
        return 13;
    }

    if (projectResult.success) {
        const QString projectOutputCodePath = QDir(projectOutputDir).filePath(QStringLiteral("main.code"));
        if (!QFileInfo::exists(projectOutputCodePath)) {
            qCritical() << "Project output .code file not found:" << projectOutputCodePath;
            return 9;
        }

        bool artifactListsSubFile = false;
        for (const CompileArtifact& artifact : projectResult.artifacts) {
            const QStringList scriptFiles = artifact.metadata.value(QStringLiteral("scriptFiles")).toStringList();
            if (scriptFiles.contains(projectSubPath)) {
                artifactListsSubFile = true;
                break;
            }
        }
        if (!artifactListsSubFile) {
            qCritical() << "Project compile artifact did not preserve the script file list.";
            return 11;
        }
    }

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

    const QString stagedDslPath = QDir(outputDir).filePath(QStringLiteral(".compiler_staging/main.dsl"));
    const QString stagedText = readTextFile(stagedDslPath);
    if (stagedText.isEmpty()) {
        qCritical() << "Staged DSL file missing or empty:" << stagedDslPath;
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
        qCritical() << "staged file:" << stagedDslPath;
        qCritical() << stagedText;
        return 5;
    }

    qInfo() << "Legacy DSL compile probe passed.";
    qInfo() << "Output file:" << outputCodePath;
    qInfo() << "Staged file:" << stagedDslPath;
    qInfo() << "Project assembled file:" << assembledPath;
    return 0;
}
