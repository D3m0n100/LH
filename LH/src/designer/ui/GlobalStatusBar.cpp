#include "GlobalStatusBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVariant>

GlobalStatusBar::GlobalStatusBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("GlobalStatusBar");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(0);

    m_projectLabel = createStatusItem("项目: 无");
    m_connectionLabel = createStatusItem("连接: 离线");
    m_protocolLabel = createStatusItem("协议: 未知");
    m_samplingLabel = createStatusItem("采样: 0 Hz");
    m_latencyLabel = createStatusItem("延迟: -- ms");
    m_alarmLabel = createStatusItem("告警: 0");
    m_buildLabel = createStatusItem("构建: 空闲");

    layout->addWidget(m_projectLabel);
    layout->addWidget(m_connectionLabel);
    layout->addWidget(m_protocolLabel);
    layout->addWidget(m_samplingLabel);
    layout->addWidget(m_latencyLabel);
    layout->addWidget(m_alarmLabel);
    layout->addWidget(m_buildLabel);
    layout->addStretch();
}

void GlobalStatusBar::setProjectName(const QString& name)
{
    setItemText(m_projectLabel, "项目", name.isEmpty() ? "无" : name);
}

void GlobalStatusBar::setConnectionState(bool connected)
{
    setItemText(m_connectionLabel, "连接", connected ? "在线" : "离线");
    setItemState(m_connectionLabel, connected ? "success" : "muted");
}

void GlobalStatusBar::setProtocolName(const QString& protocol)
{
    setItemText(m_protocolLabel, "协议", protocol.isEmpty() ? "未知" : protocol);
}

void GlobalStatusBar::setSamplingRateHz(int rateHz)
{
    setItemText(m_samplingLabel, "采样", QString::number(rateHz) + " Hz");
}

void GlobalStatusBar::setLatencyMs(int latencyMs)
{
    const QString value = (latencyMs < 0) ? QStringLiteral("-- ms") : QString::number(latencyMs) + " ms";
    setItemText(m_latencyLabel, "延迟", value);
}

void GlobalStatusBar::setAlarmCount(int count)
{
    setItemText(m_alarmLabel, "告警", QString::number(count));
    setItemState(m_alarmLabel, count > 0 ? "warning" : "muted");
}

void GlobalStatusBar::setBuildState(const QString& stateText)
{
    setItemText(m_buildLabel, "构建", stateText.isEmpty() ? "空闲" : stateText);
    const QString normalized = stateText.trimmed();
    if (normalized.contains("失败") || normalized.contains("错误")) {
        setItemState(m_buildLabel, "error");
    } else if (normalized.contains("成功") || normalized.contains("运行")) {
        setItemState(m_buildLabel, "success");
    } else if (normalized.contains("编译") || normalized.contains("中")) {
        setItemState(m_buildLabel, "active");
    } else {
        setItemState(m_buildLabel, "muted");
    }
}

QLabel* GlobalStatusBar::createStatusItem(const QString& text)
{
    auto* label = new QLabel(text, this);
    label->setObjectName("GlobalStatusItem");
    label->setProperty("state", QVariant(QStringLiteral("muted")));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

void GlobalStatusBar::setItemText(QLabel* label, const QString& prefix, const QString& value)
{
    if (!label) {
        return;
    }
    label->setText(prefix + ": " + value);
}

void GlobalStatusBar::setItemState(QLabel* label, const QString& state)
{
    if (!label) {
        return;
    }
    label->setProperty("state", QVariant(state));
    label->style()->unpolish(label);
    label->style()->polish(label);
}

