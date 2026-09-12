/**
 * @file DataManager.h
 * @brief 数据管理器
 *
 * 提供运行时数据记录、历史数据查询、系统日志等功能。
 * 使用 SQLite 数据库作为持久化存储。
 *
 * 【Schema 版本管理】
 * - 使用 schema_version 表记录当前数据库版本
 * - 支持增量升级，新版本自动迁移
 *
 * 【错误处理】
 * - 所有 SQL 操作统一通过 SqlHelper 执行
 * - 失败时记录完整的上下文信息（SQL、参数、错误码）
 *
 * 【生命周期管理】
 * - initialize() 和 shutdown() 应成对使用
 * - 应用退出前必须调用 shutdown() 以确保数据库连接被正确关闭
 */

#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <QObject>
#include <QThread>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QMutex>
#include <QVariantMap>
#include <QDateTime>
#include <functional>
#include <atomic>
#include "Common.h"
#include "common/RuntimePointTypes.h"
#include "common/RuntimeHistoryTypes.h"

// ============================================================================
// 前向声明和类型定义
// ============================================================================

/**
 * @brief SQL 执行结果
 */
struct QueryResult {
    bool success = false;           ///< 是否成功
    QString errorCode;              ///< 错误码
    QString errorText;              ///< 错误文本
    int affectedRows = 0;           ///< 影响的行数
    qint64 lastInsertId = -1;       ///< 最后插入的 ID

    /// 转换为布尔值（用于 if 判断）
    explicit operator bool() const { return success; }

    /// 获取完整错误信息
    QString fullError() const {
        if (success) return QString();
        return QString("[%1] %2").arg(errorCode, errorText);
    }
};

/**
 * @brief 运行时数据记录
 */
/**
 * @brief 运行时数据表中 SELECT 列投影的字段索引常量
 */
struct RuntimeRecordColumnIndex {
    static constexpr int Id = 0;
    static constexpr int Timestamp = 1;
    static constexpr int VariableName = 2;
    static constexpr int Value = 3;
    static constexpr int Unit = 4;
    static constexpr int ValueValid = 5;
    static constexpr int Quality = 6;
    static constexpr int Origin = 7;
    static constexpr int ErrorCode = 8;
    static constexpr int ErrorText = 9;
    static constexpr int ColumnCount = 10;
};

/// 统一 SELECT 列投影片段
constexpr const char* const RUNTIME_RECORD_SELECT_COLUMNS =
    "id, timestamp, variable_name, value, unit, value_valid, quality, origin, error_code, error_text";

/**
 * @brief 从 QSqlQuery 当前行提取并填充 RuntimeRecord（统一解析逻辑，消除各处 magic index）
 */
RuntimeRecord runtimeRecordFromSql(const QSqlQuery& query);
bool normalizeRuntimeRecord(const QVariantMap& input, QVariantMap& output, QString& error);


/**
 * @brief 运行时历史查询的 keyset 游标
 *
 * 游标由 (timestamp,id) 组成。id 用于区分同一毫秒内的多条记录，
 * 因而分页不会因为时间戳相同而重复或漏行。
 */
/**
 * @brief 数据统计信息
 */
struct DataStatistics {
    double minValue = 0.0;
    double maxValue = 0.0;
    double avgValue = 0.0;
    double sumValue = 0.0;
    int count = 0;
    bool valid = false;             ///< 是否有有效数据
};

/**
 * @brief 系统日志记录
 */
struct LogRecord {
    qint64 id = 0;
    QDateTime timestamp;
    QString level;
    QString module;
    QString message;
};

// ============================================================================
// DataManager 类定义
// ============================================================================

class DataManager : public QObject
{
    Q_OBJECT
    SINGLETON(DataManager)

public:
    /// 当前 Schema 版本
    static constexpr int CURRENT_SCHEMA_VERSION = 5;

    // =========================================================================
    // 生命周期管理
    // =========================================================================

    /// 获取平台提供的用户可写数据库路径；返回空字符串表示路径不可用。
    static QString defaultDatabasePath();

    /**
     * @brief 初始化数据管理器
     * @param dbPath 数据库文件路径
     * @param legacyDbPath 旧安装前缀数据库路径；目标已存在时不覆盖目标
     * @return 是否初始化成功
     *
     * 该方法会：
     * - 创建数据库目录（如不存在）
     * - 打开或创建 SQLite 数据库
     * - 检查并执行 Schema 升级
     * - 创建必要的数据表和索引
     */
    bool initialize(const QString& dbPath, const QString& legacyDbPath = QString());

