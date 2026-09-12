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

    m_projectLabel = createStatusItem(QStringLiteral("项目: 无"));
    m_connectionLabel = createStatusItem(QStringLiteral("连接: 离线"));
    m_protocolLabel = createStatusItem(QStringLiteral("协议: 未知"));
    m_samplingLabel = createStatusItem(QStringLiteral("采样: 0 Hz"));
    m_latencyLabel = createStatusItem(QStringLiteral("延迟: -- ms"));
    m_alarmLabel = createStatusItem(QStringLiteral("告警: 0"));
    m_buildLabel = createStatusItem(QStringLiteral("状态: 空闲"));
    m_opcLabel = createStatusItem(QStringLiteral("OPC: 关闭"));

    layout->addWidget(m_projectLabel);
    layout->addWidget(m_connectionLabel);
    layout->addWidget(m_protocolLabel);
    layout->addWidget(m_samplingLabel);
    layout->addWidget(m_latencyLabel);
    layout->addWidget(m_alarmLabel);
    layout->addWidget(m_buildLabel);
    layout->addWidget(m_opcLabel);
    layout->addStretch();
}

void GlobalStatusBar::setProjectName(const QString& name)
{
    setItemText(m_projectLabel, QStringLiteral("项目"), name.isEmpty() ? QStringLiteral("无") : name);
}

void GlobalStatusBar::setConnectionState(bool connected)
{
    setItemText(m_connectionLabel, QStringLiteral("连接"), connected ? QStringLiteral("在线") : QStringLiteral("离线"));
    setItemState(m_connectionLabel, connected ? QStringLiteral("success") : QStringLiteral("muted"));
}

void GlobalStatusBar::setProtocolName(const QString& protocol)
{
    setItemText(m_protocolLabel, QStringLiteral("协议"), protocol.isEmpty() ? QStringLiteral("未知") : protocol);
}

void GlobalStatusBar::setSamplingRateHz(int rateHz)
{
    setItemText(m_samplingLabel, QStringLiteral("采样"), QString::number(rateHz) + QStringLiteral(" Hz"));
}

void GlobalStatusBar::setLatencyMs(int latencyMs)
{
    const QString value = (latencyMs < 0) ? QStringLiteral("-- ms") : QString::number(latencyMs) + " ms";
    setItemText(m_latencyLabel, QStringLiteral("延迟"), value);
}

void GlobalStatusBar::setAlarmCount(int count)
{
    setItemText(m_alarmLabel, QStringLiteral("告警"), QString::number(count));
    setItemState(m_alarmLabel, count > 0 ? QStringLiteral("warning") : QStringLiteral("muted"));
}

void GlobalStatusBar::setBuildState(const QString& stateText)
{
    setItemText(m_buildLabel, QStringLiteral("状态"), stateText.isEmpty() ? QStringLiteral("空闲") : stateText);
    const QString normalized = stateText.trimmed();
    if (normalized.contains(QStringLiteral("失败")) || normalized.contains(QStringLiteral("错误"))) {
        setItemState(m_buildLabel, QStringLiteral("error"));
    } else if (normalized.contains(QStringLiteral("成功"))
               || normalized.contains(QStringLiteral("运行"))) {
        setItemState(m_buildLabel, QStringLiteral("success"));
    } else if (normalized.contains(QStringLiteral("编译"))
               || normalized.contains(QStringLiteral("中"))) {
        setItemState(m_buildLabel, QStringLiteral("active"));
    } else {
        setItemState(m_buildLabel, QStringLiteral("muted"));
    }
}

void GlobalStatusBar::setOpcState(bool running, const QString& errorMessage)
{
    if (running) {
        setItemText(m_opcLabel, QStringLiteral("OPC"), QStringLiteral("运行中"));
        setItemState(m_opcLabel, QStringLiteral("success"));
        return;
    }

    if (!errorMessage.trimmed().isEmpty()) {
        setItemText(m_opcLabel, QStringLiteral("OPC"), QStringLiteral("错误"));
        setItemState(m_opcLabel, QStringLiteral("error"));
        m_opcLabel->setToolTip(errorMessage);
        return;
    }

    m_opcLabel->setToolTip(QString());
    setItemText(m_opcLabel, QStringLiteral("OPC"), QStringLiteral("关闭"));
    setItemState(m_opcLabel, QStringLiteral("muted"));
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

