#ifndef GLOBAL_STATUS_BAR_H
#define GLOBAL_STATUS_BAR_H

#include <QWidget>

class QLabel;
class QToolButton;

class GlobalStatusBar : public QWidget
{
    Q_OBJECT

public:
    explicit GlobalStatusBar(QWidget* parent = nullptr);

    void setProjectName(const QString& name);
    void setConnectionState(bool connected);
    void setProtocolName(const QString& protocol);
    void setSamplingRateHz(int rateHz);
    void setLatencyMs(int latencyMs);
    void setProblemCount(int count);
    void setAlarmCount(int count);
    int problemCount() const { return m_problemCount; }
    int alarmCount() const { return m_alarmCount; }
    void setBuildState(const QString& stateText);
    void setOpcState(bool running, const QString& errorMessage = QString());

    void setDetailsExpanded(bool expanded);
    bool isDetailsExpanded() const { return m_detailsExpanded; }
    void toggleDetails();

signals:
    void problemClicked();
    void alarmClicked();
    void detailsToggled(bool expanded);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QLabel* createStatusItem(const QString& text);
    void setItemText(QLabel* label, const QString& prefix, const QString& value);
    void setItemState(QLabel* label, const QString& state);

private:
    QLabel* m_projectLabel = nullptr;
    QLabel* m_connectionLabel = nullptr;
    QLabel* m_buildLabel = nullptr;
    QLabel* m_problemLabel = nullptr;
    QLabel* m_alarmLabel = nullptr;
    QToolButton* m_detailsToggleBtn = nullptr;

    QWidget* m_detailsWidget = nullptr;
    QLabel* m_protocolLabel = nullptr;
    QLabel* m_samplingLabel = nullptr;
    QLabel* m_latencyLabel = nullptr;
    QLabel* m_opcLabel = nullptr;

    int m_problemCount = 0;
    int m_alarmCount = 0;
    bool m_detailsExpanded = false;
};

#endif // GLOBAL_STATUS_BAR_H