    /**
     * @brief 关闭数据管理器，释放所有资源
     */
    void shutdown();

    /**
     * @brief 检查数据管理器是否已初始化
     * @return 是否已初始化且数据库连接有效
     */
    bool isInitialized() const;

    /**
     * @brief 检查当前调用线程是否拥有数据库连接
     * @return 若当前线程为拥有线程或未设置拥有线程返回 true，跨线程非法访问返回 false
     */
    bool checkThreadOwnership() const;

    /**
     * @brief 获取当前数据库 Schema 版本
     * @return 版本号，-1 表示未初始化
     */
    int schemaVersion() const;

    // =========================================================================
    // 运行时数据 - 写入
    // =========================================================================

    /**
     * @brief 记录运行时数据
     * @param varName 变量名
     * @param value 数值
     * @param unit 单位（可选）
     * @return 操作结果
     */
    QueryResult logRuntimeData(const QString& varName, double value, const QString& unit = "");

    /**
     * @brief 批量记录运行时数据
     * @param records 记录列表，每条包含 varName、value、unit，可包含 quality、valueValid、timestamp、origin、errorCode、errorText
     * @return 操作结果
     */
    QueryResult logRuntimeDataBatch(const QList<QVariantMap>& records);

    // =========================================================================
    // 运行时数据 - 读取
    // =========================================================================

    /**
     * @brief 获取运行时变量的当前值（从缓存）
     * @param varName 变量名
     * @return 变量值，不存在则返回无效 QVariant
     */
    QVariant getRuntimeValue(const QString& varName) const;

    /**
     * @brief 获取所有缓存的变量名
     * @return 变量名列表
     */
    QStringList getCachedVariableNames() const;

    /**
     * @brief 清空运行时缓存
     */
    void clearRuntimeCache();

    // =========================================================================
    // 历史数据查询
    // =========================================================================

    /**
     * @brief 查询历史数据（时间范围）
     * @param varName 变量名
     * @param start 起始时间
     * @param end 结束时间
     * @return 历史记录列表
     */
    QList<RuntimeRecord> queryHistory(const QString& varName,
                                       const QDateTime& start,
                                       const QDateTime& end);

    /**
     * @brief 按 (timestamp,id) 升序进行 keyset 分页查询
     * @param pageSize 每页最多返回的记录数；SQL 不会请求超过此数量
     * @param cursor 上一页返回的游标；无效游标表示从起点开始
     * @return 明确区分成功空页/末页、未初始化和 SQL 错误的结果
     */
    RuntimeHistoryPage queryHistoryPage(const QString& varName,
                                         const QDateTime& start,
                                         const QDateTime& end,
                                         int pageSize,
                                         const RuntimeHistoryCursor& cursor = {});

    /**
     * @brief 查询最近 maxCount 条记录的升序分页视图
     *
     * 该接口用于时间窗为空时的兼容回退路径。maxCount 限定整个结果集，
     * pageSize 限定单次请求返回的行数；end 有效时限定记录时间上界。
     */
    RuntimeHistoryPage queryLatestHistoryPage(const QString& varName,
                                               int maxCount,
                                               int pageSize,
                                               const RuntimeHistoryCursor& cursor = {},
                                               const QDateTime& end = QDateTime());

    /// 轻量统计时间窗内记录数，不构造历史列表；maxRecordId >= 0 时限定记录 ID 上界，-1 表示不限制。
    RuntimeHistoryCount countHistory(const QString& varName,
                                      const QDateTime& start,
                                      const QDateTime& end,
                                      qint64 maxRecordId = -1);

    /// 轻量统计最近 maxCount 记录数，不构造历史列表；end 有效时限定时间上界，maxRecordId >= 0 时限定记录 ID 上界，-1 表示不限制。
    RuntimeHistoryCount countLatestHistory(const QString& varName,
                                            int maxCount,
                                            const QDateTime& end = QDateTime(),
                                            qint64 maxRecordId = -1);

    /**
     * @brief 获取当前数据库最大记录 ID，用于建立稳定的历史导出快照
     * @return 当前最大记录 ID，未初始化或无记录时返回 0
     */
    qint64 latestRecordId();

    /**
     * @brief 获取最近 N 条记录
     * @param varName 变量名
     * @param count 记录数量
     * @return 记录列表（按时间倒序）
     */
    QList<RuntimeRecord> getLatestRecords(const QString& varName, int count = 100);

