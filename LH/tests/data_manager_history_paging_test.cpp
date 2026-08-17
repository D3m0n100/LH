#include <QtTest/QtTest>

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "core/DataManager.h"

class DataManagerHistoryPagingTest : public QObject
{
    Q_OBJECT

private slots:
    void keysetPagingKeepsOrderAndClosedBounds()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        DataManager& manager = DataManager::instance();
        QVERIFY(manager.initialize(tempDir.filePath(QStringLiteral("history.db"))));

        const QDateTime first = QDateTime::fromMSecsSinceEpoch(1000, Qt::UTC);
        const QDateTime same = QDateTime::fromMSecsSinceEpoch(2000, Qt::UTC);
        const QDateTime last = QDateTime::fromMSecsSinceEpoch(3000, Qt::UTC);
        QList<QVariantMap> records;
        auto append = [&](const QDateTime& timestamp, double value) {
            QVariantMap record;
            record.insert(QStringLiteral("varName"), QStringLiteral("v"));
            record.insert(QStringLiteral("value"), value);
            record.insert(QStringLiteral("timestamp"), timestamp);
            records.append(record);
        };
        append(first, 1.0);
        append(same, 2.0);
        append(same, 3.0);
        append(same, 4.0);
        append(last, 5.0);
        QVERIFY(manager.logRuntimeDataBatch(records).success);

        const QList<RuntimeRecord> complete = manager.queryHistory("v", first, last);
        QCOMPARE(complete.size(), 5);

        const RuntimeHistoryCount count = manager.countHistory("v", first, last);
        QCOMPARE(count.status, RuntimeHistoryPageStatus::Success);
        QCOMPARE(count.count, qint64(5));

        QList<RuntimeRecord> paged;
        RuntimeHistoryCursor cursor;
        bool hasMore = true;
        while (hasMore) {
            const RuntimeHistoryPage page = manager.queryHistoryPage(
                QStringLiteral("v"), first, last, 2, cursor);
            QCOMPARE(page.status, RuntimeHistoryPageStatus::Success);
            QVERIFY(page.records.size() <= 2);
            paged.append(page.records);
            hasMore = page.hasMore;
            cursor = page.nextCursor;
        }

        QCOMPARE(paged.size(), complete.size());
        for (int i = 0; i < paged.size(); ++i) {
            QCOMPARE(paged.at(i).id, complete.at(i).id);
            QCOMPARE(paged.at(i).timestamp, complete.at(i).timestamp);
        }
        QVERIFY(!paged.isEmpty());
        QVERIFY(paged.first().timestamp >= first);
        QVERIFY(paged.last().timestamp <= last);

        const RuntimeHistoryPage exactStart = manager.queryHistoryPage(
            QStringLiteral("v"), first, first, 10);
        QCOMPARE(exactStart.status, RuntimeHistoryPageStatus::Success);
        QCOMPARE(exactStart.records.size(), 1);
        QCOMPARE(exactStart.records.first().timestamp, first);

        const RuntimeHistoryPage exactEnd = manager.queryHistoryPage(
            QStringLiteral("v"), last, last, 10);
        QCOMPARE(exactEnd.status, RuntimeHistoryPageStatus::Success);
        QCOMPARE(exactEnd.records.size(), 1);
        QCOMPARE(exactEnd.records.first().timestamp, last);

        const RuntimeHistoryPage invalidPageSize = manager.queryHistoryPage(
            QStringLiteral("v"), first, last, 0);
        QCOMPARE(invalidPageSize.status, RuntimeHistoryPageStatus::SqlError);
        QVERIFY(!invalidPageSize.errorText.isEmpty());

        const RuntimeHistoryPage emptyPage = manager.queryHistoryPage(
            QStringLiteral("v"), last.addMSecs(1), last.addMSecs(2), 2);
        QCOMPARE(emptyPage.status, RuntimeHistoryPageStatus::Success);
        QVERIFY(emptyPage.records.isEmpty());
        QVERIFY(emptyPage.isEnd());

