/**
 * @file OutputPaneController.cpp
 * @brief 输出窗口控制器实现
 */

#include "OutputPaneController.h"
#include "TextEncoding.h"

#include <QDockWidget>
#include <QTextEdit>
#include <QScrollBar>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMessageBox>

OutputPaneController::OutputPaneController(QDockWidget* dockWidget, QObject* parent)
    : QObject(parent)
    , m_dockWidget(dockWidget)
    , m_textEdit(nullptr)
    , m_autoScroll(true)
{
    setupUI();
}

OutputPaneController::~OutputPaneController()
{
    // m_textEdit 由 m_dockWidget 管理，无需手动删除
}

void OutputPaneController::setupUI()
{
    if (!m_dockWidget) {
        return;
    }

    m_textEdit = new QTextEdit(m_dockWidget);
    m_textEdit->setReadOnly(true);
    m_textEdit->setLineWrapMode(QTextEdit::NoWrap);
    m_textEdit->setFont(QFont("Consolas", 10));

    // 设置右键菜单
    m_textEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_textEdit, &QTextEdit::customContextMenuRequested,
            this, &OutputPaneController::showContextMenu);

    m_dockWidget->setWidget(m_textEdit);

    // 连接可见性变化信号
    connect(m_dockWidget, &QDockWidget::visibilityChanged,
            this, &OutputPaneController::visibilityChanged);
}

// ================= 日志输出 =================

void OutputPaneController::appendMessage(const QString& message)
{
    if (!m_textEdit) {
        return;
    }

    m_textEdit->append(message);

    if (m_autoScroll) {
        scrollToBottom();
    }
}

void OutputPaneController::appendTimestampedMessage(const QString& message)
{
    appendMessage(QString("[%1] %2").arg(currentTimestamp()).arg(message));
}

void OutputPaneController::appendInfo(const QString& message)
{
    if (!m_textEdit) {
        return;
    }

    QString html = QString("<span style=\"color: #0066cc;\">[%1] [INFO] %2</span>")
                       .arg(currentTimestamp())
                       .arg(message.toHtmlEscaped());
    m_textEdit->append(html);

    if (m_autoScroll) {
        scrollToBottom();
    }
}

void OutputPaneController::appendWarning(const QString& message)
{
    if (!m_textEdit) {
        return;
    }

    QString html = QString("<span style=\"color: #cc6600;\">[%1] [WARN] %2</span>")
                       .arg(currentTimestamp())
                       .arg(message.toHtmlEscaped());
    m_textEdit->append(html);

    if (m_autoScroll) {
        scrollToBottom();
    }
}

void OutputPaneController::appendError(const QString& message)
{
    if (!m_textEdit) {
        return;
    }

    QString html = QString("<span style=\"color: #cc0000; font-weight: bold;\">[%1] [ERROR] %2</span>")
                       .arg(currentTimestamp())
                       .arg(message.toHtmlEscaped());
    m_textEdit->append(html);

    if (m_autoScroll) {
        scrollToBottom();
    }
}

void OutputPaneController::appendSuccess(const QString& message)
{
    if (!m_textEdit) {
        return;
    }

    QString html = QString("<span style=\"color: #008800;\">[%1] [OK] %2</span>")
                       .arg(currentTimestamp())
                       .arg(message.toHtmlEscaped());
    m_textEdit->append(html);

    if (m_autoScroll) {
        scrollToBottom();
    }
}

// ================= 控制 =================

void OutputPaneController::clear()
{
    if (m_textEdit) {
        m_textEdit->clear();
    }
}

void OutputPaneController::setAutoScroll(bool enabled)
{
    m_autoScroll = enabled;
}

void OutputPaneController::setFontSize(int pointSize)
{
    if (m_textEdit) {
        QFont font = m_textEdit->font();
        font.setPointSize(pointSize);
        m_textEdit->setFont(font);
    }
}

QString OutputPaneController::allText() const
{
    if (m_textEdit) {
        return m_textEdit->toPlainText();
    }
    return QString();
}

QString OutputPaneController::selectedText() const
{
    if (m_textEdit) {
        return m_textEdit->textCursor().selectedText();
    }
    return QString();
}

// ================= 槽函数 =================

void OutputPaneController::copySelected()
{
    if (m_textEdit) {
        m_textEdit->copy();
    }
}

void OutputPaneController::copyAll()
{
    QApplication::clipboard()->setText(allText());
}

void OutputPaneController::saveToFile()
{
    QString fileName = QFileDialog::getSaveFileName(
        nullptr,
        "保存输出日志",
        QString("output_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "文本文件 (*.txt);;所有文件 (*.*)");

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        TextEncoding::setUtf8(stream);
        stream << allText();
        file.close();

        appendInfo(QString("输出已保存到: %1").arg(fileName));
    } else {
        QMessageBox::warning(nullptr, "保存失败",
                             QString("无法保存文件: %1").arg(file.errorString()));
    }
}

void OutputPaneController::showContextMenu(const QPoint& pos)
{
    QMenu menu;

    QAction* actCopy = menu.addAction("复制选中");
    actCopy->setShortcut(QKeySequence::Copy);
    connect(actCopy, &QAction::triggered, this, &OutputPaneController::copySelected);

    QAction* actCopyAll = menu.addAction("复制全部");
    connect(actCopyAll, &QAction::triggered, this, &OutputPaneController::copyAll);

    menu.addSeparator();

    QAction* actSave = menu.addAction("保存到文件...");
    connect(actSave, &QAction::triggered, this, &OutputPaneController::saveToFile);

    menu.addSeparator();

    QAction* actClear = menu.addAction("清空");
    connect(actClear, &QAction::triggered, this, &OutputPaneController::clear);

    menu.addSeparator();

    QAction* actAutoScroll = menu.addAction("自动滚动");
    actAutoScroll->setCheckable(true);
    actAutoScroll->setChecked(m_autoScroll);
    connect(actAutoScroll, &QAction::toggled, this, &OutputPaneController::setAutoScroll);

    menu.exec(m_textEdit->mapToGlobal(pos));
}

// ================= 辅助函数 =================

void OutputPaneController::scrollToBottom()
{
    if (m_textEdit) {
        QScrollBar* scrollBar = m_textEdit->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}

QString OutputPaneController::currentTimestamp() const
{
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}
