#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSqlError>
#include "core/DataManager.h"
#include "core/AsyncDatabaseWorker.h"
#include "monitor/MonitorManager.h"
#include "monitor/MonitorDataLogger.h"
#include "monitor/MonitorExportHelper.h"
#include "common/PathSecurityUtils.h"
using namespace Monitor;
using Core::AsyncDatabaseWorker;
class ReviewRound2RegressionTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir dir;
    QString database(const QString& name) {
        auto path=dir.filePath(name); auto& dm=DataManager::instance(); dm.shutdown();
        if (!dm.initialize(path)) return {};
        return path;
    }
    RuntimeHistoryPage latest(AsyncDatabaseWorker& worker, int size, const RuntimeHistoryCursor& cursor, QDateTime end) {
        QEventLoop loop; RuntimeHistoryPage out;
        worker.submitHistoryTask(&loop,[&]() { auto page=worker.queryLatestHistoryPage("p",5,size,cursor,end);
            QMetaObject::invokeMethod(&loop,[&,page]() { out=page;loop.quit(); },Qt::QueuedConnection); });
        QTimer::singleShot(2000,&loop,&QEventLoop::quit);loop.exec();return out;
    }
    QVariantMap record(QString name, QDateTime time=QDateTime::currentDateTimeUtc()) {
        return {{"varName",name},{"timestamp",time},{"value",1.0}};
    }
