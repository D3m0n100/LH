#ifndef GLOBAL_STATUS_BAR_H
#define GLOBAL_STATUS_BAR_H

#include <QWidget>

class QLabel;

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
    void setAlarmCount(int count);
    void setBuildState(const QString& stateText);
    void setOpcState(bool running, const QString& errorMessage = QString());

private:
    QLabel* createStatusItem(const QString& text);
    void setItemText(QLabel* label, const QString& prefix, const QString& value);
    void setItemState(QLabel* label, const QString& state);

private:
    QLabel* m_projectLabel = nullptr;
    QLabel* m_connectionLabel = nullptr;
    QLabel* m_protocolLabel = nullptr;
    QLabel* m_samplingLabel = nullptr;
    QLabel* m_latencyLabel = nullptr;
    QLabel* m_alarmLabel = nullptr;
    QLabel* m_buildLabel = nullptr;
    QLabel* m_opcLabel = nullptr;
};

#endif // GLOBAL_STATUS_BAR_H
