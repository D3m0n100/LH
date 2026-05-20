/**
 * @file DataManager.cpp
 * @brief 数据管理器实现
 */

#include "DataManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDir>
#include <QUuid>
#include <QDebug>

// ============================================================================
// Schema 定义（集中管理）
// ============================================================================

namespace SchemaDefinitions {

/// 版本表 SQL
const char* const CREATE_VERSION_TABLE = R"(
    CREATE TABLE IF NOT EXISTS schema_version (
        id INTEGER PRIMARY KEY CHECK (id = 1),
        version INTEGER NOT NULL,
        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )
)";

/// 运行时数据表 SQL（版本 1）
const char* const CREATE_RUNTIME_DATA_V1 = R"(
    CREATE TABLE IF NOT EXISTS runtime_data (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
        variable_name TEXT NOT NULL,
        value REAL,
        unit TEXT
    )
)";

/// 系统日志表 SQL（版本 1）
const char* const CREATE_SYSTEM_LOGS_V1 = R"(
    CREATE TABLE IF NOT EXISTS system_logs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
        level TEXT NOT NULL,
        module TEXT,
        message TEXT
    )
)";

/// 运行时数据索引
const char* const CREATE_RUNTIME_INDEXES = R"(
    CREATE INDEX IF NOT EXISTS idx_runtime_timestamp ON runtime_data(timestamp);
    CREATE INDEX IF NOT EXISTS idx_runtime_variable ON runtime_data(variable_name);
    CREATE INDEX IF NOT EXISTS idx_runtime_var_time ON runtime_data(variable_name, timestamp);
)";

/// 系统日志索引
const char* const CREATE_LOG_INDEXES = R"(
    CREATE INDEX IF NOT EXISTS idx_logs_timestamp ON system_logs(timestamp);
    CREATE INDEX IF NOT EXISTS idx_logs_level ON system_logs(level);
    CREATE INDEX IF NOT EXISTS idx_logs_level_time ON system_logs(level, timestamp);
)";

/// 版本 2 新增：数据摘要表（用于快速统计）
const char* const CREATE_DATA_SUMMARY_V2 = R"(
    CREATE TABLE IF NOT EXISTS data_summary (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        variable_name TEXT NOT NULL,
        summary_date DATE NOT NULL,
        min_value REAL,
        max_value REAL,
        avg_value REAL,
        sum_value REAL,
        count INTEGER,
        UNIQUE(variable_name, summary_date)
    )
)";

} // namespace SchemaDefinitions

// ============================================================================
// 构造 / 析构
// ============================================================================

DataManager::DataManager()
    : QObject(nullptr)
    , m_initialized(false)
    , m_schemaVersion(-1)
{
    // 生成唯一的连接名，支持多实例场景
    m_connectionName = QString("DataManager_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
}

DataManager::~DataManager()
{
    if (m_initialized) {
        LOG_WARN("DataManager 析构时仍处于初始化状态，建议显式调用 shutdown()");
        shutdown();
    }
}

// ============================================================================
// 生命周期管理
// ============================================================================

bool DataManager::initialize(const QString& dbPath)
{
    QMutexLocker dbLocker(&m_dbMutex);
    
    // 如果已经初始化，先关闭旧连接
    if (m_initialized) {
        LOG_INFO("DataManager 重新初始化，先关闭旧连接");
        // 临时释放锁以调用 shutdown
        dbLocker.unlock();
        shutdown();
        dbLocker.relock();
    }
    
    // 确保目录存在
    QFileInfo fileInfo(dbPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            LOG_ERROR("无法创建数据库目录: " + dir.absolutePath());
            emit databaseError("initialize", "无法创建数据库目录: " + dir.absolutePath());
            return false;
        }
    }
    
    // 使用命名连接
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(dbPath);
    
    if (!m_db.open()) {
        QString error = m_db.lastError().text();
        LOG_ERROR("数据库打开失败: " + error);
        emit databaseError("initialize", "数据库打开失败: " + error);
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }
    
    // 启用外键约束
    executeSql("PRAGMA foreign_keys = ON", "启用外键约束");
    
    // 检查并创建版本表
    executeSql(SchemaDefinitions::CREATE_VERSION_TABLE, "创建版本表");
    
    // 获取当前版本
    int currentVersion = getDatabaseVersion();
    LOG_INFO(QString("当前数据库版本: %1, 目标版本: %2")
             .arg(currentVersion).arg(CURRENT_SCHEMA_VERSION));
    
    // 执行迁移
    if (currentVersion < CURRENT_SCHEMA_VERSION) {
        if (!migrateSchema(currentVersion, CURRENT_SCHEMA_VERSION)) {
            LOG_ERROR("Schema 迁移失败");
            m_db.close();
            QSqlDatabase::removeDatabase(m_connectionName);
            return false;
        }
    }
    
    m_schemaVersion = CURRENT_SCHEMA_VERSION;
    m_initialized = true;
    
    LOG_INFO("数据库初始化完成: " + dbPath);
    return true;
}

