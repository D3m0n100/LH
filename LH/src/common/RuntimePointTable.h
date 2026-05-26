#ifndef RUNTIME_POINT_TABLE_H
#define RUNTIME_POINT_TABLE_H

#include "RuntimePointTypes.h"

#include <QHash>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QMutex>
#include <QMutexLocker>
#include <optional>

/**
 * @brief RuntimePointTable
 * 轻量运行点注册表。
 *
 * 职责：
 *   - 注册 / 批量加载运行点定义
 *   - 根据 id / name / kind 查询
 *   - 更新当前值（线程安全）
 *   - 取快照（definition + current value）
 *
 * 不持有业务逻辑，不依赖 Qt 信号槽，适合被监控、参数、下载、通信层共用。
 */
class RuntimePointTable
{
public:
    RuntimePointTable() = default;

    // ── 注册 ──────────────────────────────────────────────

    /**
     * @brief 注册单个点定义。若 id 已存在则覆盖定义并重置值。
     */
    void registerPoint(const RuntimePointDefinition& def)
    {
        QWriteLocker defLock(&m_defLock);
        QMutexLocker valLock(&m_valMutex);
        m_defs.insert(def.id, def);
        m_nameIndex.insert(def.name, def.id);
        m_kindIndex[static_cast<int>(def.kind)].insert(def.id);
        m_values.insert(def.id, RuntimePointValue{def.id, def.defaultValue, RuntimePointQuality::Unknown, QDateTime(), QStringLiteral("init")});
    }

    /**
     * @brief 批量加载点定义，等价于逐个 registerPoint。
     */
    void loadDefinitions(const QList<RuntimePointDefinition>& defs)
    {
        for (const auto& d : defs)
            registerPoint(d);
    }

    /**
     * @brief 从 ProjectRuntimeConfig 一次性加载全部点。
     */
    void loadFromProjectConfig(const ProjectRuntimeConfig& cfg)
    {
        loadDefinitions(RuntimePointConverter::fromProjectConfig(cfg));
    }

    /**
     * @brief 清空全部注册信息。
     */
    void clear()
    {
        QWriteLocker defLock(&m_defLock);
        QMutexLocker valLock(&m_valMutex);
        m_defs.clear();
        m_nameIndex.clear();
        m_kindIndex.clear();
        m_values.clear();
    }

    // ── 查询定义 ──────────────────────────────────────────

    /**
     * @brief 根据 id 获取点定义。不存在返回 std::nullopt。
     */
    std::optional<RuntimePointDefinition> definition(const QString& id) const
    {
        QReadLocker lock(&m_defLock);
        auto it = m_defs.constFind(id);
        if (it == m_defs.constEnd())
            return std::nullopt;
        return it.value();
    }

    /**
     * @brief 根据 name 获取点定义（返回第一个匹配）。
     */
    std::optional<RuntimePointDefinition> definitionByName(const QString& name) const
    {
        QReadLocker lock(&m_defLock);
        auto it = m_nameIndex.constFind(name);
        if (it == m_nameIndex.constEnd())
            return std::nullopt;
        auto defIt = m_defs.constFind(it.value());
        if (defIt == m_defs.constEnd())
            return std::nullopt;
        return defIt.value();
    }

    /**
     * @brief 按类型过滤，返回该类型的所有点 id。
     */
    QStringList idsByKind(RuntimePointKind kind) const
    {
        QReadLocker lock(&m_defLock);
        auto it = m_kindIndex.constFind(static_cast<int>(kind));
        if (it == m_kindIndex.constEnd())
            return {};
        return it.value().values();
    }

    /**
     * @brief 返回所有已注册点的 id 列表。
     */
    QStringList allIds() const
    {
        QReadLocker lock(&m_defLock);
        return m_defs.keys();
    }

    /**
     * @brief 已注册点数量。
     */
    int count() const
    {
        QReadLocker lock(&m_defLock);
        return m_defs.size();
    }

    bool contains(const QString& id) const
    {
        QReadLocker lock(&m_defLock);
        return m_defs.contains(id);
    }

    // ── 更新值 ────────────────────────────────────────────

