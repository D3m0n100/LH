#include "DiagnosticSnapshotService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

bool DiagnosticSnapshotService::exportSnapshot(const QString& baseDir,
                                               const ProjectRuntimeConfig& config,
                                               bool opcRunning,
                                               const QString& opcLastError,
                                               const QVariantMap& opcExtras,
                                               QString* outFilePath,
                                               QString* outError)
{
    QDir dir(baseDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (outError) {
            *outError = QStringLiteral("无法创建诊断目录: %1").arg(baseDir);
        }
        return false;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString filePath = dir.filePath(QStringLiteral("diagnostic_snapshot_%1.json").arg(timestamp));

    QJsonObject root;
    root.insert(QStringLiteral("generatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(QStringLiteral("projectName"), config.projectName);
    root.insert(QStringLiteral("projectConfig"), config.toJson());

    QJsonObject opc;
    opc.insert(QStringLiteral("enabled"), config.opcServer.enabled);
    opc.insert(QStringLiteral("running"), opcRunning);
    opc.insert(QStringLiteral("lastError"), opcLastError);
    opc.insert(QStringLiteral("config"), config.opcServer.toJson());
    opc.insert(QStringLiteral("status"), QJsonObject::fromVariantMap(opcExtras));
    root.insert(QStringLiteral("opc"), opc);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (outError) {
            *outError = QStringLiteral("无法写入诊断文件: %1").arg(file.errorString());
        }
        return false;
    }

    const QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    if (outFilePath) {
        *outFilePath = filePath;
    }
    return true;
}
