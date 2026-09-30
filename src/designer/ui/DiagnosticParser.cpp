#include "DiagnosticParser.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

QString DiagnosticParser::resolveFilePath(const QString& rawPath, const QString& projectRoot)
{
    const QString trimmed = rawPath.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    QFileInfo fi(trimmed);
    if (!fi.isAbsolute() && !projectRoot.isEmpty()) {
        fi.setFile(QDir(projectRoot).filePath(trimmed));
    }

    if (fi.exists()) {
        return fi.canonicalFilePath();
    }
    return QDir::cleanPath(fi.absoluteFilePath());
}

static QString normalizeSeverity(const QString& rawSeverity)
{
    const QString s = rawSeverity.trimmed().toLower();
    if (s.contains("error") || s.contains("错误") || s.contains("fatal")) {
        return QStringLiteral("error");
    }
    if (s.contains("warning") || s.contains("警告")) {
        return QStringLiteral("warning");
    }
    return QStringLiteral("info");
}

DiagnosticItem DiagnosticParser::parseSingleMessage(const QString& severity,
                                                     const QString& source,
                                                     const QString& rawMessage,
                                                     const QString& projectRoot,
                                                     const QString& defaultScriptPath)
{
    DiagnosticItem item;
    item.severity = normalizeSeverity(severity);
    item.source = source.isEmpty() ? QStringLiteral("系统") : source;
    item.timestamp = QDateTime::currentDateTime();
    item.message = rawMessage.trimmed();

    // 1. 尝试匹配 ANTLR 解析器格式: "解析错误 (第 12 行, 第 5 列): ..."
    // 注意: compiler.py 中 ANTLR 的 column 是 0-based 字符偏移
    static const QRegularExpression antlrRegex(
        QStringLiteral(R"((?:(.*?)错误)?\s*\(第\s*(\d+)\s*行[，,]\s*第\s*(\d+)\s*列\)\s*[:：]\s*(.*))")
    );
    QRegularExpressionMatch antlrMatch = antlrRegex.match(item.message);
    if (antlrMatch.hasMatch()) {
        item.line = antlrMatch.captured(2).toInt();
        const int antlrCol = antlrMatch.captured(3).toInt();
        item.column = antlrCol + 1; // 转换为 1-based 标准列
        item.hasExactColumn = true;  // 来自已知 ANTLR 字符计数，支持精确列定位
        item.message = antlrMatch.captured(4).trimmed();
        if (!defaultScriptPath.isEmpty() && QFile::exists(defaultScriptPath)) {
            item.filePath = resolveFilePath(defaultScriptPath, projectRoot);
        }
        return item;
    }

    // 2. 尝试匹配标准编译器格式: "path/to/file.lh:12:5: error: message" 或 "file.lh:12: error: message"
    static const QRegularExpression gccRegex(
        QStringLiteral(R"(^\s*(.*?):(\d+)(?::(\d+))?:\s*(error|warning|fatal error|note|错误|警告)[:：]\s*(.*)$)"),
        QRegularExpression::CaseInsensitiveOption
    );
    QRegularExpressionMatch gccMatch = gccRegex.match(item.message);
    if (gccMatch.hasMatch()) {
        const QString rawPath = gccMatch.captured(1).trimmed();
        item.filePath = resolveFilePath(rawPath, projectRoot);
        item.line = gccMatch.captured(2).toInt();
        if (!gccMatch.captured(3).isEmpty()) {
            item.column = gccMatch.captured(3).toInt();
            item.hasExactColumn = false; // 未知列号单位来源，保守行定位降级
        }
        item.severity = normalizeSeverity(gccMatch.captured(4));
        item.message = gccMatch.captured(5).trimmed();
        return item;
    }

    // 3. 尝试匹配中文行号格式: "... 第 15 行 ...: 描述"
    static const QRegularExpression cnLineRegex(
        QStringLiteral(R"(第\s*(\d+)\s*行(?:\s*[,，]\s*第\s*(\d+)\s*列)?[:：\s]*(.*))")
    );
    QRegularExpressionMatch cnMatch = cnLineRegex.match(item.message);
    if (cnMatch.hasMatch()) {
        item.line = cnMatch.captured(1).toInt();
        if (!cnMatch.captured(2).isEmpty()) {
            item.column = cnMatch.captured(2).toInt();
            item.hasExactColumn = false;
        }
        // 仅在明确绑定且当前源文件有效时补充默认路径
        if (!defaultScriptPath.isEmpty() && QFile::exists(defaultScriptPath)) {
            item.filePath = resolveFilePath(defaultScriptPath, projectRoot);
        }
        return item;
    }

    return item;
}

QList<DiagnosticItem> DiagnosticParser::parseCompilerOutput(const QString& stdOut,
                                                            const QString& stdErr,
                                                            const QString& projectRoot,
                                                            const QString& defaultScriptPath)
{
    QList<DiagnosticItem> results;

    const QString combined = (stdErr + "\n" + stdOut).trimmed();
    if (combined.isEmpty()) {
        return results;
    }

    const QStringList lines = combined.split('\n');
    for (const QString& line : lines) {
        const QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) {
            continue;
        }

        // 跳过通用提示性输出
        if (trimmedLine.startsWith("INFO:") || trimmedLine.startsWith("DEBUG:")) {
            continue;
        }

        DiagnosticItem item = parseSingleMessage("error", "构建", trimmedLine, projectRoot, defaultScriptPath);
        if (item.line > 0 || !item.filePath.isEmpty()) {
            results.append(item);
        } else if (trimmedLine.contains("error", Qt::CaseInsensitive)
                   || trimmedLine.contains("错误")
                   || trimmedLine.contains("failed", Qt::CaseInsensitive)
                   || trimmedLine.contains("失败")) {
            // 无法定位具体源文件的构建级错误
            item.source = QStringLiteral("构建");
            item.severity = QStringLiteral("error");
            item.filePath.clear();
            item.line = -1;
            item.column = -1;
            item.hasExactColumn = false;
            results.append(item);
        }
    }

    // 若没有任何匹配的错误行，但整段非空且包含有效文本，保留一条总括条目
    if (results.isEmpty() && !stdErr.trimmed().isEmpty()) {
        DiagnosticItem fallback;
        fallback.severity = QStringLiteral("error");
        fallback.source = QStringLiteral("构建");
        fallback.message = stdErr.trimmed();
        fallback.timestamp = QDateTime::currentDateTime();
        results.append(fallback);
    }

    return results;
}
