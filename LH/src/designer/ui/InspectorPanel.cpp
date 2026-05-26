#include "InspectorPanel.h"
#include "ParameterController.h"

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
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
    m_parameterTable->setHorizontalHeaderLabels({ "名称", "默认值", "当前值", "状态", "确认", "回读", "偏差" });
    m_parameterTable->horizontalHeader()->setStretchLastSection(false);
    m_parameterTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_parameterTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_parameterTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_parameterTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_parameterTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_parameterTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_parameterTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_parameterTable->horizontalHeader()->setMinimumSectionSize(48);
    m_parameterTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_parameterTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_parameterTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_parameterTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_parameterTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_parameterTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_parameterTable->setAlternatingRowColors(true);
    m_parameterTable->verticalHeader()->setDefaultSectionSize(28);
    m_parameterTable->setMinimumHeight(80);
    m_contextGroup = new QGroupBox("上下文概览", content);
    auto* contextLayout = new QFormLayout(m_contextGroup);
    contextLayout->setContentsMargins(4, 2, 4, 4);
    contextLayout->setHorizontalSpacing(8);
    contextLayout->setVerticalSpacing(6);
    contextLayout->addRow("项目路径", m_projectPathValue);
    contextLayout->addRow("当前文件", m_currentFileValue);
    contextLayout->addRow("工作区", m_workspaceValue);
    contextLayout->addRow("运行状态", m_runtimeValue);
    contextLayout->addRow("构建状态", m_buildValue);
    contextLayout->addRow("监控状态", m_monitorValue);
    contextLayout->addRow("变量摘要", m_variableValue);
    contextLayout->addRow("参数摘要", m_parameterValue);
    contextLayout->addRow("资源摘要", m_resourceValue);
    contentLayout->addWidget(m_contextGroup);

    m_paramGroup = new QGroupBox("在线参数", content);
    auto* paramLayout = new QVBoxLayout(m_paramGroup);
    paramLayout->setContentsMargins(0, 2, 0, 0);
    paramLayout->setSpacing(6);
    auto* paramHint = new QLabel("双击参数行可直接修改当前值。", this);
    paramHint->setStyleSheet("QLabel { color: #57606a; }");
    paramLayout->addWidget(paramHint);
    paramLayout->addWidget(m_parameterTable, 1);
    m_parameterEditButton = makeActionButton("编辑选中参数", ":/icons/settings.svg", this);
    connect(m_parameterEditButton, &QToolButton::clicked, this, [this]() {
        if (!m_parameterTable || !m_parameterTable->currentItem()) {
            return;
        }
        onParameterItemDoubleClicked(m_parameterTable->currentItem());
    });
    paramLayout->addWidget(m_parameterEditButton);
    m_applyParametersButton = makeActionButton("应用参数到监控", ":/icons/run.svg", this);
    connect(m_applyParametersButton, &QToolButton::clicked, this, &InspectorPanel::requestApplyParameters);
    paramLayout->addWidget(m_applyParametersButton);
    contentLayout->addWidget(m_paramGroup, 1);
}

