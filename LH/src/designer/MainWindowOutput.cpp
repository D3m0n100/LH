/**
 * @file MainWindowOutput.cpp
 * @brief MainWindow output pane helpers.
 */

#include "MainWindow.h"

#include "SettingsController.h"
#include "TextEncoding.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDockWidget>
#include <QFileDialog>
#include <QIODevice>
#include <QMenu>
#include <QPoint>
#include <QSaveFile>
#include <QScrollBar>
#include <QTextEdit>
#include <QTextStream>

void MainWindow::appendOutput(const QString& message)
{
    if (m_outputViewer) {
        m_outputViewer->append(message);
        
        if (m_settingsController->autoScrollLog()) {
            QScrollBar* scrollBar = m_outputViewer->verticalScrollBar();
            scrollBar->setValue(scrollBar->maximum());
        }
    }
}

void MainWindow::onToggleOutputDock(bool checked)
{
    if (m_logDock) {
        m_logDock->setVisible(checked);
    }
}

void MainWindow::onClearOutput()
{
    if (m_outputViewer) {
        m_outputViewer->clear();
    }
}

// ================= 输出窗口右键菜单槽函数 =================

void MainWindow::onOutputContextMenu(const QPoint& pos)
{
    QMenu menu(this);
    
    QAction* actCopy = menu.addAction("复制选中");
    connect(actCopy, &QAction::triggered, this, &MainWindow::onCopySelectedOutput);
    
    QAction* actCopyAll = menu.addAction("复制全部");
    connect(actCopyAll, &QAction::triggered, this, &MainWindow::onCopyAllOutput);
    
    menu.addSeparator();
    
    QAction* actSave = menu.addAction("保存到文件...");
    connect(actSave, &QAction::triggered, this, &MainWindow::onSaveOutputToFile);
    
    menu.addSeparator();
    
    QAction* actClear = menu.addAction("清空");
    connect(actClear, &QAction::triggered, this, &MainWindow::onClearOutput);
    
    menu.exec(m_outputViewer->mapToGlobal(pos));
}

void MainWindow::onCopySelectedOutput()
{
    if (m_outputViewer) {
        m_outputViewer->copy();
    }
}

void MainWindow::onCopyAllOutput()
{
    if (m_outputViewer) {
        QApplication::clipboard()->setText(m_outputViewer->toPlainText());
    }
}

void MainWindow::onSaveOutputToFile()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "保存输出", QString(), "文本文件 (*.txt);;所有文件 (*.*)");
    
    if (fileName.isEmpty()) {
        return;
    }
    
    QSaveFile file(fileName);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendOutput(QString("[%1] 输出保存失败（打开）: %2\n%3")
                     .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                     .arg(fileName)
                     .arg(file.errorString()));
        return;
    }

    QTextStream stream(&file);
    TextEncoding::setUtf8(stream);
    stream << m_outputViewer->toPlainText();
    stream.flush();
    if (stream.status() != QTextStream::Ok) {
        const QString error = file.errorString();
        file.cancelWriting();
        appendOutput(QString("[%1] 输出保存失败（写入）: %2\n%3")
                     .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                     .arg(fileName)
                     .arg(error));
        return;
    }

    if (!file.commit()) {
        const QString error = file.errorString();
        file.cancelWriting();
        appendOutput(QString("[%1] 输出保存失败（提交）: %2\n%3")
                     .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                     .arg(fileName)
                     .arg(error));
        return;
    }

    appendOutput(QString("[%1] 输出已保存到: %2")
                 .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                 .arg(fileName));
}