void DataManager::shutdown()
{
    QMutexLocker dbLocker(&m_dbMutex);
    
    if (!m_initialized) {
        return;
    }
    
    LOG_INFO("DataManager 正在关闭...");
    
    // 清空运行时缓存
    {
        QMutexLocker cacheLocker(&m_cacheMutex);
        m_runtimeCache.clear();
    }
    
    // 关闭数据库连接
    if (m_db.isOpen()) {
        m_db.close();
    }
    
    // 移除数据库连接
    QString connName = m_connectionName;
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connName);
    
    m_initialized = false;
    m_schemaVersion = -1;
    
    LOG_INFO("DataManager 已关闭");
}

bool DataManager::isInitialized() const
{
    return m_initialized && m_db.isOpen();
}

int DataManager::schemaVersion() const
{
    return m_schemaVersion;
}

// ============================================================================
// Schema 迁移
// ============================================================================

int DataManager::getDatabaseVersion()
{
    QSqlQuery query(m_db);
    query.prepare("SELECT version FROM schema_version WHERE id = 1");
    
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    
    // 没有版本记录，返回 0 表示全新数据库
    return 0;
}

bool DataManager::setDatabaseVersion(int version)
{
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT OR REPLACE INTO schema_version (id, version, updated_at)
        VALUES (1, :version, CURRENT_TIMESTAMP)
    )");
    query.bindValue(":version", version);
    
    if (!query.exec()) {
        logSqlError(query, "设置数据库版本");
        return false;
    }
    
    LOG_INFO(QString("数据库版本已更新为: %1").arg(version));
    return true;
}

bool DataManager::migrateSchema(int fromVersion, int toVersion)
{
    LOG_INFO(QString("开始 Schema 迁移: %1 -> %2").arg(fromVersion).arg(toVersion));
    
    // 开启事务
    if (!m_db.transaction()) {
        LOG_ERROR("无法开启迁移事务: " + m_db.lastError().text());
        return false;
    }
    
    bool success = true;
    
    // 执行各版本升级
    for (int ver = fromVersion + 1; ver <= toVersion && success; ++ver) {
        LOG_INFO(QString("执行迁移到版本 %1...").arg(ver));
        
        switch (ver) {
            case 1:
                success = createInitialSchema();
                break;
            case 2:
                success = upgradeToVersion2();
                break;
            default:
                LOG_ERROR(QString("未知的迁移版本: %1").arg(ver));
                success = false;
                break;
        }
        
        if (success) {
            success = setDatabaseVersion(ver);
        }
    }
    
    // 提交或回滚事务
    if (success) {
        if (!m_db.commit()) {
            LOG_ERROR("提交迁移事务失败: " + m_db.lastError().text());
            m_db.rollback();
            return false;
        }
        LOG_INFO("Schema 迁移完成");
    } else {
        m_db.rollback();
        LOG_ERROR("Schema 迁移失败，已回滚");
    }
    
    return success;
}

