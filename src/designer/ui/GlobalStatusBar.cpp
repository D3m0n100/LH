#include "GlobalStatusBar.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QStyle>
#include <QToolButton>
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
    m_buildLabel = createStatusItem(QStringLiteral("状态: 空闲"));
    m_problemLabel = createStatusItem(QStringLiteral("问题: 0"));
    m_alarmLabel = createStatusItem(QStringLiteral("告警: 0"));

    m_problemLabel->setObjectName("GlobalStatusProblemItem");
    m_problemLabel->setToolTip(QStringLiteral("点击查看问题列表"));
    m_problemLabel->setCursor(Qt::PointingHandCursor);
    m_problemLabel->installEventFilter(this);

    m_alarmLabel->setObjectName("GlobalStatusAlarmItem");
    m_alarmLabel->setToolTip(QStringLiteral("点击查看监控告警"));
    m_alarmLabel->setCursor(Qt::PointingHandCursor);
    m_alarmLabel->installEventFilter(this);

    m_detailsToggleBtn = new QToolButton(this);
    m_detailsToggleBtn->setObjectName("GlobalStatusDetailsToggle");
    m_detailsToggleBtn->setText(QStringLiteral("详情 ▸"));
    m_detailsToggleBtn->setToolTip(QStringLiteral("展开/折叠详细状态（协议、采样、延迟、OPC）"));
    m_detailsToggleBtn->setCursor(Qt::PointingHandCursor);
    connect(m_detailsToggleBtn, &QToolButton::clicked, this, &GlobalStatusBar::toggleDetails);

    m_detailsWidget = new QWidget(this);
    m_detailsWidget->setObjectName("GlobalStatusDetails");
    auto* detailsLayout = new QHBoxLayout(m_detailsWidget);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    detailsLayout->setSpacing(0);

    m_protocolLabel = createStatusItem(QStringLiteral("协议: 未知"));
    m_samplingLabel = createStatusItem(QStringLiteral("采样: 0 Hz"));
    m_latencyLabel = createStatusItem(QStringLiteral("延迟: -- ms"));
    m_opcLabel = createStatusItem(QStringLiteral("OPC: 关闭"));

    detailsLayout->addWidget(m_protocolLabel);
    detailsLayout->addWidget(m_samplingLabel);
    detailsLayout->addWidget(m_latencyLabel);
    detailsLayout->addWidget(m_opcLabel);
    m_detailsWidget->setVisible(false);

    layout->addWidget(m_projectLabel);
    layout->addWidget(m_connectionLabel);
    layout->addWidget(m_buildLabel);
    layout->addWidget(m_problemLabel);
    layout->addWidget(m_alarmLabel);
    layout->addWidget(m_detailsToggleBtn);
    layout->addWidget(m_detailsWidget);
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

void GlobalStatusBar::setProblemCount(int count)
{
    m_problemCount = count;
    setItemText(m_problemLabel, QStringLiteral("问题"), QString::number(count));
    setItemState(m_problemLabel, count > 0 ? QStringLiteral("error") : QStringLiteral("muted"));
}

void GlobalStatusBar::setAlarmCount(int count)
{
    m_alarmCount = count;
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
        // 异常不能因默认折叠而不可发现：自动展开详情
        setDetailsExpanded(true);
        return;
    }

    m_opcLabel->setToolTip(QString());
    setItemText(m_opcLabel, QStringLiteral("OPC"), QStringLiteral("关闭"));
    setItemState(m_opcLabel, QStringLiteral("muted"));
}

void GlobalStatusBar::setDetailsExpanded(bool expanded)
{
    if (m_detailsExpanded == expanded) {
        return;
    }
    m_detailsExpanded = expanded;
    if (m_detailsWidget) {
        m_detailsWidget->setVisible(expanded);
    }
    if (m_detailsToggleBtn) {
        m_detailsToggleBtn->setText(expanded ? QStringLiteral("详情 ◂") : QStringLiteral("详情 ▸"));
    }
    emit detailsToggled(expanded);
}

void GlobalStatusBar::toggleDetails()
{
    setDetailsExpanded(!m_detailsExpanded);
}

bool GlobalStatusBar::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (watched == m_problemLabel) {
                emit problemClicked();
                return true;
            }
            if (watched == m_alarmLabel) {
                emit alarmClicked();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

QLabel* GlobalStatusBar::createStatusItem(const QString& text)
{
    auto* label = new QLabel(text, this);
    label->setObjectName("GlobalStatusItem");
    label->setProperty("state", QVariant(QStringLiteral("muted")));
    label->setTextInteractionFlags(Qt::NoTextInteraction);
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

