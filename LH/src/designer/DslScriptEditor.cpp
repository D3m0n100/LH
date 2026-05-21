/**
 * @file DslScriptEditor.cpp
 * @brief DSL 脚本编辑器实现
 * ============== 架构说明 ==============
 *
 * 本文件为 DSL 编辑器的总控实现，主要职责：
 * - UI 组件的创建和布局
 * - 模块的初始化和协调
 * - 信号槽连接
 * - 查找/替换功能
 *
 * 具体功能由以下模块实现：
 * - DslSyntaxHighlighter: 语法高亮
 * - DslCompletionEngine: 代码补全
 * - DslDragDropHandler: 拖拽处理
 */

#include "DslScriptEditor.h"
#include "DslSyntaxHighlighter.h"
#include "DslCompletionEngine.h"
#include "DslDragDropHandler.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QDebug>
#include <QStringListModel>
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QTimer>
#include <QTextCursor>
#include <QTextBlock>
#include <QPainter>
#include <QTextCharFormat>
#include <QLabel>
#include <QMessageBox>
#include <QToolButton>
#include <QApplication>
#include <QMimeData>
#include <QDrag>
#include <QCompleter>
#include <QUuid>
#include <QRegularExpression>

// ============================================================================
// FunctionListWidget 瀹炵幇
// ============================================================================

FunctionListWidget::FunctionListWidget(QWidget* parent)
    : QListWidget(parent)
{
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setDefaultDropAction(Qt::CopyAction);
    
    // 鍚敤榧犳爣杩借釜浠ユ敮鎸?Tooltip
    setMouseTracking(true);
}

void FunctionListWidget::setSnippets(const QList<FunctionSnippet>& snippets)
{
    clear();
    m_snippetsMap.clear();
    
    for (const auto& snippet : snippets) {
        auto* item = new QListWidgetItem(snippet.name, this);
        item->setData(Qt::UserRole, snippet.id);
        item->setData(Qt::UserRole + 1, snippet.templateCode);
        
        // 璁剧疆 Tooltip
        QString tooltip = QString("<b>%1</b><br/>"
                                  "<i>%2</i><br/><br/>"
                                  "分类: %3<br/>"
                                  "单位: %4<br/>"
                                  "采样周期: %5 ms<br/><br/>"
                                  "<code>%6</code>")
            .arg(snippet.name)
            .arg(snippet.description.isEmpty() ? "无描述" : snippet.description)
            .arg(snippet.category.isEmpty() ? "通用" : snippet.category)
            .arg(snippet.unit.isEmpty() ? "-" : snippet.unit)
            .arg(snippet.defaultPeriodMs)
            .arg(snippet.templateCode.left(100).replace("\n", "<br/>"));
        item->setToolTip(tooltip);
        
        if (snippet.category == "input") {
            item->setForeground(QColor("#16825d"));
        } else if (snippet.category == "output") {
            item->setForeground(QColor("#007acc"));
        } else if (snippet.category == "control") {
            item->setForeground(QColor("#c42b1c"));
        }
        
        m_snippetsMap.insert(snippet.id, snippet);
    }
}

FunctionSnippet FunctionListWidget::snippetById(const QString& id) const
{
    return m_snippetsMap.value(id);
}

QMimeData* FunctionListWidget::mimeData(const QList<QListWidgetItem*> items) const
{
    if (items.isEmpty())
        return nullptr;

    auto* mime = new QMimeData;
    const QListWidgetItem* item = items.first();
    
    QString snippetId = item->data(Qt::UserRole).toString();
    QString snippetCode = item->data(Qt::UserRole + 1).toString();
    
    if (snippetCode.isEmpty()) {
        snippetCode = item->text();
    }
    
    mime->setText(snippetCode);
    
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << snippetId << snippetCode;
    mime->setData(DSL_SNIPPET_MIME_TYPE, data);
    
    return mime;
}

void FunctionListWidget::startDrag(Qt::DropActions supportedActions)
{
    QList<QListWidgetItem*> items = selectedItems();
    if (items.isEmpty())
        return;

    QString snippetId = items.first()->data(Qt::UserRole).toString();
    emit dragStarted(snippetId);
    
    // 楂樹寒琚嫋鎷界殑鏉＄洰
    items.first()->setBackground(QColor("#cce7ff"));
    
    // 鍒涘缓鎷栨嫿
    QMimeData* mimeData = this->mimeData(items);
    if (!mimeData)
        return;

    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    
    // 璁剧疆鎷栨嫿鏃剁殑榧犳爣鍏夋爣
    QPixmap pixmap(150, 30);
    pixmap.fill(QColor("#e8f3ff"));
    QPainter painter(&pixmap);
    painter.setPen(Qt::black);
    painter.drawRect(0, 0, 149, 29);
    painter.drawText(5, 20, items.first()->text());
    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(10, 15));
    
    // 鎵ц鎷栨嫿
    Qt::DropAction result = drag->exec(supportedActions);
    Q_UNUSED(result);
    
    // 鎭㈠鏉＄洰澶栬
    items.first()->setBackground(Qt::transparent);
    
    emit dragEnded();
}


// ============================================================================
// CodeEditor 瀹炵幇
// ============================================================================

