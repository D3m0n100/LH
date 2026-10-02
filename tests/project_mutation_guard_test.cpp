#include <QtTest>
#include <QTemporaryDir>
#include <QProcess>
#include "common/ProjectMutationGuard.h"
#include <winioctl.h>
#include "unc_test_fixture.h"

class ProjectMutationGuardTest : public QObject {
    Q_OBJECT
    static QString localFixture(const QString& uncPath) {
        const QString localRoot = qEnvironmentVariable("LH_TEST_UNC_LOCAL_ROOT");
        if (localRoot.isEmpty()) return {};
        const QString relative = QDir(qEnvironmentVariable("LH_TEST_UNC_ROOT")).relativeFilePath(uncPath);
        if (QDir::isAbsolutePath(relative) || PathSecurityUtils::hasParentTraversal(relative)) return {};
        return QDir(localRoot).filePath(relative);
    }
    static QByteArray contents(const QString& path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }
private slots:
    void uncCreateAtomicSaveRenameAndDelete() {
        if (qEnvironmentVariableIsEmpty("LH_TEST_UNC_ROOT")) QSKIP("Set LH_TEST_UNC_ROOT to a writable SMB test directory");
        UncTestDirectory temp(qEnvironmentVariable("LH_TEST_UNC_ROOT")); QVERIFY2(temp.isValid(), qPrintable(temp.errorString()));
        QVERIFY(temp.path().startsWith("//"));
        const QString root = temp.filePath(QStringLiteral("工程"));
        QVERIFY(QDir().mkpath(root));
        const QString path = QDir(root).filePath(QStringLiteral("数据.lh"));
        QString error;
        { ProjectMutationGuard guard;
          QVERIFY2(guard.acquire(root, path, false, &error), qPrintable(error));
          QVERIFY2(guard.create(false, "original"), qPrintable(error)); }
        { ProjectMutationGuard guard;
          QVERIFY2(guard.acquire(root, path, false, &error), qPrintable(error));
          QVERIFY(!guard.create(false, "overwrite")); }
        QCOMPARE(contents(path), QByteArray("original"));
        { ProjectMutationGuard guard;
          QVERIFY2(guard.acquire(root, path, false, &error), qPrintable(error));
          QVERIFY2(guard.writeAtomically("replacement"), qPrintable(error)); }
        QCOMPARE(contents(path), QByteArray("replacement"));
        { ProjectMutationGuard guard;
          QVERIFY2(guard.acquire(root, path, true, &error), qPrintable(error));
          QVERIFY2(guard.renameLeaf(QStringLiteral("新名称.lh")), qPrintable(error)); }
        QVERIFY(!QFileInfo::exists(path));
        QCOMPARE(contents(QDir(root).filePath(QStringLiteral("新名称.lh"))), QByteArray("replacement"));
        const QString folder = QDir(root).filePath("folder");
        { ProjectMutationGuard guard;
          QVERIFY2(guard.acquire(root, folder, false, &error), qPrintable(error));
          QVERIFY2(guard.create(true), qPrintable(error)); }
        { ProjectMutationGuard guard;
          QVERIFY2(guard.acquire(root, folder + "/child", false, &error), qPrintable(error));
          QVERIFY2(guard.create(false, "child-data"), qPrintable(error)); }
        { ProjectMutationGuard guard;
          QVERIFY2(guard.acquire(root, folder, true, &error), qPrintable(error));
          QVERIFY2(guard.renameLeaf(".staged"), qPrintable(error));
          QVERIFY2(guard.lockForDeletion(), qPrintable(error));
          QVERIFY2(guard.removeTree(), qPrintable(error)); }
        QVERIFY(!QFileInfo::exists(folder));
        QVERIFY(!QFileInfo::exists(QDir(root).filePath(".staged")));
        QCOMPARE(QDir(root).entryList({".lh-guard-*", ".lh-write-*"}, QDir::Files | QDir::Hidden).size(), 0);
    }
    void uncConflictAndContainmentPreserveOriginal() {
        if (qEnvironmentVariableIsEmpty("LH_TEST_UNC_ROOT")) QSKIP("Set LH_TEST_UNC_ROOT to a writable SMB test directory");
        UncTestDirectory temp(qEnvironmentVariable("LH_TEST_UNC_ROOT")); QVERIFY2(temp.isValid(), qPrintable(temp.errorString()));
        const QString root = temp.filePath("project"), parent = root + "/sub", path = parent + "/entry";
        QVERIFY(QDir().mkpath(parent));
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("preserved"); file.close();
        QString error;
        { ProjectMutationGuard guard;
          QVERIFY2(guard.acquire(root, path, true, &error), qPrintable(error));
          QVERIFY(!QDir().rename(parent, root + "/moved"));
          QVERIFY(!QFile::remove(path));
          QVERIFY2(guard.renameLeaf("staged"), qPrintable(error));
          QVERIFY2(guard.renameLeaf("entry"), qPrintable(error)); }
        QVERIFY(file.open(QIODevice::ReadOnly));
        { ProjectMutationGuard guard;
          QVERIFY(!guard.acquire(root, path, true, &error));
          QVERIFY(!guard.acquire(root, parent + "/retry", false, &error)); }
        { ProjectMutationGuard guard;
          QVERIFY2(guard.acquire(root, path, false, &error), qPrintable(error));
          QVERIFY(!guard.writeAtomically("must-not-publish")); }
        file.close(); QCOMPARE(contents(path), QByteArray("preserved"));
        QCOMPARE(QDir(parent).entryList({".lh-write-*", ".lh-guard-*"}, QDir::Files | QDir::Hidden).size(), 0);
        for (const auto& target : {temp.filePath("outside"), root + "/../outside", root,
                                  root + "/NUL.txt", root + "/bad:stream"}) {
            ProjectMutationGuard guard; QVERIFY(!guard.acquire(root, target, false, &error));
        }
        const QString deviceRoot = QStringLiteral("//?/UNC/") + root.mid(2);
        ProjectMutationGuard device;
        QVERIFY(!device.acquire(deviceRoot, deviceRoot + "/new", false, &error));
        QVERIFY(!QFileInfo::exists(temp.filePath("outside")));
    }
    void uncRejectsServerJunctionAndOutsideWrite() {
        if (qEnvironmentVariableIsEmpty("LH_TEST_UNC_ROOT") || qEnvironmentVariableIsEmpty("LH_TEST_UNC_LOCAL_ROOT"))
            QSKIP("Local backing directory is required for the SMB server-side junction attack");
        UncTestDirectory temp(qEnvironmentVariable("LH_TEST_UNC_ROOT")); QVERIFY2(temp.isValid(), qPrintable(temp.errorString()));
        const QString local = localFixture(temp.path()); QVERIFY(!local.isEmpty());
        const QString root = temp.filePath("project"), outside = temp.filePath("outside"), link = root + "/link";
        QVERIFY(QDir().mkpath(root)); QVERIFY(QDir().mkpath(outside));
        QFile sentinel(outside + "/sentinel"); QVERIFY(sentinel.open(QIODevice::WriteOnly)); sentinel.write("outside"); sentinel.close();
        QCOMPARE(contents(local + "/outside/sentinel"), QByteArray("outside"));
        QProcess process;
        process.start("cmd.exe", {"/c", "mklink", "/J", QDir::toNativeSeparators(local + "/project/link"), QDir::toNativeSeparators(local + "/outside")});
        QVERIFY(process.waitForFinished()); QCOMPARE(process.exitCode(), 0);
        QString error;
        bool acquired;
        { ProjectMutationGuard guard; acquired = guard.acquire(root, link + "/new", false, &error); }
        const bool removed = RemoveDirectoryW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(local + "/project/link").utf16()));
        QVERIFY(removed); QVERIFY2(!acquired, qPrintable(error));
        QCOMPARE(contents(outside + "/sentinel"), QByteArray("outside"));
        QVERIFY(!QFileInfo::exists(outside + "/new"));
    }
    void createsWithoutOverwriteAndRejectsInvalidNames() {
        QTemporaryDir temp;
        QString error;
        const auto root = temp.path();
        { ProjectMutationGuard guard;
          QVERIFY2(guard.acquire(root, temp.filePath("new.txt"), false, &error), qPrintable(error));
          QVERIFY2(guard.create(false, "original"), qPrintable(error)); }
        { ProjectMutationGuard guard;
          QVERIFY(guard.acquire(root, temp.filePath("new.txt"), false, &error));
          QVERIFY(!guard.create(false, "replacement")); }
        QFile file(temp.filePath("new.txt")); QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("original")); file.close();
        for (const auto& name : {"..", "bad:stream", "bad.", "bad "}) {
            ProjectMutationGuard guard;
            QVERIFY(!guard.acquire(root, temp.filePath(name), false, &error));
        }
        ProjectMutationGuard folder;
        QVERIFY(folder.acquire(root, temp.filePath("folder"), false, &error));
        QVERIFY2(folder.create(true), qPrintable(error));
    }
    void holdsAncestorsAndTargetAcrossRenameRollback() {
        QTemporaryDir temp;
        const auto parent = temp.filePath("project/sub");
        QVERIFY(QDir().mkpath(parent));
        QFile file(parent + "/entry"); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("safe"); file.close();
        QString error;
        ProjectMutationGuard guard;
        QVERIFY2(guard.acquire(temp.filePath("project"), file.fileName(), true, &error), qPrintable(error));
        QVERIFY(!QDir().rename(parent, temp.filePath("replaced")));
        QVERIFY(!QFile::remove(file.fileName()));
        QVERIFY2(guard.renameLeaf(".staged"), qPrintable(error));
        QVERIFY(!QFile::remove(parent + "/.staged"));
        QVERIFY2(guard.renameLeaf("entry"), qPrintable(error));
        QVERIFY(QFileInfo::exists(file.fileName()));
        QVERIFY2(guard.renameLeaf(".staged"), qPrintable(error));
        QVERIFY2(guard.removeTree(), qPrintable(error));
        QVERIFY(!QFileInfo::exists(parent + "/.staged"));
    }
    void removesPinnedTreeButRejectsConcurrentNewEntry() {
        QTemporaryDir temp;
        const auto tree = temp.filePath("tree");
        QVERIFY(QDir().mkpath(tree + "/child"));
        QFile file(tree + "/child/value"); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("value"); file.close();
        QString error;
        ProjectMutationGuard guard;
        QVERIFY2(guard.acquire(temp.path(), tree, true, &error), qPrintable(error));
        QVERIFY2(guard.renameLeaf(".staged"), qPrintable(error));
        QVERIFY2(guard.lockForDeletion(), qPrintable(error));
        QFile concurrent(temp.filePath(".staged/new")); QVERIFY(concurrent.open(QIODevice::WriteOnly)); concurrent.close();
        QVERIFY(!guard.removeTree());
        QVERIFY(QFileInfo::exists(concurrent.fileName()));
    }
    void rejectsRealJunctionAndPreservesOutsideSentinel() {
        QTemporaryDir temp;
        const auto root = temp.filePath("project");
        const auto outside = temp.filePath("outside");
        const auto link = root + "/link";
        QVERIFY(QDir().mkpath(root)); QVERIFY(QDir().mkpath(outside));
        QFile sentinel(outside + "/sentinel"); QVERIFY(sentinel.open(QIODevice::WriteOnly)); sentinel.write("outside"); sentinel.close();
        QProcess process;
        process.start("cmd.exe", {"/c", "mklink", "/J", QDir::toNativeSeparators(link), QDir::toNativeSeparators(outside)});
        QVERIFY(process.waitForFinished()); QCOMPARE(process.exitCode(), 0);
        QString error;
        { ProjectMutationGuard guard; QVERIFY(!guard.acquire(root, link + "/new", false, &error)); }
        { ProjectMutationGuard guard; QVERIFY(!guard.acquire(temp.path(), root, true, &error)); }
        QVERIFY(sentinel.open(QIODevice::ReadOnly)); QCOMPARE(sentinel.readAll(), QByteArray("outside")); sentinel.close();
        QVERIFY(RemoveDirectoryW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(link).utf16())));
    }
    void writeHandleConflictFailsBeforeStaging() {
        QTemporaryDir temp;
        QFile file(temp.filePath("entry")); QVERIFY(file.open(QIODevice::WriteOnly));
        QString error;
        ProjectMutationGuard guard;
        QVERIFY(!guard.acquire(temp.path(), file.fileName(), true, &error));
        QVERIFY(QFileInfo::exists(file.fileName()));
    }
    void inPlaceAncestorReparseCannotRedirectCreation_data() {
        QTest::addColumn<bool>("atomicWrite");
        QTest::addColumn<bool>("unc");
        QTest::addColumn<bool>("attributesOnly");
        QTest::newRow("new-file") << false << false << false;
        QTest::newRow("atomic-config-replacement") << true << false << false;
        QTest::newRow("unc-new-file") << false << true << false;
        QTest::newRow("unc-atomic-config-replacement") << true << true << false;
        QTest::newRow("attributes-new-file") << false << false << true;
        QTest::newRow("attributes-atomic-config") << true << false << true;
        QTest::newRow("unc-attributes-new-file") << false << true << true;
        QTest::newRow("unc-attributes-atomic-config") << true << true << true;
    }
    void inPlaceAncestorReparseCannotRedirectCreation() {
        QFETCH(bool,atomicWrite);
        QFETCH(bool,unc);
        QFETCH(bool,attributesOnly);
        if (unc && (qEnvironmentVariableIsEmpty("LH_TEST_UNC_ROOT") || qEnvironmentVariableIsEmpty("LH_TEST_UNC_LOCAL_ROOT")))
            QSKIP("Local backing directory is required for the SMB server-side in-place reparse attack");
        QTemporaryDir temp;
        UncTestDirectory uncTemp(unc ? qEnvironmentVariable("LH_TEST_UNC_ROOT") : QString());
        if (unc) QVERIFY2(uncTemp.isValid(),qPrintable(uncTemp.errorString()));
        const QString base=unc ? uncTemp.path() : temp.path();
        const auto root=QDir(base).filePath("project"); const auto parent=root+"/empty"; const auto outside=QDir(base).filePath("outside");
        const QString attackParent=unc ? localFixture(base)+"/project/empty" : parent;
        const QString attackOutside=unc ? localFixture(base)+"/outside" : outside;
        QVERIFY(QDir().mkpath(parent)); QVERIFY(QDir().mkpath(outside));
        QFile sentinel(outside+"/sentinel"); QVERIFY(sentinel.open(QIODevice::WriteOnly)); sentinel.write("outside"); sentinel.close();
        if(atomicWrite) { QFile destination(outside+"/new.txt"); QVERIFY(destination.open(QIODevice::WriteOnly)); destination.write("preserved-outside-config"); }
        QString error;
        {
            ProjectMutationGuard guard;
            QVERIFY2(guard.acquire(root,parent+"/new.txt",false,&error),qPrintable(error));
            if(unc) QCOMPARE(contents(attackOutside+"/sentinel"),QByteArray("outside"));
            const auto native=QDir::toNativeSeparators(attackParent);
            HANDLE attacker=CreateFileW(reinterpret_cast<LPCWSTR>(native.utf16()),attributesOnly ? FILE_WRITE_ATTRIBUTES : GENERIC_WRITE,
                    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
            bool attackBlocked=attacker==INVALID_HANDLE_VALUE;
            if(attackBlocked) {
                const DWORD error=GetLastError();
                QVERIFY(unc);
                QCOMPARE(error,DWORD(ERROR_SHARING_VIOLATION));
            } else {
                const auto substitute=QStringLiteral("\\??\\")+QDir::toNativeSeparators(attackOutside);
                const auto print=QDir::toNativeSeparators(attackOutside);
                struct MountPoint { DWORD tag; WORD dataLength,reserved,subOffset,subLength,printOffset,printLength; WCHAR names[2048]; } data{};
                data.tag=IO_REPARSE_TAG_MOUNT_POINT; data.subLength=WORD(substitute.size()*2); data.printOffset=data.subLength+2;
                data.printLength=WORD(print.size()*2); data.dataLength=WORD(8+data.subLength+2+data.printLength+2);
                memcpy(data.names,substitute.utf16(),data.subLength);
                memcpy(reinterpret_cast<char*>(data.names)+data.printOffset,print.utf16(),data.printLength);
                DWORD returned=0;
                const bool installed=DeviceIoControl(attacker,FSCTL_SET_REPARSE_POINT,&data,data.dataLength+8,nullptr,0,&returned,nullptr);
                const DWORD nativeError=GetLastError(); CloseHandle(attacker);
                if(unc && !installed) {
                    QCOMPARE(nativeError,DWORD(ERROR_DIR_NOT_EMPTY));
                    attackBlocked=true;
                } else QVERIFY2(installed,qPrintable(QString("FSCTL_SET_REPARSE_POINT error %1").arg(nativeError)));
            }
            const bool created=atomicWrite ? guard.writeAtomically("inside-original-directory") : guard.create(false,"inside-original-directory");
            if(attackBlocked) QVERIFY2(created,qPrintable(error));
            if(created) QVERIFY2(guard.removeTree(),qPrintable(error));
            if(atomicWrite) {
                QFile destination(outside+"/new.txt"); QVERIFY(destination.open(QIODevice::ReadOnly));
                QCOMPARE(destination.readAll(),QByteArray("preserved-outside-config"));
            } else QVERIFY(!QFileInfo::exists(outside+"/new.txt"));
        }
        QVERIFY(sentinel.open(QIODevice::ReadOnly)); QCOMPARE(sentinel.readAll(),QByteArray("outside")); sentinel.close();
        QVERIFY(RemoveDirectoryW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(attackParent).utf16())));
    }
};
QTEST_GUILESS_MAIN(ProjectMutationGuardTest)
#include "project_mutation_guard_test.moc"
