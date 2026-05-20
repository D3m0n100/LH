#ifndef PROJECTEXPLORERWIDGET_H
#define PROJECTEXPLORERWIDGET_H

#include <QWidget>
#include <QString>
#include <QModelIndex>
#include <QPoint>

class QLineEdit;
class QTreeView;
class QFileSystemModel;
class QSortFilterProxyModel;
class QToolButton;

class ProjectExplorerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectExplorerWidget(QWidget* parent = nullptr);

    void setRootPath(const QString& rootPath);
    QString rootPath() const { return m_rootPath; }
    void revealPath(const QString& filePath);

signals:
    void fileOpenRequested(const QString& filePath);
    void locateCurrentFileRequested();

private slots:
    void onItemDoubleClicked(const QModelIndex& index);
    void onFilterTextChanged(const QString& text);
    void onRefreshClicked();
    void onCollapseAllClicked();
    void onLocateCurrentFileClicked();
    void onContextMenuRequested(const QPoint& pos);

private:
    void setupUi();
    void setupConnections();
    void applyVsCodeLikeStyle();
    void createFileInDirectory(const QString& directoryPath);
    void createFolderInDirectory(const QString& directoryPath);
    void deletePath(const QString& targetPath);
    void refreshAndReveal(const QString& absolutePath);

private:
    QString m_rootPath;
    QLineEdit* m_searchEdit = nullptr;
    QTreeView* m_treeView = nullptr;
    QWidget* m_blankView = nullptr;
    QFileSystemModel* m_fileSystemModel = nullptr;
    QSortFilterProxyModel* m_proxyModel = nullptr;
    QToolButton* m_refreshButton = nullptr;
    QToolButton* m_collapseButton = nullptr;
    QToolButton* m_locateButton = nullptr;
};

#endif // PROJECTEXPLORERWIDGET_H
