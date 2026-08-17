#include "OpcServerSettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>

OpcServerSettingsDialog::OpcServerSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("OPC 服务设置"));
    setModal(true);
    setMinimumWidth(520);
    createUi();
}

void OpcServerSettingsDialog::setConfig(const OpcServerConfig& config)
{
    m_enabledCheck->setChecked(config.enabled);
    m_publishIntervalSpin->setValue(config.publishIntervalMs);
    m_exposeVariablesCheck->setChecked(config.exposeVariables);
    m_exposeParametersCheck->setChecked(config.exposeParameters);
    m_exposeStatusCheck->setChecked(config.exposeStatus);
    m_exposeAlarmsCheck->setChecked(config.exposeAlarms);
    m_channelEdit->setText(config.channelName);
    m_deviceEdit->setText(config.deviceName);
    m_modeEdit->setText(config.serialMode);
    m_timeoutSpin->setValue(config.timeoutMs);
    m_reconnectDelaySpin->setValue(config.reconnectDelayMs);
    m_retriesSpin->setValue(config.retries);
    m_maxRegistersSpin->setValue(config.maxRegistersPerRequest);
    m_rootDescriptionEdit->setText(config.rootDescription);
    m_classicServerNameEdit->setText(config.classicServerName);
    m_opcProgIdEdit->setText(config.opcProgId);
}

OpcServerConfig OpcServerSettingsDialog::config() const
{
    OpcServerConfig cfg;
    cfg.enabled = m_enabledCheck->isChecked();
    cfg.publishIntervalMs = m_publishIntervalSpin->value();
    cfg.exposeVariables = m_exposeVariablesCheck->isChecked();
    cfg.exposeParameters = m_exposeParametersCheck->isChecked();
    cfg.exposeStatus = m_exposeStatusCheck->isChecked();
    cfg.exposeAlarms = m_exposeAlarmsCheck->isChecked();
    cfg.channelName = m_channelEdit->text().trimmed();
    cfg.deviceName = m_deviceEdit->text().trimmed();
    cfg.serialMode = m_modeEdit->text().trimmed();
    cfg.timeoutMs = m_timeoutSpin->value();
    cfg.reconnectDelayMs = m_reconnectDelaySpin->value();
    cfg.retries = m_retriesSpin->value();
    cfg.maxRegistersPerRequest = m_maxRegistersSpin->value();
    cfg.rootDescription = m_rootDescriptionEdit->text().trimmed();
    cfg.classicServerName = m_classicServerNameEdit->text().trimmed();
    cfg.opcProgId = m_opcProgIdEdit->text().trimmed();
    return cfg;
}

