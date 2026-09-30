#include "AppLogging.h"
#include "common/LogSafety.h"
#include <QJsonDocument>

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QThread>

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <chrono>

namespace {

constexpr qint64 kMaxLogBytes = 4 * 1024 * 1024;
constexpr int kMaxLineBytes = 64 * 1024;
constexpr int kBackupCount = 2;

struct LogState {
    QMutex mutex;
    std::mutex queueMutex;
    std::condition_variable queueChanged;
    std::deque<QByteArray> queue;
    std::thread writer;
    qint64 queuedBytes = 0;
    quint64 submitted = 0;
    quint64 completed = 0;
    quint64 dropped = 0;
    quint64 unreportedDropped = 0;
    QDateTime firstDrop;
    QDateTime lastDrop;
    bool stopping = false;
    std::atomic_bool accepting{false};
    QFile activeFile;
    QString directory;
    QString activePath;
    QString error;
    qint64 bytes = 0;
    bool installed = false;
    bool available = false;
    QtMessageHandler previousHandler = nullptr;
};

LogState& state()
{
    // Keep the handler state alive until process exit. Qt can emit messages
    // while static objects are being destroyed during shutdown.
    static LogState* s_state = new LogState;
    return *s_state;
}

void writeStderr(const QByteArray& message)
{
    std::fwrite(message.constData(), 1, static_cast<size_t>(message.size()), stderr);
    std::fflush(stderr);
}

QString messageTypeName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }
    return QStringLiteral("UNKNOWN");
}

QString backupPath(const QString& activePath, int index)
{
    return QStringLiteral("%1.%2").arg(activePath).arg(index);
}

bool reopenActive(LogState& logState)
{
    logState.activeFile.setFileName(logState.activePath);
    if (!logState.activeFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        logState.available = false;
        logState.error = logState.activeFile.errorString();
        return false;
    }
    logState.bytes = QFileInfo(logState.activePath).size();
    logState.error.clear();
    logState.available = true;
    return true;
}

bool rotationFailure(LogState& logState, const QString& error)
{
    logState.available = false;
    logState.activeFile.close();
    logState.error = error;
    return false;
}

bool rotate(LogState& logState)
{
    logState.activeFile.close();

    // Keep a small fixed number of backups. Any failed filesystem operation
    // disables the file sink instead of reopening an over-sized active file.
    const QString oldestBackup = backupPath(logState.activePath, kBackupCount);
    if (QFileInfo::exists(oldestBackup)) {
        QFile oldest(oldestBackup);
        if (!oldest.remove() && QFileInfo::exists(oldestBackup)) {
            return rotationFailure(logState,
                                   QStringLiteral("cannot remove log backup: %1")
                                       .arg(oldest.errorString()));
        }
    }
    for (int index = kBackupCount - 1; index >= 1; --index) {
        const QString source = backupPath(logState.activePath, index);
        if (QFileInfo::exists(source)) {
            QFile sourceFile(source);
            if (!sourceFile.rename(backupPath(logState.activePath, index + 1))) {
                return rotationFailure(logState,
                                       QStringLiteral("cannot rotate log backup: %1")
                                           .arg(sourceFile.errorString()));
            }
        }
    }
    if (QFileInfo::exists(logState.activePath) &&
        !QFile(logState.activePath).rename(backupPath(logState.activePath, 1))) {
        QFile truncated(logState.activePath);
        if (!truncated.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return rotationFailure(logState,
                                   QStringLiteral("cannot rotate or truncate active log: %1")
                                       .arg(truncated.errorString()));
        }
        truncated.close();
    }

    return reopenActive(logState);
}

