#pragma once

#include "PathSecurityUtils.h"
#include <vector>
#include <cstring>
#include <cstddef>
#include <QUuid>
#ifdef Q_OS_WIN
#include <winternl.h>
#endif

// A mutation owns its parent directories until completion. On Windows every
// descendant is opened relative to an existing handle, never through a checked
// pathname. Rename and deletion operate on those same handles.
class ProjectMutationGuard
{
public:
    ProjectMutationGuard() = default;
    ProjectMutationGuard(const ProjectMutationGuard&) = delete;
    ProjectMutationGuard& operator=(const ProjectMutationGuard&) = delete;
    ~ProjectMutationGuard() {
#ifdef Q_OS_WIN
        for (auto it = m_tree.rbegin(); it != m_tree.rend(); ++it) close(*it);
        close(m_guardMarker);
        for (auto it = m_parents.rbegin(); it != m_parents.rend(); ++it) close(*it);
#endif
    }

    bool acquire(const QString& root, const QString& path, bool existing, QString* error)
    {
        m_error = error;
        if (m_attempted) return fail(QStringLiteral("操作句柄不能重复使用"));
        m_attempted = true;
#ifdef Q_OS_WIN
        // Reject device/extended namespaces before QDir can normalize their
        // special components. All accepted UNC paths name a server and share.
        for (const auto& input : {root, path}) {
            const QString normalized = QDir::fromNativeSeparators(input);
            if (normalized.startsWith(QStringLiteral("//?/"))
                    || normalized.startsWith(QStringLiteral("//./")))
                return fail(QStringLiteral("工程修改不接受设备或扩展命名空间路径"));
        }
#endif
        const QString rootPath = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
        const QString absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        const QString relative = QDir(rootPath).relativeFilePath(absolute);
        if (root.isEmpty() || relative == "." || relative == ".."
                || relative.startsWith("../") || QDir::isAbsolutePath(relative)
                || PathSecurityUtils::hasParentTraversal(path))
            return fail(QStringLiteral("路径必须位于工程根目录之下"));
#ifdef Q_OS_WIN
        QString anchorPath;
        QStringList components;
        if (absolute.startsWith(QStringLiteral("//"))) {
            const auto unc = absolute.mid(2).split('/', Qt::KeepEmptyParts);
            if (unc.size() < 3 || !validName(unc.at(0)) || !validName(unc.at(1)))
                return fail(QStringLiteral("UNC 路径必须包含服务器、共享名及工程内目标"));
            anchorPath = QStringLiteral("//") + unc.at(0) + '/' + unc.at(1) + '/';
            components = unc.mid(2);
        } else if (absolute.size() >= 3 && absolute.at(1) == ':' && absolute.at(2) == '/'
                && ((absolute.at(0) >= 'A' && absolute.at(0) <= 'Z')
                    || (absolute.at(0) >= 'a' && absolute.at(0) <= 'z'))) {
            anchorPath = absolute.left(3);
            components = absolute.mid(3).split('/', Qt::SkipEmptyParts);
        } else {
            return fail(QStringLiteral("工程修改仅支持 Windows 磁盘路径或 UNC 共享路径"));
        }
        if (components.isEmpty()) return fail(QStringLiteral("不能修改磁盘或共享根目录"));
        for (const auto& component : components)
            if (!validName(component)) return fail(QStringLiteral("路径含不安全的文件名"));
        const QString nativeAnchor = QDir::toNativeSeparators(anchorPath);
        // SMB reconstructs a pathname for a relative rename. The marker below
        // prevents converting the last parent into a reparse point, including
        // through metadata-only handles unaffected by write-sharing denial.
        m_networkPath = absolute.startsWith(QStringLiteral("//"))
                || GetDriveTypeW(reinterpret_cast<LPCWSTR>(nativeAnchor.utf16())) == DRIVE_REMOTE;
        HANDLE anchor = CreateFileW(reinterpret_cast<LPCWSTR>(nativeAnchor.utf16()),
                FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (anchor == INVALID_HANDLE_VALUE) return winFail();
        BY_HANDLE_FILE_INFORMATION anchorInfo{};
        if (GetFileType(anchor) != FILE_TYPE_DISK
                || !GetFileInformationByHandle(anchor, &anchorInfo)
                || !(anchorInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                || (anchorInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            CloseHandle(anchor);
            return fail(QStringLiteral("拒绝非文件系统、链接或无安全目录句柄的磁盘/共享根目录"));
        }
        m_parents.push_back(anchor);
        for (int i = 0; i + 1 < components.size(); ++i) {
            HANDLE parent = openRelative(m_parents.back(), components.at(i), true, false, false);
            if (parent == INVALID_HANDLE_VALUE) return false;
            m_parents.push_back(parent);
        }
        m_leaf = components.last();
        if (m_networkPath) {
            // Every earlier ancestor contains the next pinned directory. Keep
            // the immediate parent nonempty too: Windows rejects installing a
            // directory reparse point while it has entries. Share denial alone
            // does not exclude FILE_WRITE_ATTRIBUTES handles. The marker is
            // opened relative to the parent, exclusively, and delete-on-close.
            const QString markerName = QStringLiteral(".lh-guard-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
            m_guardMarker = openRelative(m_parents.back(), markerName,
                    false, true, true, true);
            if (m_guardMarker == INVALID_HANDLE_VALUE) return false;
            // Verify physical membership after pinning the whole chain. This
            // also detects a redirect during acquisition, before any target
            // content is created, replaced, renamed or removed. Each parent
            // query uses a literal-name filter rather than scanning a share.
            for (int i = 0; i + 1 < int(m_parents.size()); ++i)
                if (!verifyChild(m_parents.at(i), components.at(i), m_parents.at(i + 1))) return false;
            if (!verifyChild(m_parents.back(), markerName, m_guardMarker)) return false;
        }
        if (existing) {
            HANDLE target = openRelative(m_parents.back(), m_leaf, false, false, true);
            if (target == INVALID_HANDLE_VALUE) return false;
            m_tree.push_back(target);
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(target, &info)) return winFail();
            m_directory = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            if (m_directory && !lockChildren(target, 0)) return false;
            // Windows refuses renaming directories with pinned descendants.
            // Validate first; pin the tree again after staging by handle.
            releaseChildren();
            m_treeLocked = !m_directory;
        }
        m_acquired = true;
        return true;
#else
        Q_UNUSED(existing)
        return fail(QStringLiteral("此平台尚未提供句柄保护的工程修改，已安全拒绝"));
#endif
    }

    bool isDirectory() const { return m_directory; }
    bool lockForDeletion() {
#ifdef Q_OS_WIN
        if (!m_acquired || m_tree.empty()) return false;
        if (m_treeLocked) return true;
        releaseChildren();
        m_treeLocked = lockChildren(m_tree.front(), 0);
        if (!m_treeLocked) releaseChildren();
        return m_treeLocked;
#else
        return false;
#endif
    }
    bool renameLeaf(const QString& name, bool replaceExisting = false) {
#ifdef Q_OS_WIN
        if (!m_acquired || m_tree.empty() || !validName(name)) return fail(QStringLiteral("无效的暂存操作"));
        releaseChildren();
        m_treeLocked = !m_directory;
        const DWORD bytes = DWORD(name.size() * sizeof(wchar_t));
        std::vector<quint64> storage((offsetof(FILE_RENAME_INFO, FileName) + bytes + 7) / 8);
        auto* info = reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
        info->ReplaceIfExists = replaceExisting;
        info->RootDirectory = m_parents.back();
        info->FileNameLength = bytes;
        memcpy(info->FileName, name.utf16(), bytes);
        using Rename = NTSTATUS (NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);
        static const auto nativeRename = nativeFunction<Rename>("NtSetInformationFile");
        if (!nativeRename) return fail(QStringLiteral("无法获得安全重命名接口"));
        IO_STATUS_BLOCK result{};
        const NTSTATUS status = nativeRename(m_tree.front(), &result, info,
                DWORD(offsetof(FILE_RENAME_INFO, FileName) + bytes), FileRenameInformation);
        if (status < 0) return fail(QStringLiteral("安全重命名失败 (NTSTATUS 0x%1)").arg(quint32(status), 8, 16, QLatin1Char('0')));
        return true;
#else
        Q_UNUSED(name)
        Q_UNUSED(replaceExisting)
        return false;
#endif
    }
    bool removeTree() {
#ifdef Q_OS_WIN
        if (!m_acquired || m_tree.empty() || !m_treeLocked) return false;
        FILE_DISPOSITION_INFO disposition{TRUE};
        for (auto it = m_tree.rbegin(); it != m_tree.rend(); ++it) {
            if (!SetFileInformationByHandle(*it, FileDispositionInfo, &disposition, sizeof(disposition))) return winFail();
            close(*it);
        }
        m_tree.clear();
        return true;
#else
        return false;
#endif
    }
    bool create(bool directory, const QByteArray& contents = {}) {
#ifdef Q_OS_WIN
        if (!m_acquired || !m_tree.empty()) return false;
        HANDLE leaf = openRelative(m_parents.back(), m_leaf, directory, true, true);
        if (leaf == INVALID_HANDLE_VALUE) return false;
        m_tree.push_back(leaf);
        m_treeLocked = !directory;
        if (!directory) {
            DWORD written = 0;
            if (!WriteFile(leaf, contents.constData(), DWORD(contents.size()), &written, nullptr)
                    || written != DWORD(contents.size()) || !FlushFileBuffers(leaf)) {
                winFail();
                // Failure cleanup refers to the new object, even if its name changed.
                FILE_DISPOSITION_INFO disposition{TRUE};
                if (!SetFileInformationByHandle(leaf, FileDispositionInfo, &disposition, sizeof(disposition))
                        && m_error) *m_error += QStringLiteral("；无法清理新建内容，请检查目标目录");
                return false;
            }
        }
        return true;
#else
        Q_UNUSED(directory)
        Q_UNUSED(contents)
        return false;
#endif
    }
    bool writeAtomically(const QByteArray& contents) {
#ifdef Q_OS_WIN
        if (!m_acquired || !m_tree.empty()) return false;
        const QString destination = m_leaf;
        m_leaf = QStringLiteral(".lh-write-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        if (!create(false, contents)) return false;
        if (!renameLeaf(destination, true)) {
            const QString renameError = m_error ? *m_error : QString();
            if (!removeTree() && m_error) *m_error = renameError + QStringLiteral("；暂存配置清理失败");
            else if (m_error) *m_error = renameError;
            return false;
        }
        m_leaf = destination;
        return true;
#else
        Q_UNUSED(contents)
        return false;
#endif
    }

private:
    bool fail(const QString& message) { if (m_error) *m_error = message; return false; }
    QString* m_error = nullptr;
    bool m_attempted = false;
    bool m_acquired = false;
    bool m_directory = false;
    bool m_treeLocked = false;
#ifdef Q_OS_WIN
    QString m_leaf;
    bool m_networkPath = false;
    HANDLE m_guardMarker = INVALID_HANDLE_VALUE;
    std::vector<HANDLE> m_parents;
    std::vector<HANDLE> m_tree;
    template<typename Function> static Function nativeFunction(const char* name) {
        const auto address=GetProcAddress(GetModuleHandleW(L"ntdll.dll"),name);
        Function function=nullptr;
        static_assert(sizeof(function)==sizeof(address),"Windows function pointer size");
        std::memcpy(&function,&address,sizeof(function));
        return function;
    }
    void releaseChildren() {
        while (m_tree.size() > 1) { close(m_tree.back()); m_tree.pop_back(); }
    }
    static void close(HANDLE& handle) {
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
    bool verifyChild(HANDLE parent, const QString& name, HANDLE child) {
        BY_HANDLE_FILE_INFORMATION parentInfo{}, childInfo{};
        if (!GetFileInformationByHandle(parent, &parentInfo)
                || !GetFileInformationByHandle(child, &childInfo)
                || !(parentInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                || (parentInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                || (childInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
            return fail(QStringLiteral("共享目录身份已变化或无法核实，安全操作已拒绝"));
        using Query = NTSTATUS (NTAPI*)(HANDLE, HANDLE, PVOID, PVOID, PIO_STATUS_BLOCK,
                PVOID, ULONG, FILE_INFORMATION_CLASS, BOOLEAN, PUNICODE_STRING, BOOLEAN);
        static const auto query = nativeFunction<Query>("NtQueryDirectoryFile");
        if (!query) return fail(QStringLiteral("无法获得共享目录身份查询接口"));
        UNICODE_STRING filter{};
        filter.Length = USHORT(name.size() * sizeof(wchar_t));
        filter.MaximumLength = filter.Length;
        filter.Buffer = reinterpret_cast<PWSTR>(const_cast<ushort*>(name.utf16()));
        std::vector<quint64> storage(65536 / sizeof(quint64));
        IO_STATUS_BLOCK result{};
        // FileIdBothDirectoryInformation = 37; layout matches the Win32 DTO.
        const NTSTATUS status = query(parent, nullptr, nullptr, nullptr, &result,
                storage.data(), 65536, static_cast<FILE_INFORMATION_CLASS>(37), TRUE, &filter, TRUE);
        if (status != 0 || result.Information < offsetof(FILE_ID_BOTH_DIR_INFO, FileName)
                || result.Information > 65536)
            return fail(QStringLiteral("服务器未提供可靠的目录身份，安全操作已拒绝 (NTSTATUS 0x%1)")
                    .arg(quint32(status), 8, 16, QLatin1Char('0')));
        const auto* entry = reinterpret_cast<const FILE_ID_BOTH_DIR_INFO*>(storage.data());
        if (entry->FileNameLength % sizeof(wchar_t)
                || entry->FileNameLength > result.Information - offsetof(FILE_ID_BOTH_DIR_INFO, FileName))
            return fail(QStringLiteral("服务器返回了无效的目录身份数据"));
        const QString actualName = QString::fromWCharArray(entry->FileName, int(entry->FileNameLength / sizeof(wchar_t)));
        const quint64 childId = (quint64(childInfo.nFileIndexHigh) << 32) | childInfo.nFileIndexLow;
        if (actualName.compare(name, Qt::CaseInsensitive) != 0 || !childId
                || quint64(entry->FileId.QuadPart) != childId
                || childInfo.dwVolumeSerialNumber != parentInfo.dwVolumeSerialNumber)
            return fail(QStringLiteral("共享目录子项身份不匹配，安全操作已拒绝"));
        return true;
    }
    bool winFail() { return fail(QStringLiteral("安全文件操作失败 (Windows %1)，可能存在占用或目录竞争").arg(GetLastError())); }
    static bool validName(const QString& name) {
        for (const auto character : name)
            if (character.unicode() < 32 || QStringLiteral("<>\"|?*").contains(character)) return false;
        const QString base = name.section('.', 0, 0).trimmed().toUpper();
        if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL"
                || base == "CONIN$" || base == "CONOUT$") return false;
        if (base.size() == 4 && (base.startsWith("COM") || base.startsWith("LPT"))
                && (QStringLiteral("123456789\u00b9\u00b2\u00b3").contains(base.at(3)))) return false;
        return !name.isEmpty() && name != "." && name != ".." && !name.contains('/')
                && !name.contains('\\') && !name.contains(':') && !name.contains(QChar(0))
                && !name.endsWith('.') && !name.endsWith(' ');
    }
    HANDLE openRelative(HANDLE parent, const QString& name, bool directory, bool create, bool deleting,
                        bool deleteOnClose = false) {
        using Open = NTSTATUS (NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES,
                PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
        static const auto nativeOpen = nativeFunction<Open>("NtCreateFile");
        if (!nativeOpen || !validName(name) || name.size() > 32767) {
            fail(QStringLiteral("无法获得安全文件操作接口或文件名无效")); return INVALID_HANDLE_VALUE;
        }
        UNICODE_STRING unicode{};
        unicode.Length = USHORT(name.size() * sizeof(wchar_t));
        unicode.MaximumLength = unicode.Length;
        unicode.Buffer = reinterpret_cast<PWSTR>(const_cast<ushort*>(name.utf16()));
        OBJECT_ATTRIBUTES attributes{};
        attributes.Length = sizeof(attributes);
        attributes.RootDirectory = parent;
        attributes.ObjectName = &unicode;
        attributes.Attributes = 0x40 | 0x1000; // OBJ_CASE_INSENSITIVE | OBJ_DONT_REPARSE
        IO_STATUS_BLOCK result{};
        HANDLE handle = INVALID_HANDLE_VALUE;
        ACCESS_MASK access = FILE_READ_ATTRIBUTES | SYNCHRONIZE;
        // List access is also valid on files (FILE_READ_DATA).
        access |= FILE_LIST_DIRECTORY;
        if (deleting) access |= DELETE;
        if (create && !directory) access |= FILE_WRITE_DATA;
        const NTSTATUS status = nativeOpen(&handle, access, &attributes, &result, nullptr,
                deleteOnClose ? FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY : FILE_ATTRIBUTE_NORMAL,
                FILE_SHARE_READ | (deleting ? 0 : FILE_SHARE_WRITE), create ? 2 : 1,
                0x00200000 | 0x20 | (directory ? 1 : 0) | (deleteOnClose ? 0x1000 : 0), nullptr, 0);
        if (status < 0) {
            fail(QStringLiteral("拒绝路径竞争、链接或占用 (NTSTATUS 0x%1)").arg(quint32(status), 8, 16, QLatin1Char('0')));
            return INVALID_HANDLE_VALUE;
        }
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(handle, &info)
                || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                || (directory && !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))) {
            CloseHandle(handle);
            fail(QStringLiteral("拒绝链接、reparse point 或非目录父路径"));
            return INVALID_HANDLE_VALUE;
        }
        return handle;
    }
    bool lockChildren(HANDLE directory, int depth) {
        if (depth > 256 || m_tree.size() > 65536) return fail(QStringLiteral("目录过深或条目过多，拒绝整体删除"));
        std::vector<quint64> storage(65536 / sizeof(quint64));
        auto* buffer = reinterpret_cast<char*>(storage.data());
        bool first = true;
        for (;;) {
            if (!GetFileInformationByHandleEx(directory,
                    first ? FileIdBothDirectoryRestartInfo : FileIdBothDirectoryInfo, buffer, 65536)) {
                if (GetLastError() == ERROR_NO_MORE_FILES) return true;
                return winFail();
            }
            first = false;
            auto* entry = reinterpret_cast<FILE_ID_BOTH_DIR_INFO*>(buffer);
            for (;;) {
                const QString name = QString::fromWCharArray(entry->FileName, int(entry->FileNameLength / sizeof(wchar_t)));
                if (name != "." && name != "..") {
                    HANDLE child = openRelative(directory, name, false, false, true);
                    if (child == INVALID_HANDLE_VALUE) return false;
                    m_tree.push_back(child);
                    if (m_tree.size() > 65536) return fail(QStringLiteral("条目过多，拒绝整体删除"));
                    BY_HANDLE_FILE_INFORMATION info{};
                    if (!GetFileInformationByHandle(child, &info)) return winFail();
                    if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !lockChildren(child, depth + 1)) return false;
                }
                if (!entry->NextEntryOffset) break;
                entry = reinterpret_cast<FILE_ID_BOTH_DIR_INFO*>(reinterpret_cast<char*>(entry) + entry->NextEntryOffset);
            }
        }
    }
#endif
};
