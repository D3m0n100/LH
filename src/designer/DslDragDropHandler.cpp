/**
 * @file DslDragDropHandler.cpp
 * @brief DSL 拖拽处理器实现
 */

#include "DslDragDropHandler.h"
#include "DslCompletionEngine.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextBlock>
#include <QDataStream>

// 自定义 MIME 类型定义
const char* DSL_SNIPPET_MIME_TYPE = "application/x-dsl-snippet";

// ================= 构造 / 析构 =================

DslDragDropHandler::DslDragDropHandler(QObject* parent)
    : QObject(parent)
{
}

DslDragDropHandler::~DslDragDropHandler()
{
}

// ================= 拖拽事件处理 =================

bool DslDragDropHandler::handleDragEnter(QDragEnterEvent* event, QPlainTextEdit* editor)
{
    if (!event || !editor) {
        return false;
    }
    
    const QMimeData* mimeData = event->mimeData();
    
    if (isValidMimeData(mimeData)) {
        event->acceptProposedAction();
        m_isDragging = true;
        
        // 尝试提取 snippet ID 发出信号
        QString snippetId, snippetCode;
        if (extractSnippetData(mimeData, snippetId, snippetCode)) {
            emit dragStarted(snippetId);
        }
        
        updateDragHighlight(editor, event->pos());
        return true;
    }
    
    event->ignore();
    return false;
}

bool DslDragDropHandler::handleDragMove(QDragMoveEvent* event, QPlainTextEdit* editor)
{
    if (!event || !editor) {
        return false;
    }
    
    const QMimeData* mimeData = event->mimeData();
    
    if (isValidMimeData(mimeData)) {
        event->acceptProposedAction();
        updateDragHighlight(editor, event->pos());
        return true;
    }
    
    event->ignore();
    return false;
}

void DslDragDropHandler::handleDragLeave(QDragLeaveEvent* event, QPlainTextEdit* editor)
{
    Q_UNUSED(event);
    clearDragHighlight(editor);
    emit dragEnded();
}

DragDropResult DslDragDropHandler::handleDrop(QDropEvent* event, QPlainTextEdit* editor)
{
    DragDropResult result;
    
    if (!event || !editor) {
        result.errorMessage = "无效的事件或编辑器";
        emit dropRejected(result.errorMessage);
        return result;
    }
    
    const QMimeData* mimeData = event->mimeData();
    
    if (!isValidMimeData(mimeData)) {
        result.errorMessage = "不支持的数据格式，请从函数列表拖拽组件";
        emit dropRejected(result.errorMessage);
        event->ignore();
        clearDragHighlight(editor);
        return result;
    }
    
    // 提取 snippet 数据
    QString snippetId, snippetCode;
    if (!extractSnippetData(mimeData, snippetId, snippetCode)) {
        result.errorMessage = "拖拽数据为空";
        emit dropRejected(result.errorMessage);
        event->ignore();
        clearDragHighlight(editor);
        return result;
    }
    
    // 获取插入位置
    int lineNumber = lineNumberAtPosition(editor, event->pos());
    
    // 获取当前行缩进
    QString indentation = getCurrentIndentation(editor, lineNumber);
    
    // 如果有补全引擎，尝试获取更详细的 snippet 信息
    QString snippetName = snippetId;
    if (m_completionEngine) {
        FunctionSnippet snippet = m_completionEngine->snippetById(snippetId);
        if (snippet.isValid()) {
            snippetName = snippet.name;
            // 使用补全引擎生成带缩进的代码
            snippetCode = m_completionEngine->generateInsertCode(snippet, indentation);
        }
    }
    
    // 移动光标到目标行并插入代码
    QTextDocument* doc = editor->document();
    QTextBlock block = doc->findBlockByNumber(lineNumber);
    
    if (block.isValid()) {
        QTextCursor cursor(block);
        cursor.movePosition(QTextCursor::EndOfBlock);
        
        // 插入换行和代码
        cursor.insertText("\n" + snippetCode);
        
        editor->setTextCursor(cursor);
        
        // 填充结果
        result.success = true;
        result.snippetId = snippetId;
        result.snippetName = snippetName;
        result.lineNumber = lineNumber + 1;  // 行号从 1 开始
        result.insertedCode = snippetCode;
        
        emit dropSucceeded(result);
    } else {
        result.errorMessage = "无法定位插入位置";
        emit dropRejected(result.errorMessage);
    }
    
    event->acceptProposedAction();
    clearDragHighlight(editor);
    
    return result;
}

// ================= 拖拽数据解析 =================

bool DslDragDropHandler::isValidMimeData(const QMimeData* mimeData) const
{
    if (!mimeData) {
        return false;
    }
    
    // 检查自定义 MIME 类型或纯文本
    return mimeData->hasFormat(DSL_SNIPPET_MIME_TYPE) || mimeData->hasText();
}

bool DslDragDropHandler::extractSnippetData(const QMimeData* mimeData,
                                             QString& snippetId, 
                                             QString& snippetCode) const
{
    if (!mimeData) {
        return false;
    }
    
    // 优先使用自定义 MIME 类型
    if (mimeData->hasFormat(DSL_SNIPPET_MIME_TYPE)) {
        QByteArray data = mimeData->data(DSL_SNIPPET_MIME_TYPE);
        QDataStream stream(&data, QIODevice::ReadOnly);
        stream >> snippetId >> snippetCode;
        return !snippetCode.isEmpty();
    }
    
    // 回退到纯文本
    if (mimeData->hasText()) {
        snippetCode = mimeData->text();
        snippetId = "text_snippet";  // 没有 ID 时使用默认值
        return !snippetCode.isEmpty();
    }
    
    return false;
}

// ================= 视觉反馈 =================

int DslDragDropHandler::lineNumberAtPosition(QPlainTextEdit* editor, const QPoint& pos) const
{
    if (!editor) {
        return 0;
    }
    
    QTextCursor cursor = editor->cursorForPosition(pos);
    return cursor.blockNumber();
}

void DslDragDropHandler::updateDragHighlight(QPlainTextEdit* editor, const QPoint& pos)
{
    if (!editor || !m_highlightEnabled) {
        return;
    }
    
    int newTargetLine = lineNumberAtPosition(editor, pos);
    
    if (newTargetLine != m_dragTargetLine) {
        // 清除旧高亮
        if (m_dragTargetLine >= 0) {
            emit highlightUpdateRequested(m_dragTargetLine, false);
        }
        
        m_dragTargetLine = newTargetLine;
        
        // 设置新高亮
        emit highlightUpdateRequested(m_dragTargetLine, true);
        emit dragTargetLineChanged(m_dragTargetLine);
    }
}

void DslDragDropHandler::clearDragHighlight(QPlainTextEdit* editor)
{
    Q_UNUSED(editor);
    
    if (m_dragTargetLine >= 0) {
        emit highlightUpdateRequested(m_dragTargetLine, false);
    }
    
    m_isDragging = false;
    m_dragTargetLine = -1;
    emit dragEnded();
}

QString DslDragDropHandler::getCurrentIndentation(QPlainTextEdit* editor, int lineNumber) const
{
    if (!editor) {
        return QString();
    }
    
    QTextDocument* doc = editor->document();
    QTextBlock block = doc->findBlockByNumber(lineNumber);
    
    if (!block.isValid()) {
        return QString();
    }
    
    QString blockText = block.text();
    QString indentation;
    
    for (const QChar& ch : blockText) {
        if (ch.isSpace()) {
            indentation += ch;
        } else {
            break;
        }
    }
    
    return indentation;
}
