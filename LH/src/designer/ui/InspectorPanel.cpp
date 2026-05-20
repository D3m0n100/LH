#include "InspectorPanel.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QAbstractItemView>
#include <QColor>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QFrame>
#include <QSize>
#include <QSizePolicy>
#include <QScrollArea>
#include <QStyle>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

namespace {

QLabel* makeValueLabel(QWidget* parent)
{
    QLabel* lbl = new QLabel("-", parent);
    lbl->setWordWrap(true);
    lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lbl->setStyleSheet("QLabel { color: #24292f; line-height: 18px; }");
    return lbl;
}

QLabel* makeStateLabel(QWidget* parent)
{
    QLabel* lbl = new QLabel("-", parent);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setMinimumHeight(24);
    lbl->setProperty("state", QVariant(QStringLiteral("neutral")));
    return lbl;
}

QToolButton* makeActionButton(const QString& text, const QString& iconPath, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setText(text);
    button->setIcon(QIcon(iconPath));
    button->setIconSize(QSize(16, 16));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setMinimumHeight(30);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return button;
}

} // namespace

InspectorPanel::InspectorPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("InspectorPanel"));
    setStyleSheet(R"(
QWidget#InspectorPanel {
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
QLabel[state="neutral"] {
    color: #57606a;
    background: #f6f8fa;
    border: 1px solid #d0d7de;
    border-radius: 4px;
    padding: 2px 8px;
}
QLabel[state="success"] {
    color: #116329;
    background: #dafbe1;
    border: 1px solid #aceebb;
    border-radius: 4px;
    padding: 2px 8px;
}
QLabel[state="warning"] {
    color: #7d4e00;
    background: #fff8c5;
    border: 1px solid #f0d98c;
    border-radius: 4px;
    padding: 2px 8px;
}
QToolButton {
    color: #24292f;
    background: #ffffff;
    border: 1px solid #d0d7de;
    border-radius: 4px;
    padding: 5px 8px;
}
QToolButton:hover {
    background: #eaeef2;
    border-color: #afb8c1;
}
QToolButton:pressed {
    background: #d8dee4;
}
)");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    root->addWidget(scrollArea);

    auto* content = new QWidget(scrollArea);
    content->setObjectName(QStringLiteral("InspectorPanelContent"));
    content->setStyleSheet("QWidget#InspectorPanelContent { background: #ffffff; }");
    scrollArea->setWidget(content);

    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(8);

    m_projectPathValue = makeValueLabel(this);
    m_currentFileValue = makeValueLabel(this);
    m_workspaceValue = makeValueLabel(this);
    m_runtimeValue = makeStateLabel(this);
    m_buildValue = makeStateLabel(this);
    m_monitorValue = makeStateLabel(this);
    m_variableValue = makeValueLabel(this);
    m_parameterValue = makeValueLabel(this);
    m_resourceValue = makeValueLabel(this);
    m_parameterTable = new QTableWidget(this);
    m_parameterTable->setColumnCount(7);
    m_parameterTable->setHorizontalHeaderLabels({ "名称", "默认值", "当前值", "变更", "确认", "回读", "偏差" });
    m_parameterTable->horizontalHeader()->setStretchLastSection(true);
    m_parameterTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_parameterTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_parameterTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_parameterTable->setAlternatingRowColors(true);
    m_parameterTable->setMinimumHeight(130);
    connect(m_parameterTable, &QTableWidget::itemDoubleClicked,
            this, &InspectorPanel::onParameterItemDoubleClicked);
    m_pidParameterTable = new QTableWidget(this);
    m_pidParameterTable->setColumnCount(7);
    m_pidParameterTable->setHorizontalHeaderLabels({ "名称", "默认值", "当前值", "变更", "确认", "回读", "偏差" });
    m_pidParameterTable->horizontalHeader()->setStretchLastSection(true);
    m_pidParameterTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pidParameterTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pidParameterTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pidParameterTable->setAlternatingRowColors(true);
    m_pidParameterTable->setMinimumHeight(120);

    auto* paramGroup = new QGroupBox("在线参数", content);
    auto* paramLayout = new QVBoxLayout(paramGroup);
    paramLayout->setContentsMargins(0, 2, 0, 0);
    paramLayout->setSpacing(6);
    auto* paramHint = new QLabel("双击参数行可直接修改当前值。", this);
    paramHint->setStyleSheet("QLabel { color: #57606a; }");
    paramLayout->addWidget(paramHint);
    paramLayout->addWidget(m_parameterTable);
    m_parameterEditButton = makeActionButton("编辑选中参数", ":/icons/settings.svg", this);
    connect(m_parameterEditButton, &QToolButton::clicked, this, [this]() {
        if (!m_parameterTable || !m_parameterTable->currentItem()) {
            return;
        }
        onParameterItemDoubleClicked(m_parameterTable->currentItem());
    });
    paramLayout->addWidget(m_parameterEditButton);
    auto* applyParametersButton = makeActionButton("应用参数到监控", ":/icons/run.svg", this);
    connect(applyParametersButton, &QToolButton::clicked, this, &InspectorPanel::requestApplyParameters);
    paramLayout->addWidget(applyParametersButton);
    contentLayout->addWidget(paramGroup);

    auto* pidGroup = new QGroupBox("PID 参数", content);
    auto* pidLayout = new QVBoxLayout(pidGroup);
    pidLayout->setContentsMargins(0, 2, 0, 0);
    pidLayout->setSpacing(6);
    auto* pidHint = new QLabel("自动识别名称中包含 PID 的参数，便于专门调参与对比。", this);
    pidHint->setStyleSheet("QLabel { color: #57606a; }");
    pidLayout->addWidget(pidHint);
    pidLayout->addWidget(m_pidParameterTable);
    contentLayout->addWidget(pidGroup);
    contentLayout->addStretch();
}