void InspectorPanel::setPanelMode(PanelMode mode)
{
    m_panelMode = mode;
    const bool inspectionMode = (m_panelMode == PanelMode::Inspection);
    if (m_contextGroup) {
        m_contextGroup->setVisible(inspectionMode);
    }
    if (m_paramGroup) {
        m_paramGroup->setTitle(inspectionMode ? QStringLiteral("在线参数") : QStringLiteral("PID 参数"));
    }
    if (m_parameterEditButton) {
        m_parameterEditButton->setText(inspectionMode ? QStringLiteral("编辑选中参数")
                                                      : QStringLiteral("编辑选中 PID 参数"));
        m_parameterEditButton->setVisible(!inspectionMode);
    }
    if (m_applyParametersButton) {
        m_applyParametersButton->setText(inspectionMode ? QStringLiteral("应用参数到监控")
                                                        : QStringLiteral("应用 PID 参数到监控"));
        m_applyParametersButton->setVisible(!inspectionMode);
    }
    if (m_parameterTable) {
        if (inspectionMode) {
            m_parameterTable->setColumnCount(3);
            m_parameterTable->setHorizontalHeaderLabels({ "名称", "当前值", "详情" });
            m_parameterTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
            m_parameterTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
            m_parameterTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
            m_parameterTable->horizontalHeader()->setMinimumSectionSize(56);
            m_parameterTable->setWordWrap(true);
        } else {
            m_parameterTable->setColumnCount(7);
            m_parameterTable->setHorizontalHeaderLabels({ "名称", "默认值", "当前值", "状态", "确认", "回读", "偏差" });
            m_parameterTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
            m_parameterTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
            m_parameterTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
            m_parameterTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
            m_parameterTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
            m_parameterTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
            m_parameterTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
            m_parameterTable->horizontalHeader()->setMinimumSectionSize(48);
            m_parameterTable->setWordWrap(false);
        }
    }
    refreshParameterTable();
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

void InspectorPanel::setParameterReadbackReady(const QStringList& readyParameterNames)
{
    m_readbackReadyParameters = readyParameterNames;
    refreshParameterTable();
}

void InspectorPanel::setParameterDeviationMap(const QMap<QString, double>& deviationMap)
{
    m_parameterDeviationMap = deviationMap;
    refreshParameterTable();
}

void InspectorPanel::setParameterStateMap(const QMap<QString, ParameterStateInfo>& stateMap)
{
    m_parameterStateMap = stateMap;
    refreshParameterTable();
}

static QString parameterStateDisplayText(ParameterState state)
{
    switch (state) {
    case ParameterState::Clean:           return QStringLiteral("未修改");
    case ParameterState::Modified:        return QStringLiteral("已修改");
    case ParameterState::PendingApply:    return QStringLiteral("待下发");
    case ParameterState::Applying:        return QStringLiteral("下发中");
    case ParameterState::PendingReadback: return QStringLiteral("待回读");
    case ParameterState::Confirmed:       return QStringLiteral("已确认");
    case ParameterState::Mismatch:        return QStringLiteral("不匹配");
    case ParameterState::ApplyFailed:     return QStringLiteral("下发失败");
    default:                              return QStringLiteral("未知");
    }
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
        auto* valueItem = new QTableWidgetItem(current);
        m_parameterTable->setItem(row, 0, nameItem);
        if (m_panelMode == PanelMode::Inspection) {
            const QString detailText = QString("默认值: %1\n变更: %2\n确认: %3\n回读: %4\n偏差: %5")
                                           .arg(p.defaultValue,
                                                current == p.defaultValue ? "未变更" : "已变更",
                                                p.confirmed ? "已确认" : "待确认",
                                                readbackStateFor(p),
                                                deviationStateFor(p));
            auto* detailItem = new QTableWidgetItem(detailText);
            detailItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            detailItem->setFlags(detailItem->flags() & ~Qt::ItemIsEditable);
            m_parameterTable->setItem(row, 1, valueItem);
            m_parameterTable->setItem(row, 2, detailItem);
        } else {
            // 状态列：优先使用 ParameterController 的状态
            QString stateText;
            bool stateIsError = false;
            auto stIt = m_parameterStateMap.constFind(p.name);
            if (stIt != m_parameterStateMap.constEnd()) {
                stateText = parameterStateDisplayText(stIt->state);
                stateIsError = (stIt->state == ParameterState::Modified
                                || stIt->state == ParameterState::ApplyFailed
                                || stIt->state == ParameterState::Mismatch);
            } else {
                stateText = current == p.defaultValue ? "未变更" : "已变更";
                stateIsError = (current != p.defaultValue);
            }

            auto* stateItem = new QTableWidgetItem(stateText);
            auto* confirmItem = new QTableWidgetItem(p.confirmed ? "已确认" : "待确认");
            auto* readbackItem = new QTableWidgetItem(readbackStateFor(p));
            auto* deviationItem = new QTableWidgetItem(deviationStateFor(p));
            if (stateIsError) {
                stateItem->setForeground(QColor("#cf222e"));
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
            auto* defaultItem = new QTableWidgetItem(p.defaultValue);
            m_parameterTable->setItem(row, 1, defaultItem);
            m_parameterTable->setItem(row, 2, valueItem);
            m_parameterTable->setItem(row, 3, stateItem);
            m_parameterTable->setItem(row, 4, confirmItem);
            m_parameterTable->setItem(row, 5, readbackItem);
            m_parameterTable->setItem(row, 6, deviationItem);
        }
    }
    if (m_panelMode == PanelMode::Inspection) {
        m_parameterTable->resizeRowsToContents();
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
    if (m_panelMode == PanelMode::Inspection) {
        return;
    }
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

