#include "ProblemsPanel.h"
#include "DiagnosticParser.h"

#include <QAbstractItemView>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QSize>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QTemporaryFile>
#include <QDesktopServices>
#include <QUrl>

ProblemsPanel::ProblemsPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ProblemsPanel"));
    setStyleSheet(R"(
QWidget#ProblemsPanel {
    background: #ffffff;
}
QLabel#ProblemsTitle {
    color: #24292f;
    font-size: 12px;
    font-weight: 600;
}
QToolButton {
    color: #24292f;
    background: #ffffff;
    border: 1px solid #d0d7de;
    border-radius: 4px;
    padding: 4px 8px;
}
QToolButton:hover {
    background: #eaeef2;
    border-color: #afb8c1;
}
QTableWidget {
    gridline-color: #d8dee4;
    selection-background-color: #cce7ff;
    selection-color: #1f1f1f;
    alternate-background-color: #f6f8fa;
}
QHeaderView::section {
    background: #f3f3f3;
    color: #57606a;
    border: 0;
    border-right: 1px solid #d0d7de;
    border-bottom: 1px solid #d0d7de;
    padding: 5px 8px;
    font-weight: 600;
}
)");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 8);
    root->setSpacing(6);

    auto* tools = new QHBoxLayout;
    tools->setContentsMargins(0, 0, 0, 0);
    auto* title = new QLabel(QStringLiteral("问题"), this);
    title->setObjectName(QStringLiteral("ProblemsTitle"));
    m_summaryLabel = new QLabel(QStringLiteral("诊断摘要：错误 0，警告 0，信息 0"), this);
    m_summaryLabel->setObjectName(QStringLiteral("ProblemsSummary"));
    m_summaryLabel->setStyleSheet("QLabel#ProblemsSummary { color: #57606a; }");
    m_clearButton = new QToolButton(this);
    m_clearButton->setText(QStringLiteral("清空"));
    m_clearButton->setIcon(QIcon(":/icons/clear.svg"));
    m_clearButton->setIconSize(QSize(16, 16));
    m_clearButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_clearButton->setToolTip(QStringLiteral("清空所有问题"));
    connect(m_clearButton, &QToolButton::clicked, this, &ProblemsPanel::clearProblems);
    tools->addWidget(title);
    tools->addSpacing(12);
    tools->addWidget(m_summaryLabel);
    tools->addStretch();
    auto* detailsButton = new QToolButton(this);
    detailsButton->setText(QStringLiteral("完整详情"));
    connect(detailsButton, &QToolButton::clicked, this, [this] {
        if (m_details && m_details->isOpen()) QDesktopServices::openUrl(QUrl::fromLocalFile(m_details->fileName()));
    });
    tools->addWidget(detailsButton);
    tools->addWidget(m_clearButton);
    root->addLayout(tools);

    m_table = new QTableWidget(this);
    setupTableHeaders();

    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(28);
    m_table->horizontalHeader()->setHighlightSections(false);

    m_table->installEventFilter(this);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int /*column*/) {
        onRowActivated(row);
    });

    root->addWidget(m_table);
}

void ProblemsPanel::setupTableHeaders()
{
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels(QStringList()
                                       << QStringLiteral("时间")
                                       << QStringLiteral("级别")
                                       << QStringLiteral("来源")
                                       << QStringLiteral("位置")
                                       << QStringLiteral("说明"));
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->horizontalHeader()->setMinimumSectionSize(64);
    m_table->setColumnWidth(2, 110);
    m_table->setColumnWidth(3, 140);
}

void ProblemsPanel::addProblem(const QString& severity, const QString& source, const QString& message)
{
    DiagnosticItem item = DiagnosticParser::parseSingleMessage(severity, source, message);
    addStructuredProblem(item);
}

void ProblemsPanel::addStructuredProblem(const DiagnosticItem& item)
{
    if (!m_table) {
        return;
    }
    appendRowForDiagnostic(item);
    updateSummaryLabels();
    emit problemCountChanged(m_table->rowCount());
}

void ProblemsPanel::replaceBuildDiagnostics(const QList<DiagnosticItem>& items)
{
    QList<DiagnosticItem> merged;
    for (int row = 0; row < problemCount(); ++row) {
        const auto item = itemAtRow(row);
        if (item.source != QStringLiteral("构建")) merged.append(item);
    }
    for (auto item : items) {
        item.source = QStringLiteral("构建");
        merged.append(item);
    }
    setDiagnostics(merged);
}

