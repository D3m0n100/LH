#include "OpcServerSettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
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
    return cfg;
}

void OpcServerSettingsDialog::createUi()
{
    auto* root = new QVBoxLayout(this);

    auto* classicGroup = new QGroupBox(QStringLiteral("Classic OPC / Modbus"));
    auto* classicForm = new QFormLayout(classicGroup);

    m_enabledCheck = new QCheckBox(QStringLiteral("Enable OPC"), classicGroup);
    classicForm->addRow(QString(), m_enabledCheck);

    m_channelEdit = new QLineEdit(classicGroup);
    classicForm->addRow(QStringLiteral("Channel"), m_channelEdit);

    m_deviceEdit = new QLineEdit(classicGroup);
    classicForm->addRow(QStringLiteral("Device"), m_deviceEdit);

    m_modeEdit = new QLineEdit(classicGroup);
    classicForm->addRow(QStringLiteral("Mode"), m_modeEdit);

    m_timeoutSpin = new QSpinBox(classicGroup);
    m_timeoutSpin->setRange(1, 600000);
    classicForm->addRow(QStringLiteral("Timeout(ms)"), m_timeoutSpin);

    m_reconnectDelaySpin = new QSpinBox(classicGroup);
    m_reconnectDelaySpin->setRange(0, 600000);
    classicForm->addRow(QStringLiteral("Reconnect Delay(ms)"), m_reconnectDelaySpin);

    m_retriesSpin = new QSpinBox(classicGroup);
    m_retriesSpin->setRange(0, 100);
    classicForm->addRow(QStringLiteral("Retries"), m_retriesSpin);

    m_maxRegistersSpin = new QSpinBox(classicGroup);
    m_maxRegistersSpin->setRange(1, 125);
    classicForm->addRow(QStringLiteral("Max Registers"), m_maxRegistersSpin);

    m_publishIntervalSpin = new QSpinBox(classicGroup);
    m_publishIntervalSpin->setRange(10, 60000);
    m_publishIntervalSpin->setSingleStep(10);
    classicForm->addRow(QStringLiteral("Publish Interval(ms)"), m_publishIntervalSpin);

    m_rootDescriptionEdit = new QLineEdit(classicGroup);
    classicForm->addRow(QStringLiteral("Root Description"), m_rootDescriptionEdit);

    m_classicServerNameEdit = new QLineEdit(classicGroup);
    classicForm->addRow(QStringLiteral("Server Name"), m_classicServerNameEdit);

    auto* exposeGroup = new QGroupBox(QStringLiteral("Point Exposure"));
    auto* exposeLayout = new QHBoxLayout(exposeGroup);

    m_exposeVariablesCheck = new QCheckBox(QStringLiteral("Variables"), exposeGroup);
    m_exposeParametersCheck = new QCheckBox(QStringLiteral("Parameters"), exposeGroup);
    m_exposeStatusCheck = new QCheckBox(QStringLiteral("Status"), exposeGroup);
    m_exposeAlarmsCheck = new QCheckBox(QStringLiteral("Alarms"), exposeGroup);

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
            *errorMessage = QStringLiteral("Channel cannot be empty when OPC is enabled.");
        }
        return false;
    }

    if (cfg.enabled && cfg.deviceName.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Device cannot be empty when OPC is enabled.");
        }
        return false;
    }

    if (cfg.enabled && cfg.serialMode.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Mode cannot be empty when OPC is enabled.");
        }
        return false;
    }

    if (cfg.timeoutMs <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Timeout must be greater than 0.");
        }
        return false;
    }

    if (cfg.reconnectDelayMs < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Reconnect delay must be greater than or equal to 0.");
        }
        return false;
    }

    if (cfg.retries < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Retries must be greater than or equal to 0.");
        }
        return false;
    }

    if (cfg.maxRegistersPerRequest <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Max registers must be greater than 0.");
        }
        return false;
    }

    if (cfg.publishIntervalMs <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Publish interval must be greater than 0.");
        }
        return false;
    }

    return true;
}

void OpcServerSettingsDialog::onAccept()
{
    QString errorMessage;
    if (!validateConfig(&errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("Invalid Config"), errorMessage);
        return;
    }

    accept();
}

