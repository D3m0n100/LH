#include "SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSize>
#include <QVBoxLayout>

namespace {

void setupForm(QFormLayout* form)
{
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFormAlignment(Qt::AlignTop);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);
}

} // namespace

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("设置");
    setMinimumWidth(500);
    setModal(true);

    createUi();
    createConnections();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::createUi()
{
    setObjectName(QStringLiteral("SettingsDialog"));
    setStyleSheet(R"(
QDialog#SettingsDialog {
    background: #ffffff;
}
QGroupBox {
    margin-top: 12px;
    padding: 12px 10px 10px 10px;
    border: 1px solid #d0d7de;
    border-radius: 6px;
    background: #f6f8fa;
    font-weight: 600;
    color: #24292f;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 8px;
    padding: 0 4px;
    background: #ffffff;
}
QLabel {
    color: #57606a;
}
QLineEdit, QComboBox {
    min-height: 26px;
    padding: 3px 8px;
}
QPushButton {
    min-height: 28px;
    color: #24292f;
    background: #ffffff;
    border: 1px solid #d0d7de;
    border-radius: 4px;
    padding: 4px 12px;
}
QPushButton:hover {
    background: #eaeef2;
    border-color: #afb8c1;
}
QPushButton:default {
    color: #ffffff;
    background: #0969da;
    border-color: #0969da;
}
QPushButton:default:hover {
    background: #0757b8;
}
)");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 12, 14, 14);
    mainLayout->setSpacing(10);

    auto* title = new QLabel("应用设置", this);
    title->setStyleSheet("QLabel { color: #24292f; font-size: 14px; font-weight: 700; }");
    mainLayout->addWidget(title);

    auto* generalGroup = new QGroupBox("常规设置", this);
    auto* formLayout = new QFormLayout(generalGroup);
    setupForm(formLayout);

    auto* dirLayout = new QHBoxLayout();
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->setSpacing(6);

    m_editDefaultDir = new QLineEdit(this);
    m_editDefaultDir->setPlaceholderText("选择默认项目目录...");

    m_btnBrowseDir = new QPushButton("浏览...", this);
    m_btnBrowseDir->setIcon(QIcon(":/icons/open.svg"));
    m_btnBrowseDir->setIconSize(QSize(16, 16));
    m_btnBrowseDir->setFixedWidth(92);

    dirLayout->addWidget(m_editDefaultDir);
    dirLayout->addWidget(m_btnBrowseDir);
    formLayout->addRow("默认项目目录", dirLayout);

    m_chkAutoScroll = new QCheckBox("输出日志自动滚动到底部", this);
    m_chkAutoScroll->setChecked(true);
    formLayout->addRow("日志", m_chkAutoScroll);

    mainLayout->addWidget(generalGroup);

    auto* appearanceGroup = new QGroupBox("外观设置", this);
    auto* appearanceLayout = new QFormLayout(appearanceGroup);
    setupForm(appearanceLayout);

    m_comboFontSize = new QComboBox(this);
    m_comboFontSize->addItem("小 (9pt)");
    m_comboFontSize->addItem("中 (11pt)");
    m_comboFontSize->addItem("大 (14pt)");
    m_comboFontSize->setCurrentIndex(1);
    appearanceLayout->addRow("界面字体大小", m_comboFontSize);

    mainLayout->addWidget(appearanceGroup);
    mainLayout->addStretch();

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 4, 0, 0);
    buttonLayout->addStretch();

    m_btnOk = new QPushButton("确定", this);
    m_btnOk->setDefault(true);
    m_btnOk->setFixedWidth(86);

    m_btnCancel = new QPushButton("取消", this);
    m_btnCancel->setFixedWidth(86);

    buttonLayout->addWidget(m_btnOk);
    buttonLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(buttonLayout);
}

void SettingsDialog::createConnections()
{
    connect(m_btnBrowseDir, &QPushButton::clicked,
            this, &SettingsDialog::onBrowseDefaultDir);
    connect(m_btnOk, &QPushButton::clicked,
            this, &QDialog::accept);
    connect(m_btnCancel, &QPushButton::clicked,
            this, &QDialog::reject);
}

void SettingsDialog::onBrowseDefaultDir()
{
    QString currentDir = m_editDefaultDir->text();
    if (currentDir.isEmpty() || !QDir(currentDir).exists()) {
        currentDir = QDir::homePath();
    }

    const QString dir = QFileDialog::getExistingDirectory(
        this,
        "选择默认项目目录",
        currentDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        m_editDefaultDir->setText(dir);
    }
}

QString SettingsDialog::defaultProjectDir() const
{
    return m_editDefaultDir ? m_editDefaultDir->text() : QString();
}

bool SettingsDialog::autoScrollLog() const
{
    return m_chkAutoScroll ? m_chkAutoScroll->isChecked() : true;
}

int SettingsDialog::fontSizeIndex() const
{
    return m_comboFontSize ? m_comboFontSize->currentIndex() : 1;
}

void SettingsDialog::setDefaultProjectDir(const QString& dir)
{
    if (m_editDefaultDir) {
        m_editDefaultDir->setText(dir);
    }
}

void SettingsDialog::setAutoScrollLog(bool enabled)
{
    if (m_chkAutoScroll) {
        m_chkAutoScroll->setChecked(enabled);
    }
}

void SettingsDialog::setFontSizeIndex(int index)
{
    if (m_comboFontSize && index >= 0 && index < m_comboFontSize->count()) {
        m_comboFontSize->setCurrentIndex(index);
    }
}

int SettingsDialog::fontPointSize(int index)
{
    switch (index) {
    case 0:
        return 9;
    case 1:
        return 11;
    case 2:
        return 14;
    default:
        return 11;
    }
}
