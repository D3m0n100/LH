#ifndef DOWNLOADCONFIRMATION_H
#define DOWNLOADCONFIRMATION_H

#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <QString>
#include <QStringList>

/**
 * @brief 下载前确认快照
 *
 * 在向设备写入程序之前，集中展示目标、端点、工程/入口、产物身份、版本证据
 * 和即将执行的动作。快照的 fingerprint() 用于批准绑定：任一关键字段变化都会
 * 产生新的指纹，必须重新确认。
 */
struct DownloadConfirmationSnapshot
{
    QString operationId;
    QString targetName;
    QString targetIdentity;
    QString endpoint;
    QString projectName;
    QString projectPath;
    QString entryPath;
    QString artifactPath;
    qint64 artifactBytes = -1;
    QString artifactChecksum;
    QString configurationFingerprint;
    QString optionsFingerprint;
    QString generationId;
    QString versionEvidence;
    QString action;
    quint64 backendGeneration = 0;

    bool isValid() const
    {
        return !operationId.trimmed().isEmpty()
                && !artifactPath.trimmed().isEmpty();
    }

    QString fingerprint() const
    {
        QStringList fields;
        fields << operationId << targetName << targetIdentity << endpoint
               << projectName << projectPath << entryPath << artifactPath
               << QString::number(artifactBytes) << artifactChecksum
               << configurationFingerprint << optionsFingerprint
               << generationId << versionEvidence << action
               << QString::number(backendGeneration);
        return QString::fromLatin1(
                QCryptographicHash::hash(fields.join(QLatin1Char('\x1f')).toUtf8(),
                                         QCryptographicHash::Sha256).toHex());
    }

    QString toDisplayText() const
    {
        auto orUnknown = [](const QString& value) {
            const QString trimmed = value.trimmed();
            return trimmed.isEmpty() ? QStringLiteral("未知") : trimmed;
        };
        QString sizeText = QStringLiteral("未知");
        if (artifactBytes >= 0) {
            sizeText = QStringLiteral("%1 字节").arg(artifactBytes);
        }
        QStringList lines;
        lines << QStringLiteral("即将执行：%1").arg(orUnknown(action));
        if (action == QStringLiteral("专家诊断直接下载")) {
            lines << QStringLiteral("风险：专家诊断下载绕过正式编译检查、generation manifest 校验与源码关联。");
        }
        lines << QStringLiteral("目标设备：%1").arg(orUnknown(targetName));
        lines << QStringLiteral("目标身份：%1").arg(orUnknown(targetIdentity));
        lines << QStringLiteral("连接端点：%1").arg(orUnknown(endpoint));
        lines << QStringLiteral("工程：%1").arg(orUnknown(projectName));
        lines << QStringLiteral("构建入口：%1").arg(orUnknown(entryPath));
        lines << QStringLiteral("产物：%1").arg(orUnknown(artifactPath));
        lines << QStringLiteral("产物大小：%1").arg(sizeText);
        lines << QStringLiteral("产物摘要：%1").arg(orUnknown(artifactChecksum));
        lines << QStringLiteral("配置摘要：%1").arg(orUnknown(configurationFingerprint));
        lines << QStringLiteral("Generation：%1").arg(orUnknown(generationId));
        lines << QStringLiteral("设备程序版本证据：%1").arg(orUnknown(versionEvidence));
        lines << QString();
        lines << QStringLiteral("确认后本次批准仅对该快照有效；目标、端点、产物或版本证据变化时必须重新确认。");
        return lines.join(QLatin1Char('\n'));
    }
};

#endif // DOWNLOADCONFIRMATION_H
