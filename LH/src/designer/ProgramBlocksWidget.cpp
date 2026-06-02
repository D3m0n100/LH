/**
 * @file ProgramBlocksWidget.cpp
 */

#include "ProgramBlocksWidget.h"

#include "DslCompletionEngine.h"   // FunctionSnippet
#include "DslDragDropHandler.h"    // DSL_SNIPPET_MIME_TYPE

#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMimeData>
#include <QDrag>
#include <QDataStream>
#include <QPainter>
#include <QApplication>

#include <QMap>

#include <algorithm>

// ============================================================================
// 内部：支持拖拽的 Tree
// ============================================================================

class ProgramBlocksTreeWidget final : public QTreeWidget
{
public:
    explicit ProgramBlocksTreeWidget(QWidget* parent = nullptr)
        : QTreeWidget(parent)
    {
        setHeaderHidden(true);
        setRootIsDecorated(true);
        setUniformRowHeights(true);
        setAlternatingRowColors(true);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setDefaultDropAction(Qt::CopyAction);
        setExpandsOnDoubleClick(true);
    }

protected:
    QMimeData* mimeData(const QList<QTreeWidgetItem*> items) const override
    {
        if (items.isEmpty()) {
            return nullptr;
        }

        const QTreeWidgetItem* item = items.first();
        // 一级分类节点不允许拖拽
        if (!item || item->childCount() > 0) {
            return nullptr;
        }

        const QString snippetId = item->data(0, Qt::UserRole).toString();
        QString snippetCode = item->data(0, Qt::UserRole + 1).toString();
        if (snippetCode.isEmpty()) {
            snippetCode = item->text(0);
        }

        auto* mime = new QMimeData;
        // 兼容：纯文本
        mime->setText(snippetCode);
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << snippetId << snippetCode;
        mime->setData(DSL_SNIPPET_MIME_TYPE, data);

        return mime;
    }

    void startDrag(Qt::DropActions supportedActions) override
    {
        const auto items = selectedItems();
        if (items.isEmpty()) {
            return;
        }

        QMimeData* md = mimeData(items);
        if (!md) {
            return;
        }

        QDrag* drag = new QDrag(this);
        drag->setMimeData(md);

        // 简单拖拽预览，与 FunctionListWidget 保持一致体验
        const QString text = items.first()->text(0);
        QPixmap pixmap(180, 30);
        pixmap.fill(QColor("#e8f3ff"));
        QPainter painter(&pixmap);
        painter.setPen(QColor("#007acc"));
        painter.drawRect(0, 0, pixmap.width() - 1, pixmap.height() - 1);
        painter.setPen(QColor("#1f1f1f"));
        painter.drawText(QRect(6, 0, pixmap.width() - 12, pixmap.height()),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         text);
        drag->setPixmap(pixmap);
        drag->setHotSpot(QPoint(12, 15));

        drag->exec(supportedActions);
    }
};

// ============================================================================
// ProgramBlocksWidget
// ============================================================================

ProgramBlocksWidget::ProgramBlocksWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ProgramBlocksWidget"));
    setStyleSheet(R"(
QWidget#ProgramBlocksWidget {
    background: #ffffff;
}
QLineEdit {
    min-height: 24px;
    padding: 3px 8px;
}
QTreeWidget {
    background: #ffffff;
    alternate-background-color: #f6f8fa;
    border: 1px solid #d0d7de;
    border-radius: 4px;
    selection-background-color: #cce7ff;
    selection-color: #1f1f1f;
}
QTreeWidget::item {
    min-height: 24px;
    padding: 3px 4px;
}
QTreeWidget::item:hover {
    background: #e8f3ff;
}
)");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 8);
    layout->setSpacing(6);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("搜索程序块...（名称或描述）");
    m_filterEdit->setClearButtonEnabled(true);
    layout->addWidget(m_filterEdit);

    m_tree = new ProgramBlocksTreeWidget(this);
    m_tree->setObjectName("ProgramBlocksTree");
    layout->addWidget(m_tree, 1);

    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &ProgramBlocksWidget::onFilterTextChanged);
}

void ProgramBlocksWidget::setCompletionEngine(DslCompletionEngine* engine)
{
    if (m_engine == engine) {
        return;
    }

    if (m_engine) {
        disconnect(m_engine, nullptr, this, nullptr);
    }

    m_engine = engine;

    if (m_engine) {
        connect(m_engine, &DslCompletionEngine::snippetsChanged,
                this, &ProgramBlocksWidget::reloadFromEngine);
    }

    reloadFromEngine();
}

