#include "CommandPaletteDialog.h"
#include "MainWindow.h"

#include <QVBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QKeyEvent>
#include <QMenu>
#include <QAction>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QThread>
#include <QFutureWatcher>
#include <QLabel>
#include <QtConcurrent/QtConcurrent>
#include <QApplication>

CommandPaletteDialog::CommandPaletteDialog(MainWindow* parent, Mode mode)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Popup)
    , m_mainWindow(parent)
    , m_mode(mode)
    , m_previousFocusWidget(parent ? parent->focusWidget() : nullptr)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setupUi();
    setMode(mode);
}

CommandPaletteDialog::~CommandPaletteDialog()
{
    if (m_scanCancelled) {
        m_scanCancelled->storeRelaxed(1);
    }
}

void CommandPaletteDialog::setupUi()
{
    resize(560, 320);
    setStyleSheet(R"(
QDialog {
    background: #ffffff;
    border: 1px solid #007acc;
    border-radius: 6px;
}
QLineEdit {
    border: 1px solid #d0d7de;
    border-radius: 4px;
    padding: 6px 10px;
    font-size: 13px;
    background: #fdfdfd;
}
QLineEdit:focus {
    border-color: #007acc;
    background: #ffffff;
}
QListWidget {
    border: none;
    background: #ffffff;
    outline: none;
}
QListWidget::item {
    padding: 5px 8px;
    border-radius: 3px;
    font-size: 12px;
}
QListWidget::item:selected {
    background: #cce7ff;
    color: #1f1f1f;
}
QListWidget::item:disabled {
    color: #9a9a9a;
}
)");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setClearButtonEnabled(true);
    layout->addWidget(m_searchEdit);

    m_listWidget = new QListWidget(this);
    m_listWidget->setUniformItemSizes(true);
    layout->addWidget(m_listWidget, 1);
    m_scanStatus = new QLabel(this);
    m_scanStatus->setWordWrap(true);
    layout->addWidget(m_scanStatus);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &CommandPaletteDialog::onFilterTextChanged);
    connect(m_listWidget, &QListWidget::itemActivated, this, &CommandPaletteDialog::onItemActivated);
    connect(m_listWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        // 单击不触发执行，仅选定
        m_listWidget->setCurrentItem(item);
    });
}

void CommandPaletteDialog::setMode(Mode mode)
{
    m_mode = mode;
    m_searchEdit->clear();
    m_listWidget->clear();

    if (m_mode == Mode::Command) {
        m_searchEdit->setPlaceholderText(tr("输入命令名称以搜索... (↑↓ 选择，Enter 执行)"));
        populateCommands();
        updateList(QString());
    } else if (m_mode == Mode::QuickOpen) {
        m_searchEdit->setPlaceholderText(tr("输入文件名以搜索... (输入 : 跳转行号)"));
        startQuickOpenScan();
    } else if (m_mode == Mode::GotoLine) {
        m_searchEdit->setPlaceholderText(tr("输入行号跳转 (例如: 15 或 15:4)..."));
        m_searchEdit->setText(":");
    }

    if (m_mainWindow) {
        // 居中在主窗口顶部 15% 处
        const QRect parentGeo = m_mainWindow->geometry();
        const int x = parentGeo.x() + (parentGeo.width() - width()) / 2;
        const int y = parentGeo.y() + parentGeo.height() * 0.15;
        move(x, y);
    }

    m_searchEdit->setFocus();
}

void CommandPaletteDialog::populateCommands()
{
    m_actions.clear();
    if (!m_mainWindow) {
        return;
    }

    const auto allActions = m_mainWindow->findChildren<QAction*>();
    QSet<QString> seenNames;

    for (QAction* act : allActions) {
        if (!act || act->isSeparator() || act->text().trimmed().isEmpty()) {
            continue;
        }

        QString rawText = act->text();
        rawText.remove('&'); // 去除助记键符
        rawText = rawText.trimmed();

        // 提取所属菜单前缀
        QString category = QStringLiteral("常用");
        if (auto* menu = qobject_cast<QMenu*>(act->parentWidget())) {
            QString menuTitle = menu->title();
            menuTitle.remove('&');
            if (!menuTitle.trimmed().isEmpty()) {
                category = menuTitle.trimmed();
            }
        }

        QString fullDisplayName = QStringLiteral("%1: %2").arg(category, rawText);
        if (seenNames.contains(fullDisplayName)) {
            continue;
        }
        seenNames.insert(fullDisplayName);

        ActionEntry entry;
        entry.displayName = fullDisplayName;
        entry.shortcutText = act->shortcut().toString(QKeySequence::NativeText);
        entry.action = act;
        m_actions.append(entry);
    }
}

