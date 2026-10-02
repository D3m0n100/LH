#include <QtTest>
#include "monitor/BackendSampler.h"
#include "communication/VirtualDeviceBackend.h"

class BackendSamplerTest : public QObject {
    Q_OBJECT
private slots:
    void sinkIsIndependentOfSqlAndRebindingDropsOldSignals() {
        QList<Monitor::Sample> samples;
        Monitor::BackendSampler sampler([&](const Monitor::Sample& sample) { samples.append(sample); });
        VirtualDeviceBackend first, second;
        RuntimePointDefinition point; point.id = "point"; first.loadPointDefinitions({point}); second.loadPointDefinitions({point});
        QVERIFY(first.connectBackend()); QVERIFY(second.connectBackend());
        sampler.setDeviceBackend(&first); sampler.configure({"point"}, {{"point", "channel"}}, {{"point", 1000}}, 1000); sampler.start();
        sampler.poll(); QTRY_VERIFY(!samples.isEmpty()); QCOMPARE(samples.last().channelName, QString("channel"));
        sampler.setDeviceBackend(&second); sampler.configure({"point"}, {{"point", "new-channel"}}, {{"point", 1000}}, 1000); sampler.start();
        const auto count = samples.size(); emit first.pointsChanged({{"point", 77}}); QCOMPARE(samples.size(), count);
        emit second.pointsChanged({{"point", 88}}); QCOMPARE(samples.last().value, 88.0); QCOMPARE(samples.last().channelName, QString("new-channel"));
        sampler.stop(); const auto stopped = samples.size(); emit second.pointsChanged({{"point", 99}}); sampler.poll(); QTest::qWait(10); QCOMPARE(samples.size(), stopped);
    }
    void backendDestructionInvalidatesPendingCompletion() {
        QList<Monitor::Sample> samples; Monitor::BackendSampler sampler([&](const Monitor::Sample& sample) { samples.append(sample); });
        auto* backend = new VirtualDeviceBackend; RuntimePointDefinition point; point.id = "point"; backend->loadPointDefinitions({point}); QVERIFY(backend->connectBackend());
        sampler.setDeviceBackend(backend); sampler.configure({"point"}, {{"point", "channel"}}, {{"point", 1000}}, 1000); sampler.start(); sampler.poll();
        delete backend; QTest::qWait(10); QVERIFY(!sampler.backend()); QVERIFY(samples.isEmpty());
    }
};
QTEST_GUILESS_MAIN(BackendSamplerTest)
#include "backend_sampler_test.moc"
