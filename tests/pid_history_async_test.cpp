#include <QtTest>
#include <QWidget>
#include <QLabel>
#include <QtCharts/QLineSeries>
#include <QTemporaryDir>
#include "common/ConfigTypes.h"
#include "monitor/MonitorManager.h"
#include "core/AsyncDatabaseWorker.h"
#define private public
#include "designer/ParameterTuningWindow.h"
#undef private
class PidHistoryAsyncTest : public QObject {
    Q_OBJECT
private slots:
    void delayedDatabaseHistoryAndSelectionChanges() {
        QTemporaryDir dir;auto& dm=DataManager::instance();QVERIFY(dm.initialize(dir.filePath("pid.db")));
        auto& manager=Monitor::MonitorManager::instance();QVERIFY(manager.startDatabaseService(dir.filePath("pid.db")));
        auto* worker=manager.databaseWorker();worker->setInjectedStorageDelayMs(500);
        worker->enqueueBatch({{{"varName","param::Kp"},{"value",3.0},{"timestamp",QDateTime::currentDateTimeUtc()}}});
        ParameterDefinition kp;kp.name="Kp";kp.onlineEditable=true;kp.dataType="REAL";kp.defaultValue="1";kp.currentValue="3";
        ParameterTuningWindow window;int ticks=0;QTimer heartbeat;connect(&heartbeat,&QTimer::timeout,[&](){++ticks;});heartbeat.start(10);
        QElapsedTimer timer;timer.start();window.setPidParameterDetails({kp});QVERIFY(timer.elapsed()<100);
        QTRY_COMPARE(window.m_pidSeries->count(),1);QCOMPARE(window.m_pidSeries->at(0).y(),3.0);QVERIFY(ticks>=1);
        worker->enqueueBatch({{{"varName","param::Kp"},{"value",9.0},{"timestamp",QDateTime::currentDateTimeUtc()}}});
        window.setPidParameterDetails({kp});
        ParameterDefinition ki=kp;ki.name="Ki";window.setPidParameterDetails({ki});
        QTRY_VERIFY(window.m_pidSummaryLabel->text().contains("Ki"));
        QCOMPARE(window.m_pidSeries->count(),0);QTest::qWait(50);QCOMPARE(window.m_pidSeries->count(),0);
        manager.shutdown();dm.shutdown();
    }
};
QTEST_MAIN(PidHistoryAsyncTest)
#include "pid_history_async_test.moc"
