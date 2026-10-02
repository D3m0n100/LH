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
#include <QSignalSpy>
#include <QTimer>
#include <QTextCursor>
#include <QTextBlock>
#include <QTableWidget>
#include <QSettings>
#include <QJsonDocument>
#include "designer/BuildController.h"

#include "designer/MainWindow.h"
#include "designer/ProjectController.h"
#include "designer/DslScriptEditor.h"
#include "designer/ui/DiagnosticItem.h"
#include "designer/ui/DiagnosticParser.h"
#include "designer/ui/ProblemsPanel.h"
#include "designer/ui/CommandPaletteDialog.h"

class DesignerP1RobustnessTest : public QObject
{
    Q_OBJECT

private slots:
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

    // 4. 打开不同文件取消保存返回 nullptr，编辑器内容与光标不变
    void testOpenDifferentFileCancelAborts()
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

        // 打开 fileB 时会弹窗提示未保存修改。注入 Hook 模拟用户选择 Cancel
        window.setMessageBoxHook([](const QString&, const QString&, QMessageBox::StandardButtons, QMessageBox::StandardButton) {
            return QMessageBox::Cancel;
        });

        QWidget* res = window.openAndActivateFile(fileB);
        // 取消后中止打开，返回 nullptr
        QVERIFY(res == nullptr);
        // 且主编辑器仍然保留修改内容
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
        auto* editor = qobject_cast<QPlainTextEdit*>(window.openAndActivateFile(file.fileName()));
        QVERIFY(editor);
        QCOMPARE(window.currentWorkspaceId(), WorkspaceId::Programming);
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
        auto* editorSubWindow = window.findChild<QMdiSubWindow*>();
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
