#ifndef DIAGNOSTIC_PARSER_H
#define DIAGNOSTIC_PARSER_H

#include "DiagnosticItem.h"
#include <QList>
#include <QString>

class DiagnosticParser
{
public:
    /**
     * @brief 解析编译器 stdout 与 stderr 输出，生成结构化诊断列表
     * @param stdOut 编译器标准输出
     * @param stdErr 编译器标准错误
     * @param projectRoot 当前工程根目录（用于解析相对路径）
     * @param defaultScriptPath 本次编译的目标源文件绝对路径
     */
    static QList<DiagnosticItem> parseCompilerOutput(const QString& stdOut,
                                                     const QString& stdErr,
                                                     const QString& projectRoot,
                                                     const QString& defaultScriptPath);

    /**
     * @brief 解析单条通用消息/旧格式日志
     */
    static DiagnosticItem parseSingleMessage(const QString& severity,
                                             const QString& source,
                                             const QString& rawMessage,
                                             const QString& projectRoot = QString(),
                                             const QString& defaultScriptPath = QString());

    /**
     * @brief 提取路径并解析规范绝对路径
     */
    static QString resolveFilePath(const QString& rawPath, const QString& projectRoot);
};

#endif // DIAGNOSTIC_PARSER_H
