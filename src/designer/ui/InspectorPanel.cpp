#include "ThemeManager.h"
#include "InspectorPanel.h"
#include "ParameterController.h"
#include "StatusTextHelper.h"

#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QAbstractItemView>
#include <QColor>
#include <QLabel>
#include <QPainter>
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

class ElidedLabel : public QLabel
{
public:
    explicit ElidedLabel(QWidget* parent = nullptr)
        : QLabel(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumWidth(0);
    }

    void setFullText(const QString& text)
    {
        const QString fullText = text.isEmpty() ? QStringLiteral("-") : text;
        QLabel::setText(fullText);
        setToolTip(fullText == QStringLiteral("-") ? QString() : fullText);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setFont(font());
        painter.setPen(palette().color(QPalette::WindowText));
        const QString text = fontMetrics().elidedText(QLabel::text(), Qt::ElideMiddle, width());
        painter.drawText(rect(), alignment() | Qt::AlignVCenter, text);
    }

};

QLabel* makeElidedValueLabel(QWidget* parent)
{
    auto* lbl = new ElidedLabel(parent);
    lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    lbl->setStyleSheet("QLabel { color: #24292f; }");
    return lbl;
}

QLabel* makeCaptionLabel(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    lbl->setStyleSheet("QLabel { color: #57606a; }");
    lbl->setFixedWidth(44);
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

QTableWidgetItem* makeParameterItem(const QString& text, const QString& toolTip = QString())
{
    auto* item = new QTableWidgetItem(text);
    item->setToolTip(toolTip.isEmpty() ? text : toolTip);
    return item;
}

void setErrorText(QTableWidgetItem* item, bool error)
{
    if (item && error) {
        item->setForeground(QColor("#cf222e"));
    }
}

void configureParameterTable(QTableWidget* table, bool inspectionMode)
{
    if (!table) {
        return;
    }

    table->setColumnCount(7);
    table->setHorizontalHeaderLabels(inspectionMode
        ? QStringList{
              QStringLiteral("名称"),
              QStringLiteral("当前值"),
              QStringLiteral("状态"),
              QStringLiteral("问题"),
              QStringLiteral("确认"),
              QStringLiteral("回读"),
              QStringLiteral("偏差")}
        : QStringList{
              QStringLiteral("名称"),
              QStringLiteral("已读取值"),
              QStringLiteral("待应用值"),
              QStringLiteral("状态"),
              QStringLiteral("确认"),
              QStringLiteral("回读"),
              QStringLiteral("偏差")});

    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, inspectionMode ? QHeaderView::Stretch
                                                                      : QHeaderView::ResizeToContents);
    for (int column = 4; column < table->columnCount(); ++column) {
        table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
        table->setColumnHidden(column, true);
    }
    for (int column = 0; column < 4; ++column) {
        table->setColumnHidden(column, false);
    }

    table->horizontalHeader()->setMinimumSectionSize(48);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->setWordWrap(false);
    table->verticalHeader()->setDefaultSectionSize(inspectionMode ? 26 : 28);
}

} // namespace