bool DataManager::createInitialSchema()
{
    // 创建运行时数据表
    auto result = executeSql(SchemaDefinitions::CREATE_RUNTIME_DATA_V1, "创建 runtime_data 表");
    if (!result) return false;
    
    // 创建系统日志表
    result = executeSql(SchemaDefinitions::CREATE_SYSTEM_LOGS_V1, "创建 system_logs 表");
    if (!result) return false;
    
    // 创建索引（分开执行）
    QStringList indexSqls = QString(SchemaDefinitions::CREATE_RUNTIME_INDEXES)
                            .split(';', Qt::SkipEmptyParts);
    for (const QString& sql : indexSqls) {
        QString trimmed = sql.trimmed();
        if (!trimmed.isEmpty()) {
            executeSql(trimmed, "创建运行时数据索引");
        }
    }
    
    indexSqls = QString(SchemaDefinitions::CREATE_LOG_INDEXES).split(';', Qt::SkipEmptyParts);
    for (const QString& sql : indexSqls) {
        QString trimmed = sql.trimmed();
        if (!trimmed.isEmpty()) {
            executeSql(trimmed, "创建日志索引");
        }
    }
    
    return true;
}

bool DataManager::upgradeToVersion2()
{
    // 版本 2：添加数据摘要表
    auto result = executeSql(SchemaDefinitions::CREATE_DATA_SUMMARY_V2, "创建 data_summary 表");
    if (!result) return false;
    
    // 为运行时数据表添加复合索引（如果不存在）
    executeSql("CREATE INDEX IF NOT EXISTS idx_runtime_var_time ON runtime_data(variable_name, timestamp)",
               "创建复合索引");
    
    return true;
}

// ============================================================================
// SQL 执行辅助方法
// ============================================================================

QueryResult DataManager::executeQuery(QSqlQuery& query, const QString& description)
{
    QueryResult result;
    
    if (query.exec()) {
        result.success = true;
        result.affectedRows = query.numRowsAffected();
        result.lastInsertId = query.lastInsertId().toLongLong();
    } else {
        result.success = false;
        result.errorCode = query.lastError().nativeErrorCode();
        result.errorText = query.lastError().text();
        
        logSqlError(query, description);
        emit databaseError(description, result.fullError());
    }
    
    return result;
}

QueryResult DataManager::executeSql(const QString& sql, const QString& description)
{
    QSqlQuery query(m_db);
    query.prepare(sql);
    return executeQuery(query, description);
}

void DataManager::logSqlError(const QSqlQuery& query, const QString& description)
{
    QSqlError error = query.lastError();
    QString sql = query.executedQuery();
    if (sql.isEmpty()) {
        sql = query.lastQuery();
    }
    
    // 获取绑定参数
    QMap<QString, QVariant> boundValues = query.boundValues();
    QString params;
    for (auto it = boundValues.begin(); it != boundValues.end(); ++it) {
        if (!params.isEmpty()) params += ", ";
        params += QString("%1=%2").arg(it.key(), it.value().toString());
    }
    
    // 构建详细错误日志
    QString logMsg = QString("SQL 执行失败 [%1]\n"
                             "  SQL: %2\n"
                             "  参数: %3\n"
                             "  错误码: %4\n"
                             "  错误信息: %5")
                     .arg(description)
                     .arg(sql)
                     .arg(params.isEmpty() ? "(无)" : params)
                     .arg(error.nativeErrorCode())
                     .arg(error.text());
    
    LOG_ERROR(logMsg);
}

// ============================================================================
// 运行时数据 - 写入
// ============================================================================

QueryResult DataManager::logRuntimeData(const QString& varName, double value, const QString& unit)
{
    if (!m_initialized) {
        LOG_WARN("DataManager 未初始化，无法记录运行数据");
        QueryResult result;
        result.errorText = "DataManager 未初始化";
        return result;
    }
    
    QMutexLocker dbLocker(&m_dbMutex);
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO runtime_data (timestamp, variable_name, value, unit)
        VALUES (COALESCE(:timestamp, CURRENT_TIMESTAMP), :name, :value, :unit)
    )");
    query.bindValue(":timestamp", QDateTime::currentDateTimeUtc());
    query.bindValue(":name", varName);
    query.bindValue(":value", value);
    query.bindValue(":unit", unit);
    
    QueryResult result = executeQuery(query, "记录运行数据");
    
    if (result.success) {
        // 更新缓存
        {
            QMutexLocker cacheLocker(&m_cacheMutex);
            m_runtimeCache[varName] = value;
        }
        emit dataUpdated(varName, value);
    }
    
    return result;
}

