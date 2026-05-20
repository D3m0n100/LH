// 文件：src/designer/DownloadDockWidget.h
// 最小可用的下载 Dock：连接/探测/下载 + 进度 + 日志

#pragma once
#include <QWidget>
#include <QVariantMap>

class QComboBox;
class QSpinBox;
class QPushButton;
class QProgressBar;
class QTextEdit;
class QLabel;

class DownloadManager;

class DownloadDockWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DownloadDockWidget(QWidget* parent = nullptr);

private slots:
    void onConnectProbe();
    void onDownload();

private:
    QVariantMap buildCommConfig() const;
    void appendLog(const QString& s);

private:
    QComboBox* m_port = nullptr;
    QComboBox* m_baud = nullptr;
    QSpinBox*  m_targetId = nullptr;

    QPushButton* m_btnConnectProbe = nullptr;
    QPushButton* m_btnDownload = nullptr;

    QLabel* m_status = nullptr;
    QProgressBar* m_prog = nullptr;
    QTextEdit* m_log = nullptr;

    DownloadManager* m_mgr = nullptr;
};