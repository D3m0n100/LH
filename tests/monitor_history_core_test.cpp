#include <QtTest>
#include "monitor/MonitorHistoryService.h"
using namespace Monitor;
class MemoryStore : public IMonitorHistoryStore {
public:
    QList<RuntimeRecord> rows;
    bool cancelled = false;
    bool fail = false;
    bool isAvailable() const override { return true; }
    QList<RuntimeRecord> getLatestRecords(const QString&, int n) override { return rows.mid(0, n); }
    QList<RuntimeRecord> queryHistory(const QString&, const QDateTime&, const QDateTime&) override { return rows; }
    RuntimeHistoryPage queryHistoryPage(const QString&, const QDateTime&, const QDateTime&, int n, const RuntimeHistoryCursor& cursor) override {
        RuntimeHistoryPage p;
        p.status = cancelled ? RuntimeHistoryPageStatus::Cancelled : fail ? RuntimeHistoryPageStatus::SqlError : RuntimeHistoryPageStatus::Success;
        if (fail) { p.errorCode = "STORAGE_ERROR"; p.errorText = "injected failure"; }
        if (!p.succeeded()) return p;
        for (const auto& r : rows) if (r.id > cursor.id && (cursor.maxId < 0 || r.id <= cursor.maxId)) p.records.append(r);
        p.hasMore = p.records.size() > n; p.records = p.records.mid(0, n);
        p.nextCursor = cursor;
        if (!p.records.isEmpty()) { p.nextCursor.id = p.records.last().id; p.nextCursor.timestamp = p.records.last().timestamp; }
        return p;
    }
    RuntimeHistoryPage queryLatestHistoryPage(const QString& c, int, int n, const RuntimeHistoryCursor& cursor, const QDateTime& end) override { return queryHistoryPage(c, {}, end, n, cursor); }
    RuntimeHistoryCount countHistory(const QString&, const QDateTime&, const QDateTime&, qint64) override { RuntimeHistoryCount c; c.status = RuntimeHistoryPageStatus::Success; c.count = rows.size(); return c; }
    RuntimeHistoryCount countLatestHistory(const QString& c, int, const QDateTime& e, qint64 id) override { return countHistory(c, {}, e, id); }
    void cancelPendingRequests() override { cancelled = true; }
};
class MonitorHistoryCoreTest : public QObject {
    Q_OBJECT
private slots:
    void pagingMetadataErrorsAndInstanceIsolation() {
        auto a = std::make_shared<MemoryStore>(); auto b = std::make_shared<MemoryStore>();
        const auto time = QDateTime::fromString("2026-09-08T00:00:00.000Z", Qt::ISODateWithMs);
        for (int i = 1; i <= 3; ++i) { RuntimeRecord r; r.id=i; r.timestamp=time; r.value=i; r.valueValid=false;
            r.quality=RuntimePointQuality::Bad; r.origin="push"; r.errorCode="READ_FAILED"; r.errorText="transport failed"; a->rows.append(r); }
        MonitorHistoryService sa(a), sb(b);
        RuntimeHistoryCursor cursor; cursor.maxId=3;
        auto first=sa.page("point", {}, {}, 2, cursor);
        QVERIFY(first.succeeded()); QVERIFY(first.hasMore); QCOMPARE(first.samples.size(),2);
        QCOMPARE(first.samples[0].metadata.value("id").toLongLong(),1LL);
        QCOMPARE(first.samples[0].metadata.value("origin").toString(),QString("push"));
        QCOMPARE(first.samples[0].metadata.value("errorCode").toString(),QString("READ_FAILED"));
        QVERIFY(!first.samples[0].valueValid); QCOMPARE(first.samples[0].timestamp,time);
        auto second=sa.page("point", {}, {}, 2, first.nextCursor);
        QCOMPARE(second.samples.size(),1); QVERIFY(!second.hasMore); QCOMPARE(second.nextCursor.maxId,3LL);
        QVERIFY(sb.page("point", {}, {}, 2).samples.isEmpty());
        a->fail=true; auto failed=sa.page("point", {}, {}, 2); QCOMPARE(failed.errorCode,QString("STORAGE_ERROR"));
        QVERIFY(!failed.succeeded()); sa.cancel(); QCOMPARE(sa.page("point", {}, {}, 2).status,RuntimeHistoryPageStatus::Cancelled);
        QVERIFY(sb.page("point", {}, {}, 2).succeeded());
    }
};
QTEST_GUILESS_MAIN(MonitorHistoryCoreTest)
#include "monitor_history_core_test.moc"