void OpcServerSettingsDialog::createUi()
{
    auto* root = new QVBoxLayout(this);

    auto* classicGroup = new QGroupBox(QStringLiteral("Matrikon OPC DA / Modbus"));
    auto* classicForm = new QFormLayout(classicGroup);

    m_enabledCheck = new QCheckBox(QStringLiteral("启用 OPC"), classicGroup);
    classicForm->addRow(QString(), m_enabledCheck);

    m_channelEdit = new QLineEdit(classicGroup);
    m_channelEdit->setPlaceholderText(QStringLiteral("例如：CommPort"));
    classicForm->addRow(QStringLiteral("通道"), m_channelEdit);

    m_deviceEdit = new QLineEdit(classicGroup);
    m_deviceEdit->setPlaceholderText(QStringLiteral("例如：COM1"));
    classicForm->addRow(QStringLiteral("设备端口"), m_deviceEdit);

    m_modeEdit = new QLineEdit(classicGroup);
    m_modeEdit->setPlaceholderText(QStringLiteral("例如：19200,N,8,1"));
    classicForm->addRow(QStringLiteral("串口模式"), m_modeEdit);

    m_timeoutSpin = new QSpinBox(classicGroup);
    m_timeoutSpin->setRange(1, 600000);
    classicForm->addRow(QStringLiteral("超时(ms)"), m_timeoutSpin);

    m_reconnectDelaySpin = new QSpinBox(classicGroup);
    m_reconnectDelaySpin->setRange(0, 600000);
    classicForm->addRow(QStringLiteral("重连延迟(ms)"), m_reconnectDelaySpin);

    m_retriesSpin = new QSpinBox(classicGroup);
    m_retriesSpin->setRange(0, 100);
    classicForm->addRow(QStringLiteral("重试次数"), m_retriesSpin);

    m_maxRegistersSpin = new QSpinBox(classicGroup);
    m_maxRegistersSpin->setRange(1, 125);
    classicForm->addRow(QStringLiteral("单次最大寄存器数"), m_maxRegistersSpin);

    m_publishIntervalSpin = new QSpinBox(classicGroup);
    m_publishIntervalSpin->setRange(10, 60000);
    m_publishIntervalSpin->setSingleStep(10);
    classicForm->addRow(QStringLiteral("发布周期(ms)"), m_publishIntervalSpin);

    m_rootDescriptionEdit = new QLineEdit(classicGroup);
    classicForm->addRow(QStringLiteral("根节点描述"), m_rootDescriptionEdit);

    m_classicServerNameEdit = new QLineEdit(classicGroup);
    classicForm->addRow(QStringLiteral("服务名称"), m_classicServerNameEdit);

    m_opcProgIdEdit = new QLineEdit(classicGroup);
    classicForm->addRow(QStringLiteral("OPC ProgID"), m_opcProgIdEdit);

    auto* exposeGroup = new QGroupBox(QStringLiteral("点位暴露"));
    auto* exposeLayout = new QHBoxLayout(exposeGroup);

    m_exposeVariablesCheck = new QCheckBox(QStringLiteral("变量"), exposeGroup);
    m_exposeParametersCheck = new QCheckBox(QStringLiteral("参数"), exposeGroup);
    m_exposeStatusCheck = new QCheckBox(QStringLiteral("状态"), exposeGroup);
    m_exposeAlarmsCheck = new QCheckBox(QStringLiteral("报警"), exposeGroup);

    exposeLayout->addWidget(m_exposeVariablesCheck);
    exposeLayout->addWidget(m_exposeParametersCheck);
    exposeLayout->addWidget(m_exposeStatusCheck);
    exposeLayout->addWidget(m_exposeAlarmsCheck);
    exposeLayout->addStretch(1);

    root->addWidget(classicGroup);
    root->addWidget(exposeGroup);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &OpcServerSettingsDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

bool OpcServerSettingsDialog::validateConfig(QString* errorMessage) const
{
    const OpcServerConfig cfg = config();

    if (cfg.enabled && cfg.channelName.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("启用 OPC 时通道不能为空。");
        }
        return false;
    }

    if (cfg.enabled && cfg.deviceName.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("启用 OPC 时设备端口不能为空。");
        }
        return false;
    }

    if (cfg.enabled && cfg.serialMode.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("启用 OPC 时串口模式不能为空。");
        }
        return false;
    }

    if (cfg.enabled) {
        const QRegularExpression modePattern(
                    QStringLiteral("^\\s*\\d+\\s*,\\s*(N|E|O|M|S|None|Even|Odd|Mark|Space)\\s*,\\s*[5-8]\\s*,\\s*[12]\\s*$"),
                    QRegularExpression::CaseInsensitiveOption);
        if (!modePattern.match(cfg.serialMode).hasMatch()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("串口模式格式无效，应为 19200,N,8,1 这类格式。");
            }
            return false;
        }
    }

    if (cfg.enabled && cfg.opcProgId.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("启用 OPC 时 ProgID 不能为空。");
        }
        return false;
    }

    if (cfg.timeoutMs <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("超时时间必须大于 0。");
        }
        return false;
    }

    if (cfg.reconnectDelayMs < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("重连延迟必须大于等于 0。");
        }
        return false;
    }

    if (cfg.retries < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("重试次数必须大于等于 0。");
        }
        return false;
    }

    if (cfg.maxRegistersPerRequest <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("单次最大寄存器数必须大于 0。");
        }
        return false;
    }

    if (cfg.publishIntervalMs <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("发布周期必须大于 0。");
        }
        return false;
    }

    return true;
}

void OpcServerSettingsDialog::onAccept()
{
    QString errorMessage;
    if (!validateConfig(&errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("配置无效"), errorMessage);
        return;
    }

    accept();
}

