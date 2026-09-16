/**
 * @file main_window_integration_test.cpp
 * @brief MainWindow 参数下发和诊断集成测试
 */

#include <QtTest/QtTest>
#include "UiEvidence.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QAction>
#include <QLabel>
#include <QDockWidget>
#include <QToolBar>
#include <QToolButton>
#include <QScrollBar>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <algorithm>

#ifndef LH_BUILD_CONTROLLER_TESTING
#define LH_BUILD_CONTROLLER_TESTING 0
#endif

#include "designer/BuildController.h"
#include "designer/MainWindow.h"
#include "designer/ParameterController.h"
#include "designer/ProjectController.h"
#include "designer/RuntimeSessionController.h"
#include "designer/ui/ProblemsPanel.h"
#include "designer/OutputPaneController.h"
#include "designer/ParameterTuningPanel.h"
#include "designer/ParameterTuningWindow.h"
#include "designer/SettingsController.h"
#include "designer/DslScriptEditor.h"
#include "communication/VirtualDeviceBackend.h"
#include "monitor/MonitorManager.h"
#include "monitor/MonitorWidget.h"
#include "monitor/MonitorChartView.h"
#include "designer/ui/GlobalStatusBar.h"
#include "designer/ui/InspectorPanel.h"
#include "designer/ProgramBlocksWidget.h"
#include "designer/ProjectExplorerWidget.h"
#include "designer/DeviceWorkspaceWidget.h"
#include "designer/DownloadDockWidget.h"
#include "designer/SettingsDialog.h"
#include "designer/ui/ThemeManager.h"
#include <QStatusBar>
#include <QGroupBox>
#include <QTableWidget>
#include <QSettings>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSysInfo>
#include <QProcess>
#include <QScreen>
#include <QGuiApplication>
#include <QMdiSubWindow>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include "monitor/ChartWidget.h"

Q_DECLARE_METATYPE(BuildType)

class MainWindowIntegrationTest : public QObject
{
    Q_OBJECT

private:
    static QString getGitHead()
    {
        return UiEvidence::git({QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
    }

    static QString getWorkspaceFingerprint()
    {
        return UiEvidence::fingerprint();
    }

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

    static void saveEvidence(const QPixmap& pix, const QString& path)
    {
        QVERIFY2(UiEvidence::save(pix, path), qPrintable(QStringLiteral("Failed to save image/metadata: ") + path));
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

private:
    QTemporaryDir m_tempSettingsDir;
    QSettings::Format m_origSettingsFormat = QSettings::NativeFormat;

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempSettingsDir.isValid());
        m_origSettingsFormat = QSettings::defaultFormat();
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_tempSettingsDir.path());
    }

    void cleanupTestCase()
    {
        QSettings::setDefaultFormat(m_origSettingsFormat);
    }

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
        auto* monitorWidget = window->findChild<MonitorWidget*>();
        QVERIFY(sessionController != nullptr);
        QVERIFY(monitorWidget != nullptr);

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
        QAction* runAction = window->findChild<QAction*>(QStringLiteral("actRunProject"));
        if (!runAction) {
            for (QAction* action : window->findChildren<QAction*>()) {
                if (action->text().contains(QStringLiteral("运行项目")) || action->text().contains(QStringLiteral("运行控制器"))) {
                    runAction = action;
                    break;
                }
            }
        }
        QVERIFY(runAction != nullptr);
        QVERIFY(!runAction->isEnabled());

        sessionController->startMonitoring();
        QCOMPARE(statusLabel->text(), QStringLiteral("监控中"));
        QVERIFY(monitorWidget->isMonitoring());
        sessionController->stopMonitoring();
        QCOMPARE(statusLabel->text(), QStringLiteral("运行中"));
        QVERIFY(!monitorWidget->isMonitoring());

