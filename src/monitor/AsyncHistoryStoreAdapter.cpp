#include "AsyncHistoryStoreAdapter.h"

namespace Monitor {

AsyncHistoryStoreAdapter::AsyncHistoryStoreAdapter(Core::AsyncDatabaseWorker* worker)
    : m_worker(worker)
{
}

bool AsyncHistoryStoreAdapter::isAvailable() const
{
    return m_worker && m_worker->isDbOpen();
}

QList<RuntimeRecord> AsyncHistoryStoreAdapter::getLatestRecords(const QString& channelName, int count)
{
    if (!isAvailable()) {
        return {};
    }
    return m_worker->getLatestRecords(channelName, count);
}

QList<RuntimeRecord> AsyncHistoryStoreAdapter::queryHistory(const QString& channelName,
                                                           const QDateTime& start,
                                                           const QDateTime& end)
{
    if (!isAvailable()) {
        return {};
    }
    return m_worker->queryHistory(channelName, start, end);
}

RuntimeHistoryPage AsyncHistoryStoreAdapter::queryHistoryPage(
    const QString& channelName,
    const QDateTime& start,
    const QDateTime& end,
    int pageSize,
    const RuntimeHistoryCursor& cursor)
{
    if (!isAvailable()) {
        RuntimeHistoryPage page;
        page.status = RuntimeHistoryPageStatus::NotInitialized;
        page.errorCode = QStringLiteral("NOT_INITIALIZED");
        page.errorText = QStringLiteral("Async database worker is not available");
        return page;
    }
    return m_worker->queryHistoryPage(channelName, start, end, pageSize, cursor, &m_cancelled);
}

RuntimeHistoryPage AsyncHistoryStoreAdapter::queryLatestHistoryPage(
    const QString& channelName,
    int maxCount,
    int pageSize,
    const RuntimeHistoryCursor& cursor,
    const QDateTime& end)
{
    if (!isAvailable()) {
        RuntimeHistoryPage page;
        page.status = RuntimeHistoryPageStatus::NotInitialized;
        page.errorCode = QStringLiteral("NOT_INITIALIZED");
        page.errorText = QStringLiteral("Async database worker is not available");
        return page;
    }
    return m_worker->queryLatestHistoryPage(channelName, maxCount, pageSize, cursor, end, &m_cancelled);
}

RuntimeHistoryCount AsyncHistoryStoreAdapter::countHistory(
    const QString& channelName,
    const QDateTime& start,
    const QDateTime& end,
    qint64 maxRecordId)
{
    if (!isAvailable()) {
        RuntimeHistoryCount count;
        count.status = RuntimeHistoryPageStatus::NotInitialized;
        count.errorCode = QStringLiteral("NOT_INITIALIZED");
        count.errorText = QStringLiteral("Async database worker is not available");
        return count;
    }
    return m_worker->countHistory(channelName, start, end, maxRecordId);
}

RuntimeHistoryCount AsyncHistoryStoreAdapter::countLatestHistory(
    const QString& channelName,
    int maxCount,
    const QDateTime& end,
    qint64 maxRecordId)
{
    if (!isAvailable()) {
        RuntimeHistoryCount count;
        count.status = RuntimeHistoryPageStatus::NotInitialized;
        count.errorCode = QStringLiteral("NOT_INITIALIZED");
        count.errorText = QStringLiteral("Async database worker is not available");
        return count;
    }
    return m_worker->countLatestHistory(channelName, maxCount, end, maxRecordId);
}

void AsyncHistoryStoreAdapter::requestPage(QObject* context, const QString& channel, const QDateTime& start,
    const QDateTime& end, int size, const RuntimeHistoryCursor& cursor, int latestCount,
    std::function<void(RuntimeHistoryPage)> completed)
{
    if (!isAvailable()) { completed({}); return; }
    const auto token = m_requestCancellation;
    QPointer<QObject> guard(context);
    auto* worker = m_worker.data();
    const bool accepted = worker->submitHistoryTask(context, [=]() {
        RuntimeHistoryPage page = latestCount > 0
            ? worker->queryLatestHistoryPage(channel, latestCount, size, cursor, end, token.get())
            : worker->queryHistoryPage(channel, start, end, size, cursor, token.get());
        if (guard) QMetaObject::invokeMethod(guard, [=]() {
            if (token->load()) {
                RuntimeHistoryPage cancelled; cancelled.status = RuntimeHistoryPageStatus::Cancelled;
                completed(cancelled);
            } else completed(page);
        }, Qt::QueuedConnection);
    });
    if (!accepted) {
        RuntimeHistoryPage page; page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = QStringLiteral("HISTORY_QUEUE_UNAVAILABLE");
        page.errorText = QStringLiteral("History service is stopping or its request queue is full");
        completed(page);
    }
}

void AsyncHistoryStoreAdapter::cancelPendingRequests()
{
    m_requestCancellation->store(true);
    m_requestCancellation = std::make_shared<std::atomic_bool>(false);
}

} // namespace Monitor
