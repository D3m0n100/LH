/**
 * @file ProgramBlocksWidget.h
 * @brief 程序块（Program Blocks）面板：以 CODESYS 风格展示 DSL snippets，支持搜索 + 分组 + 拖拽插入
 *
 * 设计目标：
 * - 复用现有 DslCompletionEngine / FunctionSnippet 数据（禁止重复维护）
 * - 拖拽输出与 FunctionListWidget 完全一致的 MIME（DSL_SNIPPET_MIME_TYPE + QDataStream<<id<<code，且 setText(code) 兼容）
 * - UI 形态：搜索框 + 分类树（一级=category，二级=snippet）
 */

#ifndef PROGRAMBLOCKSWIDGET_H
#define PROGRAMBLOCKSWIDGET_H

#include <QWidget>
#include <QList>

class QLineEdit;
class QTreeWidget;

struct FunctionSnippet;
class DslCompletionEngine;

/**
 * @brief 程序块面板
 */
class ProgramBlocksWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProgramBlocksWidget(QWidget* parent = nullptr);

    /// 绑定补全引擎作为数据源（推荐：直接传 DslScriptEditor::completionEngine()）
    void setCompletionEngine(DslCompletionEngine* engine);
    DslCompletionEngine* completionEngine() const { return m_engine; }

    /// 直接设置 snippets（通常由引擎驱动刷新）
    void setSnippets(const QList<FunctionSnippet>& snippets);

private slots:
    void onFilterTextChanged(const QString& text);
    void reloadFromEngine();

private:
    void rebuildTree(const QList<FunctionSnippet>& snippets);
    void applyFilter(const QString& text);
    static QString normalizeCategory(const QString& category);
    static QString makeSnippetTooltip(const FunctionSnippet& snippet);

private:
    DslCompletionEngine* m_engine = nullptr;

    QLineEdit* m_filterEdit = nullptr;
    QTreeWidget* m_tree = nullptr;

    QList<FunctionSnippet> m_cachedSnippets;
};

#endif // PROGRAMBLOCKSWIDGET_H
