/**
 * @file main_window_integration_test.cpp
 * @brief MainWindow 参数下发和诊断集成测试
 */

#include <QtTest/QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QAction>
#include <QLabel>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QTemporaryDir>

#ifndef LH_BUILD_CONTROLLER_TESTING
#define LH_BUILD_CONTROLLER_TESTING 0
#endif

#include "designer/BuildController.h"
#include "designer/MainWindow.h"
#include "designer/ParameterController.h"
#include "designer/ProjectController.h"
#include "designer/RuntimeSessionController.h"
#include "designer/ui/ProblemsPanel.h"
#include "communication/VirtualDeviceBackend.h"
#include "monitor/MonitorManager.h"

Q_DECLARE_METATYPE(BuildType)

class MainWindowIntegrationTest : public QObject
{
    Q_OBJECT

private:
    static QString readTextFile(const QString& filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();
        return QString::fromUtf8(file.readAll());
    }

    static QString fileChecksum(const QString& filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            return QString();
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(&file);
        return QString::fromLatin1(hash.result().toHex());
    }

    static bool hasPublishResidue(const QString& outputDir)
    {
        const QStringList entries = QDir(outputDir).entryList(
                QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
        for (const auto& entry : entries) {
            if (entry.contains(QStringLiteral(".lh-stage-"))
                    || entry.contains(QStringLiteral(".lh-backup-"))) {
                return true;
            }
        }
        return false;
    }

    static ParameterDefinition makeParameter()
    {
        ParameterDefinition p;
        p.id = QStringLiteral("param.kp");
        p.name = QStringLiteral("Kp");
        p.dataType = QStringLiteral("REAL");
        p.defaultValue = QStringLiteral("1.0");
        p.currentValue = QStringLiteral("1.0");
        p.onlineEditable = true;
        p.confirmed = false;
        return p;
    }

    static VariableDefinition makeVariable()
    {
        VariableDefinition v;
        v.id = QStringLiteral("var.speed");
        v.name = QStringLiteral("Speed");
        v.dataType = QStringLiteral("REAL");
        v.scope = QStringLiteral("global");
        v.defaultValue = QStringLiteral("0");
        v.binding = QStringLiteral("speed.feedback");
        v.metadata.insert(QStringLiteral("opcItemId"), QStringLiteral("CommPort.InitDevParamnt.4:20"));
        return v;
    }

    static RuntimePointDefinition makePoint()
    {
        RuntimePointDefinition point;
        point.id = QStringLiteral("param.kp");
        point.name = QStringLiteral("Kp");
        point.kind = RuntimePointKind::Parameter;
        point.access = RuntimePointAccess::ReadWrite;
        point.dataType = QStringLiteral("REAL");
        point.defaultValue = 1.0;
        return point;
    }

private slots:
    void init()
    {
        qRegisterMetaType<ProjectRuntimeConfig>("ProjectRuntimeConfig");
        qRegisterMetaType<BuildType>("BuildType");
        auto& manager = Monitor::MonitorManager::instance();
        manager.setDatabaseLoggingEnabled(false);
        manager.stopMonitoring();
        manager.setDeviceBackend(nullptr);
        manager.applyConfiguration(ProjectRuntimeConfig());
        manager.clearAllData();
    }

    void cleanup()
    {
        auto& manager = Monitor::MonitorManager::instance();
        manager.stopMonitoring();
        manager.setDeviceBackend(nullptr);
        manager.applyConfiguration(ProjectRuntimeConfig());
        manager.clearAllData();
    }

    void applyParameterSyncsConfirmedAndCurrentValue()
    {
        QScopedPointer<MainWindow> window(new MainWindow());
        auto* projectController = window->findChild<ProjectController*>();
        auto* parameterController = window->findChild<ParameterController*>();
        QVERIFY(projectController != nullptr);
        QVERIFY(parameterController != nullptr);

        ProjectRuntimeConfig cfg;
        cfg.projectName = QStringLiteral("mw-integration-test");
        cfg.parameters.append(makeParameter());
        projectController->runtimeConfig() = cfg;

        QVERIFY(QMetaObject::invokeMethod(window.data(),
            "onProjectOpened",
            Qt::DirectConnection,
            Q_ARG(ProjectRuntimeConfig, cfg)));

        QVERIFY(parameterController->editParameter(QStringLiteral("Kp"), QStringLiteral("2.5")));

        VirtualDeviceBackend backend;
        backend.loadPointDefinitions({makePoint()});
        backend.connectBackend();
        Monitor::MonitorManager::instance().setDeviceBackend(&backend);

        QSignalSpy readbackSpy(parameterController, &ParameterController::readbackFinished);
        QVERIFY(QMetaObject::invokeMethod(window.data(), "onApplyParametersRequested", Qt::DirectConnection));
        QTRY_COMPARE(readbackSpy.count(), 1);
        QCOMPARE(readbackSpy.takeFirst().at(0).toBool(), true);

        const auto& updatedCfg = window->runtimeConfig();
        QCOMPARE(updatedCfg.parameters.size(), 1);
        QCOMPARE(updatedCfg.parameters.first().name, QStringLiteral("Kp"));
        QVERIFY(updatedCfg.parameters.first().confirmed);
        QCOMPARE(updatedCfg.parameters.first().currentValue, QStringLiteral("2.5"));

        Monitor::MonitorManager::instance().setDeviceBackend(nullptr);
    }

    void downloadDiagnosticAddsProblemEntry()
    {
        QScopedPointer<MainWindow> window(new MainWindow());
        auto* sessionController = window->findChild<RuntimeSessionController*>();
        auto* problemsPanel = window->findChild<ProblemsPanel*>();
        QVERIFY(sessionController != nullptr);
        QVERIFY(problemsPanel != nullptr);

        const int beforeCount = problemsPanel->problemCount();
        QVariantMap diagnostic;
        diagnostic.insert(QStringLiteral("severity"), QStringLiteral("error"));
        diagnostic.insert(QStringLiteral("stage"), QStringLiteral("precheck"));
        diagnostic.insert(QStringLiteral("message"), QStringLiteral("下载前置校验失败：测试诊断"));

        emit sessionController->downloadDiagnosticChanged(diagnostic);

        QCOMPARE(problemsPanel->problemCount(), beforeCount + 1);
    }

    void runtimeStatusAndMonitorButtonFollowSessionSignals()
    {
        QScopedPointer<MainWindow> window(new MainWindow());
        auto* sessionController = window->findChild<RuntimeSessionController*>();
        QVERIFY(sessionController != nullptr);

        QLabel* statusLabel = nullptr;
        for (QLabel* label : window->findChildren<QLabel*>()) {
            if (label->text() == QStringLiteral("就绪")) {
                statusLabel = label;
                break;
            }
        }
        QVERIFY(statusLabel != nullptr);

        sessionController->executeRun();
        QCOMPARE(statusLabel->text(), QStringLiteral("运行中"));
        QAction* runAction = nullptr;
        for (QAction* action : window->findChildren<QAction*>()) {
            if (action->text().contains(QStringLiteral("运行项目"))) {
                runAction = action;
                break;
            }
        }
        QVERIFY(runAction != nullptr);
        QVERIFY(!runAction->isEnabled());

        sessionController->startMonitoring();
        QCOMPARE(statusLabel->text(), QStringLiteral("监控中"));
        sessionController->stopMonitoring();
        QCOMPARE(statusLabel->text(), QStringLiteral("运行中"));

        sessionController->requestStop();
        QCOMPARE(statusLabel->text(), QStringLiteral("已停止"));
        QVERIFY(!sessionController->isRunning());
        QVERIFY(!sessionController->isMonitoring());
    }

    void projectClosedStopsRuntimeSession()
    {
        QScopedPointer<MainWindow> window(new MainWindow());
        auto* sessionController = window->findChild<RuntimeSessionController*>();
        auto* projectController = window->findChild<ProjectController*>();
        QVERIFY(sessionController != nullptr);
        QVERIFY(projectController != nullptr);

        sessionController->executeRun();
        QCOMPARE(sessionController->state(), RuntimeSessionState::Running);

        emit projectController->projectClosed();

        QCOMPARE(sessionController->state(), RuntimeSessionState::Idle);
        QVERIFY(!sessionController->isRunning());
        QVERIFY(!sessionController->isMonitoring());
    }

    void stopActionStopsConnectedAndFaultSessions()
    {
        QScopedPointer<MainWindow> window(new MainWindow());
        auto* sessionController = window->findChild<RuntimeSessionController*>();
        QVERIFY(sessionController != nullptr);

        QAction* stopAction = nullptr;
        for (QAction* action : window->findChildren<QAction*>()) {
            if (action->text().contains(QStringLiteral("停止项目"))) {
                stopAction = action;
                break;
            }
        }
        QVERIFY(stopAction != nullptr);

        VirtualDeviceBackend backend;
        QVERIFY(backend.connectBackend());
        sessionController->setDeviceBackend(&backend);
        QCOMPARE(sessionController->state(), RuntimeSessionState::Connected);
        QVERIFY(stopAction->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(window.data(), "onStopProject", Qt::DirectConnection));
        QCOMPARE(sessionController->state(), RuntimeSessionState::Idle);

        QVERIFY(backend.connectBackend());
        QCOMPARE(sessionController->state(), RuntimeSessionState::Connected);
        backend.disconnectBackend();
        QCOMPARE(sessionController->state(), RuntimeSessionState::Fault);
        QVERIFY(stopAction->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(window.data(), "onStopProject", Qt::DirectConnection));
        QCOMPARE(sessionController->state(), RuntimeSessionState::Idle);

        sessionController->setDeviceBackend(nullptr);
    }

    void generatedBuildArtifactsUseLhProjectModel()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ProjectRuntimeConfig cfg;
        cfg.projectName = QStringLiteral("artifact_probe");
        cfg.parameters.append(makeParameter());
        cfg.variables.append(makeVariable());
        cfg.opcServer.enabled = true;
        cfg.opcServer.channelName = QStringLiteral("CommPort");
        cfg.opcServer.deviceName = QStringLiteral("COM9");
        cfg.opcServer.serialMode = QStringLiteral("19200,N,8,1");

        BuildController buildController;
        QSignalSpy parameterSuccessSpy(&buildController, &BuildController::compileSucceeded);
        buildController.compileParameters(tempDir.path(), cfg);
        QCOMPARE(parameterSuccessSpy.count(), 1);

        const QString parameterDir = QDir(tempDir.path()).absoluteFilePath(QStringLiteral("build_output/parameters"));
        const QString dataPath = QDir(parameterDir).absoluteFilePath(QStringLiteral("artifact_probe.data"));
        const QString parameterReportPath = QDir(parameterDir).absoluteFilePath(QStringLiteral("artifact_probe.rep"));
        QVERIFY(QFileInfo::exists(dataPath));
        QVERIFY(QFileInfo::exists(parameterReportPath));

        const QString dataText = readTextFile(dataPath);
        const QString parameterReportText = readTextFile(parameterReportPath);
        QVERIFY(dataText.startsWith(QStringLiteral("# LH parameter data file")));
        QVERIFY(dataText.contains(QStringLiteral("SET\tKp\tREAL\t1.0")));
        QVERIFY(parameterReportText.contains(QStringLiteral("status\tsuccess")));

        const CompileResult parameterResult = buildController.lastCompileResult();
        QVERIFY(parameterResult.success);
        for (const auto& artifact : parameterResult.artifacts) {
            QVERIFY(QFileInfo::exists(artifact.path));
            QVERIFY(!artifact.path.contains(QStringLiteral(".lh-stage-")));
            QVERIFY(!artifact.path.contains(QStringLiteral(".lh-backup-")));
            QCOMPARE(artifact.checksum, fileChecksum(artifact.path));
        }
        bool hasParameterData = false;
        for (const auto& artifact : parameterResult.artifacts) {
            if (artifact.type == QStringLiteral("parameter_data")
                    && artifact.format == QStringLiteral("lh_parameter_data")
                    && QFileInfo::exists(artifact.path)
                    && !artifact.checksum.isEmpty()) {
                hasParameterData = true;
            }
        }
        QVERIFY(hasParameterData);

        QSignalSpy communicationSuccessSpy(&buildController, &BuildController::compileSucceeded);
        buildController.compileCommunication(tempDir.path(), cfg);
        QCOMPARE(communicationSuccessSpy.count(), 1);

        const QString communicationDir = QDir(tempDir.path()).absoluteFilePath(QStringLiteral("build_output/communication"));
        const QString xmlPath = QDir(communicationDir).absoluteFilePath(QStringLiteral("artifact_probe.xml"));
        const QString tagPath = QDir(communicationDir).absoluteFilePath(QStringLiteral("artifact_probe.tag"));
        const QString actPath = QDir(communicationDir).absoluteFilePath(QStringLiteral("artifact_probe.act"));
        const QString txPath = QDir(communicationDir).absoluteFilePath(QStringLiteral("artifact_probe.tx"));
        const QString rxPath = QDir(communicationDir).absoluteFilePath(QStringLiteral("artifact_probe.rx"));
        const QString rtPath = QDir(communicationDir).absoluteFilePath(QStringLiteral("artifact_probe.rt"));
        const QString engPath = QDir(communicationDir).absoluteFilePath(QStringLiteral("artifact_probe.eng"));
        const QString commPath = QDir(communicationDir).absoluteFilePath(QStringLiteral("artifact_probe.comm"));
        const QString msgPath = QDir(communicationDir).absoluteFilePath(QStringLiteral("artifact_probe.msg"));
        const QString communicationReportPath = QDir(communicationDir).absoluteFilePath(QStringLiteral("artifact_probe.rep"));
        QVERIFY(QFileInfo::exists(xmlPath));
        QVERIFY(QFileInfo::exists(tagPath));
        QVERIFY(QFileInfo::exists(actPath));
        QVERIFY(QFileInfo::exists(txPath));
        QVERIFY(QFileInfo::exists(rxPath));
        QVERIFY(QFileInfo::exists(rtPath));
        QVERIFY(QFileInfo::exists(engPath));
        QVERIFY(QFileInfo::exists(commPath));
        QVERIFY(QFileInfo::exists(msgPath));
        QVERIFY(QFileInfo::exists(communicationReportPath));

        const QString xmlText = readTextFile(xmlPath);
        const QString tagText = readTextFile(tagPath);
        const QString actText = readTextFile(actPath);
        const QString txText = readTextFile(txPath);
        const QString rxText = readTextFile(rxPath);
        const QString rtText = readTextFile(rtPath);
        const QString engText = readTextFile(engPath);
        const QString commText = readTextFile(commPath);
        const QString msgText = readTextFile(msgPath);
        const QString communicationReportText = readTextFile(communicationReportPath);
        QVERIFY(xmlText.contains(QStringLiteral("<lhCommunicationConfig")));
        QVERIFY(xmlText.contains(QStringLiteral("generatedBy=\"LH\"")));
        QVERIFY(tagText.startsWith(QStringLiteral("# LH communication tag file")));
        QVERIFY(tagText.contains(QStringLiteral("Speed\tREAL")));
        QVERIFY(actText.startsWith(QStringLiteral("# LH communication parameter file")));
        QVERIFY(actText.contains(QStringLiteral("opcItemId")));
        QVERIFY(txText.startsWith(QStringLiteral("# LH communication transmit view")));
        QVERIFY(rxText.startsWith(QStringLiteral("# LH communication receive view")));
        QVERIFY(rtText.startsWith(QStringLiteral("# LH communication realtime view")));
        QVERIFY(engText.startsWith(QStringLiteral("# LH engineering value view")));
        QVERIFY(commText.startsWith(QStringLiteral("# LH communication address view")));
        QVERIFY(msgText.startsWith(QStringLiteral("# LH communication debug index")));
        QVERIFY(txText.contains(QStringLiteral("Speed")));
        QVERIFY(rxText.contains(QStringLiteral("Kp")));
        QVERIFY(rtText.contains(QStringLiteral("Speed")) || rtText.contains(QStringLiteral("Kp")));
        QVERIFY(engText.contains(QStringLiteral("Kp")));
        QVERIFY(commText.contains(QStringLiteral("CommPort.InitDevParamnt.4:20")));
        QVERIFY(msgText.contains(QStringLiteral("status\tsuccess")));
        QVERIFY(communicationReportText.contains(QStringLiteral("tag_count")));
        const QString generatedMarker = QString(QChar(0x4c)) + QChar(0x4d)
                + QLatin1Char(' ')
                + QStringLiteral("compiler");
        QVERIFY(!xmlText.contains(generatedMarker, Qt::CaseInsensitive));
        QVERIFY(!tagText.contains(generatedMarker, Qt::CaseInsensitive));
        QVERIFY(!actText.contains(generatedMarker, Qt::CaseInsensitive));
        QVERIFY(!txText.contains(generatedMarker, Qt::CaseInsensitive));
        QVERIFY(!rxText.contains(generatedMarker, Qt::CaseInsensitive));
        QVERIFY(!rtText.contains(generatedMarker, Qt::CaseInsensitive));
        QVERIFY(!engText.contains(generatedMarker, Qt::CaseInsensitive));
        QVERIFY(!commText.contains(generatedMarker, Qt::CaseInsensitive));
        QVERIFY(!msgText.contains(generatedMarker, Qt::CaseInsensitive));

        const CompileResult communicationResult = buildController.lastCompileResult();
        QVERIFY(communicationResult.success);
        for (const auto& artifact : communicationResult.artifacts) {
            QVERIFY(QFileInfo::exists(artifact.path));
            QVERIFY(!artifact.path.contains(QStringLiteral(".lh-stage-")));
            QVERIFY(!artifact.path.contains(QStringLiteral(".lh-backup-")));
            QCOMPARE(artifact.checksum, fileChecksum(artifact.path));
        }
        bool hasCommunicationXml = false;
        bool hasCommunicationTags = false;
        bool hasCommunicationAct = false;
        bool hasCommunicationTx = false;
        bool hasCommunicationRx = false;
        bool hasCommunicationRt = false;
        bool hasCommunicationEng = false;
        bool hasCommunicationComm = false;
        bool hasCommunicationMsg = false;
        for (const auto& artifact : communicationResult.artifacts) {
            if (artifact.type == QStringLiteral("communication_xml")
                    && artifact.format == QStringLiteral("lh_communication_xml")
                    && QFileInfo::exists(artifact.path)
                    && !artifact.checksum.isEmpty()) {
                hasCommunicationXml = true;
            }
            if (artifact.type == QStringLiteral("communication_tags")
                    && artifact.format == QStringLiteral("lh_communication_tags")
                    && QFileInfo::exists(artifact.path)
                    && !artifact.checksum.isEmpty()) {
                hasCommunicationTags = true;
            }
            if (artifact.type == QStringLiteral("communication_act")
                    && artifact.format == QStringLiteral("lh_communication_act")
                    && QFileInfo::exists(artifact.path)
                    && !artifact.checksum.isEmpty()) {
                hasCommunicationAct = true;
            }
            if (artifact.type == QStringLiteral("communication_tx")
                    && artifact.format == QStringLiteral("lh_communication_tx")
                    && QFileInfo::exists(artifact.path)
                    && !artifact.checksum.isEmpty()) {
                hasCommunicationTx = true;
            }
            if (artifact.type == QStringLiteral("communication_rx")
                    && artifact.format == QStringLiteral("lh_communication_rx")
                    && QFileInfo::exists(artifact.path)
                    && !artifact.checksum.isEmpty()) {
                hasCommunicationRx = true;
            }
            if (artifact.type == QStringLiteral("communication_rt")
                    && artifact.format == QStringLiteral("lh_communication_rt")
                    && QFileInfo::exists(artifact.path)
                    && !artifact.checksum.isEmpty()) {
                hasCommunicationRt = true;
            }
            if (artifact.type == QStringLiteral("communication_engineering")
                    && artifact.format == QStringLiteral("lh_communication_engineering")
                    && QFileInfo::exists(artifact.path)
                    && !artifact.checksum.isEmpty()) {
                hasCommunicationEng = true;
            }
            if (artifact.type == QStringLiteral("communication_comm")
                    && artifact.format == QStringLiteral("lh_communication_comm")
                    && QFileInfo::exists(artifact.path)
                    && !artifact.checksum.isEmpty()) {
                hasCommunicationComm = true;
            }
            if (artifact.type == QStringLiteral("communication_debug")
                    && artifact.format == QStringLiteral("lh_communication_debug")
                    && QFileInfo::exists(artifact.path)
                    && !artifact.checksum.isEmpty()) {
                hasCommunicationMsg = true;
            }
        }
        QVERIFY(hasCommunicationXml);
        QVERIFY(hasCommunicationTags);
        QVERIFY(hasCommunicationAct);
        QVERIFY(hasCommunicationTx);
        QVERIFY(hasCommunicationRx);
        QVERIFY(hasCommunicationRt);
        QVERIFY(hasCommunicationEng);
        QVERIFY(hasCommunicationComm);
        QVERIFY(hasCommunicationMsg);
    }

