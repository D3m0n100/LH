// 文件: src/designer/DownloadDockWidget.cpp

#include "DownloadDockWidget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSerialPortInfo>
#include <QFileDialog>
#include <QDir>

#include "communication/DownloadManager.h"

DownloadDockWidget::DownloadDockWidget(QWidget* parent)
    : QWidget(parent)
{
    m_port = new QComboBox(this);
    for (const auto& info : QSerialPortInfo::availablePorts()) {
        m_port->addItem(info.portName());
    }

    m_baud = new QComboBox(this);
    m_baud->addItems({"1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"});
    m_baud->setCurrentText("115200");

    m_targetId = new QSpinBox(this);
    m_targetId->setRange(1, 247);
    m_targetId->setValue(2);

    m_btnConnectProbe = new QPushButton("连接并探测", this);
    m_btnDownload = new QPushButton("开始下载", this);

    m_status = new QLabel("状态: -", this);
    m_prog = new QProgressBar(this);
    m_prog->setRange(0, 100);
    m_prog->setTextVisible(true);

    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setPlaceholderText("下载过程日志会显示在这里");

    auto* commGroup = new QGroupBox("通信参数", this);
    auto* commLayout = new QFormLayout(commGroup);
    commLayout->setContentsMargins(10, 10, 10, 10);
    commLayout->setHorizontalSpacing(12);
    commLayout->setVerticalSpacing(8);
    commLayout->addRow("端口", m_port);
    commLayout->addRow("波特率", m_baud);
    commLayout->addRow("目标 ID", m_targetId);

    auto* actionGroup = new QGroupBox("操作", this);
    auto* actionLayout = new QHBoxLayout(actionGroup);
    actionLayout->setContentsMargins(10, 10, 10, 10);
    actionLayout->setSpacing(8);
    actionLayout->addWidget(m_btnConnectProbe);
    actionLayout->addWidget(m_btnDownload);
    actionLayout->addStretch();

    auto* statusGroup = new QGroupBox("状态日志", this);
    auto* statusLayout = new QVBoxLayout(statusGroup);
    statusLayout->setContentsMargins(10, 10, 10, 10);
    statusLayout->setSpacing(8);
    statusLayout->addWidget(m_status);
    statusLayout->addWidget(m_prog);
    statusLayout->addWidget(m_log, 1);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);
    lay->addWidget(commGroup);
    lay->addWidget(actionGroup);
    lay->addWidget(statusGroup, 1);
    setLayout(lay);

    m_mgr = new DownloadManager(this);

    connect(m_btnConnectProbe, &QPushButton::clicked, this, &DownloadDockWidget::onConnectProbe);
    connect(m_btnDownload, &QPushButton::clicked, this, &DownloadDockWidget::onDownload);

    connect(m_mgr, &DownloadManager::logLine, this, &DownloadDockWidget::appendLog);
    connect(m_mgr, &DownloadManager::statusChanged, this, [this](DownloadManager::State, const QString& msg) {
        m_status->setText("状态: " + msg);
    });
    connect(m_mgr, &DownloadManager::progressChanged, this, [this](int percent, int, int, int, int) {
        m_prog->setValue(percent);
    });
    connect(m_mgr, &DownloadManager::errorOccurred, this, [this](DownloadManager::ErrorCode c, const QString& msg, const QString& det) {
        appendLog(QString("[ERROR] code=%1 msg=%2 details=%3").arg(int(c)).arg(msg).arg(det));
    });
}

void DownloadDockWidget::appendLog(const QString& s)
{
    m_log->append(s);
}

QVariantMap DownloadDockWidget::buildCommConfig() const
{
    const int baud = m_baud->currentText().toInt();
    const int target = m_targetId->value();

    QVariantMap bridge {
        {"enableHandshake", true},
        {"enableTargetProbe", true},
        {"controller", QVariantMap{{"slaveId", 1}}},
        {"target", QVariantMap{{"deviceId", target}}},
        {"addressing", QVariantMap{{"mode", "TargetAsSlaveId"}}},
        // 注意：以下地址仅为示例，请按现场协议填写。
        {"handshake", QVariantMap{{"slaveId", 1}, {"address", 38}, {"count", 1}}},
        {"targetProbe", QVariantMap{{"slaveId", target}, {"address", 38}, {"count", 1}}}
    };

    QVariantMap comm {
        {"protocol", "MODBUS"},
        {"mode", "RTU"},
        {"type", "Master"},
        {"port", m_port->currentText()},
        {"baudRate", baud},
        {"parity", "None"},
        {"dataBits", 8},
        {"stopBits", 1},
        {"address", 1},
        {"responseTimeout", 300},
        {"retryCount", 3},
        {"enableBridge", true},
        {"bridge", bridge}
    };

    return QVariantMap{{"comm", comm}};
}

void DownloadDockWidget::onConnectProbe()
{
    appendLog("[界面] 开始连接并探测...");
    m_mgr->setConfig(buildCommConfig());
    m_mgr->startConnectProbe();
}

void DownloadDockWidget::onDownload()
{
    const QString profile = QFileDialog::getOpenFileName(
        this, "选择下载配置 JSON", QDir::currentPath(), "JSON 文件 (*.json)");
    if (profile.isEmpty()) {
        return;
    }

    const QString payload = QFileDialog::getOpenFileName(
        this, "选择下载载荷文件", QDir::currentPath(), "所有文件 (*.*)");
    if (payload.isEmpty()) {
        return;
    }

    appendLog("[界面] 开始下载...");
    m_mgr->setConfig(buildCommConfig());
    m_mgr->startDownload(profile, payload);
}