CodeEditor::CodeEditor(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_lineNumberArea(new LineNumberArea(this))
    , m_currentLineHighlightEnabled(true)
    , m_bracketMatchingEnabled(true)
    , m_dragHighlightEnabled(true)
    , m_isDragging(false)
    , m_dragTargetLine(-1)
    , m_dragHighlightColor(QColor(204, 231, 255, 120))
{
    // 鍚敤鎷栨斁
    setAcceptDrops(true);
    
    connect(this, &CodeEditor::blockCountChanged,
            this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest,
            this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged,
            this, &CodeEditor::highlightCurrentLine);
    connect(this, &CodeEditor::cursorPositionChanged,
            this, &CodeEditor::highlightMatchingBrackets);

    connect(document(), &QTextDocument::modificationChanged,
            this, &CodeEditor::modificationChanged);

    updateLineNumberAreaWidth(0);
    // 鍒濆鍖栧綋鍓嶈楂樹寒
    highlightCurrentLine();
}

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    digits = qMax(digits, 3);
    int space = 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect& rect, int dy)
{
    if (dy) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

void CodeEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(),
                                        lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor("#f3f3f3"));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    int currentLine = textCursor().blockNumber();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);

            if (m_isDragging && blockNumber == m_dragTargetLine) {
                painter.fillRect(0, top, m_lineNumberArea->width(), fontMetrics().height(),
                                 m_dragHighlightColor);
                painter.setPen(QColor("#007acc"));
                QFont boldFont = painter.font();
                boldFont.setBold(true);
                painter.setFont(boldFont);
            }
            else if (blockNumber == currentLine) {
                painter.setPen(QColor("#1f1f1f"));
                QFont boldFont = painter.font();
                boldFont.setBold(true);
                painter.setFont(boldFont);
            } else {
                painter.setPen(QColor("#6a737d"));
                QFont normalFont = painter.font();
                normalFont.setBold(false);
                painter.setFont(normalFont);
            }

            painter.drawText(0, top, m_lineNumberArea->width() - 5,
                             fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CodeEditor::setCurrentLineHighlightEnabled(bool enabled)
{
    m_currentLineHighlightEnabled = enabled;
    highlightCurrentLine();
}

void CodeEditor::setBracketMatchingEnabled(bool enabled)
{
    m_bracketMatchingEnabled = enabled;
    if (enabled) {
        highlightMatchingBrackets();
    } else {
        highlightCurrentLine();
    }
}

void CodeEditor::setDragHighlightEnabled(bool enabled)
{
    m_dragHighlightEnabled = enabled;
}

void CodeEditor::setDragTargetLine(int line, bool highlight)
{
    if (highlight) {
        m_isDragging = true;
        m_dragTargetLine = line;
    } else {
        m_isDragging = false;
        m_dragTargetLine = -1;
    }
    highlightCurrentLine();
    m_lineNumberArea->update();
}

void CodeEditor::updateCurrentLineHighlight()
{
    highlightCurrentLine();
}

void CodeEditor::setExternalSelections(const QList<QTextEdit::ExtraSelection>& selections)
{
    m_externalSelections = selections;
    highlightCurrentLine();
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections = m_externalSelections;

    if (m_currentLineHighlightEnabled && !isReadOnly()) {
        QTextEdit::ExtraSelection selection;

        QColor lineColor = QColor("#fff8c5");
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    if (m_isDragging && m_dragTargetLine >= 0 && m_dragHighlightEnabled) {
        QTextEdit::ExtraSelection dragSelection;
        dragSelection.format.setBackground(m_dragHighlightColor);
        dragSelection.format.setProperty(QTextFormat::FullWidthSelection, true);
        
        QTextBlock block = document()->findBlockByNumber(m_dragTargetLine);
        if (block.isValid()) {
            QTextCursor cursor(block);
            dragSelection.cursor = cursor;
            dragSelection.cursor.clearSelection();
            extraSelections.append(dragSelection);
        }
    }

    setExtraSelections(extraSelections);

    if (m_bracketMatchingEnabled) {
        highlightMatchingBrackets();
    }
}

void CodeEditor::highlightMatchingBrackets()
{
    if (!m_bracketMatchingEnabled) {
        return;
    }

    QList<QTextEdit::ExtraSelection> extraSelections = this->extraSelections();

    QTextCursor cursor = textCursor();
    int pos = cursor.position();
    QTextDocument* doc = document();

    if (pos <= 0 || pos > doc->characterCount()) {
        return;
    }

    QChar charBefore = doc->characterAt(pos - 1);
    QChar charAfter = doc->characterAt(pos);

    struct BracketPair {
        QChar open;
        QChar close;
    };
    QList<BracketPair> pairs = {
        {'(', ')'},
        {'{', '}'},
        {'[', ']'}
    };

    auto addBracketHighlight = [&](int bracketPos) {
        if (bracketPos < 0) return;

        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor("#dff6dd"));
        selection.format.setForeground(QColor("#16825d"));
        QFont font = selection.format.font();
        font.setBold(true);
        selection.format.setFontWeight(QFont::Bold);

        QTextCursor c = textCursor();
        c.setPosition(bracketPos);
        c.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
        selection.cursor = c;
        extraSelections.append(selection);
    };

    for (const auto& pair : pairs) {
        if (charBefore == pair.close) {
            int matchPos = findMatchingBracket(pos - 1, pair.open, pair.close, false);
            if (matchPos >= 0) {
                addBracketHighlight(pos - 1);
                addBracketHighlight(matchPos);
            }
            break;
        } else if (charBefore == pair.open) {
            int matchPos = findMatchingBracket(pos - 1, pair.open, pair.close, true);
            if (matchPos >= 0) {
                addBracketHighlight(pos - 1);
                addBracketHighlight(matchPos);
            }
            break;
        }
    }

    for (const auto& pair : pairs) {
        if (charAfter == pair.open) {
            int matchPos = findMatchingBracket(pos, pair.open, pair.close, true);
            if (matchPos >= 0) {
                addBracketHighlight(pos);
                addBracketHighlight(matchPos);
            }
            break;
        } else if (charAfter == pair.close) {
            int matchPos = findMatchingBracket(pos, pair.open, pair.close, false);
            if (matchPos >= 0) {
                addBracketHighlight(pos);
                addBracketHighlight(matchPos);
            }
            break;
        }
    }

    setExtraSelections(extraSelections);
}

int CodeEditor::findMatchingBracket(int position, QChar openBracket, QChar closeBracket, bool forward) const
{
    QTextDocument* doc = document();
    int depth = 0;

    if (forward) {
        for (int i = position; i < doc->characterCount(); ++i) {
            QChar ch = doc->characterAt(i);
            if (ch == openBracket) {
                ++depth;
            } else if (ch == closeBracket) {
                --depth;
                if (depth == 0) {
                    return i;
                }
            }
        }
    } else {
        for (int i = position; i >= 0; --i) {
            QChar ch = doc->characterAt(i);
            if (ch == closeBracket) {
                ++depth;
            } else if (ch == openBracket) {
                --depth;
                if (depth == 0) {
                    return i;
                }
            }
        }
    }

    return -1;
}

// ===== 鎷栨嫿浜嬩欢澶勭悊锛堝鎵樼粰 DslDragDropHandler锛?====

void CodeEditor::dragEnterEvent(QDragEnterEvent* event)
{
    if (m_dragHandler) {
        if (m_dragHandler->handleDragEnter(event, this)) {
            return;
        }
    }
    
    // 鍥為€€鍒伴粯璁ゅ鐞?    QPlainTextEdit::dragEnterEvent(event);
}

void CodeEditor::dragMoveEvent(QDragMoveEvent* event)
{
    if (m_dragHandler) {
        if (m_dragHandler->handleDragMove(event, this)) {
            return;
        }
    }
    
    QPlainTextEdit::dragMoveEvent(event);
}

void CodeEditor::dragLeaveEvent(QDragLeaveEvent* event)
{
    if (m_dragHandler) {
        m_dragHandler->handleDragLeave(event, this);
    }
    
    setDragTargetLine(-1, false);
    QPlainTextEdit::dragLeaveEvent(event);
}

void CodeEditor::dropEvent(QDropEvent* event)
{
    if (m_dragHandler) {
        DragDropResult result = m_dragHandler->handleDrop(event, this);
        if (result.success) {
            emit snippetDropped(result.snippetId, result.lineNumber);
            return;
        } else if (!result.errorMessage.isEmpty()) {
            emit dropRejected(result.errorMessage);
            return;
        }
    }
    
    QPlainTextEdit::dropEvent(event);
}


// ============================================================================
// DslScriptEditor 瀹炵幇
// ============================================================================

DslScriptEditor::DslScriptEditor(QWidget* parent)
    : QWidget(parent)
    , m_functionList(new FunctionListWidget(this))
    , m_editor(new CodeEditor(this))
    , m_splitter(nullptr)
    , m_findReplaceBar(nullptr)
    , m_findEdit(nullptr)
    , m_replaceEdit(nullptr)
    , m_findNextButton(nullptr)
    , m_findPrevButton(nullptr)
    , m_replaceButton(nullptr)
    , m_replaceAllButton(nullptr)
    , m_closeFindBarButton(nullptr)
    , m_caseSensitiveCheck(nullptr)
    , m_findShortcut(nullptr)
    , m_findNextShortcut(nullptr)
    , m_findPrevShortcut(nullptr)
    , m_escapeShortcut(nullptr)
{
    setupUi();
    setupShortcuts();
    setupModules();
    initCompletion();
    connectModuleSignals();

    // 鍒濆鍙戝皠涓€娆″厜鏍囦綅缃俊鍙凤紙鍒濆鐘舵€侊級
    onEditorCursorPositionChanged();
}

DslScriptEditor::~DslScriptEditor()
{
    // 妯″潡鐢?QObject 鐖跺瓙鍏崇郴鑷姩绠＄悊
}

void DslScriptEditor::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setupFindReplaceBar();
    mainLayout->addWidget(m_findReplaceBar);

    m_splitter = new QSplitter(this);
    m_splitter->setOrientation(Qt::Horizontal);

    m_functionList->setMinimumWidth(220);
    m_functionList->setSelectionMode(QAbstractItemView::SingleSelection);

    m_editor->setAcceptDrops(true);
    m_editor->setTabStopDistance(4 * m_editor->fontMetrics().horizontalAdvance(' '));
    m_editor->setPlaceholderText("// 在这里编写 DSL 脚本，例如:\n"
                                 "// drv_ai_1 = _DrvAI(...);\n"
                                 "// add_1 = _Add();\n"
                                 "// \n"
                                  "// 提示: 从左侧函数列表拖拽功能块到这里即可快速插入。");

    m_splitter->addWidget(m_functionList);
    m_splitter->addWidget(m_editor);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);

    m_splitter->setSizes({220, 600});

    mainLayout->addWidget(m_splitter);
    setLayout(mainLayout);

    connect(m_functionList, &QListWidget::itemDoubleClicked,
            this, &DslScriptEditor::onFunctionItemActivated);
    connect(m_editor, &QPlainTextEdit::textChanged,
            this, &DslScriptEditor::scriptModified);
    connect(m_editor, &QPlainTextEdit::cursorPositionChanged,
            this, &DslScriptEditor::onEditorCursorPositionChanged);
    connect(m_editor, &CodeEditor::modificationChanged,
            this, &DslScriptEditor::onEditorModificationChanged);
}

