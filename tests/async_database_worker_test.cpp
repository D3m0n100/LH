/**
 * @file async_database_worker_test.cpp
 * @brief T12 数据库异步执行、受限队列与分批清理测试
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <atomic>

#include "core/DataManager.h"
#include "core/AsyncDatabaseWorker.h"
#include "monitor/AsyncHistoryStoreAdapter.h"
#include "monitor/MonitorManager.h"

using namespace Core;
using namespace Monitor;

class AsyncDatabaseWorkerTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    QString createTestDatabase(const QString& name)
    {
        const QString dbPath = m_tempDir.filePath(name);
        // 初始化 schema
        DataManager& dm = DataManager::instance();
        dm.initialize(dbPath);
        dm.shutdown();
        return dbPath;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
    }

    void testSqlExecutedOnWorkerThreadNotGuiThread()
    {
        const QString dbPath = createTestDatabase(QStringLiteral("thread_test.db"));
        AsyncDatabaseWorker worker;
        QVERIFY(worker.startWorker(dbPath));

        QThread* const guiThread = QThread::currentThread();
        QThread* const workerThread = worker.workerThread();
        QVERIFY(workerThread != nullptr);
        QVERIFY(workerThread != guiThread);

        QSignalSpy commitSpy(&worker, &AsyncDatabaseWorker::batchCommitted);

        QVariantMap record;
        record.insert(QStringLiteral("varName"), QStringLiteral("thread_point"));
        record.insert(QStringLiteral("value"), 123.45);
        record.insert(QStringLiteral("unit"), QStringLiteral("V"));
        record.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc());

        const quint64 batchId = worker.enqueueBatch({record});
        QVERIFY(batchId > 0);

        QVERIFY(commitSpy.wait(2000));
        QCOMPARE(commitSpy.count(), 1);
        QCOMPARE(commitSpy.first().at(0).toULongLong(), batchId);

        worker.stopWorker();
    }

    void testUiHeartbeatUnder500msStorageDelay()
    {
        const QString dbPath = createTestDatabase(QStringLiteral("heartbeat_test.db"));
        AsyncDatabaseWorker worker;
        QVERIFY(worker.startWorker(dbPath));

        // 注入 500 ms 存储延迟
        worker.setInjectedStorageDelayMs(500);

        // 在主线程启动 10 ms 心跳计时器，测量最大心跳延迟
        qint64 maxHeartbeatIntervalMs = 0;
        QElapsedTimer heartbeatTimer;
        heartbeatTimer.start();
        qint64 lastHeartbeatTime = 0;

        QTimer timer;
        timer.setInterval(10);
        connect(&timer, &QTimer::timeout, [&]() {
            const qint64 now = heartbeatTimer.elapsed();
            if (lastHeartbeatTime > 0) {
                const qint64 interval = now - lastHeartbeatTime;
                if (interval > maxHeartbeatIntervalMs) {
                    maxHeartbeatIntervalMs = interval;
                }
            }
            lastHeartbeatTime = now;
        });
        timer.start();

        QSignalSpy commitSpy(&worker, &AsyncDatabaseWorker::batchCommitted);

        QVariantMap record;
        record.insert(QStringLiteral("varName"), QStringLiteral("delayed_point"));
        record.insert(QStringLiteral("value"), 999.0);
        record.insert(QStringLiteral("unit"), QStringLiteral("rpm"));
        record.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc());

        worker.enqueueBatch({record});

        // 在主线程等待提交完成（通过事件循环推进，模拟 UI 响应）
        QVERIFY(commitSpy.wait(2000));

        timer.stop();
        worker.stopWorker();

        // 验证：尽管存储写入耗时 500ms，UI 线程的心跳间隔必须 <= 100ms
        qDebug() << "[Heartbeat Benchmark] Max GUI heartbeat interval:" << maxHeartbeatIntervalMs << "ms";
        QVERIFY2(maxHeartbeatIntervalMs <= 100,
                 qPrintable(QStringLiteral("GUI thread blocked! Max interval: %1 ms").arg(maxHeartbeatIntervalMs)));
    }

    void testExplainQueryPlanUsesIndexForCleanup()
    {
        const QString dbPath = createTestDatabase(QStringLiteral("query_plan_test.db"));

        const QString connName = QStringLiteral("PlanTest_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());

            QSqlQuery query(db);
            const QString explainSql = QStringLiteral(
                "EXPLAIN QUERY PLAN "
                "SELECT id FROM runtime_data "
                "WHERE timestamp < '2026-09-08T00:00:00.000Z' "
                "ORDER BY timestamp ASC, id ASC LIMIT 1000");

            QVERIFY(query.exec(explainSql));

            bool usedIndex = false;
            QString planDetails;
            while (query.next()) {
                const QString detail = query.value(QStringLiteral("detail")).toString();
                planDetails += detail + QStringLiteral("; ");
                if (detail.contains(QStringLiteral("USING INDEX"), Qt::CaseInsensitive)
                    || detail.contains(QStringLiteral("COVERING INDEX"), Qt::CaseInsensitive)) {
                    usedIndex = true;
                }
            }

            qDebug() << "[Query Plan Output]:" << planDetails;
            QVERIFY2(usedIndex, qPrintable(QStringLiteral("Cleanup subquery does not use index: %1").arg(planDetails)));

            db.close();
        }
        QSqlDatabase::removeDatabase(connName);
    }

    void testChunkedCleanupBatchesAndCancellation()
    {
        const QString dbPath = createTestDatabase(QStringLiteral("chunk_test.db"));

        // 插入 2500 条过期数据（每条 timestamp 在 2 天前）
        const QDateTime oldTime = QDateTime::currentDateTimeUtc().addDays(-2);
        {
            DataManager& dm = DataManager::instance();
            QVERIFY(dm.initialize(dbPath));

            QList<QVariantMap> batch;
            batch.reserve(2500);
            for (int i = 1; i <= 2500; ++i) {
                QVariantMap r;
                r.insert(QStringLiteral("varName"), QStringLiteral("old_point"));
                r.insert(QStringLiteral("value"), static_cast<double>(i));
                r.insert(QStringLiteral("timestamp"), oldTime.addMSecs(i));
                batch.append(r);
            }
            QVERIFY(dm.logRuntimeDataBatch(batch).success);

            // 验证已落库 2500 条
            RuntimeHistoryCount countBefore = dm.countHistory(QStringLiteral("old_point"), oldTime.addDays(-1), oldTime.addDays(1));
            QCOMPARE(countBefore.count, 2500LL);

            // 分批清理：每批 1000 条，预计 3 批（1000 + 1000 + 500 = 2500）
            const int deleted = dm.cleanupOldData(1, nullptr, 1000);
            QCOMPARE(deleted, 2500);

            // 再次统计：过期数据已全部清理
            RuntimeHistoryCount countAfter = dm.countHistory(QStringLiteral("old_point"), oldTime.addDays(-1), oldTime.addDays(1));
            QCOMPARE(countAfter.count, 0LL);

            // 再次插入 2500 条，测试取消
            QVERIFY(dm.logRuntimeDataBatch(batch).success);

            // 设置取消标志：在循环初次检查后取消
            std::atomic_bool cancelToken{true};
            const int cancelledDeleted = dm.cleanupOldData(1, &cancelToken, 1000);
            // 立即被取消，返回 0
            QCOMPARE(cancelledDeleted, 0);

            dm.shutdown();
        }
    }

    void testBoundedQueueAndDropPolicy()
    {
        const QString dbPath = createTestDatabase(QStringLiteral("drop_test.db"));
        AsyncDatabaseWorker worker;
        worker.setMaxQueueRecords(500);
        QCOMPARE(worker.maxQueueRecords(), 500);

        // 启动 worker 但注入较大延迟以阻塞处理
        worker.setInjectedStorageDelayMs(1000);
        QVERIFY(worker.startWorker(dbPath));

        QSignalSpy droppedSpy(&worker, &AsyncDatabaseWorker::recordsDropped);

        // 提交 800 条记录（超过 500 容量限制）
        QList<QVariantMap> records;
        records.reserve(800);
        for (int i = 0; i < 800; ++i) {
            QVariantMap r;
            r.insert(QStringLiteral("varName"), QStringLiteral("flood"));
            r.insert(QStringLiteral("value"), static_cast<double>(i));
            r.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc());
            records.append(r);
        }

        worker.enqueueBatch(records);

        // 验证丢弃了 300 条
        QCOMPARE(worker.droppedRecordCount(), 300ULL);
        QCOMPARE(droppedSpy.count(), 1);
        QCOMPARE(droppedSpy.first().at(0).toInt(), 300);

        // 队列内记录数不超过 500
        QVERIFY(worker.pendingRecordCount() <= 500);

        worker.setInjectedStorageDelayMs(0);
        worker.stopWorker();
    }

    void testDrainOnShutdown()
    {
        const QString dbPath = createTestDatabase(QStringLiteral("drain_test.db"));
        AsyncDatabaseWorker worker;
        QVERIFY(worker.startWorker(dbPath));

        QList<QVariantMap> records;
        for (int i = 1; i <= 50; ++i) {
            QVariantMap r;
            r.insert(QStringLiteral("varName"), QStringLiteral("drain_point"));
            r.insert(QStringLiteral("value"), static_cast<double>(i));
            r.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc());
            records.append(r);
        }

        worker.enqueueBatch(records);

        // 立即关闭 worker，允许最多 2000 ms drain
        worker.stopWorker(2000);
        QVERIFY(!worker.isRunning());

        // 验证 50 条记录已被完整 drain 落库
        DataManager& dm = DataManager::instance();
        QVERIFY(dm.initialize(dbPath));
        auto result = dm.getLatestRecords(QStringLiteral("drain_point"), 100);
        QCOMPARE(result.size(), 50);
        dm.shutdown();
    }

    void testAsyncHistoryStoreAdapterPagingAndIsolation()
    {
        const QString dbPath = createTestDatabase(QStringLiteral("store_adapter_test.db"));
        AsyncDatabaseWorker worker;
        QVERIFY(worker.startWorker(dbPath));

        // 插入测试历史
        const QDateTime baseTime = QDateTime::fromString(QStringLiteral("2026-09-08T08:00:00.000Z"), Qt::ISODate);
        QList<QVariantMap> records;
        for (int i = 1; i <= 20; ++i) {
            QVariantMap r;
            r.insert(QStringLiteral("varName"), QStringLiteral("adapter_point"));
            r.insert(QStringLiteral("value"), static_cast<double>(i * 5));
            r.insert(QStringLiteral("timestamp"), baseTime.addSecs(i));
            r.insert(QStringLiteral("unit"), QStringLiteral("bar"));
            r.insert(QStringLiteral("quality"), QStringLiteral("Good"));
            r.insert(QStringLiteral("valueValid"), true);
            records.append(r);
        }

        QSignalSpy commitSpy(&worker, &AsyncDatabaseWorker::batchCommitted);
        worker.enqueueBatch(records);
        QVERIFY(commitSpy.wait(2000));

        // 通过 AsyncHistoryStoreAdapter 查询
        auto store = std::make_shared<AsyncHistoryStoreAdapter>(&worker);
        QVERIFY(store->isAvailable());

        MonitorManager mgr;
        mgr.setHistoryStore(store);
        QVERIFY(mgr.isDatabaseHistoryAvailable());

        // 测试分页
        RuntimeHistoryCursor cursor;
        int totalFetched = 0;
        int pages = 0;
        while (true) {
            DatabaseHistoryPage page; bool done = false;
            mgr.requestDatabaseHistoryPage(this, QStringLiteral("adapter_point"), baseTime, baseTime.addSecs(30),
                6, cursor, 0, [&](DatabaseHistoryPage result) { page = result; done = true; });
            QTRY_VERIFY(done);
            QVERIFY(page.succeeded());
            totalFetched += page.samples.size();
            pages++;
            if (!page.hasMore) {
                break;
            }
            cursor = page.nextCursor;
        }

        QCOMPARE(totalFetched, 20);
        QCOMPARE(pages, 4); // 6 + 6 + 6 + 2 = 20 (4 pages)

        // 测试计数
        RuntimeHistoryCount cnt; bool countDone = false;
        worker.submitHistoryTask(this, [&]() {
            auto result = worker.countHistory(QStringLiteral("adapter_point"), baseTime, baseTime.addSecs(30));
            QMetaObject::invokeMethod(this, [&, result]() { cnt = result; countDone = true; }, Qt::QueuedConnection);
        });
        QTRY_VERIFY(countDone);
        QVERIFY(cnt.succeeded());
        QCOMPARE(cnt.count, 20LL);

        worker.stopWorker();
    }
};

QTEST_MAIN(AsyncDatabaseWorkerTest)
#include "async_database_worker_test.moc"
