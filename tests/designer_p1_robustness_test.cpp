/**
 * @file designer_p1_robustness_test.cpp
 * @brief IDE 第一阶段（诊断导航、命令面板、快速打开、跳行、快捷键及 FL14/FL15 回归）健全性测试套件
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QPlainTextEdit>
#include <QAction>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QToolBar>
#include <QSignalSpy>
#include <QTimer>
#include <QTextCursor>
#include <QTextBlock>
#include <QTableWidget>
#include <QSettings>
#include <QPointer>
#include <QComboBox>
#include <QInputMethodEvent>
#include <QJsonDocument>
#include "designer/BuildController.h"

#include "designer/MainWindow.h"
#include "designer/ProjectController.h"
#include "designer/ProjectExplorerWidget.h"
#include "designer/ProgramBlocksWidget.h"
#include "designer/RuntimeSessionController.h"
#include "designer/SettingsController.h"
#include "designer/ParameterController.h"
#include "designer/ParameterTuningPanel.h"
#include "communication/VirtualDeviceBackend.h"
#include "communication/IOpcServer.h"
#include "monitor/MonitorManager.h"
#include "monitor/MonitorDataProcessor.h"
#include "monitor/MonitorWidget.h"
#include "monitor/SampleDataProvider.h"
#include "designer/DslScriptEditor.h"
#include "designer/ui/DiagnosticItem.h"
#include "designer/ui/DiagnosticParser.h"
#include "designer/ui/ProblemsPanel.h"
#include "designer/ui/InspectorPanel.h"
#include "designer/ui/CommandPaletteDialog.h"
#include "designer/ui/ThemeManager.h"
#include "designer/ui/NotificationBanner.h"
#include <QDockWidget>

class CountingParameterBackend : public VirtualDeviceBackend
{
public:
    int writes = 0;
    void writePointsAsync(const QHash<QString, QVariant>& values, int timeout,
                          std::shared_ptr<std::atomic_bool> cancelled, QObject* context,
                          WriteCompletion complete) override
    {
        ++writes;
        VirtualDeviceBackend::writePointsAsync(values, timeout, cancelled, context, std::move(complete));
    }
};

class DesignerP1RobustnessTest : public QObject
{
    Q_OBJECT

private slots:
    void parameterInlineWritesOnlyOnEnter()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        MainWindow window; window.show();
        auto* project = window.findChild<ProjectController*>(); QVERIFY(project);
        auto* controller = window.findChild<ParameterController*>(); QVERIFY(controller);
        auto* session = window.findChild<RuntimeSessionController*>(); QVERIFY(session);
        auto* tuning = window.findChild<ParameterTuningPanel*>(); QVERIFY(tuning);
        auto* dock = window.findChild<QDockWidget*>(QStringLiteral("ParameterTuningDock")); QVERIFY(dock);
        ParameterDefinition p; p.id = QStringLiteral("pid.kp"); p.name = QStringLiteral("Kp");
        p.dataType = QStringLiteral("REAL"); p.currentValue = QStringLiteral("1");
        p.defaultValue = QStringLiteral("1"); p.onlineEditable = true;
        project->setCurrentProjectPath(dir.path()); project->runtimeConfig().parameters = {p};
        controller->loadDefinitions({p}); window.refreshInspectorPanel();
        CountingParameterBackend backend;
        RuntimePointDefinition point; point.id = p.id; point.name = p.name;
        point.kind = RuntimePointKind::Parameter; point.dataType = p.dataType; point.access = RuntimePointAccess::ReadWrite;
        backend.loadPointDefinitions({point}); QVERIFY(backend.connectBackend()); session->setDeviceBackend(&backend);
        dock->show(); QTest::qWait(40);
        auto* table = tuning->inspectorPanel()->parameterTable(); QVERIFY(table && table->rowCount());
        table->editItem(table->item(0, 2));
        auto* editor = table->findChild<QLineEdit*>(); QVERIFY(editor);
        editor->setText(QStringLiteral("2")); QTest::keyClick(editor, Qt::Key_Escape);
        QTest::qWait(20); QCOMPARE(backend.writes, 0);
        table->editItem(table->item(0, 2)); editor = table->findChild<QLineEdit*>(); QVERIFY(editor);
        editor->setText(QStringLiteral("3")); QTest::keyClick(editor, Qt::Key_Tab);
        QTRY_COMPARE(controller->parameterStateByPointId(p.id).editedValue, QStringLiteral("3"));
        QCOMPARE(backend.writes, 0);
        QTest::qWait(30);
        table->editItem(table->item(0, 2)); editor = table->findChild<QLineEdit*>(); QVERIFY(editor);
        editor->setText(QStringLiteral("4")); QTest::keyClick(editor, Qt::Key_Return);
        QTRY_COMPARE(backend.writes, 1);
        QTRY_COMPARE(controller->parameterStateByPointId(p.id).state, ParameterState::Confirmed);
        QTest::qWait(30);
        table->editItem(table->item(0, 2)); editor = table->findChild<QLineEdit*>(); QVERIFY(editor);
        editor->setText(QStringLiteral("5"));
        backend.disconnectBackend(); QVERIFY(backend.connectBackend());
        QTest::keyClick(editor, Qt::Key_Return);
        QTest::qWait(100);
        QCOMPARE(backend.writes, 1);
        session->requestStop();
        session->setDeviceBackend(&backend);
        CountingParameterBackend replacement;
        replacement.loadPointDefinitions({point}); QVERIFY(replacement.connectBackend());
        QTest::qWait(30);
        table->editItem(table->item(0, 2)); editor = table->findChild<QLineEdit*>(); QVERIFY(editor);
        editor->setText(QStringLiteral("6"));
        const auto reentrantSwitch = connect(controller, &ParameterController::stateChanged, &window,
            [session, &replacement](const QString&, ParameterState, ParameterState next) {
                if (next == ParameterState::Modified) session->setDeviceBackend(&replacement);
            });
        QTest::keyClick(editor, Qt::Key_Return);
        QTest::qWait(100);
        QCOMPARE(backend.writes, 1);
        QCOMPARE(replacement.writes, 0);
        disconnect(reentrantSwitch);
        session->setDeviceBackend(nullptr);
    }

    void parameterInlineEditIntent()
    {
        InspectorPanel panel;
        panel.setPanelMode(InspectorPanel::PanelMode::Tuning);
        ParameterDefinition p; p.id = QStringLiteral("p1"); p.name = QStringLiteral("参数");
        p.dataType = QStringLiteral("INT32"); p.currentValue = QStringLiteral("1");
        p.onlineEditable = true; p.minValue = QStringLiteral("0"); p.maxValue = QStringLiteral("100");
        panel.setParameterDetails({p});
        panel.show();
        auto* table = panel.parameterTable(); QVERIFY(table);
        QSignalSpy committed(&panel, &InspectorPanel::parameterValueCommitted);
        table->editItem(table->item(0, 2));
        auto* editor = table->findChild<QLineEdit*>(); QVERIFY(editor);
        editor->setText(QStringLiteral("101"));
        QTest::keyClick(editor, Qt::Key_Return);
        QCOMPARE(committed.count(), 0);
        QVERIFY(editor->isVisible());
        editor->setText(QStringLiteral("12"));
        QTest::keyClick(editor, Qt::Key_Return);
        QTRY_COMPARE(committed.count(), 1);
        QCOMPARE(committed.first().at(0).toString(), QStringLiteral("p1"));
        QCOMPARE(committed.first().at(1).toString(), QStringLiteral("12"));
        QVERIFY(committed.first().at(2).toBool());
        QTest::qWait(20);
        table->editItem(table->item(0, 2));
        editor = table->findChild<QLineEdit*>(); QVERIFY(editor);
        editor->setText(QStringLiteral("13"));
        QTest::keyClick(editor, Qt::Key_Escape);
        QTest::qWait(20);
        QCOMPARE(committed.count(), 1);
        table->editItem(table->item(0, 2));
        editor = table->findChild<QLineEdit*>(); QVERIFY(editor);
        editor->setText(QStringLiteral("14"));
        QTest::keyClick(editor, Qt::Key_Tab);
        QTRY_COMPARE(committed.count(), 2);
        QCOMPARE(committed.last().at(1).toString(), QStringLiteral("14"));
        QVERIFY(!committed.last().at(2).toBool());
        QTest::qWait(20);
        table->editItem(table->item(0, 2));
        editor = table->findChild<QLineEdit*>(); QVERIFY(editor);
        editor->setText(QStringLiteral("15"));
        panel.invalidateParameterEdits();
        QTest::keyClick(editor, Qt::Key_Return);
        QTest::qWait(20);
        QCOMPARE(committed.count(), 2);
        QTest::keyClick(editor, Qt::Key_Escape);
    }

    void parameterInlineTypedEditors()
    {
        InspectorPanel panel;
        panel.setPanelMode(InspectorPanel::PanelMode::Tuning);
        auto make = [](const QString& id, const QString& type, const QString& minValue,
                       const QString& maxValue, bool editable = true) {
            ParameterDefinition def;
            def.id = id; def.name = id; def.dataType = type;
            def.currentValue = QStringLiteral("1");
            def.minValue = minValue; def.maxValue = maxValue;
            def.onlineEditable = editable;
            return def;
        };
        panel.setParameterDetails({
            make(QStringLiteral("u32"), QStringLiteral("UINT32"), QStringLiteral("0"), QStringLiteral("4294967295")),
            make(QStringLiteral("i32"), QStringLiteral("INT32"), QStringLiteral("-2147483648"), QStringLiteral("2147483647")),
            make(QStringLiteral("real"), QStringLiteral("REAL"), QStringLiteral("0"), QStringLiteral("100")),
            make(QStringLiteral("text"), QStringLiteral("STRING"), QString(), QString()),
            make(QStringLiteral("flag"), QStringLiteral("BOOL"), QString(), QString()),
            make(QStringLiteral("ro"), QStringLiteral("UINT32"), QString(), QString(), false)
        });
        panel.show();
        auto* table = panel.parameterTable();
        QVERIFY(table);

        auto editRow = [table](int row) {
            table->editItem(table->item(row, 2));
            return table->findChild<QLineEdit*>();
        };
        auto closeEditor = [table] {
            if (auto* editor = table->findChild<QLineEdit*>())
                QTest::keyClick(editor, Qt::Key_Escape);
            QTest::qWait(10);
        };
        auto validateWith = [](const QValidator* validator, const QString& text) {
            QString input = text;
            int pos = 0;
            return validator->validate(input, pos);
        };

        auto* u32 = editRow(0);
        QVERIFY(u32 && u32->validator());
        QCOMPARE(validateWith(u32->validator(), QStringLiteral("4294967295")), QValidator::Acceptable);
        QVERIFY(validateWith(u32->validator(), QStringLiteral("4294967296")) != QValidator::Acceptable);
        QCOMPARE(validateWith(u32->validator(), QStringLiteral("1.5")), QValidator::Invalid);
        closeEditor();

        auto* i32 = editRow(1);
        QVERIFY(i32 && i32->validator());
        QVERIFY(validateWith(i32->validator(), QStringLiteral("2147483648")) != QValidator::Acceptable);
        QCOMPARE(validateWith(i32->validator(), QStringLiteral("-2147483648")), QValidator::Acceptable);
        closeEditor();

        auto* real = editRow(2);
        QVERIFY(real && real->validator());
        QCOMPARE(validateWith(real->validator(), QStringLiteral("99.5")), QValidator::Acceptable);
        QCOMPARE(validateWith(real->validator(), QStringLiteral("0.0000000001")), QValidator::Acceptable);
        QCOMPARE(validateWith(real->validator(), QStringLiteral("0.123456789012")), QValidator::Acceptable);
        QCOMPARE(validateWith(real->validator(), QStringLiteral("1.25e-5")), QValidator::Acceptable);
        QVERIFY(validateWith(real->validator(), QStringLiteral("101")) != QValidator::Acceptable);
        real->clear();
        QTest::keyClicks(real, QStringLiteral("0.0000000001"));
        QCOMPARE(real->text(), QStringLiteral("0.0000000001"));
        closeEditor();

        auto* text = editRow(3);
        QVERIFY(text);
        QVERIFY(!text->validator());
        closeEditor();

        table->editItem(table->item(4, 2));
        QVERIFY(table->findChild<QComboBox*>());
        QVERIFY(table->findChild<QLineEdit*>() == nullptr);
        if (auto* combo = table->findChild<QComboBox*>())
            QTest::keyClick(combo, Qt::Key_Escape);
        QTest::qWait(10);

        table->editItem(table->item(5, 2));
        QVERIFY(table->findChild<QLineEdit*>() == nullptr);
        QVERIFY(table->findChild<QComboBox*>() == nullptr);
    }

    void parameterOfflineAndDuplicateNamesUsePointId()
    {
        InspectorPanel panel;
        panel.setPanelMode(InspectorPanel::PanelMode::Tuning);
        ParameterDefinition first;
        first.id = QStringLiteral("point.a"); first.name = QStringLiteral("Kp");
        first.dataType = QStringLiteral("INT32"); first.currentValue = QStringLiteral("1");
        first.defaultValue = QStringLiteral("1"); first.onlineEditable = true;
        ParameterDefinition second = first;
        second.id = QStringLiteral("point.b"); second.currentValue = QStringLiteral("2");
        second.defaultValue = QStringLiteral("2");
        panel.setParameterRuntimeOnline(false);
        panel.setParameterDetails({first, second});
        panel.resize(900, 420);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        QTest::qWait(30);

        auto* table = panel.parameterTable();
        QVERIFY(table && table->rowCount() == 2);
        QCOMPARE(table->item(0, 0)->text(), table->item(1, 0)->text());

        QSignalSpy committed(&panel, &InspectorPanel::parameterValueCommitted);
        table->setCurrentCell(0, 2);
        table->editItem(table->item(0, 2));
        auto* editor = table->findChild<QLineEdit*>();
        QVERIFY(editor);
        QVERIFY(qApp->activeModalWidget() == nullptr);
        editor->setText(QStringLiteral("7"));
        QTest::keyClick(editor, Qt::Key_Tab);
        QTRY_COMPARE(committed.count(), 1);
        QCOMPARE(committed.first().at(0).toString(), QStringLiteral("point.a"));
        QCOMPARE(committed.first().at(1).toString(), QStringLiteral("7"));

        ParameterStateInfo firstState;
        firstState.pointId = QStringLiteral("point.a");
        firstState.name = QStringLiteral("Kp");
        firstState.state = ParameterState::Modified;
        firstState.editedValue = QStringLiteral("7");
        firstState.onlineEditable = true;
        ParameterStateInfo secondState = firstState;
        secondState.pointId = QStringLiteral("point.b");
        secondState.state = ParameterState::Clean;
        secondState.editedValue.clear();
        panel.setParameterStateMap({{QStringLiteral("point.a"), firstState},
                                    {QStringLiteral("point.b"), secondState}});
        QVERIFY(table->item(0, 3)->text().contains(QStringLiteral("未下发")));
    }

    void systemViewPopOutKeepsSharedFrontend()
    {
        MainWindow window; window.show(); QTest::qWait(30);
        auto* monitor = window.findChild<MonitorWidget*>(); QVERIFY(monitor);
        auto* processor = Monitor::MonitorManager::instance().dataProcessor();
        auto* action = window.findChild<QAction*>(QStringLiteral("actFloatMonitor")); QVERIFY(action);

        action->setChecked(true);
        QTest::qWait(40);
        auto* dock = window.findChild<QDockWidget*>(QStringLiteral("MonitorFloatingDock"));
        QVERIFY(dock && dock->isVisible());
        QVERIFY(dock->widget());
        QVERIFY(dock->widget() == monitor || dock->widget()->isAncestorOf(monitor));
        QCOMPARE(window.findChild<MonitorWidget*>(), monitor);
        QCOMPARE(Monitor::MonitorManager::instance().dataProcessor(), processor);

        action->setChecked(false);
        QTest::qWait(40);
        QVERIFY(!window.findChild<QDockWidget*>(QStringLiteral("MonitorFloatingDock")));
        QCOMPARE(window.findChild<MonitorWidget*>(), monitor);
        QCOMPARE(Monitor::MonitorManager::instance().dataProcessor(), processor);

        action->setChecked(true);
        QTest::qWait(40);
        dock = window.findChild<QDockWidget*>(QStringLiteral("MonitorFloatingDock"));
        QVERIFY(dock);
        dock->close();
        QTest::qWait(60);
        QVERIFY(!window.findChild<QDockWidget*>(QStringLiteral("MonitorFloatingDock")));
        QCOMPARE(window.findChild<MonitorWidget*>(), monitor);
        QCOMPARE(Monitor::MonitorManager::instance().dataProcessor(), processor);
    }

    void toolbarOrderStableAcrossWorkspaces()
    {
        MainWindow window; window.show(); QTest::qWait(20);
        auto* toolbar = window.findChild<QToolBar*>(QStringLiteral("MainToolBar")); QVERIFY(toolbar);
        const auto initialActions = toolbar->actions();
        QVERIFY(!initialActions.isEmpty());
        window.switchToWorkspace(WorkspaceId::Monitor);
        QCOMPARE(toolbar->actions(), initialActions);
        window.switchToWorkspace(WorkspaceId::Device);
        QCOMPARE(toolbar->actions(), initialActions);
        window.switchToWorkspace(WorkspaceId::Programming);
        QCOMPARE(toolbar->actions(), initialActions);
    }

    void themeDensityWorkspaceGeometry()
    {
        MainWindow window;
        auto* monitor = window.findChild<MonitorWidget*>(); QVERIFY(monitor);
        auto* mdi = window.findChild<QMdiArea*>(); QVERIFY(mdi);
        window.show();
        for (const QSize size : {QSize(1366, 768), QSize(1920, 1080)}) {
            window.resize(size);
            QTest::qWait(40);
            for (auto density : {ThemeManager::Density::Compact, ThemeManager::Density::Comfortable}) {
                for (auto theme : {ThemeManager::ThemeMode::Light, ThemeManager::ThemeMode::Dark}) {
                    ThemeManager::applyTheme(qApp, theme);
                    ThemeManager::applyDensity(qApp, density);
                    QTest::qWait(20);
                    window.switchToWorkspace(WorkspaceId::Programming);
                    QVERIFY(mdi->width() > 0);
                    QVERIFY(mdi->height() > 0);
                    window.switchToWorkspace(WorkspaceId::Monitor);
                    QVERIFY(monitor->isVisible());
                    QVERIFY(monitor->width() > 0);
                    QVERIFY(monitor->height() > 0);
                }
            }
        }
        ThemeManager::applyTheme(qApp, ThemeManager::ThemeMode::Light);
        ThemeManager::applyDensity(qApp, ThemeManager::Density::Compact);
    }

    void longNotificationDoesNotGrowWindow()
    {
        MainWindow window; window.show();
        auto* banner = window.findChild<NotificationBanner*>(QStringLiteral("NotificationBanner"));
        QVERIFY(banner);
        const QString longText(160, QChar(0x8bbe));
        for (const QSize size : {QSize(911, 512), QSize(1366, 768)}) {
            for (auto theme : {ThemeManager::ThemeMode::Light, ThemeManager::ThemeMode::Dark}) {
                for (auto density : {ThemeManager::Density::Compact, ThemeManager::Density::Comfortable}) {
                    ThemeManager::applyTheme(qApp, theme);
                    ThemeManager::applyDensity(qApp, density);
                    window.resize(size);
                    QTest::qWait(30);
                    const int before = window.width();
                    banner->showNotification(QStringLiteral("error"), QStringLiteral("下载失败"), longText);
                    QTest::qWait(40);
                    QCOMPARE(window.width(), before);
                    QVERIFY(window.minimumSizeHint().width() <= before);
                    QVERIFY(banner->width() <= before);
                    auto* close = banner->findChild<QToolButton*>(QStringLiteral("NotificationCloseButton"));
                    QVERIFY(close && close->isVisible());
                    QVERIFY(window.rect().contains(
                        QRect(close->mapTo(&window, QPoint()), close->size())));
                    window.resize(size);
                    QTest::qWait(20);
                    QCOMPARE(window.width(), size.width());
                    banner->clear();
                }
            }
        }
        ThemeManager::applyTheme(qApp, ThemeManager::ThemeMode::Light);
        ThemeManager::applyDensity(qApp, ThemeManager::Density::Compact);
    }

    void floatingSystemViewsRestoreAfterRestart()
    {
        const QString org = QStringLiteral("LHPhase4Test");
        const QString app = QStringLiteral("FloatingRestart-%1").arg(QCoreApplication::applicationPid());
        QPointer<MonitorDataProcessor> processor;
        QSize savedMonitorSize, savedDeviceSize;
        {
            MainWindow window;
            window.setSettingsStorage(org, app);
            window.show();
            QTest::qWait(40);
            processor = Monitor::MonitorManager::instance().dataProcessor();

            window.switchToWorkspace(WorkspaceId::Monitor);
            auto* monitorAction = window.findChild<QAction*>(QStringLiteral("actFloatMonitor"));
            QVERIFY(monitorAction);
            monitorAction->setChecked(true);
            QTest::qWait(40);
            auto* monitorDock = window.findChild<QDockWidget*>(QStringLiteral("MonitorFloatingDock"));
            QVERIFY(monitorDock && monitorDock->isFloating());
            monitorDock->setGeometry(80, 80, 620, 460);
            QTest::qWait(40);
            savedMonitorSize = monitorDock->size();

            window.switchToWorkspace(WorkspaceId::Device);
            auto* deviceAction = window.findChild<QAction*>(QStringLiteral("actFloatDevice"));
            QVERIFY(deviceAction);
            deviceAction->setChecked(true);
            QTest::qWait(40);
            auto* deviceDock = window.findChild<QDockWidget*>(QStringLiteral("DeviceFloatingDock"));
            QVERIFY(deviceDock && deviceDock->isFloating());
            deviceDock->setGeometry(120, 120, 600, 440);
            QTest::qWait(40);
            savedDeviceSize = deviceDock->size();
            qInfo() << "Saved actual floating sizes" << savedMonitorSize << savedDeviceSize;

            window.saveWindowGeometryAndWorkspaces();
            auto* settings = window.findChild<SettingsController*>();
            QVERIFY(settings);
            settings->saveSettings();
        }
        QTest::qWait(40);
        {
            MainWindow restored;
            restored.setSettingsStorage(org, app);
            restored.show();
            QTest::qWait(80);
            auto* monitorDock = restored.findChild<QDockWidget*>(QStringLiteral("MonitorFloatingDock"));
            auto* deviceDock = restored.findChild<QDockWidget*>(QStringLiteral("DeviceFloatingDock"));
            QVERIFY(monitorDock && monitorDock->isFloating() && monitorDock->isVisible());
            QVERIFY(deviceDock && deviceDock->isFloating() && deviceDock->isVisible());
            qInfo() << "Restored floating sizes" << monitorDock->size() << deviceDock->size();
            // Qt may enlarge a requested dock size to its content minimum. The
            // persisted contract is the actual settled size the user saw.
            QVERIFY(qAbs(monitorDock->width() - savedMonitorSize.width()) <= 24);
            QVERIFY(qAbs(monitorDock->height() - savedMonitorSize.height()) <= 24);
            QVERIFY(qAbs(deviceDock->width() - savedDeviceSize.width()) <= 24);
            QVERIFY(qAbs(deviceDock->height() - savedDeviceSize.height()) <= 24);
            auto* monitorAction = restored.findChild<QAction*>(QStringLiteral("actFloatMonitor"));
            auto* deviceAction = restored.findChild<QAction*>(QStringLiteral("actFloatDevice"));
            QVERIFY(monitorAction && monitorAction->isChecked());
            QVERIFY(deviceAction && deviceAction->isChecked());
            QCOMPARE(Monitor::MonitorManager::instance().dataProcessor(), processor.data());
            QVERIFY(restored.findChild<MonitorWidget*>());
        }
        QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, org, app);
        settings.clear();
        QFile::remove(settings.fileName());
    }

    void resetAllLayoutsClearsFloatingViewsAcrossRestart()
    {
        const QString org = QStringLiteral("LHPhase4Test");
        const QString app = QStringLiteral("ResetFloating-%1").arg(QCoreApplication::applicationPid());
        QPointer<MonitorDataProcessor> processor;
        {
            MainWindow window;
            window.setSettingsStorage(org, app);
            window.show();
            auto* settings = window.findChild<SettingsController*>();
            QVERIFY(settings);
            settings->setThemeMode(1);
            settings->setDensity(1);
            settings->setFontSizeIndex(2);
            settings->setDefaultProjectDir(QStringLiteral("D:/preferred"));
            processor = Monitor::MonitorManager::instance().dataProcessor();
            for (const QString& name : {QStringLiteral("actFloatMonitor"), QStringLiteral("actFloatDevice")}) {
                auto* action = window.findChild<QAction*>(name);
                QVERIFY(action);
                action->setChecked(true);
            }
            window.saveWindowGeometryAndWorkspaces();
            settings->saveSettings();
            QVERIFY(settings->systemViewFloating(WorkspaceId::Monitor));
            QVERIFY(settings->systemViewFloating(WorkspaceId::Device));
            QVERIFY(QMetaObject::invokeMethod(&window, "onResetAllWorkspaceLayouts", Qt::DirectConnection));
            QTest::qWait(40);
            QVERIFY(!window.findChild<QDockWidget*>(QStringLiteral("MonitorFloatingDock")));
            QVERIFY(!window.findChild<QDockWidget*>(QStringLiteral("DeviceFloatingDock")));
            for (const QString& id : {WorkspaceId::Monitor, WorkspaceId::Device}) {
                QVERIFY(!settings->systemViewFloating(id));
                QVERIFY(settings->systemViewFloatGeometry(id).isEmpty());
            }
            QCOMPARE(Monitor::MonitorManager::instance().dataProcessor(), processor.data());
            window.saveWindowGeometryAndWorkspaces();
            settings->saveSettings();
        }
        {
            MainWindow restored;
            restored.setSettingsStorage(org, app);
            restored.show();
            QTest::qWait(40);
            QVERIFY(!restored.findChild<QDockWidget*>(QStringLiteral("MonitorFloatingDock")));
            QVERIFY(!restored.findChild<QDockWidget*>(QStringLiteral("DeviceFloatingDock")));
            auto* settings = restored.findChild<SettingsController*>();
            QVERIFY(settings);
            QCOMPARE(settings->themeMode(), 1);
            QCOMPARE(settings->density(), 1);
            QCOMPARE(settings->fontSizeIndex(), 2);
            QCOMPARE(settings->defaultProjectDir(), QStringLiteral("D:/preferred"));
            QCOMPARE(Monitor::MonitorManager::instance().dataProcessor(), processor.data());
        }
        QSettings saved(QSettings::defaultFormat(), QSettings::UserScope, org, app);
        saved.clear();
        ThemeManager::applyTheme(qApp, ThemeManager::ThemeMode::Light);
        ThemeManager::applyDensity(qApp, ThemeManager::Density::Compact);
    }

    void editorAcceptsImeAndKeepsFocus()
    {
        MainWindow window; window.show();
        auto* editor = window.findChild<DslScriptEditor*>(); QVERIFY(editor);
        auto* inner = editor->editor(); QVERIFY(inner);
        editor->setScript(QString());
        inner->setFocus();
        QTest::qWait(30);
        QVERIFY((inner->focusPolicy() & Qt::StrongFocus) == Qt::StrongFocus);

        QInputMethodEvent ime;
        ime.setCommitString(QStringLiteral("中文输入"));
        QApplication::sendEvent(inner, &ime);
        QVERIFY(editor->currentScript().contains(QStringLiteral("中文输入")));

        QTest::keyClick(inner, Qt::Key_A, Qt::ControlModifier);
        QVERIFY(inner->textCursor().hasSelection());
    }

    void parameterInlineBooleanAndFocusOut()
    {
        InspectorPanel panel;
        panel.setPanelMode(InspectorPanel::PanelMode::Tuning);
        ParameterDefinition p; p.id = QStringLiteral("flag"); p.name = QStringLiteral("使能");
        p.dataType = QStringLiteral("BOOL"); p.currentValue = QStringLiteral("0"); p.onlineEditable = true;
        panel.setParameterDetails({p});
        panel.show();
        auto* table = panel.parameterTable();
        QSignalSpy committed(&panel, &InspectorPanel::parameterValueCommitted);
        table->editItem(table->item(0, 2));
        auto* combo = table->findChild<QComboBox*>(); QVERIFY(combo);
        combo->setCurrentIndex(1);
        QTest::keyClick(combo, Qt::Key_Return);
        QTRY_COMPARE(committed.count(), 1);
        QCOMPARE(committed.first().at(1).toString(), QStringLiteral("1"));
        QVERIFY(committed.first().at(2).toBool());
        QTest::qWait(20);
        table->editItem(table->item(0, 2));
        combo = table->findChild<QComboBox*>(); QVERIFY(combo);
        combo->setCurrentIndex(1);
        combo->setFocus();
        QTest::qWait(20);
        table->setFocus(Qt::OtherFocusReason);
        QTRY_COMPARE(committed.count(), 2);
        QVERIFY(!committed.last().at(2).toBool());
    }

    void saveAllAfterRebindingCleanEntryClearsDirtyState()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString main = dir.filePath("main.lh"), helper = dir.filePath("helper.lh");
        for (const QString& path : {main, helper}) {
            QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("PROGRAM P\nEND_PROGRAM\n");
        }
        ProjectRuntimeConfig config; config.projectName = "dirty-rebind";
        config.mainScriptPath = "main.lh"; config.dslScriptPath = "main.lh";
        config.scriptFiles = QStringList{"main.lh", "helper.lh"};
        QFile cfg(dir.filePath("project_config.json")); QVERIFY(cfg.open(QIODevice::WriteOnly));
        cfg.write(QJsonDocument(config.toJson()).toJson()); cfg.close();
        MainWindow window; window.show();
        auto* controller = window.findChild<ProjectController*>(); QVERIFY(controller);
        QVERIFY(controller->openProjectFromPath(dir.path()));
        auto* first = qobject_cast<DslScriptEditor*>(window.openAndActivateFile(main)); QVERIFY(first);
        first->editor()->insertPlainText("preserved ");
        auto* second = qobject_cast<DslScriptEditor*>(window.openAndActivateFile(helper)); QVERIFY(second);
        QVERIFY(!second->isModified());
        auto* explorer = window.findChild<ProjectExplorerWidget*>(); QVERIFY(explorer);
        QVERIFY(QMetaObject::invokeMethod(explorer, "mainScriptRequested", Qt::DirectConnection, Q_ARG(QString, helper)));
        QVERIFY(QMetaObject::invokeMethod(&window, "onSaveAll", Qt::DirectConnection));
        QVERIFY(!first->isModified()); QVERIFY(!second->isModified()); QVERIFY(!controller->isModified());
        QFile saved(main); QVERIFY(saved.open(QIODevice::ReadOnly)); QVERIFY(saved.readAll().contains("preserved "));
        int decisions = 0;
        window.setMessageBoxHook([&](const QString&, const QString&, QMessageBox::StandardButtons,
                                     QMessageBox::StandardButton) { ++decisions; return QMessageBox::Cancel; });
        QVERIFY(QMetaObject::invokeMethod(&window, "onCloseProject", Qt::DirectConnection));
        QVERIFY(!controller->hasOpenProject()); QCOMPARE(decisions, 0);
    }

    void legacyAbsoluteEntryUsesRelativeMappingKey()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString main = dir.filePath("main.lh"), helper = dir.filePath("helper.lh");
        for (const QString& path : {main, helper}) {
            QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("PROGRAM P\nEND_PROGRAM\n");
        }
        ProjectRuntimeConfig config;
        config.projectName = "legacy"; config.mainScriptPath = main;
        config.dslScriptPath = main; config.scriptFiles = QStringList{main, helper};
        DslMappingEntry mapping; mapping.id = "legacy-map"; mapping.lineNumber = 2;
        config.dslMappings.append(mapping);
        QFile cfg(dir.filePath("project_config.json")); QVERIFY(cfg.open(QIODevice::WriteOnly));
        cfg.write(QJsonDocument(config.toJson()).toJson()); cfg.close();
        ProjectController controller;
        DslScriptEditor editor;
        controller.setDslEditor(&editor);
        connect(&controller, &ProjectController::scriptLoadRequired, &editor,
                [&editor](const QString& path, const QString& text) {
            editor.setCurrentFilePath(path); editor.setScript(text); editor.setModified(false);
        });
        QVERIFY(controller.openProjectFromPath(dir.path()));
        // Simulate a legacy configuration that retains an absolute entry path.
        controller.runtimeConfig().mainScriptPath = main;
        QVERIFY(controller.setMainScriptFile(helper));
        QCOMPARE(controller.runtimeConfig().documentDslMappings.value("main.lh").size(), 1);
        QVERIFY(!controller.runtimeConfig().documentDslMappings.contains(QDir::cleanPath(main).toCaseFolded()));
        QVERIFY(controller.setMainScriptFile(main));
        QCOMPARE(controller.runtimeConfig().dslMappings.first().id, QString("legacy-map"));
    }

    void entrySwitchPreservesIndependentBuffersAndMappings()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath("main.lh"), b = dir.filePath("helper.lh");
        for (const QString& path : {a, b}) {
            QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("PROGRAM P\nEND_PROGRAM\n");
        }
        ProjectRuntimeConfig config;
        config.projectName = "documents"; config.mainScriptPath = "main.lh";
        config.dslScriptPath = "main.lh"; config.scriptFiles = QStringList{"main.lh", "helper.lh"};
        DslMappingEntry mapping; mapping.id = "main-map"; mapping.lineNumber = 2;
        config.dslMappings.append(mapping);
        QFile cfg(dir.filePath("project_config.json")); QVERIFY(cfg.open(QIODevice::WriteOnly));
        cfg.write(QJsonDocument(config.toJson()).toJson()); cfg.close();
        MainWindow window; window.show();
        auto* controller = window.findChild<ProjectController*>(); QVERIFY(controller);
        QVERIFY(controller->openProjectFromPath(dir.path()));
        auto* first = qobject_cast<DslScriptEditor*>(window.openAndActivateFile(a)); QVERIFY(first);
        first->editor()->insertPlainText("first ");
        auto* second = qobject_cast<DslScriptEditor*>(window.openAndActivateFile(b)); QVERIFY(second && second != first);
        second->editor()->insertPlainText("second ");
        QCOMPARE(window.openAndActivateFile(QDir(dir.path()).filePath("./helper.lh")), static_cast<QWidget*>(second));
        controller->runtimeConfig().projectName = "unsaved metadata";
        controller->setModified(true);
        QVERIFY(QMetaObject::invokeMethod(&window, "onSaveProject", Qt::DirectConnection));
        QVERIFY(first->isModified()); QVERIFY(!second->isModified());
        QFile beforeSaveAll(cfg.fileName()); QVERIFY(beforeSaveAll.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(beforeSaveAll.readAll()).object().value("projectName").toString(), QString("documents"));
        beforeSaveAll.close();
        auto* blocks = window.findChild<ProgramBlocksWidget*>(); QVERIFY(blocks);
        FunctionSnippet snippet; snippet.id = "test_block"; snippet.name = "Test Block";
        snippet.templateCode = "x = test_block()";
        blocks->snippetDoubleClicked(snippet);
        QVERIFY(second->currentScript().contains(snippet.templateCode));
        QVERIFY(!first->currentScript().contains(snippet.templateCode));
        auto* explorer = window.findChild<ProjectExplorerWidget*>(); QVERIFY(explorer);
        QVERIFY(QMetaObject::invokeMethod(explorer, "mainScriptRequested", Qt::DirectConnection, Q_ARG(QString, b)));
        QCOMPARE(MainWindow::normalizeDocumentIdentity(controller->currentScriptFile()), MainWindow::normalizeDocumentIdentity(b));
        QCOMPARE(MainWindow::normalizeDocumentIdentity(controller->currentMainScriptFile()), MainWindow::normalizeDocumentIdentity(b));
        QCOMPARE(controller->runtimeConfig().dslMappings.size(), 1);
        QCOMPARE(controller->runtimeConfig().dslMappings.first().snippetId, QString("test_block"));
        QCOMPARE(controller->runtimeConfig().documentDslMappings.value("main.lh").size(), 1);
        QVERIFY(first->isModified()); QVERIFY(second->isModified());
        QVERIFY(QMetaObject::invokeMethod(&window, "onSaveAll", Qt::DirectConnection));
        QVERIFY(!first->isModified()); QVERIFY(!second->isModified());
        QFile savedA(a), savedB(b); QVERIFY(savedA.open(QIODevice::ReadOnly)); QVERIFY(savedB.open(QIODevice::ReadOnly));
        QVERIFY(savedA.readAll().contains("first ")); QVERIFY(savedB.readAll().contains("second "));
        savedA.close(); savedB.close();
        QFile savedConfig(cfg.fileName()); QVERIFY(savedConfig.open(QIODevice::ReadOnly));
        const auto reloaded = ProjectRuntimeConfig::fromJson(QJsonDocument::fromJson(savedConfig.readAll()).object());
        savedConfig.close();
        QCOMPARE(reloaded.documentDslMappings.value("main.lh").size(), 1);
        QCOMPARE(reloaded.mainScriptPath, QString("helper.lh"));
        QVERIFY(QMetaObject::invokeMethod(explorer, "mainScriptRequested", Qt::DirectConnection, Q_ARG(QString, a)));
        QCOMPARE(controller->runtimeConfig().dslMappings.size(), 1);
        QCOMPARE(controller->runtimeConfig().dslMappings.first().id, QString("main-map"));
        QCOMPARE(controller->runtimeConfig().documentDslMappings.value("helper.lh").size(), 1);
        QCOMPARE(controller->runtimeConfig().documentDslMappings.value("helper.lh").first().snippetId, QString("test_block"));
        QVERIFY(QMetaObject::invokeMethod(&window, "onSaveAll", Qt::DirectConnection));
        QTemporaryDir moved; QVERIFY(QFile::copy(a, moved.filePath("main.lh")));
        QVERIFY(QFile::copy(b, moved.filePath("helper.lh")));
        QVERIFY(QFile::copy(cfg.fileName(), moved.filePath("project_config.json")));
        ProjectController reopened;
        DslScriptEditor movedEditor;
        reopened.setDslEditor(&movedEditor);
        connect(&reopened, &ProjectController::scriptLoadRequired, &movedEditor,
                [&movedEditor](const QString& path, const QString& text) {
            movedEditor.setCurrentFilePath(path); movedEditor.setScript(text); movedEditor.setModified(false);
        });
        QVERIFY(reopened.openProjectFromPath(moved.path()));
        QCOMPARE(reopened.runtimeConfig().dslMappings.size(), 1);
        QCOMPARE(reopened.runtimeConfig().dslMappings.first().id, QString("main-map"));
        QCOMPARE(reopened.runtimeConfig().documentDslMappings.value("helper.lh").size(), 1);
        QCOMPARE(reopened.runtimeConfig().documentDslMappings.value("helper.lh").first().snippetId, QString("test_block"));
    }

    void missingEntryLeavesProjectAndBufferUnchanged()
    {
        QTemporaryDir dir;
        QFile main(dir.filePath("main.lh")); QVERIFY(main.open(QIODevice::WriteOnly));
        main.write("PROGRAM P\nEND_PROGRAM\n"); main.close();
        ProjectRuntimeConfig config; config.projectName = "entry";
        config.mainScriptPath = "main.lh"; config.dslScriptPath = "main.lh";
        config.scriptFiles = QStringList{"main.lh"};
        DslMappingEntry mapping; mapping.id = "old"; mapping.lineNumber = 2;
        config.dslMappings.append(mapping);
        QFile cfg(dir.filePath("project_config.json")); QVERIFY(cfg.open(QIODevice::WriteOnly));
        cfg.write(QJsonDocument(config.toJson()).toJson()); cfg.close();
        MainWindow window; window.show();
        auto* controller = window.findChild<ProjectController*>(); QVERIFY(controller->openProjectFromPath(dir.path()));
        auto* editor = qobject_cast<DslScriptEditor*>(window.openAndActivateFile(main.fileName())); QVERIFY(editor);
        editor->editor()->insertPlainText("unsaved "); const QString contents = editor->currentScript();
        const QString entry = controller->currentMainScriptFile();
        auto* explorer = window.findChild<ProjectExplorerWidget*>(); QVERIFY(explorer);
        QVERIFY(QMetaObject::invokeMethod(explorer, "mainScriptRequested", Qt::DirectConnection,
                                          Q_ARG(QString, dir.filePath("missing.lh"))));
        QCOMPARE(controller->currentMainScriptFile(), entry);
        QCOMPARE(controller->currentScriptFile(), entry);
        QCOMPARE(editor->currentScript(), contents); QVERIFY(editor->isModified());
        QCOMPARE(controller->runtimeConfig().dslMappings.size(), 1);
        QCOMPARE(controller->runtimeConfig().dslMappings.first().id, QString("old"));
    }

    void systemViewChangesDoNotStartOrStopMonitoring()
    {
        MainWindow window; window.show();
        auto* session = window.findChild<RuntimeSessionController*>(); QVERIFY(session);
        auto* monitor = window.findChild<MonitorWidget*>(); QVERIFY(monitor);
        auto* samples = window.findChild<SampleDataProvider*>(); QVERIFY(samples);
        const auto initialState = session->state();
        const bool monitoring = session->isMonitoring();
        const bool samplesRunning = samples->isRunning();
        QVERIFY(QMetaObject::invokeMethod(&window, "onOpenMonitor", Qt::DirectConnection));
        auto* mdi = window.findChild<QMdiArea*>(); QVERIFY(mdi && mdi->activeSubWindow());
        mdi->activeSubWindow()->close();
        window.switchToWorkspace(WorkspaceId::Device);
        QVERIFY(mdi->activeSubWindow()); mdi->activeSubWindow()->close();
        window.switchToWorkspace(WorkspaceId::Monitor);
        QCOMPARE(session->state(), initialState);
        QCOMPARE(session->isMonitoring(), monitoring);
        QCOMPARE(monitor->isMonitoring(), false);
        QCOMPARE(samples->isRunning(), samplesRunning);
    }

    void repeatedViewCyclesDoNotChangeMonitoringOrOpc()
    {
        MainWindow window; window.show();
        auto* session = window.findChild<RuntimeSessionController*>(); QVERIFY(session);
        auto* monitor = window.findChild<MonitorWidget*>(); QVERIFY(monitor);
        auto* samples = window.findChild<SampleDataProvider*>(); QVERIFY(samples);
        auto* mdi = window.findChild<QMdiArea*>(); QVERIFY(mdi);

        const auto initialState = session->state();
        const bool sessionMonitoring = session->isMonitoring();
        const bool widgetMonitoring = monitor->isMonitoring();
        const bool samplesRunning = samples->isRunning();
        const bool managerMonitoring = Monitor::MonitorManager::instance().isMonitoring();
        IOpcServer* opc = session->opcServer();
        const bool opcRunning = opc && opc->isRunning();

        QSignalSpy stateSpy(session, &RuntimeSessionController::stateChanged);
        QSignalSpy monitoringSpy(session, &RuntimeSessionController::monitoringChanged);
        QSignalSpy opcSpy(session, &RuntimeSessionController::opcRunningChanged);

        for (int i = 0; i < 3; ++i) {
            QVERIFY(QMetaObject::invokeMethod(&window, "onOpenMonitor", Qt::DirectConnection));
            QVERIFY(mdi->activeSubWindow());
            mdi->activeSubWindow()->close();
            window.switchToWorkspace(WorkspaceId::Device);
            QVERIFY(mdi->activeSubWindow());
            mdi->activeSubWindow()->close();
            window.switchToWorkspace(WorkspaceId::Programming);
            QTest::qWait(10);
            window.switchToWorkspace(WorkspaceId::Monitor);
            QTest::qWait(10);
        }

        QCOMPARE(session->state(), initialState);
        QCOMPARE(session->isMonitoring(), sessionMonitoring);
        QCOMPARE(monitor->isMonitoring(), widgetMonitoring);
        QCOMPARE(samples->isRunning(), samplesRunning);
        QCOMPARE(Monitor::MonitorManager::instance().isMonitoring(), managerMonitoring);
        if (opc) QCOMPARE(opc->isRunning(), opcRunning);
        QCOMPARE(stateSpy.count(), 0);
        QCOMPARE(monitoringSpy.count(), 0);
        QCOMPARE(opcSpy.count(), 0);
    }

    void demoMonitoringRequiresExplicitAction()
    {
        MainWindow window;
        auto* samples = window.findChild<SampleDataProvider*>(); QVERIFY(samples);
        auto* session = window.findChild<RuntimeSessionController*>(); QVERIFY(session);
        auto* action = window.findChild<QAction*>(QStringLiteral("actStartDemoMonitoring")); QVERIFY(action);
        QVERIFY(!samples->isRunning());
        QVERIFY(QMetaObject::invokeMethod(&window, "onStartMonitoring", Qt::DirectConnection));
        QVERIFY(!samples->isRunning());
        action->trigger();
        QVERIFY(samples->isRunning());
        QVERIFY(session->isDemoMode());
        QVERIFY(QMetaObject::invokeMethod(&window, "onStopMonitoring", Qt::DirectConnection));
        QVERIFY(!samples->isRunning());
        QVERIFY(!session->isDemoMode());
    }

    void demoStartRequiresOfflineIdleSession()
    {
        MainWindow window; window.show();
        auto* action = window.findChild<QAction*>(QStringLiteral("actStartDemoMonitoring"));
        QVERIFY(action);
        auto* session = window.findChild<RuntimeSessionController*>(); QVERIFY(session);
        auto* samples = window.findChild<SampleDataProvider*>(); QVERIFY(samples);
        auto& manager = Monitor::MonitorManager::instance();
        manager.stopMonitoring();

        VirtualDeviceBackend backend;
        QVERIFY(backend.connectBackend());
        manager.setDeviceBackend(&backend);
        action->trigger();
        QVERIFY(!session->isDemoMode());
        manager.setDeviceBackend(nullptr);

        session->setDeviceBackend(&backend);
        session->executeRun();
        QCOMPARE(session->state(), RuntimeSessionState::Connected);
        action->trigger();
        QVERIFY(!session->isDemoMode());
        session->requestStop();
        QCOMPARE(session->state(), RuntimeSessionState::Idle);
        session->setDeviceBackend(nullptr);
        backend.disconnectBackend();

        action->trigger();
        QVERIFY(session->isDemoMode());
        QVERIFY(samples->isRunning());
        QVERIFY(manager.hasChannel(QStringLiteral("__lh_demo__/pressure")));

        QVERIFY(QMetaObject::invokeMethod(&window, "onProjectClosed", Qt::DirectConnection));
        QVERIFY(!session->isDemoMode());
        QVERIFY(!samples->isRunning());
        QVERIFY(!manager.hasChannel(QStringLiteral("__lh_demo__/pressure")));
    }

    void discardAuxiliaryOnProjectClosePromptsOnce()
    {
        QTemporaryDir dir;
        QFile main(dir.filePath("main.lh")); QVERIFY(main.open(QIODevice::WriteOnly));
        main.write("PROGRAM P\nEND_PROGRAM\n"); main.close();
        QFile helper(dir.filePath("helper.lh")); QVERIFY(helper.open(QIODevice::WriteOnly));
        helper.write("PROGRAM H\nEND_PROGRAM\n"); helper.close();
        ProjectRuntimeConfig config; config.projectName = "close";
        config.mainScriptPath = "main.lh"; config.dslScriptPath = "main.lh";
        config.scriptFiles = QStringList{"main.lh", "helper.lh"};
        QFile cfg(dir.filePath("project_config.json")); QVERIFY(cfg.open(QIODevice::WriteOnly));
        cfg.write(QJsonDocument(config.toJson()).toJson()); cfg.close();
        MainWindow window; window.show();
        auto* controller = window.findChild<ProjectController*>(); QVERIFY(controller->openProjectFromPath(dir.path()));
        auto* editor = qobject_cast<DslScriptEditor*>(window.openAndActivateFile(helper.fileName())); QVERIFY(editor);
        editor->editor()->insertPlainText("discarded");
        QPointer<DslScriptEditor> alive(editor); int decisions = 0;
        window.setMessageBoxHook([&](const QString&, const QString&, QMessageBox::StandardButtons,
                                     QMessageBox::StandardButton) { ++decisions; return QMessageBox::Discard; });
        QVERIFY(QMetaObject::invokeMethod(&window, "onCloseProject", Qt::DirectConnection));
        QTRY_VERIFY(alive.isNull());
        QCOMPARE(decisions, 1); QVERIFY(!controller->hasOpenProject());
        QVERIFY(helper.open(QIODevice::ReadOnly)); QCOMPARE(helper.readAll(), QByteArray("PROGRAM H\nEND_PROGRAM\n"));
    }

    void f7UsesEntryWhileAnotherDocumentIsActive()
    {
        QTemporaryDir dir;
        const QString entry = dir.filePath("main.lh"), helper = dir.filePath("helper.lh");
        QFile main(entry); QVERIFY(main.open(QIODevice::WriteOnly));
        main.write("PROGRAM Broken\nVAR\n x : REAL;\nEND_VAR\nx := ;\nEND_PROGRAM\n"); main.close();
        QFile other(helper); QVERIFY(other.open(QIODevice::WriteOnly));
        other.write("PROGRAM Helper\nEND_PROGRAM\n"); other.close();
        ProjectRuntimeConfig config; config.projectName = "F7";
        config.mainScriptPath = "main.lh"; config.dslScriptPath = "main.lh";
        // The active helper is intentionally outside the build inputs. The
        // assertion below protects unrelated dirty documents from being saved
        // merely because another entry is compiled.
        config.scriptFiles = QStringList{"main.lh"};
        QFile cfg(dir.filePath("project_config.json")); QVERIFY(cfg.open(QIODevice::WriteOnly));
        cfg.write(QJsonDocument(config.toJson()).toJson()); cfg.close();
        MainWindow window; window.show();
        auto* project = window.findChild<ProjectController*>(); QVERIFY(project->openProjectFromPath(dir.path()));
        auto* active = qobject_cast<DslScriptEditor*>(window.openAndActivateFile(helper)); QVERIFY(active);
        active->editor()->insertPlainText("unsaved helper ");
        auto* build = window.findChild<BuildController*>(); QVERIFY(build);
        QSignalSpy produced(build, &BuildController::diagnosticsProduced);
        auto* compile = window.findChild<QAction*>("actCompile"); QVERIFY(compile); compile->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(produced.count() > 0, 30000);
        QCOMPARE(MainWindow::normalizeDocumentIdentity(project->currentMainScriptFile()), MainWindow::normalizeDocumentIdentity(entry));
        QVERIFY(active->isModified());
        QVERIFY(other.open(QIODevice::ReadOnly)); QCOMPARE(other.readAll(), QByteArray("PROGRAM Helper\nEND_PROGRAM\n"));
        auto* problems = window.findChild<ProblemsPanel*>(); QVERIFY(problems);
        bool entryDiagnostic = false;
        for (int row = 0; row < problems->problemCount(); ++row)
            entryDiagnostic |= MainWindow::isSameDocument(problems->itemAtRow(row).filePath, entry);
        QVERIFY(entryDiagnostic);
    }

    void problemRetentionAndDetailsAreBounded()
    {
        ProblemsPanel panel;
        DiagnosticItem item; item.source = QStringLiteral("构建"); item.severity = "error";
        item.message = QString(10000, QLatin1Char('x'));
        panel.addStructuredProblem(item);
        QVERIFY(panel.itemAtRow(0).message.size() < item.message.size());
        QFile details(panel.diagnosticDetailsPath()); QVERIFY(details.open(QIODevice::ReadOnly));
        QVERIFY(details.readAll().contains(item.message.toUtf8()));
        for (int i = 0; i < ProblemsPanel::MaxRowsPerSource + 10; ++i) {
            item.message = QString::number(i); panel.addStructuredProblem(item);
        }
        QCOMPARE(panel.problemCount(), ProblemsPanel::MaxRowsPerSource);
        QCOMPARE(panel.errorCount(), panel.problemCount());
        QCOMPARE(panel.itemAtRow(panel.problemCount() - 1).message, QString::number(ProblemsPanel::MaxRowsPerSource + 9));
    }

    void logWordsDoNotCreateDiagnostics()
    {
        MainWindow window;
        auto* panel = window.findChild<ProblemsPanel*>(); QVERIFY(panel);
        const int before = panel->problemCount();
        QVERIFY(QMetaObject::invokeMethod(&window, "onLogMessage", Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("0 errors; saved to C:/error/project.lh"))));
        QCOMPARE(panel->problemCount(), before);
    }

    void initTestCase()
    {
    }

    void cleanupTestCase()
    {
    }

    void testDiagnosticIsolationOrderAndContinuousEditing()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("main.lh");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("PROGRAM A\nEND_PROGRAM\n"); file.close();
        MainWindow window; window.show();
        auto* panel = window.findChild<ProblemsPanel*>();
        DiagnosticItem comm; comm.source = "communication"; comm.message = "retain";
        panel->addStructuredProblem(comm);
        window.onDiagnosticsProduced(10, window.projectSessionId(), {});
        QCOMPARE(panel->problemCount(), 1);
        DiagnosticItem fresh; fresh.message = "fresh";
        window.onDiagnosticsProduced(20, window.projectSessionId(), {fresh});
        DiagnosticItem stale; stale.message = "stale";
        window.onDiagnosticsProduced(19, window.projectSessionId(), {stale});
        QCOMPARE(panel->problemCount(), 2);
        QCOMPARE(panel->itemAtRow(1).message, QString("fresh"));
        auto* dsl = qobject_cast<DslScriptEditor*>(window.openAndActivateFile(path)); QVERIFY(dsl);
        dsl->editor()->insertPlainText("first");
        fresh.filePath = MainWindow::normalizeDocumentIdentity(path);
        panel->setDiagnostics({fresh});
        dsl->editor()->insertPlainText("second");
        QVERIFY(panel->itemAtRow(0).isOutdated);
        // A result arriving after content changed must be stale on arrival.
        fresh.compiledFilePath = fresh.filePath;
        fresh.compiledDocVersion = 0;
        window.onDiagnosticsProduced(21, window.projectSessionId(), {fresh});
        QVERIFY(panel->itemAtRow(panel->problemCount() - 1).isOutdated);
    }

    void testDiscardClosesWithOneDecision()
    {
        QTemporaryDir dir; QFile file(dir.filePath("notes.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write("original");file.close();
        MainWindow window;window.show();
        auto* edit = qobject_cast<QPlainTextEdit*>(window.openAndActivateFile(file.fileName())); QVERIFY(edit);
        edit->insertPlainText("modified"); QPointer<QWidget> guarded = edit;
        int decisions = 0;
        window.setMessageBoxHook([&](const QString&,const QString&,QMessageBox::StandardButtons,QMessageBox::StandardButton){++decisions;return QMessageBox::Discard;});
        window.closeCurrentActiveTab();
        QTRY_VERIFY(guarded.isNull()); QCOMPARE(decisions, 1);
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("original"));
    }

    void testRealDirectoryScanExclusionLimitAndCancellation()
    {
        QTemporaryDir dir;
        for (const QString& name : {QString("src"),QString("build_custom"),QString("node_modules")}) {
            QVERIFY(QDir().mkpath(dir.filePath(name)));
            for(int i=0;i<8;++i) {QFile f(dir.filePath(name+"/"+QString::number(i)+".lh"));QVERIFY(f.open(QIODevice::WriteOnly));}
        }
        auto cancelled = std::make_shared<QAtomicInt>(0);
        const auto files = CommandPaletteDialog::scanFiles(dir.path(), cancelled);
        QCOMPARE(files.size(), 8);
        for(const auto& path: files) QVERIFY(path.contains("/src/"));
        QCOMPARE(CommandPaletteDialog::scanFiles(dir.path(), cancelled, 3).size(), 3);
        cancelled->storeRelaxed(1);
        QVERIFY(CommandPaletteDialog::scanFiles(dir.path(), cancelled).isEmpty());
        MainWindow window;
        for(int i=0;i<15;++i) {auto* dlg=new CommandPaletteDialog(&window,CommandPaletteDialog::Mode::QuickOpen);delete dlg;}
        QTest::qWait(100);
    }

    void testRealCompilerDiagnosticsNavigateThroughPanel_data()
    {
        QTest::addColumn<QByteArray>("source");
        QTest::addColumn<int>("expectedLine");
        QTest::newRow("program") << QByteArray("PROGRAM Broken\nVAR\n x : REAL;\nEND_VAR\nx := ;\nEND_PROGRAM\n") << 5;
        QTest::newRow("leading-empty-lines") << QByteArray("\n\nPROGRAM Broken\nVAR\n x : REAL;\nEND_VAR\nx := ;\nEND_PROGRAM\n") << 7;
        QTest::newRow("wrapped-fragment") << QByteArray("x := ;\n") << 1;
        QTest::newRow("rewritten-legacy-line") << QByteArray("\nlegacy_pid = PID(Kp := );\n") << 2;
        QTest::newRow("synthetic-declaration-fallback") << QByteArray("out = VAR();\n") << -1;
    }

    void testRealCompilerDiagnosticsNavigateThroughPanel()
    {
        QTemporaryDir dir;
        const QString path=dir.filePath("main.lh");
        QFile script(path);QVERIFY(script.open(QIODevice::WriteOnly));
        QFETCH(QByteArray, source);
        QFETCH(int, expectedLine);
        script.write(source);script.close();
        ProjectRuntimeConfig cfg;cfg.projectName="DiagnosticAcceptance";cfg.mainScriptPath=path;cfg.dslScriptPath=path;cfg.scriptFiles=QStringList{path};
        QFile config(dir.filePath("project_config.json"));QVERIFY(config.open(QIODevice::WriteOnly));config.write(QJsonDocument(cfg.toJson()).toJson());config.close();
        MainWindow window;window.show();
        auto* project=window.findChild<ProjectController*>();QVERIFY(project->openProjectFromPath(dir.path()));
        auto* build=window.findChild<BuildController*>();QSignalSpy produced(build,&BuildController::diagnosticsProduced);
        build->compileConfiguration(dir.path(),project->runtimeConfig());
        QTRY_VERIFY_WITH_TIMEOUT(produced.count()>0,30000);
        auto* panel=window.findChild<ProblemsPanel*>();
        if (expectedLine < 0) {
            bool sawFallback = false;
            for (int i = 0; i < panel->problemCount(); ++i) {
                const auto item = panel->itemAtRow(i);
                if (item.message.contains(QStringLiteral("源文件位置无法可靠映射"))) {
                    sawFallback = true;
                    QVERIFY(item.filePath.isEmpty());
                    QCOMPARE(item.line, -1);
                }
            }
            QVERIFY(sawFallback);
            return;
        }
        int row=-1;for(int i=0;i<panel->problemCount();++i) if(panel->itemAtRow(i).line==expectedLine && !panel->itemAtRow(i).filePath.isEmpty()) {row=i;break;}
        QStringList observed;
        for (int i = 0; i < panel->problemCount(); ++i) {
            const auto item = panel->itemAtRow(i);
            observed.append(QStringLiteral("%1:%2 %3").arg(item.filePath).arg(item.line).arg(item.message));
        }
        QVERIFY2(row>=0, qPrintable(observed.join(QStringLiteral("; "))));
        if (qstrcmp(QTest::currentDataTag(), "rewritten-legacy-line") == 0)
            QVERIFY(!panel->itemAtRow(row).hasExactColumn);
        auto* table=panel->findChild<QTableWidget*>();QVERIFY(table);
        table->setCurrentCell(row,0);
        QTest::keyClick(table,Qt::Key_Return);
        auto* dsl=window.findChild<DslScriptEditor*>();
        QCOMPARE(dsl->editor()->textCursor().blockNumber(),expectedLine - 1);
        QVERIFY(MainWindow::isSameDocument(dsl->currentFilePath(),path));
    }

    // 1. ANTLR 列号换算与未知列号保守行定位降级，Unicode代理对与中文定位
    void testAntlrColumnBaseAndFallback()
    {
        // 1.1 ANTLR 0-based 转换为 1-based
        const QString antlrMsg = QStringLiteral("解析错误 (第 12 行, 第 5 列): mismatched input 'var'");
        DiagnosticItem item = DiagnosticParser::parseSingleMessage("error", "ANTLR", antlrMsg);
        QCOMPARE(item.line, 12);
        QCOMPARE(item.column, 6); // 0-based 5 + 1 = 6
        QVERIFY(item.hasExactColumn);
        QCOMPARE(item.severity, QStringLiteral("error"));

        // 1.2 ANTLR 0 列测试
        const QString antlrZero = QStringLiteral("错误 (第 1 行, 第 0 列): extraneous input");
        DiagnosticItem itemZero = DiagnosticParser::parseSingleMessage("error", "ANTLR", antlrZero);
        QCOMPARE(itemZero.line, 1);
        QCOMPARE(itemZero.column, 1);
        QVERIFY(itemZero.hasExactColumn);

        // 1.3 GCC 格式未知列号来源（保守降级）
        const QString gccMsg = QStringLiteral("main.lh:15:8: error: syntax error");
        DiagnosticItem gccItem = DiagnosticParser::parseSingleMessage("error", "GCC", gccMsg);
        QCOMPARE(gccItem.line, 15);
        QCOMPARE(gccItem.column, 8);
        QVERIFY(!gccItem.hasExactColumn); // 降级为行定位

        // 1.4 GCC 无列号格式
        const QString gccNoCol = QStringLiteral("main.lh:20: warning: unused variable");
        DiagnosticItem gccNoColItem = DiagnosticParser::parseSingleMessage("warning", "GCC", gccNoCol);
        QCOMPARE(gccNoColItem.line, 20);
        QCOMPARE(gccNoColItem.column, -1);
        QVERIFY(!gccNoColItem.hasExactColumn);

        // 1.5 navigateEditorPosition 中文与代理对（Emoji 🚀: UTF-16 code units 2 个）定位测试
        QPlainTextEdit edit;
        edit.setPlainText(QStringLiteral("FirstLine\n你好世界🚀Rocket\nThirdLine"));
        MainWindow window;

        // Line 2: "你"(1) "好"(2) "世"(3) "界"(4) "🚀"(5, surrogate pair, len=2) "R"(6) "o"(7)
        // 目标：第 2 行第 6 列（'R'），hasExactColumn = true
        window.navigateEditorPosition(&edit, 2, 6, true);
        QTextCursor cursor = edit.textCursor();
        QCOMPARE(cursor.blockNumber(), 1); // 0-based 对应第 2 行
        QString textBeforeCursor = edit.document()->findBlockByNumber(1).text().left(cursor.positionInBlock());
        QCOMPARE(textBeforeCursor, QStringLiteral("你好世界🚀"));

        // hasExactColumn = false：保守降级定位到行首
        window.navigateEditorPosition(&edit, 2, 6, false);
        cursor = edit.textCursor();
        QCOMPARE(cursor.blockNumber(), 1);
        QCOMPARE(cursor.positionInBlock(), 0);
    }

    // 2. 打开同一未保存文件直接激活，不弹保存，不重载覆盖
    void testOpenSameUnsavedFileReusesEditor()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString testFile = tempDir.filePath("notes.txt");
        QFile file(testFile);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("initial disk content");
        file.close();

        MainWindow window;
        window.show();
        QCoreApplication::processEvents();

        QWidget* w1 = window.openAndActivateFile(testFile);
        QVERIFY(w1 != nullptr);
        auto* edit = qobject_cast<QPlainTextEdit*>(w1);
        QVERIFY(edit != nullptr);
        QCOMPARE(edit->toPlainText(), QStringLiteral("initial disk content"));

        // 在缓冲区中修改内容
        edit->setPlainText(QStringLiteral("modified unsaved buffer"));
        w1->parentWidget()->setProperty("modified", true);

        // 再次调用 openAndActivateFile 打开同一文件
        QWidget* w2 = window.openAndActivateFile(testFile);
        QCOMPARE(w1, w2);
        // 验证没有被磁盘重载覆盖，内容仍然保留
        QCOMPARE(edit->toPlainText(), QStringLiteral("modified unsaved buffer"));
    }

    // 3. 目标文件不存在/读取失败保护，不发生行列跳转
    void testNonExistentFileTargetDoesNotNavigate()
    {
        MainWindow window;
        window.show();
        QCoreApplication::processEvents();

        QWidget* res = window.openAndActivateFile(QStringLiteral("C:/non_existent_folder_xyz_12345/missing.lh"));
        QVERIFY(res == nullptr);
    }

    // 4. 第二个脚本独立打开；取消关闭不得丢失任一缓冲区
    void testOpenDifferentFileAndCancelClosePreservesBoth()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString fileA = tempDir.filePath("file_a.lh");
        const QString fileB = tempDir.filePath("file_b.lh");

        QFile fa(fileA);
        QVERIFY(fa.open(QIODevice::WriteOnly | QIODevice::Text));
        fa.write("PROGRAM FileA\nEND_PROGRAM\n");
        fa.close();

        QFile fb(fileB);
        QVERIFY(fb.open(QIODevice::WriteOnly | QIODevice::Text));
        fb.write("PROGRAM FileB\nEND_PROGRAM\n");
        fb.close();

        MainWindow window;
        window.show();
        QCoreApplication::processEvents();

        auto* dsl = window.findChild<DslScriptEditor*>();
        QVERIFY(dsl != nullptr);
        dsl->setScript(QStringLiteral("PROGRAM FileA_Dirty\nEND_PROGRAM\n"));
        dsl->setCurrentFilePath(fileA);
        dsl->setModified(true);

        int decisions = 0;
        window.setMessageBoxHook([&](const QString&, const QString&, QMessageBox::StandardButtons, QMessageBox::StandardButton) {
            ++decisions;
            return QMessageBox::Cancel;
        });
        auto* second = qobject_cast<DslScriptEditor*>(window.openAndActivateFile(fileB));
        QVERIFY(second);
        QVERIFY(second != dsl);
        QCOMPARE(decisions, 0);
        QCOMPARE(dsl->currentScript(), QStringLiteral("PROGRAM FileA_Dirty\nEND_PROGRAM\n"));
        QVERIFY(dsl->isModified());
        second->editor()->insertPlainText(QStringLiteral("changed"));
        QVERIFY(second->isModified());
        const QString secondBuffer = second->currentScript();
        window.closeCurrentActiveTab();
        QCOMPARE(decisions, 1);
        QCOMPARE(second->currentScript(), secondBuffer);
        QVERIFY(second->isModified());
        QCOMPARE(dsl->currentScript(), QStringLiteral("PROGRAM FileA_Dirty\nEND_PROGRAM\n"));
        QVERIFY(dsl->isModified());
    }

    // 5. 扫描任务 ID 单调自增与会话变更丢弃旧结果
    void testScanTaskIdMonotonicAndSessionCheck()
    {
        MainWindow window;
        quint64 id1 = window.nextScanTaskId();
        quint64 id2 = window.nextScanTaskId();
        quint64 id3 = window.nextScanTaskId();

        // 验证单调自增
        QVERIFY(id2 > id1);
        QVERIFY(id3 > id2);

        QString session = window.projectSessionId();
        QVERIFY(!session.isEmpty());

        // 验证晚到/过期的旧会话诊断被丢弃
        auto* problems = window.findChild<ProblemsPanel*>();
        QVERIFY(problems != nullptr);
        const int prevCount = problems->problemCount();

        QList<DiagnosticItem> staleItems;
        DiagnosticItem staleItem;
        staleItem.message = QStringLiteral("Stale session error");
        staleItem.severity = QStringLiteral("error");
        staleItem.line = 1;
        staleItems.append(staleItem);

        window.onDiagnosticsProduced(1, QStringLiteral("stale-uuid-wrong"), staleItems);
        QCOMPARE(problems->problemCount(), prevCount); // 不会被错误载入
    }

    // 6. 多文件独立版本与修改后过期标记
    void testDocumentVersionPerFileAndOutdatedDiagnostics()
    {
        ProblemsPanel panel;
        const QString pathA = QStringLiteral("/workspace/proj/file_a.lh");
        const QString pathB = QStringLiteral("/workspace/proj/file_b.lh");

        DiagnosticItem itemA;
        itemA.filePath = pathA;
        itemA.line = 10;
        itemA.column = 5;
        itemA.message = QStringLiteral("Error in A");
        itemA.severity = QStringLiteral("error");

        DiagnosticItem itemB;
        itemB.filePath = pathB;
        itemB.line = 20;
        itemB.column = 8;
        itemB.message = QStringLiteral("Error in B");
        itemB.severity = QStringLiteral("error");

        panel.setDiagnostics({itemA, itemB});
        QCOMPARE(panel.problemCount(), 2);

        // 仅修改 pathA
        panel.markDiagnosticsOutdatedForFile(pathA);

        DiagnosticItem row0 = panel.itemAtRow(0);
        DiagnosticItem row1 = panel.itemAtRow(1);

        QVERIFY(row0.isOutdated);
        QVERIFY(row0.message.contains(QStringLiteral("已过期")) || row0.message.contains(QStringLiteral("Error in A")));
        QVERIFY(!row1.isOutdated); // pathB 未受影响

        // 重新全量设置诊断时清除过期标记
        panel.setDiagnostics({itemA, itemB});
        row0 = panel.itemAtRow(0);
        QVERIFY(!row0.isOutdated);
    }

    // 7. 诊断生产与双击激活端到端流程
    void testDiagnosticsProductionAndActivationFlow()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString testFile = tempDir.filePath("code.lh");
        QFile file(testFile);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("line 1\nline 2\nline 3 syntax error\nline 4\n");
        file.close();

        MainWindow window;
        window.show();
        QCoreApplication::processEvents();

        DiagnosticItem item;
        item.filePath = testFile;
        item.line = 3;
        item.column = 8;
        item.hasExactColumn = true;
        item.severity = QStringLiteral("error");
        item.message = QStringLiteral("syntax error");

        window.onDiagnosticActivated(item);
        QCoreApplication::processEvents();

        QWidget* activeEd = window.getCurrentTextEditor();
        QVERIFY(activeEd != nullptr);
        auto* dsl = qobject_cast<DslScriptEditor*>(activeEd);
        QPlainTextEdit* pe = dsl ? dsl->editor() : qobject_cast<QPlainTextEdit*>(activeEd);
        QVERIFY(pe != nullptr);
        QCOMPARE(pe->textCursor().blockNumber(), 2); // 0-based 第 3 行
    }

    // 8. 命令面板禁用动作防御、ESC恢复原焦点与确认聚焦
    void testCommandPaletteDisabledActionAndFocus()
    {
        MainWindow window;
        window.show();
        QCoreApplication::processEvents();

        // 8.1 禁用动作防御
        auto* forbiddenAct = new QAction(QStringLiteral("Forbidden Test Command"), &window);
        forbiddenAct->setEnabled(false);
        window.addAction(forbiddenAct);

        int triggerCount = 0;
        connect(forbiddenAct, &QAction::triggered, [&]() { ++triggerCount; });

        CommandPaletteDialog dlg(&window, CommandPaletteDialog::Mode::Command);
        auto* searchEdit = dlg.findChild<QLineEdit*>();
        auto* listWidget = dlg.findChild<QListWidget*>();
        QVERIFY(searchEdit != nullptr);
        QVERIFY(listWidget != nullptr);

        searchEdit->setText(QStringLiteral("Forbidden Test Command"));
        QCOMPARE(listWidget->count(), 1);
        listWidget->setCurrentRow(0);

        // 尝试按回车执行
        QTest::keyClick(&dlg, Qt::Key_Return);
        // 禁用的动作绝对不应被触发！
        QCOMPARE(triggerCount, 0);

        // 8.2 ESC 取消与焦点恢复
        auto* editor = window.findChild<DslScriptEditor*>()->editor();
        editor->setFocus();
        QTRY_VERIFY(editor->hasFocus());
        CommandPaletteDialog dlgEsc(&window, CommandPaletteDialog::Mode::Command);
        dlgEsc.show();
        QTRY_VERIFY(dlgEsc.isVisible());
        QTest::keyClick(&dlgEsc, Qt::Key_Escape);
        QCOMPARE(dlgEsc.result(), static_cast<int>(QDialog::Rejected));
        QTRY_VERIFY(editor->hasFocus());
    }

    // 9. 编辑框获焦下真实按键事件捕获 (QTest::keyClick)
    void testEditorFocusKeyClickCapture()
    {
        MainWindow window;
        window.show();
        QCoreApplication::processEvents();

        auto* dsl = window.findChild<DslScriptEditor*>();
        QVERIFY(dsl != nullptr);
        dsl->editor()->setFocus();
        QVERIFY(dsl->editor()->hasFocus());

        // 辅助 lambda：自动关闭弹出的 CommandPaletteDialog
        auto dismissPalette = []() {
            QTimer::singleShot(20, []() {
                for (QWidget* top : QApplication::topLevelWidgets()) {
                    if (auto* dlg = qobject_cast<CommandPaletteDialog*>(top)) {
                        dlg->reject();
                        return;
                    }
                }
            });
        };

        // 测试 Ctrl+G 转到行触发
        auto* actGoto = window.findChild<QAction*>("actGotoLine");
        QVERIFY(actGoto != nullptr);
        QSignalSpy spyGoto(actGoto, &QAction::triggered);
        dismissPalette();
        QTest::keyClick(dsl->editor(), Qt::Key_G, Qt::ControlModifier);
        QCOMPARE(spyGoto.count(), 1);

        // 测试 Ctrl+P 快速打开触发
        auto* actQuick = window.findChild<QAction*>("actQuickOpen");
        QVERIFY(actQuick != nullptr);
        QSignalSpy spyQuick(actQuick, &QAction::triggered);
        dismissPalette();
        QTest::keyClick(dsl->editor(), Qt::Key_P, Qt::ControlModifier);
        QCOMPARE(spyQuick.count(), 1);

        // 测试 Ctrl+W 关闭标签触发
        auto* actClose = window.findChild<QAction*>("actCloseActiveTab");
        QVERIFY(actClose != nullptr);
        QSignalSpy spyClose(actClose, &QAction::triggered);
        QTest::keyClick(dsl->editor(), Qt::Key_W, Qt::ControlModifier);
        QCOMPARE(spyClose.count(), 1);
    }

    void testAuxiliaryCloseShortcutPreservesDiscardDecision()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("notes.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("original");
        file.close();
        MainWindow window;
        window.show();
        window.switchToWorkspace(WorkspaceId::Monitor);
        QCoreApplication::processEvents();
        auto* explorerDock = window.findChild<QDockWidget*>(QStringLiteral("ExplorerDock"));
        QVERIFY(explorerDock);
        const bool explorerVisible = explorerDock->isVisible();
        const QSize explorerSize = explorerDock->size();
        auto* editor = qobject_cast<QPlainTextEdit*>(window.openAndActivateFile(file.fileName()));
        QVERIFY(editor);
        // The active content changes while the user's dock layout stays intact.
        QCOMPARE(window.currentWorkspaceId(), WorkspaceId::Programming);
        QCOMPARE(explorerDock->isVisible(), explorerVisible);
        QCOMPARE(explorerDock->size(), explorerSize);
        editor->insertPlainText("changed");
        window.activateWindow();
        editor->setFocus();
        QTRY_VERIFY(editor->hasFocus());
        QPointer<QWidget> alive(editor);
        int decisions = 0;
        window.setMessageBoxHook([&](const QString&, const QString&, QMessageBox::StandardButtons,
                                    QMessageBox::StandardButton) {
            ++decisions;
            return QMessageBox::Discard;
        });
        auto* closeAction = window.findChild<QAction*>("actCloseActiveTab");
        QVERIFY(closeAction);
        QSignalSpy triggered(closeAction, &QAction::triggered);
        QTest::keyClick(editor, Qt::Key_W, Qt::ControlModifier);
        QTRY_VERIFY(alive.isNull());
        QCOMPARE(triggered.count(), 1);
        QCOMPARE(decisions, 1);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("original"));
    }

    // 10. 保存失败确定性保护（未保存状态维持）
    void testSaveFailurePreservesModifiedState()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString roFile = tempDir.filePath("readonly.txt");
        QFile f(roFile);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("initial file content");
        f.close();

        MainWindow window;
        window.show();
        QCoreApplication::processEvents();

        QWidget* w = window.openAndActivateFile(roFile);
        QVERIFY(w != nullptr);
        auto* pe = qobject_cast<QPlainTextEdit*>(w);
        QVERIFY(pe != nullptr);
        pe->setPlainText(QStringLiteral("new modified content"));

        auto* mdi = window.findChild<QMdiArea*>();
        QVERIFY(mdi != nullptr);
        QMdiSubWindow* sub = mdi->activeSubWindow();
        QVERIFY(sub != nullptr);
        sub->setProperty("modified", true);

        // 设置只读权限
        // A directory is deterministically not a writable text-file target.
        sub->setProperty("filePath", tempDir.path());

        // 模拟关闭保存失败弹窗
        window.setMessageBoxHook([](const QString&, const QString&, QMessageBox::StandardButtons, QMessageBox::StandardButton) {
            return QMessageBox::Ok;
        });

        bool saved = window.saveAuxiliarySubWindow(sub);
        QVERIFY(!saved);
        // 保存失败：modified 状态保持为 true，防止静默丢失
        QVERIFY(sub->property("modified").toBool());
        QFile original(roFile); QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), QByteArray("initial file content"));

        // 还原权限便于临时目录清理
        QFile::setPermissions(roFile, QFile::ReadOwner | QFile::WriteOwner);
    }

    // 11. 快速打开搜索结果显示上限（真实扫描另有独立测试）
    void testQuickOpenDirectoryScanSafety()
    {
        MainWindow window;
        CommandPaletteDialog dlg(&window, CommandPaletteDialog::Mode::QuickOpen);

        // 注入文件列表，仅验证视图层上限，不代表目录扫描验收
        QStringList bigList;
        for (int i = 0; i < 6000; ++i) {
            bigList.append(QStringLiteral("src/module/file_%1.lh").arg(i));
        }
        dlg.setFileList(bigList);

        auto* searchEdit = dlg.findChild<QLineEdit*>();
        auto* listWidget = dlg.findChild<QListWidget*>();
        QVERIFY(searchEdit != nullptr);
        QVERIFY(listWidget != nullptr);

        searchEdit->setText(QStringLiteral("file_10"));
        QVERIFY(listWidget->count() > 0);
        QVERIFY(listWidget->count() <= 100); // UI 视图层安全截断不超过 100 条
    }

    // 12. 历史回归保护：FL14-01 与 FL15-01
    void testFl14AndFl15Regressions()
    {
        MainWindow window;
        window.setSettingsStorage(QStringLiteral("ServoValveTest"), QStringLiteral("Test_Rob12"));
        window.switchToWorkspace(WorkspaceId::Programming);
        window.resize(1366, 768);
        window.show();
        QTest::qWait(150);

        auto* mdi = window.findChild<QMdiArea*>();
        auto* dsl = window.findChild<DslScriptEditor*>();
        auto* actToggleDsl = window.findChild<QAction*>("actToggleDslEditor");
        auto* editorSubWindow = qobject_cast<QMdiSubWindow*>(dsl->parentWidget());
        QVERIFY(mdi != nullptr);
        QVERIFY(dsl != nullptr);
        QVERIFY(actToggleDsl != nullptr);
        QVERIFY(editorSubWindow != nullptr);

        // FL15-01 验证：主 DSL 编辑器关闭/隐藏后重新打开不白屏，可见性与焦点恢复
        editorSubWindow->close();
        QTest::qWait(100);
        QVERIFY(!dsl->isVisible());

        actToggleDsl->setChecked(true);
        QTest::qWait(100);
        QVERIFY(editorSubWindow->isVisible());
        QVERIFY(dsl->isVisible());
        QVERIFY(!dsl->isHidden());
        QVERIFY(dsl->editor() != nullptr);
        QVERIFY(dsl->editor()->isVisible());

        // FL14-01 验证：关闭未保存辅助文件时，若取消则中止关闭
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString auxPath = tempDir.filePath("aux_file.txt");
        QFile f(auxPath);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("aux text");
        f.close();

        QWidget* auxWidget = window.openAndActivateFile(auxPath);
        QVERIFY(auxWidget != nullptr);
        QMdiSubWindow* auxSub = mdi->activeSubWindow();
        QVERIFY(auxSub != nullptr);
        auxSub->setProperty("modified", true);

        // 模拟用户点击取消
        window.setMessageBoxHook([](const QString&, const QString&, QMessageBox::StandardButtons, QMessageBox::StandardButton) {
            return QMessageBox::Cancel;
        });

        window.closeCurrentActiveTab();
        // 中止关闭：窗口依然存在且处于激活状态
        QCOMPARE(mdi->activeSubWindow(), auxSub);
    }
};

int main(int argc, char* argv[])
{
    if (qEnvironmentVariable("QT_QPA_PLATFORM").isEmpty()) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QApplication app(argc, argv);
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    DesignerP1RobustnessTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "designer_p1_robustness_test.moc"