void DslScriptEditor::setupFindReplaceBar()
{
    m_findReplaceBar = new QWidget(this);
    m_findReplaceBar->setObjectName("FindReplaceBar");

    m_findReplaceBar->setStyleSheet(
        "#FindReplaceBar {"
        "  background-color: #f3f3f3;"
        "  border-bottom: 1px solid #d0d7de;"
        "  padding: 4px;"
        "}"
        "#FindReplaceBar QLabel { color: #3b3b3b; }"
        "#FindReplaceBar QLineEdit { min-height: 22px; }"
    );

    auto* layout = new QHBoxLayout(m_findReplaceBar);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    auto* findLabel = new QLabel("查找:", m_findReplaceBar);
    m_findEdit = new QLineEdit(m_findReplaceBar);
    m_findEdit->setPlaceholderText("输入查找内容...");
    m_findEdit->setMinimumWidth(150);
    m_findEdit->setClearButtonEnabled(true);

    auto* replaceLabel = new QLabel("替换:", m_findReplaceBar);
    m_replaceEdit = new QLineEdit(m_findReplaceBar);
    m_replaceEdit->setPlaceholderText("输入替换内容...");
    m_replaceEdit->setMinimumWidth(150);
    m_replaceEdit->setClearButtonEnabled(true);

    m_caseSensitiveCheck = new QCheckBox("区分大小写", m_findReplaceBar);

    m_findPrevButton = new QPushButton("上一个", m_findReplaceBar);
    m_findPrevButton->setToolTip("查找上一个匹配项 (Shift+F3)");

    m_findNextButton = new QPushButton("下一个", m_findReplaceBar);
    m_findNextButton->setToolTip("查找下一个匹配项 (F3)");

    m_replaceButton = new QPushButton("替换", m_findReplaceBar);
    m_replaceButton->setToolTip("替换当前匹配项");

    m_replaceAllButton = new QPushButton("全部替换", m_findReplaceBar);
    m_replaceAllButton->setToolTip("替换所有匹配项");

    m_closeFindBarButton = new QPushButton("×", m_findReplaceBar);
    m_closeFindBarButton->setFixedSize(24, 24);
    m_closeFindBarButton->setToolTip("关闭查找工具条 (Escape)");
    m_closeFindBarButton->setFlat(true);
    m_closeFindBarButton->setStyleSheet(
        "QPushButton { border: none; background: transparent; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background-color: #e8f3ff; }"
    );

    layout->addWidget(findLabel);
    layout->addWidget(m_findEdit);
    layout->addWidget(m_findPrevButton);
    layout->addWidget(m_findNextButton);
    layout->addSpacing(10);
    layout->addWidget(replaceLabel);
    layout->addWidget(m_replaceEdit);
    layout->addWidget(m_replaceButton);
    layout->addWidget(m_replaceAllButton);
    layout->addSpacing(10);
    layout->addWidget(m_caseSensitiveCheck);
    layout->addStretch();
    layout->addWidget(m_closeFindBarButton);

    m_findReplaceBar->setLayout(layout);
    m_findReplaceBar->setVisible(false);

    connect(m_findEdit, &QLineEdit::returnPressed, this, &DslScriptEditor::onFindNext);
    connect(m_findEdit, &QLineEdit::textChanged, this, &DslScriptEditor::onFindTextChanged);
    connect(m_findNextButton, &QPushButton::clicked, this, &DslScriptEditor::onFindNext);
    connect(m_findPrevButton, &QPushButton::clicked, this, &DslScriptEditor::onFindPrevious);
    connect(m_replaceButton, &QPushButton::clicked, this, &DslScriptEditor::onReplace);
    connect(m_replaceAllButton, &QPushButton::clicked, this, &DslScriptEditor::onReplaceAll);
    connect(m_closeFindBarButton, &QPushButton::clicked, this, &DslScriptEditor::hideFindBar);
}

