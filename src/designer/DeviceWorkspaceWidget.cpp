#include "ui/ThemeManager.h"
#include "DeviceWorkspaceWidget.h"
#include "DownloadDockWidget.h"

#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QAction>
#include <QScrollArea>
#include <QStyle>
#include <QIcon>

namespace {

QLabel* makeCaptionLabel(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral("color: #57606a; font-weight: bold;"));
    return lbl;
}

QLabel* makeValueLabel(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral("color: #24292f;"));
    return lbl;
}

QLabel* makeBadge(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setMinimumHeight(24);
    lbl->setMinimumWidth(72);
    return lbl;
}

} // namespace

DeviceWorkspaceWidget::DeviceWorkspaceWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("DeviceWorkspaceWidget"));

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(12);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    rootLayout->addWidget(scrollArea);

    auto* content = new QWidget(scrollArea);
    content->setObjectName(QStringLiteral("DeviceWorkspaceContent"));
    scrollArea->setWidget(content);

    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // 1. 设备概览分组
    auto* overviewGroup = new QGroupBox(tr("设备目标与连接概览"), content);
    overviewGroup->setStyleSheet(ThemeManager::groupBoxStyleSheet() + QStringLiteral("QGroupBox { font-weight: bold; }"));
    auto* overviewLayout = new QGridLayout(overviewGroup);
    overviewLayout->setContentsMargins(10, 8, 10, 10);
    overviewLayout->setHorizontalSpacing(16);
    overviewLayout->setVerticalSpacing(8);

    overviewLayout->addWidget(makeCaptionLabel(tr("项目目标设备:"), overviewGroup), 0, 0);
    m_targetNameValue = makeValueLabel(tr("未配置 / 离线模式"), overviewGroup);
    overviewLayout->addWidget(m_targetNameValue, 0, 1);

    overviewLayout->addWidget(makeCaptionLabel(tr("配置来源:"), overviewGroup), 0, 2);
    m_configSourceValue = makeValueLabel(tr("默认配置"), overviewGroup);
    overviewLayout->addWidget(m_configSourceValue, 0, 3);

    overviewLayout->addWidget(makeCaptionLabel(tr("项目连接目标:"), overviewGroup), 1, 0);
    m_addressValue = makeValueLabel(tr("未连接 (离线)"), overviewGroup);
    m_addressValue->setToolTip(tr("用于测试连接、控制器运行与调试；程序下载使用独立 Profile 或下方专家诊断"));
    overviewLayout->addWidget(m_addressValue, 1, 1);

    overviewLayout->addWidget(makeCaptionLabel(tr("连接状态:"), overviewGroup), 1, 2);
    m_connectionBadge = makeBadge(tr("离线"), overviewGroup);
    applyBadgeStyle(m_connectionBadge, QStringLiteral("neutral"));
    overviewLayout->addWidget(m_connectionBadge, 1, 3);

    overviewLayout->addWidget(makeCaptionLabel(tr("程序下载目标:"), overviewGroup), 2, 0);
    m_downloadProfileValue = makeValueLabel(tr("未配置下载Profile（仅离线编译）"), overviewGroup);
    overviewLayout->addWidget(m_downloadProfileValue, 2, 1);

    overviewLayout->addWidget(makeCaptionLabel(tr("控制器运行:"), overviewGroup), 2, 2);
    m_controllerBadge = makeBadge(tr("空闲"), overviewGroup);
    applyBadgeStyle(m_controllerBadge, QStringLiteral("neutral"));
    overviewLayout->addWidget(m_controllerBadge, 2, 3);

    overviewLayout->addWidget(makeCaptionLabel(tr("专家诊断目标:"), overviewGroup), 3, 0);
    m_expertAddressValue = makeValueLabel(tr("未展开 / 未配置"), overviewGroup);
    overviewLayout->addWidget(m_expertAddressValue, 3, 1);

    overviewLayout->setColumnStretch(1, 1);
    overviewLayout->setColumnStretch(3, 1);
    layout->addWidget(overviewGroup);

    // 2. 常规控制器操作分组
    auto* opsGroup = new QGroupBox(tr("常规控制器操作"), content);
    opsGroup->setStyleSheet(ThemeManager::groupBoxStyleSheet() + QStringLiteral("QGroupBox { font-weight: bold; }"));
    // Keep the controls reachable at narrow logical widths. A single
    // horizontal row made the whole workspace request more than 1,000px.
    auto* opsLayout = new QGridLayout(opsGroup);
    opsLayout->setContentsMargins(10, 8, 10, 10);
    opsLayout->setSpacing(8);

    m_btnTestConnection = new QToolButton(opsGroup);
    m_btnTestConnection->setObjectName(QStringLiteral("DeviceTestConnectionButton"));
    m_btnTestConnection->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnTestConnection->setMinimumHeight(30);
    connect(m_btnTestConnection, &QToolButton::clicked, this, [this]() {
        if (!m_btnTestConnection->defaultAction()) {
            emit requestTestConnection();
        }
    });
    opsLayout->addWidget(m_btnTestConnection, 0, 0);

    m_btnRunController = new QToolButton(opsGroup);
    m_btnRunController->setObjectName(QStringLiteral("DeviceRunControllerButton"));
    m_btnRunController->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnRunController->setMinimumHeight(30);
    connect(m_btnRunController, &QToolButton::clicked, this, [this]() {
        if (!m_btnRunController->defaultAction()) {
            emit requestRunController();
        }
    });
    opsLayout->addWidget(m_btnRunController, 0, 1);

    m_btnStopController = new QToolButton(opsGroup);
    m_btnStopController->setObjectName(QStringLiteral("DeviceStopControllerButton"));
    m_btnStopController->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnStopController->setMinimumHeight(30);
    connect(m_btnStopController, &QToolButton::clicked, this, [this]() {
        if (!m_btnStopController->defaultAction()) {
            emit requestStopController();
        }
    });
    opsLayout->addWidget(m_btnStopController, 0, 2);

    m_btnPauseController = new QToolButton(opsGroup);
    m_btnPauseController->setObjectName(QStringLiteral("DevicePauseControllerButton"));
    m_btnPauseController->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnPauseController->setMinimumHeight(30);
    connect(m_btnPauseController, &QToolButton::clicked, this, [this]() {
        if (!m_btnPauseController->defaultAction()) {
            emit requestPauseController();
        }
    });
    opsLayout->addWidget(m_btnPauseController, 1, 0);

    m_btnResumeController = new QToolButton(opsGroup);
    m_btnResumeController->setObjectName(QStringLiteral("DeviceResumeControllerButton"));
    m_btnResumeController->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnResumeController->setMinimumHeight(30);
    connect(m_btnResumeController, &QToolButton::clicked, this, [this]() {
        if (!m_btnResumeController->defaultAction()) {
            emit requestResumeController();
        }
    });
    opsLayout->addWidget(m_btnResumeController, 1, 1);

    m_btnStepController = new QToolButton(opsGroup);
    m_btnStepController->setObjectName(QStringLiteral("DeviceStepControllerButton"));
    m_btnStepController->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnStepController->setMinimumHeight(30);
    connect(m_btnStepController, &QToolButton::clicked, this, [this]() {
        if (!m_btnStepController->defaultAction()) {
            emit requestStepController();
        }
    });
    opsLayout->addWidget(m_btnStepController, 1, 2);

    m_btnDiagnosisWizard = new QToolButton(opsGroup);
    m_btnDiagnosisWizard->setObjectName(QStringLiteral("DeviceDiagnosisWizardButton"));
    m_btnDiagnosisWizard->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnDiagnosisWizard->setMinimumHeight(30);
    connect(m_btnDiagnosisWizard, &QToolButton::clicked, this, [this]() {
        if (!m_btnDiagnosisWizard->defaultAction()) {
            emit requestDiagnosisWizard();
        }
    });
    opsLayout->addWidget(m_btnDiagnosisWizard, 0, 3, 2, 1);
    opsLayout->setColumnStretch(3, 1);
    layout->addWidget(opsGroup);

    // 3. 专家诊断折叠分组
    auto* expertHeader = new QWidget(content);
    auto* expertHeaderLayout = new QHBoxLayout(expertHeader);
    expertHeaderLayout->setContentsMargins(4, 4, 4, 4);
    expertHeaderLayout->setSpacing(8);

    auto* expertTitle = new QLabel(tr("专家诊断与手动下载 (专家模式)"), expertHeader);
    expertTitle->setStyleSheet(QStringLiteral("font-weight: bold; color: #57606a; font-size: 13px;"));
    expertHeaderLayout->addWidget(expertTitle);

    auto* expertTip = new QLabel(tr("— 底层串口直接探测与诊断下载，不执行项目编译逻辑校验"), expertHeader);
    expertTip->setStyleSheet(QStringLiteral("color: #8c959f; font-size: 11px;"));
    expertHeaderLayout->addWidget(expertTip);
    expertHeaderLayout->addStretch();

    m_toggleExpertButton = new QPushButton(tr("展开专家诊断"), expertHeader);
    m_toggleExpertButton->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    m_toggleExpertButton->setObjectName(QStringLiteral("ToggleExpertDiagnosticButton"));
    m_toggleExpertButton->setCheckable(true);
    m_toggleExpertButton->setChecked(false);
    m_toggleExpertButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #f6f8fa; border: 1px solid #d0d7de; border-radius: 4px; padding: 4px 10px; color: #24292f; }"
        "QPushButton:hover { background: #eaeef2; }"
        "QPushButton:checked { background: #ddf4ff; border-color: #54aeff; color: #0969da; }"
    ));
    connect(m_toggleExpertButton, &QPushButton::toggled, this, &DeviceWorkspaceWidget::setExpertDiagnosticVisible);
    expertHeaderLayout->addWidget(m_toggleExpertButton);
    layout->addWidget(expertHeader);

    m_expertContainer = new QWidget(content);
    m_expertContainer->setObjectName(QStringLiteral("ExpertDiagnosticContainer"));
    auto* expertContainerLayout = new QVBoxLayout(m_expertContainer);
    expertContainerLayout->setContentsMargins(0, 0, 0, 0);
    expertContainerLayout->setSpacing(6);

    m_downloadWidget = new DownloadDockWidget(m_expertContainer);
    m_downloadWidget->setObjectName(QStringLiteral("ExpertDownloadWidget"));
    expertContainerLayout->addWidget(m_downloadWidget);

    m_expertContainer->setVisible(false);
    layout->addWidget(m_expertContainer);

    layout->addStretch();
}