void ProblemsPanel::setDiagnostics(const QList<DiagnosticItem>& items)
{
    clearProblems();
    if (!m_table) {
        return;
    }

    m_table->setUpdatesEnabled(false);
    for (const DiagnosticItem& item : items) {
        appendRowForDiagnostic(item);
    }
    m_table->setUpdatesEnabled(true);

    updateSummaryLabels();
    emit problemCountChanged(m_table->rowCount());
}

QString ProblemsPanel::diagnosticDetailsPath() const
{
    return m_details ? m_details->fileName() : QString();
}

void ProblemsPanel::removeDiagnosticRow(int row)
{
    const QString severity = itemAtRow(row).severity.toLower();
    if (severity == "error" || severity == "错误") --m_errorCount;
    else if (severity == "warning" || severity == "警告") --m_warningCount;
    else --m_infoCount;
    m_table->removeRow(row);
}

void ProblemsPanel::appendRowForDiagnostic(const DiagnosticItem& input)
{
    DiagnosticItem item = input;
    int sourceCount = 0;
    for (int r = 0; r < m_table->rowCount(); ++r)
        if (itemAtRow(r).source == item.source) ++sourceCount;
    for (int r = 0; sourceCount >= MaxRowsPerSource && r < m_table->rowCount();) {
        if (itemAtRow(r).source == item.source) { removeDiagnosticRow(r); --sourceCount; }
        else ++r;
    }
    while (m_table->rowCount() >= MaxRows) removeDiagnosticRow(0);
    QString detailsPath;
    if (item.message.size() > MaxMessageCharacters) {
        if (!m_details) {
            m_details = new QTemporaryFile(this);
            m_details->open();
        }
        if (m_details->isOpen()) {
            const QByteArray detail = item.message.toUtf8();
            if (detail.size() + 2 <= MaxDetailsBytes - m_details->size()) {
                m_details->write(detail);
                m_details->write("\n\n");
                m_details->flush();
                detailsPath = m_details->fileName();
            }
        }
        item.message = item.message.left(MaxMessageCharacters);
        if (!item.message.isEmpty() && item.message.back().isHighSurrogate()) item.message.chop(1);
        item.message += detailsPath.isEmpty()
                ? QStringLiteral(" [已截断；详情未保留，存储失败或达到 16 MiB 上限]")
                : QStringLiteral(" [已截断；完整内容见诊断详情]");
    }
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    const QString timeStr = item.timestamp.isValid()
        ? item.timestamp.toString("HH:mm:ss")
        : QDateTime::currentDateTime().toString("HH:mm:ss");

    auto* timeItem = new QTableWidgetItem(timeStr);
    timeItem->setData(Qt::UserRole, QVariant::fromValue(item));
    timeItem->setForeground(QColor("#57606a"));

    auto* severityItem = new QTableWidgetItem;
    const QString sevLower = item.severity.toLower();
    if (sevLower == "error" || sevLower == "错误") {
        severityItem->setText(QStringLiteral("错误"));
        severityItem->setForeground(QColor("#cf222e"));
        severityItem->setBackground(QColor("#ffebe9"));
        ++m_errorCount;
    } else if (sevLower == "warning" || sevLower == "警告") {
        severityItem->setText(QStringLiteral("警告"));
        severityItem->setForeground(QColor("#7d4e00"));
        severityItem->setBackground(QColor("#fff8c5"));
        ++m_warningCount;
    } else {
        severityItem->setText(item.severity.isEmpty() ? QStringLiteral("信息") : item.severity);
        severityItem->setForeground(QColor("#0969da"));
        severityItem->setBackground(QColor("#ddf4ff"));
        ++m_infoCount;
    }

    auto* sourceItem = new QTableWidgetItem(item.source);
    sourceItem->setForeground(QColor("#57606a"));

    // 位置列 (Location)
    auto* locationItem = new QTableWidgetItem;
    if (item.hasLocation()) {
        QString loc = QFileInfo(item.filePath).fileName() + ":" + QString::number(item.line);
        if (item.column > 0) {
            loc += ":" + QString::number(item.column);
        }
        locationItem->setText(loc);
        locationItem->setToolTip(item.filePath);
        locationItem->setForeground(QColor("#0969da"));
    } else {
        locationItem->setText(QStringLiteral("--"));
        locationItem->setForeground(QColor("#8c8c8c"));
    }

    // 说明列 (Message)
    auto* messageItem = new QTableWidgetItem;
    if (!detailsPath.isEmpty()) {
        messageItem->setToolTip(QStringLiteral("完整诊断: %1").arg(detailsPath));
        messageItem->setData(Qt::UserRole, detailsPath);
    }
    if (item.isOutdated) {
        messageItem->setText(QStringLiteral("[可能已过期] ") + item.message);
        messageItem->setForeground(QColor("#8c8c8c"));
    } else {
        messageItem->setText(item.message);
    }

    m_table->setItem(row, 0, timeItem);
    m_table->setItem(row, 1, severityItem);
    m_table->setItem(row, 2, sourceItem);
    m_table->setItem(row, 3, locationItem);
    m_table->setItem(row, 4, messageItem);
}

