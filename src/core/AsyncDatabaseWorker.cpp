#include "AsyncDatabaseWorker.h"

#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QSqlQuery>
#include <QSqlError>
#include <QElapsedTimer>
#include <algorithm>
#include <QTimer>
#include <cmath>

namespace Core {

namespace {

static QString formatIsoUtc(const QDateTime& dt)
{
    return dt.isValid() ? dt.toUTC().toString(Qt::ISODateWithMs)
                        : QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

} // namespace

AsyncDatabaseWorker::AsyncDatabaseWorker(QObject* parent)
    : QObject(parent)
{
    m_connectionName = QStringLiteral("AsyncWorker_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
}

AsyncDatabaseWorker::~AsyncDatabaseWorker()
{
    // Production owners use deleteWhenStopped after a bounded stop timeout.
    // A stack-owned worker must not be destroyed while a task is still executing.
    if (!stopWorker()) {
        qCritical() << "Worker owner destroyed before shutdown completion";
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
    }
}

bool AsyncDatabaseWorker::startWorker(const QString& dbPath)
{
    if (!startWorkerAsync(dbPath)) return false;
    QMutexLocker lock(&m_startMutex);
    if (!m_startComplete) m_startCondition.wait(&m_startMutex, 3000);
    return m_dbOpen.load();
}

bool AsyncDatabaseWorker::startWorkerAsync(const QString& dbPath)
{
    if (m_isRunning.load()) {
        return true;
    }

    if (m_workerThread) return false;
    m_startComplete = false;
    m_dbPath = dbPath;
    m_shutdownQueued = false;
    m_stopRequested = false;
    m_cleanupCancelled = false;

    if (parent() || QThread::currentThread() != thread()) return false;
    m_ownerThread = thread();
    m_workerThread = new QThread();
    moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, this, &AsyncDatabaseWorker::onThreadStarted);
    m_workerThread->start();

    return true;
}

bool AsyncDatabaseWorker::stopWorker(int drainTimeoutMs)
{
    if (!m_workerThread) return true;
    if (m_workerThread->isFinished()) { delete m_workerThread; m_workerThread = nullptr; return true; }
    if (m_shutdownQueued) return false;
    m_isRunning = false; // Close admission before draining.
    m_cleanupCancelled = true;
    QElapsedTimer timer;
    timer.start();
    while (pendingRecordCount() > 0 && timer.elapsed() < qMax(0, drainTimeoutMs)) {
        QThread::msleep(5);
    }
    m_stopRequested = true;
    m_shutdownQueued = true;
    QMetaObject::invokeMethod(this, [this]() {
        QList<BatchItem> abandoned;
        {
            QMutexLocker lock(&m_queueMutex);
            abandoned.swap(m_pendingBatches);
            if (!m_activeBatch.records.isEmpty()) abandoned.append(m_activeBatch);
            m_activeBatch = {};
            m_totalPendingRecords = 0;
            m_activeRecords = 0;
        }
        for (const auto& batch : abandoned) {
            { QMutexLocker lock(&m_queueMutex); m_droppedRecords += batch.records.size(); }
            qWarning() << "Uncommitted database batch at shutdown" << batch.batchId << "records" << batch.records.size();
            emit recordsDropped(batch.records.size());
            emit batchAbandoned(batch.batchId, batch.records.size(), QStringLiteral("Shutdown drain deadline exceeded"));
        }
        closeDatabaseInThread();
        moveToThread(m_ownerThread);
        QThread::currentThread()->quit();
    }, Qt::QueuedConnection);
    // SQL busy waits are capped at 50 ms; insert loops check stop between rows.
    // Never destroy a running QThread or its live database connection.
    if (!m_workerThread->wait(250)) return false;
    delete m_workerThread;
    m_workerThread = nullptr;
    m_processing = false;
    return true;
}

void AsyncDatabaseWorker::deleteWhenStopped()
{
    if (!m_workerThread || m_workerThread->isFinished()) { deleteLater(); return; }
    connect(m_workerThread, &QThread::finished, this, &QObject::deleteLater, Qt::QueuedConnection);
    if (m_workerThread->isFinished()) deleteLater();
}

bool AsyncDatabaseWorker::isRunning() const
{
    return m_isRunning.load();
}

bool AsyncDatabaseWorker::isDbOpen() const
{
    return m_dbOpen.load();
}

QString AsyncDatabaseWorker::databasePath() const
{
    return m_dbPath;
}

quint64 AsyncDatabaseWorker::enqueueBatch(const QList<QVariantMap>& records)
{
    if (!m_isRunning.load() || m_stopRequested.load() || records.isEmpty()) {
        return 0;
    }

    quint64 batchId = 0;
    int dropped = 0;
    QList<QPair<quint64, int>> abandoned;

    {
        QMutexLocker locker(&m_queueMutex);
        batchId = m_nextBatchId++;

        // 队列超限保护：丢弃最旧数据保留最新
        const int overflow = qMax(0, m_totalPendingRecords + m_activeRecords.load() + records.size() - m_maxQueueRecords);
        QList<QVariantMap> recordsToQueue = records;

        if (overflow > 0) {
            int toDrop = overflow;
            // 1. 先从已有待处理队列头部丢弃旧批次
            while (toDrop > 0 && !m_pendingBatches.isEmpty()) {
                auto& firstBatch = m_pendingBatches.first();
                if (firstBatch.records.size() <= toDrop) {
                    abandoned.append({firstBatch.batchId, firstBatch.records.size()});
                    toDrop -= firstBatch.records.size();
                    m_totalPendingRecords -= firstBatch.records.size();
                    m_pendingBatches.removeFirst();
                } else {
                    abandoned.append({firstBatch.batchId, toDrop});
                    for (int i = 0; i < toDrop; ++i) {
                        firstBatch.records.removeFirst();
                    }
                    m_totalPendingRecords -= toDrop;
                    toDrop = 0;
                }
            }

            // 2. 如果已有队列丢完后仍超出容量（即单次传入 batch 自身超过容量），截断传入记录的前部
            if (toDrop > 0) {
                const int dropFromIncoming = qMin(toDrop, recordsToQueue.size());
                abandoned.append({batchId, dropFromIncoming});
                recordsToQueue = recordsToQueue.mid(dropFromIncoming);
                toDrop -= dropFromIncoming;
            }

            dropped = overflow;
            m_droppedRecords += static_cast<quint64>(dropped);
        }

        if (!recordsToQueue.isEmpty()) {
            BatchItem item;
            item.batchId = batchId;
            item.records = recordsToQueue;
            item.retryCount = 0;
            m_pendingBatches.append(item);
            m_totalPendingRecords += recordsToQueue.size();
        }
    }

    for (const auto& item : abandoned) emit batchAbandoned(item.first, item.second, QStringLiteral("Queue capacity exceeded"));
    if (dropped > 0) {
        qWarning() << "Database queue capacity reached; records dropped:" << dropped;
        emit recordsDropped(dropped);
    }

    // 触发工作线程处理
    if (m_workerThread && m_workerThread->isRunning()) {
        QMetaObject::invokeMethod(this, "processWorkQueue", Qt::QueuedConnection);
    }

    return batchId;
}

int AsyncDatabaseWorker::pendingRecordCount() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_totalPendingRecords + m_activeRecords.load();
}

quint64 AsyncDatabaseWorker::droppedRecordCount() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_droppedRecords;
}