DeviceWorkspaceWidget::~DeviceWorkspaceWidget() = default;

void DeviceWorkspaceWidget::setTargetInfo(const QString& targetName, const QString& configSource,
                                          const QString& address, const QString& expertTarget,
                                          const QString& downloadProfile)
{
    if (m_targetNameValue) {
        m_targetNameValue->setText(targetName.isEmpty() ? tr("未配置 / 离线模式") : targetName);
    }
    if (m_configSourceValue) {
        m_configSourceValue->setText(configSource.isEmpty() ? tr("默认配置") : configSource);
    }
    if (m_addressValue) {
        m_addressValue->setText(address.isEmpty() ? tr("未连接 (离线)") : address);
    }
    if (m_expertAddressValue && !expertTarget.isEmpty()) {
        m_expertAddressValue->setText(expertTarget);
    }
    if (m_downloadProfileValue && !downloadProfile.isEmpty()) {
        m_downloadProfileValue->setText(downloadProfile);
    }
}

QString DeviceWorkspaceWidget::targetValue() const
{
    return m_targetNameValue ? m_targetNameValue->text() : QString();
}

QString DeviceWorkspaceWidget::addressValue() const
{
    return m_addressValue ? m_addressValue->text() : QString();
}

QString DeviceWorkspaceWidget::expertTargetText() const
{
    return m_expertAddressValue ? m_expertAddressValue->text() : QString();
}

