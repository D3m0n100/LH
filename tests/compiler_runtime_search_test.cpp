/**
 * @file compiler_runtime_search_test.cpp
 * @brief T06: 开发与安装模式的编译器运行时搜索隔离测试
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include "compiler/DSLCompilerInterface.h"

class CompilerRuntimeSearchTest : public QObject
{
    Q_OBJECT

private:
    static bool touchFile(const QString& path)
    {
        QFileInfo fi(path);
        fi.dir().mkpath(QStringLiteral("."));
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    }

private slots:
    void init()
    {
        qunsetenv("LH_RUNTIME_MODE");
        DSLCompilerInterface::clearPythonInterpreterCache();
    }

    void cleanup()
    {
        qunsetenv("LH_RUNTIME_MODE");
        DSLCompilerInterface::clearPythonInterpreterCache();
    }

    void installedModeSelectsInstalledRuntimeOverSourceTree()
    {
        QTemporaryDir installDir;
        QVERIFY(installDir.isValid());
        const QString installBin = installDir.path() + QStringLiteral("/bin");
        const QString installRuntime = installDir.path() + QStringLiteral("/third_party/custom_dsp_language/compile");
        QVERIFY(touchFile(installBin + QStringLiteral("/.lh_install_marker")));
        QVERIFY(touchFile(installRuntime + QStringLiteral("/lmc.py")));

        QTemporaryDir sourceDir;
        QVERIFY(sourceDir.isValid());
        const QString sourceRuntime = sourceDir.path() + QStringLiteral("/third_party/custom_dsp_language/compile");
        QVERIFY(touchFile(sourceRuntime + QStringLiteral("/lmc.py")));

        CompilerRuntimeSearchContext ctx;
        ctx.appDirPath = installBin;
        ctx.sourceDirOverride = sourceDir.path();

        const CompilerRuntimeResolution res = DSLCompilerInterface::resolveCompilerRuntime(ctx);
        QVERIFY(res.success);
        QVERIFY(res.isInstalledMode);
        QCOMPARE(res.runtimeWorkingDir, QDir::cleanPath(installRuntime));
        QCOMPARE(res.entryScript, QDir::cleanPath(installRuntime + QStringLiteral("/lmc.py")));
    }

    void installedModeFailsWhenInstalledRuntimeMissingWithoutSourceFallback()
    {
        QTemporaryDir installDir;
        QVERIFY(installDir.isValid());
        const QString installBin = installDir.path() + QStringLiteral("/bin");
        // 注意：不创建 installRuntime/lmc.py，模拟安装目录受损或不完整
        QVERIFY(touchFile(installBin + QStringLiteral("/.lh_install_marker")));

        QTemporaryDir sourceDir;
        QVERIFY(sourceDir.isValid());
        const QString sourceRuntime = sourceDir.path() + QStringLiteral("/third_party/custom_dsp_language/compile");
        QVERIFY(touchFile(sourceRuntime + QStringLiteral("/lmc.py")));

        CompilerRuntimeSearchContext ctx;
        ctx.appDirPath = installBin;
        ctx.sourceDirOverride = sourceDir.path();

        const CompilerRuntimeResolution res = DSLCompilerInterface::resolveCompilerRuntime(ctx);
        QVERIFY(!res.success);
        QVERIFY(res.isInstalledMode);
        // 关键断言：严禁回退到源码目录！
        QVERIFY(!res.runtimeWorkingDir.contains(QDir::cleanPath(sourceDir.path())));
        QVERIFY(res.errorMessage.contains(QStringLiteral("Running in installed layout")));
        QVERIFY(res.errorMessage.contains(QStringLiteral("strictly prohibited")));
    }

    void developerModeFallsBackToSourceTreeWhenInstalledRuntimeMissing()
    {
        QTemporaryDir appDir;
        QVERIFY(appDir.isValid());
        const QString appBin = appDir.path() + QStringLiteral("/bin");
        QDir().mkpath(appBin);
        // 注意：没有 .lh_install_marker

        QTemporaryDir sourceDir;
        QVERIFY(sourceDir.isValid());
        const QString sourceRuntime = sourceDir.path() + QStringLiteral("/third_party/custom_dsp_language/compile");
        QVERIFY(touchFile(sourceRuntime + QStringLiteral("/lmc.py")));

        CompilerRuntimeSearchContext ctx;
        ctx.appDirPath = appBin;
        ctx.sourceDirOverride = sourceDir.path();

        const CompilerRuntimeResolution res = DSLCompilerInterface::resolveCompilerRuntime(ctx);
        QVERIFY(res.success);
        QVERIFY(!res.isInstalledMode);
        QCOMPARE(res.runtimeWorkingDir, QDir::cleanPath(sourceRuntime));
        QCOMPARE(res.entryScript, QDir::cleanPath(sourceRuntime + QStringLiteral("/lmc.py")));
    }

    void developerModeFailsWhenSourceTreeMissing()
    {
        QTemporaryDir appDir;
        QVERIFY(appDir.isValid());
        const QString appBin = appDir.path() + QStringLiteral("/bin");
        QDir().mkpath(appBin);

        QTemporaryDir sourceDir;
        QVERIFY(sourceDir.isValid());
        // 源码目录下不创建 lmc.py

        CompilerRuntimeSearchContext ctx;
        ctx.appDirPath = appBin;
        ctx.sourceDirOverride = sourceDir.path();

        const CompilerRuntimeResolution res = DSLCompilerInterface::resolveCompilerRuntime(ctx);
        QVERIFY(!res.success);
        QVERIFY(!res.isInstalledMode);
        QVERIFY(res.errorMessage.contains(QStringLiteral("Compiler runtime not found in developer mode")));
    }

    void envOverrideForcesInstalledOrDeveloperMode()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString binDir = dir.path() + QStringLiteral("/bin");
        QDir().mkpath(binDir);

        CompilerRuntimeSearchContext ctx;
        ctx.appDirPath = binDir;

        // 1. 无 marker 情况下设置 LH_RUNTIME_MODE=installed
        qputenv("LH_RUNTIME_MODE", "installed");
        CompilerRuntimeResolution res = DSLCompilerInterface::resolveCompilerRuntime(ctx);
        QVERIFY(res.isInstalledMode);

        // 2. 有 marker 情况下设置 LH_RUNTIME_MODE=developer
        QVERIFY(touchFile(binDir + QStringLiteral("/.lh_install_marker")));
        qputenv("LH_RUNTIME_MODE", "developer");
        res = DSLCompilerInterface::resolveCompilerRuntime(ctx);
        QVERIFY(!res.isInstalledMode);
    }

    void pythonInterpreterCacheIsolatedPerWorkDir()
    {
        const QString workDirA = QStringLiteral("D:/Test/RuntimeA");
        const QString workDirB = QStringLiteral("D:/Test/RuntimeB");
        const QString pyA = QStringLiteral("D:/Test/RuntimeA/venv/python.exe");
        const QString pyB = QStringLiteral("D:/Test/RuntimeB/venv/python.exe");

        DSLCompilerInterface::clearPythonInterpreterCache();
        QVERIFY(DSLCompilerInterface::cachedPythonInterpreter(workDirA).isEmpty());
        QVERIFY(DSLCompilerInterface::cachedPythonInterpreter(workDirB).isEmpty());

        DSLCompilerInterface::setCachedPythonInterpreter(workDirA, pyA);
        DSLCompilerInterface::setCachedPythonInterpreter(workDirB, pyB);

        QCOMPARE(DSLCompilerInterface::cachedPythonInterpreter(workDirA), pyA);
        QCOMPARE(DSLCompilerInterface::cachedPythonInterpreter(workDirB), pyB);

        // 单独清除 workDirA
        DSLCompilerInterface::clearPythonInterpreterCache(workDirA);
        QVERIFY(DSLCompilerInterface::cachedPythonInterpreter(workDirA).isEmpty());
        QCOMPARE(DSLCompilerInterface::cachedPythonInterpreter(workDirB), pyB);

        // 全局清除
        DSLCompilerInterface::clearPythonInterpreterCache();
        QVERIFY(DSLCompilerInterface::cachedPythonInterpreter(workDirB).isEmpty());
    }

    void compilerInstanceAppliesSearchContext()
    {
        QTemporaryDir installDir;
        QVERIFY(installDir.isValid());
        const QString installBin = installDir.path() + QStringLiteral("/bin");
        const QString installRuntime = installDir.path() + QStringLiteral("/third_party/custom_dsp_language/compile");
        QVERIFY(touchFile(installBin + QStringLiteral("/.lh_install_marker")));
        QVERIFY(touchFile(installRuntime + QStringLiteral("/lmc.py")));

        CompilerRuntimeSearchContext ctx;
        ctx.appDirPath = installBin;
        ctx.modePolicy = CompilerRuntimeSearchContext::ModePolicy::ForceInstalled;

        DSLCompilerInterface compiler;
        compiler.setRuntimeSearchContext(ctx);

        QCOMPARE(compiler.compilerWorkingDir(), QDir::cleanPath(installRuntime));
        QCOMPARE(compiler.compilerEntryScript(), QDir::cleanPath(installRuntime + QStringLiteral("/lmc.py")));
    }
};

QTEST_MAIN(CompilerRuntimeSearchTest)
#include "compiler_runtime_search_test.moc"
