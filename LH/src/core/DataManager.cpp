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

/// 版本 3 新增：采样质量和值有效性
const char* const ADD_RUNTIME_QUALITY_V3 =
    "ALTER TABLE runtime_data ADD COLUMN quality TEXT NOT NULL DEFAULT 'Unknown'";
const char* const ADD_RUNTIME_VALUE_VALID_V3 =
    "ALTER TABLE runtime_data ADD COLUMN value_valid INTEGER NOT NULL DEFAULT 1";

/// 版本 4 新增：稳定的来源和错误上下文
const char* const ADD_RUNTIME_ORIGIN_V4 =
    "ALTER TABLE runtime_data ADD COLUMN origin TEXT NOT NULL DEFAULT ''";
const char* const ADD_RUNTIME_ERROR_CODE_V4 =
    "ALTER TABLE runtime_data ADD COLUMN error_code TEXT NOT NULL DEFAULT ''";
const char* const ADD_RUNTIME_ERROR_TEXT_V4 =
    "ALTER TABLE runtime_data ADD COLUMN error_text TEXT NOT NULL DEFAULT ''";

} // namespace SchemaDefinitions

namespace {

QVariantMap nestedRuntimeMetadata(const QVariantMap& record)
{
    return record.value(QStringLiteral("metadata")).toMap();
}

QString runtimeOriginFromRecord(const QVariantMap& record)
{
    const QVariantMap metadata = nestedRuntimeMetadata(record);
    QString origin = record.value(QStringLiteral("origin")).toString();
    if (origin.isEmpty()) {
        origin = metadata.value(QStringLiteral("origin")).toString();
    }
    if (origin.isEmpty()) {
        origin = record.value(QStringLiteral("source")).toString();
    }
    if (origin.isEmpty()) {
        origin = metadata.value(QStringLiteral("source")).toString();
    }
    return origin.isNull() ? QStringLiteral("") : origin;
}

QString runtimeErrorCodeFromRecord(const QVariantMap& record)
{
    const QVariantMap metadata = nestedRuntimeMetadata(record);
    QString errorCode = metadata.value(QStringLiteral("errorCodeName")).toString();
    if (errorCode.isEmpty()) {
        errorCode = record.value(QStringLiteral("errorCodeName")).toString();
    }
    if (errorCode.isEmpty()) {
        errorCode = metadata.value(QStringLiteral("errorCode")).toString();
    }
    if (errorCode.isEmpty()) {
        errorCode = record.value(QStringLiteral("errorCode")).toString();
    }
    return errorCode.isNull() ? QStringLiteral("") : errorCode;
}

QString runtimeErrorTextFromRecord(const QVariantMap& record)
{
    const QVariantMap metadata = nestedRuntimeMetadata(record);
    QString errorText = record.value(QStringLiteral("errorText")).toString();
    if (errorText.isEmpty()) {
        errorText = record.value(QStringLiteral("error")).toString();
    }
    if (errorText.isEmpty()) {
        errorText = metadata.value(QStringLiteral("error")).toString();
    }

    QString errorDetails = record.value(QStringLiteral("errorDetails")).toString();
    if (errorDetails.isEmpty()) {
        errorDetails = metadata.value(QStringLiteral("errorDetails")).toString();
    }
    if (!errorDetails.isEmpty()) {
        if (errorText.isEmpty()) {
            errorText = errorDetails;
        } else {
            errorText += QStringLiteral(" (details: ") + errorDetails
                       + QLatin1Char(')');
        }
    }
    return errorText.isNull() ? QStringLiteral("") : errorText;
}

} // namespace

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
    QString initError; // 在锁外 emit 的错误信息

    // --- 持锁阶段：准备数据库连接 ---
    {
        QMutexLocker dbLocker(&m_dbMutex);

        // 如果已经初始化，在同一把锁内关闭旧连接（避免 unlock/relock 竞态）
        if (m_initialized) {
            LOG_INFO("DataManager 重新初始化，先关闭旧连接");
            cleanupDatabaseConnection();
        }

        // 确保目录存在
        QFileInfo fileInfo(dbPath);
        QDir dir = fileInfo.absoluteDir();
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                initError = "无法创建数据库目录: " + dir.absolutePath();
                LOG_ERROR(initError);
            }
        }

        if (initError.isEmpty()) {
            // 使用命名连接
            m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
            m_db.setDatabaseName(dbPath);

            if (!m_db.open()) {
                initError = "数据库打开失败: " + m_db.lastError().text();
                LOG_ERROR(initError);
            }
        }

        if (initError.isEmpty()) {
            // 启用外键约束
            if (!executeSql("PRAGMA foreign_keys = ON", "启用外键约束")) {
                initError = "启用外键约束失败";
                LOG_ERROR(initError);
            }
        }

        if (initError.isEmpty()) {
            // 检查并创建版本表
            if (!executeSql(SchemaDefinitions::CREATE_VERSION_TABLE, "创建版本表")) {
                initError = "创建版本表失败";
                LOG_ERROR(initError);
            }
        }

        if (initError.isEmpty()) {
            // 获取当前版本
            int currentVersion = 0;
            if (!getDatabaseVersion(currentVersion)) {
                initError = "读取数据库版本失败";
                LOG_ERROR(initError);
            } else {
                LOG_INFO(QString("当前数据库版本: %1, 目标版本: %2")
                         .arg(currentVersion).arg(CURRENT_SCHEMA_VERSION));

                // 执行迁移
                if (currentVersion < CURRENT_SCHEMA_VERSION) {
                    if (!migrateSchema(currentVersion, CURRENT_SCHEMA_VERSION)) {
                        initError = "Schema 迁移失败";
                        LOG_ERROR(initError);
                    }
                }
            }
        }

        if (!initError.isEmpty()) {
            cleanupDatabaseConnection();
        } else {
            m_schemaVersion = CURRENT_SCHEMA_VERSION;
            m_initialized = true;
            LOG_INFO("数据库初始化完成: " + dbPath);
        }
    }
    // --- 锁已释放 ---

    // 在锁外 emit 信号，避免死锁
    if (!initError.isEmpty()) {
        emit databaseError("initialize", initError);
        return false;
    }
    return true;
}

