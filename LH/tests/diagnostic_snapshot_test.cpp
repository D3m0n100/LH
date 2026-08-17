#include <QtTest/QtTest>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

#include "DiagnosticSnapshotService.h"

class DiagnosticSnapshotTest : public QObject
{
    Q_OBJECT

private slots:
    void redactsNestedConfigurationWithoutMutatingInputs()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ProjectRuntimeConfig config;
        config.projectName = QStringLiteral("diagnostic-project");
        config.opcServer.enabled = true;

        QVariantMap projectParameters;
        projectParameters.insert(QStringLiteral("PASSWORD"), QStringLiteral("project-password"));
        projectParameters.insert(QStringLiteral("PASSWD"), QStringLiteral("project-passwd"));
        projectParameters.insert(QStringLiteral("pwd"), QStringLiteral("project-pwd"));
        projectParameters.insert(QStringLiteral("secret"), QStringLiteral("project-secret"));
        projectParameters.insert(QStringLiteral("access_token"), QStringLiteral("access-token"));
        projectParameters.insert(QStringLiteral("refresh-token"), QStringLiteral("refresh-token"));
        projectParameters.insert(QStringLiteral("apiKey"), QStringLiteral("api-key"));
        projectParameters.insert(QStringLiteral("access-key"), QStringLiteral("access-key"));
        projectParameters.insert(QStringLiteral("private_key"), QStringLiteral("private-key"));
        projectParameters.insert(QStringLiteral("credentials"), QStringLiteral("project-credentials"));
        projectParameters.insert(QStringLiteral("authorization"), QStringLiteral("Bearer project"));
        projectParameters.insert(QStringLiteral("sessionCookie"), QStringLiteral("project-cookie"));
        projectParameters.insert(QStringLiteral("connection-string"), QStringLiteral("project-connection"));
        projectParameters.insert(QStringLiteral("tokenCount"), 17);
        projectParameters.insert(QStringLiteral("safeValue"), QStringLiteral("keep-project-value"));

        QVariantMap nestedObject;
        nestedObject.insert(QStringLiteral("secretKey"), QStringLiteral("nested-secret-key"));
        nestedObject.insert(QStringLiteral("tokenCount"), 23);
        nestedObject.insert(QStringLiteral("safeNestedValue"), QStringLiteral("keep-nested-value"));

        QVariantMap arrayObject;
        arrayObject.insert(QStringLiteral("TOKEN"), QStringLiteral("array-token"));
        arrayObject.insert(QStringLiteral("normalValue"), QStringLiteral("keep-array-value"));
        arrayObject.insert(QStringLiteral("tokenCount"), 31);
        QVariantList nestedArray;
        nestedArray.append(QVariant(arrayObject));

        projectParameters.insert(QStringLiteral("nestedObject"), QVariant(nestedObject));
        projectParameters.insert(QStringLiteral("nestedArray"), QVariant(nestedArray));
        config.commParameters = projectParameters;

        QVariantMap opcMetadata;
        opcMetadata.insert(QStringLiteral("secretKey"), QStringLiteral("opc-secret-key"));
        opcMetadata.insert(QStringLiteral("tokenCount"), 41);
        opcMetadata.insert(QStringLiteral("safeOpcValue"), QStringLiteral("keep-opc-value"));
        config.opcServer.metadata = opcMetadata;

        QVariantMap opcStatus;
        opcStatus.insert(QStringLiteral("Pwd"), QStringLiteral("status-password"));
        opcStatus.insert(QStringLiteral("SESSION_COOKIE"), QStringLiteral("status-cookie"));
        opcStatus.insert(QStringLiteral("token_count"), 47);
        opcStatus.insert(QStringLiteral("normalStatus"), QStringLiteral("healthy"));

        QVariantMap statusArrayObject;
        statusArrayObject.insert(QStringLiteral("Authorization"), QStringLiteral("status-auth"));
        statusArrayObject.insert(QStringLiteral("statusValue"), QStringLiteral("keep-status-value"));
        QVariantList statusArray;
        statusArray.append(QVariant(statusArrayObject));
        opcStatus.insert(QStringLiteral("items"), QVariant(statusArray));

        const QJsonObject configBefore = config.toJson();
        const QVariantMap opcStatusBefore = opcStatus;

        QString outputPath;
        QString error;
        QVERIFY2(DiagnosticSnapshotService::exportSnapshot(tempDir.path(),
                                                           config,
                                                           true,
                                                           QStringLiteral("last OPC error"),
                                                           opcStatus,
                                                           &outputPath,
                                                           &error),
                 qPrintable(error));
        QVERIFY(!outputPath.isEmpty());

        QVERIFY(config.toJson() == configBefore);
        QVERIFY(opcStatus == opcStatusBefore);

        QFile outputFile(outputPath);
        QVERIFY(outputFile.open(QIODevice::ReadOnly | QIODevice::Text));
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(outputFile.readAll(), &parseError);
        outputFile.close();
        QVERIFY2(!document.isNull(), qPrintable(parseError.errorString()));
        QVERIFY(document.isObject());

        const QJsonObject root = document.object();
        QCOMPARE(root.value(QStringLiteral("projectName")).toString(), config.projectName);
        QVERIFY(root.contains(QStringLiteral("generatedAt")));

