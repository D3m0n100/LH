#include "ProjectExplorerWidget.h"

#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QLineEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QStyle>
#include <QRegularExpression>
#include <QAbstractItemView>
#include <QSizePolicy>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QInputDialog>
#include <QFile>
#include <QIcon>
#include <QSize>

class ExplorerFilterProxyModel final : public QSortFilterProxyModel
{
public:
    explicit ExplorerFilterProxyModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setFilterCaseSensitivity(Qt::CaseInsensitive);
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        if (filterRegularExpression().pattern().trimmed().isEmpty()) {
            return true;
        }

        if (!sourceModel()) {
            return false;
        }

        const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
        if (!index.isValid()) {
            return false;
        }

        const QString name = sourceModel()->data(index, Qt::DisplayRole).toString();
        if (name.contains(filterRegularExpression())) {
            return true;
        }

        const int childCount = sourceModel()->rowCount(index);
        for (int i = 0; i < childCount; ++i) {
            if (filterAcceptsRow(i, index)) {
                return true;
            }
        }

        return false;
    }
};

ProjectExplorerWidget::ProjectExplorerWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setupConnections();
    applyVsCodeLikeStyle();
}

void ProjectExplorerWidget::setupUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* topBar = new QWidget(this);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(8, 6, 8, 6);
    topLayout->setSpacing(6);

    auto* titleLabel = new QLabel(tr("EXPLORER"), topBar);
    titleLabel->setObjectName("ExplorerTitle");

    m_refreshButton = new QToolButton(topBar);
    m_refreshButton->setIcon(QIcon(":/icons/refresh.svg"));
    m_refreshButton->setIconSize(QSize(16, 16));
    m_refreshButton->setToolTip(tr("刷新"));

    m_collapseButton = new QToolButton(topBar);
    m_collapseButton->setIcon(QIcon(":/icons/collapse.svg"));
    m_collapseButton->setIconSize(QSize(16, 16));
    m_collapseButton->setToolTip(tr("折叠全部"));

    m_locateButton = new QToolButton(topBar);
    m_locateButton->setIcon(QIcon(":/icons/locate.svg"));
    m_locateButton->setIconSize(QSize(16, 16));
    m_locateButton->setToolTip(tr("定位当前文件"));

    topLayout->addWidget(titleLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_refreshButton);
    topLayout->addWidget(m_collapseButton);
    topLayout->addWidget(m_locateButton);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("搜索文件或目录"));
    m_searchEdit->setClearButtonEnabled(true);

    m_fileSystemModel = new QFileSystemModel(this);
    m_fileSystemModel->setReadOnly(true);
    m_fileSystemModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

    m_proxyModel = new ExplorerFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_fileSystemModel);
    m_proxyModel->setFilterKeyColumn(0);

    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_proxyModel);
    m_treeView->setHeaderHidden(true);
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(16);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setExpandsOnDoubleClick(false);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->hideColumn(1);
    m_treeView->hideColumn(2);
    m_treeView->hideColumn(3);

    m_blankView = new QWidget(this);
    m_blankView->setObjectName("ExplorerBlankView");
    m_blankView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* blankLayout = new QVBoxLayout(m_blankView);
    blankLayout->setContentsMargins(18, 24, 18, 24);
    blankLayout->setSpacing(8);

    auto* blankTitle = new QLabel(tr("未打开项目"), m_blankView);
    blankTitle->setObjectName("ExplorerBlankTitle");
    blankTitle->setAlignment(Qt::AlignCenter);

    auto* blankHint = new QLabel(tr("通过“文件”菜单新建或打开项目后，文件会显示在这里。"), m_blankView);
    blankHint->setObjectName("ExplorerBlankHint");
    blankHint->setWordWrap(true);
    blankHint->setAlignment(Qt::AlignCenter);

    blankLayout->addStretch();
    blankLayout->addWidget(blankTitle);
    blankLayout->addWidget(blankHint);
    blankLayout->addStretch();
    m_blankView->hide();

    rootLayout->addWidget(topBar);
    rootLayout->addWidget(m_searchEdit);
    rootLayout->addWidget(m_treeView, 1);
    rootLayout->addWidget(m_blankView, 1);
}

void ProjectExplorerWidget::setupConnections()
{
    connect(m_treeView, &QTreeView::doubleClicked,
            this, &ProjectExplorerWidget::onItemDoubleClicked);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &ProjectExplorerWidget::onFilterTextChanged);
    connect(m_refreshButton, &QToolButton::clicked,
            this, &ProjectExplorerWidget::onRefreshClicked);
    connect(m_collapseButton, &QToolButton::clicked,
            this, &ProjectExplorerWidget::onCollapseAllClicked);
    connect(m_locateButton, &QToolButton::clicked,
            this, &ProjectExplorerWidget::onLocateCurrentFileClicked);
    connect(m_treeView, &QWidget::customContextMenuRequested,
            this, &ProjectExplorerWidget::onContextMenuRequested);
}

