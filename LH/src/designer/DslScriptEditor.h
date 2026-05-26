/**
 * @file DslScriptEditor.h
 * @brief DSL 脚本编辑器
 *
 * ============== 架构设计说明 ==============
 *
 * DslScriptEditor 是 DSL 编辑器的总控类，负责 UI 组合和协调，
 * 具体功能由以下模块实现：
 *
 * - **DslSyntaxHighlighter**: 语法高亮
 * - **DslCompletionEngine**: 代码补全和函数片段管理
 * - **DslDragDropHandler**: 拖拽处理
 *
 * ============== 功能列表 ==============
 *
 * - 行号显示（LineNumberArea + CodeEditor）
 * - 查找/替换工具条
 * - 当前行高亮
 * - 括号匹配高亮
 * - 跳转到指定行
 * - 光标位置变化信号（行/列/总行数）
 * - 函数列表显示/隐藏控制
 * - 编辑器修改状态信号
 * - 语法高亮
 * - 代码自动补全
 * - 拖拽式组态
 */

#ifndef DSLSCRIPTEDITOR_H
#define DSLSCRIPTEDITOR_H

#include <QWidget>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QShortcut>
#include <QSplitter>
#include <QMap>
#include <QDialog>
#include <QLabel>
#include <functional>

#include "common/ConfigTypes.h"

// 包含模块头文件
#include "DslCompletionEngine.h"  // 包含 FunctionSnippet 和 DslInsertRecord 定义
#include "DslDragDropHandler.h"   // 包含 DragDropResult 定义

// 前向声明
class LineNumberArea;
class CodeEditor;
class DslSyntaxHighlighter;
class QCompleter;

/**
 * @brief FunctionListWidget
 * 左侧函数列表控件，支持拖拽，将 DSL 模板代码拖入脚本编辑器。
 * 增强：支持 Tooltip 和拖拽开始视觉反馈。
 */
class FunctionListWidget : public QListWidget
{
    Q_OBJECT
public:
    explicit FunctionListWidget(QWidget* parent = nullptr);

    /// 设置函数片段数据
    void setSnippets(const QList<FunctionSnippet>& snippets);
    
    /// 获取指定ID的片段
    FunctionSnippet snippetById(const QString& id) const;

signals:
    /// 拖拽开始时发出
    void dragStarted(const QString& snippetId);
    /// 拖拽结束时发出
    void dragEnded();

protected:
    QMimeData* mimeData(const QList<QListWidgetItem*> items) const override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    QHash<QString, FunctionSnippet> m_snippetsMap;
};


/**
 * @brief CodeEditor
 * 自定义代码编辑器，支持行号显示、当前行高亮、括号匹配高亮。
 * 增强：拖拽视觉反馈、格式化插入。
 */
class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget* parent = nullptr);

    /// 设置外部额外高亮（用于 DSL mapping 标记等），会与当前行/括号高亮合并
    void setExternalSelections(const QList<QTextEdit::ExtraSelection>& selections);

    /// 绘制行号区域
    void lineNumberAreaPaintEvent(QPaintEvent* event);
    /// 计算行号区域宽度
    int lineNumberAreaWidth() const;

    /// 设置是否启用当前行高亮
    void setCurrentLineHighlightEnabled(bool enabled);
    /// 设置是否启用括号匹配高亮
    void setBracketMatchingEnabled(bool enabled);

    /// 重新应用当前行高亮（提供给外部调用的接口）
    void updateCurrentLineHighlight();

    /// 设置是否启用拖拽高亮反馈
    void setDragHighlightEnabled(bool enabled);
    
    /// 设置拖拽处理器
    void setDragDropHandler(DslDragDropHandler* handler) { m_dragHandler = handler; }
    
    /// 获取当前拖拽状态
    bool isDragging() const { return m_isDragging; }
    
    /// 设置拖拽目标行（由外部拖拽处理器调用）
    void setDragTargetLine(int line, bool highlight);

signals:
    /// 文档修改状态变化信号
    void modificationChanged(bool modified);
    
    /// 拖拽放下信号（携带插入位置和数据）
    void snippetDropped(const QString& snippetId, int lineNumber);
    
    /// 拖拽被拒绝信号（用于显示错误提示）
    void dropRejected(const QString& reason);

protected:
    void resizeEvent(QResizeEvent* event) override;
    
    // 拖拽事件处理（委托给 DslDragDropHandler）
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    /// 当行数变化时更新行号区域宽度
    void updateLineNumberAreaWidth(int newBlockCount);
    /// 当文本滚动或更新时重绘行号区域
    void updateLineNumberArea(const QRect& rect, int dy);
    /// 高亮当前行
    void highlightCurrentLine();
    /// 高亮匹配的括号
    void highlightMatchingBrackets();