void DslScriptEditor::setupShortcuts()
{
    m_findShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(m_findShortcut, &QShortcut::activated, this, &DslScriptEditor::showFindBar);

    m_findNextShortcut = new QShortcut(QKeySequence("F3"), this);
    connect(m_findNextShortcut, &QShortcut::activated, this, &DslScriptEditor::onFindNext);

    m_findPrevShortcut = new QShortcut(QKeySequence("Shift+F3"), this);
    connect(m_findPrevShortcut, &QShortcut::activated, this, &DslScriptEditor::onFindPrevious);

    m_escapeShortcut = new QShortcut(QKeySequence("Escape"), this);
    connect(m_escapeShortcut, &QShortcut::activated, this, &DslScriptEditor::hideFindBar);

    m_mappingShortcut = new QShortcut(QKeySequence("Ctrl+M"), this);
    connect(m_mappingShortcut, &QShortcut::activated, this, &DslScriptEditor::onShowDslMappings);
}

void DslScriptEditor::setupModules()
{
    // 鍒涘缓琛ュ叏寮曟搸
    m_completionEngine = new DslCompletionEngine(this);
    m_completionEngine->initBuiltinSnippets();
    
    m_syntaxHighlighter = new DslSyntaxHighlighter(m_editor->document());
    
    // 鏇存柊楂樹寒鍣ㄧ殑鍏抽敭瀛楋紙浠庤ˉ鍏ㄥ紩鎿庤幏鍙栵級
    m_syntaxHighlighter->updateKeywords(m_completionEngine->componentNames());
    
    m_dragDropHandler = new DslDragDropHandler(this);
    m_dragDropHandler->setCompletionEngine(m_completionEngine);
    
    m_editor->setDragDropHandler(m_dragDropHandler);
    
    // 鏇存柊鍑芥暟鍒楄〃
    m_functionList->setSnippets(m_completionEngine->availableSnippets());
}

void DslScriptEditor::initCompletion()
{
    if (!m_completionEngine) {
        return;
    }
    
    QCompleter* completer = m_completionEngine->completer();
    if (completer) {
        completer->setWidget(m_editor);
        
        connect(completer, QOverload<const QString&>::of(&QCompleter::activated),
                this, &DslScriptEditor::insertCompletion);
    }
    
    m_editor->installEventFilter(this);
}

void DslScriptEditor::connectModuleSignals()
{
    if (m_dragDropHandler) {
        connect(m_dragDropHandler, &DslDragDropHandler::dropSucceeded,
                this, &DslScriptEditor::onDropSucceeded);
        connect(m_dragDropHandler, &DslDragDropHandler::dropRejected,
                this, &DslScriptEditor::onDropRejected);
        connect(m_dragDropHandler, &DslDragDropHandler::dragStarted,
                this, &DslScriptEditor::onDragStarted);
        connect(m_dragDropHandler, &DslDragDropHandler::dragEnded,
                this, &DslScriptEditor::onDragEnded);
        connect(m_dragDropHandler, &DslDragDropHandler::highlightUpdateRequested,
                this, &DslScriptEditor::onHighlightUpdateRequested);
    }
    
    // 杩炴帴鍑芥暟鍒楄〃鎷栨嫿淇″彿
    connect(m_functionList, &FunctionListWidget::dragStarted,
            this, &DslScriptEditor::onDragStarted);
    connect(m_functionList, &FunctionListWidget::dragEnded,
            this, &DslScriptEditor::onDragEnded);
    
    // 杩炴帴琛ュ叏寮曟搸淇″彿
    if (m_completionEngine) {
        connect(m_completionEngine, &DslCompletionEngine::snippetsChanged,
                this, [this]() {
            // 鏇存柊鍑芥暟鍒楄〃
            m_functionList->setSnippets(m_completionEngine->availableSnippets());
            if (m_syntaxHighlighter) {
                m_syntaxHighlighter->updateKeywords(m_completionEngine->componentNames());
            }
        });
    }
}

