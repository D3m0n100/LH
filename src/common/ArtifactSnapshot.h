#pragma once
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>

namespace ArtifactSnapshot {
constexpr qint64 MaxFileBytes = 16 * 1024 * 1024;
constexpr qint64 MaxTotalBytes = 64 * 1024 * 1024;
inline QString key(const QString& path) { return QFileInfo(path).absoluteFilePath(); }
inline bool read(const QString& path, QByteArray* bytes, QString* error)
{
    QFile file(path);
    auto fail = [&](const QString& message) { if (error) *error = message; return false; };
    if (!file.open(QIODevice::ReadOnly)) return fail(QStringLiteral("Cannot open artifact snapshot: %1: %2").arg(path, file.errorString()));
    const qint64 size = file.size();
    if (size < 0 || size > MaxFileBytes) return fail(QStringLiteral("Artifact exceeds 16 MiB snapshot limit: %1").arg(path));
    QByteArray result = file.read(MaxFileBytes + 1);
    if (file.error() != QFileDevice::NoError || result.size() != size || file.size() != size || !file.atEnd())
        return fail(QStringLiteral("Artifact changed size or snapshot read failed: %1").arg(path));
    *bytes = result;
    return true;
}
inline QString checksum(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
}