    void parameterArtifactFailurePreservesPreviousFiles()
    {
#if !LH_BUILD_CONTROLLER_TESTING
        QSKIP("需要以 LH_ENABLE_TEST_FAILURE_INJECTION=ON 构建测试注入路径。");
#else
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ProjectRuntimeConfig cfg;
        cfg.projectName = QStringLiteral("atomic_parameter_probe");
        cfg.parameters.append(makeParameter());

        BuildController buildController;
        QSignalSpy successSpy(&buildController, &BuildController::compileSucceeded);
        buildController.compileParameters(tempDir.path(), cfg);
        QCOMPARE(successSpy.count(), 1);

        const QString outputDir = QDir(tempDir.path()).absoluteFilePath(
                QStringLiteral("build_output/parameters"));
        const QString dataPath = QDir(outputDir).absoluteFilePath(
                QStringLiteral("atomic_parameter_probe.data"));
        const QString reportPath = QDir(outputDir).absoluteFilePath(
                QStringLiteral("atomic_parameter_probe.rep"));
        const QString oldData = readTextFile(dataPath);
        const QString oldReport = readTextFile(reportPath);
        buildController.setProperty("lh_test_inject_publish_failure_after_first", true);

        QSignalSpy failureSpy(&buildController, &BuildController::compileFailed);
        buildController.compileParameters(tempDir.path(), cfg);
        QCOMPARE(successSpy.count(), 1);
        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(!buildController.lastCompileResult().success);
        QCOMPARE(readTextFile(dataPath), oldData);
        QCOMPARE(readTextFile(reportPath), oldReport);
        QVERIFY(!hasPublishResidue(outputDir));
#endif
    }

