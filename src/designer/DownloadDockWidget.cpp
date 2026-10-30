// 文件：src/designer/DownloadDockWidget.cpp

#include "DownloadDockWidget.h"

#include <QComboBox>
#include <QDir>
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
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QUuid>

#include "communication/DownloadManager.h"
#include "DownloadConfirmation.h"

namespace {
QString fileSha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file) || file.error() != QFileDevice::NoError) return QString();
    return QString::fromLatin1(hash.result().toHex());
}

DownloadConfirmationSnapshot expertDownloadSnapshot(const QString& profile,
                                                     const QString& payload,
                                                     const QVariantMap& config,
                                                     const QString& operationId)
{
    DownloadConfirmationSnapshot snapshot;
    snapshot.operationId = operationId;
    const QVariantMap comm = config.value(QStringLiteral("comm")).toMap();
    const QVariantMap bridge = comm.value(QStringLiteral("bridge")).toMap();
    const int controllerId = bridge.value(QStringLiteral("controller")).toMap()
            .value(QStringLiteral("slaveId")).toInt();
    const int targetId = bridge.value(QStringLiteral("target")).toMap()
            .value(QStringLiteral("deviceId")).toInt();
    snapshot.targetName = QStringLiteral("专家指定目标");
    snapshot.targetIdentity = QStringLiteral("控制器站号 %1，目标站号 %2")
            .arg(controllerId).arg(targetId);
    snapshot.endpoint = QStringLiteral("%1 %2，%3 bps，%4N%5%6")
            .arg(comm.value(QStringLiteral("protocol")).toString(),
                 comm.value(QStringLiteral("mode")).toString(),
                 comm.value(QStringLiteral("baudRate")).toString(),
                 comm.value(QStringLiteral("dataBits")).toString(),
                 comm.value(QStringLiteral("parity")).toString(),
                 comm.value(QStringLiteral("stopBits")).toString());
    snapshot.endpoint += QStringLiteral("，端口 %1；通信配置摘要 %2")
            .arg(comm.value(QStringLiteral("port")).toString(),
                 QString::fromLatin1(QCryptographicHash::hash(
                     QJsonDocument::fromVariant(config).toJson(QJsonDocument::Compact),
                     QCryptographicHash::Sha256).toHex()));
    snapshot.projectName = QStringLiteral("专家诊断下载（外部文件）");
    snapshot.entryPath = QFileInfo(profile).absoluteFilePath();
    snapshot.artifactPath = QFileInfo(payload).absoluteFilePath();
    snapshot.artifactBytes = QFileInfo(payload).size();
    const QString profileHash = fileSha256(profile);
    const QString payloadHash = fileSha256(payload);
    snapshot.artifactChecksum = profileHash.isEmpty() || payloadHash.isEmpty()
            ? QString() : profileHash + QLatin1Char(':') + payloadHash;
    snapshot.versionEvidence = QStringLiteral("未知（无设备程序版本读回）");
    snapshot.action = QStringLiteral("专家诊断直接下载");
    return snapshot;
}
}