void InspectorPanel::setProjectPath(const QString& projectPath)
{
    m_projectPathValue->setText(projectPath.isEmpty() ? "-" : projectPath);
}

void InspectorPanel::setCurrentFile(const QString& filePath)
{
    m_currentFileValue->setText(filePath.isEmpty() ? "-" : filePath);
}

void InspectorPanel::setWorkspaceName(const QString& workspaceName)
{
    m_workspaceValue->setText(workspaceName.isEmpty() ? "-" : workspaceName);
}

void InspectorPanel::setRuntimeState(const QString& runtimeState)
{
    const QString state = runtimeState.isEmpty() ? "-" : runtimeState;
    m_runtimeValue->setText(state);
    applyStateStyle(m_runtimeValue, state);
}

void InspectorPanel::setBuildState(const QString& buildState)
{
    const QString state = buildState.isEmpty() ? "-" : buildState;
    m_buildValue->setText(state);
    applyStateStyle(m_buildValue, state);
}

void InspectorPanel::setMonitoringState(const QString& monitoringState)
{
    const QString state = monitoringState.isEmpty() ? "-" : monitoringState;
    m_monitorValue->setText(state);
    applyStateStyle(m_monitorValue, state);
}

void InspectorPanel::setVariableSummary(const QString& summary)
{
    m_variableValue->setText(summary.isEmpty() ? "-" : summary);
}

void InspectorPanel::setParameterSummary(const QString& summary)
{
    m_parameterValue->setText(summary.isEmpty() ? "-" : summary);
}

void InspectorPanel::setResourceSummary(const QString& summary)
{
    m_resourceValue->setText(summary.isEmpty() ? "-" : summary);
}

void InspectorPanel::setParameterDetails(const QList<ParameterDefinition>& parameters)
{
    m_parameterData = parameters;
    refreshParameterTable();
}

void InspectorPanel::setPidParameterDetails(const QList<ParameterDefinition>& parameters)
{
    m_pidParameterData = parameters;
    refreshPidParameterTable();
}

void InspectorPanel::setParameterReadbackReady(const QStringList& readyParameterNames)
{
    m_readbackReadyParameters = readyParameterNames;
    refreshParameterTable();
    refreshPidParameterTable();
}

void InspectorPanel::setParameterDeviationMap(const QMap<QString, double>& deviationMap)
{
    m_parameterDeviationMap = deviationMap;
    refreshParameterTable();
    refreshPidParameterTable();
}

