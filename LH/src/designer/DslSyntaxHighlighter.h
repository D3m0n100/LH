/**
 * @file DslSyntaxHighlighter.h
 * @brief DSL 语法高亮器
 *
 * ============== 职责说明 ==============
 *
 * DslSyntaxHighlighter 负责 DSL 脚本的语法高亮，包括：
 * - 关键字高亮（如 digital_input, analog_output, pid_controller 等）
 * - 类型高亮（如 int, float, bool 等）
 * - 注释高亮（单行 // 和多行）
 * - 字符串高亮
 * - 数字高亮
 * - 运算符高亮
 *
 * ============== 与 DslScriptEditor 的关系 ==============
 *
 * - DslSyntaxHighlighter 由 DslScriptEditor 创建并管理
 * - 通过 setDocument() 关联到 CodeEditor 的文档
 * - 可通过 updateKeywords() 动态更新关键字列表（如从 DSL 编译器导出）
 */

#ifndef DSLSYNTAXHIGHLIGHTER_H
#define DSLSYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>

/**
 * @brief DSL 语法高亮器
 *
 * 继承 QSyntaxHighlighter，为 DSL 脚本提供语法着色。
 */
class DslSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit DslSyntaxHighlighter(QTextDocument* parent = nullptr);
    ~DslSyntaxHighlighter() override = default;

    // ===== 动态更新接口 =====
    
    /// 更新 DSL 关键字列表（组件名称）
    void updateKeywords(const QStringList& keywords);
    
    /// 更新内置函数列表
    void updateBuiltinFunctions(const QStringList& functions);
    
    /// 设置是否启用高亮
    void setHighlightingEnabled(bool enabled);
    
    /// 获取高亮是否启用
    bool isHighlightingEnabled() const { return m_enabled; }

protected:
    /// 重写高亮方法
    void highlightBlock(const QString& text) override;

private:
    /// 初始化高亮规则
    void setupHighlightingRules();
    
    /// 高亮规则结构
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    
    /// 高亮规则列表
    QVector<HighlightingRule> m_highlightingRules;
    
    /// 多行注释相关
    QRegularExpression m_commentStartExpression;
    QRegularExpression m_commentEndExpression;
    
    /// 格式定义
    QTextCharFormat m_keywordFormat;        ///< 关键字格式
    QTextCharFormat m_componentFormat;      ///< 组件名格式
    QTextCharFormat m_typeFormat;           ///< 类型格式
    QTextCharFormat m_singleLineCommentFormat; ///< 单行注释格式
    QTextCharFormat m_multiLineCommentFormat;  ///< 多行注释格式
    QTextCharFormat m_stringFormat;         ///< 字符串格式
    QTextCharFormat m_numberFormat;         ///< 数字格式
    QTextCharFormat m_operatorFormat;       ///< 运算符格式
    QTextCharFormat m_functionFormat;       ///< 函数调用格式
    QTextCharFormat m_parameterFormat;      ///< 参数名格式
    
    /// 是否启用高亮
    bool m_enabled = true;
    
    /// 用户自定义关键字
    QStringList m_customKeywords;
};

#endif // DSLSYNTAXHIGHLIGHTER_H