QByteArray formatLine(QtMsgType type, const QMessageLogContext& context,
                      const QString& message)
{
    const QString category = context.category && *context.category
        ? QString::fromUtf8(context.category)
        : QStringLiteral("default");
    const QByteArray prefix = QStringLiteral("%1 [%2] [category=%3] [thread=0x%4] ")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs))
        .arg(messageTypeName(type))
        .arg(LogSafety::escapeLine(category))
        .arg(QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16))
        .toUtf8();
    const QByteArray messageBytes = LogSafety::escapeLine(LogSafety::redactText(message)).toUtf8();
    const QByteArray truncationMarker(" [truncated]\n");
    if (prefix.size() + messageBytes.size() + 1 <= kMaxLineBytes) {
        return prefix + messageBytes + '\n';
    }

    const int available = qMax(0, kMaxLineBytes - prefix.size() - truncationMarker.size());
    int messageLength = 0;
    while (messageLength < messageBytes.size() && messageLength < available) {
        const unsigned char byte = static_cast<unsigned char>(messageBytes.at(messageLength));
        const int characterLength = (byte & 0x80) == 0x00 ? 1
            : (byte & 0xE0) == 0xC0 ? 2
            : (byte & 0xF0) == 0xE0 ? 3
            : (byte & 0xF8) == 0xF0 ? 4 : 1;
        if (messageLength + characterLength > available) {
            break;
        }
        messageLength += characterLength;
    }
    return prefix + messageBytes.left(messageLength) + truncationMarker;
}

void writeBatch(LogState& logState, const std::deque<QByteArray>& batch)
{
    QMutexLocker lock(&logState.mutex);
    for (const auto& encoded : batch) {
        if (logState.available && logState.bytes + encoded.size() > kMaxLogBytes)
            rotate(logState);
        if (logState.available) {
            const qint64 written = logState.activeFile.write(encoded);
            if (written == encoded.size()) logState.bytes += written;
            else {
                logState.available = false;
                logState.error = logState.activeFile.errorString();
                writeStderr(encoded);
                logState.activeFile.close();
            }
        } else writeStderr(encoded);
    }
    if (logState.available && !logState.activeFile.flush()) {
        logState.available = false;
        logState.error = logState.activeFile.errorString();
        writeStderr(QByteArray("LH logging flush failed: ") + logState.error.toUtf8() + '\n');
        for (const auto& encoded : batch) writeStderr(encoded);
        logState.activeFile.close();
    }
}

void writerLoop(LogState& logState)
{
    for (;;) {
        std::deque<QByteArray> batch;
        quint64 admittedCount = 0;
        {
            std::unique_lock<std::mutex> lock(logState.queueMutex);
            logState.queueChanged.wait(lock, [&]() { return logState.stopping || !logState.queue.empty(); });
            if (logState.queue.empty() && logState.stopping) break;
            logState.queueChanged.wait_for(lock, std::chrono::milliseconds(5), [&]() {
                return logState.stopping || logState.queuedBytes >= 64 * 1024;
            });
            batch.swap(logState.queue);
            admittedCount = batch.size();
            logState.queuedBytes = 0;
            if (logState.unreportedDropped) {
                const QString summary = QStringLiteral("LOG_OVERLOAD dropped=%1 first=%2 last=%3 terminalPolicy=reserved-capacity-then-stderr")
                        .arg(logState.unreportedDropped)
                        .arg(logState.firstDrop.toString(Qt::ISODateWithMs), logState.lastDrop.toString(Qt::ISODateWithMs));
                batch.push_back(formatLine(QtWarningMsg, QMessageLogContext(), summary));
                logState.unreportedDropped = 0;
            }
        }
        writeBatch(logState, batch);
        {
            std::lock_guard<std::mutex> lock(logState.queueMutex);
            logState.completed += admittedCount;
        }
        logState.queueChanged.notify_all();
    }
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    LogState& logState = state();
    const QByteArray encoded = formatLine(type, context, message);
    const bool critical = type == QtCriticalMsg || type == QtFatalMsg;
    static const QRegularExpression terminalStatus(QStringLiteral(R"(\bstatus=(?:success|succeeded|failed|failure|canceled|cancelled|completed)\b)"));
    const bool terminal = context.category && QByteArray(context.category) == "business_event"
            && terminalStatus.match(message).hasMatch();
    quint64 sequence = 0;
    {
        std::unique_lock<std::mutex> lock(logState.queueMutex);
        if (!logState.accepting) {
            lock.unlock(); writeStderr(encoded);
        } else {
            constexpr qint64 maxQueueBytes = 1024 * 1024;
            if (critical) {
                logState.queueChanged.wait(lock, [&]() {
                    return !logState.accepting || logState.queuedBytes + encoded.size() <= maxQueueBytes;
                });
            }
            if (!logState.accepting) { lock.unlock(); writeStderr(encoded); }
            else if (logState.queuedBytes + encoded.size() > (terminal || critical ? maxQueueBytes : maxQueueBytes - 64 * 1024)) {
                ++logState.dropped;
                if (logState.unreportedDropped++ == 0) logState.firstDrop = QDateTime::currentDateTimeUtc();
                logState.lastDrop = QDateTime::currentDateTimeUtc();
                if (terminal) { lock.unlock(); writeStderr(encoded); }
            }
            else {
                logState.queue.push_back(encoded);
                logState.queuedBytes += encoded.size();
                sequence = ++logState.submitted;
            }
        }
    }
    logState.queueChanged.notify_all();
    if (critical && sequence) {
        std::unique_lock<std::mutex> lock(logState.queueMutex);
        logState.queueChanged.wait(lock, [&]() { return logState.completed >= sequence; });
    }
    if (type == QtFatalMsg) std::abort();
}

