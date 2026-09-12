#ifndef PATHSECURITYUTILS_H
#define PATHSECURITYUTILS_H

#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QCryptographicHash>

namespace PathSecurityUtils {

inline QString sha256ForFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    // QCryptographicHash propagates QIODevice read failures; never publish a
    // digest of a prefix when a file becomes unreadable during hashing.
    if (!hash.addData(&file))
        return QString();
    return QString::fromLatin1(hash.result().toHex());
}

inline bool hasParentTraversal(const QString& path)
{
    const QStringList parts = QDir::fromNativeSeparators(path).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        if (part == QStringLiteral(".."))
            return true;
    }
    return false;
}

inline bool safeManifestRelativePath(const QString& path)
{
    const QString trimmed = path.trimmed();
    return !trimmed.isEmpty()
            && !QDir::isAbsolutePath(trimmed)
            && !hasParentTraversal(trimmed);
}

inline QString comparablePath(const QString& path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(QFileInfo(path).absoluteFilePath()) : canonical;
}

inline bool pathWithinRoot(const QString& root, const QString& path)
{
    const QString relative = QDir(comparablePath(root)).relativeFilePath(comparablePath(path));
    return relative != QStringLiteral("..")
            && !relative.startsWith(QStringLiteral("../"))
            && !relative.startsWith(QStringLiteral("..\\"))
            && !QDir::isAbsolutePath(relative);
}

inline bool pathWithinDirectory(const QString& directory, const QString& path)
{
    return pathWithinRoot(directory, path);
}

} // namespace PathSecurityUtils

#endif // PATHSECURITYUTILS_H
