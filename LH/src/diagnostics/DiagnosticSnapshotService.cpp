#include "DiagnosticSnapshotService.h"

#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QSet>

namespace {

const QString kRedactedValue = QStringLiteral("[REDACTED]");

QString normalizedKey(const QString& key)
{
    QString normalized;
    normalized.reserve(key.size());
    for (const QChar character : key) {
        if (character.isLetterOrNumber()) {
            normalized.append(character.toLower());
        }
    }
    return normalized;
}

bool isSensitiveKey(const QString& key)
{
    static const QSet<QString> sensitiveNames = {
        QStringLiteral("password"),
        QStringLiteral("passwd"),
        QStringLiteral("pwd"),
        QStringLiteral("secret"),
        QStringLiteral("token"),
        QStringLiteral("accesstoken"),
        QStringLiteral("refreshtoken"),
        QStringLiteral("apikey"),
        QStringLiteral("accesskey"),
        QStringLiteral("secretkey"),
        QStringLiteral("privatekey"),
        QStringLiteral("credential"),
        QStringLiteral("credentials"),
        QStringLiteral("authorization"),
        QStringLiteral("cookie"),
        QStringLiteral("sessioncookie"),
        QStringLiteral("connectionstring"),
        QStringLiteral("passphrase")
    };

    const QString normalized = normalizedKey(key);
    if (sensitiveNames.contains(normalized)) {
        return true;
    }

    // Also cover common qualified names such as databasePassword or clientSecret.
    // Do not use a substring match: tokenCount is a diagnostic counter, not a token.
    for (const QString& sensitiveName : sensitiveNames) {
        if (normalized.size() > sensitiveName.size()
                && normalized.endsWith(sensitiveName)) {
            return true;
        }
    }
    return false;
}

QJsonValue redactJsonValue(const QJsonValue& value)
{
    if (value.isObject()) {
        QJsonObject redacted;
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (isSensitiveKey(it.key())) {
                redacted.insert(it.key(), kRedactedValue);
            } else {
                redacted.insert(it.key(), redactJsonValue(it.value()));
            }
        }
        return redacted;
    }

    if (value.isArray()) {
        QJsonArray redacted;
        const QJsonArray array = value.toArray();
        for (const QJsonValue& item : array) {
            redacted.append(redactJsonValue(item));
        }
        return redacted;
    }

    return value;
}

QJsonObject redactJsonObject(const QJsonObject& object)
{
    return redactJsonValue(object).toObject();
}

}

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
    root.insert(QStringLiteral("projectConfig"), redactJsonObject(config.toJson()));

    QJsonObject opc;
    opc.insert(QStringLiteral("enabled"), config.opcServer.enabled);
    opc.insert(QStringLiteral("running"), opcRunning);
    opc.insert(QStringLiteral("lastError"), opcLastError);
    opc.insert(QStringLiteral("config"), redactJsonObject(config.opcServer.toJson()));
    opc.insert(QStringLiteral("status"), redactJsonObject(QJsonObject::fromVariantMap(opcExtras)));
    root.insert(QStringLiteral("opc"), opc);

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (outError) {
            *outError = QStringLiteral("无法写入诊断文件: %1").arg(file.errorString());
        }
        return false;
    }

    const QJsonDocument doc(root);
    const QByteArray payload = doc.toJson(QJsonDocument::Indented);
    const qint64 written = file.write(payload);
    if (written != payload.size()) {
        if (outError) {
            *outError = QStringLiteral("诊断文件写入不完整: %1").arg(file.errorString());
        }
        file.cancelWriting();
        return false;
    }
    if (!file.flush()) {
        if (outError) {
            *outError = QStringLiteral("无法刷新诊断文件: %1").arg(file.errorString());
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        const QString commitError = file.errorString();
        file.cancelWriting();
        if (outError) {
            *outError = QStringLiteral("无法提交诊断文件: %1").arg(commitError);
        }
        return false;
    }

    if (outFilePath) {
        *outFilePath = filePath;
    }
    return true;
}