DownloadDockWidget::DownloadDockWidget(QWidget* parent)
    : QWidget(parent)
{
    m_port = new QComboBox(this);
    m_port->setObjectName(QStringLiteral("cbPort"));
    m_port->setEditable(true);
    for (const auto& info : QSerialPortInfo::availablePorts()) {
        m_port->addItem(info.portName());
    }

    m_baud = new QComboBox(this);
    m_baud->setObjectName(QStringLiteral("cbBaud"));
    m_baud->addItems({QStringLiteral("1200"), QStringLiteral("2400"), QStringLiteral("4800"),
                      QStringLiteral("9600"), QStringLiteral("19200"), QStringLiteral("38400"),
                      QStringLiteral("57600"), QStringLiteral("115200")});
    m_baud->setCurrentText(QStringLiteral("115200"));

    m_targetId = new QSpinBox(this);
    m_targetId->setObjectName(QStringLiteral("sbTargetId"));
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
    m_log->document()->setMaximumBlockCount(2000);
    m_log->setToolTip(QStringLiteral("保留最近 2000 行日志；达到上限后会裁剪较早内容"));
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
    connect(m_mgr, &DownloadManager::busyChanged, this, [this](bool busy) {
        if (m_busy == busy) return;
        m_busy = busy;
        emit busyChanged(busy);
    });
    connect(m_mgr, &DownloadManager::statusChanged, this, [this](DownloadManager::State, const QString& msg) {
        m_status->setText(QStringLiteral("状态：%1").arg(msg));
    });
    connect(m_mgr, &DownloadManager::progressChanged, this, [this](int percent, int, int, int, int) {
        m_prog->setValue(percent);
    });
    connect(m_mgr, &DownloadManager::errorOccurred, this, [this](DownloadManager::ErrorCode c, const QString& msg, const QString& det) {
        appendLog(QStringLiteral("[错误] code=%1 msg=%2 details=%3").arg(int(c)).arg(msg, det));
    });

    connect(m_port, &QComboBox::currentTextChanged, this, &DownloadDockWidget::targetConfigChanged);
    connect(m_targetId, QOverload<int>::of(&QSpinBox::valueChanged), this, &DownloadDockWidget::targetConfigChanged);
}

void DownloadDockWidget::appendLog(const QString& s)
{
    QTextCursor cursor(m_log->document());
    cursor.movePosition(QTextCursor::End);
    if (!m_log->document()->isEmpty()) cursor.insertBlock();
    cursor.insertText(s);
    m_log->setTextCursor(cursor);
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

    const QVariantMap config = buildCommConfig();
    const QString operationId = QUuid::createUuid().toString();
    const DownloadConfirmationSnapshot approvedSnapshot = expertDownloadSnapshot(
                profile, payload, config, operationId);
    if (approvedSnapshot.artifactChecksum.isEmpty()) {
        appendLog(QStringLiteral("[界面] 无法读取外部配置或载荷摘要，已阻止下载。"));
        return;
    }
    if (!m_confirmHandler) {
        appendLog(QStringLiteral("[界面] 未配置下载确认处理器，已阻止下载。"));
        return;
    }
    appendLog(QStringLiteral("[界面] 等待下载确认..."));
    if (!m_confirmHandler(approvedSnapshot)) {
        appendLog(QStringLiteral("[界面] 用户已取消专家诊断下载。"));
        return;
    }

    if (expertDownloadSnapshot(profile, payload, buildCommConfig(), operationId).fingerprint()
            != approvedSnapshot.fingerprint()) {
        appendLog(QStringLiteral("[界面] 专家下载批准快照已变化（目标、端点或文件内容改变），已取消。"));
        return;
    }

    appendLog(QStringLiteral("[界面] 用户确认专家诊断下载，开始下载..."));
    m_mgr->setConfig(config);
    const QStringList hashes = approvedSnapshot.artifactChecksum.split(QLatin1Char(':'));
    if (hashes.size() != 2) {
        appendLog(QStringLiteral("[界面] 批准快照不完整，已阻止下载。"));
        return;
    }
    m_mgr->startDownload(profile, payload, hashes.at(0), hashes.at(1));
}

QString DownloadDockWidget::selectedPort() const
{
    return m_port ? m_port->currentText().trimmed() : QString();
}

void DownloadDockWidget::setPort(const QString& port)
{
    if (m_port) {
        m_port->setEditText(port);
    }
}

int DownloadDockWidget::baudRate() const
{
    return m_baud ? m_baud->currentText().toInt() : 115200;
}

int DownloadDockWidget::targetStationId() const
{
    return m_targetId ? m_targetId->value() : 2;
}

void DownloadDockWidget::setTargetStationId(int id)
{
    if (m_targetId) {
        m_targetId->setValue(id);
    }
}
