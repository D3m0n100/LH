/**
 * @file DslSyntaxHighlighter.cpp
 * @brief DSL 语法高亮器实现
 */

#include "DslSyntaxHighlighter.h"

DslSyntaxHighlighter::DslSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    setupHighlightingRules();
}

void DslSyntaxHighlighter::setupHighlightingRules()
{
    // ===== 关键字格式（DSL 组件函数）====
    m_componentFormat.setForeground(QColor("#007acc"));
    
    QStringList componentPatterns = {
        "\\bdigital_input\\b",
        "\\bdigital_output\\b",
        "\\banalog_input\\b",
        "\\banalog_output\\b",
        "\\bpid_controller\\b",
        "\\bmonitor\\b",
        "\\btimer\\b",
        "\\bcounter\\b",
        "\\bsequence\\b",
        "\\balarm\\b"
    };
    
    for (const QString& pattern : componentPatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_componentFormat;
        m_highlightingRules.append(rule);
    }
    
    // ===== 类型格式 =====
    m_typeFormat.setForeground(QColor("#8661c5"));
    m_typeFormat.setFontWeight(QFont::Bold);
    
    QStringList typePatterns = {
        "\\bint\\b",
        "\\bfloat\\b",
        "\\bdouble\\b",
        "\\bbool\\b",
        "\\bstring\\b",
        "\\bvoid\\b",
        "\\btrue\\b",
        "\\bfalse\\b"
    };
    
    for (const QString& pattern : typePatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_typeFormat;
        m_highlightingRules.append(rule);
    }
    
    // ===== 关键字格式（控制流）=====
    m_keywordFormat.setForeground(QColor("#af00db"));
    
    QStringList keywordPatterns = {
        "\\bif\\b",
        "\\belse\\b",
        "\\bwhile\\b",
        "\\bfor\\b",
        "\\breturn\\b",
        "\\bbreak\\b",
        "\\bcontinue\\b",
        "\\bswitch\\b",
        "\\bcase\\b",
        "\\bdefault\\b"
    };
    
    for (const QString& pattern : keywordPatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_keywordFormat;
        m_highlightingRules.append(rule);
    }
    
    // ===== 参数名格式（name = value 中的 name）=====
    m_parameterFormat.setForeground(QColor("#5f6a72"));
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*=");
        rule.format = m_parameterFormat;
        m_highlightingRules.append(rule);
    }
    
    // ===== 鏁板瓧鏍煎紡 =====
    m_numberFormat.setForeground(QColor("#098658"));
    {
        HighlightingRule rule;
        // 匹配整数和浮点数
        rule.pattern = QRegularExpression("\\b[0-9]+\\.?[0-9]*([eE][+-]?[0-9]+)?\\b");
        rule.format = m_numberFormat;
        m_highlightingRules.append(rule);
    }
    
    // ===== 运算符格式 =====
    m_operatorFormat.setForeground(QColor("#1f1f1f"));
    m_operatorFormat.setFontWeight(QFont::Bold);
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("[=+\\-*/<>!&|^~%]+");
        rule.format = m_operatorFormat;
        m_highlightingRules.append(rule);
    }
    
    // ===== 字符串格式 =====
    m_stringFormat.setForeground(QColor("#a31515"));
    {
        HighlightingRule rule;
        // 双引号字符串
        rule.pattern = QRegularExpression("\"[^\"]*\"");
        rule.format = m_stringFormat;
        m_highlightingRules.append(rule);
    }
    {
        HighlightingRule rule;
        // 单引号字符串
        rule.pattern = QRegularExpression("'[^']*'");
        rule.format = m_stringFormat;
        m_highlightingRules.append(rule);
    }
    
    // ===== 函数调用格式 =====
    m_functionFormat.setForeground(QColor("#795e26"));
    {
        HighlightingRule rule;
        // 匹配函数调用 name(
        rule.pattern = QRegularExpression("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(");
        rule.format = m_functionFormat;
        m_highlightingRules.append(rule);
    }
    
    // ===== 单行注释格式 =====
    m_singleLineCommentFormat.setForeground(QColor("#008000"));
    m_singleLineCommentFormat.setFontItalic(true);
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("//[^\n]*");
        rule.format = m_singleLineCommentFormat;
        m_highlightingRules.append(rule);
    }

    // 内部映射锚点仅用于编辑器定位，不希望在正文中显眼展示
    m_internalMarkerFormat.setForeground(QColor("#ffffff"));
    m_internalMarkerFormat.setBackground(QColor("#ffffff"));
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(R"(^\s*//\s*@dsl_mapping_id\s*:\s*[A-Za-z0-9\-]+\s*$)");
        rule.format = m_internalMarkerFormat;
        m_highlightingRules.append(rule);
    }
    
    // ===== 多行注释格式 =====
    m_multiLineCommentFormat.setForeground(QColor("#008000"));
    m_multiLineCommentFormat.setFontItalic(true);
    
    m_commentStartExpression = QRegularExpression("/\\*");
    m_commentEndExpression = QRegularExpression("\\*/");
}

void DslSyntaxHighlighter::updateKeywords(const QStringList& keywords)
{
    m_customKeywords = keywords;
    
    // 重新追加自定义关键字规则，保留前面固定数量的内置规则
    for (const QString& keyword : keywords) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("\\b" + QRegularExpression::escape(keyword) + "\\b");
        rule.format = m_componentFormat;
        m_highlightingRules.append(rule);
    }
    
    // 触发重新高亮
    rehighlight();
}

void DslSyntaxHighlighter::updateBuiltinFunctions(const QStringList& functions)
{
    for (const QString& func : functions) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("\\b" + QRegularExpression::escape(func) + "\\b");
        rule.format = m_functionFormat;
        m_highlightingRules.append(rule);
    }
    
    rehighlight();
}

void DslSyntaxHighlighter::setHighlightingEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        rehighlight();
    }
}

void DslSyntaxHighlighter::highlightBlock(const QString& text)
{
    if (!m_enabled) {
        return;
    }
    
    for (const HighlightingRule& rule : qAsConst(m_highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
    
    // 处理多行注释
    setCurrentBlockState(0);
    
    int startIndex = 0;
    if (previousBlockState() != 1) {
        startIndex = text.indexOf(m_commentStartExpression);
    }
    
    while (startIndex >= 0) {
        QRegularExpressionMatch endMatch = m_commentEndExpression.match(text, startIndex);
        int endIndex = endMatch.capturedStart();
        int commentLength;
        
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        }
        
        setFormat(startIndex, commentLength, m_multiLineCommentFormat);
        startIndex = text.indexOf(m_commentStartExpression, startIndex + commentLength);
    }
}
