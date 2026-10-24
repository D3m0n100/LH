#include <QtTest>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QFile>
#include <QProcess>
#include <QXmlStreamReader>
#include "designer/BuildController.h"
#include <QSemaphore>
#include <QScopeGuard>
#include "designer/ProjectController.h"
#include "designer/ParameterController.h"
#include "designer/RuntimeSessionController.h"
#include "communication/ControllerDeviceBackend.h"
#include "communication/ControllerDebugProtocol.h"
#include "communication/VirtualDeviceBackend.h"

class CountingBackend : public VirtualDeviceBackend {
public:
    int writes = 0;
    bool writePoints(const QHash<QString,QVariant>&, QString*, QHash<QString,CommError>*) override {
        ++writes; return true;
    }
};

class ParityDebugTransport : public IControllerDebugTransport {
public:
    int writes = 0;
    bool connected = true;
    bool isConnected() const override { return connected; }
    int stationAddress() const override { return 1; }
    void setStationAddress(int) override {}
    bool readHoldingRegisters(int, int) override { return connected; }
    QVector<quint16> holdingRegisterValues(int address) const override { return {quint16(address == 38 ? 105 : 1)}; }
    bool writeMultipleRegisters(int, const QVector<quint16>&) override { ++writes; return connected; }
    CommError lastError() const override { return {}; }
};