QueryResult DataManager::logRuntimeDataBatch(const QList<QVariantMap>& records)
{
    if (!m_initialized) {
        LOG_WARN("DataManager 未初始化，无法批量记录运行数据");
        QueryResult result;
        result.errorText = "DataManager 未初始化";
        return result;
    }
    
    if (records.isEmpty()) {
        QueryResult result;
        result.success = true;
        return result;
    }
    
    QMutexLocker dbLocker(&m_dbMutex);
    
    // 开启事务以提高批量插入性能
    if (!m_db.transaction()) {
        QueryResult result;
        result.errorText = "无法开启事务: " + m_db.lastError().text();
        LOG_ERROR(result.errorText);
        return result;
    }
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO runtime_data (timestamp, variable_name, value, unit)
        VALUES (COALESCE(:timestamp, CURRENT_TIMESTAMP), :name, :value, :unit)
    )");
    
    int successCount = 0;
    QMap<QString, double> updatedValues;
    
    for (const QVariantMap& record : records) {
        QString varName = record.value("varName").toString();
        double value = record.value("value").toDouble();
        QString unit = record.value("unit").toString();

        // 可选：允许外部提供真实采样时间（用于历史查询/回放）
        // 如果未提供，则由 SQL 中的 CURRENT_TIMESTAMP 自动填充
        const QVariant tsVar = record.value("timestamp");
        QDateTime ts;
        if (tsVar.isValid()) {
            ts = tsVar.toDateTime();
        }

        query.bindValue(":timestamp", ts.isValid() ? QVariant(ts) : QVariant());
        query.bindValue(":name", varName);
        query.bindValue(":value", value);
        query.bindValue(":unit", unit);
        
        if (query.exec()) {
            ++successCount;
            updatedValues[varName] = value;
        } else {
            logSqlError(query, QString("批量记录运行数据[%1]").arg(varName));
        }
    }
    
    QueryResult result;
    if (m_db.commit()) {
        result.success = true;
        result.affectedRows = successCount;
        
        // 更新缓存并发送信号
        {
            QMutexLocker cacheLocker(&m_cacheMutex);
            for (auto it = updatedValues.begin(); it != updatedValues.end(); ++it) {
                m_runtimeCache[it.key()] = it.value();
            }
        }
        
        for (auto it = updatedValues.begin(); it != updatedValues.end(); ++it) {
            emit dataUpdated(it.key(), it.value());
        }
        
        LOG_DEBUG(QString("批量记录运行数据: %1/%2 条成功")
                  .arg(successCount).arg(records.size()));
    } else {
        m_db.rollback();
        result.errorText = "提交事务失败: " + m_db.lastError().text();
        LOG_ERROR(result.errorText);
    }
    
    return result;
}

// ============================================================================
// 运行时数据 - 读取
// ============================================================================

QVariant DataManager::getRuntimeValue(const QString& varName) const
{
    QMutexLocker locker(&m_cacheMutex);
    return m_runtimeCache.value(varName);
}

QStringList DataManager::getCachedVariableNames() const
{
    QMutexLocker locker(&m_cacheMutex);
    return m_runtimeCache.keys();
}

void DataManager::clearRuntimeCache()
{
    QMutexLocker locker(&m_cacheMutex);
    m_runtimeCache.clear();
    LOG_DEBUG("运行时缓存已清空");
}

// ============================================================================
// 历史数据查询
// ============================================================================