void ProgramBlocksWidget::setSnippets(const QList<FunctionSnippet>& snippets)
{
    m_cachedSnippets = snippets;
    rebuildTree(m_cachedSnippets);
    applyFilter(m_filterEdit ? m_filterEdit->text() : QString());
}

void ProgramBlocksWidget::reloadFromEngine()
{
    if (!m_engine) {
        setSnippets({});
        return;
    }
    setSnippets(m_engine->availableSnippets());
}

void ProgramBlocksWidget::onFilterTextChanged(const QString& text)
{
    applyFilter(text);
}

QString ProgramBlocksWidget::normalizeCategory(const QString& category)
{
    const QString c = category.trimmed();
    return c.isEmpty() ? QStringLiteral("general") : c;
}

QString ProgramBlocksWidget::makeSnippetTooltip(const FunctionSnippet& snippet)
{
    const QString desc = snippet.description.isEmpty() ? QStringLiteral("无描述") : snippet.description;
    const QString cat = snippet.category.isEmpty() ? QStringLiteral("通用") : snippet.category;
    const QString unit = snippet.unit.isEmpty() ? QStringLiteral("-") : snippet.unit;
    const QString codePreview = snippet.templateCode.left(140).replace("\n", "<br/>");

    return QString("<b>%1</b><br/>"
                   "<i>%2</i><br/><br/>"
                   "分类: %3<br/>"
                   "单位: %4<br/>"
                   "采样周期: %5 ms<br/><br/>"
                   "<code>%6</code>")
        .arg(snippet.name)
        .arg(desc)
        .arg(cat)
        .arg(unit)
        .arg(snippet.defaultPeriodMs)
        .arg(codePreview);
}

void ProgramBlocksWidget::rebuildTree(const QList<FunctionSnippet>& snippets)
{
    if (!m_tree) {
        return;
    }

    m_tree->clear();

    // category -> parent item
    QMap<QString, QTreeWidgetItem*> categoryItems;

    // 稳定排序：先按 category，再按 name
    QList<FunctionSnippet> sorted = snippets;
    std::sort(sorted.begin(), sorted.end(), [](const FunctionSnippet& a, const FunctionSnippet& b) {
        const QString ca = a.category.toLower();
        const QString cb = b.category.toLower();
        if (ca != cb) return ca < cb;
        return a.name.toLower() < b.name.toLower();
    });

    for (const auto& sn : sorted) {
        const QString catKey = normalizeCategory(sn.category);
        QTreeWidgetItem* parent = categoryItems.value(catKey, nullptr);
        if (!parent) {
            const QString catText = sn.category.isEmpty() ? QStringLiteral("通用") : sn.category;
            parent = new QTreeWidgetItem(m_tree, QStringList() << catText);
            parent->setFirstColumnSpanned(true);
            parent->setFlags(parent->flags() & ~Qt::ItemIsDragEnabled);
            categoryItems.insert(catKey, parent);
        }

        auto* leaf = new QTreeWidgetItem(parent, QStringList() << sn.name);
        leaf->setData(0, Qt::UserRole, sn.id);
        leaf->setData(0, Qt::UserRole + 1, sn.templateCode);
        leaf->setData(0, Qt::UserRole + 2, sn.description);
        leaf->setToolTip(0, makeSnippetTooltip(sn));

        // 简单分类配色，沿用 FunctionListWidget 的视觉语义
        if (sn.category == "input") {
            leaf->setForeground(0, QColor("#16825d"));
        } else if (sn.category == "output") {
            leaf->setForeground(0, QColor("#007acc"));
        } else if (sn.category == "control") {
            leaf->setForeground(0, QColor("#c42b1c"));
        }
    }

    m_tree->expandAll();
}

void ProgramBlocksWidget::applyFilter(const QString& text)
{
    if (!m_tree) {
        return;
    }

    const QString key = text.trimmed().toLower();
    const bool filtering = !key.isEmpty();

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* catItem = m_tree->topLevelItem(i);
        if (!catItem) continue;

        bool anyChildVisible = false;
        for (int c = 0; c < catItem->childCount(); ++c) {
            QTreeWidgetItem* leaf = catItem->child(c);
            if (!leaf) continue;

            const QString name = leaf->text(0).toLower();
            const QString desc = leaf->data(0, Qt::UserRole + 2).toString().toLower();
            const bool match = !filtering || name.contains(key) || desc.contains(key);
            leaf->setHidden(!match);
            anyChildVisible = anyChildVisible || match;
        }

        catItem->setHidden(!anyChildVisible);
        if (filtering && anyChildVisible) {
            m_tree->expandItem(catItem);
        }
    }
}