QString configuredDirectory(LogState& logState)
{
    if (logState.directory.isEmpty()) {
        const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (!appData.isEmpty()) {
            logState.directory = QDir(appData).filePath(QStringLiteral("logs"));
            logState.activePath = QDir(logState.directory).filePath(QStringLiteral("lh.log"));
        }
    }
    return logState.directory;
}

} // namespace

namespace AppLogging {

bool install()
{
    LogState& logState = state();
    QMutexLocker locker(&logState.mutex);
    if (logState.installed) {
        return logState.available;
    }

    configuredDirectory(logState);
    bool ready = !logState.directory.isEmpty();
    if (ready) {
        QDir directory;
        ready = directory.mkpath(logState.directory);
    }
    if (ready) {
        ready = reopenActive(logState);
    }
    if (!ready && logState.error.isEmpty()) {
        logState.error = logState.directory.isEmpty()
            ? QStringLiteral("QStandardPaths returned no writable AppDataLocation")
            : QStringLiteral("unable to create log directory");
    }

    {
        std::lock_guard<std::mutex> lock(logState.queueMutex);
        logState.stopping = false;
        logState.accepting = true;
    }
    logState.writer = std::thread([&logState]() { writerLoop(logState); });
    logState.installed = true;
    logState.previousHandler = qInstallMessageHandler(messageHandler);
    if (!ready) {
        writeStderr(QByteArray("LH persistent logging initialization failed: ") +
                    logState.error.toLocal8Bit() + '\n');
    }
    return ready;
}

void flush()
{
    LogState& logState = state();
    std::unique_lock<std::mutex> lock(logState.queueMutex);
    const auto target = logState.submitted;
    logState.queueChanged.notify_all();
    logState.queueChanged.wait(lock, [&]() { return logState.completed >= target; });
}

quint64 droppedMessageCount()
{
    auto& logState = state();
    std::lock_guard<std::mutex> lock(logState.queueMutex);
    return logState.dropped;
}

QVariantMap overloadStatus()
{
    auto& logState = state();
    std::lock_guard<std::mutex> lock(logState.queueMutex);
    return {{QStringLiteral("droppedMessages"), QVariant::fromValue(logState.dropped)},
            {QStringLiteral("firstDrop"), logState.firstDrop},
            {QStringLiteral("lastDrop"), logState.lastDrop},
            {QStringLiteral("queuedBytes"), logState.queuedBytes},
            {QStringLiteral("terminalPolicy"), QStringLiteral("reserved-capacity-then-stderr")}};
}

void shutdown()
{
    LogState& logState = state();
    {
        std::lock_guard<std::mutex> lock(logState.queueMutex);
        logState.accepting = false;
        logState.stopping = true;
    }
    logState.queueChanged.notify_all();
    if (logState.writer.joinable()) logState.writer.join();
    QtMessageHandler previousHandler = nullptr;
    {
        QMutexLocker locker(&logState.mutex);
        if (!logState.installed) return;
        previousHandler = logState.previousHandler;
        logState.available = false;
        logState.activeFile.close();
        logState.installed = false;
        logState.previousHandler = nullptr;
    }
    qInstallMessageHandler(previousHandler);
}

bool isAvailable()
{
    LogState& logState = state();
    QMutexLocker locker(&logState.mutex);
    return logState.installed && logState.available;
}

QString lastError()
{
    LogState& logState = state();
    QMutexLocker locker(&logState.mutex);
    return logState.error;
}

QString logDirectory()
{
    LogState& logState = state();
    QMutexLocker locker(&logState.mutex);
    return configuredDirectory(logState);
}

QString activeLogPath()
{
    LogState& logState = state();
    QMutexLocker locker(&logState.mutex);
    configuredDirectory(logState);
    return logState.activePath;
}

namespace {

QString sanitizeBusinessValue(const QString& str)
{
    // Physical-line framing is applied once to the complete record.
    return str;
}

bool isCredentialKey(const QString& key)
{
    return LogSafety::isSensitiveKey(key);
}

} // namespace

void writeBusinessEvent(const BusinessEvent& event)
{
    QStringList parts;
    parts.append(QStringLiteral("[BUSINESS_EVENT]"));
    parts.append(QStringLiteral("event=%1").arg(sanitizeBusinessValue(event.eventName)));
    parts.append(QStringLiteral("opId=%1").arg(sanitizeBusinessValue(event.operationId)));
    parts.append(QStringLiteral("phase=%1").arg(sanitizeBusinessValue(event.phase)));
    parts.append(QStringLiteral("status=%1").arg(sanitizeBusinessValue(event.status)));
    parts.append(QStringLiteral("code=%1").arg(event.errorCode));
    parts.append(QStringLiteral("target=%1").arg(sanitizeBusinessValue(event.target)));
    parts.append(QStringLiteral("attempt=%1").arg(event.attempt));

    for (auto it = event.fields.constBegin(); it != event.fields.constEnd(); ++it) {
        const QString& key = it.key();
        QString valStr;
        if (isCredentialKey(key)) {
            valStr = QStringLiteral("******");
        } else {
            const auto safe = LogSafety::redact(QJsonValue::fromVariant(it.value()));
            if (safe.isObject()) valStr = QString::fromUtf8(QJsonDocument(safe.toObject()).toJson(QJsonDocument::Compact));
            else if (safe.isArray()) valStr = QString::fromUtf8(QJsonDocument(safe.toArray()).toJson(QJsonDocument::Compact));
            else valStr = safe.toVariant().toString();
            valStr = sanitizeBusinessValue(valStr);
        }
        parts.append(QStringLiteral("%1=%2").arg(sanitizeBusinessValue(key), valStr));
    }

    const QString message = parts.join(QLatin1Char(' '));
    QMessageLogContext context(__FILE__, __LINE__, __FUNCTION__, "business_event");
    messageHandler(event.severity, context, message);
}

void writeBusinessEvent(const QString& eventName,
                        QtMsgType severity,
                        const QString& operationId,
                        const QString& phase,
                        const QString& status,
                        int errorCode,
                        const QString& target,
                        const QVariantMap& fields,
                        int attempt)
{
    BusinessEvent event;
    event.eventName = eventName;
    event.severity = severity;
    event.operationId = operationId;
    event.phase = phase;
    event.status = status;
    event.errorCode = errorCode;
    event.target = target;
    event.attempt = attempt;
    event.fields = fields;
    writeBusinessEvent(event);
}

} // namespace AppLogging
