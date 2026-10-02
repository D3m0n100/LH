#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <stdexcept>
#include "core/DataManager.h"

namespace {
QString seedV4(const QString& path, int count, bool ambiguous = false) {
    const auto name = QUuid::createUuid().toString(); QString error;
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", name); db.setDatabaseName(path);
        if (!db.open()) error = db.lastError().text();
        QSqlQuery query(db);
        const QStringList statements{
            "CREATE TABLE schema_version(id INTEGER PRIMARY KEY, version INTEGER, updated_at TEXT)",
            "INSERT INTO schema_version VALUES(1,4,'2026-10-01')",
            "CREATE TABLE runtime_data(id INTEGER PRIMARY KEY,timestamp TEXT,variable_name TEXT,value REAL,unit TEXT,quality TEXT,value_valid INTEGER,origin TEXT,error_code TEXT,error_text TEXT)",
            "CREATE TABLE system_logs(id INTEGER PRIMARY KEY,timestamp TEXT,level TEXT,module TEXT,message TEXT)",
            "CREATE TABLE data_summary(id INTEGER PRIMARY KEY,variable_name TEXT,summary_date TEXT,min_value REAL,max_value REAL,avg_value REAL,sum_value REAL,count INTEGER,UNIQUE(variable_name,summary_date))",
            "CREATE INDEX idx_runtime_timestamp ON runtime_data(timestamp)",
            "CREATE INDEX idx_runtime_variable ON runtime_data(variable_name)",
            "CREATE INDEX idx_runtime_var_time ON runtime_data(variable_name,timestamp)",
            "CREATE INDEX idx_logs_timestamp ON system_logs(timestamp)",
            "CREATE INDEX idx_logs_level ON system_logs(level)",
            "CREATE INDEX idx_logs_level_time ON system_logs(level,timestamp)",
            QString("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM n WHERE i<%1) INSERT INTO runtime_data SELECT i,'2026-10-01T12:00:00+08:00','point',i,'bar','Good',1,'fixture','','' FROM n").arg(count),
            QString("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM n WHERE i<%1) INSERT INTO system_logs SELECT i,'2026-10-01T12:00:00+08:00','INFO','fixture','message' FROM n").arg(qMax(1, count / 10))
        };
        if (error.isEmpty()) for (const auto& sql : statements) if (!query.exec(sql)) { error = query.lastError().text(); break; }
        if (ambiguous && error.isEmpty() && !query.exec("UPDATE runtime_data SET timestamp='2026-10-01T12:00:00' WHERE id=2")) error = query.lastError().text();
    }
    QSqlDatabase::removeDatabase(name); return error;
}
QVariantMap inspect(const QString& path) {
    const auto name = QUuid::createUuid().toString(); QVariantMap state;
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", name); db.setDatabaseName(path);
        if (db.open()) {
            QSqlQuery query(db);
            if (query.exec("SELECT version FROM schema_version") && query.next()) state["version"] = query.value(0);
            if (query.exec("SELECT COUNT(*),MIN(timestamp),MAX(timestamp) FROM runtime_data") && query.next()) {
                state["count"] = query.value(0); state["first"] = query.value(1); state["last"] = query.value(2);
            }
            if (query.exec("SELECT timestamp FROM runtime_data WHERE id=1") && query.next()) state["row1"] = query.value(0);
            if (query.exec("SELECT COUNT(*),MIN(timestamp),MAX(timestamp) FROM system_logs") && query.next()) {
                state["logCount"] = query.value(0); state["logFirst"] = query.value(1); state["logLast"] = query.value(2);
            }
        }
    }
    QSqlDatabase::removeDatabase(name); return state;
}
}
class DatabaseStartupMigrationTest : public QObject {
    Q_OBJECT
private slots:
    void largeUpgradeRunsOnWorkerAndReportsProgress() {
        QTemporaryDir dir; const auto path = dir.filePath("v4.db");
        const auto seedError = seedV4(path, 100000); QVERIFY2(seedError.isEmpty(), qPrintable(seedError));
        bool ready = false; QString error; std::atomic_bool cancelled{false}; std::atomic<int> progress{0};
        int heartbeat = 0; QTimer timer; connect(&timer, &QTimer::timeout, [&] { ++heartbeat; }); timer.start(1);
        QEventLoop loop; QElapsedTimer elapsed; elapsed.start();
        auto* thread = QThread::create([&] { ready = DataManager::prepareDatabase(path, {}, &cancelled,
            [&](QString, qint64) { ++progress; }, &error); });
        connect(thread, &QThread::finished, &loop, &QEventLoop::quit); thread->start(); loop.exec(); thread->wait(); delete thread;
        qInfo() << "migration_rows=100000 elapsed_ms=" << elapsed.elapsed() << "gui_heartbeats=" << heartbeat;
        QVERIFY2(ready, qPrintable(error)); QVERIFY(heartbeat > 2); QVERIFY(progress >= 10);
        const auto state = inspect(path); QCOMPARE(state.value("version").toInt(), 5);
        QCOMPARE(state.value("count").toInt(), 100000);
        QCOMPARE(state.value("first").toString(), QString("2026-10-01T04:00:00.000Z"));
        QCOMPARE(state.value("last").toString(), QString("2026-10-01T04:00:00.000Z"));
        QCOMPARE(state.value("logCount").toInt(), 10000);
        QCOMPARE(state.value("logFirst").toString(), QString("2026-10-01T04:00:00.000Z"));
        QCOMPARE(state.value("logLast").toString(), QString("2026-10-01T04:00:00.000Z"));
    }
    void cancellationRollsBackTheWholeUpgrade() {
        QTemporaryDir dir; const auto path = dir.filePath("cancel.db"); QVERIFY(seedV4(path, 20000).isEmpty());
        std::atomic_bool cancelled{false}; QString error;
        QVERIFY(!DataManager::prepareDatabase(path, {}, &cancelled, [&](QString, qint64 count) { if (count >= 10000) cancelled = true; }, &error));
        QVERIFY(error.contains(QStringLiteral("取消")));
        const auto state = inspect(path); QCOMPARE(state.value("version").toInt(), 4);
        QCOMPARE(state.value("count").toInt(), 20000);
        QCOMPARE(state.value("row1").toString(), QString("2026-10-01T12:00:00+08:00"));
    }
    void ambiguousTimestampFailsWithoutPartialCommit() {
        QTemporaryDir dir; const auto path = dir.filePath("bad.db"); QVERIFY(seedV4(path, 2, true).isEmpty());
        QString error; QVERIFY(!DataManager::prepareDatabase(path, {}, nullptr, {}, &error));
        QVERIFY(error.contains("timezone"));
        const auto state = inspect(path); QCOMPARE(state.value("version").toInt(), 4);
        QCOMPARE(state.value("row1").toString(), QString("2026-10-01T12:00:00+08:00"));
    }
    void progressCallbackExceptionRollsBackAndClosesConnection() {
        QTemporaryDir dir; const auto path = dir.filePath("callback-error.db"); QVERIFY(seedV4(path, 20000).isEmpty());
        QString error;
        QVERIFY(!DataManager::prepareDatabase(path, {}, nullptr, [](QString, qint64 count) {
            if (count >= 10000) throw std::runtime_error("progress sink failed");
        }, &error));
        QVERIFY(!error.isEmpty()); const auto state = inspect(path);
        QCOMPARE(state.value("version").toInt(), 4);
        QCOMPARE(state.value("row1").toString(), QString("2026-10-01T12:00:00+08:00"));
    }
    void newAndSmallDatabasesWorkAndPreCancelledCreatesNothing() {
        QTemporaryDir dir; QString error;
        const auto fresh = dir.filePath("new.db"); QVERIFY(DataManager::prepareDatabase(fresh, {}, nullptr, {}, &error));
        QCOMPARE(inspect(fresh).value("version").toInt(), 5);
        const auto small = dir.filePath("small.db"); QVERIFY(seedV4(small, 2).isEmpty());
        QVERIFY(DataManager::prepareDatabase(small, {}, nullptr, {}, &error)); QCOMPARE(inspect(small).value("version").toInt(), 5);
        std::atomic_bool cancelled{true}; const auto untouched = dir.filePath("untouched.db");
        QVERIFY(!DataManager::prepareDatabase(untouched, {}, &cancelled, {}, &error)); QVERIFY(!QFileInfo::exists(untouched));
    }
};
QTEST_GUILESS_MAIN(DatabaseStartupMigrationTest)
#include "database_startup_migration_test.moc"