QString DeviceWorkspaceWidget::downloadProfileText() const
{
    return m_downloadProfileValue ? m_downloadProfileValue->text() : QString();
}

void DeviceWorkspaceWidget::setConnectionStatus(bool connected, const QString& statusText)
{
    if (!m_connectionBadge) {
        return;
    }
    const QString text = statusText.isEmpty() ? (connected ? tr("已连接") : tr("离线")) : statusText;
    m_connectionBadge->setText(text);
    applyBadgeStyle(m_connectionBadge, connected ? QStringLiteral("success") : QStringLiteral("neutral"));
}

void DeviceWorkspaceWidget::setControllerRunning(bool running, bool paused)
{
    if (!m_controllerBadge) {
        return;
    }
    if (paused) {
        m_controllerBadge->setText(tr("暂停"));
        applyBadgeStyle(m_controllerBadge, QStringLiteral("warning"));
    } else if (running) {
        m_controllerBadge->setText(tr("运行中"));
        applyBadgeStyle(m_controllerBadge, QStringLiteral("success"));
    } else {
        m_controllerBadge->setText(tr("停止"));
        applyBadgeStyle(m_controllerBadge, QStringLiteral("neutral"));
    }

    if (m_btnRunController && !m_btnRunController->defaultAction()) {
        m_btnRunController->setEnabled(!running || paused);
    }
    if (m_btnStopController && !m_btnStopController->defaultAction()) {
        m_btnStopController->setEnabled(running || paused);
    }
    if (m_btnPauseController && !m_btnPauseController->defaultAction()) {
        m_btnPauseController->setEnabled(running && !paused);
    }
    if (m_btnResumeController && !m_btnResumeController->defaultAction()) {
        m_btnResumeController->setEnabled(paused);
    }
    if (m_btnStepController && !m_btnStepController->defaultAction()) {
        m_btnStepController->setEnabled(paused);
    }
}