void AsyncDatabaseWorker::setMaxQueueRecords(int maxRecords)
{
    QMutexLocker locker(&m_queueMutex);
    m_maxQueueRecords = qMax(100, maxRecords);
}

int AsyncDatabaseWorker::maxQueueRecords() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_maxQueueRecords;
}

void AsyncDatabaseWorker::requestCleanup(int retentionDays, int chunkSize)
{
    if (!m_isRunning || m_cleanupScheduled.exchange(true)) return;
    m_cleanupCancelled = false;
    if (m_workerThread && m_workerThread->isRunning()) {
        QMetaObject::invokeMethod(this, [this, retentionDays, chunkSize]() {
            onCleanupRequested(retentionDays, chunkSize);
        }, Qt::QueuedConnection);
    }
}

void AsyncDatabaseWorker::cancelCleanup()
{
    m_cleanupCancelled = true;
}

void AsyncDatabaseWorker::setInjectedStorageDelayMs(int delayMs)
{
    m_injectedDelayMs.store(qMax(0, delayMs));
}

int AsyncDatabaseWorker::injectedStorageDelayMs() const
{
    return m_injectedDelayMs.load();
}

void AsyncDatabaseWorker::onThreadStarted()
{
    m_dbOpen = openDatabaseInThread();
    m_isRunning = m_dbOpen.load() && !m_stopRequested.load();
    {
        QMutexLocker lock(&m_startMutex);
        m_startComplete = true;
        m_startCondition.wakeAll();
    }
    emit startupFinished(m_dbOpen.load(), m_dbOpen ? QString() : m_db.lastError().text());
}

