#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QTimer>
#include <QPixmap>
#include <QAction>
#include <QMessageBox>
#include <QLayout>
#include "monitor/ChartWidget.h"
#include "designer/SettingsDialog.h"
#include "designer/DeviceWorkspaceWidget.h"
#include "designer/DslScriptEditor.h"
#include "designer/ProgramBlocksWidget.h"
#include "designer/SnippetRepository.h"
#include <QTreeWidget>
#include "designer/ui/InspectorPanel.h"
#include "designer/ui/ThemeManager.h"

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    if(app.arguments().size()!=2) return 1;
    QDir output(app.arguments().at(1)); if(!output.exists()) return 2;
    for(const auto mode:{ThemeManager::ThemeMode::Light,ThemeManager::ThemeMode::Dark}) {
        ThemeManager::applyTheme(&app,mode);
        const auto suffix=mode==ThemeManager::ThemeMode::Light ? "light" : "dark";
        DslScriptEditor editor;
        editor.setFunctionListVisible(true);
        ProgramBlocksWidget blocks;
        QList<FunctionSnippet> fbList;
        fbList.append({"FB18_TSO", QStringLiteral("FB18: TSO (伺服控制)"), "_tso.py", QStringLiteral("电液伺服控制专用功能块"), ""});
        fbList.append({"FB19_TSOAutoTune", QStringLiteral("FB19: TSOAutoTune (自整定)"), "_tso.py", QStringLiteral("伺服参数在线自整定算法"), ""});
        fbList.append({"FB08_TaskPeriodic", QStringLiteral("FB08: TaskPeriodic (周期调度)"), "_task.py", QStringLiteral("高精度周期性任务调度器"), ""});
        fbList.append({"FB09_Lock", QStringLiteral("FB09: Lock (互斥锁)"), "_task.py", QStringLiteral("临界区并发互斥保护锁"), ""});
        fbList.append({"FB01_KeyScan", QStringLiteral("FB01: KeyScan (按键扫描)"), "_display.py", QStringLiteral("工业面板按键去抖与状态读取"), ""});
        fbList.append({"FB02_M600Text", QStringLiteral("FB02: M600Text (屏显输出)"), "_display.py", QStringLiteral("M600 工业液晶屏文本输出"), ""});
        fbList.append({"FB07_FilterBW", QStringLiteral("FB07: FilterBW (二阶巴特沃斯滤波)"), "_filter.py", QStringLiteral("信号低通降噪滤波算法"), ""});
        fbList.append({"PID_Controller", QStringLiteral("PID: DualLoopPID (双闭环控制)"), "_control.py", QStringLiteral("电液伺服位置与压力解耦PID"), ""});
        blocks.setSnippets(fbList);
        if (blocks.treeWidget()) {
            blocks.treeWidget()->expandAll();
        }
        editor.setScript(QStringLiteral(
            "// LH 伺服阀控制平台 - 电液伺服位置与压力双闭环控制主程序\n"
            "PROGRAM Main\n"
            "VAR\n"
            "    targetPosition : REAL := 45.0; // 目标位置指令 / mm\n"
            "    actualPosition : REAL := 0.0;  // LVDT 传感器位移反馈采样\n"
            "    targetPressure : REAL := 32.0; // 负载敏感压力设定 / bar\n"
            "    actualPressure : REAL := 0.0;  // 压力传感器实时采样\n"
            "    valveOutput    : REAL := 0.0;  // 伺服阀驱动输出电流 / mA\n"
            "    systemReady    : BOOL := TRUE;\n"
            "END_VAR\n\n"
            "// 电液伺服双闭环控制主循环\n"
            "IF systemReady THEN\n"
            "    valveOutput := PID_Controller(targetPosition, actualPosition, 3.25, 0.42, 0.08);\n"
            "    DRV_AO_Channel01 := valveOutput;\n"
            "    DRV_DO_StatusLed := TRUE;\n"
            "ELSE\n"
            "    DRV_AO_Channel01 := 0.0;\n"
            "    DRV_DO_StatusLed := FALSE;\n"
            "END_IF;\n"
            "END_PROGRAM\n"
        ));
        SettingsDialog settings; settings.setDefaultProjectDir(QStringLiteral("D:/工程/ServoValve/LH"));
        InspectorPanel inspector; inspector.setProjectPath(QStringLiteral("D:/工程/ServoValve/LH"));
        inspector.setCurrentFile(QStringLiteral("main.lh")); inspector.setWorkspaceName(QStringLiteral("控制器调试"));
        inspector.setSelectedObject(QStringLiteral("变量"),QStringLiteral("压力反馈"),{{QStringLiteral("数据类型"),"REAL"},{QStringLiteral("单位"),"bar"}});
        DeviceWorkspaceWidget device; device.setTargetInfo("F2812","project_config.json","COM7","F2812","download_profile.json");
        ChartWidget chart; chart.addChannelSeries("pressure", QStringLiteral("压力反馈 / bar"));
        // The offscreen plugin has no native OpenGL context; render the same series in software.
        for (auto* series : chart.chart()->series()) series->setUseOpenGL(false);
        const auto now = QDateTime::currentMSecsSinceEpoch();
        for (int i = 0; i < 100; ++i) chart.appendPoint("pressure", QPointF(now - 10000 + i * 100, 45 + (i % 20)));
        QMessageBox error(QMessageBox::Critical, QStringLiteral("编译失败"),
            QStringLiteral("无法生成下载产物：第 12 行存在未知类型。\n请修正后重新编译。"), QMessageBox::Ok);
        // Qt 5's Windows system-menu path requires an HWND, unavailable in offscreen capture.
        error.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        device.setConnectionStatus(true,QStringLiteral("已连接"));
        QAction test(QIcon(":/icons/refresh.svg"),QStringLiteral("连接测试"),&device);
        QAction run(QIcon(":/icons/run.svg"),QStringLiteral("运行"),&device);
        QAction stop(QIcon(":/icons/stop.svg"),QStringLiteral("停止"),&device);
        QAction pause(QIcon(":/icons/stop.svg"),QStringLiteral("暂停"),&device);
        QAction resume(QIcon(":/icons/run.svg"),QStringLiteral("继续"),&device);
        QAction step(QIcon(":/icons/run.svg"),QStringLiteral("单步"),&device);
        QAction diagnosis(QIcon(":/icons/settings.svg"),QStringLiteral("诊断"),&device);
        device.bindActions(&test,&run,&stop,&pause,&resume,&step,&diagnosis);
        auto capture=[&](QWidget& widget,const QString& name,QSize size) {
            widget.resize(size);
            // QMessageBox::showEvent unconditionally accesses the Windows system menu in Qt 5.
            // Render its real content while hidden; physical window chrome is a field check.
            if (qobject_cast<QMessageBox*>(&widget)) { widget.ensurePolished(); widget.layout()->activate(); }
            else { widget.show(); app.processEvents(); }
            const bool saved=widget.grab().save(output.filePath(name+"-"+suffix+".png")); widget.hide(); return saved;
        };
        if(!capture(settings,"settings",{660,420}) || !capture(inspector,"inspector",{600,800})
                || !capture(device,"device",{850,850}) || !capture(chart,"chart",{900,420})
                || !capture(editor,"editor",{960,560})
                || !capture(blocks,"function_blocks",{480,720})
                || !capture(error,"error",{620,200})) return 3;
    }
    return 0;
}