void ProjectExplorerWidget::applyVsCodeLikeStyle()
{
    setStyleSheet(R"(
        ProjectExplorerWidget {
            background: #ffffff;
            color: #222222;
        }
        QLabel#ExplorerTitle {
            font-size: 11px;
            font-weight: 600;
            color: #3b3b3b;
        }
        QLineEdit {
            margin: 0 8px 6px 8px;
        }
        QToolButton {
            border: none;
            padding: 4px;
            background: transparent;
            border-radius: 3px;
        }
        QToolButton:hover {
            background: #e8f3ff;
        }
        QTreeView {
            border: none;
            background: #ffffff;
            outline: none;
            padding-left: 4px;
        }
        QTreeView::item {
            height: 22px;
            padding: 2px 4px;
        }
        QWidget#ExplorerBlankView {
            background: #ffffff;
        }
        QLabel#ExplorerBlankTitle {
            color: #3b3b3b;
            font-size: 14px;
            font-weight: 700;
        }
        QLabel#ExplorerBlankHint {
            color: #6a737d;
        }
    )");
}

void ProjectExplorerWidget::setRootPath(const QString& rootPath)
{
    const QString trimmed = rootPath.trimmed();
    if (trimmed.isEmpty()) {
        m_rootPath.clear();
        m_searchEdit->clear();
        m_fileSystemModel->setRootPath(QString());
        m_treeView->setRootIndex(QModelIndex());
        m_treeView->hide();
        m_searchEdit->hide();
        m_locateButton->setEnabled(false);
        if (m_blankView) {
            m_blankView->show();
        }
        return;
    }

    const QString cleanedRoot = QDir(trimmed).absolutePath();
    m_rootPath = cleanedRoot;
    const QModelIndex sourceRoot = m_fileSystemModel->setRootPath(m_rootPath);
    const QModelIndex proxyRoot = m_proxyModel->mapFromSource(sourceRoot);
    m_treeView->setRootIndex(proxyRoot);
    m_treeView->collapseAll();
    m_searchEdit->show();
    m_treeView->show();
    m_locateButton->setEnabled(true);
    if (m_blankView) {
        m_blankView->hide();
    }
}

void ProjectExplorerWidget::revealPath(const QString& filePath)
{
    if (filePath.isEmpty()) {
        return;
    }

    const QModelIndex sourceIndex = m_fileSystemModel->index(filePath);
    if (!sourceIndex.isValid()) {
        return;
    }

    QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
    if (!proxyIndex.isValid()) {
        return;
    }

    QModelIndex p = proxyIndex.parent();
    while (p.isValid()) {
        m_treeView->expand(p);
        p = p.parent();
    }

    m_treeView->setCurrentIndex(proxyIndex);
    m_treeView->scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
}

void ProjectExplorerWidget::onItemDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    const QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    if (!sourceIndex.isValid()) {
        return;
    }

    const QFileInfo info = m_fileSystemModel->fileInfo(sourceIndex);
    if (!info.exists()) {
        return;
    }

    if (info.isDir()) {
        const bool expanded = m_treeView->isExpanded(index);
        m_treeView->setExpanded(index, !expanded);
        return;
    }

    emit fileOpenRequested(info.absoluteFilePath());
}

void ProjectExplorerWidget::onFilterTextChanged(const QString& text)
{
    QRegularExpression expr(QRegularExpression::escape(text.trimmed()),
                            QRegularExpression::CaseInsensitiveOption);
    m_proxyModel->setFilterRegularExpression(expr);
    if (!text.trimmed().isEmpty()) {
        m_treeView->expandAll();
    }
}

void ProjectExplorerWidget::onRefreshClicked()
{
    if (m_rootPath.isEmpty()) {
        return;
    }

    m_fileSystemModel->setRootPath(QString());
    setRootPath(m_rootPath);
}

void ProjectExplorerWidget::onCollapseAllClicked()
{
    m_treeView->collapseAll();
}

void ProjectExplorerWidget::onLocateCurrentFileClicked()
{
    emit locateCurrentFileRequested();
}

void ProjectExplorerWidget::onContextMenuRequested(const QPoint& pos)
{
    if (m_rootPath.isEmpty()) {
        return;
    }

    const QModelIndex proxyIndex = m_treeView->indexAt(pos);
    const QModelIndex sourceIndex = proxyIndex.isValid()
        ? m_proxyModel->mapToSource(proxyIndex)
        : QModelIndex();
    const QFileInfo info = sourceIndex.isValid()
        ? m_fileSystemModel->fileInfo(sourceIndex)
        : QFileInfo();

    QString targetPath;
    QString parentDirectory = m_rootPath;
    if (info.exists()) {
        targetPath = info.absoluteFilePath();
        parentDirectory = info.isDir() ? targetPath : info.absolutePath();
    }

    QMenu menu(this);
    QAction* newFileAction = menu.addAction(tr("新建文件"));
    QAction* newFolderAction = menu.addAction(tr("新建文件夹"));
    QAction* deleteAction = nullptr;
    if (!targetPath.isEmpty()) {
        menu.addSeparator();
        deleteAction = menu.addAction(tr("删除"));
    }

    QAction* picked = menu.exec(m_treeView->viewport()->mapToGlobal(pos));
    if (!picked) {
        return;
    }

    if (picked == newFileAction) {
        createFileInDirectory(parentDirectory);
        return;
    }

    if (picked == newFolderAction) {
        createFolderInDirectory(parentDirectory);
        return;
    }

    if (deleteAction && picked == deleteAction) {
        deletePath(targetPath);
    }
}

