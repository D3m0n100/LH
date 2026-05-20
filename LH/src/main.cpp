/**
 * @file main.cpp
 * @brief 应用程序入口
 *
 * 优化内容：
 * - 为应用程序设置窗口图标
 */

#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QTimer>
#include <QTextCodec>
#include <QIcon>
#include "designer/MainWindow.h"
#include "core/DataManager.h"
#include "core/TaskScheduler.h"
#include "monitor/MonitorManager.h"
#include "Common.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LH");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("DUT");

    // 设置中文支持
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    // 设置应用程序图标（使用资源文件中的图标）
    QIcon appIcon(":/icons/app_icon.png");
    app.setWindowIcon(appIcon);

    // 启动画面（可选）
    QSplashScreen splash(QPixmap(400, 300));
    splash.showMessage("正在初始化平台...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    splash.show();
    app.processEvents();

    LOG_INFO("平台启动中...");

    // 初始化数据管理器
    QString dbPath = QCoreApplication::applicationDirPath() + "/../data/platform.db";
    if (!DataManager::instance().initialize(dbPath)) {
        LOG_ERROR("数据库初始化失败！");
        return -1;
    }

    // 初始化任务调度器
    TaskScheduler::instance().start();
    LOG_INFO("任务调度器已启动");
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        Monitor::MonitorManager::instance().stopMonitoring();
        Monitor::MonitorManager::instance().flushDatabaseLogging();
        TaskScheduler::instance().shutdown();
        DataManager::instance().shutdown();
    });

    // 创建主窗口
    MainWindow mainWindow;
    mainWindow.setWindowTitle("LH v1.0.0");
    mainWindow.resize(1280, 720);

    // 为主窗口也设置图标（确保在任务栏等位置显示正确）
    mainWindow.setWindowIcon(appIcon);

    // 延迟关闭启动画面
    QTimer::singleShot(1500, &splash, &QWidget::close);
    QTimer::singleShot(1500, &mainWindow, &QWidget::show);

    LOG_INFO("平台启动成功！");

    return app.exec();
}
