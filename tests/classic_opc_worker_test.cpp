#include <QtTest>
#include <QElapsedTimer>
#include <QTimer>
#include "communication/ClassicOpcPollWorker.h"
#include "communication/Communication.h"
#define private public
#include "communication/ClassicOpcServer.h"
#undef private

class FixtureModbus : public ModbusInterface {
public:
    bool connected = false, shortReply = false;
    int delayMs = 0;
    std::atomic_int reads{0}, opens{0}, closes{0};
    QMap<int, QVector<quint16>> words;
    QMap<int, QVector<bool>> bits;
    QMap<int, QVector<quint16>> wordCache;
    QMap<int, QVector<bool>> bitCache;
    QString lastArea;
    QDeadlineTimer deadline{QDeadlineTimer::Forever};
    const std::atomic_bool* cancelled = nullptr;
    bool open(const ModbusConfig&) override { ++opens; connected = true; return true; }
    void close() override { ++closes; connected = false; }
    bool isConnected() const override { return connected; }
    void setRequestBudget(int ms, const std::atomic_bool* token) override {
        cancelled = token; deadline = ms < 0 ? QDeadlineTimer(QDeadlineTimer::Forever) : QDeadlineTimer(ms);
    }
    bool read(int address, int count, QString area) {
        ++reads; lastArea = area; QElapsedTimer delay; delay.start();
        while (delay.elapsed() < delayMs) {
            if (deadline.hasExpired() || (cancelled && cancelled->load())) return false;
            QThread::msleep(1);
        }
        auto data = words.value(address); auto dataBits = bits.value(address);
        if (shortReply) { data.resize(qMax(0,count-1)); dataBits.resize(qMax(0,count-1)); }
        wordCache.insert(address, data); bitCache.insert(address, dataBits); return true;
    }
    bool readHoldingRegisters(int a,int n) override { return read(a,n,"holding"); }
    bool readInputRegisters(int a,int n) override { return read(a,n,"input"); }
    bool readCoils(int a,int n) override { return read(a,n,"coil"); }
    bool readDiscreteInputs(int a,int n) override { return read(a,n,"discrete"); }
    QMap<int,QVector<quint16>> holdingRegisters() const override { return wordCache; }
    QMap<int,QVector<quint16>> inputRegisters() const override { return wordCache; }
    QMap<int,QVector<bool>> coils() const override { return bitCache; }
    QMap<int,QVector<bool>> discreteInputs() const override { return bitCache; }
};
class ClassicOpcWorkerTest : public QObject {
    Q_OBJECT
    static ClassicPollPoint point(QString type="UINT16", QString area="holding", int address=0) {
        ClassicPollPoint p; p.point.id=QString::number(address); p.point.dataType=type;
        p.area=area; p.address=address; p.codec.dataType=type;
        p.codec.registerCount=(type=="UINT32" || type=="INT32" || type=="REAL") ? 2 : 1;
        return p;
    }
    static ModbusConfig config() { ModbusConfig c; c.portName="LH-classic-worker-fixture"; return c; }
private slots:
    void typedDecoding_data() {
        QTest::addColumn<QString>("type"); QTest::addColumn<QString>("byteOrder"); QTest::addColumn<QString>("wordOrder");
        QTest::addColumn<QVector<quint16>>("payload"); QTest::addColumn<double>("expected");
        for (const auto& type: {QString("BOOL"),QString("UINT16"),QString("INT16"),QString("UINT32"),QString("INT32"),QString("REAL")}) {
            const quint32 raw = type=="BOOL" ? 1 : type=="UINT16" ? 0x1234 : type=="INT16" ? 0xfff4
                               : type=="UINT32" ? 0x12345678 : type=="INT32" ? 0xfffffff4 : 0x40600000;
            const double expected=type=="BOOL" ? 1 : type=="UINT16" ? 4660 : type=="INT16" || type=="INT32" ? -12
                                  : type=="UINT32" ? 305419896 : 3.5;
            for (const auto& byte: {QString("BigEndian"),QString("LittleEndian")})
                for (const auto& word: {QString("BigEndian"),QString("LittleEndian")}) {
                    auto p=point(type); QVector<quint16> data;
                    if (p.width()==2) data={quint16(raw>>16),quint16(raw)}; else data={quint16(raw)};
                    if (word=="LittleEndian") std::reverse(data.begin(),data.end());
                    if (byte=="LittleEndian") for (auto& value:data) value=quint16((value>>8)|(value<<8));
                    QTest::newRow(qPrintable(type+byte+word))<<type<<byte<<word<<data<<expected;
                }
        }
    }
    void typedDecoding() {
        QFETCH(QString,type); QFETCH(QString,byteOrder); QFETCH(QString,wordOrder);
        QFETCH(QVector<quint16>,payload); QFETCH(double,expected);
        auto transport=std::make_unique<FixtureModbus>(); transport->words[0]=payload;
        ClassicOpcPollWorker worker(std::move(transport)); auto p=point(type); p.codec.byteOrder=byteOrder; p.codec.wordOrder=wordOrder;
        QList<RuntimePointValue> samples; QString error;
        worker.poll(config(),{p},200,125,std::make_shared<std::atomic_bool>(false),[&](auto result,auto issue,bool){samples=result;error=issue;});
        QCOMPARE(samples.size(),1); QVERIFY2(error.isEmpty(),qPrintable(error));
        QCOMPARE(samples[0].quality,RuntimePointQuality::Good); QCOMPARE(samples[0].value.toDouble(),expected);
    }
    void areasArraysBitOffsetAndShortReplies() {
        auto fixture=std::make_unique<FixtureModbus>(); auto* io=fixture.get(); io->words[0]={0x8}; io->bits[0]={true,false,true};
        ClassicOpcPollWorker worker(std::move(fixture)); auto token=std::make_shared<std::atomic_bool>(false);
        for (const auto& area: {"coil","discrete","input","holding"}) {
            auto p=point("BOOL",area); if(p.area=="coil" || p.area=="discrete") p.codec.elementCount=3; else p.bit=3;
            QList<RuntimePointValue> samples;
            worker.poll(config(),{p},200,125,token,[&](auto values,QString,bool){samples=values;});
            QCOMPARE(samples.size(),1); QCOMPARE(samples[0].quality,RuntimePointQuality::Good); QCOMPARE(io->lastArea,QString(area));
            if(p.width()==3) QCOMPARE(samples[0].value.toList(),QVariantList({true,false,true})); else QCOMPARE(samples[0].value.toBool(),true);
            io->shortReply=true;
            worker.poll(config(),{p},200,125,token,[&](auto values,QString,bool){samples=values;});
            QCOMPARE(samples[0].quality,RuntimePointQuality::Bad); QVERIFY(!samples[0].value.isValid()); io->shortReply=false;
        }
        auto invalid=point(); invalid.address=-1; QList<RuntimePointValue> samples;
        worker.poll(config(),{invalid},200,125,token,[&](auto values,QString,bool){samples=values;});
        QCOMPARE(samples.size(),1); QCOMPARE(samples[0].quality,RuntimePointQuality::Bad);
    }
    void totalBudgetAndOwnership() {
        auto fixture=std::make_unique<FixtureModbus>(); auto* io=fixture.get(); io->delayMs=70; io->words[0]={7}; io->words[10]={8};
        ClassicOpcPollWorker worker(std::move(fixture)); QList<RuntimePointValue> samples; QElapsedTimer clock; clock.start();
        auto token=std::make_shared<std::atomic_bool>(false);
        worker.poll(config(),{point(),point("UINT16","holding",10)},100,125,token,[&](auto result,QString,bool){samples=result;});
        QVERIFY(clock.elapsed()<200); QCOMPARE(samples.size(),2); QCOMPARE(samples[1].quality,RuntimePointQuality::Bad);
        worker.closeWhenIdle(token); QVERIFY(Communication::tryClaimRtuPort(config().portName,"other-session"));
        bool connected=true;
        worker.poll(config(),{point()},100,125,token,[&](auto,QString issue,bool c){connected=c; QVERIFY(!issue.isEmpty());});
        QVERIFY(!connected); QCOMPARE(Communication::currentRtuPortOwner(config().portName),QString("other-session"));
        Communication::releaseRtuPort(config().portName,"other-session");
    }
    void scaledTypedArraysUseThePollDecoder() {
        auto fixture=std::make_unique<FixtureModbus>(); fixture->words[0]={0x4060,0,0xc000,0};
        ClassicOpcPollWorker worker(std::move(fixture)); auto p=point("REAL","input");
        p.unit=2; p.codec.elementCount=2; p.codec.registerCount=4; p.codec.scale=2; p.codec.offset=3;
        QList<RuntimePointValue> values; QString error;
        worker.poll(config(),{p},100,125,std::make_shared<std::atomic_bool>(false),[&](auto samples,auto issue,bool){values=samples;error=issue;});
        QVERIFY2(error.isEmpty(),qPrintable(error)); QCOMPARE(values.size(),1);
        QCOMPARE(values[0].quality,RuntimePointQuality::Good);
        QCOMPARE(values[0].value.toList(),QVariantList({10.0,-1.0}));
    }
    void serverHeartbeatStopRestartAndReadonlyHolding() {
        auto fixture=std::make_unique<FixtureModbus>(); auto* io=fixture.get(); io->delayMs=120; io->words[0]={7};
        ClassicOpcServer server(nullptr,std::move(fixture)); OpcServerConfig c;
        c.channelName="test"; c.deviceName=config().portName; c.serialMode="9600,None,8,1"; c.timeoutMs=250; c.publishIntervalMs=5000;
        QString error; QVERIFY(server.applyConfig(c,&error)); auto p=point().point; p.name="ReadOnly"; p.opcTagName="Readonly";
        p.access=RuntimePointAccess::ReadOnly; p.addressing={{"area","holding"},{"address",0},{"unitId",1}};
        server.setRuntimePoints({p}); QCOMPARE(server.m_pointAddressing[p.id].area,QString("holding"));
        QVERIFY(!server.routeWriteRequest(server.m_pointToNodePath[p.id],99,&error)); QVERIFY(error.contains("read-only"));
        int ticks=0; QTimer heartbeat; connect(&heartbeat,&QTimer::timeout,[&]{++ticks;}); heartbeat.start(5);
        QVERIFY(server.start()); QTRY_VERIFY(io->reads.load()>0); server.stop();
        QVERIFY(server.start()); QTRY_COMPARE_WITH_TIMEOUT(server.statusSnapshot().extras["successfulPollCount"].toInt(),1,1000);
        QVERIFY(ticks>=5); QCOMPARE(server.m_values[p.id].value.toInt(),7);
        QTest::qWait(150); QCOMPARE(server.statusSnapshot().extras["successfulPollCount"].toInt(),1);
        QElapsedTimer stop; stop.start(); server.stop(); QVERIFY(stop.elapsed()<100);
    }
};
QTEST_GUILESS_MAIN(ClassicOpcWorkerTest)
#include "classic_opc_worker_test.moc"
