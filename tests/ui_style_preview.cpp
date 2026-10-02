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
#include "designer/ui/InspectorPanel.h"
#include "designer/ui/ThemeManager.h"

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    if(app.arguments().size()!=2) return 1;
    QDir output(app.arguments().at(1)); if(!output.exists()) return 2;
    for(const auto mode:{ThemeManager::ThemeMode::Light,ThemeManager::ThemeMode::Dark}) {
        ThemeManager::applyTheme(&app,mode);
        const auto suffix=mode==ThemeManager::ThemeMode::Light ? "light" : "dark";
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
                || !capture(error,"error",{620,200})) return 3;
    }
    return 0;
}
