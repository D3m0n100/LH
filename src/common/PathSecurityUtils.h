#ifndef PATHSECURITYUTILS_H
#define PATHSECURITYUTILS_H

#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QCryptographicHash>
#include <QDirIterator>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

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

inline bool isLinkOrReparsePoint(const QString& path)
{
    if (QFileInfo(path).isSymLink()) return true;
#ifdef Q_OS_WIN
    const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()));
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return true;
#endif
    return false;
}

// Check lexical ancestors as well as canonical containment: an internal link
// pointing back into the project is still not a safe recursive mutation entry.
inline bool safeProjectMutation(const QString& root, const QString& path, bool deleting,
                                QString* reason = nullptr)
{
    auto reject = [reason](const QString& error) { if (reason) *reason = error; return false; };
    if (root.isEmpty() || path.isEmpty() || hasParentTraversal(path) || !pathWithinRoot(root, path))
        return reject(QStringLiteral("路径不在工程目录内，或包含父目录跳转"));
    const QString rootPath = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    QString current = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (deleting && comparablePath(root) == comparablePath(path))
        return reject(QStringLiteral("不能删除工程根目录"));
    if (!pathWithinRoot(rootPath, QFileInfo(current).absolutePath()) && current != rootPath)
        return reject(QStringLiteral("路径的父目录不在工程内"));
    for (;;) {
        if (isLinkOrReparsePoint(current)) return reject(QStringLiteral("拒绝修改链接或 reparse point: %1").arg(current));
        if (current == rootPath) break;
        const QString parent = QFileInfo(current).absolutePath();
        if (parent == current) return reject(QStringLiteral("路径未到达工程根目录"));
        current = parent;
    }
    if (deleting && QFileInfo(path).isDir()) {
        QDirIterator it(path, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString entry = it.next();
            if (isLinkOrReparsePoint(entry) || !pathWithinRoot(root, entry))
                return reject(QStringLiteral("待删除目录含链接或越界条目: %1").arg(entry));
        }
    }
    return true;
}

} // namespace PathSecurityUtils

#endif // PATHSECURITYUTILS_H