void ProblemsPanel::markDiagnosticsOutdatedForFile(const QString& filePath)
{
    if (!m_table || filePath.trimmed().isEmpty()) {
        return;
    }

    const QString targetCanonical = QFileInfo(filePath.trimmed()).canonicalFilePath();
    const QString targetClean = QDir::cleanPath(filePath.trimmed());

    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto* timeItem = m_table->item(row, 0);
        if (!timeItem) {
            continue;
        }

        DiagnosticItem item = timeItem->data(Qt::UserRole).value<DiagnosticItem>();
        if (item.filePath.isEmpty()) {
            continue;
        }

        const QString itemCanonical = QFileInfo(item.filePath).canonicalFilePath();
        const QString itemClean = QDir::cleanPath(item.filePath);

        const bool match = (!targetCanonical.isEmpty() && !itemCanonical.isEmpty() && targetCanonical == itemCanonical)
                           || (targetClean.compare(itemClean, Qt::CaseInsensitive) == 0);

        if (match) {
            item.isOutdated = true;
            timeItem->setData(Qt::UserRole, QVariant::fromValue(item));

            auto* msgItem = m_table->item(row, 4);
            if (msgItem) {
                if (!msgItem->text().startsWith(QStringLiteral("[可能已过期]"))) {
                    msgItem->setText(QStringLiteral("[可能已过期] ") + msgItem->text());
                }
                msgItem->setForeground(QColor("#8c8c8c"));
            }
        }
    }
}

DiagnosticItem ProblemsPanel::itemAtRow(int row) const
{
    if (!m_table || row < 0 || row >= m_table->rowCount()) {
        return DiagnosticItem();
    }
    auto* timeItem = m_table->item(row, 0);
    if (!timeItem) {
        return DiagnosticItem();
    }
    return timeItem->data(Qt::UserRole).value<DiagnosticItem>();
}

void ProblemsPanel::onRowActivated(int row)
{
    DiagnosticItem item = itemAtRow(row);
    if (!item.message.isEmpty() || !item.filePath.isEmpty()) {
        emit diagnosticActivated(item);
    }
}

bool ProblemsPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_table && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            onRowActivated(m_table->currentRow());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ProblemsPanel::clearProblems()
{
    delete m_details;
    m_details = nullptr;
    if (!m_table) {
        return;
    }

    m_table->setRowCount(0);
    m_errorCount = 0;
    m_warningCount = 0;
    m_infoCount = 0;
    updateSummaryLabels();
    emit problemCountChanged(0);
}

void ProblemsPanel::selectFirstError()
{
    if (!m_table) {
        return;
    }
    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto* item = m_table->item(row, 1);
        if (item && item->text() == QStringLiteral("错误")) {
            m_table->selectRow(row);
            m_table->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            return;
        }
    }
    if (m_table->rowCount() > 0) {
        m_table->selectRow(0);
        auto* item = m_table->item(0, 0);
        if (item) {
            m_table->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        }
    }
}

int ProblemsPanel::problemCount() const
{
    return m_table ? m_table->rowCount() : 0;
}

void ProblemsPanel::setDiagnosticSummary(const QString& summary)
{
    if (m_summaryLabel) {
        m_summaryLabel->setText(summary);
    }
}

void ProblemsPanel::updateSummaryLabels()
{
    if (!m_summaryLabel) {
        return;
    }

    m_summaryLabel->setText(QStringLiteral("诊断摘要：错误 %1，警告 %2，信息 %3")
                            .arg(m_errorCount)
                            .arg(m_warningCount)
                            .arg(m_infoCount));
}
