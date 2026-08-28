#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>

#include <functional>

#include "common/ConfigTypes.h"
#include "common/RuntimePointTypes.h"
#include "Common.h"

#define private public
#include "compiler/DSLCompilerInterface.h"
#undef private

class ScopedCompilerProbeEnvironment
{
public:
    ScopedCompilerProbeEnvironment(const QByteArray& python, const QByteArray& path)
        : m_hadPython(qEnvironmentVariableIsSet("PYTHON"))
        , m_previousPython(qgetenv("PYTHON"))
        , m_hadPath(qEnvironmentVariableIsSet("PATH"))
        , m_previousPath(qgetenv("PATH"))
        , m_previousInterpreter(DSLCompilerInterface::s_cachedPythonInterpreter)
    {
        qputenv("PYTHON", python);
        qputenv("PATH", path);
        DSLCompilerInterface::s_cachedPythonInterpreter.clear();
    }

    ~ScopedCompilerProbeEnvironment()
    {
        if (m_hadPython) {
            qputenv("PYTHON", m_previousPython);
        } else {
            qunsetenv("PYTHON");
        }
        if (m_hadPath) {
            qputenv("PATH", m_previousPath);
        } else {
            qunsetenv("PATH");
        }
        DSLCompilerInterface::s_cachedPythonInterpreter = m_previousInterpreter;
    }

private:
    bool m_hadPython;
    QByteArray m_previousPython;
    bool m_hadPath;
    QByteArray m_previousPath;
    QString m_previousInterpreter;
};

class DslCompilerCancellationTest : public QObject
{
    Q_OBJECT

private slots:
    void interpreterProbeDoesNotBlockEventLoop()
    {
        QTemporaryDir temporaryDir;
        QVERIFY(temporaryDir.isValid());
        if (!QFileInfo::exists(QStringLiteral("/bin/sh"))) {
            QSKIP("/bin/sh is unavailable on this platform");
        }

        DSLCompilerInterface compiler;
        QDir compilerDir(compiler.compilerWorkingDir());
#ifdef Q_OS_WIN
        const QString venvPython = compilerDir.absoluteFilePath(QStringLiteral("venv/Scripts/python.exe"));
#else
        const QString venvPython = compilerDir.absoluteFilePath(QStringLiteral("venv/bin/python3"));
#endif
        if (QFileInfo::exists(venvPython)) {
            QSKIP("compiler venv takes precedence over PYTHON; probe timing is not controllable");
        }

        const QString probeScript = QDir(temporaryDir.path()).filePath(QStringLiteral("slow-python"));
        QFile script(probeScript);
        QVERIFY(script.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(script.write("#!/bin/sh\n/bin/sleep 1\ncase \"$2\" in\n*antlr4*) exit 1 ;;\nesac\nexit 0\n") >= 0);
        script.close();
        QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

        const QByteArray emptyPath = QDir(temporaryDir.path()).filePath(QStringLiteral("no-python")).toUtf8();
        ScopedCompilerProbeEnvironment probeEnvironment(probeScript.toUtf8(), emptyPath);

        const QString sourceFile = QDir(temporaryDir.path()).filePath(QStringLiteral("source.lh"));
        QFile source(sourceFile);
        QVERIFY(source.open(QIODevice::WriteOnly | QIODevice::Text));
        source.write("program\nendprogram\n");
        source.close();

        QSignalSpy failed(&compiler, &DSLCompilerInterface::compileFailedToStartForGeneration);
        QSignalSpy finished(&compiler, &DSLCompilerInterface::compileFinishedForGeneration);
        bool eventLoopAdvanced = false;
        QTimer::singleShot(0, &compiler, [&eventLoopAdvanced] { eventLoopAdvanced = true; });
        QElapsedTimer timer;
        timer.start();
        compiler.compileDslFileAsync(sourceFile,
                                     QDir(temporaryDir.path()).filePath(QStringLiteral("out")),
                                     QStringLiteral("probe-test"),
                                     71);

        QTRY_VERIFY_WITH_TIMEOUT(eventLoopAdvanced, 250);
        QVERIFY2(timer.elapsed() < 250, "interpreter probing must not block the event loop");
        QVERIFY(compiler.m_pythonProbeProcess != nullptr);
        compiler.cancelCurrentCompile();
        QCOMPARE(compiler.m_pythonProbeProcess, nullptr);
        QCOMPARE(compiler.m_process, nullptr);
        QTest::qWait(2500);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(finished.count(), 0);
        QCOMPARE(compiler.m_process, nullptr);
    }

    void cancellationDetachesProcessAndPreservesNewGeneration()
    {
        QTemporaryDir temporaryDir;
        QVERIFY(temporaryDir.isValid());
        if (!QFileInfo::exists(QStringLiteral("/bin/sh"))) {
            QSKIP("/bin/sh is unavailable on this platform");
        }

        DSLCompilerInterface compiler;
        QSignalSpy finished(&compiler, &DSLCompilerInterface::compileFinishedForGeneration);
        const QString sourceFile = QDir(temporaryDir.path()).filePath(QStringLiteral("source.lh"));
        const QString outputFile = QDir(temporaryDir.path()).filePath(QStringLiteral("source.code"));
        const QString compilerInputFile = QDir(temporaryDir.path()).filePath(QStringLiteral("input.lh"));

        compiler.startAsyncCompilerProcess(
                QStringLiteral("cancel-test"),
                QStringLiteral("/bin/sh"),
                QStringList{QStringLiteral("-c"), QStringLiteral("sleep 5"), QStringLiteral("sh")},
                temporaryDir.path(),
                sourceFile,
                sourceFile,
                QStringList{sourceFile},
                temporaryDir.path(),
                QStringLiteral("cancel-test"),
                outputFile,
                compilerInputFile,
                QString(),
                QString(),
                41);

        QVERIFY(compiler.m_process != nullptr);
        QElapsedTimer timer;
        timer.start();
        compiler.cancelCurrentCompile();
        QVERIFY2(timer.elapsed() < 500, "cancellation must not wait for the compiler process");
        QVERIFY(compiler.m_process == nullptr);

        compiler.startAsyncCompilerProcess(
                QStringLiteral("new-generation-test"),
                QStringLiteral("/bin/sh"),
                QStringList{
                    QStringLiteral("-c"),
                    QStringLiteral(": > \"$1\""),
                    QStringLiteral("sh"),
                    outputFile
                },
                temporaryDir.path(),
                sourceFile,
                sourceFile,
                QStringList{sourceFile},
                temporaryDir.path(),
                QStringLiteral("new-generation-test"),
                outputFile,
                compilerInputFile,
                QString(),
                QString(),
                42);

        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(finished.at(0).at(0).toULongLong(), static_cast<qulonglong>(42));
        QTest::qWait(100);
        QCOMPARE(finished.count(), 1);
    }
};

QTEST_MAIN(DslCompilerCancellationTest)
#include "dsl_compiler_cancellation_test.moc"