    /**
     * @brief 获取某时间之后的所有记录
     * @param varName 变量名
     * @param since 起始时间
     * @return 记录列表
     */
    QList<RuntimeRecord> getRecordsSince(const QString& varName, const QDateTime& since);

    /**
     * @brief 获取统计信息
     * @param varName 变量名
     * @param start 起始时间
     * @param end 结束时间
     * @return 统计信息（min/max/avg/sum/count）
     */
    DataStatistics getStatistics(const QString& varName,
                                  const QDateTime& start,
                                  const QDateTime& end);

    /**
     * @brief 获取所有已记录的变量名
     * @return 变量名列表
     */
    QStringList getAllVariableNames();

    // =========================================================================
    // 系统日志
    // =========================================================================

    /**
     * @brief 写入系统日志
     * @param level 日志级别
     * @param module 模块名
     * @param message 日志消息
     * @return 操作结果
     */
    QueryResult writeLog(const QString& level,
                         const QString& module,
                         const QString& message);

    /**
     * @brief 查询系统日志
     * @param start 起始时间
     * @param end 结束时间
     * @param level 日志级别过滤（空字符串表示不过滤）
     * @param limit 最大返回数量
     * @return 日志记录列表
     */
    QList<LogRecord> queryLogs(const QDateTime& start,
                                const QDateTime& end,
                                const QString& level = QString(),
                                int limit = 1000);

    // =========================================================================
    // 数据维护
    // =========================================================================

    /**
     * @brief 清理过期数据（支持分批短事务提交与取消）
     * @param retentionDays 保留天数
     * @param cancelToken 可选的取消标志指针
     * @param chunkSize 单批删除记录上限（默认 1000）
     * @return 删除的记录数；未初始化、参数无效或 SQL/事务失败时返回 -1
     */
    int cleanupOldData(int retentionDays,
                       const std::atomic_bool* cancelToken = nullptr,
                       int chunkSize = 1000);

    /**
     * @brief 执行数据库优化（VACUUM）
     * @return 操作结果
     */
    QueryResult optimizeDatabase();

signals:
    /**
     * @brief 数据更新信号
     * @param varName 变量名
     * @param value 新值
     */
    void dataUpdated(const QString& varName, const QVariant& value);

    /**
     * @brief 数据库错误信号
     * @param operation 操作名称
     * @param error 错误信息
     */
    void databaseError(const QString& operation, const QString& error);

private:
    // =========================================================================
    // 内部辅助方法
    // =========================================================================

    /// 执行 SQL 查询（带完整错误日志）
    QueryResult executeQuery(QSqlQuery& query, const QString& description);

    /// 执行 SQL 语句（简化版本）
    QueryResult executeSql(const QString& sql, const QString& description);

    /// 获取当前数据库版本
    bool getDatabaseVersion(int& version);

    /// 将旧安装前缀数据库原子迁移到目标路径；失败时写入 errorText。
    static bool migrateLegacyDatabase(const QString& legacyDbPath,
                                      const QString& dbPath,
                                      bool& migrated,
                                      QString& errorText);

    /// 清理数据库连接（调用方必须持有 m_dbMutex）
    void cleanupDatabaseConnection();

    /// 设置数据库版本
    bool setDatabaseVersion(int version);

    /// 执行 Schema 迁移
    bool migrateSchema(int fromVersion, int toVersion);

    /// 创建初始 Schema（版本 1）
    bool createInitialSchema();

    /// 升级到版本 2
    bool upgradeToVersion2();

    /// 升级到版本 3
    bool upgradeToVersion3();

    /// 升级到版本 4：添加稳定的来源和错误上下文字段
    bool upgradeToVersion4();

    /// 升级到版本 5：规范化历史时间存储为标准固定宽度 UTC ISO-8601 字符串
    bool upgradeToVersion5();

    /// 记录 SQL 错误详情
    void logSqlError(const QSqlQuery& query, const QString& description);

private:
    QSqlDatabase m_db;                          ///< 数据库连接
    mutable QMutex m_cacheMutex;                ///< 缓存互斥锁
    mutable QMutex m_dbMutex;                   ///< 数据库操作互斥锁
    QMap<QString, QVariant> m_runtimeCache;     ///< 运行时变量缓存
    QString m_connectionName;                   ///< 数据库连接名
    bool m_initialized = false;                 ///< 初始化状态标志
    int m_schemaVersion = -1;                   ///< 当前 Schema 版本
    QThread* m_ownerThread = nullptr;           ///< 数据库所有者线程
};

#endif // DATAMANAGER_H