bool AsyncDatabaseWorker::openDatabaseInThread()
{
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open()) {
        emit workerError(QStringLiteral("open"), m_db.lastError().text());
        return false;
    }

    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL;"));
    pragma.exec(QStringLiteral("PRAGMA busy_timeout = 50;"));
    pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL;"));

    return true;
}

void AsyncDatabaseWorker::closeDatabaseInThread()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
    m_dbOpen = false;
}

void AsyncDatabaseWorker::processWorkQueue()
{
    if (m_processing || m_stopRequested) return;
    {
        QMutexLocker lock(&m_queueMutex);
        if (m_pendingBatches.isEmpty()) return;
        m_activeBatch = m_pendingBatches.takeFirst();
        m_totalPendingRecords -= m_activeBatch.records.size();
        m_activeRecords = m_activeBatch.records.size();
        m_processing = true;
    }
    QTimer::singleShot(m_injectedDelayMs.load(), this, [this]() { finishActiveBatch(); });
}

void AsyncDatabaseWorker::finishActiveBatch()
{
    if (m_stopRequested || m_activeBatch.records.isEmpty()) return;
    QString error;
    if (!executeBatchInsert(m_activeBatch.records, error)) {
        const int retry = ++m_activeBatch.retryCount;
        emit batchFailed(m_activeBatch.batchId, error);
        if (retry <= 3 && !m_stopRequested) {
            QTimer::singleShot(50 * (1 << (retry - 1)), this, [this]() { finishActiveBatch(); });
            return;
        }
        { QMutexLocker lock(&m_queueMutex); m_droppedRecords += m_activeBatch.records.size(); }
        qWarning() << "Database batch permanently failed" << m_activeBatch.batchId << "records" << m_activeBatch.records.size() << error;
        emit recordsDropped(m_activeBatch.records.size());
        emit batchAbandoned(m_activeBatch.batchId, m_activeBatch.records.size(), error);
    } else {
        emit batchCommitted(m_activeBatch.batchId, m_activeBatch.records.size());
    }
    m_activeBatch = {};
    m_activeRecords = 0;
    m_processing = false;
    QTimer::singleShot(0, this, &AsyncDatabaseWorker::processWorkQueue);
}

bool AsyncDatabaseWorker::submitHistoryTask(QObject* context, std::function<void()> task,
                                           std::shared_ptr<std::atomic_bool> cancelled)
{
    if (!context || !m_isRunning || m_stopRequested) return false;
    if (m_pendingHistoryTasks.fetch_add(1) >= 128) { --m_pendingHistoryTasks; return false; }
    quint64 barrier;
    { QMutexLocker lock(&m_queueMutex); barrier = m_nextBatchId - 1; }
    QPointer<QObject> guard(context);
    QMetaObject::invokeMethod(this, [this, guard, task, cancelled, barrier]() {
        if (guard) scheduleHistoryTask(guard, task, cancelled, barrier);
        else --m_pendingHistoryTasks;
    }, Qt::QueuedConnection);
    return true;
}

void AsyncDatabaseWorker::scheduleHistoryTask(QObject* context, std::function<void()> task,
                                             std::shared_ptr<std::atomic_bool> cancelled, quint64 barrier)
{
    if (m_stopRequested || (cancelled && cancelled->load())) { --m_pendingHistoryTasks; return; }
    bool pending = false;
    {
        QMutexLocker lock(&m_queueMutex);
        pending = m_activeRecords > 0 && m_activeBatch.batchId <= barrier;
        for (const auto& batch : m_pendingBatches) pending = pending || batch.batchId <= barrier;
    }
    // Wait only for writes accepted before this request, not for an endless live stream.
    if (pending) {
        QPointer<QObject> guard(context);
        QTimer::singleShot(10, this, [this, guard, task, cancelled, barrier]() {
            if (guard) scheduleHistoryTask(guard, task, cancelled, barrier);
            else --m_pendingHistoryTasks;
        });
        return;
    }
    task();
    --m_pendingHistoryTasks;
}