void DataManager::shutdown()
{
    QMutexLocker dbLocker(&m_dbMutex);
    const bool wasInitialized = m_initialized;

    if (wasInitialized) {
        LOG_INFO("DataManager 正在关闭...");
    }

    cleanupDatabaseConnection();

    if (wasInitialized) {
        LOG_INFO("DataManager 已关闭");
    }
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

bool DataManager::getDatabaseVersion(int& version)
{
    version = 0;
    QSqlQuery query(m_db);
    query.prepare("SELECT typeof(version), version FROM schema_version WHERE id = 1");

    if (!query.exec()) {
        logSqlError(query, "读取数据库版本");
        return false;
    }

    // 查询成功但没有版本记录，表示全新数据库。
    if (!query.next()) {
        return true;
    }

    if (query.value(0).toString() != QStringLiteral("integer")) {
        LOG_ERROR("数据库版本必须是 SQLite INTEGER");
        return false;
    }

    bool ok = false;
    const qlonglong storedVersion = query.value(1).toLongLong(&ok);
    if (!ok || storedVersion < 0 || storedVersion > CURRENT_SCHEMA_VERSION) {
        LOG_ERROR(QString("数据库版本无效: %1").arg(query.value(1).toString()));
        return false;
    }

    version = static_cast<int>(storedVersion);
    return true;
}

void DataManager::cleanupDatabaseConnection()
{
    // 调用方持有 m_dbMutex；按 m_db -> m_cacheMutex 的既有锁顺序清理。
    {
        QMutexLocker cacheLocker(&m_cacheMutex);
        m_runtimeCache.clear();
    }

    if (m_db.isOpen()) {
        m_db.close();
    }

    const QString connName = m_connectionName;
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connName);
    m_initialized = false;
    m_schemaVersion = -1;
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
            case 3:
                success = upgradeToVersion3();
                break;
            case 4:
                success = upgradeToVersion4();
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
            if (!executeSql(trimmed, "创建运行时数据索引")) {
                return false;
            }
        }
    }
    
    indexSqls = QString(SchemaDefinitions::CREATE_LOG_INDEXES).split(';', Qt::SkipEmptyParts);
    for (const QString& sql : indexSqls) {
        QString trimmed = sql.trimmed();
        if (!trimmed.isEmpty()) {
            if (!executeSql(trimmed, "创建日志索引")) {
                return false;
            }
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
    return executeSql("CREATE INDEX IF NOT EXISTS idx_runtime_var_time ON runtime_data(variable_name, timestamp)",
                      "创建复合索引").success;
}

bool DataManager::upgradeToVersion3()
{
    auto result = executeSql(SchemaDefinitions::ADD_RUNTIME_QUALITY_V3,
                             "为 runtime_data 添加质量字段");
    if (!result) return false;

    result = executeSql(SchemaDefinitions::ADD_RUNTIME_VALUE_VALID_V3,
                        "为 runtime_data 添加值有效性字段");
    return result.success;
}

bool DataManager::upgradeToVersion4()
{
    auto result = executeSql(SchemaDefinitions::ADD_RUNTIME_ORIGIN_V4,
                             "为 runtime_data 添加来源字段");
    if (!result) return false;

    result = executeSql(SchemaDefinitions::ADD_RUNTIME_ERROR_CODE_V4,
                        "为 runtime_data 添加错误码字段");
    if (!result) return false;

    result = executeSql(SchemaDefinitions::ADD_RUNTIME_ERROR_TEXT_V4,
                        "为 runtime_data 添加错误文本字段");
    return result.success;
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
        // 注意：不在这里 emit databaseError，因为调用方通常持有 m_dbMutex。
        // 调用方应在释放锁后自行 emit。
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
    const QSqlError error = query.lastError();
    const QString sqlTemplate = query.lastQuery().simplified();

    // 仅记录参数元数据，不记录绑定值。
    const QMap<QString, QVariant> boundValues = query.boundValues();
    QString params;
    for (auto it = boundValues.begin(); it != boundValues.end(); ++it) {
        if (!params.isEmpty()) params += ", ";
        const QVariant& value = it.value();
        const QString typeName = value.typeName()
            ? QString::fromLatin1(value.typeName())
            : QStringLiteral("unknown");
        const int length = value.isNull() ? 0 : value.toString().size();
        params += QStringLiteral("%1<type=%2,len=%3>")
                      .arg(it.key(), typeName)
                      .arg(length);
    }

    const QString logMsg = QStringLiteral("SQL 执行失败 [%1]\n"
                             "  SQL 模板: %2\n"
                             "  参数: %3\n"
                             "  错误码: %4\n"
                             "  错误信息: %5")
                     .arg(description)
                     .arg(sqlTemplate.isEmpty() ? QStringLiteral("(未知)") : sqlTemplate)
                     .arg(params.isEmpty() ? QStringLiteral("(无)") : params)
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

    QueryResult result;
    bool shouldEmit = false;

    {
        QMutexLocker dbLocker(&m_dbMutex);

        QSqlQuery query(m_db);
        query.prepare(R"(
            INSERT INTO runtime_data (
                timestamp, variable_name, value, unit, quality, value_valid,
                origin, error_code, error_text)
            VALUES (
                COALESCE(:timestamp, CURRENT_TIMESTAMP), :name, :value, :unit,
                :quality, 1, '', '', '')
        )");
        query.bindValue(":timestamp", QDateTime::currentDateTimeUtc());
        query.bindValue(":name", varName);
        query.bindValue(":value", value);
        query.bindValue(":unit", unit);
        query.bindValue(":quality", runtimePointQualityToString(RuntimePointQuality::Good));

        result = executeQuery(query, "记录运行数据");

        if (result.success) {
            QMutexLocker cacheLocker(&m_cacheMutex);
            m_runtimeCache[varName] = value;
            shouldEmit = true;
        }
    }
    // 锁已释放

    if (shouldEmit) {
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
        INSERT INTO runtime_data (
            timestamp, variable_name, value, unit, quality, value_valid,
            origin, error_code, error_text)
        VALUES (
            COALESCE(:timestamp, CURRENT_TIMESTAMP), :name, :value, :unit,
            :quality, :valueValid, :origin, :errorCode, :errorText)
    )");
    
    QMap<QString, double> updatedValues;
    QueryResult result;
    
    for (const QVariantMap& record : records) {
        QString varName = record.value("varName").toString();
        double value = record.value("value").toDouble();
        QString unit = record.value("unit").toString();
        const bool valueValid = record.value("valueValid", true).toBool();
        const RuntimePointQuality quality = runtimePointQualityFromString(
            record.value("quality", QStringLiteral("Good")).toString());
        const QString origin = runtimeOriginFromRecord(record);
        const QString errorCode = runtimeErrorCodeFromRecord(record);
        const QString errorText = runtimeErrorTextFromRecord(record);

        // 可选：允许外部提供真实采样时间（用于历史查询/回放）
        // 如果未提供，则由 SQL 中的 CURRENT_TIMESTAMP 自动填充
        const QVariant tsVar = record.value("timestamp");
        QDateTime ts;
        if (tsVar.isValid()) {
            ts = tsVar.toDateTime();
        }

        query.bindValue(":timestamp", ts.isValid() ? QVariant(ts) : QVariant());
        query.bindValue(":name", varName);
        query.bindValue(":value", valueValid ? QVariant(value) : QVariant());
        query.bindValue(":unit", unit);
        query.bindValue(":quality", runtimePointQualityToString(quality));
        query.bindValue(":valueValid", valueValid ? 1 : 0);
        query.bindValue(":origin", origin);
        query.bindValue(":errorCode", errorCode);
        query.bindValue(":errorText", errorText);
        
        if (query.exec()) {
            if (valueValid) {
                updatedValues[varName] = value;
            }
        } else {
            logSqlError(query, QStringLiteral("批量记录运行数据"));
            result.errorCode = query.lastError().nativeErrorCode();
            result.errorText = query.lastError().text();
            m_db.rollback();
            return result;
        }
    }
    
    QMap<QString, double> emitValues; // 在锁外 emit 的数据

    if (m_db.commit()) {
        result.success = true;
        result.affectedRows = records.size();

        // 更新缓存
        {
            QMutexLocker cacheLocker(&m_cacheMutex);
            for (auto it = updatedValues.begin(); it != updatedValues.end(); ++it) {
                m_runtimeCache[it.key()] = it.value();
            }
        }

        emitValues = std::move(updatedValues);
        LOG_DEBUG(QString("批量记录运行数据: %1 条成功").arg(records.size()));
    } else {
        m_db.rollback();
        result.errorText = "提交事务失败: " + m_db.lastError().text();
        LOG_ERROR(result.errorText);
    }

    dbLocker.unlock(); // 在 emit 前释放锁

    for (auto it = emitValues.begin(); it != emitValues.end(); ++it) {
        emit dataUpdated(it.key(), it.value());
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

    const QDateTime startUtc = start.isValid() ? start.toUTC() : start;
    const QDateTime endUtc = end.isValid() ? end.toUTC() : end;
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT id, timestamp, variable_name, value, unit, value_valid, quality,
               origin, error_code, error_text
        FROM runtime_data
        WHERE variable_name = :name
          AND timestamp BETWEEN :start AND :end
        ORDER BY timestamp ASC, id ASC
    )");
    query.bindValue(":name", varName);
    query.bindValue(":start", startUtc);
    query.bindValue(":end", endUtc);
    
    if (query.exec()) {
        while (query.next()) {
            RuntimeRecord record;
            record.id = query.value(0).toLongLong();
            record.timestamp = query.value(1).toDateTime();
            record.variableName = query.value(2).toString();
            record.valueValid = query.value(5).toBool() && !query.value(3).isNull();
            if (record.valueValid) {
                record.value = query.value(3).toDouble();
            }
            record.unit = query.value(4).toString();
            record.quality = runtimePointQualityFromString(query.value(6).toString());
            record.origin = query.value(7).toString();
            record.errorCode = query.value(8).toString();
            record.errorText = query.value(9).toString();
            results.append(record);
        }
    } else {
        logSqlError(query, "查询历史数据");
    }
    
    return results;
}

RuntimeHistoryPage DataManager::queryHistoryPage(const QString& varName,
                                                  const QDateTime& start,
                                                  const QDateTime& end,
                                                  int pageSize,
                                                  const RuntimeHistoryCursor& cursor)
{
    RuntimeHistoryPage page;

    if (!m_initialized) {
        LOG_WARN("DataManager 未初始化，无法分页查询历史数据");
        page.status = RuntimeHistoryPageStatus::NotInitialized;
        page.errorText = "DataManager 未初始化";
        return page;
    }

    if (pageSize <= 0) {
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = "InvalidPageSize";
        page.errorText = "分页大小必须大于 0";
        return page;
    }

    const QDateTime startUtc = start.toUTC();
    const QDateTime endUtc = end.toUTC();
    QMutexLocker dbLocker(&m_dbMutex);

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT id, timestamp, variable_name, value, unit, value_valid, quality,
               origin, error_code, error_text
        FROM runtime_data
        WHERE variable_name = :name
          AND timestamp >= :start
          AND timestamp <= :end
          AND (:hasCursor = 0
               OR timestamp > :cursorTimestamp
               OR (timestamp = :cursorTimestamp AND id > :cursorId))
        ORDER BY timestamp ASC, id ASC
        LIMIT :limit
    )");
    query.bindValue(":name", varName);
    query.bindValue(":start", startUtc);
    query.bindValue(":end", endUtc);
    query.bindValue(":hasCursor", cursor.isValid() ? 1 : 0);
    query.bindValue(":cursorTimestamp", cursor.timestamp.toUTC());
    query.bindValue(":cursorId", cursor.id);
    query.bindValue(":limit", pageSize);

    if (!query.exec()) {
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = query.lastError().nativeErrorCode();
        page.errorText = query.lastError().text();
        logSqlError(query, "分页查询历史数据");
        return page;
    }

    while (query.next()) {
        RuntimeRecord record;
        record.id = query.value(0).toLongLong();
        record.timestamp = query.value(1).toDateTime();
        record.variableName = query.value(2).toString();
        record.valueValid = query.value(5).toBool() && !query.value(3).isNull();
        if (record.valueValid) {
            record.value = query.value(3).toDouble();
        }
        record.unit = query.value(4).toString();
        record.quality = runtimePointQualityFromString(query.value(6).toString());
        record.origin = query.value(7).toString();
        record.errorCode = query.value(8).toString();
        record.errorText = query.value(9).toString();
        page.records.append(record);
    }

    page.status = RuntimeHistoryPageStatus::Success;
    if (!page.records.isEmpty()) {
        page.nextCursor.timestamp = page.records.constLast().timestamp.toUTC();
        page.nextCursor.id = page.records.constLast().id;
    } else {
        page.nextCursor = cursor;
    }

    // 不使用 LIMIT pageSize + 1，保证每次数据请求都不超过调用方指定的页大小。
    // 仅在整页时做一次 EXISTS 检查来判断是否还有下一页。
    if (page.records.size() == pageSize) {
        QSqlQuery moreQuery(m_db);
        moreQuery.prepare(R"(
            SELECT 1
            FROM runtime_data
            WHERE variable_name = :name
              AND timestamp >= :start
              AND timestamp <= :end
              AND (:hasCursor = 0
                   OR timestamp > :cursorTimestamp
                   OR (timestamp = :cursorTimestamp AND id > :cursorId))
            LIMIT 1
        )");
        moreQuery.bindValue(":name", varName);
        moreQuery.bindValue(":start", startUtc);
        moreQuery.bindValue(":end", endUtc);
        moreQuery.bindValue(":hasCursor", page.nextCursor.isValid() ? 1 : 0);
        moreQuery.bindValue(":cursorTimestamp", page.nextCursor.timestamp);
        moreQuery.bindValue(":cursorId", page.nextCursor.id);
        if (!moreQuery.exec()) {
            page.status = RuntimeHistoryPageStatus::SqlError;
            page.errorCode = moreQuery.lastError().nativeErrorCode();
            page.errorText = moreQuery.lastError().text();
            logSqlError(moreQuery, "判断历史分页末页");
            page.records.clear();
            page.hasMore = false;
            return page;
        }
        page.hasMore = moreQuery.next();
    }

    return page;
}

