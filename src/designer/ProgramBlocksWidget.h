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
#include <QPointer>

class QLineEdit;
class QTreeWidgetItem;
class QTreeWidget;
class QLabel;
class QPushButton;

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
    DslCompletionEngine* completionEngine() const { return m_engine.data(); }

    /// 直接设置 snippets（通常由引擎驱动刷新）
    void setSnippets(const QList<FunctionSnippet>& snippets);

    /// 按分类筛选（如 "display"）
    void filterCategory(const QString& category);

    QTreeWidget* treeWidget() const { return m_tree; }
    QLabel* emptyLabel() const { return m_emptyLabel; }
    QPushButton* clearFilterButton() const { return m_clearFilterBtn; }

signals:
    void snippetSelected(const FunctionSnippet& snippet);
    void snippetDoubleClicked(const FunctionSnippet& snippet);

private slots:
    void onFilterTextChanged(const QString& text);
    void reloadFromEngine();
    void onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onTreeItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    void rebuildTree(const QList<FunctionSnippet>& snippets);
    void applyFilter(const QString& text);
    void updateEmptyState(int visibleLeafCount);
    FunctionSnippet findSnippetById(const QString& id) const;
    static QString normalizeCategory(const QString& category);
    static QString makeSnippetTooltip(const FunctionSnippet& snippet);

private:
    QPointer<DslCompletionEngine> m_engine;

    QLineEdit* m_filterEdit = nullptr;
    QTreeWidget* m_tree = nullptr;

    QWidget* m_emptyContainer = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QPushButton* m_clearFilterBtn = nullptr;

    QList<FunctionSnippet> m_cachedSnippets;
};

#endif // PROGRAMBLOCKSWIDGET_H
