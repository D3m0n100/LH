/**
 * @file MainWindowExplorer.cpp
 * @brief MainWindow project explorer file-opening helpers.
 */

#include "MainWindow.h"

#include "ProjectController.h"
#include "ProjectExplorerWidget.h"
#include "TextEncoding.h"
#include "DslScriptEditor.h"
#include "RuntimeSessionController.h"
#include "ui/ProblemsPanel.h"

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
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

void MainWindow::deleteProjectDocumentPath(const QString& path)
{
    if (!m_projectController || !m_projectController->hasOpenProject()) return;
    if ((m_buildController && m_buildController->isBusy())
            || (m_sessionController && (m_sessionController->isRunning()
                || m_sessionController->state() == RuntimeSessionState::Downloading
                || m_sessionController->state() == RuntimeSessionState::Connecting))) {
        showMessageBox(QStringLiteral("无法删除"), QStringLiteral("请等待当前构建或运行操作结束后再删除文件"));
        return;
    }
    const QString target = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const bool directory = QFileInfo(target).isDir();
    const auto affected = [&](const QString& document) {
        if (document.isEmpty()) return false;
        const QString full = QDir::cleanPath(QFileInfo(document).absoluteFilePath());
        return full.compare(target, Qt::CaseInsensitive) == 0
                || (directory && full.startsWith(target + QLatin1Char('/'), Qt::CaseInsensitive));
    };
    const bool mainAffected = m_dslEditor && affected(m_dslEditor->currentFilePath());
    QList<QPointer<QMdiSubWindow>> documents;
    if (m_mdiArea) for (auto* sub : m_mdiArea->subWindowList())
        if (sub != m_editorSubWindow && affected(sub->property("filePath").toString())) documents.append(sub);
    if (mainAffected && m_dslEditor->isModified()) {
        const auto choice = showMessageBox(QStringLiteral("删除前处理修改"),
                QStringLiteral("即将删除的脚本有未保存修改，是否先保存？"),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
        if (choice == QMessageBox::Cancel || (choice == QMessageBox::Save && !m_projectController->saveProject())) return;
    }
    for (const auto& sub : documents) {
        if (!sub) continue;
        auto* editor = qobject_cast<QPlainTextEdit*>(sub->widget());
        if (!sub->property("modified").toBool() && (!editor || !editor->document()->isModified())) continue;
        const auto choice = showMessageBox(QStringLiteral("删除前处理修改"),
                QStringLiteral("文件 %1 有未保存修改，是否先保存？").arg(sub->property("filePath").toString()),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
        if (choice == QMessageBox::Cancel || (choice == QMessageBox::Save && !saveAuxiliarySubWindow(sub))) return;
    }
    QString error;
    if (!m_projectController->removeProjectPath(target, &error)) {
        showMessageBox(QStringLiteral("删除失败"), error); return;
    }
    if (m_sessionController) m_sessionController->invalidateCompiledArtifact();
    if (mainAffected) {
        m_dslEditor->setCurrentFilePath(QString());
        m_dslEditor->setScript(QString());
        m_dslEditor->setModified(false);
        m_dslEditor->editor()->setReadOnly(true);
    }
    for (const auto& sub : documents) if (sub) {
        sub->setProperty("modified", false); sub->setProperty("filePath", QString());
        if (auto* editor = qobject_cast<QPlainTextEdit*>(sub->widget())) editor->document()->setModified(false);
        sub->close();
    }
    for (const QString& key : m_documentVersions.keys()) if (affected(key)) m_documentVersions.remove(key);
    if (m_problemsPanel) {
        QList<DiagnosticItem> remaining;
        for (int row = 0; row < m_problemsPanel->problemCount(); ++row) {
            const auto item = m_problemsPanel->itemAtRow(row);
            if (!affected(item.filePath)) remaining.append(item);
        }
        m_problemsPanel->setDiagnostics(remaining);
    }
    refreshInspectorPanel();
    updateStatusBar(QStringLiteral("已删除：%1").arg(QFileInfo(target).fileName()));
}

QString MainWindow::normalizeDocumentIdentity(const QString& path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    const QFileInfo fi(trimmed);
    if (fi.exists()) {
        return fi.canonicalFilePath();
    }
    return QDir::cleanPath(fi.absoluteFilePath());
}

bool MainWindow::isSameDocument(const QString& pathA, const QString& pathB)
{
    const QString idA = normalizeDocumentIdentity(pathA);
    const QString idB = normalizeDocumentIdentity(pathB);
    if (idA.isEmpty() || idB.isEmpty()) {
        return false;
    }
    return idA.compare(idB, Qt::CaseInsensitive) == 0;
}

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

QWidget* MainWindow::openAndActivateFile(const QString& filePath)
{
    const QString targetId = normalizeDocumentIdentity(filePath);
    if (targetId.isEmpty()) {
        return nullptr;
    }

    if (!isSupportedTextFile(filePath)) {
        updateStatusBar(QStringLiteral("不支持直接打开该文件类型: %1").arg(QFileInfo(filePath).fileName()));
        return nullptr;
    }

    // 1. 先查复用：若已打开，直接激活并返回（不读盘、不弹保存确认、完整保留未保存缓冲区）
    if (m_mdiArea) {
        const auto subWindows = m_mdiArea->subWindowList();
        for (QMdiSubWindow* sub : subWindows) {
            if (!sub) {
                continue;
            }
            const QString existingPath = sub->property("filePath").toString();
            if (isSameDocument(existingPath, targetId)) {
                sub->show();
                sub->raise();
                m_mdiArea->setActiveSubWindow(sub);
                if (QWidget* w = sub->widget()) {
                    w->setFocus();
                    return w;
                }
                return sub;
            }
        }
    }

    if (m_dslEditor && isSameDocument(m_dslEditor->currentFilePath(), targetId)) {
        if (m_editorSubWindow) {
            m_dslEditor->show();
            m_editorSubWindow->show();
            m_editorSubWindow->raise();
            m_mdiArea->setActiveSubWindow(m_editorSubWindow);
            m_dslEditor->setFocus();
        }
        return m_dslEditor;
    }

    // 2. 读目标文件至临时缓冲区（若读取失败则直接中止，原文档不被切走）
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        updateStatusBar(QStringLiteral("文件不存在: %1").arg(info.fileName()));
        return nullptr;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        showMessageBox(QStringLiteral("打开失败"),
                       QStringLiteral("无法打开文件: %1").arg(filePath));
        return nullptr;
    }
    const QString content = TextEncoding::decodeUtf8WithLocalFallback(file.readAll());
    file.close();

    // 3. 仅在需要替换当前主 DSL 文档且处于未保存状态时，才确认保存
    if (info.suffix().compare("lh", Qt::CaseInsensitive) == 0) {
        if (m_dslEditor && m_dslEditor->isModified()) {
            const auto choice = showMessageBox(QStringLiteral("未保存修改"),
                                               QStringLiteral("当前 DSL 脚本有未保存修改，是否保存？"),
                                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                               QMessageBox::Save);
            if (choice == QMessageBox::Cancel) {
                return nullptr;
            }
            if (choice == QMessageBox::Save) {
                if (!m_projectController || !m_projectController->saveProject()) {
                    return nullptr; // 保存失败中止切换
                }
            }
        }

        m_dslEditor->setCurrentFilePath(filePath);

        m_dslEditor->setScript(content);
        m_dslEditor->editor()->setReadOnly(false);
        m_dslEditor->setModified(false);

        if (m_projectController) {
            m_projectController->setCurrentScriptFile(filePath);
        }
        if (m_editorSubWindow) {
            m_dslEditor->show();
            m_editorSubWindow->show();
            m_editorSubWindow->raise();
            m_mdiArea->setActiveSubWindow(m_editorSubWindow);
            m_dslEditor->setFocus();
        }
        updateStatusBar(QStringLiteral("已打开文件: %1").arg(info.fileName()));
        refreshInspectorPanel();
        if (m_projectExplorerWidget) {
            m_projectExplorerWidget->revealPath(filePath);
        }
        return m_dslEditor;
    }

    // 4. 打开辅助文本文件
    auto* viewer = new QPlainTextEdit;
    viewer->setReadOnly(false);
    viewer->setLineWrapMode(QPlainTextEdit::NoWrap);
    viewer->setPlainText(content);
    viewer->setWindowTitle(info.fileName());

    auto* sub = m_mdiArea->addSubWindow(viewer);
    sub->setAttribute(Qt::WA_DeleteOnClose, true);
    sub->setProperty("filePath", targetId);
    sub->setProperty("modified", false);
    sub->setWindowTitle(info.fileName());
    sub->installEventFilter(this);

    connect(viewer, &QPlainTextEdit::textChanged, this, [this, sub = QPointer<QMdiSubWindow>(sub), targetId]() {
        if (!sub) {
            return;
        }
        if (!sub->property("modified").toBool()) {
            sub->setProperty("modified", true);
            sub->setWindowTitle(QFileInfo(targetId).fileName() + "*");
        }
        onDocumentModified(targetId);
    });

    sub->show();
    m_mdiArea->setActiveSubWindow(sub);
    viewer->setFocus();

    updateStatusBar(QStringLiteral("已打开文件: %1").arg(info.fileName()));
    refreshInspectorPanel();
    if (m_projectExplorerWidget) {
        m_projectExplorerWidget->revealPath(filePath);
    }
    return viewer;
}

void MainWindow::navigateEditorPosition(QWidget* editorWidget, int targetLine, int targetColumn, bool hasExactColumn)
{
    if (!editorWidget || targetLine <= 0) {
        return;
    }

    QPlainTextEdit* plainEdit = nullptr;
    if (auto* dsl = qobject_cast<DslScriptEditor*>(editorWidget)) {
        plainEdit = dsl->editor();
    } else {
        plainEdit = qobject_cast<QPlainTextEdit*>(editorWidget);
    }

    if (!plainEdit) {
        return;
    }

    QTextDocument* doc = plainEdit->document();
    if (!doc) {
        return;
    }

    const int totalBlocks = doc->blockCount();
    const int clampedLine = qBound(1, targetLine, totalBlocks);
    QTextBlock block = doc->findBlockByNumber(clampedLine - 1);
    if (!block.isValid()) {
        return;
    }

    int utf16Offset = 0;
    if (targetColumn > 1 && hasExactColumn) {
        const QString lineText = block.text();
        int charCount = 0;
        int i = 0;
        const int targetCharIndex = targetColumn - 1;
        while (i < lineText.length() && charCount < targetCharIndex) {
            if (lineText.at(i).isHighSurrogate() && (i + 1 < lineText.length()) && lineText.at(i + 1).isLowSurrogate()) {
                i += 2;
            } else {
                i += 1;
            }
            charCount++;
        }
        utf16Offset = qMin(i, lineText.length());
    }

    QTextCursor cursor(block);
    cursor.setPosition(block.position() + utf16Offset);
    plainEdit->setTextCursor(cursor);
    plainEdit->ensureCursorVisible();
    plainEdit->setFocus();
}

bool MainWindow::loadTextFileToEditor(const QString& filePath)
{
    QWidget* w = openAndActivateFile(filePath);
    return (w != nullptr);
}

void MainWindow::openAuxiliaryTextFileInMdi(const QString& filePath)
{
    openAndActivateFile(filePath);
}

void MainWindow::openFileFromExplorer(const QString& filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
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

    openAndActivateFile(filePath);
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
