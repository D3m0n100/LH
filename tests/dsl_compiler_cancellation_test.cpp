#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QCoreApplication>
#include <QThread>
#include <QtTest/QtTest>

#include <cstdio>
#include <functional>
#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#endif

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
        , m_previousInterpreters(DSLCompilerInterface::s_cachedPythonInterpretersByWorkDir)
    {
        qputenv("PYTHON", python);
        qputenv("PATH", path);
        DSLCompilerInterface::clearPythonInterpreterCache();
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
        DSLCompilerInterface::s_cachedPythonInterpretersByWorkDir = m_previousInterpreters;
    }

private:
    bool m_hadPython;
    QByteArray m_previousPython;
    bool m_hadPath;
    QByteArray m_previousPath;
    QString m_previousInterpreter;
    QHash<QString, QString> m_previousInterpreters;
};

class DslCompilerCancellationTest : public QObject
{
    Q_OBJECT

private slots:
    void interpreterProbeDoesNotBlockEventLoop()
    {
        QTemporaryDir temporaryDir;
        QVERIFY(temporaryDir.isValid());
        DSLCompilerInterface compiler;
        // Isolate runtime discovery from developer venvs and interpreter caches.
        const QString bin = temporaryDir.filePath("bin");
        const QString runtime = temporaryDir.filePath("third_party/custom_dsp_language/compile");
        QVERIFY(QDir().mkpath(bin));
        QVERIFY(QDir().mkpath(runtime));
        QFile marker(bin + "/.lh_install_marker");
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.close();
        QFile entry(runtime + "/lmc.py");
        QVERIFY(entry.open(QIODevice::WriteOnly));
        entry.close();
        CompilerRuntimeSearchContext context;
        context.appDirPath = bin;
        context.sourceDirOverride = temporaryDir.path();
        compiler.setRuntimeSearchContext(context);
        QCOMPARE(compiler.compilerWorkingDir(), QDir::cleanPath(runtime));
        ScopedCompilerProbeEnvironment probeEnvironment(
                QCoreApplication::applicationFilePath().toUtf8(), qgetenv("PATH"));

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
        QTRY_COMPARE_WITH_TIMEOUT(compiler.m_pythonProbeProcess->state(), QProcess::Running, 1000);
        const QPointer<QProcess> cancelledProbe = compiler.m_pythonProbeProcess;
        timer.restart();
        compiler.cancelCurrentCompile();
        QVERIFY2(timer.elapsed() < 500, "probe cancellation must not wait for process exit");
        QCOMPARE(compiler.m_pythonProbeProcess, nullptr);
        QCOMPARE(compiler.m_process, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(cancelledProbe.isNull(), 1500);
        QTest::qWait(2500);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(finished.count(), 0);
        QCOMPARE(compiler.m_process, nullptr);
    }

    void cancellationDetachesProcessAndPreservesNewGeneration()
    {
        QTemporaryDir temporaryDir;
        QVERIFY(temporaryDir.isValid());
        DSLCompilerInterface compiler;
        QSignalSpy finished(&compiler, &DSLCompilerInterface::compileFinishedForGeneration);
        const QString sourceFile = QDir(temporaryDir.path()).filePath(QStringLiteral("source.lh"));
        const QString outputFile = QDir(temporaryDir.path()).filePath(QStringLiteral("source.code"));
        const QString compilerInputFile = QDir(temporaryDir.path()).filePath(QStringLiteral("input.lh"));

        compiler.startAsyncCompilerProcess(
                QStringLiteral("cancel-test"),
                QCoreApplication::applicationFilePath(),
                QStringList{QStringLiteral("--compiler-test-helper"), QStringLiteral("sleep")},
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
        QTRY_COMPARE_WITH_TIMEOUT(compiler.m_process->state(), QProcess::Running, 1000);
        QElapsedTimer timer;
        timer.start();
        compiler.cancelCurrentCompile();
        QVERIFY2(timer.elapsed() < 500, "cancellation must not wait for the compiler process");
        QVERIFY(compiler.m_process == nullptr);
        QCOMPARE(finished.count(), 0);

        compiler.startAsyncCompilerProcess(
                QStringLiteral("new-generation-test"),
                QCoreApplication::applicationFilePath(),
                QStringList{
                    QStringLiteral("--compiler-test-helper"),
                    QStringLiteral("write"),
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

    void compilerOutputIsDrainedAndBounded()
    {
        QTemporaryDir temporaryDir;
        QVERIFY(temporaryDir.isValid());
        DSLCompilerInterface compiler;
        QSignalSpy finished(&compiler, &DSLCompilerInterface::compileFinishedForGeneration);
        const QString sourceFile = QDir(temporaryDir.path()).filePath(QStringLiteral("source.lh"));
        const QString outputFile = QDir(temporaryDir.path()).filePath(QStringLiteral("source.code"));
        const QString compilerInputFile = QDir(temporaryDir.path()).filePath(QStringLiteral("input.lh"));

        compiler.startAsyncCompilerProcess(
                QStringLiteral("bounded-output-test"),
                QCoreApplication::applicationFilePath(),
                QStringList{
                    QStringLiteral("--compiler-test-helper"),
                    QStringLiteral("output")
                },
                temporaryDir.path(),
                sourceFile,
                sourceFile,
                QStringList{sourceFile},
                temporaryDir.path(),
                QStringLiteral("bounded-output-test"),
                outputFile,
                compilerInputFile,
                QString(),
                QString(),
                43);

        QTRY_COMPARE(finished.count(), 1);
        const QList<QVariant> result = finished.at(0);
        const QString standardOutput = result.at(3).toString();
        const QByteArray marker = QByteArrayLiteral(
                "[DSLCompilerInterface output truncated; showing tail]");
        QCOMPARE(standardOutput.count(QString::fromLatin1(marker)), 1);
        QVERIFY(standardOutput.toUtf8().size() <= 1024 * 1024);
        QVERIFY(standardOutput.contains(
                QString::fromUtf8("\342\200\223utf8-tail-sentinel\n")));
    }
};

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    const QStringList args = application.arguments();
    // Real native child processes replace the former POSIX-only shell fixtures.
    if (args.value(1) == "-c") {
        QThread::msleep(1000);
        return args.value(2).contains("antlr4") ? 1 : 0;
    }
    if (args.value(1) == "--compiler-test-helper") {
        if (args.value(2) == "sleep") {
            QThread::msleep(5000);
            return 0;
        }
        if (args.value(2) == "write") {
            QFile output(args.value(3));
            return output.open(QIODevice::WriteOnly) ? 0 : 1;
        }
        if (args.value(2) == "output") {
#ifdef Q_OS_WIN
            _setmode(_fileno(stdout), _O_BINARY);
#endif
            QFile output;
            if (!output.open(stdout, QIODevice::WriteOnly)) return 1;
            const QByteArray bytes = QByteArray(2097152, '\0')
                    + QByteArray("\342\200\223utf8-tail-sentinel\n");
            return output.write(bytes) == bytes.size() && output.flush() ? 0 : 1;
        }
        return 1;
    }
    DslCompilerCancellationTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "dsl_compiler_cancellation_test.moc"