        manager.shutdown();
        const RuntimeHistoryPage uninitialized = manager.queryHistoryPage(
            QStringLiteral("v"), first, last, 2);
        QCOMPARE(uninitialized.status, RuntimeHistoryPageStatus::NotInitialized);
        const RuntimeHistoryCount uninitializedCount = manager.countHistory(
            QStringLiteral("v"), first, last);
        QCOMPARE(uninitializedCount.status, RuntimeHistoryPageStatus::NotInitialized);
    }

    void sqlFailureIsNotAnEmptyPage()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        DataManager& manager = DataManager::instance();
        const QString path = tempDir.filePath(QStringLiteral("history_error.db"));
        QVERIFY(manager.initialize(path));

        const QString connectionName = QStringLiteral("history_paging_mutator");
        {
            QSqlDatabase mutator = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                               connectionName);
            mutator.setDatabaseName(path);
            QVERIFY(mutator.open());
            QSqlQuery query(mutator);
            QVERIFY(query.exec(QStringLiteral("DROP TABLE runtime_data")));
            mutator.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        const RuntimeHistoryPage page = manager.queryHistoryPage(
            QStringLiteral("v"), QDateTime::fromMSecsSinceEpoch(0, Qt::UTC),
            QDateTime::currentDateTimeUtc(), 10);
        QCOMPARE(page.status, RuntimeHistoryPageStatus::SqlError);
        QVERIFY(!page.errorText.isEmpty());

        const RuntimeHistoryCount count = manager.countHistory(
            QStringLiteral("v"), QDateTime::fromMSecsSinceEpoch(0, Qt::UTC),
            QDateTime::currentDateTimeUtc());
        QCOMPARE(count.status, RuntimeHistoryPageStatus::SqlError);
        QVERIFY(!count.errorText.isEmpty());

        manager.shutdown();
    }

    void latestPagingIsAscendingAndBounded()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        DataManager& manager = DataManager::instance();
        QVERIFY(manager.initialize(tempDir.filePath(QStringLiteral("latest.db"))));

        const QDateTime base = QDateTime::fromMSecsSinceEpoch(10000, Qt::UTC);
        QList<QVariantMap> records;
        for (int i = 0; i < 8; ++i) {
            QVariantMap record;
            record.insert(QStringLiteral("varName"), QStringLiteral("latest"));
            record.insert(QStringLiteral("value"), i);
            record.insert(QStringLiteral("timestamp"), base.addMSecs(i / 2));
            records.append(record);
        }
        QVERIFY(manager.logRuntimeDataBatch(records).success);

        QVariantMap futureRecord;
        futureRecord.insert(QStringLiteral("varName"), QStringLiteral("latest"));
        futureRecord.insert(QStringLiteral("value"), 99.0);
        futureRecord.insert(QStringLiteral("timestamp"), base.addMSecs(4));
        QList<QVariantMap> futureRecords;
        futureRecords.append(futureRecord);
        QVERIFY(manager.logRuntimeDataBatch(futureRecords).success);

        QList<RuntimeRecord> paged;
        RuntimeHistoryCursor cursor;
        bool hasMore = true;
        const QDateTime fixedEnd = base.addMSecs(3);
        while (hasMore) {
            const RuntimeHistoryPage page = manager.queryLatestHistoryPage(
                QStringLiteral("latest"), 5, 2, cursor, fixedEnd);
            QCOMPARE(page.status, RuntimeHistoryPageStatus::Success);
            QVERIFY(page.records.size() <= 2);
            paged.append(page.records);
            cursor = page.nextCursor;
            hasMore = page.hasMore;
        }

        QCOMPARE(paged.size(), 5);
        for (const RuntimeRecord& record : paged) {
            QVERIFY(record.value != 99.0);
            QVERIFY(record.timestamp <= fixedEnd);
        }
        for (int i = 1; i < paged.size(); ++i) {
            QVERIFY(paged.at(i - 1).timestamp < paged.at(i).timestamp
                    || (paged.at(i - 1).timestamp == paged.at(i).timestamp
                        && paged.at(i - 1).id < paged.at(i).id));
        }
        QCOMPARE(paged.first().value, 3.0);
        QCOMPARE(paged.last().value, 7.0);

        const RuntimeHistoryCount count = manager.countLatestHistory(
            QStringLiteral("latest"), 5, fixedEnd);
        QCOMPARE(count.status, RuntimeHistoryPageStatus::Success);
        QCOMPARE(count.count, qint64(5));

        manager.shutdown();
    }

    void provenanceFieldsAreStableAcrossReadPaths()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        DataManager& manager = DataManager::instance();
        QVERIFY(manager.initialize(tempDir.filePath(QStringLiteral("provenance.db"))));

        const QDateTime first = QDateTime::fromMSecsSinceEpoch(20000, Qt::UTC);
        const QDateTime second = first.addMSecs(1);

        QVariantMap firstMetadata;
        firstMetadata.insert(QStringLiteral("origin"), QStringLiteral("source-A"));
        firstMetadata.insert(QStringLiteral("errorCodeName"), QStringLiteral("CodeA"));
        firstMetadata.insert(QStringLiteral("error"), QStringLiteral("error-A"));
        firstMetadata.insert(QStringLiteral("errorDetails"), QStringLiteral("detail-A"));

        QVariantMap firstRecord;
        firstRecord.insert(QStringLiteral("varName"), QStringLiteral("mixed"));
        firstRecord.insert(QStringLiteral("value"), 1.0);
        firstRecord.insert(QStringLiteral("valueValid"), true);
        firstRecord.insert(QStringLiteral("quality"), QStringLiteral("Good"));
        firstRecord.insert(QStringLiteral("timestamp"), first);
        firstRecord.insert(QStringLiteral("metadata"), firstMetadata);

        QVariantMap secondRecord;
        secondRecord.insert(QStringLiteral("varName"), QStringLiteral("mixed"));
        secondRecord.insert(QStringLiteral("value"), 2.0);
        secondRecord.insert(QStringLiteral("valueValid"), false);
        secondRecord.insert(QStringLiteral("quality"), QStringLiteral("Bad"));
        secondRecord.insert(QStringLiteral("timestamp"), second);
        secondRecord.insert(QStringLiteral("origin"), QStringLiteral("source-B"));
        secondRecord.insert(QStringLiteral("errorCode"), QStringLiteral("CodeB"));
        secondRecord.insert(QStringLiteral("errorText"), QStringLiteral("error-B"));

        QVERIFY(manager.logRuntimeDataBatch({firstRecord, secondRecord}).success);

        const QList<RuntimeRecord> complete = manager.queryHistory(
            QStringLiteral("mixed"), first, second);
        QCOMPARE(complete.size(), 2);
        QCOMPARE(complete.at(0).origin, QStringLiteral("source-A"));
        QCOMPARE(complete.at(0).errorCode, QStringLiteral("CodeA"));
        QCOMPARE(complete.at(0).errorText,
                 QStringLiteral("error-A (details: detail-A)"));
        QCOMPARE(complete.at(1).origin, QStringLiteral("source-B"));
        QCOMPARE(complete.at(1).errorCode, QStringLiteral("CodeB"));
        QCOMPARE(complete.at(1).errorText, QStringLiteral("error-B"));
        QVERIFY(!complete.at(1).valueValid);
        QCOMPARE(complete.at(1).quality, RuntimePointQuality::Bad);

        const QList<RuntimeRecord> latest = manager.getLatestRecords(
            QStringLiteral("mixed"), 2);
        QCOMPARE(latest.size(), 2);
        QCOMPARE(latest.at(0).origin, QStringLiteral("source-B"));
        QCOMPARE(latest.at(0).errorCode, QStringLiteral("CodeB"));
        QCOMPARE(latest.at(1).origin, QStringLiteral("source-A"));
        QCOMPARE(latest.at(1).errorCode, QStringLiteral("CodeA"));

        const QList<RuntimeRecord> since = manager.getRecordsSince(
            QStringLiteral("mixed"), first.addMSecs(-1));
        QCOMPARE(since.size(), 2);
        QCOMPARE(since.at(0).errorText,
                 QStringLiteral("error-A (details: detail-A)"));
        QCOMPARE(since.at(1).errorText, QStringLiteral("error-B"));

        QList<RuntimeRecord> paged;
        RuntimeHistoryCursor cursor;
        bool hasMore = true;
        while (hasMore) {
            const RuntimeHistoryPage page = manager.queryHistoryPage(
                QStringLiteral("mixed"), first, second, 1, cursor);
            QCOMPARE(page.status, RuntimeHistoryPageStatus::Success);
            QCOMPARE(page.records.size(), 1);
            paged.append(page.records.first());
            cursor = page.nextCursor;
            hasMore = page.hasMore;
        }
        QCOMPARE(paged.size(), 2);
        QCOMPARE(paged.at(0).errorCode, QStringLiteral("CodeA"));
        QCOMPARE(paged.at(1).errorCode, QStringLiteral("CodeB"));

        QList<RuntimeRecord> latestPaged;
        cursor.clear();
        hasMore = true;
        while (hasMore) {
            const RuntimeHistoryPage page = manager.queryLatestHistoryPage(
                QStringLiteral("mixed"), 2, 1, cursor);
            QCOMPARE(page.status, RuntimeHistoryPageStatus::Success);
            QCOMPARE(page.records.size(), 1);
            latestPaged.append(page.records.first());
            cursor = page.nextCursor;
            hasMore = page.hasMore;
        }
        QCOMPARE(latestPaged.size(), 2);
        QCOMPARE(latestPaged.at(0).origin, QStringLiteral("source-A"));
        QCOMPARE(latestPaged.at(1).origin, QStringLiteral("source-B"));

        manager.shutdown();
    }
};

QTEST_MAIN(DataManagerHistoryPagingTest)
#include "data_manager_history_paging_test.moc"