void CommandPaletteDialog::startQuickOpenScan()
{
    if (!m_mainWindow) {
        return;
    }

    const QString rootPath = m_mainWindow->resolveExplorerRootPath();
    if (rootPath.isEmpty() || !QDir(rootPath).exists()) {
        m_scannedFiles.clear();
        updateList(QString());
        return;
    }

    if (m_scanCancelled) {
        m_scanCancelled->storeRelaxed(1);
    }

    m_scanCancelled = std::make_shared<QAtomicInt>(0);
    m_scanTaskId = m_mainWindow->nextScanTaskId();
    m_scanSessionId = m_mainWindow->projectSessionId();

    const quint64 taskId = m_scanTaskId;
    const QString sessionId = m_scanSessionId;
    auto cancelFlag = m_scanCancelled;

    auto* watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this,
            [this, watcher, taskId, sessionId, cancelFlag]() {
        const auto files = watcher->result();
        watcher->deleteLater();
        if (cancelFlag->loadRelaxed() || !m_mainWindow
                || taskId != m_scanTaskId || sessionId != m_mainWindow->projectSessionId()) return;
        m_scannedFiles = files;
        m_scanStatus->setText(files.size() >= 5000
            ? tr("已达到 5000 个文件上限，请缩小工程范围") : tr("已索引 %1 个文件").arg(files.size()));
        updateList(m_searchEdit->text());
    });
    // Worker owns only value data. Destroying the watcher disconnects delivery;
    // no worker ever dereferences the dialog, even while it is being destroyed.
    watcher->setFuture(QtConcurrent::run([rootPath, cancelFlag]() {
        return scanFiles(rootPath, cancelFlag, 5000);
    }));
}

QStringList CommandPaletteDialog::scanFiles(const QString& rootPath,
        const std::shared_ptr<QAtomicInt>& cancelled, int limit)
{
    QStringList files, pending{rootPath};
    QSet<QString> visited;
    const QSet<QString> extensions{"lh", "json", "txt", "xml", "yaml", "yml", "ini",
                                  "cpp", "c", "h", "hpp", "qss", "cmake", "md", "log"};
    while (!pending.isEmpty() && files.size() < limit) {
        if (cancelled->loadRelaxed()) return {};
        const QString directory = pending.takeLast();
        const QString canonical = QFileInfo(directory).canonicalFilePath();
        if (canonical.isEmpty() || visited.contains(canonical)) continue;
        visited.insert(canonical);
        QDirIterator it(directory, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        while (it.hasNext() && files.size() < limit) {
            if (cancelled->loadRelaxed()) return {};
            it.next();
            const QFileInfo fi = it.fileInfo();
            if (fi.isDir()) {
                const auto name = fi.fileName().toLower();
                if (name.startsWith("build") || name == ".git" || name == ".cache"
                        || name == "tmp" || name == "node_modules" || name == "obj") continue;
                pending.append(fi.absoluteFilePath());
            } else if (fi.isFile() && (extensions.contains(fi.suffix().toLower())
                       || fi.fileName() == "CMakeLists.txt")) {
                files.append(fi.absoluteFilePath());
            }
        }
    }
    return files;
}

void CommandPaletteDialog::setFileList(const QStringList& fileList)
{
    m_scannedFiles = fileList;
    updateList(m_searchEdit->text());
}

void CommandPaletteDialog::onFilterTextChanged(const QString& text)
{
    if (m_mode == Mode::QuickOpen && text.startsWith(':')) {
        // 快速跳转行号模式指示
        m_listWidget->clear();
        auto* item = new QListWidgetItem(tr("跳转到当前文件指定行 (格式 :行号 或 :行号:列号)..."), m_listWidget);
        item->setFlags(Qt::NoItemFlags);
        return;
    }
    updateList(text);
}

void CommandPaletteDialog::updateList(const QString& filter)
{
    m_listWidget->clear();
    const QString trimmed = filter.trimmed();

    if (m_mode == Mode::Command) {
        for (const auto& entry : m_actions) {
            if (entry.action.isNull()) {
                continue;
            }
            if (!trimmed.isEmpty() && !entry.displayName.contains(trimmed, Qt::CaseInsensitive)) {
                continue;
            }

            auto* item = new QListWidgetItem(m_listWidget);
            QString label = entry.displayName;
            if (!entry.shortcutText.isEmpty()) {
                label += QStringLiteral("    [%1]").arg(entry.shortcutText);
            }
            item->setText(label);
            item->setData(Qt::UserRole, QVariant::fromValue(entry.action));

            if (!entry.action->isEnabled()) {
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            }
        }
    } else if (m_mode == Mode::QuickOpen) {
        const QString rootPath = m_mainWindow ? m_mainWindow->resolveExplorerRootPath() : QString();
        const QDir rootDir(rootPath);

        int count = 0;
        for (const QString& absPath : m_scannedFiles) {
            if (count >= 100) {
                break;
            }
            const QString fileName = QFileInfo(absPath).fileName();
            const QString relPath = rootPath.isEmpty() ? absPath : rootDir.relativeFilePath(absPath);

            if (!trimmed.isEmpty()) {
                if (!fileName.contains(trimmed, Qt::CaseInsensitive)
                    && !relPath.contains(trimmed, Qt::CaseInsensitive)) {
                    continue;
                }
            }

            auto* item = new QListWidgetItem(m_listWidget);
            item->setText(QStringLiteral("%1  (%2)").arg(fileName, relPath));
            item->setData(Qt::UserRole, absPath);
            ++count;
        }
    }

    if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
    }
}