bool AsyncDatabaseWorker::executeBatchInsert(const QList<QVariantMap>& records, QString& errorText)
{
    if (!m_db.isOpen()) {
        errorText = QStringLiteral("Database not open in worker thread");
        return false;
    }

    if (!m_db.transaction()) {
        errorText = m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO runtime_data "
        "(timestamp, variable_name, value, unit, quality, value_valid, origin, error_code, error_text) "
        "VALUES (:timestamp, :varName, :value, :unit, :quality, :valueValid, :origin, :errorCode, :errorText)"));

    for (const auto& raw : records) {
        QVariantMap r;
        if (!normalizeRuntimeRecord(raw, r, errorText)) { m_db.rollback(); return false; }
        if (m_stopRequested) { errorText = QStringLiteral("Write cancelled at shutdown"); m_db.rollback(); return false; }
        bool numeric = false;
        const double value = r.value(QStringLiteral("value")).toDouble(&numeric);
        const bool valid = r.value(QStringLiteral("valueValid"), true).toBool();
        if (r.value(QStringLiteral("varName")).toString().trimmed().isEmpty()
                || (valid && (!numeric || !std::isfinite(value)))) {
            errorText = QStringLiteral("Invalid runtime record"); m_db.rollback(); return false;
        }
        const QVariant tsVar = r.value(QStringLiteral("timestamp"));
        QDateTime ts;
        if (tsVar.type() == QVariant::DateTime) {
            ts = tsVar.toDateTime();
        } else if (tsVar.type() == QVariant::String) {
            ts = QDateTime::fromString(tsVar.toString(), Qt::ISODateWithMs);
            if (!ts.isValid()) {
                ts = QDateTime::fromString(tsVar.toString(), Qt::ISODate);
            }
        }
        query.bindValue(QStringLiteral(":timestamp"), formatIsoUtc(ts));
        query.bindValue(QStringLiteral(":varName"), r.value(QStringLiteral("varName")));
        query.bindValue(QStringLiteral(":value"), valid ? QVariant(value) : QVariant());

        const QString unit = r.value(QStringLiteral("unit")).toString();
        query.bindValue(QStringLiteral(":unit"), unit.isNull() ? QStringLiteral("") : unit);

        const QString quality = r.value(QStringLiteral("quality")).toString();
        query.bindValue(QStringLiteral(":quality"), quality.isEmpty() ? QStringLiteral("Good") : quality);
        query.bindValue(QStringLiteral(":valueValid"), r.value(QStringLiteral("valueValid"), true).toBool() ? 1 : 0);

        const QString origin = r.value(QStringLiteral("origin")).toString();
        query.bindValue(QStringLiteral(":origin"), origin.isNull() ? QStringLiteral("") : origin);

        const QString errorCode = r.value(QStringLiteral("errorCode")).toString();
        query.bindValue(QStringLiteral(":errorCode"), errorCode.isNull() ? QStringLiteral("") : errorCode);

        const QString errorTextVal = r.value(QStringLiteral("errorText")).toString();
        query.bindValue(QStringLiteral(":errorText"), errorTextVal.isNull() ? QStringLiteral("") : errorTextVal);

        if (!query.exec()) {
            errorText = query.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        errorText = m_db.lastError().text();
        m_db.rollback();
        return false;
    }

    return true;
}

void AsyncDatabaseWorker::onCleanupRequested(int retentionDays, int chunkSize)
{
    if (m_stopRequested) return;
    const int limit = qBound(1, chunkSize, 1000);
    const CleanupResult res = doCleanupInThread(retentionDays, limit);
    m_cleanupTotal += res.totalDeleted;
    if (res.totalDeleted > 0) emit cleanupBatchCompleted(res.totalDeleted);
    if (!res.success) { m_cleanupScheduled = false; emit cleanupFailed(res.errorText); m_cleanupTotal = 0; }
    else if (res.cancelled || res.totalDeleted == 0) {
        m_cleanupScheduled = false; emit cleanupCompleted(m_cleanupTotal, res.cancelled); m_cleanupTotal = 0;
    } else {
        QTimer::singleShot(0, this, [this, retentionDays, limit]() { onCleanupRequested(retentionDays, limit); });
    }
}