private slots:
    void artifactHashRejectsUnavailableFiles() {
        const QString path = dir.filePath("hash.code");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("abc"), qint64(3));
        file.close();
        QCOMPARE(PathSecurityUtils::sha256ForFile(path),
                 QString("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        QVERIFY(PathSecurityUtils::sha256ForFile(dir.filePath("missing.code")).isEmpty());
        QVERIFY(PathSecurityUtils::sha256ForFile(dir.path()).isEmpty());
    }
    void cleanup() { DataManager::instance().shutdown(); }
    void timestampPreservesInstantAcrossZonesAndDays() {
        QVERIFY(!database("zones.db").isEmpty()); auto& dm=DataManager::instance();
        const auto utc=QDateTime::fromString("2026-09-08T20:00:00.123Z",Qt::ISODateWithMs);
        const QList<QDateTime> times{utc,utc.toLocalTime(),utc.toOffsetFromUtc(8*3600),utc.toOffsetFromUtc(-7*3600)};
        for (int i=0;i<times.size();++i) {
            const auto name=QString::number(i); QVERIFY(dm.logRuntimeDataBatch({record(name,times[i])}).success);
            const auto rows=dm.getLatestRecords(name,1); QCOMPARE(rows.size(),1); QCOMPARE(rows[0].timestamp,utc);
        }
    }
    void ambiguousMigrationRollsBackAndCanonicalReopenIsIdempotent() {
        auto path=database("ambiguous.db");auto& dm=DataManager::instance();
        const auto time=QDateTime::fromString("2026-09-08T20:00:00.123Z",Qt::ISODateWithMs);
        QVERIFY(dm.logRuntimeDataBatch({record("legacy",time)}).success);dm.shutdown();
        QVERIFY(dm.initialize(path));QCOMPARE(dm.getLatestRecords("legacy",1)[0].timestamp,time);dm.shutdown();
        auto db=QSqlDatabase::addDatabase("QSQLITE","round2_ambiguous");db.setDatabaseName(path);QVERIFY(db.open());
        QSqlQuery q(db);QVERIFY(q.exec("UPDATE schema_version SET version=4"));
        QVERIFY(q.exec("UPDATE runtime_data SET timestamp='2026-09-08T12:00:00.000'"));
        QVERIFY(!dm.initialize(path));
        QVERIFY(q.exec("SELECT version FROM schema_version"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),4);
        QVERIFY(q.exec("SELECT timestamp FROM runtime_data"));QVERIFY(q.next());QCOMPARE(q.value(0).toString(),QString("2026-09-08T12:00:00.000"));
        q=QSqlQuery();db.close();db=QSqlDatabase();QSqlDatabase::removeDatabase("round2_ambiguous");
    }
    void latestSnapshotSurvivesConcurrentAppends() {
        auto path=database("snapshot.db"); QVERIFY(!path.isEmpty()); auto& dm=DataManager::instance();
        auto base=QDateTime::currentDateTimeUtc().addSecs(-100);
        for(int i=0;i<10;++i) QVERIFY(dm.logRuntimeDataBatch({record("p",base.addSecs(i))}).success);
        RuntimeHistoryCursor cursor; cursor.maxId=dm.latestRecordId();
        for(int i=10;i<15;++i) QVERIFY(dm.logRuntimeDataBatch({record("p",base.addSecs(i))}).success);
        AsyncDatabaseWorker worker; QVERIFY(worker.startWorker(path));
        auto page=latest(worker,2,cursor,base.addSecs(100));
        QCOMPARE(page.records.size(),2); QCOMPARE(page.records[0].id,6LL); cursor=page.nextCursor;
        QVERIFY(dm.logRuntimeDataBatch({record("p",base.addSecs(20))}).success);
        page=latest(worker,4,cursor,base.addSecs(100));
        QCOMPARE(page.records.size(),3); QCOMPARE(page.records.last().id,10LL); QVERIFY(!page.hasMore);
    }
    void retryRecoversWithoutNewEnqueueAndPermanentFailureTerminates() {
        auto path=database("retry.db"); DataManager::instance().shutdown();
        auto db=QSqlDatabase::addDatabase("QSQLITE","round2_retry");db.setDatabaseName(path);QVERIFY(db.open());
        QSqlQuery q(db);QVERIFY(q.exec("CREATE TRIGGER fail_insert BEFORE INSERT ON runtime_data BEGIN SELECT RAISE(ABORT,'injected'); END"));
        AsyncDatabaseWorker worker;QVERIFY(worker.startWorker(path)); QSignalSpy committed(&worker,&AsyncDatabaseWorker::batchCommitted);
        QSignalSpy failed(&worker,&AsyncDatabaseWorker::batchFailed);QSignalSpy abandoned(&worker,&AsyncDatabaseWorker::batchAbandoned);
        worker.enqueueBatch({record("retry")});QTRY_VERIFY(failed.count()>0);
        QVERIFY(q.exec("DROP TRIGGER fail_insert"));QTRY_COMPARE(committed.count(),1);QCOMPARE(worker.pendingRecordCount(),0);
        QVERIFY(q.exec("CREATE TRIGGER fail_insert BEFORE INSERT ON runtime_data BEGIN SELECT RAISE(ABORT,'injected'); END"));
        worker.enqueueBatch({record("permanent")});QTRY_COMPARE(abandoned.count(),1);QCOMPARE(worker.pendingRecordCount(),0);
        QCOMPARE(worker.droppedRecordCount(),1ULL); worker.stopWorker(); q=QSqlQuery();db.close();db=QSqlDatabase();QSqlDatabase::removeDatabase("round2_retry");
    }
    void databaseLockRecoveryAndReadOnlyFailureAreReported() {
        auto path=database("lock.db");DataManager::instance().shutdown();
        auto db=QSqlDatabase::addDatabase("QSQLITE","round2_lock");db.setDatabaseName(path);QVERIFY(db.open());
        AsyncDatabaseWorker worker;QVERIFY(worker.startWorker(path));
        QSqlQuery q(db);QVERIFY(q.exec("BEGIN IMMEDIATE"));
        QSignalSpy failed(&worker,&AsyncDatabaseWorker::batchFailed);QSignalSpy committed(&worker,&AsyncDatabaseWorker::batchCommitted);
        worker.enqueueBatch({record("locked")});QTRY_VERIFY(failed.count()>0);QVERIFY(q.exec("ROLLBACK"));
        QTRY_COMPARE(committed.count(),1);
        bool readonly=false;
        worker.submitHistoryTask(this,[&]() {
            // The independent SQL connection rejects writes exactly as a read-only database does.
            for(const auto& name:QSqlDatabase::connectionNames()) if(name.startsWith("AsyncWorker_")) {
                QSqlQuery setting(QSqlDatabase::database(name));setting.exec("PRAGMA query_only=ON");
            }
            QMetaObject::invokeMethod(this,[&](){readonly=true;},Qt::QueuedConnection);
        });
        QTRY_VERIFY(readonly);QSignalSpy abandoned(&worker,&AsyncDatabaseWorker::batchAbandoned);
        worker.enqueueBatch({record("readonly")});QTRY_COMPARE(abandoned.count(),1);QCOMPARE(worker.droppedRecordCount(),1ULL);
        QVERIFY(abandoned[0][2].toString().contains("readonly",Qt::CaseInsensitive));worker.stopWorker();
        q=QSqlQuery();db.close();db=QSqlDatabase();QSqlDatabase::removeDatabase("round2_lock");
    }
    void loggerOnlyAcknowledgesCommittedBatch() {
        auto path=database("logger.db");AsyncDatabaseWorker worker;QVERIFY(worker.startWorker(path));worker.setInjectedStorageDelayMs(500);
        MonitorDataLogger logger;logger.setAsyncWorker(&worker);QSignalSpy committed(&logger,&MonitorDataLogger::samplesCommitted);
        logger.enqueueSample(Sample("p",1.0,"V",QDateTime::currentDateTimeUtc()));logger.flush();
        QTest::qWait(50);QCOMPARE(committed.count(),0);QTRY_COMPARE(committed.count(),1);QCOMPARE(committed[0][0].toInt(),1);
        logger.shutdown();worker.stopWorker();
    }
    void productionManagerHistoryRequestKeepsUiResponsiveAndCanCancel() {
        auto path=database("manager.db");MonitorManager manager;QVERIFY(manager.startDatabaseService(path));
        auto* worker=manager.databaseWorker();worker->setInjectedStorageDelayMs(500);
        manager.recordSample("p",1.0);manager.flushDatabaseLogging();
        int ticks=0;qint64 maxGap=0,last=0;QElapsedTimer beatClock;beatClock.start();
        QTimer heartbeat;connect(&heartbeat,&QTimer::timeout,[&](){auto now=beatClock.elapsed();maxGap=qMax(maxGap,now-last);last=now;++ticks;});heartbeat.start(10);
        bool done=false;DatabaseHistoryPage result;QElapsedTimer timer;timer.start();
        manager.requestDatabaseHistoryPage(this,"p",QDateTime::currentDateTimeUtc().addDays(-1),QDateTime::currentDateTimeUtc().addDays(1),10,{},0,
            [&](DatabaseHistoryPage page){result=page;done=true;});
        QVERIFY(timer.elapsed()<100);QTRY_VERIFY(done);QVERIFY(result.succeeded());QCOMPARE(result.samples.size(),1);QVERIFY(ticks>=10);QVERIFY2(maxGap<=100,qPrintable(QString::number(maxGap)));
        qInfo()<<"Production history 500ms delay heartbeat max ms"<<maxGap<<"ticks"<<ticks;
        bool cancelledDone=false;RuntimeHistoryPageStatus status=RuntimeHistoryPageStatus::Success;
        worker->enqueueBatch({record("p")});
        manager.requestDatabaseHistoryPage(this,"p",{},{},10,{},0,[&](DatabaseHistoryPage page){status=page.status;cancelledDone=true;});
        manager.historyStore()->cancelPendingRequests();QTRY_VERIFY(cancelledDone);QCOMPARE(status,RuntimeHistoryPageStatus::Cancelled);
        manager.shutdown();
    }
    void asynchronousExportUsesCommittedSnapshotAndPreservesFileOnCancellation() {
        auto path=database("export.db");MonitorManager manager;QVERIFY(manager.startDatabaseService(path));
        auto* worker=manager.databaseWorker();worker->setInjectedStorageDelayMs(500);
        QList<Sample> samples;const auto base=QDateTime::currentDateTimeUtc();
        for(int i=0;i<600;++i)samples.append(Sample("export.point",i,"V",base.addMSecs(i)));
        manager.recordSamples("export.point",samples);manager.flushDatabaseLogging();
        const auto store=manager.historyStore();const auto file=dir.filePath("history.csv");
        ExportResult result,cancelledResult;bool done=false;int ticks=0;QTimer timer;
        connect(&timer,&QTimer::timeout,[&](){++ticks;});timer.start(10);
        QVERIFY(worker->submitHistoryTask(this,[&]() {
            MonitorHistoryService history(store);MonitorExportHelper helper;
            const auto maxId=worker->latestRecordId();
            const auto count=store->countHistory("export.point",base.addSecs(-1),base.addSecs(2),maxId);
            ExportChannelInfo info;info.channelId="export.point";info.displayName="point";info.unit="V";info.sampleCount=count.count;
            ExportMetadata metadata;metadata.exportTime=base.addSecs(2);metadata.timeWindowMs=3000;metadata.totalChannels=1;metadata.totalSamples=count.count;
            ExportPageProvider provider=[&](const QString& channel,const ExportCursor& cursor,int size) {
                RuntimeHistoryCursor c;c.id=cursor.id;c.timestamp=cursor.timestamp;c.maxId=maxId;
                auto page=history.page(channel,base.addSecs(-1),base.addSecs(2),size,c);
                ExportPage out;out.success=page.succeeded();out.samples=page.samples;out.hasMore=page.hasMore;
                out.nextCursor.id=page.nextCursor.id;out.nextCursor.timestamp=page.nextCursor.timestamp;out.errorMessage=page.errorText;return out;
            };
            auto exported=helper.exportPackagePaged({info},metadata,provider,file,100);
            ExportPageProvider cancelProvider=[](const QString&,const ExportCursor&,int) { ExportPage out;out.success=false;out.errorMessage="Export cancelled";return out; };
            auto cancelled=helper.exportPackagePaged({info},metadata,cancelProvider,file,100);
            QMetaObject::invokeMethod(this,[&,exported,cancelled](){result=exported;cancelledResult=cancelled;done=true;},Qt::QueuedConnection);
        }));
        QTRY_VERIFY(done);QVERIFY2(result.success,qPrintable(result.errorMessage));QCOMPARE(result.exportedCount,600);
        QVERIFY(!cancelledResult.success);QVERIFY(ticks>=10);QFile output(file);QVERIFY(output.open(QIODevice::ReadOnly));
        QVERIFY(output.size()>1000);QVERIFY(output.readAll().contains("export.point"));manager.shutdown();
    }
    void millionRowCleanupIsIndexedChunkedAndResponsive() {
        const auto path=database("million.db");DataManager::instance().shutdown();
        auto db=QSqlDatabase::addDatabase("QSQLITE","round2_million");db.setDatabaseName(path);QVERIFY(db.open());
        QSqlQuery q(db);QElapsedTimer setup;setup.start();
        QVERIFY2(q.exec("WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x<1000000) "
            "INSERT INTO runtime_data(timestamp,variable_name,value) SELECT CASE WHEN x<=5000 THEN '2000-01-01T00:00:00.000Z' ELSE '2099-01-01T00:00:00.000Z' END,'million',x FROM n"),qPrintable(q.lastError().text()));
        qInfo()<<"Million-row fixture setup ms"<<setup.elapsed();
        QVERIFY(q.exec("EXPLAIN QUERY PLAN SELECT id FROM runtime_data WHERE timestamp<'2026-01-01T00:00:00.000Z' ORDER BY timestamp,id LIMIT 1000"));
        QString plan;while(q.next())plan+=q.value(3).toString();QVERIFY(plan.contains("idx_runtime_timestamp"));
        MonitorManager manager;QVERIFY(manager.startDatabaseService(path));auto* worker=manager.databaseWorker();
        int ticks=0;qint64 maxGap=0,last=0;QElapsedTimer clock;clock.start();QTimer timer;
        connect(&timer,&QTimer::timeout,[&](){auto now=clock.elapsed();maxGap=qMax(maxGap,now-last);last=now;++ticks;});timer.start(10);
        QSignalSpy batches(worker,&AsyncDatabaseWorker::cleanupBatchCompleted);QSignalSpy complete(worker,&AsyncDatabaseWorker::cleanupCompleted);
        worker->requestCleanup(1,1000);QTRY_COMPARE_WITH_TIMEOUT(complete.count(),1,10000);
        QCOMPARE(complete[0][0].toInt(),5000);QCOMPARE(batches.count(),5);
        for(const auto& batch:batches)QVERIFY(batch[0].toInt()<=1000);
        QTest::qWait(30);QVERIFY(ticks>0);QVERIFY2(maxGap<=100,qPrintable(QString::number(maxGap)));
        qInfo()<<"Million-row cleanup ms"<<clock.elapsed()<<"batches"<<batches.count()<<"heartbeat max ms"<<maxGap<<"plan"<<plan;
        QVERIFY(q.exec("SELECT COUNT(*) FROM runtime_data"));QVERIFY(q.next());QCOMPARE(q.value(0).toLongLong(),995000LL);
        manager.shutdown();q=QSqlQuery();db.close();db=QSqlDatabase();QSqlDatabase::removeDatabase("round2_million");
    }
    void shutdownDeadlineIncludesInflightDelay() {
        auto path=database("shutdown.db");AsyncDatabaseWorker worker;QVERIFY(worker.startWorker(path));worker.setInjectedStorageDelayMs(5000);
        QSignalSpy abandoned(&worker,&AsyncDatabaseWorker::batchAbandoned);worker.enqueueBatch({record("tail")});QTest::qWait(20);
        QElapsedTimer timer;timer.start();worker.stopWorker(50);QVERIFY(timer.elapsed()<250);
        QCOMPARE(worker.droppedRecordCount(),1ULL);QTRY_COMPARE(abandoned.count(),1);
    }
};
QTEST_GUILESS_MAIN(ReviewRound2RegressionTest)
#include "review_round2_regression_test.moc"
