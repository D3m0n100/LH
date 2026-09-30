#ifndef DIAGNOSTIC_ITEM_H
#define DIAGNOSTIC_ITEM_H

#include <QString>
#include <QDateTime>
#include <QMetaType>

/**
 * @brief 结构化诊断条目
 */
struct DiagnosticItem {
    QString compiledFilePath;
    quint64 compiledDocVersion = 0;
    QString filePath;           ///< 关联文件规范绝对路径（若为系统/通信等无源码位置则为空）
    int line = -1;              ///< 1-indexed 行号（<=0 表示无）
    int column = -1;            ///< 1-indexed Unicode 字符列号（<=0 表示无）
    QString severity;           ///< "error", "warning", "info"
    QString source;             ///< 来源，如 "构建", "配置校验", "系统", "下载诊断"
    QString message;            ///< 诊断说明文本
    QDateTime timestamp;        ///< 产生时间
    bool isOutdated = false;    ///< 是否可能已过期（源文件被再次编辑后置为 true）
    bool hasExactColumn = false;///< 列号是否为确定的精确字符单位（为 false 时定位安全降级到行首）

    bool hasLocation() const {
        return !filePath.isEmpty() && line > 0;
    }
};

Q_DECLARE_METATYPE(DiagnosticItem)

#endif // DIAGNOSTIC_ITEM_H
