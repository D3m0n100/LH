#ifndef IMONITOR_HISTORY_STORE_H
#define IMONITOR_HISTORY_STORE_H

#include <QDateTime>
#include <QList>
#include <QString>
#include <memory>
#include <functional>
#include <QObject>
#include <QTimer>

#include "common/RuntimeHistoryTypes.h"

namespace Monitor {

/**
 * @brief 监控历史数据存储抽象接口
 *
 * 封装历史记录的查询、分页与统计能力，使监控服务与全局单例解耦。
 * 支持在单元测试中注入内存替身，以及在后续任务中接入异步数据库执行器。
 */
class IMonitorHistoryStore
{
public:
    virtual ~IMonitorHistoryStore() = default;

    /// 存储后端是否可用
    virtual bool isAvailable() const = 0;

    /// 获取通道最新 records（按降序排列）
    virtual QList<RuntimeRecord> getLatestRecords(const QString& channelName, int count) = 0;

    /// 按时间范围查询历史 records（按时间升序排列）
    virtual QList<RuntimeRecord> queryHistory(const QString& channelName,
                                              const QDateTime& start,
                                              const QDateTime& end) = 0;

    /// keyset 游标分页查询
    virtual RuntimeHistoryPage queryHistoryPage(const QString& channelName,
                                                const QDateTime& start,
                                                const QDateTime& end,
                                                int pageSize,
                                                const RuntimeHistoryCursor& cursor = {}) = 0;

    /// 最近记录 keyset 分页查询
    virtual RuntimeHistoryPage queryLatestHistoryPage(const QString& channelName,
                                                      int maxCount,
                                                      int pageSize,
                                                      const RuntimeHistoryCursor& cursor = {},
                                                      const QDateTime& end = QDateTime()) = 0;

    /// 时间窗统计计数
    virtual RuntimeHistoryCount countHistory(const QString& channelName,
                                             const QDateTime& start,
                                             const QDateTime& end,
                                             qint64 maxRecordId = -1) = 0;

    /// 最近记录统计计数
    virtual RuntimeHistoryCount countLatestHistory(const QString& channelName,
                                                   int maxCount,
                                                   const QDateTime& end = QDateTime(),
                                                   qint64 maxRecordId = -1) = 0;

    /// 取消挂起的异步或耗时查询请求
    virtual void requestPage(QObject* context, const QString& channel, const QDateTime& start,
                             const QDateTime& end, int size, const RuntimeHistoryCursor& cursor,
                             int latestCount, std::function<void(RuntimeHistoryPage)> completed)
    {
        Q_UNUSED(context);
        completed(latestCount > 0 ? queryLatestHistoryPage(channel, latestCount, size, cursor, end)
                                  : queryHistoryPage(channel, start, end, size, cursor));
    }
    virtual void cancelPendingRequests() {}
};

} // namespace Monitor

#endif // IMONITOR_HISTORY_STORE_H