private:
    /// 查找匹配的括号位置，返回 -1 表示未找到
    int findMatchingBracket(int position, QChar openBracket, QChar closeBracket, bool forward) const;

private:
    LineNumberArea* m_lineNumberArea;      ///< 行号区域
    bool m_currentLineHighlightEnabled;    ///< 是否启用当前行高亮
    bool m_bracketMatchingEnabled;         ///< 是否启用括号匹配高亮
    bool m_dragHighlightEnabled;           ///< 是否启用拖拽高亮
    bool m_isDragging;                     ///< 当前是否处于拖拽状态
    int  m_dragTargetLine;                 ///< 拖拽目标行号

    QList<QTextEdit::ExtraSelection> m_externalSelections; ///< 外部高亮（映射标记等）
    QColor m_dragHighlightColor;           ///< 拖拽高亮颜色
    DslDragDropHandler* m_dragHandler = nullptr;  ///< 拖拽处理器
};


/**
 * @brief LineNumberArea
 * 行号区域控件，与 CodeEditor 配合使用。
 */
class LineNumberArea : public QWidget
{
    Q_OBJECT

public:
    explicit LineNumberArea(CodeEditor* editor)
        : QWidget(editor)
        , m_codeEditor(editor)
    {
    }

    QSize sizeHint() const override
    {
        return QSize(m_codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        m_codeEditor->lineNumberAreaPaintEvent(event);
    }

private:
    CodeEditor* m_codeEditor;
};


/**
 * @brief DslScriptEditor
 * DSL 脚本编辑器总控类：
 * - 组合 CodeEditor、FunctionListWidget、查找替换等 UI；
 * - 创建并持有 DslSyntaxHighlighter、DslCompletionEngine、DslDragDropHandler；
 * - 协调各模块的交互。
 */
class DslScriptEditor : public QWidget
{
    Q_OBJECT
public:
    explicit DslScriptEditor(QWidget* parent = nullptr);
    ~DslScriptEditor() override;

    /// 获取当前脚本文本
    QString currentScript() const;
    QString scriptForSave() const;
    /// 设置脚本文本
    void setScript(const QString& text);
    /// 清空脚本
    void clearScript();

    /// 获取当前文件路径
    QString currentFilePath() const { return m_currentFilePath; }
    /// 设置当前文件路径
    void setCurrentFilePath(const QString& path) { m_currentFilePath = path; }

    /// 跳转到指定行（行号从 1 开始）
    void gotoLine(int lineNumber);


    // ===== DSL 映射稳定（插入标记法） =====

    /// 设置 DSL 映射列表（用于高亮与跳转）
    void setDslMappings(const QList<DslMappingEntry>& mappings);

    /// 扫描脚本中的 mapping marker，返回 mappingId -> marker 行号（从 1 开始）
    QMap<QString, int> scanDslMappingMarkers() const;
    static QString stripDslMappingMarkers(const QString& script);

    /// 查找某个 mappingId 的 marker 行号（从 1 开始），找不到返回 -1
    int findDslMappingMarkerLine(const QString& mappingId) const;

    /// 确保某个 mappingId 的 marker 存在；优先插入在 nearCodeLine 前一行
    /// 返回 marker 行号（从 1 开始），失败返回 -1
    /// 根据 mappingId 跳转到对应代码行（优先 marker 下一行）
    void gotoMappingId(const QString& mappingId);

    /// 显示查找工具条
    void showFindBar();
    /// 隐藏查找工具条
    void hideFindBar();

    /// 获取内部编辑器指针（供外部需要时使用）
    CodeEditor* editor() const { return m_editor; }

    /// 设置函数列表的可见性
    void setFunctionListVisible(bool visible);
    /// 获取函数列表的可见性
    bool isFunctionListVisible() const;

    /// 设置文档修改状态（用于保存后重置）
    void setModified(bool modified);
    /// 获取文档修改状态
    bool isModified() const;

    // ===== 拖拽组态相关 =====
    
    /// 获取所有插入记录
    QList<DslInsertRecord> insertRecords() const { return m_insertRecords; }
    /// 清空插入记录
    void clearInsertRecords() { m_insertRecords.clear(); }
    
    /// 获取可用的函数片段列表
    QList<FunctionSnippet> availableSnippets() const;
    
    /// 根据ID获取函数片段
    FunctionSnippet snippetById(const QString& id) const;

    /// 设置状态栏消息回调（用于显示拖拽提示）
    void setStatusCallback(std::function<void(const QString&)> callback);

    // ===== 模块访问 =====
    
    /// 获取补全引擎
    DslCompletionEngine* completionEngine() const { return m_completionEngine; }
    
    /// 获取语法高亮器
    DslSyntaxHighlighter* syntaxHighlighter() const { return m_syntaxHighlighter; }
    