AsyncDatabaseWorker::CleanupResult AsyncDatabaseWorker::doCleanupInThread(int retentionDays, int chunkSize)
{
    CleanupResult result;
    if (!m_db.isOpen() || retentionDays <= 0) {
        result.errorText = QStringLiteral("Database not open or invalid retention days");
        return result;
    }

    if (chunkSize <= 0) {
        chunkSize = DEFAULT_CHUNK_SIZE;
    }

    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-retentionDays);
    const QString cutoffText = formatIsoUtc(cutoff);

    int totalDeleted = 0;

    while (!m_stopRequested.load()) {
        if (m_cleanupCancelled.load()) {
            result.cancelled = true;
            break;
        }

        if (!m_db.transaction()) {
            result.errorText = m_db.lastError().text();
            return result;
        }

        QSqlQuery runtimeQuery(m_db);
        runtimeQuery.prepare(QStringLiteral(
            "DELETE FROM runtime_data WHERE id IN ("
            "SELECT id FROM runtime_data WHERE timestamp < :cutoff ORDER BY timestamp ASC, id ASC LIMIT :limit)"));
        runtimeQuery.bindValue(QStringLiteral(":cutoff"), cutoffText);
        runtimeQuery.bindValue(QStringLiteral(":limit"), chunkSize);

        if (!runtimeQuery.exec()) {
            result.errorText = runtimeQuery.lastError().text();
            m_db.rollback();
            return result;
        }
        const int runtimeDeleted = runtimeQuery.numRowsAffected();

        QSqlQuery logsQuery(m_db);
        logsQuery.prepare(QStringLiteral(
            "DELETE FROM system_logs WHERE id IN ("
            "SELECT id FROM system_logs WHERE timestamp < :cutoff ORDER BY timestamp ASC, id ASC LIMIT :limit)"));
        logsQuery.bindValue(QStringLiteral(":cutoff"), cutoffText);
        logsQuery.bindValue(QStringLiteral(":limit"), chunkSize);

        if (!logsQuery.exec()) {
            result.errorText = logsQuery.lastError().text();
            m_db.rollback();
            return result;
        }
        const int logsDeleted = logsQuery.numRowsAffected();

        if (!m_db.commit()) {
            result.errorText = m_db.lastError().text();
            m_db.rollback();
            return result;
        }

        totalDeleted += (runtimeDeleted + logsDeleted);
        break; // Yield to queued reads/cancellation between short transactions.
    }

    result.totalDeleted = totalDeleted;
    result.success = true;
    return result;
}