void InspectorPanel::refreshParameterTable()
{
    if (!m_parameterTable) {
        return;
    }

    m_parameterTable->setRowCount(0);
    for (const auto& p : m_parameterData) {
        const int row = m_parameterTable->rowCount();
        m_parameterTable->insertRow(row);

        const QString current = p.currentValue.isEmpty() ? p.defaultValue : p.currentValue;
        auto* nameItem = new QTableWidgetItem(p.name);
        auto* defaultItem = new QTableWidgetItem(p.defaultValue);
        auto* valueItem = new QTableWidgetItem(current);
        auto* changeItem = new QTableWidgetItem(current == p.defaultValue ? "未变更" : "已变更");
        auto* confirmItem = new QTableWidgetItem(p.confirmed ? "已确认" : "待确认");
        auto* readbackItem = new QTableWidgetItem(readbackStateFor(p));
        auto* deviationItem = new QTableWidgetItem(deviationStateFor(p));
        if (current != p.defaultValue) {
            changeItem->setForeground(QColor("#cf222e"));
        }
        if (!p.confirmed) {
            confirmItem->setForeground(QColor("#cf222e"));
        }
        if (readbackItem->text() == "待回读") {
            readbackItem->setForeground(QColor("#cf222e"));
        }
        if (deviationItem->text() != "无") {
            deviationItem->setForeground(QColor("#cf222e"));
        }
        m_parameterTable->setItem(row, 0, nameItem);
        m_parameterTable->setItem(row, 1, defaultItem);
        m_parameterTable->setItem(row, 2, valueItem);
        m_parameterTable->setItem(row, 3, changeItem);
        m_parameterTable->setItem(row, 4, confirmItem);
        m_parameterTable->setItem(row, 5, readbackItem);
        m_parameterTable->setItem(row, 6, deviationItem);
    }
}

void InspectorPanel::refreshPidParameterTable()
{
    if (!m_pidParameterTable) {
        return;
    }

    m_pidParameterTable->setRowCount(0);
    for (const auto& p : m_pidParameterData) {
        const int row = m_pidParameterTable->rowCount();
        m_pidParameterTable->insertRow(row);

        const QString current = p.currentValue.isEmpty() ? p.defaultValue : p.currentValue;
        auto* nameItem = new QTableWidgetItem(p.name);
        auto* defaultItem = new QTableWidgetItem(p.defaultValue);
        auto* valueItem = new QTableWidgetItem(current);
        auto* changeItem = new QTableWidgetItem(current == p.defaultValue ? "未变更" : "已变更");
        auto* confirmItem = new QTableWidgetItem(p.confirmed ? "已确认" : "待确认");
        auto* readbackItem = new QTableWidgetItem(readbackStateFor(p));
        auto* deviationItem = new QTableWidgetItem(deviationStateFor(p));
        if (current != p.defaultValue) {
            changeItem->setForeground(QColor("#cf222e"));
        }
        if (!p.confirmed) {
            confirmItem->setForeground(QColor("#cf222e"));
        }
        if (readbackItem->text() == "待回读") {
            readbackItem->setForeground(QColor("#cf222e"));
        }
        if (deviationItem->text() != "无") {
            deviationItem->setForeground(QColor("#cf222e"));
        }
        m_pidParameterTable->setItem(row, 0, nameItem);
        m_pidParameterTable->setItem(row, 1, defaultItem);
        m_pidParameterTable->setItem(row, 2, valueItem);
        m_pidParameterTable->setItem(row, 3, changeItem);
        m_pidParameterTable->setItem(row, 4, confirmItem);
        m_pidParameterTable->setItem(row, 5, readbackItem);
        m_pidParameterTable->setItem(row, 6, deviationItem);
    }
}

QString InspectorPanel::readbackStateFor(const ParameterDefinition& parameter) const
{
    const bool ready = m_readbackReadyParameters.contains(parameter.name);
    return ready ? QStringLiteral("已回读") : QStringLiteral("待回读");
}

QString InspectorPanel::deviationStateFor(const ParameterDefinition& parameter) const
{
    const auto it = m_parameterDeviationMap.constFind(parameter.name);
    if (it == m_parameterDeviationMap.constEnd()) {
        return QStringLiteral("无");
    }
    return QString::number(it.value(), 'f', 3);
}

void InspectorPanel::onParameterItemDoubleClicked(QTableWidgetItem* item)
{
    if (!item || item->column() != 1 || !m_parameterTable) {
        return;
    }

    const int row = item->row();
    if (row < 0 || row >= m_parameterData.size()) {
        return;
    }

    const auto& param = m_parameterData[row];
    if (!param.onlineEditable) {
        return;
    }

    emit requestEditParameter(param.name);
}

void InspectorPanel::applyStateStyle(QLabel* label, const QString& state)
{
    if (!label) {
        return;
    }

    QString visualState = QStringLiteral("neutral");
    if (state == QStringLiteral("运行中") ||
        state == QStringLiteral("活动") ||
        state == QStringLiteral("空闲")) {
        visualState = QStringLiteral("success");
    } else if (state == QStringLiteral("忙碌")) {
        visualState = QStringLiteral("warning");
    }

    label->setProperty("state", QVariant(visualState));
    label->style()->unpolish(label);
    label->style()->polish(label);
}