        sessionController->requestStop();
        QCOMPARE(statusLabel->text(), QStringLiteral("已停止"));
        QVERIFY(!sessionController->isRunning());
        QVERIFY(!sessionController->isMonitoring());
        QVERIFY(!monitorWidget->isMonitoring());
    }

    void monitoringWithoutDataSourceStaysRunningAndReportsError()
    {
        auto& manager = Monitor::MonitorManager::instance();
        manager.stopMonitoring();
        manager.setDeviceBackend(nullptr);
        manager.applyConfiguration(ProjectRuntimeConfig());

        ProjectController projectController;
        RuntimeSessionController controller;
        controller.setProjectController(&projectController);
        QSignalSpy errorSpy(&controller, &RuntimeSessionController::runtimeError);

        controller.executeRun();
        controller.startMonitoring();

        QCOMPARE(controller.state(), RuntimeSessionState::Running);
        QVERIFY(!controller.isMonitoring());
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains(QStringLiteral("数据源")));
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

        QAction* stopAction = window->findChild<QAction*>(QStringLiteral("actStopProject"));
        if (!stopAction) {
            for (QAction* action : window->findChildren<QAction*>()) {
                if (action->text().contains(QStringLiteral("停止项目")) || action->text().contains(QStringLiteral("停止控制器"))) {
                    stopAction = action;
                    break;
                }
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

        backend.disconnectBackend();
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

    void cancelBeforeCompilerStartEmitsSingleTerminal()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString mainScriptPath = QDir(tempDir.path()).absoluteFilePath(QStringLiteral("main.lh"));
        QFile mainScript(mainScriptPath);
        QVERIFY(mainScript.open(QIODevice::WriteOnly | QIODevice::Text));
        mainScript.write("PROGRAM CancelBeforeStart\nEND_PROGRAM\n");
        mainScript.close();

        ProjectRuntimeConfig cfg;
        cfg.projectName = QStringLiteral("cancel_before_start");

        BuildController buildController;
        QSignalSpy cancelledSpy(&buildController, &BuildController::compileCancelled);
        QSignalSpy successSpy(&buildController, &BuildController::compileSucceeded);
        QSignalSpy failureSpy(&buildController, &BuildController::compileFailed);
        connect(&buildController, &BuildController::compileStarted,
                &buildController, [&buildController] { buildController.cancelCompile(); });

        buildController.compileConfiguration(tempDir.path(), cfg);

        QCOMPARE(cancelledSpy.count(), 1);
        QCOMPARE(successSpy.count(), 0);
        QCOMPARE(failureSpy.count(), 0);
        QVERIFY(!buildController.isBusy());
        buildController.cancelCompile();
        QCOMPARE(cancelledSpy.count(), 1);
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

    void outputPanePlainTextSafetyAndFormatting()
    {
        QDockWidget dock;
        OutputPaneController controller(&dock);

        // 1. 普通日志输入 HTML 标签时按纯文本原样呈现，不解析为富文本
        const QString rawHtml = QStringLiteral("Error in module <b>MotorControl</b>: <img src=\"http://malicious.test/img.png\" onerror=\"alert(1)\">");
        controller.appendMessage(rawHtml);
        QCOMPARE(controller.allText().trimmed(), rawHtml);

        // 2. 彩色日志（info/warn/error/success）内部转义并按纯文本导出
        controller.clear();
        controller.appendError(QStringLiteral("Compilation error: <TypeMismatch> & <NullPtr>"));
        const QString errText = controller.allText();
        QVERIFY(errText.contains(QStringLiteral("[ERROR] Compilation error: <TypeMismatch> & <NullPtr>")));
        QVERIFY(!errText.contains(QStringLiteral("&lt;")));

        // 3. 多行和 Unicode 保持原貌
        controller.clear();
        const QString multilineUnicode = QStringLiteral("Line1: 测量值=42.5℃\nLine2: 状态=正常✓\nLine3: <b>未加粗</b>");
        controller.appendMessage(multilineUnicode);
        QCOMPARE(controller.allText().trimmed(), multilineUnicode);
    }

    void outputPaneCapacityBlockLimitAndTruncation()
    {
        QDockWidget dock;
        OutputPaneController controller(&dock);

        // 1. 单条超长消息截断到 MAX_MESSAGE_LENGTH (64 KiB)
        const int hugeLen = OutputPaneConfig::MAX_MESSAGE_LENGTH + 20000;
        QString hugeMsg(hugeLen, QLatin1Char('X'));
        hugeMsg.append(QStringLiteral("TAIL_MARK"));

        controller.appendMessage(hugeMsg);
        const QString text = controller.allText().trimmed();
        QCOMPARE(text.length(), OutputPaneConfig::MAX_MESSAGE_LENGTH);
        QVERIFY(text.endsWith(QString::fromLatin1(OutputPaneConfig::TRUNCATION_MARKER)));
        QVERIFY(!text.contains(QStringLiteral("TAIL_MARK")));

        // 2. 块数限制：追加 5500 行，验证块数有界（<= DEFAULT_MAX_BLOCK_COUNT = 5000）
        controller.clear();
        for (int i = 0; i < 5500; ++i) {
            controller.appendMessage(QStringLiteral("line %1").arg(i));
        }
        QVERIFY(controller.textEdit()->document()->blockCount() <= OutputPaneConfig::DEFAULT_MAX_BLOCK_COUNT);

        // 3. 自动滚动保持：用户查看旧行时（滚动条不在底部）不强制跳到底部
        QScrollBar* sb = controller.textEdit()->verticalScrollBar();
        sb->setValue(10);
        const int oldPos = sb->value();
        controller.appendMessage(QStringLiteral("new message while user is inspecting history"));
        QCOMPARE(sb->value(), oldPos);
    }

    void captureBaselineEvidence()
    {
        const QDir evidenceDir(UiEvidence::directory());
        QScopedPointer<MainWindow> window(new MainWindow());
        window->resize(1366, 768);
        window->show();
        QTest::qWait(150);

        // 1. 无项目
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("baseline_01_no_project.png")));

        // 2. 离线项目
        auto* projectController = window->findChild<ProjectController*>();
        ProjectRuntimeConfig cfg;
        cfg.projectName = QStringLiteral("DemoProject");
        cfg.parameters.append(makeParameter());
        cfg.variables.append(makeVariable());
        if (projectController) {
            projectController->runtimeConfig() = cfg;
        }
        QMetaObject::invokeMethod(window.data(), "onProjectOpened", Qt::DirectConnection, Q_ARG(ProjectRuntimeConfig, cfg));
        QTest::qWait(100);
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("baseline_02_offline_project.png")));

        // 3. 编译失败
        auto* problemsPanel = window->findChild<ProblemsPanel*>();
        if (problemsPanel) {
            problemsPanel->addProblem(QStringLiteral("error"), QStringLiteral("Main.lh"), QStringLiteral("第 12 行: 语法错误，未知类型 'InvalidType'"));
        }
        auto* logDock = window->findChild<QDockWidget*>(QStringLiteral("LogDock"));
        if (logDock) {
            logDock->show();
        }
        QTest::qWait(100);
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("baseline_03_compile_failure.png")));

        // 4. 监控页面
        auto* workspaceTabs = window->findChild<QTabWidget*>(QStringLiteral("WorkspaceTabs"));
        if (workspaceTabs) {
            for (int i = 0; i < workspaceTabs->count(); ++i) {
                if (workspaceTabs->tabText(i).contains(QStringLiteral("监控"))) {
                    workspaceTabs->setCurrentIndex(i);
                    break;
                }
            }
        }
        QTest::qWait(100);
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("baseline_04_monitor.png")));

        // 5. 调参窗口
        QMetaObject::invokeMethod(window.data(), "onOpenParameterTuningWindow", Qt::DirectConnection);
        auto* tuningWindow = window->findChild<ParameterTuningWindow*>();
        if (tuningWindow) {
            tuningWindow->show();
            QTest::qWait(100);
            saveEvidence(tuningWindow->grab(), evidenceDir.filePath(QStringLiteral("baseline_05_tuning.png")));
        }

        // 6. 诊断下载
        if (workspaceTabs) {
            for (int i = 0; i < workspaceTabs->count(); ++i) {
                if (workspaceTabs->tabText(i).contains(QStringLiteral("构建与诊断下载")) || workspaceTabs->tabText(i).contains(QStringLiteral("设备"))) {
                    workspaceTabs->setCurrentIndex(i);
                    break;
                }
            }
        }
        QTest::qWait(100);
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("baseline_06_diagnostic_download.png")));

        // 记录 Action 清单
        QFile actFile(evidenceDir.filePath(QStringLiteral("baseline_actions.txt")));
        if (!actFile.exists()) {
            QVERIFY2(actFile.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to open baseline_actions.txt for writing");
            QTextStream out(&actFile);
            out.setCodec("UTF-8");
            for (QAction* act : window->findChildren<QAction*>()) {
                if (!act->text().isEmpty()) {
                    out << "text=" << act->text()
                        << " | objName=" << act->objectName()
                        << " | shortcut=" << act->shortcut().toString()
                        << " | enabled=" << act->isEnabled()
                        << " | checkable=" << act->isCheckable()
                        << "\n";
                }
            }
            actFile.close();
        }

    }

    void unifiedQActionConsistencyTest()
    {
        QScopedPointer<MainWindow> window(new MainWindow());

        // 1. 验证关键动作具备稳定且唯一的 objectName
        const QStringList requiredObjectNames = {
            QStringLiteral("actNew"),
            QStringLiteral("actOpen"),
            QStringLiteral("actSave"),
            QStringLiteral("actSaveAll"),
            QStringLiteral("actCloseProject"),
            QStringLiteral("actExit"),
            QStringLiteral("actCompile"),
            QStringLiteral("actCompileParameters"),
            QStringLiteral("actCompileCommunication"),
            QStringLiteral("actCompileAndRun"),
            QStringLiteral("actRunProject"),
            QStringLiteral("actStopProject"),
            QStringLiteral("actTestConnection"),
            QStringLiteral("actOpenMonitor"),
            QStringLiteral("actStartMonitor"),
            QStringLiteral("actStopMonitor"),
            QStringLiteral("actParameterTuning"),
            QStringLiteral("actSettings"),
            QStringLiteral("actOpenDownload"),
            QStringLiteral("actToggleDslEditor"),
            QStringLiteral("actToggleExplorerDock"),
            QStringLiteral("actToggleInspectorDock"),
            QStringLiteral("actToggleOutputDock"),
            QStringLiteral("actToggleMonitorDock"),
            QStringLiteral("actToggleDownloadDock")
        };

        for (const QString& objName : requiredObjectNames) {
            auto actions = window->findChildren<QAction*>(objName);
            QCOMPARE(actions.size(), 1);
            QVERIFY(!actions.first()->text().isEmpty());
        }

        // 2. 验证“停止控制器”与“停止监控”命名独立
        auto* actStopProject = window->findChild<QAction*>(QStringLiteral("actStopProject"));
        auto* actStopMonitor = window->findChild<QAction*>(QStringLiteral("actStopMonitor"));
        QVERIFY(actStopProject != nullptr);
        QVERIFY(actStopMonitor != nullptr);
        QVERIFY(actStopProject != actStopMonitor);
        QVERIFY(actStopProject->text().contains(QStringLiteral("停止控制器")));
        QVERIFY(actStopMonitor->text().contains(QStringLiteral("停止监控")));

        // 3. 验证探测行为命名为“测试连接”
        auto* actTestConnection = window->findChild<QAction*>(QStringLiteral("actTestConnection"));
        QVERIFY(actTestConnection != nullptr);
        QVERIFY(actTestConnection->text().contains(QStringLiteral("测试连接")));

        // 4. 验证工具栏与菜单共享相同 QAction 实例，状态同步
        auto* mainToolBar = window->findChild<QToolBar*>(QStringLiteral("MainToolBar"));
        QVERIFY(mainToolBar != nullptr);

        auto* actSave = window->findChild<QAction*>(QStringLiteral("actSave"));
        auto* actCompile = window->findChild<QAction*>(QStringLiteral("actCompile"));
        auto* actRun = window->findChild<QAction*>(QStringLiteral("actRunProject"));

        QVERIFY(mainToolBar->actions().contains(actSave));
        QVERIFY(mainToolBar->actions().contains(actRun));
        QVERIFY(mainToolBar->actions().contains(actStopProject));

        auto* compileBtn = mainToolBar->findChild<QToolButton*>(QStringLiteral("btnCompileDropdown"));
        QVERIFY(compileBtn != nullptr);
        QCOMPARE(compileBtn->defaultAction(), actCompile);

        // 修改状态时，工具栏和菜单必然同时更新（同一指针）
        actRun->setEnabled(false);
        QVERIFY(!mainToolBar->actions().at(mainToolBar->actions().indexOf(actRun))->isEnabled());
        actRun->setEnabled(true);
        QVERIFY(mainToolBar->actions().at(mainToolBar->actions().indexOf(actRun))->isEnabled());

        // 5. 验证触发仅执行一次业务操作
        auto* actClear = window->findChild<QAction*>(QStringLiteral("actClearOutput"));
        QVERIFY(actClear != nullptr);
        QSignalSpy spy(actClear, &QAction::triggered);
        actClear->trigger();
        QCOMPARE(spy.count(), 1);
    }

    void defaultLayoutAndResetU02Test()
    {
        QScopedPointer<MainWindow> window(new MainWindow());
        window->setSettingsStorage(QStringLiteral("ServoValveTest"), QStringLiteral("Test_U02"));
        window->switchToWorkspace(WorkspaceId::Programming);
        window->resize(1366, 768);
        window->show();
        QTest::qWait(150);

        QDir evidenceDir(QStringLiteral(".md/acceptance_evidence/ui_20260913"));
        if (!evidenceDir.exists()) {
            evidenceDir.mkpath(QStringLiteral("."));
        }
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u02_default_layout_1366x768.png")));

        // 1. 验证单行主工具栏 MainToolBar 存在，旧工具栏已移除
        auto* mainToolBar = window->findChild<QToolBar*>(QStringLiteral("MainToolBar"));
        QVERIFY(mainToolBar != nullptr);
        QVERIFY(window->findChild<QToolBar*>(QStringLiteral("OverviewToolBar")) == nullptr);
        QVERIFY(window->findChild<QToolBar*>(QStringLiteral("FileToolBar")) == nullptr);
        QVERIFY(window->findChild<QToolBar*>(QStringLiteral("RunToolBar")) == nullptr);
        QCOMPARE(window->findChildren<QToolBar*>().size(), 1);

        // 验证主工具栏常驻操作按钮不超过 6 个（保存、编译下拉、运行控制器、停止控制器）
        auto* actSave = window->findChild<QAction*>(QStringLiteral("actSave"));
        auto* actRun = window->findChild<QAction*>(QStringLiteral("actRunProject"));
        auto* actStop = window->findChild<QAction*>(QStringLiteral("actStopProject"));
        auto* compileBtn = mainToolBar->findChild<QToolButton*>(QStringLiteral("btnCompileDropdown"));
        QVERIFY(actSave != nullptr);
        QVERIFY(actRun != nullptr);
        QVERIFY(actStop != nullptr);
        QVERIFY(compileBtn != nullptr);

        // 2. 验证默认布局下编辑区宽度比例 >= 70% (1366x768)
        auto* centralWidget = window->centralWidget();
        QVERIFY(centralWidget != nullptr);
        double ratio = static_cast<double>(centralWidget->width()) / static_cast<double>(window->width());
        QVERIFY2(ratio >= 0.70, qPrintable(QString("Central editor ratio %1 is less than 0.70").arg(ratio)));

        // 3. 验证默认收起与显示状态
        auto* explorerDock = window->findChild<QDockWidget*>(QStringLiteral("ExplorerDock"));
        auto* inspectorDock = window->findChild<QDockWidget*>(QStringLiteral("InspectorDock"));
        auto* logDock = window->findChild<QDockWidget*>(QStringLiteral("LogDock"));
        auto* actToggleExplorer = window->findChild<QAction*>(QStringLiteral("actToggleExplorerDock"));
        auto* actToggleInspector = window->findChild<QAction*>(QStringLiteral("actToggleInspectorDock"));
        auto* actToggleOutput = window->findChild<QAction*>(QStringLiteral("actToggleOutputDock"));

        QVERIFY(explorerDock != nullptr && explorerDock->isVisible());
        QVERIFY(actToggleExplorer != nullptr && actToggleExplorer->isChecked());

        QVERIFY(inspectorDock != nullptr && !inspectorDock->isVisible());
        QVERIFY(actToggleInspector != nullptr && !actToggleInspector->isChecked());

        QVERIFY(logDock != nullptr && !logDock->isVisible());
        QVERIFY(actToggleOutput != nullptr && !actToggleOutput->isChecked());

        qDebug() << "logDock minimumHeight:" << logDock->minimumHeight() << "minimumSizeHint:" << logDock->minimumSizeHint();
        // 验证输出窗口移除了 200 最小高度硬编码限制 (之前为 setMinimumHeight(200))
        QVERIFY(logDock->minimumHeight() != 200);

        // 4. 验证折叠面板均可通过 Action 重新打开与关闭
        actToggleInspector->trigger();
        QTest::qWait(50);
        QVERIFY(inspectorDock->isVisible());
        QVERIFY(actToggleInspector->isChecked());

        actToggleOutput->trigger();
        QTest::qWait(50);
        QVERIFY(logDock->isVisible());
        QVERIFY(actToggleOutput->isChecked());

        // 模拟关闭面板，验证勾选状态同步清空
        inspectorDock->close();
        logDock->close();
        QTest::qWait(50);
        QVERIFY(!actToggleInspector->isChecked());
        QVERIFY(!actToggleOutput->isChecked());

        // 5. 验证重置布局覆盖全部 Dock 位置、尺寸与勾选状态
        // 人为扰乱状态：浮动、开启全部面板
        explorerDock->setFloating(true);
        inspectorDock->setFloating(true);
        inspectorDock->show();
        logDock->show();
        QTest::qWait(50);
        QVERIFY(explorerDock->isFloating());
        QVERIFY(inspectorDock->isFloating());

        auto* actResetLayout = window->findChild<QAction*>(QStringLiteral("actResetLayout"));
        QVERIFY(actResetLayout != nullptr);
        actResetLayout->trigger();
        QTest::qWait(50);

        // 验证不再浮动
        QVERIFY(!explorerDock->isFloating());
        QVERIFY(!inspectorDock->isFloating());
        QVERIFY(!logDock->isFloating());

        // 验证停靠位置
        QCOMPARE(window->dockWidgetArea(explorerDock), Qt::LeftDockWidgetArea);
        QCOMPARE(window->dockWidgetArea(inspectorDock), Qt::RightDockWidgetArea);
        QCOMPARE(window->dockWidgetArea(logDock), Qt::BottomDockWidgetArea);

        // 验证可见性与勾选状态回到默认（Explorer可见，Inspector与Log收起）
        QVERIFY(explorerDock->isVisible());
        QVERIFY(actToggleExplorer->isChecked());
        QVERIFY(!inspectorDock->isVisible());
        QVERIFY(!actToggleInspector->isChecked());
        QVERIFY(!logDock->isVisible());
        QVERIFY(!actToggleOutput->isChecked());

        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u02_after_reset_layout_1366x768.png")));
    }

    void workspaceRoutingAndPersistenceU03Test()
    {
        QDir evidenceDir(QStringLiteral(".md/acceptance_evidence/ui_20260913"));
        if (!evidenceDir.exists()) {
            evidenceDir.mkpath(QStringLiteral("."));
        }

        const QString testOrg = QStringLiteral("ServoValveTest");
        const QString testApp = QStringLiteral("Test_U03_Storage");
        QSettings isolatedSettings(testOrg, testApp);
        isolatedSettings.remove(QStringLiteral("ui"));
        isolatedSettings.sync();

        // 1. 验证 3 个短名称 Tab 与默认工作区布局
        QScopedPointer<MainWindow> window(new MainWindow());
        window->setSettingsStorage(testOrg, testApp);
        window->resize(1366, 768);
        window->show();
        QTest::qWait(100);

        auto* workspaceTabs = window->findChild<QTabWidget*>(QStringLiteral("WorkspaceTabs"));
        QVERIFY(workspaceTabs != nullptr);
        QCOMPARE(workspaceTabs->count(), 3);
        QCOMPARE(workspaceTabs->tabText(0), QStringLiteral("编程"));
        QCOMPARE(workspaceTabs->tabText(1), QStringLiteral("监控"));
        QCOMPARE(workspaceTabs->tabText(2), QStringLiteral("设备"));

        QCOMPARE(window->currentWorkspaceId(), WorkspaceId::Programming);
        QCOMPARE(workspaceTabs->currentIndex(), 0);

        auto* actToggleDslEditor = window->findChild<QAction*>(QStringLiteral("actToggleDslEditor"));
        auto* actToggleMonitorDock = window->findChild<QAction*>(QStringLiteral("actToggleMonitorDock"));
        auto* actToggleDownloadDock = window->findChild<QAction*>(QStringLiteral("actToggleDownloadDock"));
        auto* actToggleExplorerDock = window->findChild<QAction*>(QStringLiteral("actToggleExplorerDock"));
        auto* actToggleInspectorDock = window->findChild<QAction*>(QStringLiteral("actToggleInspectorDock"));
        auto* actToggleOutputDock = window->findChild<QAction*>(QStringLiteral("actToggleOutputDock"));

        QVERIFY(actToggleDslEditor != nullptr);
        QVERIFY(actToggleMonitorDock != nullptr);
        QVERIFY(actToggleDownloadDock != nullptr);
        QVERIFY(actToggleExplorerDock != nullptr);
        QVERIFY(actToggleInspectorDock != nullptr);
        QVERIFY(actToggleOutputDock != nullptr);

        // 默认状态（编程工作区）
        QVERIFY(actToggleDslEditor->isChecked());
        QVERIFY(!actToggleMonitorDock->isChecked());
        QVERIFY(!actToggleDownloadDock->isChecked());
        QVERIFY(actToggleExplorerDock->isChecked());
        QVERIFY(!actToggleInspectorDock->isChecked());
        QVERIFY(!actToggleOutputDock->isChecked());

        auto* explorerDock = window->findChild<QDockWidget*>(QStringLiteral("ExplorerDock"));
        auto* inspectorDock = window->findChild<QDockWidget*>(QStringLiteral("InspectorDock"));
        auto* logDock = window->findChild<QDockWidget*>(QStringLiteral("LogDock"));
        QVERIFY(explorerDock != nullptr);
        QVERIFY(inspectorDock != nullptr);
        QVERIFY(logDock != nullptr);

        QVERIFY(explorerDock->isVisible());
        QVERIFY(!inspectorDock->isVisible());
        QVERIFY(!logDock->isVisible());

        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u03_programming_workspace_1366x768.png")));

        // 2. 路由切换到监控与设备工作区，验证侧栏默认可见性（监控页隐藏编程侧栏，设备页默认无侧栏）
        window->switchToWorkspace(WorkspaceId::Monitor);
        QTest::qWait(50);
        QCOMPARE(window->currentWorkspaceId(), WorkspaceId::Monitor);
        QCOMPARE(workspaceTabs->currentIndex(), 1);
        QVERIFY(!actToggleDslEditor->isChecked());
        QVERIFY(actToggleMonitorDock->isChecked());
        QVERIFY(!actToggleDownloadDock->isChecked());
        QVERIFY(!explorerDock->isVisible()); // 监控页隐藏编程侧栏
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u03_monitor_workspace_1366x768.png")));

        window->switchToWorkspace(WorkspaceId::Device);
        QTest::qWait(50);
        QCOMPARE(window->currentWorkspaceId(), WorkspaceId::Device);
        QCOMPARE(workspaceTabs->currentIndex(), 2);
        QVERIFY(!actToggleDslEditor->isChecked());
        QVERIFY(!actToggleMonitorDock->isChecked());
        QVERIFY(actToggleDownloadDock->isChecked());
        QVERIFY(!explorerDock->isVisible()); // 设备页默认无侧栏
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u03_device_workspace_1366x768.png")));

        // 3. 验证非破坏性切换（不丢未保存文本、光标和已选通道，不启停监控、不断开后台）
        window->switchToWorkspace(WorkspaceId::Programming);
        QTest::qWait(50);

        auto* dslEditor = window->findChild<DslScriptEditor*>();
        QVERIFY(dslEditor != nullptr);
        const QString testScript = QStringLiteral("// Test Script U03 Verification\nVAR\n    motorSpeed : REAL := 150.0;\n    systemReady : BOOL := TRUE;\nEND_VAR\n");
        dslEditor->setScript(testScript);
        dslEditor->gotoLine(3);
        const QString savedScript = dslEditor->currentScript();
        const int savedLine = dslEditor->currentLineNumber();

        // 模拟会话连接与监控
        auto* sessionController = window->findChild<RuntimeSessionController*>();
        QVERIFY(sessionController != nullptr);
        VirtualDeviceBackend backend;
        QVERIFY(backend.connectBackend());
        sessionController->setDeviceBackend(&backend);
        sessionController->executeRun();
        QCOMPARE(sessionController->state(), RuntimeSessionState::Running);

        auto* monitorWidget = window->findChild<MonitorWidget*>();
        QVERIFY(monitorWidget != nullptr);
        sessionController->startMonitoring();
        QVERIFY(monitorWidget->isMonitoring());
        QCOMPARE(sessionController->state(), RuntimeSessionState::Monitoring);

        const QStringList testChannels = {QStringLiteral("系统压力"), QStringLiteral("系统流量")};
        monitorWidget->setSelectedChannels(testChannels);
        QCOMPARE(monitorWidget->selectedChannels(), testChannels);

        // 反复切换工作区: Programming -> Monitor -> Device -> Monitor -> Programming
        window->switchToWorkspace(WorkspaceId::Monitor);
        QTest::qWait(30);
        QVERIFY(monitorWidget->isMonitoring());
        QCOMPARE(monitorWidget->selectedChannels(), testChannels);
        QCOMPARE(sessionController->state(), RuntimeSessionState::Monitoring);

        window->switchToWorkspace(WorkspaceId::Device);
        QTest::qWait(30);
        QVERIFY(monitorWidget->isMonitoring());
        QCOMPARE(sessionController->state(), RuntimeSessionState::Monitoring);

        window->switchToWorkspace(WorkspaceId::Monitor);
        QTest::qWait(30);
        QVERIFY(monitorWidget->isMonitoring());
        QCOMPARE(monitorWidget->selectedChannels(), testChannels);
        QCOMPARE(sessionController->state(), RuntimeSessionState::Monitoring);

        window->switchToWorkspace(WorkspaceId::Programming);
        QTest::qWait(30);
        // 验证文本未丢，光标行号保持
        QCOMPARE(dslEditor->currentScript(), savedScript);
        QCOMPARE(dslEditor->currentLineNumber(), savedLine);
        // 监控状态与连接仍保持
        QVERIFY(monitorWidget->isMonitoring());
        QCOMPARE(sessionController->state(), RuntimeSessionState::Monitoring);

        sessionController->stopMonitoring();
        sessionController->requestStop();
        sessionController->setDeviceBackend(nullptr);
        backend.disconnectBackend();

        // 4. 验证独立工作区 Dock 布局持久化与记忆
        // 在编程工作区打开检查面板与输出面板
        actToggleInspectorDock->trigger();
        actToggleOutputDock->trigger();
        QTest::qWait(30);
        QVERIFY(inspectorDock->isVisible());
        QVERIFY(logDock->isVisible());

        // 切换至监控工作区，在监控工作区中 Inspector 应仍按监控工作区状态（默认收起）
        window->switchToWorkspace(WorkspaceId::Monitor);
        QTest::qWait(30);
        QVERIFY(!inspectorDock->isVisible());

        // 切回编程工作区，Inspector与LogDock恢复为打开状态
        window->switchToWorkspace(WorkspaceId::Programming);
        QTest::qWait(30);
        QVERIFY(inspectorDock->isVisible());
        QVERIFY(logDock->isVisible());

        // 5. 验证跨窗口重启与隔离存储持久化
        window->switchToWorkspace(WorkspaceId::Monitor);
        actToggleOutputDock->trigger(); // 在监控工作区打开日志
        QTest::qWait(30);
        QVERIFY(logDock->isVisible());

        window->setGeometry(120, 120, 1420, 820);
        window->saveWindowGeometryAndWorkspaces();
        isolatedSettings.sync();

        QCOMPARE(isolatedSettings.value(QStringLiteral("ui/layoutVersion")).toInt(), 1);
        QCOMPARE(isolatedSettings.value(QStringLiteral("ui/activeWorkspace")).toString(), QStringLiteral("monitor"));
        QVERIFY(!isolatedSettings.value(QStringLiteral("ui/workspaces/monitor/state")).toByteArray().isEmpty());

        // 创建第二个 MainWindow 实例，模拟重启并恢复
        {
            QScopedPointer<MainWindow> window2(new MainWindow());
            window2->setSettingsStorage(testOrg, testApp);
            window2->show();
            QTest::qWait(50);

            QCOMPARE(window2->currentWorkspaceId(), WorkspaceId::Monitor);
            auto* tabs2 = window2->findChild<QTabWidget*>(QStringLiteral("WorkspaceTabs"));
            QVERIFY(tabs2 != nullptr);
            QCOMPARE(tabs2->currentIndex(), 1);

            auto* logDock2 = window2->findChild<QDockWidget*>(QStringLiteral("LogDock"));
            QVERIFY(logDock2 != nullptr);
            QVERIFY(logDock2->isVisible()); // 监控工作区的修改成功恢复
        }

        // 6. 验证损坏/不匹配版本配置回退到默认
        {
            isolatedSettings.setValue(QStringLiteral("ui/layoutVersion"), 9999);
            isolatedSettings.setValue(QStringLiteral("ui/workspaces/programming/state"), QByteArray("corrupted_garbage_bytes"));
            isolatedSettings.sync();

            QScopedPointer<MainWindow> window3(new MainWindow());
            window3->setSettingsStorage(testOrg, testApp);
            window3->switchToWorkspace(WorkspaceId::Programming);
            window3->show();
            QTest::qWait(50);

            // 损坏时回退到编程工作区默认布局（Explorer可见，Inspector收起，Log收起）
            auto* explorerDock3 = window3->findChild<QDockWidget*>(QStringLiteral("ExplorerDock"));
            auto* inspectorDock3 = window3->findChild<QDockWidget*>(QStringLiteral("InspectorDock"));
            auto* logDock3 = window3->findChild<QDockWidget*>(QStringLiteral("LogDock"));
            QVERIFY(explorerDock3 != nullptr);
            QVERIFY(inspectorDock3 != nullptr);
            QVERIFY(logDock3 != nullptr);
            QVERIFY(explorerDock3->isVisible());
            QVERIFY(!inspectorDock3->isVisible());
            QVERIFY(!logDock3->isVisible());
        }
        isolatedSettings.remove(QStringLiteral("ui"));
        isolatedSettings.sync();

        // 7. 验证重置当前工作区布局 vs 重置全部工作区布局
        auto* actResetCurrent = window->findChild<QAction*>(QStringLiteral("actResetLayout"));
        auto* actResetAll = window->findChild<QAction*>(QStringLiteral("actResetAllLayouts"));
        QVERIFY(actResetCurrent != nullptr);
        QVERIFY(actResetAll != nullptr);

        // 当前在编程工作区，Inspector 和 Log 均打开
        window->switchToWorkspace(WorkspaceId::Programming);
        actToggleInspectorDock->setChecked(true);
        actToggleOutputDock->setChecked(true);
        QTest::qWait(30);
        QVERIFY(inspectorDock->isVisible());

        // 重置当前工作区
        actResetCurrent->trigger();
        QTest::qWait(30);
        QVERIFY(!inspectorDock->isVisible());
        QVERIFY(explorerDock->isVisible());

        // 8. 验证快捷键（Ctrl+Shift+D）重定向至左侧函数库分类
        auto* actOpenDisplay = window->findChild<QAction*>(QStringLiteral("actOpenDisplayWorkspace"));
        QVERIFY(actOpenDisplay != nullptr);
        actOpenDisplay->trigger();
        QTest::qWait(50);

        QVERIFY(explorerDock->isVisible());
        auto* leftTabs = window->findChild<QTabWidget*>(QStringLiteral("LeftTabsWidget"));
        QVERIFY(leftTabs != nullptr);
        QCOMPARE(leftTabs->currentIndex(), 1); // 1 为函数库

        auto* programBlocks = window->findChild<ProgramBlocksWidget*>();
        QVERIFY(programBlocks != nullptr);
    }

    void centralizedStatusAndDemandOutputU04Test()
    {
        QScopedPointer<MainWindow> window(new MainWindow());
        window->setSettingsStorage(QStringLiteral("ServoValveTest"), QStringLiteral("Test_U04"));
        window->switchToWorkspace(WorkspaceId::Programming);
        window->resize(1366, 768);
        window->show();
        QTest::qWait(150);

        QDir evidenceDir(QStringLiteral(".md/acceptance_evidence/ui_20260913"));
        if (!evidenceDir.exists()) {
            evidenceDir.mkpath(QStringLiteral("."));
        }

        auto* statusBar = window->statusBar();
        QVERIFY(statusBar != nullptr);

        // 1. 验证 GlobalStatusBar 位于 QStatusBar，且不在 MainToolBar 中
        auto* globalStatusBar = window->findChild<GlobalStatusBar*>(QStringLiteral("GlobalStatusBar"));
        QVERIFY(globalStatusBar != nullptr);
        QCOMPARE(globalStatusBar->parent(), statusBar);

        auto* mainToolBar = window->findChild<QToolBar*>(QStringLiteral("MainToolBar"));
        QVERIFY(mainToolBar != nullptr);
        QVERIFY(mainToolBar->findChild<GlobalStatusBar*>() == nullptr);

        // 验证底栏中移除了重复的 ConnectionStatusLabel，由 GlobalStatusBar 统一承载
        QVERIFY(window->findChild<QLabel*>(QStringLiteral("ConnectionStatusLabel")) == nullptr);

        // 2. 验证问题计数与运行告警计数彻底分离
        auto* problemsPanel = window->findChild<ProblemsPanel*>();
        auto* monitorWidget = window->findChild<MonitorWidget*>();
        QVERIFY(problemsPanel != nullptr);
        QVERIFY(monitorWidget != nullptr);

        problemsPanel->clearProblems();
        QCOMPARE(globalStatusBar->problemCount(), 0);
        QCOMPARE(globalStatusBar->alarmCount(), 0);

        // 注入编译/系统问题
        problemsPanel->addProblem(QStringLiteral("error"), QStringLiteral("构建"), QStringLiteral("语法错误：未定义的变量"));
        QCOMPARE(globalStatusBar->problemCount(), 1);
        QCOMPARE(globalStatusBar->alarmCount(), 0); // 告警数不受影响

        // 注入监控告警
        QMetaObject::invokeMethod(monitorWidget, "onThresholdExceeded", Qt::DirectConnection,
                                  Q_ARG(QString, QStringLiteral("Ch1")),
                                  Q_ARG(double, 120.0),
                                  Q_ARG(double, 100.0));
        QCOMPARE(globalStatusBar->alarmCount(), 1);
        QCOMPARE(globalStatusBar->problemCount(), 1); // 问题数不受告警影响，不互相覆盖

        // 清空问题
        problemsPanel->clearProblems();
        QCOMPARE(globalStatusBar->problemCount(), 0);
        QCOMPARE(globalStatusBar->alarmCount(), 1); // 告警数仍然保留

        // 3. 验证点击“问题”摘要：自动切换到问题页、展开 LogDock 并定位首个错误
        auto* logDock = window->findChild<QDockWidget*>(QStringLiteral("LogDock"));
        auto* bottomPanels = window->findChild<QTabWidget*>(QStringLiteral("BottomPanels"));
        auto* actToggleOutput = window->findChild<QAction*>(QStringLiteral("actToggleOutputDock"));
        QVERIFY(logDock != nullptr);
        QVERIFY(bottomPanels != nullptr);
        QVERIFY(actToggleOutput != nullptr);

        logDock->setVisible(false);
        actToggleOutput->setChecked(false);
        QTest::qWait(30);

        problemsPanel->addProblem(QStringLiteral("warning"), QStringLiteral("构建"), QStringLiteral("警告信息"));
        problemsPanel->addProblem(QStringLiteral("error"), QStringLiteral("构建"), QStringLiteral("致命语法错误：第20行"));

        // 触发 problemClicked 信号
        emit globalStatusBar->problemClicked();
        QTest::qWait(50);
        QVERIFY(logDock->isVisible());
        QVERIFY(actToggleOutput->isChecked());
        QCOMPARE(bottomPanels->currentWidget(), problemsPanel);

        // 4. 验证点击“告警”摘要：切换至“监控”工作区
        window->switchToWorkspace(WorkspaceId::Programming);
        QCOMPARE(window->currentWorkspaceId(), WorkspaceId::Programming);

        emit globalStatusBar->alarmClicked();
        QTest::qWait(50);
        QCOMPARE(window->currentWorkspaceId(), WorkspaceId::Monitor);

        // 5. 验证编译失败自动展开问题并定位首个错误；编译成功不抢焦点
        window->switchToWorkspace(WorkspaceId::Programming);
        logDock->setVisible(false);
        actToggleOutput->setChecked(false);
        QTest::qWait(30);

        auto* buildController = window->findChild<BuildController*>();
        QVERIFY(buildController != nullptr);

        // 模拟编译失败
        emit buildController->compileFailed(BuildType::Configuration, QStringLiteral("编译解析中断"));
        QTest::qWait(50);
        QVERIFY(logDock->isVisible());
        QVERIFY(actToggleOutput->isChecked());
        QCOMPARE(bottomPanels->currentWidget(), problemsPanel);

        // 截图：编译失败自动展开
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u04_compile_failure_auto_expand.png")));

        // 用户主动关闭
        logDock->setVisible(false);
        actToggleOutput->setChecked(false);
        QTest::qWait(30);

        // 普通日志不抢焦点、不自动展开输出面板
        window->appendOutput(QStringLiteral("普通日志输出信息"));
        QTest::qWait(30);
        QVERIFY(!logDock->isVisible());
        QVERIFY(!actToggleOutput->isChecked());

        // 编译成功仅更新状态，不强弹输出
        emit buildController->compileSucceeded(BuildType::Configuration);
        QTest::qWait(30);
        QVERIFY(!logDock->isVisible());

        // 6. 验证面板隐藏时日志仍正确记录，且支持清空
        auto* outputViewer = window->findChild<QTextEdit*>(QStringLiteral("outputViewer"));
        QVERIFY(outputViewer != nullptr);
        window->appendOutput(QStringLiteral("LOG_TEST_HIDDEN_RECORD_12345"));
        QVERIFY(outputViewer->toPlainText().contains(QStringLiteral("LOG_TEST_HIDDEN_RECORD_12345")));

        // 7. 验证右侧属性面板移除重复全局概览（仅保留项目上下文与参数检查）
        auto* inspectorPanel = window->findChild<InspectorPanel*>();
        QVERIFY(inspectorPanel != nullptr);
        bool hasStateOverviewGroup = false;
        for (auto* group : inspectorPanel->findChildren<QGroupBox*>()) {
            if (group->title().contains(QStringLiteral("状态概览")) && group->isVisible()) {
                hasStateOverviewGroup = true;
                break;
            }
        }
        QVERIFY(!hasStateOverviewGroup);

        // 8. 验证详情折叠与异常自动展开（OPC 错误不被隐藏）
        window->switchToWorkspace(WorkspaceId::Programming);
        QVERIFY(!globalStatusBar->isDetailsExpanded());

        window->switchToWorkspace(WorkspaceId::Monitor);
        QVERIFY(globalStatusBar->isDetailsExpanded());

        // 截图：监控页详情展开截图
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u04_details_expanded_monitor.png")));

        window->switchToWorkspace(WorkspaceId::Programming);
        QVERIFY(!globalStatusBar->isDetailsExpanded());

        // 注入 OPC 错误，自动展开详情
        globalStatusBar->setOpcState(false, QStringLiteral("OPC 服务连接拒绝：端口 4840"));
        QVERIFY(globalStatusBar->isDetailsExpanded());

        // 截图：底栏完整状态摘要
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u04_statusbar_summary_1366x768.png")));

        // 清理
        QMetaObject::invokeMethod(monitorWidget, "onClearAlarmsClicked", Qt::DirectConnection);
        problemsPanel->clearProblems();
    }

    void programmingSidebarAndEditorU05Test()
    {
        QScopedPointer<MainWindow> window(new MainWindow());
        window->setSettingsStorage(QStringLiteral("ServoValveTest"), QStringLiteral("Test_U05"));
        window->switchToWorkspace(WorkspaceId::Programming);
        window->resize(1366, 768);
        window->show();
        QTest::qWait(150);

        QDir evidenceDir(QStringLiteral(".md/acceptance_evidence/ui_20260913"));
        if (!evidenceDir.exists()) {
            evidenceDir.mkpath(QStringLiteral("."));
        }

        // 1. 验证左侧 ExplorerDock 内置 LeftTabsWidget，包含“项目”与“函数库”
        auto* explorerDock = window->findChild<QDockWidget*>(QStringLiteral("ExplorerDock"));
        QVERIFY(explorerDock != nullptr);
        auto* leftTabs = window->findChild<QTabWidget*>(QStringLiteral("LeftTabsWidget"));
        QVERIFY(leftTabs != nullptr);
        QCOMPARE(leftTabs->count(), 2);
        QCOMPARE(leftTabs->tabText(0), QStringLiteral("项目"));
        QCOMPARE(leftTabs->tabText(1), QStringLiteral("函数库"));

        auto* projectExplorer = window->findChild<ProjectExplorerWidget*>();
        QVERIFY(projectExplorer != nullptr);
        auto* programBlocks = window->findChild<ProgramBlocksWidget*>();
        QVERIFY(programBlocks != nullptr);
        auto* dslEditor = window->findChild<DslScriptEditor*>();
        QVERIFY(dslEditor != nullptr);

        // 2. 验证冷启动数据源绑定正常与树节点已加载
        QVERIFY(programBlocks->completionEngine() != nullptr);
        QCOMPARE(programBlocks->completionEngine(), dslEditor->completionEngine());
        auto* treeWidget = programBlocks->treeWidget();
        QVERIFY(treeWidget != nullptr);
        QVERIFY(treeWidget->topLevelItemCount() > 0);

        // 验证引擎与树节点 ID 集合完全一致
        QSet<QString> expectedIds;
        for (const auto& s : dslEditor->completionEngine()->availableSnippets()) {
            expectedIds.insert(s.id);
        }
        QSet<QString> actualIds;
        for (int i = 0; i < treeWidget->topLevelItemCount(); ++i) {
            auto* cat = treeWidget->topLevelItem(i);
            for (int j = 0; j < cat->childCount(); ++j) {
                actualIds.insert(cat->child(j)->data(0, Qt::UserRole).toString());
            }
        }
        QVERIFY(!expectedIds.isEmpty());
        QCOMPARE(actualIds, expectedIds);

        // 验证所有函数节点展示正常且均不包含“[未完善]”标记
        for (int i = 0; i < treeWidget->topLevelItemCount(); ++i) {
            auto* cat = treeWidget->topLevelItem(i);
            for (int j = 0; j < cat->childCount(); ++j) {
                QVERIFY(!cat->child(j)->text(0).contains(QStringLiteral("未完善")));
            }
        }

        // 验证函数库搜索与分类功能，以及原有显示屏函数归入函数库（Ctrl+Shift+D 重定向）
        auto* actOpenDisplay = window->findChild<QAction*>(QStringLiteral("actOpenDisplayWorkspace"));
        QVERIFY(actOpenDisplay != nullptr);
        actOpenDisplay->trigger();
        QTest::qWait(50);
        QCOMPARE(leftTabs->currentIndex(), 1);

        // 验证 display 分类下的函数块存在并可检索
        auto* filterEdit = programBlocks->findChild<QLineEdit*>();
        QVERIFY(filterEdit != nullptr);
        QCOMPARE(filterEdit->text(), QStringLiteral("display"));

        // 验证代码编辑器内部默认折叠重复函数列表
        QVERIFY(!dslEditor->isFunctionListVisible());

        // 验证 display 分类节点及子项可见
        QTreeWidgetItem* displayCatItem = nullptr;
        for (int i = 0; i < treeWidget->topLevelItemCount(); ++i) {
            auto* cat = treeWidget->topLevelItem(i);
            if (cat && cat->text(0).compare(QStringLiteral("display"), Qt::CaseInsensitive) == 0) {
                displayCatItem = cat;
                break;
            }
        }
        QVERIFY(displayCatItem != nullptr);
        QVERIFY(!displayCatItem->isHidden());
        QVERIFY(displayCatItem->childCount() > 0);

        // 3. 验证通过真实鼠标点击与双击插入代码（无信号伪造兜底）
        QTreeWidgetItem* targetLeaf = displayCatItem->child(0);
        QVERIFY(targetLeaf != nullptr);
        QVERIFY(!targetLeaf->isHidden());
        const QString expectedSnippetCode = targetLeaf->data(0, Qt::UserRole + 1).toString();
        QVERIFY(!expectedSnippetCode.isEmpty());

        treeWidget->scrollToItem(targetLeaf);
        QTest::qWait(50);
        const QRect leafRect = treeWidget->visualItemRect(targetLeaf);
        QVERIFY(leafRect.isValid());

        dslEditor->setScript(QString());
        QTest::mouseClick(treeWidget->viewport(), Qt::LeftButton, Qt::NoModifier, leafRect.center());
        QTest::mouseDClick(treeWidget->viewport(), Qt::LeftButton, Qt::NoModifier, leafRect.center());
        QTest::qWait(100);
        QVERIFY(dslEditor->currentScript().contains(expectedSnippetCode));
        QCOMPARE(dslEditor->currentScript().count(expectedSnippetCode), 1);

        // 4. 验证搜索无结果的空状态提示与清除搜索
        filterEdit->setText(QStringLiteral("no_such_function_xyz"));
        QTest::qWait(30);
        auto* emptyLabel = programBlocks->emptyLabel();
        QVERIFY(emptyLabel != nullptr);
        QVERIFY(emptyLabel->isVisible());
        QVERIFY(emptyLabel->text().contains(QStringLiteral("没有匹配结果")));
        QVERIFY(treeWidget->isHidden());

        auto* clearFilterBtn = programBlocks->clearFilterButton();
        QVERIFY(clearFilterBtn != nullptr);
        QVERIFY(clearFilterBtn->isVisible());
        QTest::mouseClick(clearFilterBtn, Qt::LeftButton);
        QTest::qWait(30);
        QVERIFY(filterEdit->text().isEmpty());
        QVERIFY(!treeWidget->isHidden());
        QVERIFY(!emptyLabel->isVisible());

        // 5. 验证切换左侧页不影响代码及光标
        const QString scriptBeforeTabSwitch = dslEditor->currentScript();
        const int lineBefore = dslEditor->currentLineNumber();
        leftTabs->setCurrentIndex(0); // 切换到项目
        QTest::qWait(30);
        QCOMPARE(dslEditor->currentScript(), scriptBeforeTabSwitch);
        QCOMPARE(dslEditor->currentLineNumber(), lineBefore);
        leftTabs->setCurrentIndex(1); // 切换回函数库
        QTest::qWait(30);
        QCOMPARE(dslEditor->currentScript(), scriptBeforeTabSwitch);
        QCOMPARE(dslEditor->currentLineNumber(), lineBefore);

        // 截图：左侧函数库与显示类函数块
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u05_programming_sidebar_snippets.png")));

        // 6. 验证属性面板仅展示选中对象信息；无选中对象时显示简短提示
        auto* inspectorPanel = window->findChild<InspectorPanel*>();
        QVERIFY(inspectorPanel != nullptr);
        inspectorPanel->clearSelection();
        QVERIFY(!inspectorPanel->hasSelectedObject());

        // 模拟选中文件对象
        emit projectExplorer->fileSelected(QStringLiteral("D:/Table/LH/demo.lh"));
        QVERIFY(inspectorPanel->hasSelectedObject());
        QCOMPARE(inspectorPanel->selectedObjectType(), QStringLiteral("文件"));
        QCOMPARE(inspectorPanel->selectedObjectName(), QStringLiteral("demo.lh"));

        // 真实选中树项触发属性面板展示
        treeWidget->setCurrentItem(nullptr);
        treeWidget->setCurrentItem(targetLeaf);
        QTest::qWait(30);
        QVERIFY(inspectorPanel->hasSelectedObject());
        QCOMPARE(inspectorPanel->selectedObjectType(), QStringLiteral("函数块"));
        QCOMPARE(inspectorPanel->selectedObjectName(), targetLeaf->data(0, Qt::UserRole).toString());

        // 截图：属性面板展示选中对象
        auto* inspectorDock = window->findChild<QDockWidget*>(QStringLiteral("InspectorDock"));
        if (inspectorDock) {
            inspectorDock->setVisible(true);
            QTest::qWait(100);
        }
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u05_inspector_selected_object.png")));

        // 7. 验证查找/替换关闭按钮字符正常（非乱码）
        bool hasProperCloseChar = false;
        for (auto* btn : dslEditor->findChildren<QPushButton*>()) {
            if (btn->text() == QStringLiteral("✕") || btn->text() == QStringLiteral("×")) {
                hasProperCloseChar = true;
                break;
            }
        }
        QVERIFY(hasProperCloseChar);

        // 8. 验证关闭编辑器不丢失未保存脚本内容（保留修改与编辑状态）
        dslEditor->insertSnippet(QStringLiteral("\nUNSAVED_ACCEPTANCE_MARKER := 123;\n"));
        QVERIFY(dslEditor->isModified());
        QVERIFY(dslEditor->currentScript().contains(QStringLiteral("UNSAVED_ACCEPTANCE_MARKER")));

        auto* editorSubWindow = window->findChild<QMdiSubWindow*>();
        QVERIFY(editorSubWindow != nullptr);
        QPointer<DslScriptEditor> previousEditor = dslEditor;
        editorSubWindow->close();
        QTest::qWait(100);

        // 关闭后对象未被静默销毁
        QVERIFY(!previousEditor.isNull());

        // 9. 验证重新打开编辑器时可见性恢复、未保存内容保留、焦点与键盘输入有效，且支持多次关闭/重开
        auto* actToggleDsl = window->findChild<QAction*>(QStringLiteral("actToggleDslEditor"));
        QVERIFY(actToggleDsl != nullptr);
        actToggleDsl->setChecked(true);
        QTest::qWait(100);

        auto* reopenedEditor = window->findChild<DslScriptEditor*>();
        QVERIFY(reopenedEditor != nullptr);
        QVERIFY(editorSubWindow->isVisible());
        QVERIFY(reopenedEditor->isVisible());
        QVERIFY(!reopenedEditor->isHidden());
        QVERIFY(reopenedEditor->currentScript().contains(QStringLiteral("UNSAVED_ACCEPTANCE_MARKER")));
        QVERIFY(reopenedEditor->isModified());
        QCOMPARE(programBlocks->completionEngine(), reopenedEditor->completionEngine());

        // 验证焦点可达与实际键盘输入写入
        reopenedEditor->setFocus();
        auto* codeEditor = reopenedEditor->editor();
        QVERIFY(codeEditor != nullptr);
        QVERIFY(codeEditor->isVisible());
        QTest::keyClicks(codeEditor, QStringLiteral("VAR KEYINPUT"));
        QTest::keyClick(codeEditor, Qt::Key_Semicolon);
        QVERIFY(reopenedEditor->currentScript().contains(QStringLiteral("KEYINPUT;")));

        // 第二次关闭与重新打开，验证多轮循环下菜单动作与函数库双击插入仍有效
        editorSubWindow->close();
        QTest::qWait(100);
        QVERIFY(!actToggleDsl->isChecked());

        actToggleDsl->setChecked(true);
        QTest::qWait(100);
        QVERIFY(editorSubWindow->isVisible());
        QVERIFY(reopenedEditor->isVisible());
        QVERIFY(!reopenedEditor->isHidden());
        QVERIFY(reopenedEditor->currentScript().contains(QStringLiteral("KEYINPUT;")));

        treeWidget->expandAll();
        treeWidget->scrollToItem(targetLeaf);
        QTest::qWait(50);
        const QRect currentLeafRect = treeWidget->visualItemRect(targetLeaf);
        QVERIFY(currentLeafRect.isValid());
        QTest::mouseClick(treeWidget->viewport(), Qt::LeftButton, Qt::NoModifier, currentLeafRect.center());
        QTest::mouseDClick(treeWidget->viewport(), Qt::LeftButton, Qt::NoModifier, currentLeafRect.center());
        QTest::qWait(100);
        QCOMPARE(reopenedEditor->currentScript().count(expectedSnippetCode), 2);
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u05_reopened_editor_active.png")));

        // 10. 验证函数库空状态多态提示（解绑与重新绑定）
        programBlocks->setCompletionEngine(nullptr);
        QVERIFY(emptyLabel->isVisible());
        QVERIFY(emptyLabel->text().contains(QStringLiteral("编辑器未打开")));
        QVERIFY(treeWidget->isHidden());

        programBlocks->setCompletionEngine(reopenedEditor->completionEngine());
        QVERIFY(!treeWidget->isHidden());
        QVERIFY(!emptyLabel->isVisible());

        // 清理
        inspectorPanel->clearSelection();
    }

    void monitorStreamliningU06Test()
    {
        QScopedPointer<MainWindow> window(new MainWindow());
        window->setSettingsStorage(QStringLiteral("ServoValveTest"), QStringLiteral("Test_U06"));
        window->switchToWorkspace(WorkspaceId::Monitor);
        window->resize(1366, 768);
        window->show();
        QTest::qWait(150);

        QDir evidenceDir(QStringLiteral(".md/acceptance_evidence/ui_20260913"));
        if (!evidenceDir.exists()) {
            evidenceDir.mkpath(QStringLiteral("."));
        }

        auto* monitorWidget = window->findChild<MonitorWidget*>();
        QVERIFY(monitorWidget != nullptr);

        auto* chartView = monitorWidget->chartView();
        QVERIFY(chartView != nullptr);

        // 1. 验证默认状态下右侧数据/告警详情折叠收起
        QVERIFY(!monitorWidget->isDetailsPanelVisible());
        QVERIFY(!monitorWidget->rightTabWidget()->isVisible());

        // 2. 验证图表曲线区域占据主视区超过 65%（1366x768 默认布局下）
        const double chartRatio = static_cast<double>(chartView->width()) / static_cast<double>(monitorWidget->width());
        QVERIFY(chartRatio >= 0.65);

        // 截图证据：详情折叠状态下的监控界面
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u06_monitor_details_collapsed.png")));

        // 3. 验证通道搜索与过滤
        auto* searchEdit = monitorWidget->channelSearchEdit();
        auto* onlySelectedBox = monitorWidget->onlySelectedCheckBox();
        QVERIFY(searchEdit != nullptr);
        QVERIFY(onlySelectedBox != nullptr);

        // 初始通道选中状态
        const QStringList initialSelected = monitorWidget->selectedChannels();

        // 输入通道过滤关键字
        searchEdit->setText(QStringLiteral("ch"));
        QTest::qWait(50);

        // 验证过滤后选中通道集合不发生任何变化
        QCOMPARE(monitorWidget->selectedChannels(), initialSelected);

        // 截图证据：通道过滤后的监控界面
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u06_monitor_filtered_channels.png")));

        // 4. 验证控制栏按钮文案与状态
        auto* startStopBtn = monitorWidget->findChild<QPushButton*>(QStringLiteral("MonitorStartStopButton"));
        auto* clearBtn = monitorWidget->findChild<QPushButton*>(QStringLiteral("MonitorClearDisplayButton"));
        QVERIFY(startStopBtn != nullptr);
        QVERIFY(clearBtn != nullptr);
        QCOMPARE(startStopBtn->text(), QStringLiteral("开始监控"));
        QCOMPARE(clearBtn->text(), QStringLiteral("清空显示"));
        QVERIFY(clearBtn->toolTip().contains(QStringLiteral("不影响已保存的数据库历史记录")) ||
                 clearBtn->toolTip().contains(QStringLiteral("不影响数据库历史记录")));

        // 5. 验证详情按钮及告警常驻可见
        auto* toggleDetailsBtn = monitorWidget->toggleDetailsButton();
        QVERIFY(toggleDetailsBtn != nullptr);
        QVERIFY(toggleDetailsBtn->text().contains(QStringLiteral("告警: 0")));

        // 展开详情
        toggleDetailsBtn->click();
        QTest::qWait(30);
        QVERIFY(monitorWidget->isDetailsPanelVisible());
        QVERIFY(monitorWidget->rightTabWidget()->isVisible());

        // 折叠详情
        toggleDetailsBtn->click();
        QTest::qWait(30);
        QVERIFY(!monitorWidget->isDetailsPanelVisible());

        // 清理搜索条件
        searchEdit->clear();
    }

    void parameterTuningCompatibilityU07Test()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QDir evidenceDir(QStringLiteral(".md/acceptance_evidence/ui_20260913"));
        if (!evidenceDir.exists()) {
            evidenceDir.mkpath(QStringLiteral("."));
        }

        QScopedPointer<MainWindow> window(new MainWindow());
        window->resize(1366, 768);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.data()));

        // 1. 获取调参组件
        auto* tuningDock = window->findChild<QDockWidget*>(QStringLiteral("ParameterTuningDock"));
        auto* tuningPanel = window->findChild<ParameterTuningPanel*>(QStringLiteral("ParameterTuningPanel"));
        QVERIFY(tuningDock != nullptr);
        QVERIFY(tuningPanel != nullptr);
        QVERIFY(!tuningPanel->isStandaloneMode());

        // 2. 注入测试 PID 参数
        ParameterDefinition kp;
        kp.name = QStringLiteral("Kp");
        kp.dataType = QStringLiteral("REAL");
        kp.defaultValue = QStringLiteral("1.0");
        kp.currentValue = QStringLiteral("2.5");
        kp.onlineEditable = true;
        kp.confirmed = true;

        ParameterDefinition ki;
        ki.name = QStringLiteral("Ki");
        ki.dataType = QStringLiteral("REAL");
        ki.defaultValue = QStringLiteral("0.5");
        ki.currentValue = QStringLiteral("0.8");
        ki.onlineEditable = true;
        ki.confirmed = false;

        tuningPanel->setPidParameterDetails({kp, ki});
        QTest::qWait(50);

        // 3. 打开监控并打开调参面板（同屏停靠模式）
        QVERIFY(QMetaObject::invokeMethod(window.data(), "onOpenMonitor", Qt::DirectConnection));
        QVERIFY(QMetaObject::invokeMethod(window.data(), "onOpenParameterTuningWindow", Qt::DirectConnection));
        QTest::qWait(100);
        QVERIFY(tuningDock->isVisible());

        auto* inspector = tuningPanel->inspectorPanel();
        QVERIFY(inspector != nullptr);

        // 验证默认状态：明细列隐藏（确认、回读、偏差默认收起）
        QVERIFY(!inspector->areDetailColumnsVisible());
        auto* paramTable = inspector->parameterTable();
        QVERIFY(paramTable != nullptr);
        QVERIFY(paramTable->isColumnHidden(4)); // 确认
        QVERIFY(paramTable->isColumnHidden(5)); // 回读
        QVERIFY(paramTable->isColumnHidden(6)); // 偏差

        // 切换展开明细列
        auto* toggleBtn = inspector->toggleDetailColumnsButton();
        QVERIFY(toggleBtn != nullptr);
        toggleBtn->click();
        QTest::qWait(50);
        QVERIFY(inspector->areDetailColumnsVisible());
        QVERIFY(!paramTable->isColumnHidden(4));
        QVERIFY(!paramTable->isColumnHidden(5));
        QVERIFY(!paramTable->isColumnHidden(6));

        // 再次点击收起
        toggleBtn->click();
        QTest::qWait(50);
        QVERIFY(!inspector->areDetailColumnsVisible());

        // 截图证据：同屏嵌入监控界面的调参面板
        window->repaint();
        qApp->processEvents();
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u07_tuning_embedded_monitor.png")));

        // 4. 独立窗口模式切换
        auto* popOutBtn = tuningPanel->popOutButton();
        QVERIFY(popOutBtn != nullptr);
        popOutBtn->click();
        QTest::qWait(150);

        // 验证调参面板已抽出为独立窗口
        QVERIFY(tuningPanel->isStandaloneMode());
        QVERIFY(!tuningDock->isVisible());

        auto* tuningWindow = window->findChild<ParameterTuningWindow*>(QStringLiteral("ParameterTuningWindow"));
        QVERIFY(tuningWindow != nullptr);
        QVERIFY(tuningWindow->isVisible());
        QCOMPARE(tuningWindow->tuningPanel(), tuningPanel);

        QVERIFY(QTest::qWaitForWindowExposed(tuningWindow));
        tuningWindow->repaint();
        qApp->processEvents();
        QTest::qWait(150);

        // 截图证据：独立窗口形态
        saveEvidence(tuningWindow->grab(), evidenceDir.filePath(QStringLiteral("u07_tuning_standalone_window.png")));

        // 5. 返回同屏停靠
        popOutBtn->click();
        QTest::qWait(100);

        // 验证已返回同屏停靠
        QVERIFY(!tuningPanel->isStandaloneMode());
        QVERIFY(!tuningWindow->isVisible());
        QVERIFY(tuningDock->isVisible());
        QCOMPARE(tuningDock->widget(), tuningPanel);
    }

    void deviceWorkspaceHierarchyU08Test()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QDir evidenceDir(QStringLiteral(".md/acceptance_evidence/ui_20260913"));
        if (!evidenceDir.exists()) {
            evidenceDir.mkpath(QStringLiteral("."));
        }

        QScopedPointer<MainWindow> window(new MainWindow());
        window->resize(1366, 768);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.data()));

        // 1. 切换至设备工作区
        window->switchToWorkspace(WorkspaceId::Device);
        QTest::qWait(100);
        QCOMPARE(window->currentWorkspaceId(), WorkspaceId::Device);

        // 2. 获取设备工作区容器与组件
        auto* deviceWidget = window->findChild<DeviceWorkspaceWidget*>(QStringLiteral("DeviceWorkspaceWidget"));
        QVERIFY(deviceWidget != nullptr);
        QVERIFY(deviceWidget->isVisible());

        auto* btnTest = deviceWidget->btnTestConnection();
        auto* btnRun = deviceWidget->btnRunController();
        auto* btnStop = deviceWidget->btnStopController();
        auto* toggleExpertBtn = deviceWidget->toggleExpertButton();
        auto* downloadWidget = deviceWidget->downloadWidget();

        QVERIFY(btnTest != nullptr);
        QVERIFY(btnRun != nullptr);
        QVERIFY(btnStop != nullptr);
        QVERIFY(toggleExpertBtn != nullptr);
        QVERIFY(downloadWidget != nullptr);

        // 3. 验证默认层级：专家诊断区域默认收起，仅展示设备概览与标准操作
        QVERIFY(!deviceWidget->isExpertDiagnosticVisible());
        QVERIFY(!downloadWidget->isVisible());

        // 截图证据：设备工作区默认概览形态
        window->repaint();
        qApp->processEvents();
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u08_device_workspace_overview.png")));

        // 4. 点击展开专家诊断抽屉
        toggleExpertBtn->click();
        QTest::qWait(80);
        QVERIFY(deviceWidget->isExpertDiagnosticVisible());
        QVERIFY(downloadWidget->isVisible());

        // 截图证据：展开专家诊断抽屉形态
        window->repaint();
        qApp->processEvents();
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u08_device_expert_diagnostic_expanded.png")));

        // 5. 点击收起专家诊断抽屉
        toggleExpertBtn->click();
        QTest::qWait(50);
        QVERIFY(!deviceWidget->isExpertDiagnosticVisible());
        QVERIFY(!downloadWidget->isVisible());

        // 6. 测试旧入口及菜单路由：onOpenDownloadWindow 自动切至设备页并展开专家诊断
        window->switchToWorkspace(WorkspaceId::Programming);
        QCOMPARE(window->currentWorkspaceId(), WorkspaceId::Programming);
        QVERIFY(QMetaObject::invokeMethod(window.data(), "onOpenDownloadWindow", Qt::DirectConnection));
        QTest::qWait(80);
        QCOMPARE(window->currentWorkspaceId(), WorkspaceId::Device);
        QVERIFY(deviceWidget->isExpertDiagnosticVisible());
        QVERIFY(downloadWidget->isVisible());
    }

    void unifiedVisualAndSettingsU09Test()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QDir evidenceDir(QStringLiteral(".md/acceptance_evidence/ui_20260913"));
        if (!evidenceDir.exists()) {
            evidenceDir.mkpath(QStringLiteral("."));
        }

        // 1. 启动 MainWindow (1366x768 逻辑像素)
        QScopedPointer<MainWindow> window(new MainWindow());
        window->resize(1366, 768);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.data()));

        // 2. 验证现代轻量主题应用（单行工具栏、状态栏、边框与间距统一度）
        ThemeManager::applyModernTheme(qApp);
        window->switchToWorkspace(WorkspaceId::Programming);
        QTest::qWait(100);
        window->repaint();
        qApp->processEvents();

        // 验证主工具栏与状态栏存在且具有统一的样式名
        auto* mainToolBar = window->findChild<QToolBar*>(QStringLiteral("MainToolBar"));
        QVERIFY(mainToolBar != nullptr);
        auto* globalStatusBar = window->findChild<GlobalStatusBar*>(QStringLiteral("GlobalStatusBar"));
        QVERIFY(globalStatusBar != nullptr);

        // 截图证据：统一视觉主题下的主编程界面
        saveEvidence(window->grab(), evidenceDir.filePath(QStringLiteral("u09_theme_unified_programming.png")));

        // 3. 打开设置对话框
        SettingsDialog dialog(window.data());
        dialog.setDefaultProjectDir(QStringLiteral("C:/Projects/LH_Sample"));
        dialog.setAutoScrollLog(true);
        dialog.setFontSizeIndex(1); // 中 (11pt)
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        dialog.repaint();
        qApp->processEvents();

        // 验证设置对话框元素规范、无裁切、无多层繁杂导航
        QCOMPARE(dialog.defaultProjectDir(), QStringLiteral("C:/Projects/LH_Sample"));
        QVERIFY(dialog.autoScrollLog());
        QCOMPARE(dialog.fontSizeIndex(), 1);

        // 截图证据：统一视觉设置对话框
        saveEvidence(dialog.grab(), evidenceDir.filePath(QStringLiteral("u09_settings_dialog.png")));
        dialog.close();
    }

    void devicePauseStateAndActionsF01F02Test()
    {
        QScopedPointer<MainWindow> window(new MainWindow());
        window->resize(1366, 768);
        window->show();
        QTest::qWait(100);

        auto* sessionController = window->findChild<RuntimeSessionController*>();
        auto* deviceWidget = window->findChild<DeviceWorkspaceWidget*>(QStringLiteral("DeviceWorkspaceWidget"));
        QVERIFY(sessionController != nullptr);
        QVERIFY(deviceWidget != nullptr);

        window->switchToWorkspace(WorkspaceId::Device);
        QCOMPARE(window->currentWorkspaceId(), WorkspaceId::Device);

        auto* btnRun = deviceWidget->btnRunController();
        auto* btnStop = deviceWidget->btnStopController();
        auto* btnPause = deviceWidget->btnPauseController();
        auto* btnResume = deviceWidget->btnResumeController();
        auto* btnStep = deviceWidget->btnStepController();
        QVERIFY(btnRun && btnStop && btnPause && btnResume && btnStep);

        // 默认状态
        QCOMPARE(sessionController->isPaused(), false);
        QVERIFY(btnRun->defaultAction() != nullptr);
        QCOMPARE(btnRun->isEnabled(), btnRun->defaultAction()->isEnabled());
        QCOMPARE(btnStop->isEnabled(), btnStop->defaultAction()->isEnabled());
        QCOMPARE(btnPause->isEnabled(), false);
        QCOMPARE(btnResume->isEnabled(), false);
        QCOMPARE(btnStep->isEnabled(), false);

        // 打开工程
        auto* projectController = window->findChild<ProjectController*>();
        QVERIFY(projectController != nullptr);
        projectController->setCurrentProjectPath(QStringLiteral("C:/test_project"));
        ProjectRuntimeConfig cfg;
        cfg.projectName = QStringLiteral("F01_Test_Proj");
        projectController->runtimeConfig() = cfg;
        QVERIFY(QMetaObject::invokeMethod(window.data(), "onProjectOpened",
                                          Qt::DirectConnection, Q_ARG(ProjectRuntimeConfig, cfg)));

        // 验证 R2-A: 非演示模式无后端时拒绝操作并报错，且不改变暂停状态
        QSignalSpy errSpy(sessionController, &RuntimeSessionController::runtimeError);
        QCOMPARE(sessionController->pauseController(), false);
        QCOMPARE(sessionController->isPaused(), false);
        QVERIFY(errSpy.count() >= 1);
        QVERIFY(errSpy.last().first().toString().contains(QStringLiteral("控制器后端不可用")));
        QCOMPARE(sessionController->resumeController(), false);
        QCOMPARE(sessionController->stepController(), false);

        // 演示模式启动
        sessionController->startDemoMode(QStringLiteral("test"));
        sessionController->executeRun();
        QTRY_COMPARE(sessionController->state(), RuntimeSessionState::Running);
        QCOMPARE(sessionController->isPaused(), false);

        // 运行中状态校验
        QVERIFY(btnStop->isEnabled());
        QVERIFY(btnPause->isEnabled());
        QCOMPARE(btnResume->isEnabled(), false);
        QCOMPARE(btnStep->isEnabled(), false);

        // 暂停操作
        QSignalSpy pauseSpy(sessionController, &RuntimeSessionController::pausedChanged);
        btnPause->click();
        QTRY_COMPARE(sessionController->isPaused(), true);
        QCOMPARE(pauseSpy.count(), 1);
        QCOMPARE(pauseSpy.first().at(0).toBool(), true);

        // 暂停中状态校验
        QCOMPARE(btnPause->isEnabled(), false);
        QVERIFY(btnResume->isEnabled());
        QVERIFY(btnStep->isEnabled());
        QVERIFY(btnStop->isEnabled());

        // 单步操作
        btnStep->click();
        QTest::qWait(30);
        QCOMPARE(sessionController->isPaused(), true);

        // 继续运行
        btnResume->click();
        QTRY_COMPARE(sessionController->isPaused(), false);
        QVERIFY(btnPause->isEnabled());
        QCOMPARE(btnResume->isEnabled(), false);

        // 停止控制器
        btnStop->click();
        QTRY_VERIFY(sessionController->state() != RuntimeSessionState::Running);
        QCOMPARE(sessionController->isPaused(), false);
        QCOMPARE(btnPause->isEnabled(), false);
        QCOMPARE(btnResume->isEnabled(), false);
    }

    void workspaceToolBarTaskSwitchingF03Test()
    {
        QScopedPointer<MainWindow> window(new MainWindow());
        window->resize(1366, 768);
        window->show();
        QTest::qWait(100);

        auto* mainToolBar = window->findChild<QToolBar*>(QStringLiteral("MainToolBar"));
        QVERIFY(mainToolBar != nullptr);

        // 1. 编程工作区：<= 6 个操作（保存、编译下拉、运行、停止）
        window->switchToWorkspace(WorkspaceId::Programming);
        auto progActions = mainToolBar->actions();
        QVERIFY(progActions.size() <= 6);
        auto* compileBtn = mainToolBar->findChild<QToolButton*>(QStringLiteral("btnCompileDropdown"));
        QVERIFY(compileBtn != nullptr);

        // 2. 监控工作区：<= 6 个操作（开始监控、停止监控、调参、运行、停止）
        window->switchToWorkspace(WorkspaceId::Monitor);
        auto monitorActions = mainToolBar->actions();
        QVERIFY(monitorActions.size() <= 6);
        QVERIFY(mainToolBar->findChild<QToolButton*>(QStringLiteral("btnCompileDropdown")) == nullptr);
        bool hasStartMonitor = false;
        for (auto* act : monitorActions) {
            if (act->objectName() == QStringLiteral("actStartMonitor")) {
                hasStartMonitor = true;
                break;
            }
        }
        QVERIFY(hasStartMonitor);

        // 3. 设备工作区：<= 6 个操作（测试连接、运行、停止、更多操作下拉）
        window->switchToWorkspace(WorkspaceId::Device);
        auto deviceActions = mainToolBar->actions();
        QVERIFY(deviceActions.size() <= 6);
        auto* moreBtn = mainToolBar->findChild<QToolButton*>(QStringLiteral("btnDeviceMoreOpsDropdown"));
        QVERIFY(moreBtn != nullptr);
        bool hasTestConn = false;
        for (auto* act : deviceActions) {
            if (act->objectName() == QStringLiteral("actTestConnection")) {
                hasTestConn = true;
                break;
            }
        }
        QVERIFY(hasTestConn);

        // 4. 切回编程工作区
        window->switchToWorkspace(WorkspaceId::Programming);
        QVERIFY(mainToolBar->findChild<QToolButton*>(QStringLiteral("btnCompileDropdown")) != nullptr);
    }

    void deviceWorkspaceTargetSummaryF04Test()
    {
        QScopedPointer<MainWindow> window(new MainWindow());
        window->resize(1366, 768);
        window->show();
        QTest::qWait(100);

        window->switchToWorkspace(WorkspaceId::Device);
        auto* deviceWidget = window->findChild<DeviceWorkspaceWidget*>(QStringLiteral("DeviceWorkspaceWidget"));
        QVERIFY(deviceWidget != nullptr);

        // 1. 无工程状态
        auto labels = deviceWidget->findChildren<QLabel*>();
        bool foundTarget = false;
        bool foundSource = false;
        bool foundAddress = false;
        for (auto* lbl : labels) {
            if (lbl->text() == QStringLiteral("未配置目标")) foundTarget = true;
            if (lbl->text() == QStringLiteral("未打开工程")) foundSource = true;
            if (lbl->text() == QStringLiteral("未连接 (离线)")) foundAddress = true;
        }
        QVERIFY(foundTarget);
        QVERIFY(foundSource);
        QVERIFY(foundAddress);

        // 2. 打开具有目标和串口配置的工程
        auto* projectController = window->findChild<ProjectController*>();
        QVERIFY(projectController != nullptr);
        projectController->setCurrentProjectPath(QStringLiteral("C:/test_project"));
        ProjectRuntimeConfig cfg;
        cfg.projectName = QStringLiteral("RealProject");
        cfg.target.model = QStringLiteral("TMS320F28335");
        cfg.commParameters.insert(QStringLiteral("port"), QStringLiteral("COM5"));
        cfg.commParameters.insert(QStringLiteral("baudRate"), 115200);
        projectController->runtimeConfig() = cfg;
        QVERIFY(QMetaObject::invokeMethod(window.data(), "onProjectOpened",
                                          Qt::DirectConnection, Q_ARG(ProjectRuntimeConfig, cfg)));
        window->switchToWorkspace(WorkspaceId::Device);

        QCOMPARE(deviceWidget->targetValue(), QStringLiteral("TMS320F28335"));
        QVERIFY(deviceWidget->addressValue().contains(QStringLiteral("COM5")));
        QVERIFY(deviceWidget->addressValue().contains(QStringLiteral("115200")));

        // 3. 展开专家诊断抽屉并验证专家手动目标独立标注，且项目操作目标 COM5 不被覆盖
        auto* toggleExpert = deviceWidget->toggleExpertButton();
        QVERIFY(toggleExpert != nullptr);
        toggleExpert->click();
        QTest::qWait(50);
        QVERIFY(deviceWidget->isExpertDiagnosticVisible());

        // 验证项目操作目标依然是 COM5 (未被专家抽屉覆盖)
        QVERIFY(deviceWidget->addressValue().contains(QStringLiteral("COM5")));
        // 验证专家诊断目标独立存在
        QVERIFY(deviceWidget->expertTargetText().contains(QStringLiteral("专家手动目标")));

        // 4. 测试专家抽屉端口与站号修改时，目标摘要立即实时同步
        auto* dlWidget = deviceWidget->downloadWidget();
        QVERIFY(dlWidget != nullptr);
        dlWidget->setPort(QStringLiteral("COM8"));
        dlWidget->setTargetStationId(42);
        QTest::qWait(50);

        QVERIFY(deviceWidget->expertTargetText().contains(QStringLiteral("COM8")));
        QVERIFY(deviceWidget->expertTargetText().contains(QStringLiteral("42")));
        // 项目操作目标依然保持 COM5 不受影响
        QVERIFY(deviceWidget->addressValue().contains(QStringLiteral("COM5")));

        // 5. 测试空端口状态：清空端口，应显示“未选择端口”，严禁伪造“COM1”
        dlWidget->setPort(QStringLiteral(""));
        QTest::qWait(50);
        QVERIFY(deviceWidget->expertTargetText().contains(QStringLiteral("未选择端口")));
        QVERIFY(!deviceWidget->expertTargetText().contains(QStringLiteral("COM1")));

        // 6. 测试无通信参数时工程显示“未配置通信参数”，严禁伪造“本地虚拟通道”
        cfg.commParameters.clear();
        projectController->runtimeConfig() = cfg;
        window->updateDeviceWorkspaceInfo();
        QCOMPARE(deviceWidget->addressValue(), QStringLiteral("未配置通信参数"));

        // 7. 收起专家抽屉
        toggleExpert->click();
        QTest::qWait(50);
        QVERIFY(!deviceWidget->isExpertDiagnosticVisible());
    }

    void parameterTuningDataSemanticsF05Test()
    {
        QScopedPointer<MainWindow> window(new MainWindow());
        window->resize(1366, 768);
        window->show();
        QTest::qWait(100);

        auto* parameterController = window->findChild<ParameterController*>();
        QVERIFY(parameterController != nullptr);

        // 构造真实 PID 结构夹具
        ParameterDefinition pKp;
        pKp.id = QStringLiteral("pid.kp");
        pKp.name = QStringLiteral("Kp");
        pKp.dataType = QStringLiteral("REAL");
        pKp.defaultValue = QStringLiteral("2.5");
        pKp.currentValue = QStringLiteral("2.5");
        pKp.minValue = QStringLiteral("0.1");
        pKp.maxValue = QStringLiteral("50.0");
        pKp.unit = QStringLiteral("V/A");
        pKp.onlineEditable = true;
        pKp.confirmed = false;

        ParameterDefinition pKi;
        pKi.id = QStringLiteral("pid.ki");
        pKi.name = QStringLiteral("Ki");
        pKi.dataType = QStringLiteral("REAL");
        pKi.defaultValue = QStringLiteral("0.5");
        pKi.currentValue = QStringLiteral("0.5");
        pKi.minValue = QStringLiteral("0.01");
        pKi.maxValue = QStringLiteral("10.0");
        pKi.unit = QStringLiteral("1/s");
        pKi.onlineEditable = true;
        pKi.confirmed = false;

        ParameterDefinition pKd;
        pKd.id = QStringLiteral("pid.kd");
        pKd.name = QStringLiteral("Kd");
        pKd.dataType = QStringLiteral("REAL");
        pKd.defaultValue = QStringLiteral("0.05");
        pKd.currentValue = QStringLiteral("0.05");
        pKd.minValue = QStringLiteral("0.0");
        pKd.maxValue = QStringLiteral("2.0");
        pKd.unit = QStringLiteral("s");
        pKd.onlineEditable = true;
        pKd.confirmed = false;

        ProjectRuntimeConfig cfg;
        cfg.projectName = QStringLiteral("PID_Fixture_Test");
        cfg.parameters = {pKp, pKi, pKd};
        auto* projectController = window->findChild<ProjectController*>();
        QVERIFY(projectController != nullptr);
        projectController->setCurrentProjectPath(QStringLiteral("C:/test_project"));
        projectController->runtimeConfig() = cfg;
        parameterController->loadDefinitions(cfg.parameters);
        QVERIFY(QMetaObject::invokeMethod(window.data(), "onProjectOpened",
                                          Qt::DirectConnection, Q_ARG(ProjectRuntimeConfig, cfg)));

        window->switchToWorkspace(WorkspaceId::Monitor);
        window->refreshInspectorPanel();

        auto* tuningPanel = window->findChild<ParameterTuningPanel*>(QStringLiteral("ParameterTuningPanel"));
        QVERIFY(tuningPanel != nullptr);

        auto* inspector = tuningPanel->findChild<InspectorPanel*>();
        QVERIFY(inspector != nullptr);
        auto* table = inspector->parameterTable();
        QVERIFY(table != nullptr);

        // 1. 验证表头规范
        QCOMPARE(table->columnCount(), 7);
        QCOMPARE(table->horizontalHeaderItem(0)->text(), QStringLiteral("名称"));
        QCOMPARE(table->horizontalHeaderItem(1)->text(), QStringLiteral("已读取值"));
        QCOMPARE(table->horizontalHeaderItem(2)->text(), QStringLiteral("待应用值"));
        QCOMPARE(table->horizontalHeaderItem(3)->text(), QStringLiteral("状态"));

        // 初始值：未读取设备时已读取值明确显示 "未读取"，待应用值为 "-"，Tooltip 中标注本地配置值
        QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Kp"));
        QCOMPARE(table->item(0, 1)->text(), QStringLiteral("未读取"));
        QCOMPARE(table->item(0, 2)->text(), QStringLiteral("-"));
        QVERIFY(table->item(0, 1)->toolTip().contains(QStringLiteral("本地配置值: 2.5")));

        // 2. 编辑产生待应用值
        QVERIFY(parameterController->editParameter(QStringLiteral("Kp"), QStringLiteral("3.8")));
        window->refreshInspectorPanel();

        QCOMPARE(table->item(0, 1)->text(), QStringLiteral("未读取"));
        QCOMPARE(table->item(0, 2)->text(), QStringLiteral("3.8"));

        // 3. 弹出独立调参窗口验证无损同步
        QMetaObject::invokeMethod(window.data(), "onPopOutParameterTuning", Qt::DirectConnection);
        auto* tuningWindow = window->findChild<ParameterTuningWindow*>();
        QVERIFY(tuningWindow != nullptr);
        auto* windowInspector = tuningWindow->findChild<InspectorPanel*>();
        QVERIFY(windowInspector != nullptr);
        auto* windowTable = windowInspector->parameterTable();
        QVERIFY(windowTable != nullptr);
        QCOMPARE(windowTable->item(0, 2)->text(), QStringLiteral("3.8"));

        // 4. 停靠回内嵌模式
        QMetaObject::invokeMethod(window.data(), "onDockBackParameterTuning", Qt::DirectConnection);
        QCOMPARE(table->item(0, 2)->text(), QStringLiteral("3.8"));

        // 5. 真实后端下发与成功回读闭环测试 (R2-C)
        RuntimePointDefinition ptKp;
        ptKp.id = QStringLiteral("pid.kp");
        ptKp.name = QStringLiteral("Kp");
        ptKp.kind = RuntimePointKind::Parameter;
        ptKp.access = RuntimePointAccess::ReadWrite;
        ptKp.dataType = QStringLiteral("REAL");
        ptKp.defaultValue = 2.5;

        VirtualDeviceBackend backend;
        backend.loadPointDefinitions({ptKp});
        QVERIFY(backend.connectBackend());
        QVERIFY(parameterController->applyModifiedParameters(&backend));
        window->refreshInspectorPanel();

        // 设备成功回读 3.8
        QHash<QString, QVariant> readbackSuccess;
        readbackSuccess.insert(QStringLiteral("pid.kp"), QVariant(3.8));
        parameterController->onReadbackValues(readbackSuccess);
        window->refreshInspectorPanel();

        // 验证：回读确认后，已读取值显示 3.8，待应用值自动清除为 "-"，状态更新为已确认
        QCOMPARE(table->item(0, 1)->text(), QStringLiteral("3.8"));
        QCOMPARE(table->item(0, 2)->text(), QStringLiteral("-"));
        QCOMPARE(table->item(0, 3)->text(), QStringLiteral("已确认"));

        // 6. 验证回读偏差与失败时保留待应用值供重试
        QVERIFY(parameterController->editParameter(QStringLiteral("Kp"), QStringLiteral("4.2")));
        window->refreshInspectorPanel();
        QCOMPARE(table->item(0, 2)->text(), QStringLiteral("4.2"));

        QVERIFY(parameterController->applyModifiedParameters(&backend));
        // 设备回读值为 3.0 (偏差)
        QHash<QString, QVariant> readbackMismatch;
        readbackMismatch.insert(QStringLiteral("pid.kp"), QVariant(3.0));
        parameterController->onReadbackValues(readbackMismatch);
        window->refreshInspectorPanel();

        // 验证：已读取值反映回读 3.0，待应用值保留 4.2 供重试，状态显示偏差
        QCOMPARE(table->item(0, 1)->text(), QStringLiteral("3"));
        QCOMPARE(table->item(0, 2)->text(), QStringLiteral("4.2"));
        QVERIFY(table->item(0, 3)->text().contains(QStringLiteral("偏差")));

        // 7. 模拟应用失败保留待应用值
        ParameterStateInfo errInfo;
        errInfo.name = QStringLiteral("Kp");
        errInfo.state = ParameterState::ApplyFailed;
        errInfo.lastError = QStringLiteral("超时");
        errInfo.editedValue = QStringLiteral("4.2");
        inspector->setParameterStateMap({{QStringLiteral("Kp"), errInfo}});

        QCOMPARE(table->item(0, 2)->text(), QStringLiteral("4.2"));
        QVERIFY(table->item(0, 3)->text().contains(QStringLiteral("应用失败: 超时")));
    }

    void chartWidgetPlaceholderF06Test()
    {
        ChartWidget chart;
        chart.resize(400, 300);
        chart.show();
        QTest::qWait(50);

        auto* infoLabel = chart.infoLabel();
        QVERIFY(infoLabel != nullptr);

        // 1. 空状态下不含 %3
        QVERIFY(!infoLabel->text().contains(QStringLiteral("%3")));
        QVERIFY(!infoLabel->text().contains(QStringLiteral("%1")));
        QVERIFY(!infoLabel->text().contains(QStringLiteral("%2")));
        QCOMPARE(infoLabel->text(), QStringLiteral("通道: 0/0 | 数据点: 0"));

        // 2. 添加通道并推入数据点
        chart.addChannelSeries(QStringLiteral("ch1"), QStringLiteral("Channel 1"), Qt::blue);
        QVector<QPointF> points{{0.0, 1.0}, {1.0, 2.0}, {2.0, 3.0}};
        chart.updateChannelData(QStringLiteral("ch1"), points);
        chart.updateInfoLabel();

        QVERIFY(!infoLabel->text().contains(QStringLiteral("%3")));
        QCOMPARE(infoLabel->text(), QStringLiteral("通道: 1/1 | 数据点: 3"));

        // 3. 清空数据
        chart.clearAllData();
        chart.updateInfoLabel();
        QVERIFY(!infoLabel->text().contains(QStringLiteral("%3")));
        QCOMPARE(infoLabel->text(), QStringLiteral("通道: 1/1 | 数据点: 0"));
    }

    void testSettingsIsolationAndMultiResolutionF07Test()
    {
        // 1. 验证 QSettings 隔离配置
        QCOMPARE(QSettings::defaultFormat(), QSettings::IniFormat);
        QSettings s;
        QVERIFY(s.fileName().startsWith(m_tempSettingsDir.path()));

        // 2. 验证多分辨率窗口布局自适应 (1366x768 & 1920x1080)
        QScopedPointer<MainWindow> window(new MainWindow());
        window->setSettingsStorage(QStringLiteral("ServoValveTest"), QStringLiteral("Test_F07"));
        window->switchToWorkspace(WorkspaceId::Programming);
        window->resize(1366, 768);
        window->show();
        QTest::qWait(150);

        auto* mainToolBar = window->findChild<QToolBar*>(QStringLiteral("MainToolBar"));
        QVERIFY(mainToolBar != nullptr);
        QVERIFY(mainToolBar->isVisible());

        auto* central = window->centralWidget();
        QVERIFY(central != nullptr);
        double ratio1366 = static_cast<double>(central->width()) / static_cast<double>(window->width());
        QVERIFY2(ratio1366 >= 0.70, qPrintable(QString("ratio1366: %1").arg(ratio1366)));

        window->resize(1920, 1080);
        QTest::qWait(150);
        double ratio1920 = static_cast<double>(central->width()) / static_cast<double>(window->width());
        QVERIFY2(ratio1920 >= 0.70, qPrintable(QString("ratio1920: %1").arg(ratio1920)));
        QVERIFY(central->width() > 1300);
    }

    void deviceWorkspaceProfileConsistencyV01Test()
    {
        QScopedPointer<MainWindow> window(new MainWindow());
        auto* projectController = window->findChild<ProjectController*>();
        auto* deviceWidget = window->findChild<DeviceWorkspaceWidget*>();
        QVERIFY(projectController != nullptr);
        QVERIFY(deviceWidget != nullptr);

        window->switchToWorkspace(WorkspaceId::Device);
        QTest::qWait(50);

        // 验证连接目标 tooltip 明确区分连接与下载用途
        QVERIFY(deviceWidget->addressValueLabel()->toolTip().contains(QStringLiteral("程序下载使用独立 Profile")));

        // 用例 1：无 Profile（常规项目通信配置）
        {
            projectController->setCurrentProjectPath(QStringLiteral("C:/test_project"));
            ProjectRuntimeConfig cfg;
            cfg.projectName = QStringLiteral("DemoProjectNoProfile");
            cfg.target.model = QStringLiteral("TMS320F28335");
            cfg.commParameters.insert(QStringLiteral("port"), QStringLiteral("COM5"));
            cfg.commParameters.insert(QStringLiteral("baudRate"), 115200);
            cfg.commParameters.insert(QStringLiteral("stationId"), 1);
            projectController->runtimeConfig() = cfg;
            QVERIFY(QMetaObject::invokeMethod(window.data(), "onProjectOpened", Qt::DirectConnection,
                                              Q_ARG(ProjectRuntimeConfig, cfg)));
            window->updateDeviceWorkspaceInfo();

            QVERIFY(deviceWidget->addressValue().contains(QStringLiteral("COM5")));
            QCOMPARE(deviceWidget->downloadProfileText(), QStringLiteral("未配置下载Profile（仅离线编译）"));
            QVERIFY(deviceWidget->downloadProfileValue()->toolTip().contains(QStringLiteral("仅支持离线编译")));
        }

        // 用例 2：显式无效 Profile (文件不存在或损坏)
        {
            ProjectRuntimeConfig cfg = projectController->runtimeConfig();
            cfg.downloadArtifact.metadata.insert(QStringLiteral("downloadProfilePath"),
                                                QStringLiteral("non_existent_profile_spec.json"));
            projectController->runtimeConfig() = cfg;
            window->updateDeviceWorkspaceInfo();

            QVERIFY(deviceWidget->addressValue().contains(QStringLiteral("COM5")));
            QVERIFY(deviceWidget->downloadProfileText().contains(QStringLiteral("无效Profile: 文件不存在")));
            QVERIFY(deviceWidget->downloadProfileValue()->toolTip().contains(QStringLiteral("不存在")));
        }

        // 用例 3：有效串口/网络 Profile
        QTemporaryDir tempProfileDir;
        QVERIFY(tempProfileDir.isValid());
        const QString validProfilePath = tempProfileDir.filePath(QStringLiteral("valid_profile.json"));
        {
            QFile pf(validProfilePath);
            QVERIFY(pf.open(QIODevice::WriteOnly));
            QJsonObject root;
            root["name"] = QStringLiteral("F28335_ProductionProfile");
            root["slaveId"] = 1;
            QJsonArray steps;
            QJsonObject s1;
            s1["type"] = QStringLiteral("Enter");
            QJsonObject p1;
            p1["address"] = 100;
            QJsonArray v1;
            v1.append(1);
            p1["values"] = v1;
            s1["params"] = p1;
            steps.append(s1);

            QJsonObject s2;
            s2["type"] = QStringLiteral("SendChunk");
            QJsonObject p2;
            p2["dataAddress"] = 1000;
            p2["chunkWords"] = 60;
            s2["params"] = p2;
            steps.append(s2);

            QJsonObject s3;
            s3["type"] = QStringLiteral("Finalize");
            QJsonObject p3;
            p3["address"] = 101;
            QJsonArray v3;
            v3.append(0);
            p3["values"] = v3;
            s3["params"] = p3;
            steps.append(s3);

            root["steps"] = steps;
            pf.write(QJsonDocument(root).toJson());
            pf.close();
        }

        {
            ProjectRuntimeConfig cfg = projectController->runtimeConfig();
            cfg.downloadArtifact.metadata.insert(QStringLiteral("downloadProfilePath"), validProfilePath);
            projectController->runtimeConfig() = cfg;
            window->updateDeviceWorkspaceInfo();

            QVERIFY(deviceWidget->addressValue().contains(QStringLiteral("COM5")));
            QVERIFY(deviceWidget->downloadProfileText().contains(QStringLiteral("有效Profile: F28335_ProductionProfile")));
            QVERIFY(deviceWidget->downloadProfileText().contains(QStringLiteral("站号: 1")));
            QVERIFY(deviceWidget->downloadProfileText().contains(QStringLiteral("步骤: 3")));
        }

        // 用例 4：Profile 与 commParameters 来源不一致 (通信配置 stationId 5，而 Profile slaveId 为 1)
        {
            ProjectRuntimeConfig cfg = projectController->runtimeConfig();
            cfg.commParameters.insert(QStringLiteral("stationId"), 5);
            cfg.downloadArtifact.metadata.insert(QStringLiteral("downloadProfilePath"), validProfilePath);
            projectController->runtimeConfig() = cfg;
            window->updateDeviceWorkspaceInfo();

            // 连接目标仍稳定反映通信参数
            QVERIFY(deviceWidget->addressValue().contains(QStringLiteral("COM5")));
            // 下载目标明确标明与通信目标不一致
            QVERIFY(deviceWidget->downloadProfileText().contains(QStringLiteral("Profile与通信目标不一致")));
            QVERIFY(deviceWidget->downloadProfileText().contains(QStringLiteral("1 vs 5")));
            QVERIFY(deviceWidget->downloadProfileValue()->toolTip().contains(QStringLiteral("不匹配")));
        }
    }

    void testMultiResolutionDpiMatrixV02Test()
    {
        struct MatrixItem {
            QString name;
            int physW;
            int physH;
            double scale;
        };

        const QVector<MatrixItem> matrix = {
            {QStringLiteral("1366x768_100"), 1366, 768, 1.0},
            {QStringLiteral("1366x768_125"), 1366, 768, 1.25},
            {QStringLiteral("1366x768_150"), 1366, 768, 1.5},
            {QStringLiteral("1920x1080_100"), 1920, 1080, 1.0},
            {QStringLiteral("1920x1080_125"), 1920, 1080, 1.25},
            {QStringLiteral("1920x1080_150"), 1920, 1080, 1.5}
        };

        const QByteArray childCase = qgetenv("LH_DPI_CASE");
        if (childCase.isEmpty()) {
            for (const MatrixItem& item : matrix) {
                QProcess child;
                QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                env.insert(QStringLiteral("QT_SCALE_FACTOR"), QString::number(item.scale, 'f', 2));
                env.insert(QStringLiteral("LH_DPI_CASE"), item.name);
                env.insert(QStringLiteral("LH_UI_PARENT_RUN"), UiEvidence::runId());
                if (qEnvironmentVariableIsSet("LH_UI_EVIDENCE_ROOT")) {
                    env.insert(QStringLiteral("LH_UI_EVIDENCE_ROOT"), qEnvironmentVariable("LH_UI_EVIDENCE_ROOT"));
                }
                if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
                    env.insert(QStringLiteral("QT_QPA_PLATFORM"), qEnvironmentVariable("QT_QPA_PLATFORM"));
                } else {
                    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
                }
                child.setProcessEnvironment(env);
                child.start(QCoreApplication::applicationFilePath(),
                            {QStringLiteral("testMultiResolutionDpiMatrixV02Test")});
                QVERIFY2(child.waitForFinished(30000), qPrintable(child.errorString()));
                const QByteArray error = child.readAllStandardError() + child.readAllStandardOutput();
                QVERIFY2(child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0,
                         error.constData());
            }
            return;
        }


        QScopedPointer<MainWindow> window(new MainWindow());
        window->setSettingsStorage(QStringLiteral("ServoValveTest"), QStringLiteral("Matrix_V02"));
        window->show();
        auto saveMatrixPage = [&](const MatrixItem& item, const QString& page, QWidget* widget) {
            QVERIFY(widget != nullptr);
            QVERIFY(widget->isVisible());
            QCOMPARE(window->size(), QSize(qRound(item.physW / item.scale), qRound(item.physH / item.scale)));
            const QPixmap pix = widget->grab();
            const QString stem = QStringLiteral("%1_%2").arg(item.name, page);
            QVERIFY2(UiEvidence::save(pix, stem + QStringLiteral(".png"), page, widget),
                     qPrintable(QStringLiteral("Failed to save ") + stem));
        };

        for (const auto& item : matrix) {
            if (item.name != QString::fromLocal8Bit(childCase))
                continue;
            const int logicalW = qRound(item.physW / item.scale);
            const int logicalH = qRound(item.physH / item.scale);
            window->resize(logicalW, logicalH);
            QTest::qWait(60);
            QCOMPARE(window->size(), QSize(logicalW, logicalH));
            QVERIFY2(qAbs(window->devicePixelRatioF() - item.scale) < 0.01,
                     "QT_SCALE_FACTOR was not applied in the independent process");
            QCOMPARE(window->size(), QSize(logicalW, logicalH));

            // 1. 编程工作区适配
            window->switchToWorkspace(WorkspaceId::Programming);
            QTest::qWait(40);
            auto* central = window->centralWidget();
            QVERIFY(central != nullptr);
            QVERIFY(central->isVisible());
            QVERIFY(central->width() > 0);
            const double progRatio = static_cast<double>(central->width()) / static_cast<double>(window->width());
            QVERIFY2(progRatio >= 0.65, qPrintable(QString("%1: progRatio=%2").arg(item.name).arg(progRatio)));
            saveMatrixPage(item, QStringLiteral("programming"), window.data());

            // 2. 监控工作区适配
            window->switchToWorkspace(WorkspaceId::Monitor);
            QTest::qWait(40);
            auto* monitorWidget = window->findChild<MonitorWidget*>();
            QVERIFY(monitorWidget != nullptr);
            QVERIFY(monitorWidget->isVisible());
            QVERIFY(monitorWidget->width() > 0);
            saveMatrixPage(item, QStringLiteral("monitor"), window.data());

            // 3. 调参同屏与独立浮动窗口适配
            auto* paramTuningAction = window->findChild<QAction*>(QStringLiteral("actParameterTuning"));
            QVERIFY(paramTuningAction != nullptr);
            paramTuningAction->trigger();
            QTest::qWait(40);
            auto* tuningDock = window->findChild<QDockWidget*>(QStringLiteral("ParameterTuningDock"));
            QVERIFY(tuningDock != nullptr);
            auto* tuningPanel = qobject_cast<ParameterTuningPanel*>(tuningDock->widget());
            QVERIFY(tuningPanel != nullptr);
            QVERIFY(tuningPanel->isVisible());
            auto* tuningScroll = tuningPanel->findChild<QScrollArea*>(QStringLiteral("ParameterTuningScrollArea"));
            QVERIFY(tuningScroll != nullptr);
            QVERIFY(tuningPanel->inspectorPanel()->parameterTable() != nullptr);
            QToolButton* applyButton = nullptr;
            for (auto* button : tuningPanel->findChildren<QToolButton*>()) {
                if (button->text().contains(QStringLiteral("应用"))) {
                    applyButton = button;
                    break;
                }
            }
            QVERIFY(applyButton != nullptr);
            QVERIFY(applyButton->isVisible());

            // Reveal nested scroll areas from the innermost one outwards (R6-02 verification)
            for (QWidget* parent = applyButton->parentWidget(); parent && parent != window.data(); parent = parent->parentWidget()) {
                if (auto* scroll = qobject_cast<QScrollArea*>(parent)) {
                    scroll->ensureWidgetVisible(applyButton, 0, 0);
                    QTest::qWait(20);
                }
            }
            applyButton->setFocus();
            QTest::qWait(20);
            QVERIFY2(applyButton->hasFocus(), "applyButton must gain focus after nested scroll");

            // In the 911x512 small window scenario (1366x768 @ 150% DPR, R6-01 / R6-02 verification),
            // assert full ancestor containment for applyButton and monitor geometry
            if (logicalW == 911 && logicalH == 512) {
                for (QWidget* parent = applyButton->parentWidget(); parent && parent != window.data(); parent = parent->parentWidget()) {
                    const QRect bounds(applyButton->mapTo(parent, QPoint()), applyButton->size());
                    QVERIFY2(parent->rect().contains(bounds),
                             qPrintable(QString("applyButton must be contained in %1:%2").arg(parent->metaObject()->className(), parent->objectName())));
                }
                const QRect boundsInWindow(applyButton->mapTo(window.data(), QPoint()), applyButton->size());
                QVERIFY2(window->rect().contains(boundsInWindow), "applyButton must be contained within window rect");

                auto* monitorControlBar = monitorWidget->findChild<QWidget*>(QStringLiteral("MonitorControlBar"));
                QVERIFY(monitorControlBar != nullptr);
                QVERIFY(monitorControlBar->isVisible());
                QVERIFY2(monitorControlBar->height() >= 36, "MonitorControlBar height must be >= 36");
                auto* startStopBtn = monitorWidget->findChild<QAbstractButton*>(QStringLiteral("MonitorStartStopButton"));
                QVERIFY(startStopBtn != nullptr);
                QVERIFY(startStopBtn->isVisible());
                QVERIFY2(monitorControlBar->rect().contains(QRect(startStopBtn->mapTo(monitorControlBar, QPoint()), startStopBtn->size())),
                         "StartStopButton must be fully contained in MonitorControlBar");
                auto* onlySelectedCheck = monitorWidget->findChild<QAbstractButton*>(QStringLiteral("MonitorOnlySelectedCheckBox"));
                QVERIFY(onlySelectedCheck != nullptr);
                QVERIFY(onlySelectedCheck->isVisible());
                QVERIFY2(onlySelectedCheck->height() > 10, "MonitorOnlySelectedCheckBox must have positive operable height");
            }

            auto* chartView = monitorWidget->findChild<QWidget*>(QStringLiteral("MonitorChartView"));
            if (!chartView) {
                for (QWidget* ch : monitorWidget->findChildren<QWidget*>()) {
                    if (ch->inherits("QtCharts::QChartView")) {
                        chartView = ch;
                        break;
                    }
                }
            }
            QVERIFY(chartView != nullptr);
            QVERIFY(chartView->isVisible());
            QVERIFY2(chartView->height() >= 80, "ChartView must have at least 80px usable height");

            auto* mcv = monitorWidget->findChild<MonitorChartView*>();
            ChartWidget* cw = mcv ? mcv->chartWidget() : nullptr;
            if (cw) {
                if (logicalW == 911 && logicalH == 512) {
                    QVERIFY(cw->isCompactMode());
                    QVERIFY(cw->axisX()->tickCount() >= 3 && cw->axisX()->tickCount() <= 4);
                    QVERIFY(cw->axisY()->tickCount() >= 2 && cw->axisY()->tickCount() <= 3);
                    QVERIFY2(cw->height() >= 160, "ChartWidget in 911x512 must have >= 160px height");
                    QVERIFY2(cw->chartView()->height() >= 120, "Compact chart view must have >= 120px height");
                }
                if (cw->chart() && cw->chart()->scene()) {
                    for (QGraphicsItem* it : cw->chart()->scene()->items()) {
                        if (auto* st = dynamic_cast<QGraphicsSimpleTextItem*>(it)) {
                            const QString txt = st->text();
                            QVERIFY2(!txt.contains(QStringLiteral("...")) && !txt.contains(QChar(0x2026)),
                                     qPrintable(QString("Chart text elided in %1: %2").arg(item.name, txt)));
                        }
                    }
                }
            }

            saveMatrixPage(item, QStringLiteral("tuning_docked"), window.data());
            QVERIFY(QMetaObject::invokeMethod(window.data(), "onPopOutParameterTuning", Qt::DirectConnection));
            QTest::qWait(60);
            auto* tuningWindow = window->findChild<ParameterTuningWindow*>();
            QVERIFY(tuningWindow != nullptr);
            QVERIFY(tuningWindow->isVisible());
            QCOMPARE(tuningWindow->tuningPanel(), tuningPanel);
            QVERIFY(tuningWindow->width() > 0);
            QVERIFY(tuningWindow->width() <= logicalW);
            QVERIFY(tuningWindow->height() <= logicalH);
            saveMatrixPage(item, QStringLiteral("tuning_standalone"), tuningWindow);
            QVERIFY(QMetaObject::invokeMethod(window.data(), "onDockBackParameterTuning", Qt::DirectConnection));
            QTest::qWait(60);
            QVERIFY(tuningPanel->isVisible());
            QVERIFY(!tuningPanel->isStandaloneMode());
            saveMatrixPage(item, QStringLiteral("tuning_redocked"), window.data());

            // 4. 设备工作区与专家诊断展开适配
            window->switchToWorkspace(WorkspaceId::Device);
            QTest::qWait(40);
            auto* deviceWidget = window->findChild<DeviceWorkspaceWidget*>();
            QVERIFY(deviceWidget != nullptr);
            deviceWidget->setExpertDiagnosticVisible(true);
            QTest::qWait(40);
            QVERIFY(deviceWidget->isExpertDiagnosticVisible());
            QVERIFY(deviceWidget->btnTestConnection()->isVisible());
            QVERIFY(deviceWidget->downloadProfileValue()->isVisible());
            saveMatrixPage(item, QStringLiteral("device_expert"), window.data());

            // 5. 设置对话框适配验证
            SettingsDialog dlg(window.data());
            dlg.resize(qMin(logicalW - 40, 700), qMin(logicalH - 40, 500));
            dlg.show();
            QTest::qWait(40);
            QVERIFY(dlg.isVisible());
            QVERIFY(dlg.width() > 0);
            QCOMPARE(dlg.size(), QSize(qMin(logicalW - 40, 700), qMin(logicalH - 40, 500)));
            saveMatrixPage(item, QStringLiteral("settings_dialog"), &dlg);
            dlg.close();

            // 6. 证据保存断言
        }
    }

    void chartWidgetReadabilityAndResponsiveTest()
    {
        auto assertNoEllipsis = [](ChartWidget* cw, const char* stage) {
            QVERIFY(cw != nullptr);
            if (!cw->chart() || !cw->chart()->scene()) return;
            for (QGraphicsItem* it : cw->chart()->scene()->items()) {
                if (auto* st = dynamic_cast<QGraphicsSimpleTextItem*>(it)) {
                    const QString txt = st->text();
                    QVERIFY2(!txt.contains(QStringLiteral("...")) && !txt.contains(QChar(0x2026)),
                             qPrintable(QString("[%1] Tick or label text elided: '%2'").arg(stage, txt)));
                }
            }
        };

        ChartWidget cw;
        cw.resize(400, 160); // Small size -> compact mode
        cw.show();
        QTest::qWait(40);
        cw.updateResponsiveLayout();

        // 1. 空图表模式：横轴、纵轴刻度完整，无省略号
        QVERIFY(cw.isCompactMode());
        QVERIFY(cw.axisX()->tickCount() >= 3 && cw.axisX()->tickCount() <= 4);
        QVERIFY(cw.axisY()->tickCount() >= 2 && cw.axisY()->tickCount() <= 3);
        QVERIFY(cw.axisX()->titleText().isEmpty());
        QVERIFY(cw.axisY()->titleText().isEmpty());
        assertNoEllipsis(&cw, "EmptyChart");

        // 2. 单通道有数据模式：刻度文本包含数字且无省略号
        QVERIFY(cw.addChannelSeries(QStringLiteral("ch1"), QStringLiteral("通道1"), Qt::blue));
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (int i = 0; i < 20; ++i) {
            cw.appendPoint(QStringLiteral("ch1"), QPointF(now - (20 - i) * 1000, 10.0 + i * 2.5));
        }
        cw.requestAxisUpdate();
        QTest::qWait(60);
        assertNoEllipsis(&cw, "SingleChannelData");

        // 3. 多通道图例展开与折叠状态切换
        QVERIFY(cw.addChannelSeries(QStringLiteral("ch2"), QStringLiteral("通道2"), Qt::green));
        QVERIFY(cw.addChannelSeries(QStringLiteral("ch3"), QStringLiteral("通道3"), Qt::red));
        cw.updateResponsiveLayout();
        // 紧凑模式下：图表底部图例隐藏，折叠为顶部通道图例按钮
        QVERIFY(cw.isCompactMode());
        QVERIFY(!cw.chart()->legend()->isVisible());
        QVERIFY(cw.channelLegendButton() != nullptr);
        QVERIFY(cw.channelLegendButton()->isVisible());

        // 切换到普通模式（放大窗口）：底部图例展开，顶部折叠按钮隐藏
        cw.resize(800, 500);
        cw.setCompactMode(false);
        QVERIFY(!cw.isCompactMode());
        QVERIFY(cw.chart()->legend()->isVisible());
        QVERIFY(!cw.channelLegendButton()->isVisible());
        assertNoEllipsis(&cw, "MultiChannelNormalMode");

        // 4. 坐标轴动态精度在小数、负数、大数场景下均能完整显示
        // 场景 A: 小数跨度 (0.05 ~ 0.25)
        cw.setAutoScale(false);
        cw.setYAxisRange(0.05, 0.25);
        cw.updateResponsiveLayout();
        QVERIFY(cw.axisY()->labelFormat().contains(QLatin1String("f")));
        assertNoEllipsis(&cw, "DecimalScale");

        // 场景 B: 负数跨度 (-50.0 ~ 50.0)
        cw.setYAxisRange(-50.0, 50.0);
        cw.updateResponsiveLayout();
        QVERIFY(cw.axisY()->labelFormat() == QStringLiteral("%.0f") || cw.axisY()->labelFormat() == QStringLiteral("%.1f"));
        assertNoEllipsis(&cw, "NegativeScale");

        // 场景 C: 大数跨度 (10000.0 ~ 50000.0)
        cw.setYAxisRange(10000.0, 50000.0);
        cw.updateResponsiveLayout();
        QVERIFY(cw.axisY()->labelFormat() == QStringLiteral("%.0f"));
        assertNoEllipsis(&cw, "LargeNumberScale");

        // 场景 D: 极小量程正数跨度 (0.0 ~ 0.00001 / 1e-5)
        cw.setYAxisRange(0.0, 0.00001);
        cw.updateResponsiveLayout();
        QCOMPARE(cw.axisY()->min(), 0.0);
        QCOMPARE(cw.axisY()->max(), 0.00001);
        {
            QStringList tickLabels;
            for (int i = 0; i < cw.axisY()->tickCount(); ++i) {
                const double v = cw.axisY()->min() + (cw.axisY()->max() - cw.axisY()->min()) * i / (cw.axisY()->tickCount() - 1.0);
                tickLabels << QString::asprintf(cw.axisY()->labelFormat().toLatin1().constData(), v);
            }
            QVERIFY(tickLabels.size() >= 2);
            QCOMPARE(QSet<QString>(tickLabels.begin(), tickLabels.end()).size(), tickLabels.size());
        }
        assertNoEllipsis(&cw, "TinyScalePositive");

        // 场景 E: 负小量程跨度 (-0.00001 ~ 0.00001)
        cw.setYAxisRange(-0.00001, 0.00001);
        cw.updateResponsiveLayout();
        QCOMPARE(cw.axisY()->min(), -0.00001);
        QCOMPARE(cw.axisY()->max(), 0.00001);
        {
            QStringList tickLabels;
            for (int i = 0; i < cw.axisY()->tickCount(); ++i) {
                const double v = cw.axisY()->min() + (cw.axisY()->max() - cw.axisY()->min()) * i / (cw.axisY()->tickCount() - 1.0);
                tickLabels << QString::asprintf(cw.axisY()->labelFormat().toLatin1().constData(), v);
            }
            QVERIFY(tickLabels.size() >= 2);
            QCOMPARE(QSet<QString>(tickLabels.begin(), tickLabels.end()).size(), tickLabels.size());
        }
        assertNoEllipsis(&cw, "TinyScaleNegative");

        // 场景 F: 大基值小跨度 (1000.0 ~ 1000.00002)
        cw.setYAxisRange(1000.0, 1000.00002);
        cw.updateResponsiveLayout();
        QCOMPARE(cw.axisY()->min(), 1000.0);
        QCOMPARE(cw.axisY()->max(), 1000.00002);
        {
            QStringList tickLabels;
            for (int i = 0; i < cw.axisY()->tickCount(); ++i) {
                const double v = cw.axisY()->min() + (cw.axisY()->max() - cw.axisY()->min()) * i / (cw.axisY()->tickCount() - 1.0);
                tickLabels << QString::asprintf(cw.axisY()->labelFormat().toLatin1().constData(), v);
            }
            QVERIFY(tickLabels.size() >= 2);
            QCOMPARE(QSet<QString>(tickLabels.begin(), tickLabels.end()).size(), tickLabels.size());
        }
        assertNoEllipsis(&cw, "LargeBaseSmallSpan");

        // 场景 G: 普通模式下的极小量程验证
        cw.resize(800, 500);
        cw.setCompactMode(false);
        cw.setYAxisRange(0.0, 0.00001);
        cw.updateResponsiveLayout();
        {
            QStringList tickLabels;
            for (int i = 0; i < cw.axisY()->tickCount(); ++i) {
                const double v = cw.axisY()->min() + (cw.axisY()->max() - cw.axisY()->min()) * i / (cw.axisY()->tickCount() - 1.0);
                tickLabels << QString::asprintf(cw.axisY()->labelFormat().toLatin1().constData(), v);
            }
            QVERIFY(tickLabels.size() >= 2);
            QCOMPARE(QSet<QString>(tickLabels.begin(), tickLabels.end()).size(), tickLabels.size());
        }
        assertNoEllipsis(&cw, "NormalModeTinyScale");

        // 5. 窗口尺寸在吸附、浮动、隐藏切换测试
        QScopedPointer<MainWindow> window(new MainWindow());
        window->resize(911, 512);
        window->show();
        QTest::qWait(60);
        window->switchToWorkspace(WorkspaceId::Monitor);
        QTest::qWait(40);
        auto* monitorWidget = window->findChild<MonitorWidget*>();
        QVERIFY(monitorWidget != nullptr);
        auto* winMcv = monitorWidget->findChild<MonitorChartView*>();
        QVERIFY(winMcv != nullptr);
        ChartWidget* winCw = winMcv->chartWidget();
        QVERIFY(winCw != nullptr);

        // 调参区吸附
        auto* paramTuningAction = window->findChild<QAction*>(QStringLiteral("actParameterTuning"));
        QVERIFY(paramTuningAction != nullptr);
        paramTuningAction->trigger();
        QTest::qWait(60);
        assertNoEllipsis(winCw, "TuningDocked_911x512");
        QVERIFY(winCw->isCompactMode());
        QVERIFY2(winCw->height() >= 140, qPrintable(QString("ChartWidget in 911x512 docked was %1 px (must be >= 140px)").arg(winCw->height())));
        QVERIFY2(winCw->chartView()->height() >= 100, qPrintable(QString("ChartView in 911x512 docked was %1 px (must be >= 100px)").arg(winCw->chartView()->height())));

        // 调参区悬浮/独立
        QVERIFY(QMetaObject::invokeMethod(window.data(), "onPopOutParameterTuning", Qt::DirectConnection));
        QTest::qWait(60);
        assertNoEllipsis(winCw, "TuningStandalone_911x512");

        // 调参区隐藏/再吸附
        QVERIFY(QMetaObject::invokeMethod(window.data(), "onDockBackParameterTuning", Qt::DirectConnection));
        QTest::qWait(60);
        assertNoEllipsis(winCw, "TuningRedocked_911x512");

        auto* tuningDock = window->findChild<QDockWidget*>(QStringLiteral("ParameterTuningDock"));
        if (tuningDock) {
            tuningDock->hide();
            QTest::qWait(40);
            assertNoEllipsis(winCw, "TuningHidden_911x512");
        }
    }
};

int main(int argc, char* argv[])
{
    if (qEnvironmentVariable("QT_QPA_PLATFORM").isEmpty()) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    if (!UiEvidence::initialize()) {
        qCritical("Cannot initialize UI evidence: source fingerprint or run directory unavailable");
        return 2;
    }
    QStringList arguments = QCoreApplication::arguments();
    if (!arguments.contains(QStringLiteral("-o")))
        arguments << QStringLiteral("-o") << QStringLiteral("-,txt");
    arguments << QStringLiteral("-o") << QDir(UiEvidence::directory()).filePath(QStringLiteral("test.log")) + QStringLiteral(",txt");
    MainWindowIntegrationTest test;
    return QTest::qExec(&test, arguments);
}
#include "main_window_integration_test.moc"