RuntimeHistoryPage AsyncDatabaseWorker::queryHistoryPage(
    const QString& channelName,
    const QDateTime& start,
    const QDateTime& end,
    int pageSize,
    const RuntimeHistoryCursor& cursor,
    const std::atomic_bool* cancelToken)
{
    if (QThread::currentThread() != m_workerThread) {
        RuntimeHistoryPage page;
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = QStringLiteral("ASYNC_REQUEST_REQUIRED");
        page.errorText = QStringLiteral("Use the asynchronous history request API from the caller thread");
        return page;
    }

    RuntimeHistoryPage page;
    if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
        page.status = RuntimeHistoryPageStatus::Cancelled;
        page.errorCode = QStringLiteral("CANCELLED");
        page.errorText = QStringLiteral("Query was cancelled");
        return page;
    }

    if (!m_db.isOpen()) {
        page.status = RuntimeHistoryPageStatus::NotInitialized;
        page.errorCode = QStringLiteral("NOT_INITIALIZED");
        page.errorText = QStringLiteral("Worker database is not open");
        return page;
    }

    if (pageSize <= 0 || pageSize > 10000) {
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = QStringLiteral("INVALID_PAGE_SIZE");
        return page;
    }

    const QString startText = formatIsoUtc(start.isValid() ? start.toUTC() : QDateTime::fromMSecsSinceEpoch(0, Qt::UTC));
    const QString endText = formatIsoUtc(end.isValid() ? end.toUTC() : QDateTime::currentDateTimeUtc());

    QString sql = QStringLiteral("SELECT ") + RUNTIME_RECORD_SELECT_COLUMNS + QStringLiteral(
        " FROM runtime_data "
        "WHERE variable_name = :varName AND timestamp >= :start AND timestamp <= :end ");

    if (cursor.isValid()) {
        sql += QStringLiteral("AND (timestamp > :cursorTime OR (timestamp = :cursorTime AND id > :cursorId)) ");
    }
    if (cursor.maxId >= 0) {
        sql += QStringLiteral("AND id <= :maxId ");
    }

    sql += QStringLiteral("ORDER BY timestamp ASC, id ASC LIMIT :limit");

    QSqlQuery query(m_db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":varName"), channelName);
    query.bindValue(QStringLiteral(":start"), startText);
    query.bindValue(QStringLiteral(":end"), endText);
    if (cursor.isValid()) {
        query.bindValue(QStringLiteral(":cursorTime"), formatIsoUtc(cursor.timestamp.toUTC()));
        query.bindValue(QStringLiteral(":cursorId"), cursor.id);
    }
    if (cursor.maxId >= 0) {
        query.bindValue(QStringLiteral(":maxId"), cursor.maxId);
    }
    query.bindValue(QStringLiteral(":limit"), pageSize + 1);

    if (!query.exec()) {
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = query.lastError().nativeErrorCode();
        page.errorText = query.lastError().text();
        return page;
    }

    QList<RuntimeRecord> records;
    while (query.next()) {
        if (m_stopRequested || (cancelToken && cancelToken->load())) {
            page.status = RuntimeHistoryPageStatus::Cancelled; return page;
        }
        records.append(runtimeRecordFromSql(query));
    }

    page.hasMore = records.size() > pageSize;
    if (page.hasMore) {
        records.removeLast();
    }

    page.records = records;
    page.status = RuntimeHistoryPageStatus::Success;

    if (!page.records.isEmpty()) {
        page.nextCursor.timestamp = page.records.last().timestamp;
        page.nextCursor.id = page.records.last().id;
        page.nextCursor.maxId = cursor.maxId;
    }

    return page;
}

