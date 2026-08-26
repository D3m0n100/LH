#include <QElapsedTimer>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>

#include "common/ConfigTypes.h"
#include "common/RuntimePointTypes.h"
#include "Common.h"

#define private public
#include "compiler/DSLCompilerInterface.h"
#undef private

class DslCompilerCancellationTest : public QObject
{
    Q_OBJECT

private slots:
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