        const QJsonObject projectConfig = root.value(QStringLiteral("projectConfig")).toObject();
        const QJsonObject projectConfigParameters = projectConfig.value(QStringLiteral("commParameters")).toObject();
        const QString redacted = QStringLiteral("[REDACTED]");
        QCOMPARE(projectConfigParameters.value(QStringLiteral("PASSWORD")).toString(), redacted);
        QCOMPARE(projectConfigParameters.value(QStringLiteral("PASSWD")).toString(), redacted);
        QCOMPARE(projectConfigParameters.value(QStringLiteral("pwd")).toString(), redacted);
        QCOMPARE(projectConfigParameters.value(QStringLiteral("secret")).toString(), redacted);
        QCOMPARE(projectConfigParameters.value(QStringLiteral("access_token")).toString(), redacted);
        QCOMPARE(projectConfigParameters.value(QStringLiteral("refresh-token")).toString(), redacted);
        QCOMPARE(projectConfigParameters.value(QStringLiteral("apiKey")).toString(), redacted);
        QCOMPARE(projectConfigParameters.value(QStringLiteral("access-key")).toString(), redacted);
        QCOMPARE(projectConfigParameters.value(QStringLiteral("private_key")).toString(), redacted);
        QCOMPARE(projectConfigParameters.value(QStringLiteral("credentials")).toString(), redacted);
        QCOMPARE(projectConfigParameters.value(QStringLiteral("authorization")).toString(), redacted);
        QCOMPARE(projectConfigParameters.value(QStringLiteral("sessionCookie")).toString(), redacted);
        QCOMPARE(projectConfigParameters.value(QStringLiteral("connection-string")).toString(), redacted);
        QCOMPARE(projectConfigParameters.value(QStringLiteral("tokenCount")).toInt(), 17);
        QCOMPARE(projectConfigParameters.value(QStringLiteral("safeValue")).toString(),
                 QStringLiteral("keep-project-value"));

        const QJsonObject nestedOutput = projectConfigParameters.value(QStringLiteral("nestedObject")).toObject();
        QCOMPARE(nestedOutput.value(QStringLiteral("secretKey")).toString(), redacted);
        QCOMPARE(nestedOutput.value(QStringLiteral("tokenCount")).toInt(), 23);
        QCOMPARE(nestedOutput.value(QStringLiteral("safeNestedValue")).toString(),
                 QStringLiteral("keep-nested-value"));

        const QJsonArray nestedOutputArray = projectConfigParameters.value(QStringLiteral("nestedArray")).toArray();
        QCOMPARE(nestedOutputArray.size(), 1);
        const QJsonObject arrayOutput = nestedOutputArray.at(0).toObject();
        QCOMPARE(arrayOutput.value(QStringLiteral("TOKEN")).toString(), redacted);
        QCOMPARE(arrayOutput.value(QStringLiteral("normalValue")).toString(),
                 QStringLiteral("keep-array-value"));
        QCOMPARE(arrayOutput.value(QStringLiteral("tokenCount")).toInt(), 31);

        const QJsonObject opc = root.value(QStringLiteral("opc")).toObject();
        QCOMPARE(opc.value(QStringLiteral("enabled")).toBool(), true);
        QCOMPARE(opc.value(QStringLiteral("running")).toBool(), true);
        QCOMPARE(opc.value(QStringLiteral("lastError")).toString(), QStringLiteral("last OPC error"));

        const QJsonObject opcConfig = opc.value(QStringLiteral("config")).toObject();
        const QJsonObject opcConfigMetadata = opcConfig.value(QStringLiteral("metadata")).toObject();
        QCOMPARE(opcConfigMetadata.value(QStringLiteral("secretKey")).toString(), redacted);
        QCOMPARE(opcConfigMetadata.value(QStringLiteral("tokenCount")).toInt(), 41);
        QCOMPARE(opcConfigMetadata.value(QStringLiteral("safeOpcValue")).toString(),
                 QStringLiteral("keep-opc-value"));

        const QJsonObject status = opc.value(QStringLiteral("status")).toObject();
        QCOMPARE(status.value(QStringLiteral("Pwd")).toString(), redacted);
        QCOMPARE(status.value(QStringLiteral("SESSION_COOKIE")).toString(), redacted);
        QCOMPARE(status.value(QStringLiteral("token_count")).toInt(), 47);
        QCOMPARE(status.value(QStringLiteral("normalStatus")).toString(), QStringLiteral("healthy"));
        const QJsonArray statusOutputArray = status.value(QStringLiteral("items")).toArray();
        QCOMPARE(statusOutputArray.size(), 1);
        const QJsonObject statusArrayOutput = statusOutputArray.at(0).toObject();
        QCOMPARE(statusArrayOutput.value(QStringLiteral("Authorization")).toString(), redacted);
        QCOMPARE(statusArrayOutput.value(QStringLiteral("statusValue")).toString(),
                 QStringLiteral("keep-status-value"));
    }

    void invalidTargetDirectoryFailsWithoutOutputPath()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString occupiedPath = tempDir.filePath(QStringLiteral("existing-file"));
        QFile occupiedFile(occupiedPath);
        QVERIFY(occupiedFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(occupiedFile.write("not a directory") > 0);
        occupiedFile.close();

        ProjectRuntimeConfig config;
        config.projectName = QStringLiteral("failure-project");
        QString outputPath = QStringLiteral("unchanged-output-path");
        QString error;

        QVERIFY(!DiagnosticSnapshotService::exportSnapshot(occupiedPath,
                                                            config,
                                                            false,
                                                            QString(),
                                                            QVariantMap(),
                                                            &outputPath,
                                                            &error));
        QCOMPARE(outputPath, QStringLiteral("unchanged-output-path"));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_MAIN(DiagnosticSnapshotTest)
#include "diagnostic_snapshot_test.moc"