void CommandPaletteDialog::executeCurrentSelection()
{
    const QString query = m_searchEdit->text().trimmed();

    // 1. 处理以冒号开头的跳行模式
    if (query.startsWith(':')) {
        const QString lineQuery = query.mid(1).trimmed();
        const QStringList parts = lineQuery.split(QRegularExpression("[,:]"));
        if (!parts.isEmpty() && !parts.first().isEmpty()) {
            const int targetLine = parts.first().toInt();
            int targetCol = -1;
            if (parts.size() > 1 && !parts.at(1).isEmpty()) {
                targetCol = parts.at(1).toInt();
            }
            accept();
            if (m_mainWindow && targetLine > 0) {
                m_mainWindow->navigateEditorPosition(m_mainWindow->getCurrentTextEditor(), targetLine, targetCol, true);
            }
            return;
        }
    }

    QListWidgetItem* item = m_listWidget->currentItem();
    if (!item) {
        return;
    }
    onItemActivated(item);
}

void CommandPaletteDialog::onItemActivated(QListWidgetItem* item)
{
    if (!item) {
        return;
    }

    if (m_mode == Mode::Command) {
        auto action = item->data(Qt::UserRole).value<QPointer<QAction>>();
        // 执行前严格二次校验：动作是否已被销毁、禁用或隐藏
        if (action.isNull() || !action->isEnabled() || !action->isVisible()) {
            return;
        }
        accept();
        // 命令接管焦点，不强制抢夺
        action->trigger();
    } else if (m_mode == Mode::QuickOpen) {
        const QString filePath = item->data(Qt::UserRole).toString();
        if (!filePath.isEmpty()) {
            accept();
            if (m_mainWindow) {
                QWidget* targetEditor = m_mainWindow->openAndActivateFile(filePath);
                if (targetEditor) {
                    targetEditor->setFocus();
                }
            }
        }
    } else if (m_mode == Mode::GotoLine) {
        executeCurrentSelection();
    }
}

void CommandPaletteDialog::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Up: {
        const int row = m_listWidget->currentRow();
        if (row > 0) {
            m_listWidget->setCurrentRow(row - 1);
        }
        event->accept();
        return;
    }
    case Qt::Key_Down: {
        const int row = m_listWidget->currentRow();
        if (row + 1 < m_listWidget->count()) {
            m_listWidget->setCurrentRow(row + 1);
        }
        event->accept();
        return;
    }
    case Qt::Key_Return:
    case Qt::Key_Enter:
        executeCurrentSelection();
        event->accept();
        return;
    case Qt::Key_Escape:
        reject();
        event->accept();
        return;
    default:
        QDialog::keyPressEvent(event);
        break;
    }
}

void CommandPaletteDialog::reject()
{
    if (m_scanCancelled) {
        m_scanCancelled->storeRelaxed(1);
    }
    QDialog::reject();
    // 取消路径安全恢复原焦点
    if (m_previousFocusWidget) {
        m_previousFocusWidget->setFocus();
    }
}