class LmParitySoftwareTest : public QObject {
    Q_OBJECT
    static ParameterDefinition definition(QString name, QString id) {
        ParameterDefinition d; d.name=name; d.id=id; d.dataType="INT";
        d.onlineEditable=true; d.defaultValue="1"; d.currentValue="1"; d.minValue="0"; d.maxValue="10";
        return d;
    }
private slots:
    void parameterValidationAndAtomicBatch() {
        auto a=definition("A","a"), b=definition("B","b");
        ParameterController controller; controller.loadDefinitions({a,b});
        QVERIFY(controller.editParameter("A","5"));
        for (const auto& value : {"11","-1","1.5","nan","32768"}) {
            QVERIFY(!controller.editParameter("A", value));
            QCOMPARE(controller.parameterState("A").editedValue, QString("5"));
            QVERIFY(!controller.parameterState("A").lastError.isEmpty());
        }
        QVERIFY(controller.editParameter("B","8"));
        b.maxValue="3"; controller.loadDefinitions({a,b});
        CountingBackend backend;
        QString error;
        QVERIFY(!controller.applyModifiedParametersWithReadback(&backend,0,0,&error));
        QCOMPARE(backend.writes,0);
        QVERIFY(!controller.applyModifiedParametersWithReadbackAsync(&backend,0,0,&error));
        QCOMPARE(backend.writes,0);
        QVERIFY(!error.isEmpty());
        QCOMPARE(controller.parameterState("A").state,ParameterState::Modified);
        a.onlineEditable=false; QVERIFY(!ParameterController::validateValue(a,"2",&error));
        a.onlineEditable=true; a.minValue="20"; QVERIFY(!ParameterController::validateValue(a,"2",&error));
        a=definition("A","a"); a.dataType="REAL";
        QVERIFY(!ParameterController::validateValue(a,"nan",&error));
        QVERIFY(!ParameterController::validateValue(a,"inf",&error));
        QVERIFY(ParameterController::validateValue(a,"2.5",&error));
    }
    void snapshotPreviewAndRollback() {
        ProjectController project; project.setCurrentProjectPath("local-test-project");
        project.runtimeConfig().parameters={definition("A","a"),definition("B","b")};
        auto snapshot=project.parameterSnapshot();
        QJsonArray values=snapshot["parameters"].toArray();
        auto a=values[0].toObject(); a["value"]="5"; values[0]=a;
        auto b=values[1].toObject(); b["value"]="99"; values[1]=b;
        snapshot["parameters"]=values;
        QString error; QStringList changes;
        QVERIFY(!project.previewParameterSnapshot(snapshot,&changes,&error));
        QVERIFY(changes.isEmpty());
        QVERIFY(!project.restoreParameterSnapshot(snapshot,&error));
        QCOMPARE(project.runtimeConfig().parameters[0].currentValue,QString("1"));
        QVERIFY(!project.isModified());
        b["value"]="6"; values[1]=b; snapshot["parameters"]=values;
        QVERIFY(project.previewParameterSnapshot(snapshot,&changes,&error));
        QCOMPARE(changes.size(),2);
        QVERIFY(project.restoreParameterSnapshot(snapshot,&error));
        QCOMPARE(project.runtimeConfig().parameters[0].currentValue,QString("5"));
        QVERIFY(!project.runtimeConfig().parameters[0].confirmed);
        QVERIFY(project.isModified());
        auto roundtrip=project.parameterSnapshot();
        QVERIFY(project.restoreParameterSnapshot(roundtrip,&error));
        b["id"]="missing"; values[1]=b; snapshot["parameters"]=values;
        QVERIFY(!project.restoreParameterSnapshot(snapshot,&error));
        b["id"]="a"; values[1]=b; snapshot["parameters"]=values;
        QVERIFY(!project.restoreParameterSnapshot(snapshot,&error));
        snapshot=roundtrip; snapshot["version"]=2;
        QVERIFY(!project.restoreParameterSnapshot(snapshot,&error));
        snapshot=roundtrip; values=snapshot["parameters"].toArray();
        a=values[0].toObject(); a["dataType"]="REAL"; values[0]=a; snapshot["parameters"]=values;
        QVERIFY(!project.restoreParameterSnapshot(snapshot,&error));
    }
    void nativeArtifactsParseAndProjectParametersRoundTrip() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        ProjectRuntimeConfig config; config.projectName="parity & format";
        auto parameter=definition("Gain","gain"); parameter.unit="bar"; parameter.currentValue="5";
        config.parameters={parameter}; config.opcServer.enabled=true;
        const auto restored=ProjectRuntimeConfig::fromJson(config.toJson());
        QCOMPARE(restored.parameters.size(),1);
        QCOMPARE(restored.parameters[0].toJson(),parameter.toJson());
        BuildController build;
        QSignalSpy success(&build,&BuildController::compileSucceeded);
        build.compileParameters(directory.path(),config); QCOMPARE(success.count(),1);
        bool dataParsed=false;
        for (const auto& artifact : build.lastCompileResult().artifacts) {
            if (artifact.format!="lh_parameter_data") continue;
            QFile file(artifact.path); QVERIFY(file.open(QIODevice::ReadOnly));
            for (const auto& line : QString::fromUtf8(file.readAll()).split('\n')) {
                if (!line.startsWith("SET\t")) continue;
                const auto columns=line.trimmed().split('\t'); QCOMPARE(columns.size(),8);
                QCOMPARE(columns[1],parameter.name); QCOMPARE(columns[2],parameter.dataType);
                QCOMPARE(columns[3],parameter.currentValue); QCOMPARE(columns[4],parameter.defaultValue);
                dataParsed=true;
            }
        }
        QVERIFY(dataParsed);
        build.compileCommunication(directory.path(),config); QCOMPARE(success.count(),2);
        bool xmlParsed=false;
        for (const auto& artifact : build.lastCompileResult().artifacts) {
            if (!artifact.path.endsWith(".xml")) continue;
            QFile file(artifact.path); QVERIFY(file.open(QIODevice::ReadOnly));
            QXmlStreamReader reader(file.readAll());
            QVERIFY(reader.readNextStartElement()); QCOMPARE(reader.name().toString(),QString("lhCommunicationConfig"));
            QCOMPARE(reader.attributes().value("project").toString(),config.projectName);
            QCOMPARE(reader.attributes().value("generatedBy").toString(),QString("LH"));
            while (!reader.atEnd()) reader.readNext();
            QVERIFY(!reader.hasError()); xmlParsed=true;
        }
        QVERIFY(xmlParsed);
    }
    void newProjectTemplateCompiles() {
        QTemporaryDir directory(QStringLiteral(LH_SOURCE_DIR) + "/build_current_mingw/parity-template-XXXXXX"); QVERIFY(directory.isValid());
        ProjectController project; project.setDefaultProjectDir(directory.path());
        connect(&project,&ProjectController::projectNameRequired,this,[](QString& name,bool& accepted){name="parity";accepted=true;});
        connect(&project,&ProjectController::directorySelectionRequired,this,[&](const QString&,const QString&,QString& dir,bool& accepted){dir=directory.path();accepted=true;});
        QString projectError;
        connect(&project,&ProjectController::logMessage,this,[&](const QString& message){ projectError += message + "\n"; });
        connect(&project,&ProjectController::errorOccurred,this,[&](const QString&,const QString& message){ projectError += message + "\n"; });
        project.createNewProject(); QVERIFY2(project.hasOpenProject(),qPrintable(projectError));
        QFile file(project.currentMainScriptFile()); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto content=file.readAll(); QVERIFY(!content.contains("add_1"));
        const QString root=QStringLiteral(LH_SOURCE_DIR);
        QProcess process;
        process.setWorkingDirectory(root+"/third_party/custom_dsp_language/compile");
        process.start(root+"/third_party/custom_dsp_language/compile/venv/Scripts/python.exe",
                      {"lmc.py",project.currentMainScriptFile(),"--check"});
        QVERIFY(process.waitForFinished(30000));
        QVERIFY2(process.exitCode()==0,process.readAllStandardOutput()+process.readAllStandardError());
    }
    void continuousStepWaitsForCompletion() {
        struct Gate { QSemaphore entered, resume; std::atomic<int> writes{0}; };
        auto gate=std::make_shared<Gate>();
        class SlowTransport : public ParityDebugTransport {
        public:
            explicit SlowTransport(std::shared_ptr<Gate> state) : gate(std::move(state)) {}
            bool writeMultipleRegisters(int address,const QVector<quint16>& values) override {
                if (++gate->writes == 1) { gate->entered.release(); gate->resume.tryAcquire(1,3000); }
                return ParityDebugTransport::writeMultipleRegisters(address,values);
            }
            std::shared_ptr<Gate> gate;
        };
        ControllerDebugClient client;
        client.enableWorkerThread([gate]{return new SlowTransport(gate);});
        const auto release=qScopeGuard([gate]{gate->resume.release();});
        ControllerDeviceBackend backend; backend.setDebugClientForTest(&client);
        ProjectRuntimeConfig config; config.transport.parameters.insert("port","parity-slow-debug");
        QVERIFY(backend.configure(config)); QVERIFY(backend.connectBackend());
        RuntimeSessionController controller; controller.setDeviceBackend(&backend);
        QVERIFY(controller.startContinuousStep(50));
        QTRY_VERIFY_WITH_TIMEOUT(gate->entered.available()==1,2000);
        QTest::qWait(180); QCOMPARE(gate->writes.load(),1);
        QVERIFY(controller.isDebugCommandPending());
        gate->resume.release();
        QTRY_VERIFY_WITH_TIMEOUT(gate->writes.load() >= 2,2000);
        controller.stopContinuousStep(); QTest::qWait(150);
        const auto count=gate->writes.load(); QTest::qWait(150); QCOMPARE(gate->writes.load(),count);
    }
    void continuousStepSerializesAndCancels() {
        ParityDebugTransport transport; ControllerDebugClient client(&transport);
        ControllerDeviceBackend backend; backend.setDebugClientForTest(&client);
        ProjectRuntimeConfig config; config.transport.parameters.insert("port","parity-debug");
        QVERIFY(backend.configure(config)); QVERIFY(backend.connectBackend());
        RuntimeSessionController controller; controller.setDeviceBackend(&backend);
        QVERIFY(!controller.startContinuousStep(49)); QVERIFY(!controller.startContinuousStep(60001));
        QVERIFY(controller.startContinuousStep(50));
        QVERIFY(!controller.startContinuousStep(50));
        QTRY_VERIFY_WITH_TIMEOUT(transport.writes >= 3,2000);
        controller.stopContinuousStep(); QTRY_VERIFY(!controller.isDebugCommandPending());
        const int stopped=transport.writes; QTest::qWait(150); QCOMPARE(transport.writes,stopped);
        QVERIFY(controller.startContinuousStep(50)); controller.requestStop();
        QTest::qWait(150); QVERIFY(!controller.isContinuousStepActive());
        const int canceled=transport.writes; QTest::qWait(150); QCOMPARE(transport.writes,canceled);
        QVERIFY(controller.startContinuousStep(50));
        controller.setProjectController(nullptr);
        QVERIFY(!controller.isContinuousStepActive());
        QTRY_VERIFY(!controller.isDebugCommandPending());
        QTest::qWait(50); // Drain cancellation in the backend before a new admission.
        QVERIFY(controller.startContinuousStep(50)); backend.disconnectBackend();
        QVERIFY(!controller.isContinuousStepActive());
    }
};
QTEST_GUILESS_MAIN(LmParitySoftwareTest)
#include "lm_parity_software_test.moc"
