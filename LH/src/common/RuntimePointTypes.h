#ifndef RUNTIME_POINT_TYPES_H
#define RUNTIME_POINT_TYPES_H

#include <QString>
#include <QDateTime>
#include <QVariantMap>
#include <QList>
#include <QMetaType>

/**
 * @brief RuntimePointKind
 * 运行点类型分类
 */
enum class RuntimePointKind {
    Variable,   ///< DSL 变量
    Parameter,  ///< 可调参数
    Status,     ///< 状态量
    Alarm,      ///< 告警量
    Resource    ///< 硬件资源
};

/**
 * @brief RuntimePointAccess
 * 运行点读写权限
 */
enum class RuntimePointAccess {
    ReadOnly,
    WriteOnly,
    ReadWrite
};

/**
 * @brief RuntimePointQuality
 * 运行点数据质量标识
 */
enum class RuntimePointQuality {
    Unknown,    ///< 初始/未知
    Good,       ///< 正常
    Simulated,  ///< 仿真值
    Stale,      ///< 过期
    Bad,        ///< 异常
    Offline     ///< 离线
};

/**
 * @brief RuntimePointDefinition
 * 运行点定义，描述一个可被监控/参数/下载/通信共用的点位元数据
 */
struct RuntimePointDefinition
{
    QString id;                  ///< 唯一标识（UUID 或业务 key）
    QString name;                ///< 显示名称
    RuntimePointKind kind;       ///< 点类型
    QString dataType;            ///< 数据类型（REAL, INT, BOOL 等）
    QString unit;                ///< 物理单位
    RuntimePointAccess access = RuntimePointAccess::ReadWrite;
    QString sourceModule;        ///< 来源模块标识（如 "dsl", "monitor", "hardware"）
    QString bindingPath;         ///< 绑定路径（信号路径、通道名等）
    QVariantMap addressing;      ///< 寄存器/地址信息
    QVariant defaultValue;       ///< 默认值
    QVariantMap metadata;        ///< 扩展元数据
};

/**
 * @brief RuntimePointValue
 * 运行点当前值，携带质量戳和时间戳
 */
struct RuntimePointValue
{
    QString pointId;
    QVariant value;
    RuntimePointQuality quality = RuntimePointQuality::Unknown;
    QDateTime timestamp;
    QString origin;              ///< 值来源标识
    quint64 sequence = 0;        ///< 单调递增序号
};

/**
 * @brief RuntimePointSnapshot
 * 运行点快照 = 定义 + 当前值
 */
struct RuntimePointSnapshot
{
    RuntimePointDefinition definition;
    RuntimePointValue current;
};

// ========== 从 ProjectRuntimeConfig 各字段转换为 RuntimePointDefinition ==========

#include "ConfigTypes.h"