QList<RuntimeRecord> DataManager::queryHistory(const QString& varName,
                                                const QDateTime& start,
                                                const QDateTime& end)
{
    QList<RuntimeRecord> results;
    
    if (!m_initialized) {
        LOG_WARN("DataManager 未初始化，无法查询历史数据");
        return results;
    }
    
    QMutexLocker dbLocker(&m_dbMutex);
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT id, timestamp, variable_name, value, unit
        FROM runtime_data
        WHERE variable_name = :name
          AND timestamp BETWEEN :start AND :end
        ORDER BY timestamp ASC
    )");
    query.bindValue(":name", varName);
    query.bindValue(":start", start);
    query.bindValue(":end", end);
    
    if (query.exec()) {
        while (query.next()) {
            RuntimeRecord record;
            record.id = query.value(0).toLongLong();
            record.timestamp = query.value(1).toDateTime();
            record.variableName = query.value(2).toString();
            record.value = query.value(3).toDouble();
            record.unit = query.value(4).toString();
            results.append(record);
        }
    } else {
        logSqlError(query, "查询历史数据");
    }
    
    return results;
}

QList<RuntimeRecord> DataManager::getLatestRecords(const QString& varName, int count)
{
    QList<RuntimeRecord> results;
    
    if (!m_initialized) {
        LOG_WARN("DataManager 未初始化");
        return results;
    }
    
    QMutexLocker dbLocker(&m_dbMutex);
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT id, timestamp, variable_name, value, unit
        FROM runtime_data
        WHERE variable_name = :name
        ORDER BY timestamp DESC
        LIMIT :count
    )");
    query.bindValue(":name", varName);
    query.bindValue(":count", count);
    
    if (query.exec()) {
        while (query.next()) {
            RuntimeRecord record;
            record.id = query.value(0).toLongLong();
            record.timestamp = query.value(1).toDateTime();
            record.variableName = query.value(2).toString();
            record.value = query.value(3).toDouble();
            record.unit = query.value(4).toString();
            results.append(record);
        }
    } else {
        logSqlError(query, "获取最近记录");
    }
    
    return results;
}

QList<RuntimeRecord> DataManager::getRecordsSince(const QString& varName, const QDateTime& since)
{
    QList<RuntimeRecord> results;
    
    if (!m_initialized) {
        LOG_WARN("DataManager 未初始化");
        return results;
    }
    
    QMutexLocker dbLocker(&m_dbMutex);
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT id, timestamp, variable_name, value, unit
        FROM runtime_data
        WHERE variable_name = :name
          AND timestamp > :since
        ORDER BY timestamp ASC
    )");
    query.bindValue(":name", varName);
    query.bindValue(":since", since);
    
    if (query.exec()) {
        while (query.next()) {
            RuntimeRecord record;
            record.id = query.value(0).toLongLong();
            record.timestamp = query.value(1).toDateTime();
            record.variableName = query.value(2).toString();
            record.value = query.value(3).toDouble();
            record.unit = query.value(4).toString();
            results.append(record);
        }
    } else {
        logSqlError(query, "获取指定时间后的记录");
    }
    
    return results;
}

DataStatistics DataManager::getStatistics(const QString& varName,
                                           const QDateTime& start,
                                           const QDateTime& end)
{
    DataStatistics stats;
    
    if (!m_initialized) {
        LOG_WARN("DataManager 未初始化");
        return stats;
    }
    
    QMutexLocker dbLocker(&m_dbMutex);
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT MIN(value), MAX(value), AVG(value), SUM(value), COUNT(*)
        FROM runtime_data
        WHERE variable_name = :name
          AND timestamp BETWEEN :start AND :end
    )");
    query.bindValue(":name", varName);
    query.bindValue(":start", start);
    query.bindValue(":end", end);
    
    if (query.exec() && query.next()) {
        stats.count = query.value(4).toInt();
        if (stats.count > 0) {
            stats.minValue = query.value(0).toDouble();
            stats.maxValue = query.value(1).toDouble();
            stats.avgValue = query.value(2).toDouble();
            stats.sumValue = query.value(3).toDouble();
            stats.valid = true;
        }
    } else {
        logSqlError(query, "获取统计信息");
    }
    
    return stats;
}