    /// 获取拖拽处理器
    DslDragDropHandler* dragDropHandler() const { return m_dragDropHandler; }

signals:
    /// 脚本内容被修改时发出
    void scriptModified();

    /**
     * @brief cursorPositionChanged
     * 光标位置变化信号
     * @param line 当前行号（从 1 开始）
     * @param column 当前列号（从 1 开始）
     * @param totalLines 文档总行数
     */
    void cursorPositionChanged(int line, int column, int totalLines);

    /**
     * @brief editorModified
     * 编辑器修改状态变化信号
     * @param modified 是否已修改（未保存状态）
     */
    void editorModified(bool modified);

    /**
     * @brief snippetInserted
     * 当一个函数片段被拖拽插入时发出
     * @param record 插入记录
     */
    void snippetInserted(const DslInsertRecord& record);

    /**
     * @brief dropError
     * 当拖拽失败时发出
     * @param errorMessage 错误信息
     */
    void dropError(const QString& errorMessage);

private slots:
    /// 双击函数列表项时插入模板
    void onFunctionItemActivated(QListWidgetItem* item);
    /// 当用户从补全列表中选择一个函数名时触发
    void insertCompletion(const QString& completion);

    // ===== DSL 映射导航（Ctrl+M） =====
    void onShowDslMappings();

    // ===== 查找/替换相关槽函数 =====
    void onFindNext();
    void onFindPrevious();
    void onReplace();
    void onReplaceAll();
    void onFindTextChanged(const QString& text);

    // ===== 光标位置变化内部槽函数 =====
    void onEditorCursorPositionChanged();

    // ===== 编辑器修改状态变化内部槽函数 =====
    void onEditorModificationChanged(bool modified);

    // ===== 拖拽相关槽函数 =====
    void onDropSucceeded(const DragDropResult& result);
    void onDropRejected(const QString& reason);
    void onDragStarted(const QString& snippetId);
    void onDragEnded();
    void onHighlightUpdateRequested(int lineNumber, bool highlight);

private:
    /// 初始化基础 UI
    void setupUi();
    /// 初始化查找/替换工具条
    void setupFindReplaceBar();
    /// 初始化快捷键
    void setupShortcuts();
    /// 初始化模块
    void setupModules();
    /// 初始化补全功能
    void initCompletion();
    /// 连接模块信号
    void connectModuleSignals();

    /// 向脚本中插入一段 DSL 模板
    void insertSnippet(const QString& text);

    // ===== 自动补全相关 =====
    bool eventFilter(QObject* obj, QEvent* event) override;
    void updateCompletionPrefix(bool forceShow);
    QString currentWordPrefix(int* outBlockPos = nullptr, int* outCursorColumn = nullptr) const;
    static bool isIdentifierChar(const QChar& ch);

    // ===== 查找相关辅助函数 =====
    bool findText(const QString& text, bool forward, bool caseSensitive);
    void clearFindHighlights();
    void highlightAllMatches(const QString& text, bool caseSensitive);

private:
    // ===== UI 组件 =====
    FunctionListWidget* m_functionList;
    CodeEditor*         m_editor;
    QSplitter*          m_splitter;

    // ===== 查找/替换工具条 =====
    QWidget*      m_findReplaceBar;
    QLineEdit*    m_findEdit;
    QLineEdit*    m_replaceEdit;
    QPushButton*  m_findNextButton;
    QPushButton*  m_findPrevButton;
    QPushButton*  m_replaceButton;
    QPushButton*  m_replaceAllButton;
    QPushButton*  m_closeFindBarButton;
    QCheckBox*    m_caseSensitiveCheck;

    // ===== 快捷键 =====
    QShortcut*    m_findShortcut;
    QShortcut*    m_findNextShortcut;
    QShortcut*    m_findPrevShortcut;
    QShortcut*    m_escapeShortcut;
    QShortcut*    m_mappingShortcut = nullptr;

    // ===== 模块 =====
    DslSyntaxHighlighter* m_syntaxHighlighter = nullptr;  ///< 语法高亮器
    DslCompletionEngine*  m_completionEngine = nullptr;   ///< 补全引擎
    DslDragDropHandler*   m_dragDropHandler = nullptr;    ///< 拖拽处理器

    // ===== 其他成员 =====
    QString m_currentFilePath;

    // DSL 映射列表（用于高亮与 Ctrl+M 导航）
    QList<DslMappingEntry> m_dslMappings;

    // 查找高亮相关
    QList<QTextEdit::ExtraSelection> m_findHighlights;

    // 函数列表显示状态相关
    QList<int> m_savedSplitterSizes;

    // ===== 拖拽组态相关 =====
    QList<DslInsertRecord> m_insertRecords;          ///< 插入记录列表
    std::function<void(const QString&)> m_statusCallback;  ///< 状态消息回调
};

#endif // DSLSCRIPTEDITOR_H
