#ifndef COMMAND_PALETTE_DIALOG_H
#define COMMAND_PALETTE_DIALOG_H

#include <QDialog>
#include <QPointer>
#include <QStringList>
#include <QList>
#include <QAtomicInt>
#include <memory>

class QLineEdit;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QAction;
class MainWindow;

class CommandPaletteDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        Command,    ///< 命令面板 (Ctrl+Shift+P / F1)
        QuickOpen,  ///< 快速打开文件 (Ctrl+P)
        GotoLine    ///< 跳转到行 (Ctrl+G)
    };

    explicit CommandPaletteDialog(MainWindow* parent, Mode mode = Mode::Command);
    ~CommandPaletteDialog() override;

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    /// 供测试或外部显式刷新文件缓存
    void setFileList(const QStringList& fileList);
    static QStringList scanFiles(const QString& rootPath,
        const std::shared_ptr<QAtomicInt>& cancelled, int limit = 5000);

public slots:
    void reject() override;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onFilterTextChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);

private:
    void setupUi();
    void populateCommands();
    void startQuickOpenScan();
    void updateList(const QString& filter);
    void executeCurrentSelection();

private:
    MainWindow* m_mainWindow = nullptr;
    Mode m_mode = Mode::Command;
    QPointer<QWidget> m_previousFocusWidget;

    QLabel* m_scanStatus = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QListWidget* m_listWidget = nullptr;

    struct ActionEntry {
        QString displayName;
        QString shortcutText;
        QPointer<QAction> action;
    };
    QList<ActionEntry> m_actions;

    QStringList m_scannedFiles;
    quint64 m_scanTaskId = 0;
    QString m_scanSessionId;
    std::shared_ptr<QAtomicInt> m_scanCancelled;
};

#endif // COMMAND_PALETTE_DIALOG_H