namespace RuntimePointConverter {

/**
 * @brief VariableDefinition -> RuntimePointDefinition
 */
inline RuntimePointDefinition fromVariable(const VariableDefinition& v)
{
    RuntimePointDefinition pt;
    pt.id = v.id;
    pt.name = v.name;
    pt.kind = RuntimePointKind::Variable;
    pt.dataType = v.dataType;
    pt.access = v.readOnly ? RuntimePointAccess::ReadOnly : RuntimePointAccess::ReadWrite;
    pt.sourceModule = QStringLiteral("dsl");
    pt.bindingPath = v.binding;
    pt.defaultValue = v.defaultValue;
    pt.metadata = v.metadata;
    return pt;
}

/**
 * @brief ParameterDefinition -> RuntimePointDefinition
 */
inline RuntimePointDefinition fromParameter(const ParameterDefinition& p)
{
    RuntimePointDefinition pt;
    pt.id = p.id;
    pt.name = p.name;
    pt.kind = RuntimePointKind::Parameter;
    pt.dataType = p.dataType;
    pt.unit = p.unit;
    pt.access = p.onlineEditable ? RuntimePointAccess::ReadWrite : RuntimePointAccess::ReadOnly;
    pt.sourceModule = QStringLiteral("parameter");
    pt.defaultValue = p.defaultValue;
    if (!p.minValue.isEmpty() || !p.maxValue.isEmpty()) {
        pt.metadata.insert(QStringLiteral("minValue"), p.minValue);
        pt.metadata.insert(QStringLiteral("maxValue"), p.maxValue);
    }
    pt.metadata.insert(QStringLiteral("confirmed"), p.confirmed);
    for (auto it = p.metadata.constBegin(); it != p.metadata.constEnd(); ++it)
        pt.metadata.insert(it.key(), it.value());
    return pt;
}

/**
 * @brief HardwareResourceBinding -> RuntimePointDefinition
 */
inline RuntimePointDefinition fromResource(const HardwareResourceBinding& r)
{
    RuntimePointDefinition pt;
    pt.id = r.id;
    pt.name = r.resourceName;
    pt.kind = RuntimePointKind::Resource;
    pt.sourceModule = QStringLiteral("hardware");
    pt.bindingPath = r.channel;
    pt.addressing.insert(QStringLiteral("resourceType"), r.resourceType);
    pt.addressing.insert(QStringLiteral("channel"), r.channel);
    pt.addressing.insert(QStringLiteral("owner"), r.owner);
    pt.addressing.insert(QStringLiteral("exclusive"), r.exclusive);
    pt.metadata = r.metadata;
    return pt;
}

/**
 * @brief MonitorProviderRuntimeConfig -> RuntimePointDefinition
 */
inline RuntimePointDefinition fromProvider(const MonitorProviderRuntimeConfig& prov)
{
    RuntimePointDefinition pt;
    pt.id = prov.id;
    pt.name = prov.channelName;
    pt.kind = RuntimePointKind::Status;
    pt.unit = prov.unit;
    pt.access = RuntimePointAccess::ReadOnly;
    pt.sourceModule = QStringLiteral("monitor");
    pt.bindingPath = prov.channelName;
    pt.addressing.insert(QStringLiteral("periodMs"), prov.periodMs);
    pt.addressing.insert(QStringLiteral("priority"), prov.priority);
    pt.metadata = prov.metadata;
    return pt;
}

/**
 * @brief DslMappingEntry -> RuntimePointDefinition
 */
inline RuntimePointDefinition fromDslMapping(const DslMappingEntry& m)
{
    RuntimePointDefinition pt;
    pt.id = m.id;
    pt.name = m.snippetName.isEmpty() ? m.channelName : m.snippetName;
    pt.kind = RuntimePointKind::Status;
    pt.unit = m.unit;
    pt.access = RuntimePointAccess::ReadOnly;
    pt.sourceModule = QStringLiteral("dsl_mapping");
    pt.bindingPath = m.signalPath;
    pt.addressing.insert(QStringLiteral("snippetId"), m.snippetId);
    pt.addressing.insert(QStringLiteral("channelName"), m.channelName);
    pt.addressing.insert(QStringLiteral("periodMs"), m.periodMs);
    pt.addressing.insert(QStringLiteral("lineNumber"), m.lineNumber);
    pt.metadata = m.metadata;
    return pt;
}

/**
 * @brief ProjectRuntimeConfig -> QList<RuntimePointDefinition>
 *
 * 将项目配置中的 variables / parameters / resources / providers / dslMappings
 * 全部转换为统一点定义列表。原有字段保持不变。
 */
inline QList<RuntimePointDefinition> fromProjectConfig(const ProjectRuntimeConfig& cfg)
{
    QList<RuntimePointDefinition> points;
    points.reserve(cfg.variables.size()
                   + cfg.parameters.size()
                   + cfg.resources.size()
                   + cfg.providers.size()
                   + cfg.dslMappings.size());

    for (const auto& v : cfg.variables)
        points.append(fromVariable(v));

    for (const auto& p : cfg.parameters)
        points.append(fromParameter(p));

    for (const auto& r : cfg.resources)
        points.append(fromResource(r));

    for (const auto& prov : cfg.providers)
        points.append(fromProvider(prov));

    for (const auto& m : cfg.dslMappings)
        points.append(fromDslMapping(m));

    return points;
}

} // namespace RuntimePointConverter

#endif // RUNTIME_POINT_TYPES_H