RuntimeHistoryPage DataManager::queryLatestHistoryPage(const QString& varName,
                                                        int maxCount,
                                                        int pageSize,
                                                        const RuntimeHistoryCursor& cursor,
                                                        const QDateTime& end)
{
    RuntimeHistoryPage page;

    if (!m_initialized) {
        LOG_WARN("DataManager 未初始化，无法分页查询最近历史数据");
        page.status = RuntimeHistoryPageStatus::NotInitialized;
        page.errorText = "DataManager 未初始化";
        return page;
    }

    if (maxCount <= 0 || pageSize <= 0) {
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = "InvalidPageSize";
        page.errorText = "最近记录数和分页大小必须大于 0";
        return page;
    }

    QMutexLocker dbLocker(&m_dbMutex);

    const bool hasEnd = end.isValid();
    const QDateTime endUtc = hasEnd ? end.toUTC() : end;

    // 子查询只选最近 maxCount 个 id；外层按升序分页，避免把回退数据一次
    // 读入内存，同时保持与普通历史分页完全相同的游标语义。
    const QString latestIds = R"(
        SELECT id
        FROM runtime_data
        WHERE variable_name = :latestName
          AND (:hasEnd = 0 OR timestamp <= :end)
        ORDER BY timestamp DESC, id DESC
        LIMIT :maxCount
    )";
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, timestamp, variable_name, value, unit, value_valid, quality, "
        "origin, error_code, error_text "
        "FROM runtime_data WHERE id IN (") + latestIds + QStringLiteral(
        ") AND (:hasEnd = 0 OR timestamp <= :end) "
        "AND (:hasCursor = 0 "
        "OR timestamp > :cursorTimestamp "
        "OR (timestamp = :cursorTimestamp AND id > :cursorId)) "
        "ORDER BY timestamp ASC, id ASC LIMIT :limit"));
    query.bindValue(":latestName", varName);
    query.bindValue(":hasEnd", hasEnd ? 1 : 0);
    query.bindValue(":end", endUtc);
    query.bindValue(":maxCount", maxCount);
    query.bindValue(":hasCursor", cursor.isValid() ? 1 : 0);
    query.bindValue(":cursorTimestamp", cursor.timestamp.toUTC());
    query.bindValue(":cursorId", cursor.id);
    query.bindValue(":limit", pageSize);

    if (!query.exec()) {
        page.status = RuntimeHistoryPageStatus::SqlError;
        page.errorCode = query.lastError().nativeErrorCode();
        page.errorText = query.lastError().text();
        logSqlError(query, "分页查询最近历史数据");
        return page;
    }

    while (query.next()) {
        RuntimeRecord record;
        record.id = query.value(0).toLongLong();
        record.timestamp = query.value(1).toDateTime();
        record.variableName = query.value(2).toString();
        record.valueValid = query.value(5).toBool() && !query.value(3).isNull();
        if (record.valueValid) {
            record.value = query.value(3).toDouble();
        }
        record.unit = query.value(4).toString();
        record.quality = runtimePointQualityFromString(query.value(6).toString());
        record.origin = query.value(7).toString();
        record.errorCode = query.value(8).toString();
        record.errorText = query.value(9).toString();
        page.records.append(record);
    }

    page.status = RuntimeHistoryPageStatus::Success;
    if (!page.records.isEmpty()) {
        page.nextCursor.timestamp = page.records.constLast().timestamp.toUTC();
        page.nextCursor.id = page.records.constLast().id;
    } else {
        page.nextCursor = cursor;
    }

    if (page.records.size() == pageSize) {
        QSqlQuery moreQuery(m_db);
        moreQuery.prepare(QStringLiteral(
            "SELECT 1 FROM runtime_data WHERE id IN (") + latestIds + QStringLiteral(
            ") AND (:hasEnd = 0 OR timestamp <= :end) "
            "AND (:hasCursor = 0 "
            "OR timestamp > :cursorTimestamp "
            "OR (timestamp = :cursorTimestamp AND id > :cursorId)) LIMIT 1"));
        moreQuery.bindValue(":latestName", varName);
        moreQuery.bindValue(":hasEnd", hasEnd ? 1 : 0);
        moreQuery.bindValue(":end", endUtc);
        moreQuery.bindValue(":maxCount", maxCount);
        moreQuery.bindValue(":hasCursor", page.nextCursor.isValid() ? 1 : 0);
        moreQuery.bindValue(":cursorTimestamp", page.nextCursor.timestamp);
        moreQuery.bindValue(":cursorId", page.nextCursor.id);
        if (!moreQuery.exec()) {
            page.status = RuntimeHistoryPageStatus::SqlError;
            page.errorCode = moreQuery.lastError().nativeErrorCode();
            page.errorText = moreQuery.lastError().text();
            logSqlError(moreQuery, "判断最近历史分页末页");
            page.records.clear();
            page.hasMore = false;
            return page;
        }
        page.hasMore = moreQuery.next();
    }

    return page;
}

