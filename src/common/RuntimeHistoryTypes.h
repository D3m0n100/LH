#pragma once
#include <QDateTime>
#include <QList>
#include <QString>
#include "RuntimePointTypes.h"

struct RuntimeRecord {
    qint64 id = 0;
    QDateTime timestamp;
    QString variableName;
    double value = 0.0;
    bool valueValid = true;
    RuntimePointQuality quality = RuntimePointQuality::Unknown;
    QString unit;
    QString origin;
    QString errorCode;
    QString errorText;
};

struct RuntimeHistoryCursor
{
    QDateTime timestamp;
    qint64 id = 0;
    qint64 maxId = -1;

    bool isValid() const { return timestamp.isValid() && id > 0; }
    void clear() { timestamp = QDateTime(); id = 0; maxId = -1; }
};

enum class RuntimeHistoryPageStatus
{
    Success,
    NotInitialized,
    SqlError,
    Cancelled
};

/**
 * @brief 一页历史记录及其状态
 *
 * Success 且 records 为空表示成功空页/已到末页；不能用空列表代替
 * NotInitialized 或 SqlError，否则导出会把数据库故障误判为没有数据。
 */
struct RuntimeHistoryPage
{
    RuntimeHistoryPageStatus status = RuntimeHistoryPageStatus::NotInitialized;
    QList<RuntimeRecord> records;
    RuntimeHistoryCursor nextCursor;
    bool hasMore = false;
    QString errorCode;
    QString errorText;

    bool succeeded() const { return status == RuntimeHistoryPageStatus::Success; }
    bool isEnd() const { return succeeded() && !hasMore; }
};

/**
 * @brief 历史结果集的轻量计数结果
 *
 * 只执行 COUNT 查询，不物化历史记录，用于流式导出预先写入兼容的
 * 元数据计数。
 */
struct RuntimeHistoryCount
{
    RuntimeHistoryPageStatus status = RuntimeHistoryPageStatus::NotInitialized;
    qint64 count = 0;
    QString errorCode;
    QString errorText;

    bool succeeded() const { return status == RuntimeHistoryPageStatus::Success; }
};

// 便于调用方使用简短、兼容的类型名。
using HistoryCursor = RuntimeHistoryCursor;
using HistoryPageStatus = RuntimeHistoryPageStatus;
using HistoryPage = RuntimeHistoryPage;
using HistoryPageResult = RuntimeHistoryPage;
using HistoryQueryResult = RuntimeHistoryPage;
using HistoryCount = RuntimeHistoryCount;

