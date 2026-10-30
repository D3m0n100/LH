#pragma once

#include <QWidget>

class QLabel;
class QToolButton;
class QTimer;

class NotificationBanner : public QWidget
{
    Q_OBJECT

public:
    explicit NotificationBanner(QWidget* parent = nullptr);

    void showNotification(const QString& severity, const QString& title, const QString& details);
    void clear();
    QString currentText() const;
    int repeatCount() const { return m_repeat; }

signals:
    void detailsRequested();

private:
    void updateSeverityStyle();

    QLabel* m_message = nullptr;
    QToolButton* m_details = nullptr;
    QToolButton* m_close = nullptr;
    QTimer* m_autoHide = nullptr;
    QString m_key;
    QString m_detailsText;
    int m_repeat = 0;
};