InspectorPanel::InspectorPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("InspectorPanel"));
    setStyleSheet(ThemeManager::groupBoxStyleSheet(true) + R"(
QWidget#InspectorPanel {
    background: #ffffff;
    border: 1px solid #d0d7de;
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
QLabel[state="error"] {
    color: #cf222e;
    background: #ffebe9;
    border: 1px solid #ff8182;
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
    root->setContentsMargins(9, 9, 9, 9);
    root->setSpacing(6);

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
    contentLayout->setSpacing(6);

    m_projectPathValue = makeElidedValueLabel(this);
    m_currentFileValue = makeElidedValueLabel(this);
    m_workspaceValue = makeElidedValueLabel(this);
    m_variableValue = makeElidedValueLabel(this);
    m_parameterValue = makeElidedValueLabel(this);
    m_resourceValue = makeElidedValueLabel(this);
    m_parameterTable = new QTableWidget(this);
    configureParameterTable(m_parameterTable, m_panelMode == PanelMode::Inspection);
    m_parameterTable->horizontalHeader()->setStretchLastSection(false);
    m_parameterTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_parameterTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_parameterTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_parameterTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_parameterTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_parameterTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_parameterTable->setAlternatingRowColors(true);
    m_parameterTable->setTextElideMode(Qt::ElideRight);
    m_parameterTable->setMinimumHeight(80);

    // U04: 右侧属性面板移除重复全局概览，仅保留项目上下文与参数检查
    m_stateGroup = nullptr;
    m_runtimeValue = nullptr;
    m_buildValue = nullptr;
    m_monitorValue = nullptr;
    m_downloadValue = nullptr;
    m_opcValue = nullptr;

    m_contextGroup = new QGroupBox(QStringLiteral("项目上下文"), content);
    auto* contextLayout = new QVBoxLayout(m_contextGroup);
    contextLayout->setContentsMargins(6, 2, 6, 6);
    contextLayout->setSpacing(6);

    auto* pathLayout = new QGridLayout();
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setHorizontalSpacing(6);
    pathLayout->setVerticalSpacing(4);
    pathLayout->addWidget(makeCaptionLabel(QStringLiteral("项目"), this), 0, 0);
    pathLayout->addWidget(m_projectPathValue, 0, 1);
    pathLayout->addWidget(makeCaptionLabel(QStringLiteral("文件"), this), 1, 0);
    pathLayout->addWidget(m_currentFileValue, 1, 1);
    pathLayout->addWidget(makeCaptionLabel(QStringLiteral("工作区"), this), 2, 0);
    pathLayout->addWidget(m_workspaceValue, 2, 1);
    pathLayout->setColumnStretch(1, 1);
    contextLayout->addLayout(pathLayout);

    auto* summaryLayout = new QGridLayout();
    summaryLayout->setContentsMargins(0, 0, 0, 0);
    summaryLayout->setHorizontalSpacing(6);
    summaryLayout->setVerticalSpacing(4);
    summaryLayout->addWidget(makeCaptionLabel(QStringLiteral("变量"), this), 0, 0);
    summaryLayout->addWidget(m_variableValue, 0, 1);
    summaryLayout->addWidget(makeCaptionLabel(QStringLiteral("参数"), this), 1, 0);
    summaryLayout->addWidget(m_parameterValue, 1, 1);
    summaryLayout->addWidget(makeCaptionLabel(QStringLiteral("资源"), this), 2, 0);
    summaryLayout->addWidget(m_resourceValue, 2, 1);
    summaryLayout->setColumnStretch(1, 1);
    contextLayout->addLayout(summaryLayout);
    contentLayout->addWidget(m_contextGroup);
    m_contextGroup->setVisible(false);

    m_selectionHintLabel = new QLabel(QStringLiteral("未选择任何对象。\n可在项目树、函数库或参数列表中选中项以查看其详细属性。"), content);
    m_selectionHintLabel->setWordWrap(true);
    m_selectionHintLabel->setAlignment(Qt::AlignCenter);
    m_selectionHintLabel->setStyleSheet(QStringLiteral("QLabel { color: #57606a; padding: 16px 8px; font-size: 12px; }"));
    contentLayout->addWidget(m_selectionHintLabel);

    m_selectedGroup = new QGroupBox(QStringLiteral("选中对象属性"), content);
    auto* selLayout = new QVBoxLayout(m_selectedGroup);
    selLayout->setContentsMargins(6, 4, 6, 6);
    selLayout->setSpacing(6);

    auto* selHeaderLayout = new QGridLayout();
    selHeaderLayout->setContentsMargins(0, 0, 0, 0);
    selHeaderLayout->setHorizontalSpacing(6);
    selHeaderLayout->setVerticalSpacing(4);
    selHeaderLayout->addWidget(makeCaptionLabel(QStringLiteral("类型"), this), 0, 0);
    m_selectedTypeValue = makeElidedValueLabel(this);
    selHeaderLayout->addWidget(m_selectedTypeValue, 0, 1);
    selHeaderLayout->addWidget(makeCaptionLabel(QStringLiteral("名称"), this), 1, 0);
    m_selectedNameValue = makeElidedValueLabel(this);
    selHeaderLayout->addWidget(m_selectedNameValue, 1, 1);
    selHeaderLayout->setColumnStretch(1, 1);
    selLayout->addLayout(selHeaderLayout);

    m_selectedPropsTable = new QTableWidget(this);
    m_selectedPropsTable->setColumnCount(2);
    m_selectedPropsTable->setHorizontalHeaderLabels({QStringLiteral("属性"), QStringLiteral("值")});
    m_selectedPropsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_selectedPropsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_selectedPropsTable->verticalHeader()->setVisible(false);
    m_selectedPropsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_selectedPropsTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_selectedPropsTable->setAlternatingRowColors(true);
    m_selectedPropsTable->setMinimumHeight(110);
    selLayout->addWidget(m_selectedPropsTable);

    contentLayout->addWidget(m_selectedGroup);
    m_selectedGroup->setVisible(false);

    m_paramGroup = new QGroupBox(QStringLiteral("参数检查"), content);
    auto* paramLayout = new QVBoxLayout(m_paramGroup);
    paramLayout->setContentsMargins(0, 2, 0, 0);
    paramLayout->setSpacing(6);
    m_paramHint = new QLabel(QStringLiteral("双击参数行可直接修改当前值。"), this);
    m_paramHint->setStyleSheet("QLabel { color: #57606a; }");
    paramLayout->addWidget(m_paramHint);
    paramLayout->addWidget(m_parameterTable, 1);
    connect(m_parameterTable, &QTableWidget::itemSelectionChanged,
            this, &InspectorPanel::onParameterSelectionChanged);

    m_toggleDetailColumnsButton = new QPushButton(QStringLiteral("展开明细列"), this);
    m_toggleDetailColumnsButton->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    m_toggleDetailColumnsButton->setObjectName(QStringLiteral("ToggleDetailColumnsButton"));
    m_toggleDetailColumnsButton->setCheckable(true);
    m_toggleDetailColumnsButton->setChecked(false);
    m_toggleDetailColumnsButton->setMinimumHeight(28);
    m_toggleDetailColumnsButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #f6f8fa; border: 1px solid #d0d7de; border-radius: 4px; padding: 4px 8px; color: #57606a; font-size: 12px; }"
        "QPushButton:hover { background: #eaeef2; color: #24292f; }"
        "QPushButton:checked { background: #ddf4ff; border-color: #54aeff; color: #0969da; }"
    ));
    connect(m_toggleDetailColumnsButton, &QPushButton::toggled, this, [this](bool checked) {
        setDetailColumnsVisible(checked);
    });
    paramLayout->addWidget(m_toggleDetailColumnsButton);

    m_parameterEditButton = makeActionButton(QStringLiteral("编辑选中参数"), ":/icons/settings.svg", this);
    connect(m_parameterEditButton, &QToolButton::clicked, this, [this]() {
        if (!m_parameterTable || !m_parameterTable->currentItem()) {
            return;
        }
        onParameterItemDoubleClicked(m_parameterTable->currentItem());
    });
    paramLayout->addWidget(m_parameterEditButton);
    m_applyParametersButton = makeActionButton(QStringLiteral("应用参数到监控"), ":/icons/run.svg", this);
    connect(m_applyParametersButton, &QToolButton::clicked, this, &InspectorPanel::requestApplyParameters);
    paramLayout->addWidget(m_applyParametersButton);
    contentLayout->addWidget(m_paramGroup, 1);

    clearSelection();
}

