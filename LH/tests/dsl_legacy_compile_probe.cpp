#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
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

    const QString sourcePath = QDir(runtimeRoot).filePath(QStringLiteral("main.lh"));
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

    QString componentStdout;
    QString componentStderr;
    if (!compiler.listComponents(&componentStdout, &componentStderr)) {
        if (componentStderr.contains(QStringLiteral("No suitable Python interpreter found"))) {
            qWarning() << "LH compiler probe skipped: compiler runtime is unavailable.";
            qWarning() << "stderr:" << componentStderr;
            return 0;
        }
        qCritical() << "Compiler component list could not be loaded.";
        qCritical() << "stdout:" << componentStdout;
        qCritical() << "stderr:" << componentStderr;
        return 14;
    }
    if (!componentStdout.contains(QStringLiteral("DrvDO"))
            || componentStdout.contains(QStringLiteral("_DrvDO"))) {
        qCritical() << "Compiler component list is not aligned with the LH compiler.";
        qCritical() << "stdout:" << componentStdout;
        qCritical() << "stderr:" << componentStderr;
        return 14;
    }

    const QString componentDescription = compiler.describeComponents(&componentStderr);
    const QJsonDocument componentDoc = QJsonDocument::fromJson(componentDescription.toUtf8());
    const QJsonArray components = componentDoc.object().value(QStringLiteral("components")).toArray();
    bool describedDrvDo = false;
    for (const QJsonValue& value : components) {
        const QJsonObject component = value.toObject();
        if (component.value(QStringLiteral("name")).toString() == QStringLiteral("DrvDO")
                && component.value(QStringLiteral("parameters")).toArray().size() == 4) {
            describedDrvDo = true;
            break;
        }
    }
    if (!describedDrvDo) {
        qCritical() << "Compiler component metadata did not describe DrvDO parameters.";
        qCritical() << "description:" << componentDescription.left(500);
        qCritical() << "stderr:" << componentStderr;
        return 15;
    }

    ProjectRuntimeConfig missingChildConfig;
    missingChildConfig.mainScriptPath = sourcePath;
    missingChildConfig.dslScriptPath = sourcePath;
    missingChildConfig.scriptFiles = QStringList{
        sourcePath,
        QStringLiteral("missing_child.lh")
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

    const QString projectMainPath = QDir(projectRoot).filePath(QStringLiteral("main.lh"));
    const QString projectSubPath = QDir(projectRoot).filePath(QStringLiteral("valve_auto.lh"));
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
    config.mainScriptPath = QStringLiteral("main.lh");
    config.dslScriptPath = config.mainScriptPath;
    config.scriptFiles = QStringList{
        QStringLiteral("main.lh"),
        QStringLiteral("valve_auto.lh")
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
        QStringLiteral(".compiler_staging/main_assembled.lh"));
    const QString assembledText = readTextFile(assembledPath);
    if (!assembledText.contains(QStringLiteral("drv_ao_1 : DrvAO;"))
            || !assembledText.contains(QStringLiteral("drv_do_1 : DrvDO;"))
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
        const QString projectCodeText = readTextFile(projectOutputCodePath);
        if (!projectCodeText.startsWith(QStringLiteral("// Generated by LH compiler integration"))
                || !projectCodeText.contains(QStringLiteral("// Source:"))
                || !projectCodeText.contains(QStringLiteral("// Output:"))
                || !projectCodeText.contains(QStringLiteral("// Format: .lh -> .code"))) {
            qCritical() << "Project .code metadata header is missing.";
            qCritical() << projectCodeText.left(500);
            return 16;
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
    bool hasParameterInitialsArtifact = false;
    bool hasParameterLayoutArtifact = false;
    bool hasCompileReportArtifact = false;
    for (const CompileArtifact& artifact : result.artifacts) {
        if (artifact.type == QStringLiteral("download")
                && artifact.format == QStringLiteral("dsl_custom")
                && QFileInfo::exists(artifact.path)
                && !artifact.checksum.isEmpty()) {
            hasDownloadArtifact = true;
        }
        if (artifact.type == QStringLiteral("parameter_initials")
                && artifact.format == QStringLiteral("lh_parameter_initials")
                && QFileInfo::exists(artifact.path)
                && !artifact.checksum.isEmpty()) {
            hasParameterInitialsArtifact = true;
        }
        if (artifact.type == QStringLiteral("parameter_layout")
                && artifact.format == QStringLiteral("lh_parameter_layout")
                && QFileInfo::exists(artifact.path)
                && !artifact.checksum.isEmpty()) {
            hasParameterLayoutArtifact = true;
        }
        if (artifact.type == QStringLiteral("compile_report")
                && artifact.format == QStringLiteral("lh_compile_report")
                && QFileInfo::exists(artifact.path)
                && !artifact.checksum.isEmpty()) {
            hasCompileReportArtifact = true;
        }
    }
    if (!hasDownloadArtifact) {
        qCritical() << "CompileResult did not report a dsl_custom download artifact.";
        return 6;
    }
    if (!hasParameterInitialsArtifact || !hasParameterLayoutArtifact || !hasCompileReportArtifact) {
        qCritical() << "CompileResult did not report LH sidecar artifacts.";
        qCritical() << "parameter_initials:" << hasParameterInitialsArtifact
                    << "parameter_layout:" << hasParameterLayoutArtifact
                    << "compile_report:" << hasCompileReportArtifact;
        return 18;
    }

    const QString stagedDslPath = QDir(outputDir).filePath(QStringLiteral(".compiler_staging/main.lh"));
    const QString stagedText = readTextFile(stagedDslPath);
    if (stagedText.isEmpty()) {
        qCritical() << "Staged LH file missing or empty:" << stagedDslPath;
        return 4;
    }

    const QString codeText = readTextFile(outputCodePath);
    if (!codeText.startsWith(QStringLiteral("// Generated by LH compiler integration"))
            || !codeText.contains(QStringLiteral("// GeneratedAt:"))
            || !codeText.contains(QStringLiteral("// Source:"))
            || !codeText.contains(QStringLiteral("// Output:"))
            || !codeText.contains(QStringLiteral("// Format: .lh -> .code"))) {
        qCritical() << ".code metadata header is missing.";
        qCritical() << codeText.left(500);
        return 17;
    }

    const QString outputListPath = QDir(outputDir).filePath(QStringLiteral("main.list"));
    const QString outputTypPath = QDir(outputDir).filePath(QStringLiteral("main.typ"));
    const QString outputReportPath = QDir(outputDir).filePath(QStringLiteral("main.rep"));
    if (!QFileInfo::exists(outputListPath)
            || !QFileInfo::exists(outputTypPath)
            || !QFileInfo::exists(outputReportPath)) {
        qCritical() << "LH sidecar files were not generated.";
        qCritical() << outputListPath << QFileInfo::exists(outputListPath);
        qCritical() << outputTypPath << QFileInfo::exists(outputTypPath);
        qCritical() << outputReportPath << QFileInfo::exists(outputReportPath);
        return 19;
    }

    const QString listText = readTextFile(outputListPath);
    const QString typText = readTextFile(outputTypPath);
    const QString reportText = readTextFile(outputReportPath);
    if (!listText.startsWith(QStringLiteral("# LH compiler generated file"))
            || !typText.startsWith(QStringLiteral("# LH compiler generated file"))
            || !reportText.startsWith(QStringLiteral("# LH compiler generated file"))
            || listText.contains(QStringLiteral("LM compiler"), Qt::CaseInsensitive)
            || typText.contains(QStringLiteral("LM compiler"), Qt::CaseInsensitive)
            || reportText.contains(QStringLiteral("LM compiler"), Qt::CaseInsensitive)) {
        qCritical() << "LH sidecar headers are not aligned with LH naming.";
        qCritical() << listText.left(200);
        qCritical() << typText.left(200);
        qCritical() << reportText.left(200);
        return 20;
    }
    if (!listText.contains(QStringLiteral("drv_ao_1"))
            || !typText.contains(QStringLiteral("function_block"))
            || !reportText.contains(QStringLiteral("status\tsuccess"))) {
        qCritical() << "LH sidecar contents are incomplete.";
        qCritical() << listText.left(500);
        qCritical() << typText.left(500);
        qCritical() << reportText.left(500);
        return 21;
    }

    const bool wrapped = stagedText.contains(QStringLiteral("PROGRAM main"))
                      && stagedText.contains(QStringLiteral("END_PROGRAM"));
    const bool declared = stagedText.contains(QStringLiteral("drv_ao_1 : DrvAO;"))
                       && stagedText.contains(QStringLiteral("drv_do_1 : DrvDO;"));
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
