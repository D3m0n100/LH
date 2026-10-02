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
#include <QDir>
#include <QMessageBox>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QProgressDialog>
#include <QEventLoop>
#include <QThread>
#include <atomic>
#include "designer/MainWindow.h"
#include "core/AppLogging.h"
#include "core/DataManager.h"
#include "core/BackgroundTaskRegistry.h"
#include <cstdlib>
#include "monitor/MonitorManager.h"
#include "Common.h"
#include "common/DeferredThreadCleanup.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LH");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("DUT");

    QCommandLineParser parser;
    parser.setApplicationDescription("LH - DSP Visual Programming Platform");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption smokeTestOption(
        QStringLiteral("smoke-test"),
        QStringLiteral("运行快速无头自检并退出（供 CI 与部署验证）"));
    parser.addOption(smokeTestOption);

    parser.process(app);

    const bool loggingAvailable = AppLogging::install();
    QString loggingError;
    if (!loggingAvailable) {
        // install() has already emitted a direct stderr diagnostic; keep the
        // startup failure visible through the normal Qt logging path as well.
        LOG_WARN("持久化日志不可用，将回退到 stderr");
        loggingError = AppLogging::lastError();
    }

    if (parser.isSet(smokeTestOption)) {
        LOG_INFO("执行无头自检 (smoke-test)...");
        const QString dbPath = DataManager::defaultDatabasePath();
        if (dbPath.isEmpty()) {
            LOG_ERROR("Smoke test 失败: 无法确定应用数据目录");
            AppLogging::shutdown();
            return 1;
        }
        if (!DataManager::instance().initialize(dbPath)) {
            LOG_ERROR("Smoke test 失败: 数据库初始化失败");
            AppLogging::shutdown();
            return 1;
        }
        DataManager::instance().shutdown();
        LOG_INFO("Smoke test 成功完成");
        AppLogging::shutdown();
        return 0;
    }

    // 设置中文支持

    // 设置应用程序图标（使用资源文件中的图标）
    QIcon appIcon(":/icons/app_icon.png");
    app.setWindowIcon(appIcon);

    // 启动画面（可选）
    QSplashScreen splash(QPixmap(400, 300));
    splash.showMessage("正在初始化平台...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    splash.show();
    app.processEvents();

    LOG_INFO("平台启动中...");

    // 初始化数据管理器：运行期数据只写入平台用户数据目录。
    const QString dbPath = DataManager::defaultDatabasePath();
    if (dbPath.isEmpty()) {
        LOG_ERROR("无法确定用户可写的应用数据目录");
        AppLogging::shutdown();
        return -1;
    }
    const QString legacyDbPath = QDir::cleanPath(
        QDir(QCoreApplication::applicationDirPath()).filePath("../data/platform.db"));
    // SQL objects are created, used and closed in the startup worker; no connection crosses threads.
    bool databaseReady = false;
    QString databaseError;
    std::atomic_bool migrationCancelled{false};
    QProgressDialog databaseProgress(QStringLiteral("正在检查和升级数据库…"), QStringLiteral("取消"), 0, 0, &splash);
    databaseProgress.setWindowModality(Qt::NonModal);
    databaseProgress.setMinimumDuration(250);
    QObject::connect(&databaseProgress, &QProgressDialog::canceled,
        [&] { migrationCancelled.store(true); });
    QEventLoop startupLoop;
    auto* startupThread = QThread::create([&] {
        databaseReady = DataManager::prepareDatabase(dbPath, legacyDbPath, &migrationCancelled,
            [&](const QString& table, qint64 count) {
                QMetaObject::invokeMethod(&databaseProgress, [&, table, count] {
                    databaseProgress.setLabelText(QStringLiteral("正在升级数据库：%1，已处理 %2 条").arg(table).arg(count));
                }, Qt::QueuedConnection);
            }, &databaseError);
    });
    QObject::connect(startupThread, &QThread::finished, &startupLoop, &QEventLoop::quit);
    startupThread->start();
    startupLoop.exec();
    startupThread->wait();
    delete startupThread;
    databaseProgress.close();
    if (!databaseReady || !DataManager::instance().initialize(dbPath)) {
        LOG_ERROR("数据库初始化失败: " + databaseError);
        if (!migrationCancelled.load()) QMessageBox::critical(&splash, QStringLiteral("数据库升级失败"), databaseError);
        AppLogging::shutdown();
        return -1;
    }

    QObject::connect(&Monitor::MonitorManager::instance(),
                     &Monitor::MonitorManager::databaseServiceStarted, &app,
                     [&app](bool ready, const QString& error) {
        if (!ready) {
            qCritical() << "Cannot start asynchronous history database service:" << error;
            app.exit(-1);
        }
    });
    Monitor::MonitorManager::instance().startDatabaseServiceAsync(dbPath);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        Core::BackgroundTaskRegistry::instance().cancelAndWait(3000);
        Monitor::MonitorManager::instance().shutdown();
        DataManager::instance().shutdown();
    });

    // 创建主窗口
    int exitCode = 0;
    {
        MainWindow mainWindow;
        mainWindow.setWindowTitle("LH v1.0.0");
        mainWindow.resize(1280, 720);

        // 为主窗口也设置图标（确保在任务栏等位置显示正确）
        mainWindow.setWindowIcon(appIcon);

        // 延迟关闭启动画面
        QTimer::singleShot(1500, &splash, &QWidget::close);
        QTimer::singleShot(1500, &mainWindow, &QWidget::show);
        if (!loggingAvailable) {
            QTimer::singleShot(1500, &mainWindow,
                               [&mainWindow, loggingError]() {
                                   auto* messageBox = new QMessageBox(
                                       QMessageBox::Warning,
                                       QStringLiteral("日志"),
                                       QStringLiteral("持久化日志不可用：%1").arg(loggingError),
                                       QMessageBox::Ok,
                                       &mainWindow);
                                   messageBox->setModal(false);
                                   messageBox->setAttribute(Qt::WA_DeleteOnClose);
                                   messageBox->open();
                               });
        }

        LOG_INFO("平台启动成功！");

        exitCode = app.exec();
        if (!Core::BackgroundTaskRegistry::instance().cancelAndWait(0)) {
            // An uninterruptible OS file call cannot safely outlive Qt global teardown.
            qCritical("历史导出在退出期限后仍未结束；退出进程以保护全局资源及原目标文件");
            AppLogging::shutdown();
            std::_Exit(2);
        }
    }
    if (!DeferredThreadCleanup::drain()) {
        qCritical("OPC 原生调用在退出期限后仍未结束；退出进程以保护 Qt 全局资源");
        AppLogging::shutdown();
        std::_Exit(2);
    }
    AppLogging::shutdown();
    return exitCode;
}
