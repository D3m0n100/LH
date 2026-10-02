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
#include <QLabel>
#include <QPushButton>

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
        if (!item || item->childCount() > 0 || !item->data(0, Qt::UserRole + 3).toBool()) {
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

    m_emptyContainer = new QWidget(this);
    m_emptyContainer->setObjectName(QStringLiteral("ProgramBlocksEmptyState"));
    auto* emptyLayout = new QVBoxLayout(m_emptyContainer);
    emptyLayout->setContentsMargins(12, 24, 12, 24);
    emptyLayout->setSpacing(12);
    emptyLayout->setAlignment(Qt::AlignCenter);

    m_emptyLabel = new QLabel(m_emptyContainer);
    m_emptyLabel->setObjectName(QStringLiteral("ProgramBlocksEmptyLabel"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setTextFormat(Qt::RichText);
    m_emptyLabel->setStyleSheet(QStringLiteral("color: #6e7781; font-size: 13px; line-height: 1.5;"));
    emptyLayout->addWidget(m_emptyLabel);

    m_clearFilterBtn = new QPushButton(QStringLiteral("清除搜索"), m_emptyContainer);
    m_clearFilterBtn->setObjectName(QStringLiteral("ClearFilterButton"));
    m_clearFilterBtn->setFixedWidth(96);
    m_clearFilterBtn->setStyleSheet(R"(
QPushButton {
    background-color: #f6f8fa;
    border: 1px solid #d0d7de;
    border-radius: 4px;
    padding: 4px 12px;
    color: #24292f;
    font-size: 12px;
}
QPushButton:hover {
    background-color: #f3f4f6;
    border-color: #0969da;
}
QPushButton:pressed {
    background-color: #ebecf0;
}
)");
    emptyLayout->addWidget(m_clearFilterBtn, 0, Qt::AlignCenter);

    layout->addWidget(m_emptyContainer, 1);
    m_emptyContainer->hide();

    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &ProgramBlocksWidget::onFilterTextChanged);
    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, &ProgramBlocksWidget::onCurrentItemChanged);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &ProgramBlocksWidget::onTreeItemDoubleClicked);
    connect(m_clearFilterBtn, &QPushButton::clicked, this, [this]() {
        if (m_filterEdit) {
            m_filterEdit->clear();
        }
    });
    connect(m_emptyLabel, &QLabel::linkActivated, this, [this]() {
        if (m_filterEdit) {
            m_filterEdit->clear();
        }
    });

    updateEmptyState(0);
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
        connect(m_engine, &QObject::destroyed, this, [this]() {
            m_engine = nullptr;
            reloadFromEngine();
        });
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

void ProgramBlocksWidget::filterCategory(const QString& category)
{
    if (m_filterEdit) {
        m_filterEdit->setText(category);
    }
}

void ProgramBlocksWidget::onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    Q_UNUSED(previous);
    if (!current || current->childCount() > 0) {
        return;
    }
    const QString id = current->data(0, Qt::UserRole).toString();
    FunctionSnippet sn = findSnippetById(id);
    if (sn.isValid()) {
        emit snippetSelected(sn);
    }
}

void ProgramBlocksWidget::onTreeItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (!item || item->childCount() > 0) {
        return;
    }
    const QString id = item->data(0, Qt::UserRole).toString();
    FunctionSnippet sn = findSnippetById(id);
    if (sn.isValid() && sn.canInsert()) {
        emit snippetDoubleClicked(sn);
    }
}

FunctionSnippet ProgramBlocksWidget::findSnippetById(const QString& id) const
{
    for (const auto& sn : m_cachedSnippets) {
        if (sn.id == id) {
            return sn;
        }
    }
    return FunctionSnippet();
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
                   "采样周期: %5 ms<br/>"
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
        leaf->setData(0, Qt::UserRole + 3, sn.canInsert());
        leaf->setToolTip(0, makeSnippetTooltip(sn));
        if (!sn.canInsert()) {
            leaf->setFlags(leaf->flags() & ~Qt::ItemIsDragEnabled);
        }

        // 简单分类配色
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
    int visibleLeafCount = 0;

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* catItem = m_tree->topLevelItem(i);
        if (!catItem) continue;

        const bool catMatch = !key.isEmpty() && catItem->text(0).toLower().contains(key);
        bool anyChildVisible = false;
        for (int c = 0; c < catItem->childCount(); ++c) {
            QTreeWidgetItem* leaf = catItem->child(c);
            if (!leaf) continue;

            const QString name = leaf->text(0).toLower();
            const QString desc = leaf->data(0, Qt::UserRole + 2).toString().toLower();
            const bool match = !filtering || catMatch || name.contains(key) || desc.contains(key);
            leaf->setHidden(!match);
            if (match) {
                anyChildVisible = true;
                ++visibleLeafCount;
            }
        }

        catItem->setHidden(!anyChildVisible);
        if (filtering && anyChildVisible) {
            m_tree->expandItem(catItem);
        }
    }

    updateEmptyState(visibleLeafCount);
}

void ProgramBlocksWidget::updateEmptyState(int visibleLeafCount)
{
    if (!m_emptyContainer || !m_emptyLabel || !m_tree) {
        return;
    }

    if (!m_engine) {
        m_emptyLabel->setText(QStringLiteral("编辑器未打开，暂无可用函数"));
        if (m_clearFilterBtn) {
            m_clearFilterBtn->setVisible(false);
        }
        m_emptyContainer->setVisible(true);
        m_tree->setVisible(false);
        return;
    }

    if (m_cachedSnippets.isEmpty()) {
        m_emptyLabel->setText(QStringLiteral("暂无函数"));
        if (m_clearFilterBtn) {
            m_clearFilterBtn->setVisible(false);
        }
        m_emptyContainer->setVisible(true);
        m_tree->setVisible(false);
        return;
    }

    if (visibleLeafCount == 0) {
        m_emptyLabel->setText(QStringLiteral("没有匹配结果 (<a href=\"#clear\" style=\"color:#0969da; text-decoration:none;\">清除搜索</a>)"));
        if (m_clearFilterBtn) {
            m_clearFilterBtn->setVisible(true);
        }
        m_emptyContainer->setVisible(true);
        m_tree->setVisible(false);
        return;
    }

    m_emptyContainer->setVisible(false);
    m_tree->setVisible(true);
}

