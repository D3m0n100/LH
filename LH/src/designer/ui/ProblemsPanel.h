#ifndef PROBLEMS_PANEL_H
#define PROBLEMS_PANEL_H

#include <QWidget>

class QTableWidget;
class QToolButton;
class QLabel;

class ProblemsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ProblemsPanel(QWidget* parent = nullptr);

    void addProblem(const QString& severity, const QString& source, const QString& message);
    void clearProblems();
    int problemCount() const;
    void setDiagnosticSummary(const QString& summary);

signals:
    void problemCountChanged(int count);

private:
    void updateSummaryLabels();

    QToolButton* m_clearButton = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QTableWidget* m_table = nullptr;
    int m_errorCount = 0;
    int m_warningCount = 0;
    int m_infoCount = 0;
};

#endif // PROBLEMS_PANEL_H