RuntimeHistoryCount DataManager::countHistory(const QString& varName,
                                               const QDateTime& start,
                                               const QDateTime& end)
{
    RuntimeHistoryCount result;

    if (!m_initialized) {
        LOG_WARN("DataManager 未初始化，无法统计历史数据");
        result.status = RuntimeHistoryPageStatus::NotInitialized;
        result.errorText = "DataManager 未初始化";
        return result;
    }

    const QDateTime startUtc = start.isValid() ? start.toUTC() : start;
    const QDateTime endUtc = end.isValid() ? end.toUTC() : end;
    QMutexLocker dbLocker(&m_dbMutex);

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT COUNT(*)
        FROM runtime_data
        WHERE variable_name = :name
          AND timestamp >= :start
          AND timestamp <= :end
    )");
    query.bindValue(":name", varName);
    query.bindValue(":start", startUtc);
    query.bindValue(":end", endUtc);

    if (!query.exec() || !query.next()) {
        result.status = RuntimeHistoryPageStatus::SqlError;
        result.errorCode = query.lastError().nativeErrorCode();
        result.errorText = query.lastError().text();
        logSqlError(query, "统计历史数据");
        return result;
    }

    result.status = RuntimeHistoryPageStatus::Success;
    result.count = query.value(0).toLongLong();
    return result;
}