void DslScriptEditor::showFindBar()
{
    m_findReplaceBar->setVisible(true);
    m_findEdit->setFocus();
    m_findEdit->selectAll();

    QTextCursor cursor = m_editor->textCursor();
    if (cursor.hasSelection()) {
        QString selectedText = cursor.selectedText();
        if (!selectedText.contains(QChar::ParagraphSeparator)) {
            m_findEdit->setText(selectedText);
            m_findEdit->selectAll();
        }
    }
}

void DslScriptEditor::hideFindBar()
{
    m_findReplaceBar->setVisible(false);
    clearFindHighlights();
    m_editor->setFocus();
}

void DslScriptEditor::gotoLine(int lineNumber)
{
    if (!m_editor || lineNumber < 1) {
        return;
    }

    QTextDocument* doc = m_editor->document();
    int blockCount = doc->blockCount();

    if (lineNumber > blockCount) {
        lineNumber = blockCount;
    }

    QTextBlock block = doc->findBlockByLineNumber(lineNumber - 1);
    if (!block.isValid()) {
        return;
    }

    QTextCursor cursor(block);
    m_editor->setTextCursor(cursor);
    m_editor->centerCursor();
    m_editor->setFocus();
}

// ===== DSL 鏄犲皠绋冲畾锛堟彃鍏ユ爣璁版硶锛?=====

QMap<QString, int> DslScriptEditor::scanDslMappingMarkers() const
{
    QMap<QString, int> result;
    if (!m_editor) {
        return result;
    }

    static const QRegularExpression re(
        R"(^\s*//\s*@dsl_mapping_id\s*:\s*([A-Za-z0-9\-]+)\s*$)"
    );

    QTextDocument* doc = m_editor->document();
    int line = 1;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next(), ++line) {
        QRegularExpressionMatch m = re.match(block.text());
        if (m.hasMatch()) {
            result.insert(m.captured(1), line);
        }
    }
    return result;
}

int DslScriptEditor::findDslMappingMarkerLine(const QString& mappingId) const
{
    if (mappingId.isEmpty() || !m_editor) {
        return -1;
    }

    static const QRegularExpression re(
        R"(^\s*//\s*@dsl_mapping_id\s*:\s*([A-Za-z0-9\-]+)\s*$)"
    );

    QTextDocument* doc = m_editor->document();
    int line = 1;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next(), ++line) {
        QRegularExpressionMatch m = re.match(block.text());
        if (m.hasMatch() && m.captured(1) == mappingId) {
            return line;
        }
    }
    return -1;
}

int DslScriptEditor::ensureDslMappingMarker(const QString& mappingId, int nearCodeLine)
{
    if (!m_editor || mappingId.isEmpty()) {
        return -1;
    }

    // 宸插瓨鍦ㄥ垯鐩存帴杩斿洖
    int existing = findDslMappingMarkerLine(mappingId);
    if (existing > 0) {
        return existing;
    }

    QTextDocument* doc = m_editor->document();
    const QString markerLine = QString("// @dsl_mapping_id: %1").arg(mappingId);

    int blockCount = doc->blockCount();
    if (nearCodeLine < 1) {
        nearCodeLine = 1;
    }

    if (nearCodeLine <= blockCount) {
        QTextBlock target = doc->findBlockByLineNumber(nearCodeLine - 1);
        if (!target.isValid()) {
            return -1;
        }

        // 鑻ヤ笂涓€琛屽凡缁忔槸璇?mapping 鐨?marker锛屽垯澶嶇敤
        QTextBlock prev = target.previous();
        if (prev.isValid() && prev.text().contains(mappingId) && prev.text().contains("@dsl_mapping_id")) {
            return prev.blockNumber() + 1;
        }

        QTextCursor cursor(target);
        cursor.insertText(markerLine + "\n");
        return findDslMappingMarkerLine(mappingId);
    }

    // 濡傛灉 nearCodeLine 瓒呭嚭鑼冨洿锛岃拷鍔犲埌鏈熬
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::End);
    QString tail = doc->toPlainText();
    if (!tail.isEmpty() && !tail.endsWith("\n")) {
        cursor.insertText("\n");
    }
    cursor.insertText(markerLine + "\n");
    return findDslMappingMarkerLine(mappingId);
}

void DslScriptEditor::gotoMappingId(const QString& mappingId)
{
    if (mappingId.isEmpty() || !m_editor) {
        return;
    }

    int markerLine = findDslMappingMarkerLine(mappingId);
    if (markerLine > 0) {
        // 榛樿璺冲埌 marker 涓嬩竴琛岋紙閫氬父鏄?snippet 浠ｇ爜鎵€鍦ㄨ锛?        gotoLine(markerLine + 1);
        return;
    }

    QString needle = QString("@dsl_mapping_id: %1").arg(mappingId);
    QTextCursor cursor = m_editor->document()->find(needle);
    if (cursor.isNull()) {
        return;
    }
    m_editor->setTextCursor(cursor);
    m_editor->centerCursor();
    m_editor->setFocus();
}

