#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QString>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
QT_END_NAMESPACE

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override;

    QString defaultProjectDir() const;
    bool autoScrollLog() const;
    int fontSizeIndex() const;

    void setDefaultProjectDir(const QString& dir);
    void setAutoScrollLog(bool enabled);
    void setFontSizeIndex(int index);

    static int fontPointSize(int index);

private slots:
    void onBrowseDefaultDir();

private:
    void createUi();
    void createConnections();

private:
    QLineEdit* m_editDefaultDir = nullptr;
    QPushButton* m_btnBrowseDir = nullptr;
    QCheckBox* m_chkAutoScroll = nullptr;
    QComboBox* m_comboFontSize = nullptr;

    QPushButton* m_btnOk = nullptr;
    QPushButton* m_btnCancel = nullptr;
};

#endif // SETTINGSDIALOG_H