    /**
     * @brief 更新单个点的值。点必须已注册。
     * @return true 更新成功，false 点不存在。
     */
    bool updateValue(const QString& id, const QVariant& value,
                     RuntimePointQuality quality = RuntimePointQuality::Good,
                     const QString& origin = QString())
    {
        QMutexLocker lock(&m_valMutex);
        auto it = m_values.find(id);
        if (it == m_values.end())
            return false;
        it->value = value;
        it->quality = quality;
        it->timestamp = QDateTime::currentDateTime();
        it->origin = origin;
        it->sequence++;
        return true;
    }

    /**
     * @brief 批量更新值。
     * @return 成功更新的点数量。
     */
    int updateValues(const QHash<QString, QVariant>& updates,
                     RuntimePointQuality quality = RuntimePointQuality::Good,
                     const QString& origin = QString())
    {
        int ok = 0;
        QMutexLocker lock(&m_valMutex);
        for (auto it = updates.constBegin(); it != updates.constEnd(); ++it) {
            auto vit = m_values.find(it.key());
            if (vit == m_values.end())
                continue;
            vit->value = it.value();
            vit->quality = quality;
            vit->timestamp = QDateTime::currentDateTime();
            vit->origin = origin;
            vit->sequence++;
            ok++;
        }
        return ok;
    }

    /**
     * @brief 设置指定点的数据质量（不改变值）。
     */
    void setQuality(const QString& id, RuntimePointQuality quality)
    {
        QMutexLocker lock(&m_valMutex);
        auto it = m_values.find(id);
        if (it != m_values.end())
            it->quality = quality;
    }

    // ── 读取值 ────────────────────────────────────────────

    /**
     * @brief 获取单个点的当前值快照。点不存在返回 std::nullopt。
     */
    std::optional<RuntimePointSnapshot> snapshot(const QString& id) const
    {
        QReadLocker defLock(&m_defLock);
        QMutexLocker valLock(&m_valMutex);
        auto dit = m_defs.constFind(id);
        if (dit == m_defs.constEnd())
            return std::nullopt;
        auto vit = m_values.constFind(id);
        RuntimePointValue val;
        if (vit != m_values.constEnd())
            val = vit.value();
        return RuntimePointSnapshot{dit.value(), val};
    }

    /**
     * @brief 获取所有点的快照列表。
     */
    QList<RuntimePointSnapshot> allSnapshots() const
    {
        QReadLocker defLock(&m_defLock);
        QMutexLocker valLock(&m_valMutex);
        QList<RuntimePointSnapshot> result;
        result.reserve(m_defs.size());
        for (auto it = m_defs.constBegin(); it != m_defs.constEnd(); ++it) {
            RuntimePointValue val;
            auto vit = m_values.constFind(it.key());
            if (vit != m_values.constEnd())
                val = vit.value();
            result.append(RuntimePointSnapshot{it.value(), val});
        }
        return result;
    }

    /**
     * @brief 获取指定类型的全部快照。
     */
    QList<RuntimePointSnapshot> snapshotsByKind(RuntimePointKind kind) const
    {
        QReadLocker defLock(&m_defLock);
        QMutexLocker valLock(&m_valMutex);
        QList<RuntimePointSnapshot> result;
        auto kit = m_kindIndex.constFind(static_cast<int>(kind));
        if (kit == m_kindIndex.constEnd())
            return result;
        const auto& ids = kit.value();
        result.reserve(ids.size());
        for (const auto& id : ids) {
            auto dit = m_defs.constFind(id);
            if (dit == m_defs.constEnd())
                continue;
            RuntimePointValue val;
            auto vit = m_values.constFind(id);
            if (vit != m_values.constEnd())
                val = vit.value();
            result.append(RuntimePointSnapshot{dit.value(), val});
        }
        return result;
    }

private:
    // 定义表（读写锁保护）
    mutable QReadWriteLock m_defLock;
    QHash<QString, RuntimePointDefinition> m_defs;             ///< id -> 定义
    QHash<QString, QString> m_nameIndex;                       ///< name -> id（首个注册的）
    QHash<int, QSet<QString>> m_kindIndex;                     ///< kind -> {id, ...}

    // 值表（互斥锁保护，写操作可能较频繁）
    mutable QMutex m_valMutex;
    QHash<QString, RuntimePointValue> m_values;                ///< id -> 当前值
};

#endif // RUNTIME_POINT_TABLE_H