void DslScriptEditor::setDslMappings(const QList<DslMappingEntry>& mappings)
{
    m_dslMappings = mappings;

    if (!m_editor) {
        return;
    }

    QMap<QString, int> markerMap = scanDslMappingMarkers();

    QList<QTextEdit::ExtraSelection> selections;
    selections.reserve(m_dslMappings.size());

    for (const auto& m : m_dslMappings) {
        int markerLine = markerMap.value(m.id, -1);
        int lineToHighlight = markerLine > 0 ? markerLine : m.lineNumber;
        if (lineToHighlight < 1) {
            continue;
        }

        QTextBlock block = m_editor->document()->findBlockByLineNumber(lineToHighlight - 1);
        if (!block.isValid()) {
            continue;
        }

        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(QColor("#e8f3ff"));
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = QTextCursor(block);
        sel.cursor.clearSelection();
        selections.append(sel);
    }

    m_editor->setExternalSelections(selections);
}

void DslScriptEditor::setFunctionListVisible(bool visible)
{
    if (!m_splitter || !m_functionList) {
        return;
    }

    if (visible) {
        m_functionList->show();
        if (!m_savedSplitterSizes.isEmpty()) {
            m_splitter->setSizes(m_savedSplitterSizes);
        } else {
            m_splitter->setSizes({220, 600});
        }
    } else {
        m_savedSplitterSizes = m_splitter->sizes();
        m_functionList->hide();
    }
}

bool DslScriptEditor::isFunctionListVisible() const
{
    return m_functionList && m_functionList->isVisible();
}

void DslScriptEditor::setModified(bool modified)
{
    if (m_editor && m_editor->document()) {
        m_editor->document()->setModified(modified);
    }
}

bool DslScriptEditor::isModified() const
{
    if (m_editor && m_editor->document()) {
        return m_editor->document()->isModified();
    }
    return false;
}

QString DslScriptEditor::currentScript() const
{
    return m_editor ? m_editor->toPlainText() : QString();
}

void DslScriptEditor::setScript(const QString& text)
{
    if (m_editor) {
        m_editor->setPlainText(text);
    }
}

void DslScriptEditor::clearScript()
{
    if (m_editor) {
        m_editor->clear();
    }
    m_insertRecords.clear();
}

QList<FunctionSnippet> DslScriptEditor::availableSnippets() const
{
    return m_completionEngine ? m_completionEngine->availableSnippets() : QList<FunctionSnippet>();
}

FunctionSnippet DslScriptEditor::snippetById(const QString& id) const
{
    return m_completionEngine ? m_completionEngine->snippetById(id) : FunctionSnippet();
}

void DslScriptEditor::setStatusCallback(std::function<void(const QString&)> callback)
{
    m_statusCallback = callback;
}

void DslScriptEditor::onEditorModificationChanged(bool modified)
{
    emit editorModified(modified);
}

void DslScriptEditor::onEditorCursorPositionChanged()
{
    if (!m_editor) {
        return;
    }

    QTextCursor cursor = m_editor->textCursor();
    int line = cursor.blockNumber() + 1;
    int column = cursor.positionInBlock() + 1;
    int totalLines = m_editor->document()->blockCount();

    emit cursorPositionChanged(line, column, totalLines);
}

void DslScriptEditor::onFunctionItemActivated(QListWidgetItem* item)
{
    if (!item) return;

    if (!m_editor) {
        return;
    }

    QString snippetId = item->data(Qt::UserRole).toString();
    QString snippetCode = item->data(Qt::UserRole + 1).toString();

    if (snippetCode.isEmpty()) {
        snippetCode = item->text();
    }

    const QString mappingId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QTextCursor cursor = m_editor->textCursor();

    // 纭繚鍦ㄦ柊琛屾彃鍏?marker + snippet
    if (!cursor.atBlockStart()) {
        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.insertText("\n");
    }

    const int markerLine = cursor.blockNumber() + 1;
    cursor.insertText(QString("// @dsl_mapping_id: %1\n").arg(mappingId));
    m_editor->setTextCursor(cursor);

    // 鎻掑叆 snippet 浠ｇ爜
    insertSnippet(snippetCode);

    // 璁板綍鎻掑叆锛坙ineNumber 璁板綍浠ｇ爜琛岋紝鑰岄潪 marker 琛岋級
    DslInsertRecord record;
    record.snippetId = snippetId;
    record.snippetName = item->text();
    record.mappingId = mappingId;
    record.lineNumber = markerLine + 1;
    record.generatedCode = snippetCode;
    record.insertTime = QDateTime::currentDateTime();
    m_insertRecords.append(record);

    emit snippetInserted(record);
}

void DslScriptEditor::insertSnippet(const QString& text)
{
    if (!m_editor || text.isEmpty()) {
        return;
    }
    
    QTextCursor cursor = m_editor->textCursor();
    
    if (!cursor.atBlockStart()) {
        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.insertText("\n");
    }
    
    cursor.insertText(text);
    cursor.insertText("\n");
    
    m_editor->setTextCursor(cursor);
    m_editor->setFocus();
}

// ===== 鎷栨嫿鐩稿叧妲藉嚱鏁?=====

void DslScriptEditor::onDropSucceeded(const DragDropResult& result)
{
    const QString mappingId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const int markerLine = ensureDslMappingMarker(mappingId, result.lineNumber);

    // 璁板綍鎻掑叆锛坙ineNumber 璁板綍浠ｇ爜琛岋紝鑰岄潪 marker 琛岋級
    DslInsertRecord record;
    record.snippetId = result.snippetId;
    record.snippetName = result.snippetName;
    record.mappingId = mappingId;
    record.lineNumber = (markerLine > 0) ? (markerLine + 1) : result.lineNumber;
    record.generatedCode = result.insertedCode;
    record.insertTime = QDateTime::currentDateTime();
    m_insertRecords.append(record);

    emit snippetInserted(record);

    if (m_statusCallback) {
        m_statusCallback(QString("已插入组件: %1 (行 %2)")
                         .arg(result.snippetName.isEmpty() ? result.snippetId : result.snippetName)
                         .arg(record.lineNumber));
    }
}