RuntimeHistoryCount DataManager::countLatestHistory(const QString& varName,
                                                     int maxCount,
                                                     const QDateTime& end)
{
    RuntimeHistoryCount result;

    if (!m_initialized) {
        LOG_WARN("DataManager 未初始化，无法统计最近历史数据");
        result.status = RuntimeHistoryPageStatus::NotInitialized;
        result.errorText = "DataManager 未初始化";
        return result;
    }

    if (maxCount <= 0) {
        result.status = RuntimeHistoryPageStatus::SqlError;
        result.errorCode = "InvalidMaxCount";
        result.errorText = "最近记录数必须大于 0";
        return result;
    }

    QMutexLocker dbLocker(&m_dbMutex);
    const bool hasEnd = end.isValid();
    const QDateTime endUtc = hasEnd ? end.toUTC() : end;
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT COUNT(*)
        FROM (
            SELECT id
            FROM runtime_data
            WHERE variable_name = :name
              AND (:hasEnd = 0 OR timestamp <= :end)
            ORDER BY timestamp DESC, id DESC
            LIMIT :limit
        )
    )");
    query.bindValue(":name", varName);
    query.bindValue(":hasEnd", hasEnd ? 1 : 0);
    query.bindValue(":end", endUtc);
    query.bindValue(":limit", maxCount);

    if (!query.exec() || !query.next()) {
        result.status = RuntimeHistoryPageStatus::SqlError;
        result.errorCode = query.lastError().nativeErrorCode();
        result.errorText = query.lastError().text();
        logSqlError(query, "统计最近历史数据");
        return result;
    }

    result.status = RuntimeHistoryPageStatus::Success;
    result.count = query.value(0).toLongLong();
    return result;
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
        SELECT id, timestamp, variable_name, value, unit, value_valid, quality,
               origin, error_code, error_text
        FROM runtime_data
        WHERE variable_name = :name
        ORDER BY timestamp DESC, id DESC
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
            record.valueValid = query.value(5).toBool() && !query.value(3).isNull();
            if (record.valueValid) {
                record.value = query.value(3).toDouble();
            }
            record.unit = query.value(4).toString();
            record.quality = runtimePointQualityFromString(query.value(6).toString());
            record.origin = query.value(7).toString();
            record.errorCode = query.value(8).toString();
            record.errorText = query.value(9).toString();
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
        SELECT id, timestamp, variable_name, value, unit, value_valid, quality,
               origin, error_code, error_text
        FROM runtime_data
        WHERE variable_name = :name
          AND timestamp > :since
        ORDER BY timestamp ASC, id ASC
    )");
    query.bindValue(":name", varName);
    query.bindValue(":since", since);
    
    if (query.exec()) {
        while (query.next()) {
            RuntimeRecord record;
            record.id = query.value(0).toLongLong();
            record.timestamp = query.value(1).toDateTime();
            record.variableName = query.value(2).toString();
            record.valueValid = query.value(5).toBool() && !query.value(3).isNull();
            if (record.valueValid) {
                record.value = query.value(3).toDouble();
            }
            record.unit = query.value(4).toString();
            record.quality = runtimePointQualityFromString(query.value(6).toString());
            record.origin = query.value(7).toString();
            record.errorCode = query.value(8).toString();
            record.errorText = query.value(9).toString();
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
          AND value_valid = 1
          AND value IS NOT NULL
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
    if (!m_initialized || !m_db.isOpen() || retentionDays <= 0) {
        LOG_WARN("DataManager 未初始化或保留天数无效，无法清理数据");
        return -1;
    }
    
    QMutexLocker dbLocker(&m_dbMutex);

    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-retentionDays);
    if (!m_db.transaction()) {
        LOG_ERROR("无法开启数据清理事务: " + m_db.lastError().text());
        return -1;
    }

    QSqlQuery runtimeQuery(m_db);
    runtimeQuery.prepare("DELETE FROM runtime_data WHERE julianday(timestamp) < julianday(:cutoff)");
    runtimeQuery.bindValue(":cutoff", cutoff);
    if (!runtimeQuery.exec()) {
        logSqlError(runtimeQuery, "清理运行时数据");
        m_db.rollback();
        return -1;
    }

    const int runtimeDeleted = runtimeQuery.numRowsAffected();

    QSqlQuery logsQuery(m_db);
    logsQuery.prepare("DELETE FROM system_logs WHERE julianday(timestamp) < julianday(:cutoff)");
    logsQuery.bindValue(":cutoff", cutoff);
    if (!logsQuery.exec()) {
        logSqlError(logsQuery, "清理系统日志");
        m_db.rollback();
        return -1;
    }

    const int logsDeleted = logsQuery.numRowsAffected();
    if (!m_db.commit()) {
        const QString error = m_db.lastError().text();
        m_db.rollback();
        LOG_ERROR("提交数据清理事务失败: " + error);
        return -1;
    }

    const int totalDeleted = runtimeDeleted + logsDeleted;
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
