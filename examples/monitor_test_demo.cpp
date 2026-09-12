#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QTimer>
#include <QDebug>
#include <QDateTime>

// 简单的监控测试类
class MonitorTestDemo : public QMainWindow {
    Q_OBJECT

public:
    MonitorTestDemo(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("监控功能测试演示");
        resize(1200, 800);
        
        // 创建菜单栏
        createMenus();
        
        // 创建状态栏
        statusBar()->showMessage("监控功能测试就绪");
        
        // 创建监控窗口（模拟）
        createMonitorWindow();
        
        // 启动数据更新定时器
        m_updateTimer = new QTimer(this);
        connect(m_updateTimer, &QTimer::timeout, this, &MonitorTestDemo::updateMonitorData);
        m_updateTimer->start(1000); // 每秒更新一次
    }

private slots:
    void onStartMonitor() {
        qDebug() << "开始监控...";
        statusBar()->showMessage("监控已启动 - " + QDateTime::currentDateTime().toString("hh:mm:ss"));
        m_monitoring = true;
    }
    
    void onStopMonitor() {
        qDebug() << "停止监控...";
        statusBar()->showMessage("监控已停止 - " + QDateTime::currentDateTime().toString("hh:mm:ss"));
        m_monitoring = false;
    }
    
    void updateMonitorData() {
        if (!m_monitoring) return;
        
        // 模拟数据更新
        double pressure = 50.0 + (qrand() % 100) / 10.0;
        double flow = 20.0 + (qrand() % 50) / 5.0;
        double temperature = 25.0 + (qrand() % 20) / 2.0;
        
        qDebug() << QString("压力: %1 bar, 流量: %2 L/min, 温度: %3 °C")
                    .arg(pressure, 0, 'f', 1)
                    .arg(flow, 0, 'f', 1)
                    .arg(temperature, 0, 'f', 1);
    }

private:
    void createMenus() {
        QMenu *monitorMenu = menuBar()->addMenu("监控(&M)");
        
        QAction *startAction = monitorMenu->addAction("开始监控(&S)");
        startAction->setShortcut(QKeySequence("F5"));
        connect(startAction, &QAction::triggered, this, &MonitorTestDemo::onStartMonitor);
        
        QAction *stopAction = monitorMenu->addAction("停止监控(&T)");
        stopAction->setShortcut(QKeySequence("Shift+F5"));
        connect(stopAction, &QAction::triggered, this, &MonitorTestDemo::onStopMonitor);
        
        monitorMenu->addSeparator();
        
        QAction *exitAction = monitorMenu->addAction("退出(&X)");
        connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
    }
    
    void createMonitorWindow() {
        // 创建监控窗口（简化版本）
        QDockWidget *monitorDock = new QDockWidget("实时监控", this);
        monitorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        
        QWidget *monitorWidget = new QWidget();
        monitorDock->setWidget(monitorWidget);
        
        addDockWidget(Qt::RightDockWidgetArea, monitorDock);
    }
    
    QTimer *m_updateTimer;
    bool m_monitoring = false;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    MonitorTestDemo demo;
    demo.show();
    
    return app.exec();
}

#include "monitor_test_demo.moc"