QStringList DataManager::getAllVariableNames()
{
    QStringList names;
    
    if (!m_initialized) {
        return names;
    }
    
    QMutexLocker dbLocker(&m_dbMutex);
    
    QSqlQuery query(m_db);
    query.prepare("SELECT DISTINCT variable_name FROM runtime_data ORDER BY variable_name");
    
    if (query.exec()) {
        while (query.next()) {
            names.append(query.value(0).toString());
        }
    } else {
        logSqlError(query, "获取所有变量名");
    }
    
    return names;
}

// ============================================================================
// 系统日志
// ============================================================================

QueryResult DataManager::writeLog(const QString& level,
                                   const QString& module,
                                   const QString& message)
{
    if (!m_initialized) {
        QueryResult result;
        result.errorText = "DataManager 未初始化";
        return result;
    }
    
    QMutexLocker dbLocker(&m_dbMutex);
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO system_logs (level, module, message)
        VALUES (:level, :module, :message)
    )");
    query.bindValue(":level", level);
    query.bindValue(":module", module);
    query.bindValue(":message", message);
    
    // 日志写入失败不递归记录，避免无限循环
    QueryResult result;
    if (query.exec()) {
        result.success = true;
        result.lastInsertId = query.lastInsertId().toLongLong();
    } else {
        result.errorText = query.lastError().text();
    }
    
    return result;
}

QList<LogRecord> DataManager::queryLogs(const QDateTime& start,
                                         const QDateTime& end,
                                         const QString& level,
                                         int limit)
{
    QList<LogRecord> results;
    
    if (!m_initialized) {
        return results;
    }
    
    QMutexLocker dbLocker(&m_dbMutex);
    
    QString sql = R"(
        SELECT id, timestamp, level, module, message
        FROM system_logs
        WHERE timestamp BETWEEN :start AND :end
    )";
    
    if (!level.isEmpty()) {
        sql += " AND level = :level";
    }
    
    sql += " ORDER BY timestamp DESC LIMIT :limit";
    
    QSqlQuery query(m_db);
    query.prepare(sql);
    query.bindValue(":start", start);
    query.bindValue(":end", end);
    query.bindValue(":limit", limit);
    
    if (!level.isEmpty()) {
        query.bindValue(":level", level);
    }
    
    if (query.exec()) {
        while (query.next()) {
            LogRecord record;
            record.id = query.value(0).toLongLong();
            record.timestamp = query.value(1).toDateTime();
            record.level = query.value(2).toString();
            record.module = query.value(3).toString();
            record.message = query.value(4).toString();
            results.append(record);
        }
    } else {
        logSqlError(query, "查询系统日志");
    }
    
    return results;
}

// ============================================================================
// 数据维护
// ============================================================================

int DataManager::cleanupOldData(int retentionDays)
{
    if (!m_initialized || retentionDays <= 0) {
        return 0;
    }
    
    QMutexLocker dbLocker(&m_dbMutex);
    
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-retentionDays);
    
    int totalDeleted = 0;
    
    // 清理运行时数据
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM runtime_data WHERE timestamp < :cutoff");
    query.bindValue(":cutoff", cutoff);
    
    if (query.exec()) {
        totalDeleted += query.numRowsAffected();
    } else {
        logSqlError(query, "清理运行时数据");
    }
    
    // 清理系统日志
    query.prepare("DELETE FROM system_logs WHERE timestamp < :cutoff");
    query.bindValue(":cutoff", cutoff);
    
    if (query.exec()) {
        totalDeleted += query.numRowsAffected();
    } else {
        logSqlError(query, "清理系统日志");
    }
    
    LOG_INFO(QString("数据清理完成，删除 %1 条记录（保留 %2 天）")
             .arg(totalDeleted).arg(retentionDays));
    
    return totalDeleted;
}

QueryResult DataManager::optimizeDatabase()
{
    if (!m_initialized) {
        QueryResult result;
        result.errorText = "DataManager 未初始化";
        return result;
    }
    
    QMutexLocker dbLocker(&m_dbMutex);
    
    LOG_INFO("开始数据库优化 (VACUUM)...");
    
    QueryResult result = executeSql("VACUUM", "数据库优化");
    
    if (result.success) {
        LOG_INFO("数据库优化完成");
    }
    
    return result;
}