RuntimeHistoryPage AsyncDatabaseWorker::queryLatestHistoryPage(
    const QString& channelName,
    int maxCount,
    int pageSize,
    const RuntimeHistoryCursor& cursor,
    const QDateTime& end,
    const std::atomic_bool* cancelToken)
{
    if (QThread::currentThread() != m_workerThread) {
        RuntimeHistoryPage page;
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = QStringLiteral("ASYNC_REQUEST_REQUIRED");
        page.errorText = QStringLiteral("Use the asynchronous history request API from the caller thread");
        return page;
    }

    RuntimeHistoryPage page;
    if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
        page.status = RuntimeHistoryPageStatus::Cancelled;
        page.errorCode = QStringLiteral("CANCELLED");
        page.errorText = QStringLiteral("Query was cancelled");
        return page;
    }

    if (!m_db.isOpen()) {
        page.status = RuntimeHistoryPageStatus::NotInitialized;
        page.errorCode = QStringLiteral("NOT_INITIALIZED");
        page.errorText = QStringLiteral("Worker database is not open");
        return page;
    }

    if (pageSize <= 0 || pageSize > 10000 || maxCount <= 0) {
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = QStringLiteral("INVALID_PARAM");
        return page;
    }

    const QString endText = end.isValid() ? formatIsoUtc(end.toUTC()) : QString();

    QString subquery = QStringLiteral("SELECT ") + RUNTIME_RECORD_SELECT_COLUMNS + QStringLiteral(
        " FROM runtime_data WHERE variable_name = :varName ");
    if (!endText.isEmpty()) {
        subquery += QStringLiteral("AND timestamp <= :end ");
    }
    if (cursor.maxId >= 0) {
        subquery += QStringLiteral("AND id <= :maxId ");
    }
    subquery += QStringLiteral("ORDER BY timestamp DESC, id DESC LIMIT :maxCount");

    QString sql = QStringLiteral("SELECT * FROM (") + subquery + QStringLiteral(") WHERE 1=1 ");
    if (cursor.isValid()) {
        sql += QStringLiteral("AND (timestamp > :cursorTime OR (timestamp = :cursorTime AND id > :cursorId)) ");
    }
    if (cursor.maxId >= 0) {
        sql += QStringLiteral("AND id <= :maxId ");
    }
    sql += QStringLiteral("ORDER BY timestamp ASC, id ASC LIMIT :limit");

    QSqlQuery query(m_db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":varName"), channelName);
    if (!endText.isEmpty()) {
        query.bindValue(QStringLiteral(":end"), endText);
    }
    query.bindValue(QStringLiteral(":maxCount"), maxCount);
    if (cursor.isValid()) {
        query.bindValue(QStringLiteral(":cursorTime"), formatIsoUtc(cursor.timestamp.toUTC()));
        query.bindValue(QStringLiteral(":cursorId"), cursor.id);
    }
    if (cursor.maxId >= 0) {
        query.bindValue(QStringLiteral(":maxId"), cursor.maxId);
    }
    query.bindValue(QStringLiteral(":limit"), pageSize + 1);

    if (!query.exec()) {
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = query.lastError().nativeErrorCode();
        page.errorText = query.lastError().text();
        return page;
    }

    QList<RuntimeRecord> records;
    while (query.next()) {
        if (m_stopRequested || (cancelToken && cancelToken->load())) {
            page.status = RuntimeHistoryPageStatus::Cancelled; return page;
        }
        records.append(runtimeRecordFromSql(query));
    }

    page.hasMore = records.size() > pageSize;
    if (page.hasMore) {
        records.removeLast();
    }

    page.records = records;
    page.status = RuntimeHistoryPageStatus::Success;

    if (!page.records.isEmpty()) {
        page.nextCursor.timestamp = page.records.last().timestamp;
        page.nextCursor.id = page.records.last().id;
        page.nextCursor.maxId = cursor.maxId;
    }

    return page;
}

qint64 AsyncDatabaseWorker::latestRecordId()
{
    if (QThread::currentThread() != m_workerThread || !m_dbOpen) return -1;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT COALESCE(MAX(id), 0) FROM runtime_data")) || !query.next()) return -1;
    return query.value(0).toLongLong();
}

RuntimeHistoryCount AsyncDatabaseWorker::countHistory(
    const QString& channelName,
    const QDateTime& start,
    const QDateTime& end,
    qint64 maxRecordId)
{
    if (QThread::currentThread() != m_workerThread) {
        RuntimeHistoryCount count;
        count.status = RuntimeHistoryPageStatus::SqlError;
        count.errorCode = QStringLiteral("ASYNC_REQUEST_REQUIRED");
        count.errorText = QStringLiteral("Use the asynchronous history request API from the caller thread");
        return count;
    }

    RuntimeHistoryCount count;
    if (!m_db.isOpen()) {
        count.status = RuntimeHistoryPageStatus::NotInitialized;
        count.errorCode = QStringLiteral("NOT_INITIALIZED");
        return count;
    }

    const QString startText = formatIsoUtc(start.isValid() ? start.toUTC() : QDateTime::fromMSecsSinceEpoch(0, Qt::UTC));
    const QString endText = formatIsoUtc(end.isValid() ? end.toUTC() : QDateTime::currentDateTimeUtc());

    QString sql = QStringLiteral(
        "SELECT COUNT(*) FROM runtime_data "
        "WHERE variable_name = :varName AND timestamp >= :start AND timestamp <= :end ");
    if (maxRecordId >= 0) {
        sql += QStringLiteral("AND id <= :maxId");
    }

    QSqlQuery query(m_db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":varName"), channelName);
    query.bindValue(QStringLiteral(":start"), startText);
    query.bindValue(QStringLiteral(":end"), endText);
    if (maxRecordId >= 0) {
        query.bindValue(QStringLiteral(":maxId"), maxRecordId);
    }

    if (!query.exec() || !query.next()) {
        count.status = RuntimeHistoryPageStatus::SqlError;
        count.errorCode = query.lastError().nativeErrorCode();
        count.errorText = query.lastError().text();
        return count;
    }

    count.status = RuntimeHistoryPageStatus::Success;
    count.count = query.value(0).toLongLong();
    return count;
}

