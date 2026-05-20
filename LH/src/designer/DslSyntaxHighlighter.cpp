/**
 * @file DslSyntaxHighlighter.cpp
 * @brief DSL 璇硶楂樹寒鍣ㄥ疄鐜? */

#include "DslSyntaxHighlighter.h"

DslSyntaxHighlighter::DslSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    setupHighlightingRules();
}

void DslSyntaxHighlighter::setupHighlightingRules()
{
    // ===== 鍏抽敭瀛楁牸寮忥紙DSL 缁勪欢鍑芥暟锛?====
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
    
    // ===== 绫诲瀷鏍煎紡 =====
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
    
    // ===== 鍏抽敭瀛楁牸寮忥紙鎺у埗娴侊級=====
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
    
    // ===== 鍙傛暟鍚嶆牸寮忥紙name = value 涓殑 name锛?====
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
        // 鍖归厤鏁存暟鍜屾诞鐐规暟
        rule.pattern = QRegularExpression("\\b[0-9]+\\.?[0-9]*([eE][+-]?[0-9]+)?\\b");
        rule.format = m_numberFormat;
        m_highlightingRules.append(rule);
    }
    
    // ===== 杩愮畻绗︽牸寮?=====
    m_operatorFormat.setForeground(QColor("#1f1f1f"));
    m_operatorFormat.setFontWeight(QFont::Bold);
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("[=+\\-*/<>!&|^~%]+");
        rule.format = m_operatorFormat;
        m_highlightingRules.append(rule);
    }
    
    // ===== 瀛楃涓叉牸寮?=====
    m_stringFormat.setForeground(QColor("#a31515"));
    {
        HighlightingRule rule;
        // 鍙屽紩鍙峰瓧绗︿覆
        rule.pattern = QRegularExpression("\"[^\"]*\"");
        rule.format = m_stringFormat;
        m_highlightingRules.append(rule);
    }
    {
        HighlightingRule rule;
        // 鍗曞紩鍙峰瓧绗︿覆
        rule.pattern = QRegularExpression("'[^']*'");
        rule.format = m_stringFormat;
        m_highlightingRules.append(rule);
    }
    
    // ===== 鍑芥暟璋冪敤鏍煎紡 =====
    m_functionFormat.setForeground(QColor("#795e26"));
    {
        HighlightingRule rule;
        // 鍖归厤鍑芥暟璋冪敤 name(
        rule.pattern = QRegularExpression("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(");
        rule.format = m_functionFormat;
        m_highlightingRules.append(rule);
    }
    
    // ===== 鍗曡娉ㄩ噴鏍煎紡 =====
    m_singleLineCommentFormat.setForeground(QColor("#008000"));
    m_singleLineCommentFormat.setFontItalic(true);
    {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("//[^\n]*");
        rule.format = m_singleLineCommentFormat;
        m_highlightingRules.append(rule);
    }
    
    // ===== 澶氳娉ㄩ噴鏍煎紡 =====
    m_multiLineCommentFormat.setForeground(QColor("#008000"));
    m_multiLineCommentFormat.setFontItalic(true);
    
    m_commentStartExpression = QRegularExpression("/\\*");
    m_commentEndExpression = QRegularExpression("\\*/");
}

void DslSyntaxHighlighter::updateKeywords(const QStringList& keywords)
{
    m_customKeywords = keywords;
    
    // 绉婚櫎鏃х殑鑷畾涔夊叧閿瓧瑙勫垯锛堜繚鐣欏墠闈㈠浐瀹氭暟閲忕殑瑙勫垯锛?    // 閲嶆柊娣诲姞鑷畾涔夊叧閿瓧
    for (const QString& keyword : keywords) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("\\b" + QRegularExpression::escape(keyword) + "\\b");
        rule.format = m_componentFormat;
        m_highlightingRules.append(rule);
    }
    
    // 瑙﹀彂閲嶆柊楂樹寒
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
    
    // 澶勭悊澶氳娉ㄩ噴
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
