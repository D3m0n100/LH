#include "ProblemsPanel.h"

#include <QDateTime>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QSize>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

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
    auto* title = new QLabel("问题", this);
    title->setObjectName(QStringLiteral("ProblemsTitle"));
    m_summaryLabel = new QLabel("诊断摘要：错误 0，警告 0，信息 0", this);
    m_summaryLabel->setObjectName(QStringLiteral("ProblemsSummary"));
    m_summaryLabel->setStyleSheet("QLabel#ProblemsSummary { color: #57606a; }");
    m_clearButton = new QToolButton(this);
    m_clearButton->setText("清空");
    m_clearButton->setIcon(QIcon(":/icons/clear.svg"));
    m_clearButton->setIconSize(QSize(16, 16));
    m_clearButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_clearButton->setToolTip("清空所有问题");
    connect(m_clearButton, &QToolButton::clicked, this, &ProblemsPanel::clearProblems);
    tools->addWidget(title);
    tools->addSpacing(12);
    tools->addWidget(m_summaryLabel);
    tools->addStretch();
    tools->addWidget(m_clearButton);
    root->addLayout(tools);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(QStringList() << "时间" << "级别" << "来源" << "消息");
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->horizontalHeader()->setMinimumSectionSize(64);
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
    m_table->setColumnWidth(2, 140);
    root->addWidget(m_table);
}

void ProblemsPanel::addProblem(const QString& severity, const QString& source, const QString& message)
{
    if (!m_table) {
        return;
    }

    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto* timeItem = new QTableWidgetItem(QDateTime::currentDateTime().toString("HH:mm:ss"));
    auto* severityItem = new QTableWidgetItem(severity);
    auto* sourceItem = new QTableWidgetItem(source);
    auto* messageItem = new QTableWidgetItem(message);

    if (severity.compare("错误", Qt::CaseInsensitive) == 0 ||
        severity.compare("error", Qt::CaseInsensitive) == 0) {
        severityItem->setText("错误");
        severityItem->setForeground(QColor("#cf222e"));
        severityItem->setBackground(QColor("#ffebe9"));
    } else if (severity.compare("警告", Qt::CaseInsensitive) == 0 ||
               severity.compare("warning", Qt::CaseInsensitive) == 0) {
        severityItem->setText("警告");
        severityItem->setForeground(QColor("#7d4e00"));
        severityItem->setBackground(QColor("#fff8c5"));
    } else {
        severityItem->setText(severity.isEmpty() ? "信息" : severity);
        severityItem->setForeground(QColor("#0969da"));
        severityItem->setBackground(QColor("#ddf4ff"));
    }

    timeItem->setForeground(QColor("#57606a"));
    sourceItem->setForeground(QColor("#57606a"));

    m_table->setItem(row, 0, timeItem);
    m_table->setItem(row, 1, severityItem);
    m_table->setItem(row, 2, sourceItem);
    m_table->setItem(row, 3, messageItem);

    if (severityItem->text() == "错误") {
        ++m_errorCount;
    } else if (severityItem->text() == "警告") {
        ++m_warningCount;
    } else {
        ++m_infoCount;
    }
    updateSummaryLabels();

    emit problemCountChanged(m_table->rowCount());
}

void ProblemsPanel::clearProblems()
{
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

    m_summaryLabel->setText(
        QString("诊断摘要：错误 %1，警告 %2，信息 %3")
            .arg(m_errorCount)
            .arg(m_warningCount)
            .arg(m_infoCount));
}