void DeviceWorkspaceWidget::bindActions(QAction* actTest, QAction* actRun, QAction* actStop,
                                        QAction* actPause, QAction* actResume, QAction* actStep,
                                        QAction* actDiagnosis)
{
    if (m_btnTestConnection && actTest) {
        m_btnTestConnection->setDefaultAction(actTest);
    }
    if (m_btnRunController && actRun) {
        m_btnRunController->setDefaultAction(actRun);
    }
    if (m_btnStopController && actStop) {
        m_btnStopController->setDefaultAction(actStop);
    }
    if (m_btnPauseController && actPause) {
        m_btnPauseController->setDefaultAction(actPause);
    }
    if (m_btnResumeController && actResume) {
        m_btnResumeController->setDefaultAction(actResume);
    }
    if (m_btnStepController && actStep) {
        m_btnStepController->setDefaultAction(actStep);
    }
    if (m_btnDiagnosisWizard && actDiagnosis) {
        m_btnDiagnosisWizard->setDefaultAction(actDiagnosis);
    }
}

bool DeviceWorkspaceWidget::isExpertDiagnosticVisible() const
{
    if (m_toggleExpertButton) {
        return m_toggleExpertButton->isChecked();
    }
    return m_expertContainer && !m_expertContainer->isHidden();
}

void DeviceWorkspaceWidget::setExpertDiagnosticVisible(bool visible)
{
    if (m_expertContainer) {
        m_expertContainer->setVisible(visible);
    }
    if (m_toggleExpertButton) {
        m_toggleExpertButton->blockSignals(true);
        m_toggleExpertButton->setChecked(visible);
        m_toggleExpertButton->setText(visible ? tr("收起专家诊断") : tr("展开专家诊断"));
        m_toggleExpertButton->setIcon(style()->standardIcon(visible ? QStyle::SP_ArrowUp : QStyle::SP_ArrowDown));
        m_toggleExpertButton->blockSignals(false);
    }
}

void DeviceWorkspaceWidget::applyBadgeStyle(QLabel* label, const QString& visualState)
{
    if (!label) {
        return;
    }
    if (visualState == QStringLiteral("success")) {
        label->setStyleSheet(QStringLiteral("color: #116329; background: #dafbe1; border: 1px solid #aceebb; border-radius: 4px; padding: 2px 8px; font-weight: bold;"));
    } else if (visualState == QStringLiteral("warning")) {
        label->setStyleSheet(QStringLiteral("color: #7d4e00; background: #fff8c5; border: 1px solid #f0d98c; border-radius: 4px; padding: 2px 8px; font-weight: bold;"));
    } else if (visualState == QStringLiteral("error")) {
        label->setStyleSheet(QStringLiteral("color: #cf222e; background: #ffebe9; border: 1px solid #ff8182; border-radius: 4px; padding: 2px 8px; font-weight: bold;"));
    } else {
        label->setStyleSheet(QStringLiteral("color: #57606a; background: #f6f8fa; border: 1px solid #d0d7de; border-radius: 4px; padding: 2px 8px; font-weight: bold;"));
    }
}
