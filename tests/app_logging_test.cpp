#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QList>
#include <QRegularExpression>
#include <QStandardPaths>

#include <cstdio>

#include "AppLogging.h"

namespace {
bool previousHandlerCalled = false;

void previousHandler(QtMsgType, const QMessageLogContext&, const QString&)
{
    previousHandlerCalled = true;
}

void removeLogFiles(const QString& activePath)
{
    if (activePath.isEmpty()) {
        return;
    }
    QFile::remove(activePath);
    QFile::remove(activePath + QStringLiteral(".1"));
    QFile::remove(activePath + QStringLiteral(".2"));
}
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LH-AppLoggingTest"));
    app.setOrganizationName(QStringLiteral("DUT"));

    const QtMessageHandler originalHandler = qInstallMessageHandler(previousHandler);
    const QString expected = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)).filePath(QStringLiteral("logs"));
    const QString testLogPath = QDir(expected).filePath(QStringLiteral("lh.log"));
    removeLogFiles(testLogPath);

    if (!AppLogging::install()) {
        qInstallMessageHandler(originalHandler);
        std::fprintf(stderr, "app logging installation failed\n");
        return 1;
    }

    if (AppLogging::logDirectory() != expected) {
        AppLogging::shutdown();
        qInstallMessageHandler(originalHandler);
        return 2;
    }

    const QString marker = QStringLiteral("app-logging-format-check-%1")
                               .arg(QCoreApplication::applicationPid());
    qInfo().noquote() << marker;
    const QString longMarker = QStringLiteral("app-logging-truncation-check-%1")
                                   .arg(QCoreApplication::applicationPid());
    qInfo().noquote() << longMarker << QString(100000, QChar(0x4F60));

    // Test business event logging
    const QString bizMarker = QStringLiteral("op-test-%1").arg(QCoreApplication::applicationPid());
    AppLogging::writeBusinessEvent(
        QStringLiteral("download_test"),
        QtInfoMsg,
        bizMarker,
        QStringLiteral("transfer"),
        QStringLiteral("running"),
        0,
        QStringLiteral("target-device\nwith\nnewlines"),
        {{QStringLiteral("password"), QStringLiteral("SuperSecret123")},
         {QStringLiteral("token"), QStringLiteral("secret-token-abc")},
         {QStringLiteral("multiline_field"), QStringLiteral("row1\r\nrow2\trow3")}},
        2
    );

    AppLogging::flush();
    QFile activeLog(AppLogging::activeLogPath());
    if (!activeLog.open(QIODevice::ReadOnly | QIODevice::Text)) {
        AppLogging::shutdown();
        qInstallMessageHandler(originalHandler);
        return 3;
    }
    const QByteArray rawContents = activeLog.readAll();
    activeLog.close();
    const QString contents = QString::fromUtf8(rawContents);
    const QRegularExpression linePattern(
        QStringLiteral("\\d{4}-\\d{2}-\\d{2}T[^ ]+ \\[INFO\\] \\[category=default\\] \\[thread=0x[0-9a-f]+\\] %1")
            .arg(QRegularExpression::escape(marker)));
    const QList<QByteArray> lines = rawContents.split('\n');
    QByteArray longLine;
    QByteArray bizLine;
    for (const QByteArray& line : lines) {
        if (line.contains(longMarker.toUtf8())) {
            longLine = line;
        }
        if (line.contains(bizMarker.toUtf8())) {
            bizLine = line;
        }
    }
    const QString decodedLongLine = QString::fromUtf8(longLine);
    if (!linePattern.match(contents).hasMatch() || longLine.isEmpty() ||
        longLine.size() + 1 > 64 * 1024 ||
        !longLine.contains("[truncated]") ||
        decodedLongLine.contains(QChar::ReplacementCharacter)) {
        AppLogging::shutdown();
        qInstallMessageHandler(originalHandler);
        return 4;
    }

    // Verify Business Event
    if (bizLine.isEmpty()) {
        AppLogging::shutdown();
        qInstallMessageHandler(originalHandler);
        return 10;
    }
    const QString decodedBizLine = QString::fromUtf8(bizLine);
    if (!decodedBizLine.contains(QStringLiteral("[category=business_event]")) ||
        !decodedBizLine.contains(QStringLiteral("[BUSINESS_EVENT]")) ||
        !decodedBizLine.contains(QStringLiteral("event=download_test")) ||
        !decodedBizLine.contains(QStringLiteral("attempt=2")) ||
        !decodedBizLine.contains(QStringLiteral("target=target-device\\nwith\\nnewlines")) ||
        !decodedBizLine.contains(QStringLiteral("password=******")) ||
        !decodedBizLine.contains(QStringLiteral("token=******")) ||
        !decodedBizLine.contains(QStringLiteral("multiline_field=row1\\r\\nrow2\\trow3")) ||
        decodedBizLine.contains(QStringLiteral("SuperSecret123"))) {
        AppLogging::shutdown();
        qInstallMessageHandler(originalHandler);
        return 11;
    }

    const quint64 dropsBefore = AppLogging::droppedMessageCount();
    for (int i = 0; i < 1000; ++i)
        qInfo().noquote() << "bounded-queue-probe" << i << QString(2000, 'x');
    AppLogging::flush();
    QFile queueLog(AppLogging::activeLogPath());
    if (!queueLog.open(QIODevice::ReadOnly)) return 12;
    const auto queuedContents = queueLog.readAll(); queueLog.close();
    if (static_cast<quint64>(queuedContents.count("bounded-queue-probe"))
            + AppLogging::droppedMessageCount() - dropsBefore != 1000) return 13;

    qCritical() << "critical-must-be-visible";
    if (!queueLog.open(QIODevice::ReadOnly)) return 14;
    if (!queueLog.readAll().contains("critical-must-be-visible")) return 15;
    queueLog.close();
    qInfo() << "shutdown-must-drain";
    AppLogging::shutdown();
    if (!queueLog.open(QIODevice::ReadOnly)) return 16;
    if (!queueLog.readAll().contains("shutdown-must-drain")) return 17;
    queueLog.close();

    // Force a rotation failure using only this test's log directory.
    if (!queueLog.open(QIODevice::WriteOnly) || !queueLog.resize(4 * 1024 * 1024)) return 18;
    queueLog.close();
    QFile::remove(testLogPath + ".2");
    if (!QDir().mkdir(testLogPath + ".2")) return 19;
    if (!AppLogging::install()) return 20;
    qCritical() << "rotation-failure-probe";
    if (AppLogging::isAvailable()) return 21;
    AppLogging::shutdown();
    QDir().rmdir(testLogPath + ".2");
    qInfo() << "after app logging shutdown";
    const bool restored = previousHandlerCalled;
    qInstallMessageHandler(originalHandler);
    removeLogFiles(testLogPath);
    return restored ? 0 : 5;
}
