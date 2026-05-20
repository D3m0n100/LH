/**
 * @file DslDragDropHandler.h
 * @brief DSL 拖拽处理器
 *
 * ============== 职责说明 ==============
 *
 * DslDragDropHandler 负责处理编辑器的拖拽操作，包括：
 * - 处理拖入事件（dragEnter）
 * - 处理拖动事件（dragMove）
 * - 处理放下事件（drop）
 * - 验证拖拽数据的合法性
 * - 提供拖入高亮效果
 * - 生成插入代码
 *
 * ============== 与 DslScriptEditor 的关系 ==============
 *
 * - DslDragDropHandler 由 DslScriptEditor 创建并管理
 * - 与 DslCompletionEngine 协作获取 snippet 信息
 * - 不直接持有编辑器控件，通过参数传入
 * - 通过信号向 DslScriptEditor 报告拖拽结果
 */

#ifndef DSLDRAGDROPHANDLER_H
#define DSLDRAGDROPHANDLER_H

#include <QObject>
#include <QString>
#include <QPoint>
#include <QColor>

// 前向声明
class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class QMimeData;
class QPlainTextEdit;
class DslCompletionEngine;

// 自定义 MIME 类型
extern const char* DSL_SNIPPET_MIME_TYPE;

/**
 * @brief 拖拽结果信息
 */
struct DragDropResult {
    bool success = false;       ///< 是否成功
    QString snippetId;          ///< 插入的片段 ID
    QString snippetName;        ///< 片段名称
    int lineNumber = -1;        ///< 插入行号
    QString insertedCode;       ///< 插入的代码
    QString errorMessage;       ///< 错误信息
};

/**
 * @brief DSL 拖拽处理器
 *
 * 封装编辑器侧的拖拽逻辑。
 */
class DslDragDropHandler : public QObject
{
    Q_OBJECT

public:
    explicit DslDragDropHandler(QObject* parent = nullptr);
    ~DslDragDropHandler() override;

    // ===== 关联设置 =====
    
    /// 设置补全引擎（用于获取 snippet 信息）
    void setCompletionEngine(DslCompletionEngine* engine) { m_completionEngine = engine; }
    
    /// 获取补全引擎
    DslCompletionEngine* completionEngine() const { return m_completionEngine; }

    // ===== 拖拽事件处理 =====
    
    /// 处理拖入事件
    bool handleDragEnter(QDragEnterEvent* event, QPlainTextEdit* editor);
    
    /// 处理拖动事件
    bool handleDragMove(QDragMoveEvent* event, QPlainTextEdit* editor);
    
    /// 处理拖离事件
    void handleDragLeave(QDragLeaveEvent* event, QPlainTextEdit* editor);
    
    /// 处理放下事件
    DragDropResult handleDrop(QDropEvent* event, QPlainTextEdit* editor);

    // ===== 拖拽数据解析 =====
    
    /// 验证 MIME 数据是否有效
    bool isValidMimeData(const QMimeData* mimeData) const;
    
    /// 从 MIME 数据中提取 snippet ID 和代码
    bool extractSnippetData(const QMimeData* mimeData, 
                            QString& snippetId, QString& snippetCode) const;

    // ===== 视觉反馈 =====
    
    /// 设置拖入高亮颜色
    void setDragHighlightColor(const QColor& color) { m_highlightColor = color; }
    
    /// 获取拖入高亮颜色
    QColor dragHighlightColor() const { return m_highlightColor; }
    
    /// 设置是否启用拖入高亮
    void setHighlightEnabled(bool enabled) { m_highlightEnabled = enabled; }
    
    /// 获取是否启用拖入高亮
    bool isHighlightEnabled() const { return m_highlightEnabled; }

    // ===== 状态查询 =====
    
    /// 是否正在拖拽
    bool isDragging() const { return m_isDragging; }
    
    /// 获取当前拖拽目标行号
    int dragTargetLine() const { return m_dragTargetLine; }

signals:
    /// 拖拽开始
    void dragStarted(const QString& snippetId);
    
    /// 拖拽目标行变化
    void dragTargetLineChanged(int lineNumber);
    
    /// 拖拽结束
    void dragEnded();
    
    /// 拖拽成功放下
    void dropSucceeded(const DragDropResult& result);
    
    /// 拖拽被拒绝
    void dropRejected(const QString& reason);
    
    /// 请求更新高亮（需要编辑器响应）
    void highlightUpdateRequested(int lineNumber, bool highlight);

private:
    /// 获取位置对应的行号
    int lineNumberAtPosition(QPlainTextEdit* editor, const QPoint& pos) const;
    
    /// 更新拖拽高亮
    void updateDragHighlight(QPlainTextEdit* editor, const QPoint& pos);
    
    /// 清除拖拽高亮
    void clearDragHighlight(QPlainTextEdit* editor);
    
    /// 获取当前行的缩进
    QString getCurrentIndentation(QPlainTextEdit* editor, int lineNumber) const;

private:
    /// 补全引擎
    DslCompletionEngine* m_completionEngine = nullptr;
    
    /// 拖拽状态
    bool m_isDragging = false;
    int m_dragTargetLine = -1;
    
    /// 高亮设置
    bool m_highlightEnabled = true;
    QColor m_highlightColor = QColor(180, 220, 255, 100);
};

#endif // DSLDRAGDROPHANDLER_H
