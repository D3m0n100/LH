#include "NotificationBanner.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVariant>

namespace {
// A notification must never raise the main window's minimum width. The label
// reports zero minimum width and paints an elided single line; the full text
// remains available through the banner tooltip and the problems panel.
class ElidedNotificationLabel final : public QLabel
{
public:
    explicit ElidedNotificationLabel(QWidget* parent = nullptr) : QLabel(parent)
    {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setMinimumWidth(0);
    }

    QSize minimumSizeHint() const override
    {
        return QSize(0, fontMetrics().height() + 2);
    }

    QSize sizeHint() const override
    {
        return QSize(0, fontMetrics().height() + 2);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setPen(palette().color(QPalette::WindowText));
        const QString shown = fontMetrics().elidedText(text(), Qt::ElideRight, width());
        painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, shown);
    }
};
}

NotificationBanner::NotificationBanner(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("NotificationBanner"));
    setProperty("severity", QStringLiteral("info"));
    setMinimumWidth(0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setVisible(false);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 4, 8, 4);
    layout->setSpacing(8);
    m_message = new ElidedNotificationLabel(this);
    m_message->setObjectName(QStringLiteral("NotificationText"));
    m_message->setMinimumWidth(0);
    m_message->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(m_message, 1);

    m_details = new QToolButton(this);
    m_details->setObjectName(QStringLiteral("NotificationDetailsButton"));
    m_details->setText(tr("详情"));
    m_details->setAutoRaise(true);
    connect(m_details, &QToolButton::clicked, this, &NotificationBanner::detailsRequested);
    layout->addWidget(m_details);

    m_close = new QToolButton(this);
    m_close->setObjectName(QStringLiteral("NotificationCloseButton"));
    m_close->setText(QStringLiteral("×"));
    m_close->setToolTip(tr("关闭通知"));
    m_close->setAutoRaise(true);
    connect(m_close, &QToolButton::clicked, this, [this] { clear(); });
    layout->addWidget(m_close);

    m_autoHide = new QTimer(this);
    m_autoHide->setSingleShot(true);
    connect(m_autoHide, &QTimer::timeout, this, [this] { clear(); });
}

void NotificationBanner::showNotification(const QString& severity,
                                          const QString& title,
                                          const QString& details)
{
    const QString normalized = severity.trimmed().toLower();
    const QString key = normalized + QLatin1Char('\x1f') + title + QLatin1Char('\x1f') + details;
    if (m_key == key && isVisible()) {
        ++m_repeat;
    } else {
        m_key = key;
        m_repeat = 1;
    }
    m_detailsText = details;
    QString text = title.trimmed();
    if (!details.trimmed().isEmpty()) {
        QString firstLine = details.trimmed().section(QLatin1Char('\n'), 0, 0);
        if (firstLine.size() > 160) firstLine = firstLine.left(157) + QStringLiteral("...");
        text = text.isEmpty() ? firstLine : QStringLiteral("%1 — %2").arg(text, firstLine);
    }
    if (m_repeat > 1) text += tr("（×%1）").arg(m_repeat);
    m_message->setText(text);
    m_message->setToolTip(m_detailsText);
    setToolTip(m_detailsText);
    setProperty("severity", normalized.isEmpty() ? QStringLiteral("info") : normalized);
    updateSeverityStyle();
    setVisible(true);
    if (normalized == QStringLiteral("info") || normalized == QStringLiteral("success")) {
        m_autoHide->start(6000);
    } else {
        m_autoHide->stop();
    }
}

void NotificationBanner::clear()
{
    m_autoHide->stop();
    m_key.clear();
    m_repeat = 0;
    m_detailsText.clear();
    m_message->clear();
    setToolTip(QString());
    setVisible(false);
}

QString NotificationBanner::currentText() const
{
    return m_message ? m_message->text() : QString();
}

void NotificationBanner::updateSeverityStyle()
{
    style()->unpolish(this);
    style()->polish(this);
    update();
}