void ProjectExplorerWidget::createFileInDirectory(const QString& directoryPath)
{
    bool accepted = false;
    const QString fileName = QInputDialog::getText(
        this,
        tr("新建文件"),
        tr("文件名:"),
        QLineEdit::Normal,
        QStringLiteral("main.lh"),
        &accepted).trimmed();
    if (!accepted || fileName.isEmpty()) {
        return;
    }

    if (fileName.contains('/') || fileName.contains('\\')) {
        QMessageBox::warning(this, tr("名称无效"),
                             tr("文件名不能包含 '/' 或 '\\'。"));
        return;
    }

    const QString absoluteFilePath = QDir(directoryPath).filePath(fileName);
    if (QFileInfo::exists(absoluteFilePath)) {
        QMessageBox::warning(this, tr("已存在"),
                             tr("同名文件或文件夹已经存在。"));
        return;
    }

    QFile file(absoluteFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("创建失败"),
                             tr("无法创建文件:\n%1").arg(absoluteFilePath));
        return;
    }
    if (QFileInfo(absoluteFilePath).suffix().compare(QStringLiteral("lh"), Qt::CaseInsensitive) == 0) {
        const QString text =
                QString::fromUtf8(u8"// LH脚本\n")
                + QString::fromUtf8(u8"// 创建时间: ")
                + QDateTime::currentDateTime().toString(Qt::ISODate)
                + QString::fromUtf8(u8"\n\n")
                + QString::fromUtf8(u8"PROGRAM Main\n")
                + QString::fromUtf8(u8"VAR\n")
                + QString::fromUtf8(u8"    system_1 : System;\n")
                + QString::fromUtf8(u8"    drv_ai_1 : DrvAI;\n")
                + QString::fromUtf8(u8"    add_1 : Add;\n")
                + QString::fromUtf8(u8"END_VAR\n\n")
                + QString::fromUtf8(u8"system_1(Author := 1, Config := 100, Date := 2601);\n")
                + QString::fromUtf8(u8"drv_ai_1(NumChannels := 1, InputNum := 0, DivisionNum := 4096);\n")
                + QString::fromUtf8(u8"add_1();\n\n")
                + QString::fromUtf8(u8"END_PROGRAM\n");
        file.write(text.toUtf8());
    }
    file.close();

    refreshAndReveal(absoluteFilePath);
}

void ProjectExplorerWidget::createFolderInDirectory(const QString& directoryPath)
{
    bool accepted = false;
    const QString folderName = QInputDialog::getText(
        this,
        tr("新建文件夹"),
        tr("文件夹名称:"),
        QLineEdit::Normal,
        QString(),
        &accepted).trimmed();
    if (!accepted || folderName.isEmpty()) {
        return;
    }

    if (folderName.contains('/') || folderName.contains('\\')) {
        QMessageBox::warning(this, tr("名称无效"),
                             tr("文件夹名称不能包含 '/' 或 '\\'。"));
        return;
    }

    const QString absoluteFolderPath = QDir(directoryPath).filePath(folderName);
    if (QFileInfo::exists(absoluteFolderPath)) {
        QMessageBox::warning(this, tr("已存在"),
                             tr("同名文件或文件夹已经存在。"));
        return;
    }

    QDir dir;
    if (!dir.mkpath(absoluteFolderPath)) {
        QMessageBox::warning(this, tr("创建失败"),
                             tr("无法创建文件夹:\n%1").arg(absoluteFolderPath));
        return;
    }

    refreshAndReveal(absoluteFolderPath);
}

void ProjectExplorerWidget::deletePath(const QString& targetPath)
{
    if (targetPath.isEmpty()) {
        return;
    }

    const QFileInfo info(targetPath);
    if (!info.exists()) {
        onRefreshClicked();
        return;
    }

    const QString targetName = info.fileName().isEmpty() ? targetPath : info.fileName();
    const QString message = info.isDir()
        ? tr("确定删除文件夹“%1”及其全部内容吗？").arg(targetName)
        : tr("确定删除文件“%1”吗？").arg(targetName);

    const QMessageBox::StandardButton button = QMessageBox::question(
        this,
        tr("确认删除"),
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (button != QMessageBox::Yes) {
        return;
    }

    bool ok = false;
    if (info.isDir()) {
        QDir dir(targetPath);
        ok = dir.removeRecursively();
    } else {
        ok = QFile::remove(targetPath);
    }

    if (!ok) {
        QMessageBox::warning(this, tr("删除失败"),
                             tr("无法删除:\n%1").arg(targetPath));
        return;
    }

    onRefreshClicked();
}

void ProjectExplorerWidget::refreshAndReveal(const QString& absolutePath)
{
    onRefreshClicked();
    if (!absolutePath.isEmpty()) {
        revealPath(absolutePath);
    }
}
