#include <QCoreApplication>
#include <QProcess>
#include <QFile>
#include <QElapsedTimer>
#include <QThread>
#include <QDebug>
#include <QDateTime>
#include "core/AppLogging.h"

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    app.setOrganizationName("DUT");
    app.setApplicationName("LH-LoggingOverloadTest");
    if (app.arguments().contains("--shutdown-child")) {
        AppLogging::TestHooks::pauseWriter(true);
        if (!AppLogging::install()) return 10;
        QElapsedTimer elapsed; elapsed.start();
        while (!AppLogging::TestHooks::writerWaiting() && elapsed.elapsed() < 1000) QThread::msleep(1);
        qInfo("shutdown-drain-probe"); elapsed.restart(); AppLogging::shutdown();
        const bool bounded = elapsed.elapsed() < 1300;
        AppLogging::TestHooks::pauseWriter(false);
        elapsed.restart();
        while (!AppLogging::install() && elapsed.elapsed() < 1000) QThread::msleep(1);
        const bool ready = AppLogging::isAvailable(); AppLogging::flush(); AppLogging::shutdown();
        return bounded && ready ? 0 : 11;
    }
    if (!app.arguments().contains("--child")) {
        QProcess child;
        child.start(app.applicationFilePath(), {"--child"});
        if (!child.waitForFinished(15000) || child.exitStatus() != QProcess::NormalExit || child.exitCode() != 0) {
            qCritical() << child.readAllStandardOutput() << child.readAllStandardError(); return 1;
        }
        const auto fallback = child.readAllStandardError();
        if (!fallback.contains("status=confirmed") || !fallback.contains("status=cancelled")
                || !fallback.contains("status=succeeded") || !fallback.contains("opId=terminal-")
                || fallback.contains("must-hide") || fallback.contains("json-secret")) return 2;
        QProcess shutdownChild; shutdownChild.start(app.applicationFilePath(), {"--shutdown-child"});
        if (!shutdownChild.waitForFinished(5000) || shutdownChild.exitCode() != 0
            || shutdownChild.exitStatus() != QProcess::NormalExit
            || !shutdownChild.readAllStandardError().contains("shutdown exceeded 1000 ms")) return 12;
        return 0;
    }
    const auto logPath = AppLogging::activeLogPath();
    QFile::remove(logPath);
    AppLogging::TestHooks::pauseWriter(true);
    if (!AppLogging::install()) return 3;
    QElapsedTimer wait; wait.start();
    while (!AppLogging::TestHooks::writerWaiting() && wait.elapsed() < 1000) QThread::msleep(1);
    if (!AppLogging::TestHooks::writerWaiting()) { AppLogging::TestHooks::pauseWriter(false); AppLogging::shutdown(); return 4; }
    const QString payload = QStringLiteral("status=failed ") + QString(60000, 'x');
    for (int i = 0; i < 30; ++i)
        AppLogging::writeBusinessEvent("bulk", QtInfoMsg, QString::number(i), "write", "running", 0, {}, {{"payload", payload}});
    const QStringList statuses{"failed", "confirmed", "cancelled", "succeeded"};
    for (int i = 0; i < statuses.size(); ++i)
        AppLogging::writeBusinessEvent("terminal", QtInfoMsg, QString("terminal-%1").arg(i), "write", statuses.at(i), 1, {},
                                      {{"payload", payload}, {"password", "must-hide"}});
    const auto status = AppLogging::overloadStatus();
    const auto dropped = status.value("droppedMessages").toULongLong();
    const bool bounded = status.value("queuedBytes").toLongLong() <= 1024 * 1024;
    QElapsedTimer criticalTime; criticalTime.start();
    qCritical().noquote() << "body={\"password\":\"json-secret\"}";
    const bool criticalResponsive = criticalTime.elapsed() < 100;
    const bool flushTimedOut = !AppLogging::flush(20);
    AppLogging::TestHooks::pauseWriter(false);
    AppLogging::flush();
    AppLogging::shutdown();
    QFile log(logPath);
    if (!log.open(QIODevice::ReadOnly)) return 5;
    const auto bytes = log.readAll();
    if (!bounded || !criticalResponsive || !flushTimedOut || dropped == 0 || !status.value("firstDrop").toDateTime().isValid()
            || !bytes.contains(QByteArray("LOG_OVERLOAD dropped=") + QByteArray::number(dropped))
            || !bytes.contains("event=terminal opId=terminal-") || bytes.contains("must-hide") || bytes.contains("json-secret")) return 6;
    const int admitted = bytes.count("event=bulk") + bytes.count("event=terminal");
    if (admitted + int(dropped) != 34) return 7;
    return 0;
}