    void parameterArtifactFailureLeavesNoNewFormalFiles()
    {
#if !LH_BUILD_CONTROLLER_TESTING
        QSKIP("需要以 LH_ENABLE_TEST_FAILURE_INJECTION=ON 构建测试注入路径。");
#else
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ProjectRuntimeConfig cfg;
        cfg.projectName = QStringLiteral("atomic_parameter_empty");
        cfg.parameters.append(makeParameter());

        const QString outputDir = QDir(tempDir.path()).absoluteFilePath(
                QStringLiteral("build_output/parameters"));
        QVERIFY(QDir().mkpath(outputDir));
        const QString dataPath = QDir(outputDir).absoluteFilePath(
                QStringLiteral("atomic_parameter_empty.data"));
        const QString reportPath = QDir(outputDir).absoluteFilePath(
                QStringLiteral("atomic_parameter_empty.rep"));
        const QString unrelatedPath = QDir(outputDir).absoluteFilePath(QStringLiteral("keep.me"));
        QFile unrelated(unrelatedPath);
        QVERIFY(unrelated.open(QIODevice::WriteOnly | QIODevice::Text));
        unrelated.write("unrelated");
        unrelated.close();

        BuildController buildController;
        buildController.setProperty("lh_test_inject_publish_failure_after_first", true);
        QSignalSpy successSpy(&buildController, &BuildController::compileSucceeded);
        QSignalSpy failureSpy(&buildController, &BuildController::compileFailed);
        buildController.compileParameters(tempDir.path(), cfg);

        QCOMPARE(successSpy.count(), 0);
        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(!buildController.lastCompileResult().success);
        QVERIFY(!QFileInfo::exists(reportPath));
        QVERIFY(!QFileInfo::exists(dataPath));
        QCOMPARE(readTextFile(unrelatedPath), QStringLiteral("unrelated"));
        QVERIFY(!hasPublishResidue(outputDir));
#endif
    }