bool InspectorPanel::areDetailColumnsVisible() const
{
    if (!m_parameterTable || m_parameterTable->columnCount() <= 4) {
        return false;
    }
    return !m_parameterTable->isColumnHidden(4);
}

void InspectorPanel::setDetailColumnsVisible(bool visible)
{
    if (!m_parameterTable) {
        return;
    }
    for (int col = 4; col < m_parameterTable->columnCount(); ++col) {
        m_parameterTable->setColumnHidden(col, !visible);
    }
    if (m_toggleDetailColumnsButton) {
        m_toggleDetailColumnsButton->blockSignals(true);
        m_toggleDetailColumnsButton->setChecked(visible);
        m_toggleDetailColumnsButton->setText(visible ? QStringLiteral("收起明细列") : QStringLiteral("展开明细列"));
        m_toggleDetailColumnsButton->setIcon(style()->standardIcon(visible ? QStyle::SP_ArrowLeft : QStyle::SP_ArrowRight));
        m_toggleDetailColumnsButton->blockSignals(false);
    }
}

void InspectorPanel::setPanelMode(PanelMode mode)
{
    m_panelMode = mode;
    const bool inspectionMode = (m_panelMode == PanelMode::Inspection);
    if (m_stateGroup) {
        m_stateGroup->setVisible(inspectionMode);
    }
    if (m_contextGroup) {
        m_contextGroup->setVisible(inspectionMode);
    }
    if (m_paramGroup) {
        m_paramGroup->setTitle(inspectionMode ? QStringLiteral("参数检查") : QStringLiteral("PID 参数"));
    }
    if (m_toggleDetailColumnsButton) {
        m_toggleDetailColumnsButton->setVisible(!inspectionMode);
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
    if (m_paramHint) {
        m_paramHint->setVisible(!inspectionMode);
    }
    configureParameterTable(m_parameterTable, inspectionMode);
    setDetailColumnsVisible(false);
    refreshParameterTable();
}

void InspectorPanel::setProjectPath(const QString& projectPath)
{
    const QString text = projectPath.isEmpty() ? QStringLiteral("-") : projectPath;
    if (auto* label = dynamic_cast<ElidedLabel*>(m_projectPathValue)) {
        label->setFullText(text);
    } else {
        m_projectPathValue->setText(text);
    }
}

void InspectorPanel::setCurrentFile(const QString& filePath)
{
    const QString text = filePath.isEmpty() ? QStringLiteral("-") : filePath;
    if (auto* label = dynamic_cast<ElidedLabel*>(m_currentFileValue)) {
        label->setFullText(text);
    } else {
        m_currentFileValue->setText(text);
    }
}

void InspectorPanel::setWorkspaceName(const QString& workspaceName)
{
    const QString text = workspaceName.isEmpty() ? QStringLiteral("-") : workspaceName;
    if (auto* label = dynamic_cast<ElidedLabel*>(m_workspaceValue)) {
        label->setFullText(text);
    } else {
        m_workspaceValue->setText(text);
    }
}

void InspectorPanel::setRuntimeState(const QString& runtimeState)
{
    if (!m_runtimeValue) {
        return;
    }
    const QString state = runtimeState.isEmpty() ? "-" : runtimeState;
    m_runtimeValue->setText(state);
    applyStateStyle(m_runtimeValue, state);
}

void InspectorPanel::setBuildState(const QString& buildState)
{
    if (!m_buildValue) {
        return;
    }
    const QString state = buildState.isEmpty() ? "-" : buildState;
    m_buildValue->setText(state);
    applyStateStyle(m_buildValue, state);
}

void InspectorPanel::setMonitoringState(const QString& monitoringState)
{
    if (!m_monitorValue) {
        return;
    }
    const QString state = monitoringState.isEmpty() ? "-" : monitoringState;
    m_monitorValue->setText(state);
    applyStateStyle(m_monitorValue, state);
}

void InspectorPanel::setDownloadState(const QString& downloadState)
{
    if (!m_downloadValue) {
        return;
    }
    const QString state = downloadState.isEmpty() ? QStringLiteral("-") : downloadState;
    const QString shortState = state.section(QStringLiteral(" | "), 0, 0).trimmed();
    m_downloadValue->setText(shortState.isEmpty() ? QStringLiteral("-") : shortState);
    m_downloadValue->setToolTip(state == QStringLiteral("-") ? QString() : state);
    applyStateStyle(m_downloadValue, state);
}

void InspectorPanel::setOpcState(const QString& opcState)
{
    if (!m_opcValue) {
        return;
    }
    const QString state = opcState.isEmpty() ? QStringLiteral("-") : opcState;
    const QString shortState = state.section(QStringLiteral(" | "), 0, 0).trimmed();
    m_opcValue->setText(shortState.isEmpty() ? QStringLiteral("-") : shortState);
    m_opcValue->setToolTip(state == QStringLiteral("-") ? QString() : state);
    applyStateStyle(m_opcValue, state);
}

void InspectorPanel::setVariableSummary(const QString& summary)
{
    const QString text = summary.isEmpty() ? QStringLiteral("-") : summary;
    if (auto* label = dynamic_cast<ElidedLabel*>(m_variableValue)) {
        label->setFullText(text);
    } else {
        m_variableValue->setText(text);
    }
}

void InspectorPanel::setParameterSummary(const QString& summary)
{
    const QString text = summary.isEmpty() ? QStringLiteral("-") : summary;
    if (auto* label = dynamic_cast<ElidedLabel*>(m_parameterValue)) {
        label->setFullText(text);
    } else {
        m_parameterValue->setText(text);
    }
}

void InspectorPanel::setResourceSummary(const QString& summary)
{
    const QString text = summary.isEmpty() ? QStringLiteral("-") : summary;
    if (auto* label = dynamic_cast<ElidedLabel*>(m_resourceValue)) {
        label->setFullText(text);
    } else {
        m_resourceValue->setText(text);
    }
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
        auto* nameItem = makeParameterItem(p.name);
        auto* valueItem = makeParameterItem(current);
        m_parameterTable->setItem(row, 0, nameItem);
        if (m_panelMode == PanelMode::Inspection) {
            const QString stateText = current == p.defaultValue ? QStringLiteral("未变更") : QStringLiteral("已变更");
            const QString issueText = issueSummaryFor(p);
            const QString detailToolTip = QStringLiteral("默认值: %1\n确认: %2\n回读: %3\n偏差: %4")
                                              .arg(p.defaultValue,
                                                   p.confirmed ? QStringLiteral("已确认") : QStringLiteral("待确认"),
                                                   readbackStateFor(p),
                                                   deviationStateFor(p));
            auto* stateItem = makeParameterItem(stateText);
            auto* issueItem = makeParameterItem(issueText, detailToolTip);
            setErrorText(stateItem, stateText == QStringLiteral("已变更"));
            setErrorText(issueItem, issueText != QStringLiteral("正常"));
            m_parameterTable->setItem(row, 1, valueItem);
            m_parameterTable->setItem(row, 2, stateItem);
            m_parameterTable->setItem(row, 3, issueItem);
        } else {
            // 状态列：优先使用 ParameterController 的状态，并直接呈现错误与偏差原因
            QString stateText;
            bool stateIsError = false;
            auto stIt = m_parameterStateMap.constFind(p.name);
            QString readText = QStringLiteral("未读取");
            QString pendingText = QStringLiteral("-");
            if (stIt != m_parameterStateMap.constEnd()) {
                if (!stIt->readbackValue.isEmpty()) {
                    readText = stIt->readbackValue;
                } else if (p.confirmed && !p.currentValue.isEmpty()) {
                    readText = p.currentValue;
                }

                // 待应用值生命周期：确认成功或初始干净状态清除待应用标记，未应用/失败/超时保留以供重试
                if (stIt->state == ParameterState::Confirmed || stIt->state == ParameterState::Clean) {
                    pendingText = QStringLiteral("-");
                } else if (!stIt->editedValue.isEmpty()) {
                    pendingText = stIt->editedValue;
                }

                stateText = parameterStateText(stIt->state);
                stateIsError = (stIt->state == ParameterState::Modified
                                || stIt->state == ParameterState::ApplyFailed
                                || stIt->state == ParameterState::Mismatch
                                || stIt->state == ParameterState::Timeout);
                if (stIt->state == ParameterState::ApplyFailed && !stIt->lastError.isEmpty()) {
                    stateText = QStringLiteral("应用失败: %1").arg(stIt->lastError);
                } else if (stIt->state == ParameterState::Mismatch) {
                    const auto devIt = m_parameterDeviationMap.constFind(p.name);
                    if (devIt != m_parameterDeviationMap.constEnd()) {
                        stateText = QStringLiteral("偏差超限 (%1)").arg(QString::number(devIt.value(), 'f', 3));
                    } else if (!stIt->readbackValue.isEmpty()) {
                        stateText = QStringLiteral("偏差 (回读 %1)").arg(stIt->readbackValue);
                    }
                } else if (stIt->state == ParameterState::Timeout) {
                    stateText = QStringLiteral("回读超时");
                }
            } else {
                if (p.confirmed && !p.currentValue.isEmpty()) {
                    readText = p.currentValue;
                    stateText = QStringLiteral("已确认");
                } else {
                    readText = QStringLiteral("未读取");
                    stateText = QStringLiteral("未读取");
                }
            }

            const QString localCfg = p.defaultValue.isEmpty() ? (p.currentValue.isEmpty() ? QStringLiteral("-") : p.currentValue) : p.defaultValue;
            const QString detailToolTip = QStringLiteral("本地配置值: %1\n已读取值: %2\n待应用值: %3\n状态: %4\n确认: %5\n回读: %6\n偏差: %7")
                                              .arg(localCfg,
                                                   readText,
                                                   pendingText,
                                                   stateText,
                                                   p.confirmed ? QStringLiteral("已确认") : QStringLiteral("待确认"),
                                                   readbackStateFor(p),
                                                   deviationStateFor(p));
            auto* readItem = makeParameterItem(readText, QStringLiteral("本地配置值: %1").arg(localCfg));
            auto* pendingItem = makeParameterItem(pendingText);
            auto* stateItem = makeParameterItem(stateText, detailToolTip);
            auto* confirmItem = makeParameterItem(p.confirmed ? QStringLiteral("已确认") : QStringLiteral("待确认"));
            auto* readbackItem = makeParameterItem(readbackStateFor(p));
            auto* deviationItem = makeParameterItem(deviationStateFor(p));
            setErrorText(pendingItem, pendingText != QStringLiteral("-"));
            setErrorText(stateItem, stateIsError);
            setErrorText(confirmItem, !p.confirmed);
            setErrorText(readbackItem, readbackItem->text() == QStringLiteral("待回读"));
            setErrorText(deviationItem, deviationItem->text() != QStringLiteral("无"));
            m_parameterTable->setItem(row, 1, readItem);
            m_parameterTable->setItem(row, 2, pendingItem);
            m_parameterTable->setItem(row, 3, stateItem);
            m_parameterTable->setItem(row, 4, confirmItem);
            m_parameterTable->setItem(row, 5, readbackItem);
            m_parameterTable->setItem(row, 6, deviationItem);
        }
    }
    if (m_panelMode == PanelMode::Inspection) {
        m_parameterTable->verticalHeader()->setDefaultSectionSize(26);
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

QString InspectorPanel::issueSummaryFor(const ParameterDefinition& parameter) const
{
    QStringList issues;
    if (!parameter.confirmed) {
        issues.append(QStringLiteral("待确认"));
    }
    const QString readbackState = readbackStateFor(parameter);
    if (readbackState == QStringLiteral("待回读")) {
        issues.append(readbackState);
    }
    const QString deviationState = deviationStateFor(parameter);
    if (deviationState != QStringLiteral("无")) {
        issues.append(QStringLiteral("偏差 %1").arg(deviationState));
    }
    return issues.isEmpty() ? QStringLiteral("正常") : issues.join(QStringLiteral(" / "));
}

void InspectorPanel::onParameterItemDoubleClicked(QTableWidgetItem* item)
{
    if (m_panelMode == PanelMode::Inspection) {
        return;
    }
    if (!item || (item->column() != 1 && item->column() != 2) || !m_parameterTable) {
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
    if (state.contains(QStringLiteral("错误")) ||
        state.contains(QStringLiteral("失败")) ||
        state.contains(QStringLiteral("拒绝")) ||
        state.contains(QStringLiteral("未连接"))) {
        visualState = QStringLiteral("error");
    } else if (state == QStringLiteral("运行中") ||
        state == QStringLiteral("活动") ||
        state == QStringLiteral("空闲") ||
        state.contains(QStringLiteral("成功"))) {
        visualState = QStringLiteral("success");
    } else if (state == QStringLiteral("忙碌") ||
               state.contains(QStringLiteral("校验中")) ||
               state.contains(QStringLiteral("下载中")) ||
               state.contains(QStringLiteral("重试中")) ||
               state.contains(QStringLiteral("前置校验")) ||
               state.contains(QStringLiteral("警告"))) {
        visualState = QStringLiteral("warning");
    }

    label->setProperty("state", QVariant(visualState));
    label->style()->unpolish(label);
    label->style()->polish(label);
}

void InspectorPanel::clearSelection()
{
    m_hasSelection = false;
    m_selectedType.clear();
    m_selectedName.clear();

    if (m_selectionHintLabel) {
        m_selectionHintLabel->setVisible(true);
    }
    if (m_selectedGroup) {
        m_selectedGroup->setVisible(false);
    }
    if (m_contextGroup) {
        m_contextGroup->setVisible(false);
    }
}

void InspectorPanel::setSelectedObject(const QString& type, const QString& name, const QMap<QString, QString>& properties)
{
    m_hasSelection = true;
    m_selectedType = type;
    m_selectedName = name;

    if (m_selectionHintLabel) {
        m_selectionHintLabel->setVisible(false);
    }
    if (m_contextGroup) {
        m_contextGroup->setVisible(false);
    }
    if (m_selectedGroup) {
        m_selectedGroup->setVisible(true);
    }
    if (m_selectedTypeValue) {
        m_selectedTypeValue->setText(type);
        m_selectedTypeValue->setToolTip(type);
    }
    if (m_selectedNameValue) {
        m_selectedNameValue->setText(name);
        m_selectedNameValue->setToolTip(name);
    }

    if (m_selectedPropsTable) {
        m_selectedPropsTable->setRowCount(properties.size());
        int row = 0;
        for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
            auto* kItem = new QTableWidgetItem(it.key());
            kItem->setToolTip(it.key());
            auto* vItem = new QTableWidgetItem(it.value());
            vItem->setToolTip(it.value());
            m_selectedPropsTable->setItem(row, 0, kItem);
            m_selectedPropsTable->setItem(row, 1, vItem);
            ++row;
        }
    }
}

void InspectorPanel::onParameterSelectionChanged()
{
    if (!m_parameterTable) {
        return;
    }
    const auto items = m_parameterTable->selectedItems();
    if (items.isEmpty()) {
        clearSelection();
        return;
    }
    const int row = items.first()->row();
    const QString paramName = m_parameterTable->item(row, 0) ? m_parameterTable->item(row, 0)->text() : QString();
    if (paramName.isEmpty()) {
        clearSelection();
        return;
    }

    QMap<QString, QString> props;
    for (const auto& p : m_parameterData) {
        if (p.name == paramName) {
            props.insert(QStringLiteral("当前值"), p.currentValue.isEmpty() ? p.defaultValue : p.currentValue);
            props.insert(QStringLiteral("默认值"), p.defaultValue);
            props.insert(QStringLiteral("数据类型"), p.dataType.isEmpty() ? QStringLiteral("LREAL") : p.dataType);
            props.insert(QStringLiteral("单位"), p.unit.isEmpty() ? QStringLiteral("-") : p.unit);
            if (!p.minValue.isEmpty()) {
                props.insert(QStringLiteral("最小值"), p.minValue);
            }
            if (!p.maxValue.isEmpty()) {
                props.insert(QStringLiteral("最大值"), p.maxValue);
            }
            props.insert(QStringLiteral("在线可改"), p.onlineEditable ? QStringLiteral("是") : QStringLiteral("否"));
            const QString desc = p.metadata.value(QStringLiteral("description")).toString();
            if (!desc.isEmpty()) {
                props.insert(QStringLiteral("描述"), desc);
            }
            break;
        }
    }
    setSelectedObject(QStringLiteral("参数"), paramName, props);
}