void DslScriptEditor::onDropRejected(const QString& reason)
{
    emit dropError(reason);
    
    if (m_statusCallback) {
        m_statusCallback(QString("拖拽失败: %1").arg(reason));
    }
    
    QMessageBox::warning(this, "拖拽失败", reason);
}

void DslScriptEditor::onDragStarted(const QString& snippetId)
{
    if (m_statusCallback) {
        FunctionSnippet snippet = snippetById(snippetId);
        m_statusCallback(QString("正在拖拽: %1 - 释放到编辑器中插入代码")
                         .arg(snippet.name.isEmpty() ? snippetId : snippet.name));
    }
}

void DslScriptEditor::onDragEnded()
{
    if (m_statusCallback) {
        m_statusCallback("就绪");
    }
}

void DslScriptEditor::onHighlightUpdateRequested(int lineNumber, bool highlight)
{
    if (m_editor) {
        m_editor->setDragTargetLine(lineNumber, highlight);
    }
}

// ===== 鏌ユ壘/鏇挎崲瀹炵幇 =====

void DslScriptEditor::onFindNext()
{
    QString text = m_findEdit->text();
    if (text.isEmpty()) {
        return;
    }

    bool caseSensitive = m_caseSensitiveCheck->isChecked();
    if (!findText(text, true, caseSensitive)) {
        QTextCursor cursor = m_editor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        m_editor->setTextCursor(cursor);

        if (!findText(text, true, caseSensitive)) {
            m_findEdit->setStyleSheet("QLineEdit { background-color: #ffcccc; }");
        }
    }
}

void DslScriptEditor::onFindPrevious()
{
    QString text = m_findEdit->text();
    if (text.isEmpty()) {
        return;
    }

    bool caseSensitive = m_caseSensitiveCheck->isChecked();
    if (!findText(text, false, caseSensitive)) {
        QTextCursor cursor = m_editor->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_editor->setTextCursor(cursor);

        if (!findText(text, false, caseSensitive)) {
            m_findEdit->setStyleSheet("QLineEdit { background-color: #ffcccc; }");
        }
    }
}

void DslScriptEditor::onReplace()
{
    if (!m_editor->textCursor().hasSelection()) {
        onFindNext();
        return;
    }
    
    QString findStr = m_findEdit->text();
    QString replaceText = m_replaceEdit->text();
    
    QTextCursor cursor = m_editor->textCursor();
    if (cursor.selectedText().compare(findStr, 
            m_caseSensitiveCheck->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive) == 0) {
        cursor.insertText(replaceText);
        onFindNext();
    }
}

void DslScriptEditor::onReplaceAll()
{
    QString findStr = m_findEdit->text();
    QString replaceStr = m_replaceEdit->text();
    
    if (findStr.isEmpty()) {
        return;
    }
    
    QTextDocument* doc = m_editor->document();
    QTextCursor cursor(doc);
    
    cursor.beginEditBlock();
    
    int count = 0;
    QTextDocument::FindFlags flags;
    if (m_caseSensitiveCheck->isChecked()) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    
    cursor.movePosition(QTextCursor::Start);
    m_editor->setTextCursor(cursor);
    
    while (true) {
        cursor = doc->find(findStr, cursor, flags);
        if (cursor.isNull()) {
            break;
        }
        cursor.insertText(replaceStr);
        ++count;
    }
    
    cursor.endEditBlock();
    
    if (count > 0) {
        if (m_statusCallback) {
            m_statusCallback(QString("已替换 %1 处").arg(count));
        }
    }
}

void DslScriptEditor::onFindTextChanged(const QString& text)
{
    m_findEdit->setStyleSheet("");
    
    if (!text.isEmpty()) {
        highlightAllMatches(text, m_caseSensitiveCheck->isChecked());
    } else {
        clearFindHighlights();
    }
}

bool DslScriptEditor::findText(const QString& text, bool forward, bool caseSensitive)
{
    QTextDocument::FindFlags flags;
    if (!forward) {
        flags |= QTextDocument::FindBackward;
    }
    if (caseSensitive) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    
    bool found = m_editor->find(text, flags);
    
    if (found) {
        m_findEdit->setStyleSheet("");
    }
    
    return found;
}

void DslScriptEditor::clearFindHighlights()
{
    m_findHighlights.clear();
    m_editor->updateCurrentLineHighlight();
}

void DslScriptEditor::highlightAllMatches(const QString& text, bool caseSensitive)
{
    m_findHighlights.clear();
    
    if (text.isEmpty()) {
        m_editor->updateCurrentLineHighlight();
        return;
    }
    
    QTextDocument* doc = m_editor->document();
    QTextCursor cursor(doc);
    
    QTextDocument::FindFlags flags;
    if (caseSensitive) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    
    QTextCharFormat highlightFormat;
    highlightFormat.setBackground(QColor(255, 242, 0, 110));
    
    while (true) {
        cursor = doc->find(text, cursor, flags);
        if (cursor.isNull()) {
            break;
        }
        
        QTextEdit::ExtraSelection selection;
        selection.format = highlightFormat;
        selection.cursor = cursor;
        m_findHighlights.append(selection);
    }
}

// ===== 鑷姩琛ュ叏鐩稿叧 =====


