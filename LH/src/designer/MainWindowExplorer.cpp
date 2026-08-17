/**
 * @file MainWindowExplorer.cpp
 * @brief MainWindow project explorer file-opening helpers.
 */

#include "MainWindow.h"

#include "ProjectController.h"
#include "ProjectExplorerWidget.h"
#include "TextEncoding.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSet>

QString MainWindow::resolveExplorerRootPath() const
{
    if (m_projectController && m_projectController->hasOpenProject()) {
        return QDir(m_projectController->currentProjectPath()).absolutePath();
    }

    if (m_projectController) {
        const QStringList recentProjects = m_projectController->recentProjects();
        for (const QString& recentPath : recentProjects) {
            const QFileInfo dirInfo(recentPath);
            if (!dirInfo.exists() || !dirInfo.isDir()) {
                continue;
            }

            if (QFileInfo::exists(QDir(recentPath).filePath("project_config.json"))) {
                return dirInfo.absoluteFilePath();
            }
        }
    }

    return QString();
}

void MainWindow::refreshExplorerRoot()
{
    if (m_projectExplorerWidget) {
        m_projectExplorerWidget->setRootPath(resolveExplorerRootPath());
    }
}

bool MainWindow::isSupportedTextFile(const QString& filePath) const
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    static const QSet<QString> allowed = {
        "txt", "json", "xml", "yaml", "yml", "ini",
        "lh", "cpp", "c", "h", "hpp", "cc", "cxx",
        "ui", "qss", "pro", "pri", "cmake", "md", "log"
    };
    return allowed.contains(suffix) || QFileInfo(filePath).fileName() == "CMakeLists.txt";
}

bool MainWindow::loadTextFileToEditor(const QString& filePath)
{
    if (!m_dslEditor) {
        return false;
    }

    const QString currentPath = QFileInfo(m_dslEditor->currentFilePath()).canonicalFilePath();
    const QString targetPath = QFileInfo(filePath).canonicalFilePath();
    if (!currentPath.isEmpty() && currentPath != targetPath && m_dslEditor->isModified()) {
        const auto choice = QMessageBox::warning(this, QStringLiteral("未保存修改"),
                                                  QStringLiteral("当前 DSL 有未保存修改，是否保存？"),
                                                  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                                  QMessageBox::Save);
        if (choice == QMessageBox::Cancel) return false;
        if (choice == QMessageBox::Save && !m_projectController->saveProject()) return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "打开失败", QString("无法打开文件: %1").arg(filePath));
        return false;
    }

    const QString content = TextEncoding::decodeUtf8WithLocalFallback(file.readAll());
    file.close();

    m_dslEditor->setScript(content);
    m_dslEditor->setCurrentFilePath(filePath);
    m_dslEditor->editor()->setReadOnly(false);
    m_dslEditor->setModified(false);
    updateStatusBar(QString("已打开文件: %1").arg(QFileInfo(filePath).fileName()));
    refreshInspectorPanel();
    return true;
}

void MainWindow::openAuxiliaryTextFileInMdi(const QString& filePath)
{
    const QString canonicalPath = QFileInfo(filePath).canonicalFilePath();

    const auto subWindows = m_mdiArea->subWindowList();
    for (QMdiSubWindow* sub : subWindows) {
        if (!sub) {
            continue;
        }
        const QString existingPath = sub->property("filePath").toString();
        if (!existingPath.isEmpty() && QFileInfo(existingPath).canonicalFilePath() == canonicalPath) {
            sub->show();
            sub->raise();
            m_mdiArea->setActiveSubWindow(sub);
            return;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "打开失败", QString("无法打开文件: %1").arg(filePath));
        return;
    }

    const QString content = TextEncoding::decodeUtf8WithLocalFallback(file.readAll());
    file.close();

    auto* viewer = new QPlainTextEdit;
    viewer->setReadOnly(false);
    viewer->setLineWrapMode(QPlainTextEdit::NoWrap);
    viewer->setPlainText(content);
    viewer->setWindowTitle(QFileInfo(filePath).fileName());

    auto* sub = m_mdiArea->addSubWindow(viewer);
    sub->setAttribute(Qt::WA_DeleteOnClose, true);
    sub->setProperty("filePath", filePath);
    sub->setProperty("modified", false);
    sub->setWindowTitle(QFileInfo(filePath).fileName());
    sub->installEventFilter(this);
    connect(viewer, &QPlainTextEdit::textChanged, this, [sub = QPointer<QMdiSubWindow>(sub), filePath]() {
        if (!sub) {
            return;
        }
        if (sub->property("modified").toBool()) {
            return;
        }
        sub->setProperty("modified", true);
        sub->setWindowTitle(QFileInfo(filePath).fileName() + "*");
    });
    sub->show();
    m_mdiArea->setActiveSubWindow(sub);

    updateStatusBar(QString("已打开文件: %1").arg(QFileInfo(filePath).fileName()));
    refreshInspectorPanel();
}

void MainWindow::openFileFromExplorer(const QString& filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        return;
    }

    if (!isSupportedTextFile(filePath)) {
        updateStatusBar(QString("不支持直接打开该文件类型: %1").arg(info.fileName()));
        return;
    }

    if (m_projectController && !m_projectController->hasOpenProject()) {
        QDir dir = info.absoluteDir();
        QString projectRoot;
        while (dir.exists()) {
            if (QFileInfo::exists(dir.filePath("project_config.json"))) {
                projectRoot = dir.absolutePath();
                break;
            }
            if (!dir.cdUp()) {
                break;
            }
        }

        if (!projectRoot.isEmpty()) {
            if (!m_projectController->openProjectFromPath(projectRoot)) return;
        }
    }

    if (info.suffix().compare("lh", Qt::CaseInsensitive) == 0) {
        if (loadTextFileToEditor(filePath)) {
            if (m_projectController) {
                m_projectController->setCurrentScriptFile(filePath);
            }
            if (m_editorSubWindow) {
                m_editorSubWindow->show();
                m_editorSubWindow->raise();
                m_mdiArea->setActiveSubWindow(m_editorSubWindow);
            }
            if (m_projectExplorerWidget) {
                m_projectExplorerWidget->revealPath(filePath);
            }
        }
        return;
    }

    const QString canonicalTarget = info.canonicalFilePath();
    const QString currentDslFile = QFileInfo(m_projectController ? m_projectController->currentScriptFile() : QString()).canonicalFilePath();

    if (!currentDslFile.isEmpty() && canonicalTarget == currentDslFile) {
        if (loadTextFileToEditor(filePath)) {
            if (m_editorSubWindow) {
                m_editorSubWindow->show();
                m_editorSubWindow->raise();
                m_mdiArea->setActiveSubWindow(m_editorSubWindow);
            }
            if (m_projectExplorerWidget) {
                m_projectExplorerWidget->revealPath(filePath);
            }
        }
        return;
    }

    openAuxiliaryTextFileInMdi(filePath);
}

void MainWindow::onExplorerFileOpenRequested(const QString& filePath)
{
    openFileFromExplorer(filePath);
}

void MainWindow::onLocateCurrentFileInExplorer()
{
    if (!m_projectExplorerWidget) {
        return;
    }

    if (m_dslEditor && !m_dslEditor->currentFilePath().isEmpty()) {
        m_projectExplorerWidget->revealPath(m_dslEditor->currentFilePath());
        return;
    }

    if (QMdiSubWindow* sub = m_mdiArea->activeSubWindow()) {
        const QString filePath = sub->property("filePath").toString();
        if (!filePath.isEmpty()) {
            m_projectExplorerWidget->revealPath(filePath);
        }
    }
}