RuntimeHistoryCount AsyncDatabaseWorker::countLatestHistory(
    const QString& channelName,
    int maxCount,
    const QDateTime& end,
    qint64 maxRecordId)
{
    if (QThread::currentThread() != m_workerThread) {
        RuntimeHistoryCount count;
        count.status = RuntimeHistoryPageStatus::SqlError;
        count.errorCode = QStringLiteral("ASYNC_REQUEST_REQUIRED");
        count.errorText = QStringLiteral("Use the asynchronous history request API from the caller thread");
        return count;
    }

    RuntimeHistoryCount count;
    if (!m_db.isOpen()) {
        count.status = RuntimeHistoryPageStatus::NotInitialized;
        count.errorCode = QStringLiteral("NOT_INITIALIZED");
        return count;
    }

    const QString endText = end.isValid() ? formatIsoUtc(end.toUTC()) : QString();

    QString sql = QStringLiteral("SELECT COUNT(*) FROM runtime_data WHERE variable_name = :varName ");
    if (!endText.isEmpty()) {
        sql += QStringLiteral("AND timestamp <= :end ");
    }
    if (maxRecordId >= 0) {
        sql += QStringLiteral("AND id <= :maxId ");
    }

    QSqlQuery query(m_db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":varName"), channelName);
    if (!endText.isEmpty()) {
        query.bindValue(QStringLiteral(":end"), endText);
    }
    if (maxRecordId >= 0) {
        query.bindValue(QStringLiteral(":maxId"), maxRecordId);
    }

    if (!query.exec() || !query.next()) {
        count.status = RuntimeHistoryPageStatus::SqlError;
        count.errorCode = query.lastError().nativeErrorCode();
        count.errorText = query.lastError().text();
        return count;
    }

    const qint64 total = query.value(0).toLongLong();
    count.status = RuntimeHistoryPageStatus::Success;
    count.count = (maxCount > 0) ? qMin(static_cast<qint64>(maxCount), total) : total;
    return count;
}

QList<RuntimeRecord> AsyncDatabaseWorker::getLatestRecords(const QString& channelName, int count)
{
    if (QThread::currentThread() != m_workerThread) return {};

    if (!m_db.isOpen() || count <= 0) {
        return {};
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT ") + RUNTIME_RECORD_SELECT_COLUMNS + QStringLiteral(
        " FROM runtime_data WHERE variable_name = :varName ORDER BY timestamp DESC, id DESC LIMIT :limit"));
    query.bindValue(QStringLiteral(":varName"), channelName);
    query.bindValue(QStringLiteral(":limit"), count);

    if (!query.exec()) {
        return {};
    }

    QList<RuntimeRecord> out;
    while (!m_stopRequested && query.next()) {
        out.append(runtimeRecordFromSql(query));
    }
    return out;
}

QList<RuntimeRecord> AsyncDatabaseWorker::queryHistory(
    const QString& channelName,
    const QDateTime& start,
    const QDateTime& end)
{
    if (QThread::currentThread() != m_workerThread) return {};

    if (!m_db.isOpen()) {
        return {};
    }

    const QString startText = formatIsoUtc(start.isValid() ? start.toUTC() : QDateTime::fromMSecsSinceEpoch(0, Qt::UTC));
    const QString endText = formatIsoUtc(end.isValid() ? end.toUTC() : QDateTime::currentDateTimeUtc());

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT ") + RUNTIME_RECORD_SELECT_COLUMNS + QStringLiteral(
        " FROM runtime_data "
        "WHERE variable_name = :varName AND timestamp >= :start AND timestamp <= :end "
        "ORDER BY timestamp ASC, id ASC"));
    query.bindValue(QStringLiteral(":varName"), channelName);
    query.bindValue(QStringLiteral(":start"), startText);
    query.bindValue(QStringLiteral(":end"), endText);

    if (!query.exec()) {
        return {};
    }

    QList<RuntimeRecord> out;
    while (!m_stopRequested && query.next()) {
        out.append(runtimeRecordFromSql(query));
    }
    return out;
}

} // namespace Core
