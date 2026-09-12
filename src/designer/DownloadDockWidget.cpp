// 文件：src/designer/DownloadDockWidget.cpp

#include "DownloadDockWidget.h"

#include <QComboBox>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

#include "communication/DownloadManager.h"

DownloadDockWidget::DownloadDockWidget(QWidget* parent)
    : QWidget(parent)
{
    m_port = new QComboBox(this);
    for (const auto& info : QSerialPortInfo::availablePorts()) {
        m_port->addItem(info.portName());
    }

    m_baud = new QComboBox(this);
    m_baud->addItems({QStringLiteral("1200"), QStringLiteral("2400"), QStringLiteral("4800"),
                      QStringLiteral("9600"), QStringLiteral("19200"), QStringLiteral("38400"),
                      QStringLiteral("57600"), QStringLiteral("115200")});
    m_baud->setCurrentText(QStringLiteral("115200"));

    m_targetId = new QSpinBox(this);
    m_targetId->setRange(1, 247);
    m_targetId->setValue(2);

    m_btnConnectProbe = new QPushButton(QStringLiteral("连接并探测"), this);
    m_btnDownload = new QPushButton(QStringLiteral("诊断下载 (专家模式)"), this);

    m_status = new QLabel(QStringLiteral("状态：-"), this);
    m_prog = new QProgressBar(this);
    m_prog->setRange(0, 100);
    m_prog->setTextVisible(true);

    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setPlaceholderText(QStringLiteral("下载过程日志会显示在这里"));

    auto* commGroup = new QGroupBox(QStringLiteral("通信参数"), this);
    auto* commLayout = new QFormLayout(commGroup);
    commLayout->setContentsMargins(10, 10, 10, 10);
    commLayout->setHorizontalSpacing(12);
    commLayout->setVerticalSpacing(8);
    commLayout->addRow(QStringLiteral("端口"), m_port);
    commLayout->addRow(QStringLiteral("波特率"), m_baud);
    commLayout->addRow(QStringLiteral("目标 ID"), m_targetId);

    auto* actionGroup = new QGroupBox(QStringLiteral("操作"), this);
    auto* actionLayout = new QHBoxLayout(actionGroup);
    actionLayout->setContentsMargins(10, 10, 10, 10);
    actionLayout->setSpacing(8);
    actionLayout->addWidget(m_btnConnectProbe);
    actionLayout->addWidget(m_btnDownload);
    actionLayout->addStretch();

    auto* statusGroup = new QGroupBox(QStringLiteral("状态日志"), this);
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
        m_status->setText(QStringLiteral("状态：%1").arg(msg));
    });
    connect(m_mgr, &DownloadManager::progressChanged, this, [this](int percent, int, int, int, int) {
        m_prog->setValue(percent);
    });
    connect(m_mgr, &DownloadManager::errorOccurred, this, [this](DownloadManager::ErrorCode c, const QString& msg, const QString& det) {
        appendLog(QStringLiteral("[错误] code=%1 msg=%2 details=%3").arg(int(c)).arg(msg, det));
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
        {QStringLiteral("enableHandshake"), true},
        {QStringLiteral("enableTargetProbe"), true},
        {QStringLiteral("controller"), QVariantMap{{QStringLiteral("slaveId"), 1}}},
        {QStringLiteral("target"), QVariantMap{{QStringLiteral("deviceId"), target}}},
        {QStringLiteral("addressing"), QVariantMap{{QStringLiteral("mode"), QStringLiteral("TargetAsSlaveId")}}},
        {QStringLiteral("handshake"), QVariantMap{{QStringLiteral("slaveId"), 1}, {QStringLiteral("address"), 38}, {QStringLiteral("count"), 1}}},
        {QStringLiteral("targetProbe"), QVariantMap{{QStringLiteral("slaveId"), target}, {QStringLiteral("address"), 38}, {QStringLiteral("count"), 1}}}
    };

    QVariantMap comm {
        {QStringLiteral("protocol"), QStringLiteral("MODBUS")},
        {QStringLiteral("mode"), QStringLiteral("RTU")},
        {QStringLiteral("type"), QStringLiteral("Master")},
        {QStringLiteral("port"), m_port->currentText()},
        {QStringLiteral("baudRate"), baud},
        {QStringLiteral("parity"), QStringLiteral("None")},
        {QStringLiteral("dataBits"), 8},
        {QStringLiteral("stopBits"), 1},
        {QStringLiteral("address"), 1},
        {QStringLiteral("responseTimeout"), 300},
        {QStringLiteral("retryCount"), 3},
        {QStringLiteral("enableBridge"), true},
        {QStringLiteral("bridge"), bridge}
    };

    return QVariantMap{{QStringLiteral("comm"), comm}};
}

void DownloadDockWidget::onConnectProbe()
{
    appendLog(QStringLiteral("[界面] 开始连接并探测..."));
    m_mgr->setConfig(buildCommConfig());
    m_mgr->startConnectProbe();
}

void DownloadDockWidget::onDownload()
{
    const QString profile = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择下载配置 JSON"),
        QDir::currentPath(),
        QStringLiteral("JSON 文件 (*.json)"));
    if (profile.isEmpty()) {
        return;
    }

    const QString payload = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择下载载荷文件"),
        QDir::currentPath(),
        QStringLiteral("所有文件 (*.*)"));
    if (payload.isEmpty()) {
        return;
    }

    const QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        QStringLiteral("专家诊断下载风险确认"),
        QStringLiteral("警告：当前为「诊断下载（专家模式）」，该操作将直接下载外部指定的配置文件和载荷，"
                       "绕过正式的工程编译检查、generation manifest 校验与源码 SHA-256 关联。\n\n"
                       "仅限现场排障或专家诊断使用。是否确认继续执行下载？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        appendLog(QStringLiteral("[界面] 用户已取消专家诊断下载。"));
        return;
    }

    appendLog(QStringLiteral("[界面] 用户确认专家诊断下载，开始下载..."));
    m_mgr->setConfig(buildCommConfig());
    m_mgr->startDownload(profile, payload);
}
