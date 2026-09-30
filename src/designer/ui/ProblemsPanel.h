#ifndef PROBLEMS_PANEL_H
#define PROBLEMS_PANEL_H

#include "DiagnosticItem.h"

#include <QWidget>
#include <QList>

class QTableWidget;
class QToolButton;
class QLabel;
class QTemporaryFile;

class ProblemsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ProblemsPanel(QWidget* parent = nullptr);

    /// 添加通用非结构化/旧格式问题（内部自动解析为 DiagnosticItem）
    void addProblem(const QString& severity, const QString& source, const QString& message);

    /// 添加结构化诊断
    void addStructuredProblem(const DiagnosticItem& item);

    /// 全量重置设置诊断列表（通常来自单次编译）
    void replaceBuildDiagnostics(const QList<DiagnosticItem>& items);
    void setDiagnostics(const QList<DiagnosticItem>& items);

    /// 标记某个文件相关的诊断为“可能已过期”
    void markDiagnosticsOutdatedForFile(const QString& filePath);

    void clearProblems();
    void selectFirstError();
    int errorCount() const { return m_errorCount; }
    int warningCount() const { return m_warningCount; }
    int infoCount() const { return m_infoCount; }
    int problemCount() const;
    void setDiagnosticSummary(const QString& summary);
    QString diagnosticDetailsPath() const;
    static constexpr int MaxRows = 1000;
    static constexpr int MaxRowsPerSource = 250;
    static constexpr int MaxMessageCharacters = 4096;
    static constexpr qint64 MaxDetailsBytes = 16 * 1024 * 1024;

    /// 获取指定行所绑定的 DiagnosticItem（若行无效返回空条目）
    DiagnosticItem itemAtRow(int row) const;

signals:
    void problemCountChanged(int count);
    void diagnosticActivated(const DiagnosticItem& item);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onRowActivated(int row);

private:
    void setupTableHeaders();
    void updateSummaryLabels();
    void appendRowForDiagnostic(const DiagnosticItem& item);
    void removeDiagnosticRow(int row);

private:
    QToolButton* m_clearButton = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QTableWidget* m_table = nullptr;
    int m_errorCount = 0;
    int m_warningCount = 0;
    int m_infoCount = 0;
    QTemporaryFile* m_details = nullptr;
};

#endif // PROBLEMS_PANEL_H