    void communicationArtifactFailureLeavesNoNewFormalFiles()
    {
#if !LH_BUILD_CONTROLLER_TESTING
        QSKIP("需要以 LH_ENABLE_TEST_FAILURE_INJECTION=ON 构建测试注入路径。");
#else
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ProjectRuntimeConfig cfg;
        cfg.projectName = QStringLiteral("atomic_communication_empty");
        cfg.variables.append(makeVariable());

        const QString outputDir = QDir(tempDir.path()).absoluteFilePath(
                QStringLiteral("build_output/communication"));
        QVERIFY(QDir().mkpath(outputDir));
        const QString xmlPath = QDir(outputDir).absoluteFilePath(
                QStringLiteral("atomic_communication_empty.xml"));
        const QString tagPath = QDir(outputDir).absoluteFilePath(
                QStringLiteral("atomic_communication_empty.tag"));
        const QString unrelatedPath = QDir(outputDir).absoluteFilePath(QStringLiteral("keep.me"));
        QFile unrelated(unrelatedPath);
        QVERIFY(unrelated.open(QIODevice::WriteOnly | QIODevice::Text));
        unrelated.write("unrelated");
        unrelated.close();

        BuildController buildController;
        buildController.setProperty("lh_test_inject_publish_failure_after_first", true);
        QSignalSpy successSpy(&buildController, &BuildController::compileSucceeded);
        QSignalSpy failureSpy(&buildController, &BuildController::compileFailed);
        buildController.compileCommunication(tempDir.path(), cfg);

        QCOMPARE(successSpy.count(), 0);
        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(!buildController.lastCompileResult().success);
        QVERIFY(!QFileInfo::exists(xmlPath));
        QVERIFY(!QFileInfo::exists(tagPath));
        QCOMPARE(readTextFile(unrelatedPath), QStringLiteral("unrelated"));
        QVERIFY(!hasPublishResidue(outputDir));
#endif
    }
};

QTEST_MAIN(MainWindowIntegrationTest)
#include "main_window_integration_test.moc"
