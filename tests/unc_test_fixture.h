#pragma once

#include <QDir>
#include <QUuid>
#include <QString>
#include <QDebug>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

// QTemporaryDir in the Qt 5.15 Windows runtime fails to create this UNC
// fixture. Use a unique, exclusively created directory on the actual share;
// every tested operation still goes through UNC, not a mapped local path.
class UncTestDirectory
{
public:
    explicit UncTestDirectory(const QString& root)
    {
#ifdef Q_OS_WIN
        const QString base = QDir::cleanPath(QDir::fromNativeSeparators(root));
        if (!base.startsWith("//") || base.startsWith("//?/") || base.startsWith("//./")) {
            m_error = QStringLiteral("UNC test root is required");
            return;
        }
        const QString name = QStringLiteral("lh-unc-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString candidate = QDir(base).filePath(name);
        const QString native = QDir::toNativeSeparators(candidate);
        if (!CreateDirectoryW(reinterpret_cast<LPCWSTR>(native.utf16()), nullptr)) {
            m_error = QStringLiteral("CreateDirectoryW failed: %1").arg(GetLastError());
            return;
        }
        m_path = candidate;
#else
        Q_UNUSED(root)
        m_error = QStringLiteral("Windows UNC fixture only");
#endif
    }
    UncTestDirectory(const UncTestDirectory&) = delete;
    UncTestDirectory& operator=(const UncTestDirectory&) = delete;
    ~UncTestDirectory() {
        if (!m_path.isEmpty() && !QDir(m_path).removeRecursively())
            qWarning() << "Cannot clean owned UNC test fixture:" << m_path;
    }
    bool isValid() const { return !m_path.isEmpty(); }
    QString path() const { return m_path; }
    QString filePath(const QString& name) const { return QDir(m_path).filePath(name); }
    QString errorString() const { return m_error; }
private:
    QString m_path;
    QString m_error;
};
