#include <algorithm>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest/QtTest>

#include "compiler/DSLCompilerInterface.h"

namespace {

bool writeTextFile(const QString& filePath, const QString& text)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
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

QString diagnostics(const CompileResult& result)
{
    return result.errors.join(QLatin1Char('\n'))
           + QLatin1Char('\n') + result.stdErr
           + QLatin1Char('\n') + result.stdOut;
}

bool runtimeUnavailable(const QString& text)
{
    static const QStringList markers{
        QStringLiteral("No suitable Python interpreter found"),
        QStringLiteral("Missing required module 'antlr4-python3-runtime'"),
        QStringLiteral("No module named 'antlr4'"),
        QStringLiteral("ModuleNotFoundError: No module named 'antlr4'"),
        QStringLiteral("Compiler entry script not found"),
        QStringLiteral("Failed to start")
    };
    for (const QString& marker : markers) {
        if (text.contains(marker, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool compileProject(DSLCompilerInterface* compiler,
                    const QString& projectPath,
                    const ProjectRuntimeConfig& config,
                    const QString& outputDir,
                    quint64 expectedGeneration,
                    CompileResult* result,
                    QString* startError)
{
    QSignalSpy finished(compiler, &DSLCompilerInterface::compileFinishedForGeneration);
    QSignalSpy failed(compiler, &DSLCompilerInterface::compileFailedToStartForGeneration);
    compiler->compileProjectAsync(projectPath,
                                  config,
                                  outputDir,
                                  QStringLiteral("async_semantics"),
                                  expectedGeneration);
    QElapsedTimer timer;
    timer.start();
    while (finished.count() + failed.count() == 0 && timer.elapsed() < 120 * 1000) {
        QTest::qWait(50);
    }
    if (finished.count() + failed.count() == 0) {
        if (startError) {
            *startError = QStringLiteral("Timed out waiting for compile completion.");
        }
        return false;
    }

    if (!failed.isEmpty()) {
        if (startError) {
            *startError = failed.at(0).at(1).toString();
        }
        return false;
    }
    if (finished.count() != 1) {
        if (startError) {
            *startError = QStringLiteral("Expected one compile completion signal.");
        }
        return false;
    }
    if (finished.at(0).at(0).toULongLong() != expectedGeneration) {
        if (startError) {
            *startError = QStringLiteral("Compile completion generation mismatch: expected %1, got %2.")
                    .arg(expectedGeneration)
                    .arg(finished.at(0).at(0).toULongLong());
        }
        return false;
    }
    if (result) {
        *result = compiler->lastCompileResult();
    }
    return true;
}

bool hasArtifact(const CompileResult& result,
                 const QString& type,
                 const QString& format,
                 const QString& path)
{
    for (const CompileArtifact& artifact : result.artifacts) {
        if (artifact.type == type
                && artifact.format == format
                && artifact.path == path
                && QFileInfo(artifact.path).isFile()
                && !artifact.checksum.isEmpty()) {
            return true;
        }
    }
    return false;
}

bool isSafeRelativePath(const QString& path)
{
    if (path.trimmed().isEmpty() || QDir::isAbsolutePath(path)) {
        return false;
    }
    const QStringList parts = QDir::fromNativeSeparators(path).split(
            QLatin1Char('/'), Qt::SkipEmptyParts);
    return std::none_of(parts.constBegin(), parts.constEnd(), [](const QString& part) {
        return part == QStringLiteral("..");
    });
}

} // namespace

class DslCompilerSemanticsTest : public QObject
{
    Q_OBJECT

private slots:
    void missingProfileAllowsOfflineCompileButInvalidProfileStillFails()
    {
        QTemporaryDir project;
        QVERIFY(project.isValid());
        QVERIFY(writeTextFile(QDir(project.path()).filePath("main.lh"),
                              "PROGRAM Main\nVAR\n x : REAL;\nEND_VAR\nx := 1.0;\nEND_PROGRAM\n"));
        ProjectRuntimeConfig config;
        config.projectName = QStringLiteral("offline");
        config.mainScriptPath = QStringLiteral("main.lh");
        config.scriptFiles = QStringList{QStringLiteral("main.lh")};
        DSLCompilerInterface compiler;
        CompileResult result;
        QString error;
        QVERIFY2(compileProject(&compiler, project.path(), config,
                                QDir(project.path()).filePath("build_output"), 901, &result, &error),
                 qPrintable(error));
        QVERIFY2(result.success, qPrintable(diagnostics(result)));
        QVERIFY(result.metadata.value(QStringLiteral("compileOnly")).toBool());
        bool hasCode = false;
        for (const auto& artifact : result.artifacts) {
            QVERIFY(artifact.type != QStringLiteral("download"));
            QVERIFY(artifact.type != QStringLiteral("runtime_manifest"));
            if (artifact.type == QStringLiteral("compiled_code"))
                hasCode = QFileInfo::exists(artifact.path);
        }
        QVERIFY(hasCode);
        config.downloadArtifact.metadata.insert(QStringLiteral("downloadProfilePath"),
                                                 QStringLiteral("missing.json"));
        QVERIFY(!compileProject(&compiler, project.path(), config,
                                 QDir(project.path()).filePath("build_output"), 902, &result, &error));
        QVERIFY(!error.isEmpty());
    }

    void asyncProjectPublishesValidatedGeneration()
    {
        QTemporaryDir temporaryRoot;
        QVERIFY(temporaryRoot.isValid());
        const QString projectRoot = QDir(temporaryRoot.path()).filePath(QStringLiteral("project"));
        const QString outputRoot = QDir(projectRoot).filePath(QStringLiteral("build_output"));
        QVERIFY(QDir().mkpath(projectRoot));

        const QString profileJson = QStringLiteral(
                "{\n"
                "  \"name\": \"async_semantics\",\n"
                "  \"slaveId\": 1,\n"
                "  \"steps\": [\n"
                "    {\"type\": \"sendChunk\", \"params\": {\"dataAddress\": 210, \"chunkWords\": 1}}\n"
                "  ]\n"
                "}\n");
        const QString mainPath = QDir(projectRoot).filePath(QStringLiteral("main.lh"));
        const QString childPath = QDir(projectRoot).filePath(QStringLiteral("valve_auto.lh"));
        QVERIFY(writeTextFile(mainPath,
                              QStringLiteral(
                                      "PROGRAM Main\n"
                                      "VAR\n"
                                      "END_VAR\n"
                                      "drv_ao_1 = _DrvAO(\n"
                                      "    channel = 0,\n"
                                      "    value = 0\n"
                                      ");\n"
                                      "END_PROGRAM\n")));
        QVERIFY(writeTextFile(childPath,
                              QStringLiteral(
                                      "PROGRAM ValveAuto\n"
                                      "VAR\n"
                                      "    subReady : BOOL;\n"
                                      "END_VAR\n"
                                      "drv_do_1 = _DrvDO(\n"
                                      "    OutputWord = 0,\n"
                                      "    Port = 0,\n"
                                      "    Mask = 255,\n"
                                      "    Action = 1\n"
                                      ");\n"
                                      "END_PROGRAM\n")));
        QVERIFY(writeTextFile(QDir(projectRoot).filePath(QStringLiteral("download_profile.json")),
                              profileJson));

        ProjectRuntimeConfig config;
        config.mainScriptPath = QStringLiteral("main.lh");
        config.dslScriptPath = config.mainScriptPath;
        config.scriptFiles = QStringList{QStringLiteral("main.lh"), QStringLiteral("valve_auto.lh")};
        config.downloadArtifact.metadata.insert(
                QStringLiteral("downloadProfilePath"), QStringLiteral("download_profile.json"));

        DSLCompilerInterface compiler;
        CompileResult result;
        QString startError;
        const bool started = compileProject(&compiler,
                                           projectRoot,
                                           config,
                                           outputRoot,
                                           1001,
                                           &result,
                                           &startError);
        if (!started && runtimeUnavailable(startError)) {
            QSKIP(qPrintable(QStringLiteral("Python/ANTLR compiler runtime unavailable: %1")
                                     .arg(startError)));
        }
        QVERIFY2(started, qPrintable(startError));
        if (runtimeUnavailable(diagnostics(result))) {
            QSKIP(qPrintable(QStringLiteral("Python/ANTLR compiler runtime unavailable: %1")
                                     .arg(diagnostics(result))));
        }
        QVERIFY2(result.success, qPrintable(diagnostics(result)));
        QVERIFY(!result.generationId.isEmpty());
        QCOMPARE(result.metadata.value(QStringLiteral("generationId")).toString(),
                 result.generationId);

        const QString generationDir = QDir(outputRoot).filePath(
                QStringLiteral("generations/%1").arg(result.generationId));
        const QString codePath = QDir(generationDir).filePath(QStringLiteral("main.code"));
        QVERIFY(hasArtifact(result, QStringLiteral("download"), QStringLiteral("dsl_custom"), codePath));
        QVERIFY(hasArtifact(result, QStringLiteral("download_profile"), QStringLiteral("json"),
                            QDir(generationDir).filePath(QStringLiteral("download_profile.json"))));
        QVERIFY(hasArtifact(result, QStringLiteral("runtime_points"), QStringLiteral("json"),
                            QDir(generationDir).filePath(QStringLiteral("runtime_points.json"))));
        QVERIFY(hasArtifact(result, QStringLiteral("runtime_manifest"), QStringLiteral("json"),
                            QDir(generationDir).filePath(QStringLiteral("runtime_manifest.json"))));
        QVERIFY(hasArtifact(result, QStringLiteral("parameter_initials"),
                            QStringLiteral("lh_parameter_initials"),
                            QDir(generationDir).filePath(QStringLiteral("main.list"))));
        QVERIFY(hasArtifact(result, QStringLiteral("parameter_layout"),
                            QStringLiteral("lh_parameter_layout"),
                            QDir(generationDir).filePath(QStringLiteral("main.typ"))));
        QVERIFY(hasArtifact(result, QStringLiteral("compile_report"),
                            QStringLiteral("lh_compile_report"),
                            QDir(generationDir).filePath(QStringLiteral("main.rep"))));
        QVERIFY(result.artifacts.size() >= 4);

        const QString code = readTextFile(codePath);
        QVERIFY(code.startsWith(QStringLiteral("// Generated by LH compiler integration")));
        QVERIFY(code.contains(QStringLiteral("// Source:")));
        QVERIFY(code.contains(QStringLiteral("// Output:")));
        QVERIFY(code.contains(QStringLiteral("// Format: .lh -> .code")));

        const QString assembledPath = QDir(generationDir).filePath(
                QStringLiteral(".compiler_staging/main_assembled.lh"));
        const QString assembled = readTextFile(assembledPath);
        QVERIFY(assembled.contains(QStringLiteral("drv_do_1 : DrvDO;")));
        QVERIFY(assembled.contains(QStringLiteral("drv_do_1(")));
        QVERIFY(!assembled.contains(QStringLiteral("= _DrvDO(")));

        const QString manifestPath = QDir(generationDir).filePath(QStringLiteral("runtime_manifest.json"));
        const QJsonObject manifest = QJsonDocument::fromJson(readTextFile(manifestPath).toUtf8()).object();
        QCOMPARE(manifest.value(QStringLiteral("complete")).toBool(), true);
        QCOMPARE(manifest.value(QStringLiteral("generationId")).toString(), result.generationId);
        QCOMPARE(manifest.value(QStringLiteral("scriptFileCount")).toInt(), 2);
        QVERIFY(manifest.value(QStringLiteral("artifactPaths")).toArray().size() >= 3);
        for (const QJsonValue& value : manifest.value(QStringLiteral("artifactPaths")).toArray()) {
            QVERIFY(isSafeRelativePath(value.toString()));
        }
    }

    void asyncProjectRejectsInvalidDslWithoutArtifact()
    {
        QTemporaryDir temporaryRoot;
        QVERIFY(temporaryRoot.isValid());
        const QString projectRoot = QDir(temporaryRoot.path()).filePath(QStringLiteral("project"));
        QVERIFY(QDir().mkpath(projectRoot));
        QVERIFY(writeTextFile(QDir(projectRoot).filePath(QStringLiteral("valve_auto.lh")),
                              QStringLiteral("PROGRAM ValveAuto\nEND_PROGRAM\n")));
        QVERIFY(writeTextFile(QDir(projectRoot).filePath(QStringLiteral("download_profile.json")),
                              QStringLiteral("{\"steps\":[{\"type\":\"sendChunk\",\"params\":{}}]}\n")));

        ProjectRuntimeConfig config;
        config.mainScriptPath = QStringLiteral("main.lh");
        config.dslScriptPath = config.mainScriptPath;
        config.scriptFiles = QStringList{QStringLiteral("main.lh"), QStringLiteral("valve_auto.lh")};
        config.downloadArtifact.metadata.insert(
                QStringLiteral("downloadProfilePath"), QStringLiteral("download_profile.json"));

        const QStringList invalidSources{
            QStringLiteral("PROGRAM Broken\nVAR\n    flag : BOOL;\nEND_VAR\nflag := ;\nEND_PROGRAM\n"),
            QStringLiteral("unknown_1 = _DefinitelyUnknown(\n    Value = 1\n);\n")
        };
        for (int index = 0; index < invalidSources.size(); ++index) {
            const QString mainPath = QDir(projectRoot).filePath(QStringLiteral("main.lh"));
            QVERIFY(writeTextFile(mainPath, invalidSources.at(index)));
            DSLCompilerInterface compiler;
            CompileResult result;
            QString startError;
            const QString outputDir = QDir(projectRoot).filePath(
                    QStringLiteral("output_%1").arg(index));
            const bool started = compileProject(&compiler,
                                               projectRoot,
                                               config,
                                               outputDir,
                                               static_cast<quint64>(2001 + index),
                                               &result,
                                               &startError);
            if (!started && runtimeUnavailable(startError)) {
                QSKIP(qPrintable(QStringLiteral("Python/ANTLR compiler runtime unavailable: %1")
                                         .arg(startError)));
            }
            QVERIFY2(started, qPrintable(startError));
            if (runtimeUnavailable(diagnostics(result))) {
                QSKIP(qPrintable(QStringLiteral("Python/ANTLR compiler runtime unavailable: %1")
                                         .arg(diagnostics(result))));
            }
            QVERIFY(!result.success);
            QVERIFY2(!diagnostics(result).trimmed().isEmpty(),
                     "Invalid DSL compile must return diagnostics.");
            QVERIFY(result.artifacts.isEmpty()
                    || std::none_of(result.artifacts.constBegin(), result.artifacts.constEnd(),
                                    [](const CompileArtifact& artifact) {
                                        return artifact.type == QStringLiteral("download");
                                     }));
            const QFileInfoList generations = QDir(outputDir).entryInfoList(
                    QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo& generation : generations) {
                QVERIFY(!QFileInfo(QDir(generation.absoluteFilePath()).filePath(QStringLiteral("main.code"))).exists());
            }
        }
    }

    void nonFiniteFloatExpressionsFailClosed()
    {
        QTemporaryDir temporaryRoot;
        QVERIFY(temporaryRoot.isValid());
        const QString projectRoot = QDir(temporaryRoot.path()).filePath(QStringLiteral("project"));
        QVERIFY(QDir().mkpath(projectRoot));

        const QString profileJson = QStringLiteral(
                "{\n"
                "  \"name\": \"async_semantics\",\n"
                "  \"slaveId\": 1,\n"
                "  \"steps\": [\n"
                "    {\"type\": \"sendChunk\", \"params\": {\"dataAddress\": 210, \"chunkWords\": 1}}\n"
                "  ]\n"
                "}\n");
        QVERIFY(writeTextFile(QDir(projectRoot).filePath(QStringLiteral("download_profile.json")),
                              profileJson));

        ProjectRuntimeConfig config;
        config.mainScriptPath = QStringLiteral("main.lh");
        config.dslScriptPath = config.mainScriptPath;
        config.scriptFiles = QStringList{QStringLiteral("main.lh")};
        config.downloadArtifact.metadata.insert(
                QStringLiteral("downloadProfilePath"), QStringLiteral("download_profile.json"));

        const QStringList badFloatSources{
            QStringLiteral("PROGRAM Main\nVAR\n  system : System;\n  r : REAL;\nEND_VAR\nsystem(Author := 1, Config := 100, Date := 2601);\nr := 1.0e999;\nEND_PROGRAM\n"),
            QStringLiteral("PROGRAM Main\nVAR\n  system : System;\n  r : REAL;\nEND_VAR\nsystem(Author := 1, Config := 100, Date := 2601);\nr := 1.0e999 - 1.0e999;\nEND_PROGRAM\n"),
            QStringLiteral("PROGRAM Main\nVAR\n  system : System;\n  r : REAL;\nEND_VAR\nsystem(Author := 1, Config := 100, Date := 2601);\nr := 1.0e40;\nEND_PROGRAM\n")
        };

        for (int index = 0; index < badFloatSources.size(); ++index) {
            const QString mainPath = QDir(projectRoot).filePath(QStringLiteral("main.lh"));
            QVERIFY(writeTextFile(mainPath, badFloatSources.at(index)));
            DSLCompilerInterface compiler;
            CompileResult result;
            QString startError;
            const QString outputDir = QDir(projectRoot).filePath(
                    QStringLiteral("output_bad_float_%1").arg(index));
            const bool started = compileProject(&compiler,
                                               projectRoot,
                                               config,
                                               outputDir,
                                               static_cast<quint64>(3001 + index),
                                               &result,
                                               &startError);
            if (!started && runtimeUnavailable(startError)) {
                QSKIP(qPrintable(QStringLiteral("Python/ANTLR compiler runtime unavailable: %1")
                                         .arg(startError)));
            }
            QVERIFY2(started, qPrintable(startError));
            if (runtimeUnavailable(diagnostics(result))) {
                QSKIP(qPrintable(QStringLiteral("Python/ANTLR compiler runtime unavailable: %1")
                                         .arg(diagnostics(result))));
            }
            QVERIFY(!result.success);
            QVERIFY2(!diagnostics(result).trimmed().isEmpty(),
                     "Non-finite or overflow float compile must return diagnostics.");
            const QFileInfoList generations = QDir(outputDir).entryInfoList(
                    QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo& generation : generations) {
                QVERIFY(!QFileInfo(QDir(generation.absoluteFilePath()).filePath(QStringLiteral("main.code"))).exists());
            }
        }
    }
};

QTEST_MAIN(DslCompilerSemanticsTest)
#include "dsl_compiler_semantics_test.moc"
