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
#include <QTextStream>
#include <QDateTime>
#include <QMessageBox>
#include <QSaveFile>

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
    m_textEdit->document()->setMaximumBlockCount(OutputPaneConfig::DEFAULT_MAX_BLOCK_COUNT);

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

    const bool atBottom = OutputPaneConfig::isScrolledToBottom(m_textEdit);
    const QString sanitized = OutputPaneConfig::sanitizeAndTruncate(message);

    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (!m_textEdit->document()->isEmpty()) {
        cursor.insertBlock();
    }
    cursor.insertText(sanitized);

    if (m_autoScroll && atBottom) {
        scrollToBottom();
    }
}

void OutputPaneController::appendTimestampedMessage(const QString& message)
{
    appendMessage(QString("[%1] %2").arg(currentTimestamp()).arg(message));
}

void OutputPaneController::appendFormatted(const QString& tag, const QString& message, const QColor& color, bool bold)
{
    if (!m_textEdit) {
        return;
    }

    const bool atBottom = OutputPaneConfig::isScrolledToBottom(m_textEdit);
    const QString sanitized = OutputPaneConfig::sanitizeAndTruncate(message);

    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (!m_textEdit->document()->isEmpty()) {
        cursor.insertBlock();
    }

    QTextCharFormat fmt;
    fmt.setForeground(color);
    if (bold) {
        fmt.setFontWeight(QFont::Bold);
    }
    cursor.setCharFormat(fmt);
    cursor.insertText(QString("[%1] [%2] %3").arg(currentTimestamp(), tag, sanitized));

    if (m_autoScroll && atBottom) {
        scrollToBottom();
    }
}

void OutputPaneController::appendInfo(const QString& message)
{
    appendFormatted(QStringLiteral("INFO"), message, QColor(0x00, 0x66, 0xcc), false);
}

void OutputPaneController::appendWarning(const QString& message)
{
    appendFormatted(QStringLiteral("WARN"), message, QColor(0xcc, 0x66, 0x00), false);
}

void OutputPaneController::appendError(const QString& message)
{
    appendFormatted(QStringLiteral("ERROR"), message, QColor(0xcc, 0x00, 0x00), true);
}

void OutputPaneController::appendSuccess(const QString& message)
{
    appendFormatted(QStringLiteral("OK"), message, QColor(0x00, 0x88, 0x00), false);
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

    QSaveFile file(fileName);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(nullptr, "保存失败",
                             QString("无法打开文件: %1\n%2")
                                 .arg(fileName, file.errorString()));
        return;
    }

    QTextStream stream(&file);
    TextEncoding::setUtf8(stream);
    stream << allText();
    stream.flush();
    if (stream.status() != QTextStream::Ok) {
        const QString error = file.errorString();
        file.cancelWriting();
        QMessageBox::warning(nullptr, "保存失败",
                             QString("写入文件失败: %1\n%2")
                                 .arg(fileName, error));
        return;
    }

    if (!file.commit()) {
        const QString error = file.errorString();
        file.cancelWriting();
        QMessageBox::warning(nullptr, "保存失败",
                             QString("提交文件失败: %1\n%2")
                                 .arg(fileName, error));
        return;
    }

    appendInfo(QString("输出已保存到: %1").arg(fileName));
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
