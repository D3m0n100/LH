#ifndef OPCSERVERSETTINGSDIALOG_H
#define OPCSERVERSETTINGSDIALOG_H

#include <QDialog>

#include "common/ConfigTypes.h"

class QCheckBox;
class QLineEdit;
class QSpinBox;

class OpcServerSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OpcServerSettingsDialog(QWidget* parent = nullptr);
    ~OpcServerSettingsDialog() override = default;

    void setConfig(const OpcServerConfig& config);
    OpcServerConfig config() const;

private:
    bool validateConfig(QString* errorMessage = nullptr) const;
    void createUi();
    void onAccept();

    QCheckBox* m_enabledCheck = nullptr;
    QSpinBox* m_publishIntervalSpin = nullptr;
    QCheckBox* m_exposeVariablesCheck = nullptr;
    QCheckBox* m_exposeParametersCheck = nullptr;
    QCheckBox* m_exposeStatusCheck = nullptr;
    QCheckBox* m_exposeAlarmsCheck = nullptr;
    QLineEdit* m_channelEdit = nullptr;
    QLineEdit* m_deviceEdit = nullptr;
    QLineEdit* m_modeEdit = nullptr;
    QSpinBox* m_timeoutSpin = nullptr;
    QSpinBox* m_reconnectDelaySpin = nullptr;
    QSpinBox* m_retriesSpin = nullptr;
    QSpinBox* m_maxRegistersSpin = nullptr;
    QLineEdit* m_rootDescriptionEdit = nullptr;
    QLineEdit* m_classicServerNameEdit = nullptr;
};

#endif // OPCSERVERSETTINGSDIALOG_H
