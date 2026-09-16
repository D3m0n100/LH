#ifndef DEVICE_WORKSPACE_WIDGET_H
#define DEVICE_WORKSPACE_WIDGET_H

#include <QWidget>

class QLabel;
class QPushButton;
class QToolButton;
class QGroupBox;
class QAction;
class DownloadDockWidget;

class DeviceWorkspaceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceWorkspaceWidget(QWidget* parent = nullptr);
    ~DeviceWorkspaceWidget() override;

    void setTargetInfo(const QString& targetName, const QString& configSource, const QString& address,
                       const QString& expertTarget = QString(), const QString& downloadProfile = QString());
    QLabel* targetNameValue() const { return m_targetNameValue; }
    QLabel* addressValueLabel() const { return m_addressValue; }
    QLabel* expertTargetValue() const { return m_expertAddressValue; }
    QLabel* downloadProfileValue() const { return m_downloadProfileValue; }
    QString targetValue() const;
    QString addressValue() const;
    QString expertTargetText() const;
    QString downloadProfileText() const;
    void setConnectionStatus(bool connected, const QString& statusText);
    void setControllerRunning(bool running, bool paused = false);

    void bindActions(QAction* actTest, QAction* actRun, QAction* actStop,
                     QAction* actPause, QAction* actResume, QAction* actStep,
                     QAction* actDiagnosis = nullptr);

    bool isExpertDiagnosticVisible() const;
    void setExpertDiagnosticVisible(bool visible);

    DownloadDockWidget* downloadWidget() const { return m_downloadWidget; }
    QPushButton* toggleExpertButton() const { return m_toggleExpertButton; }

    QToolButton* btnTestConnection() const { return m_btnTestConnection; }
    QToolButton* btnRunController() const { return m_btnRunController; }
    QToolButton* btnStopController() const { return m_btnStopController; }
    QToolButton* btnPauseController() const { return m_btnPauseController; }
    QToolButton* btnResumeController() const { return m_btnResumeController; }
    QToolButton* btnStepController() const { return m_btnStepController; }
    QToolButton* btnDiagnosisWizard() const { return m_btnDiagnosisWizard; }

signals:
    void requestTestConnection();
    void requestRunController();
    void requestStopController();
    void requestPauseController();
    void requestResumeController();
    void requestStepController();
    void requestDiagnosisWizard();

private:
    void applyBadgeStyle(QLabel* label, const QString& visualState);

private:
    QLabel* m_targetNameValue = nullptr;
    QLabel* m_configSourceValue = nullptr;
    QLabel* m_addressValue = nullptr;
    QLabel* m_expertAddressValue = nullptr;
    QLabel* m_downloadProfileValue = nullptr;
    QLabel* m_connectionBadge = nullptr;
    QLabel* m_controllerBadge = nullptr;

    QToolButton* m_btnTestConnection = nullptr;
    QToolButton* m_btnRunController = nullptr;
    QToolButton* m_btnStopController = nullptr;
    QToolButton* m_btnPauseController = nullptr;
    QToolButton* m_btnResumeController = nullptr;
    QToolButton* m_btnStepController = nullptr;
    QToolButton* m_btnDiagnosisWizard = nullptr;

    QWidget* m_expertContainer = nullptr;
    QPushButton* m_toggleExpertButton = nullptr;
    DownloadDockWidget* m_downloadWidget = nullptr;
};

#endif // DEVICE_WORKSPACE_WIDGET_H
