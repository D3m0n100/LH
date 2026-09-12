#ifndef APPLOGGING_H
#define APPLOGGING_H

#include <QString>
#include <QVariantMap>
#include <QtGlobal>

namespace AppLogging {

struct BusinessEvent {
    QString eventName;
    QtMsgType severity = QtInfoMsg;
    QString operationId;
    QString phase;
    QString status;
    int errorCode = 0;
    QString target;
    int attempt = 1;
    QVariantMap fields;
};

// Installs the process-wide Qt message handler and opens the bounded log file.
bool install();

// Restores the handler that was active before install() and closes the file.
void shutdown();
// Wait for all admitted messages to reach the sink; critical/fatal messages do this automatically.
void flush();
quint64 droppedMessageCount();

// Reports the current sink state without emitting a Qt log message.
bool isAvailable();
QString lastError();

// Returns the same directory used by the message handler.
QString logDirectory();

// Returns the active log path, or an empty string when no writable app data
// location is available.
QString activeLogPath();

// Writes a structured, single-line business event to the persistent log sink.
// Enforces:
// 1. Newlines and carriage returns in all strings are escaped to prevent log forging.
// 2. Sensitive fields (passwords, tokens, credentials, secrets) are masked.
// 3. Thread-safe, bounded, degrades to stderr on sink failure without recursion.
void writeBusinessEvent(const BusinessEvent& event);

void writeBusinessEvent(const QString& eventName,
                        QtMsgType severity,
                        const QString& operationId,
                        const QString& phase,
                        const QString& status,
                        int errorCode = 0,
                        const QString& target = QString(),
                        const QVariantMap& fields = QVariantMap(),
                        int attempt = 1);

} // namespace AppLogging

#endif // APPLOGGING_H