void DslScriptEditor::onShowDslMappings()
{
    if (!m_editor) {
        return;
    }

    // 濡傛灉娌℃湁 mappings锛屽垯鑷冲皯鎻愮ず鐢ㄦ埛
    QDialog dlg(this);
    dlg.setWindowTitle("DSL 映射导航");
    dlg.resize(520, 360);

    QVBoxLayout* layout = new QVBoxLayout(&dlg);

    QLabel* hint = new QLabel(
        "选择一条映射后跳转到对应位置（优先 marker 下一行）。\n"
        "提示: 插入 snippet 时会自动写入 // @dsl_mapping_id: <uuid> 作为稳定锚点。",
        &dlg
    );
    hint->setWordWrap(true);
    layout->addWidget(hint);

    QListWidget* list = new QListWidget(&dlg);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(list, 1);

    auto markerMap = scanDslMappingMarkers();

    for (const auto& m : m_dslMappings) {
        const int markerLine = markerMap.value(m.id, -1);
        const int codeLine = (markerLine > 0) ? (markerLine + 1) : m.lineNumber;

        QString title = m.snippetName.isEmpty() ? m.snippetId : m.snippetName;
        QString subtitle = m.channelName.isEmpty() ? QString() : QString("  [%1]").arg(m.channelName);
        QString text = QString("%1%2  (行 %3)").arg(title).arg(subtitle).arg(codeLine > 0 ? codeLine : -1);

        QListWidgetItem* item = new QListWidgetItem(text, list);
        item->setData(Qt::UserRole, m.id);
    }

    connect(list, &QListWidget::itemDoubleClicked, &dlg, [&](QListWidgetItem* item) {
        if (!item) return;
        const QString id = item->data(Qt::UserRole).toString();
        dlg.accept();
        gotoMappingId(id);
    });

    // 娌℃湁 mapping 鏃剁粰涓€涓┖鎻愮ず
    if (m_dslMappings.isEmpty()) {
        QListWidgetItem* item = new QListWidgetItem("（暂无映射）请先从左侧函数列表插入 snippet。", list);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    }

    dlg.exec();
}

void DslScriptEditor::insertCompletion(const QString& completion)
{
    if (!m_editor || !m_completionEngine) {
        return;
    }
    
    QCompleter* completer = m_completionEngine->completer();
    if (!completer) {
        return;
    }
    
    // 1) 濡傛灉琛ュ叏椤瑰搴斾竴涓?Snippet锛屽垯鎻掑叆鏁翠釜妯℃澘浠ｇ爜锛堣€屼笉浠呮槸鍚嶇О锛?    // 2) 鍚﹀垯鍥為€€鍒扳€滆ˉ鍏ㄥ崟璇嶁€濈殑鏃ч€昏緫

    const FunctionSnippet snippet = m_completionEngine->snippetByName(completion);
    if (snippet.isValid() && !snippet.templateCode.isEmpty()) {
        QTextCursor cursor = m_editor->textCursor();
        const int blockPos = cursor.block().position();
        const QString blockText = cursor.block().text();
        const int col = cursor.positionInBlock();

        int start = col;
        while (start > 0 && isIdentifierChar(blockText[start - 1])) {
            --start;
        }
        int end = col;
        while (end < blockText.size() && isIdentifierChar(blockText[end])) {
            ++end;
        }

        // 鍙栧綋鍓嶈鐨勫熀纭€缂╄繘锛堣棣栫┖鐧斤級锛岀敤浜庡琛屾ā鏉跨殑瀵归綈
        int indentLen = 0;
        while (indentLen < blockText.size() && (blockText[indentLen] == ' ' || blockText[indentLen] == '\t')) {
            ++indentLen;
        }
        const QString indentation = blockText.left(indentLen);
        const QString insertText = m_completionEngine->generateInsertCode(snippet, indentation);

        cursor.beginEditBlock();
        cursor.setPosition(blockPos + start);
        cursor.setPosition(blockPos + end, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(insertText);
        cursor.endEditBlock();

        m_editor->setTextCursor(cursor);
        m_editor->setFocus();
        return;
    }

    QTextCursor cursor = m_editor->textCursor();
    int extra = completion.length() - completer->completionPrefix().length();
    cursor.movePosition(QTextCursor::Left);
    cursor.movePosition(QTextCursor::EndOfWord);
    cursor.insertText(completion.right(extra));
    m_editor->setTextCursor(cursor);
}

bool DslScriptEditor::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_editor && event->type() == QEvent::KeyPress && m_completionEngine) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        QCompleter* completer = m_completionEngine->completer();
        
        if (completer && completer->popup()->isVisible()) {
            switch (keyEvent->key()) {
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Escape:
            case Qt::Key_Tab:
            case Qt::Key_Backtab:
                event->ignore();
                return false;
            default:
                break;
            }
        }
        
        bool isShortcut = (keyEvent->modifiers() & Qt::ControlModifier) && 
                          keyEvent->key() == Qt::Key_Space;
        
        if (isShortcut) {
            updateCompletionPrefix(true);
            return true;
        }
        
        QTimer::singleShot(0, this, [this]() {
            updateCompletionPrefix(false);
        });
    }
    
    return QWidget::eventFilter(obj, event);
}

void DslScriptEditor::updateCompletionPrefix(bool forceShow)
{
    if (!m_completionEngine) {
        return;
    }
    
    QCompleter* completer = m_completionEngine->completer();
    if (!completer) {
        return;
    }
    
    QString prefix = currentWordPrefix();
    
    if (prefix.isEmpty() && !forceShow) {
        completer->popup()->hide();
        return;
    }
    
    completer->setCompletionPrefix(prefix);
    
    if (completer->completionCount() == 0) {
        completer->popup()->hide();
        return;
    }
    
    if (forceShow || prefix.length() >= 2) {
        QRect cr = m_editor->cursorRect();
        cr.setWidth(completer->popup()->sizeHintForColumn(0) + 
                    completer->popup()->verticalScrollBar()->sizeHint().width());
        completer->complete(cr);
    }
}

QString DslScriptEditor::currentWordPrefix(int* outBlockPos, int* outCursorColumn) const
{
    QTextCursor cursor = m_editor->textCursor();
    QString block = cursor.block().text();
    int col = cursor.positionInBlock();
    
    int start = col;
    while (start > 0 && isIdentifierChar(block[start - 1])) {
        --start;
    }
    
    if (outBlockPos) *outBlockPos = cursor.block().position();
    if (outCursorColumn) *outCursorColumn = col;
    
    return block.mid(start, col - start);
}

bool DslScriptEditor::isIdentifierChar(const QChar& ch)
{
    return ch.isLetterOrNumber() || ch == '_';
}
