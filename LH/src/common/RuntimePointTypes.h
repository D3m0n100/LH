#ifndef RUNTIME_POINT_TYPES_H
#define RUNTIME_POINT_TYPES_H

#include <QString>
#include <QDateTime>
#include <QVariantMap>
#include <QList>
#include <QMetaType>
#include <QtGlobal>
#include <QJsonObject>

#include "../communication/CommTypes.h"

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

inline QString runtimePointAccessToString(RuntimePointAccess access)
{
    switch (access) {
    case RuntimePointAccess::ReadOnly:
        return QStringLiteral("ReadOnly");
    case RuntimePointAccess::WriteOnly:
        return QStringLiteral("WriteOnly");
    case RuntimePointAccess::ReadWrite:
    default:
        return QStringLiteral("ReadWrite");
    }
}

inline RuntimePointAccess runtimePointAccessFromString(const QString& accessText)
{
    const QString normalized = accessText.trimmed().toLower();
    if (normalized == QStringLiteral("readonly") || normalized == QStringLiteral("read-only")) {
        return RuntimePointAccess::ReadOnly;
    }
    if (normalized == QStringLiteral("writeonly") || normalized == QStringLiteral("write-only")) {
        return RuntimePointAccess::WriteOnly;
    }
    return RuntimePointAccess::ReadWrite;
}

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

struct OpcTagDefinition
{
    QString tagName;
    QString dataType;
    QString tagDescription;
    QString tagGroup;
    QString tagAccess;
    QString server;
    QString item;
    QString ioGroup;
    QString unit;
    QVariantMap addressing;
    QVariantMap metadata;

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["tagName"] = tagName;
        obj["dataType"] = dataType;
        obj["tagDescription"] = tagDescription;
        obj["tagGroup"] = tagGroup;
        obj["tagAccess"] = tagAccess;
        obj["server"] = server;
        obj["item"] = item;
        obj["ioGroup"] = ioGroup;
        obj["unit"] = unit;
        obj["addressing"] = QJsonObject::fromVariantMap(addressing);
        obj["metadata"] = QJsonObject::fromVariantMap(metadata);
        return obj;
    }

    static OpcTagDefinition fromJson(const QJsonObject& obj)
    {
        OpcTagDefinition tag;
        tag.tagName = obj["tagName"].toString();
        tag.dataType = obj["dataType"].toString();
        tag.tagDescription = obj["tagDescription"].toString();
        tag.tagGroup = obj["tagGroup"].toString();
        tag.tagAccess = obj["tagAccess"].toString();
        tag.server = obj["server"].toString();
        tag.item = obj["item"].toString();
        tag.ioGroup = obj["ioGroup"].toString();
        tag.unit = obj["unit"].toString();
        tag.addressing = obj["addressing"].toObject().toVariantMap();
        tag.metadata = obj["metadata"].toObject().toVariantMap();
        return tag;
    }
};

/**
 * @brief RuntimePointDefinition
 * 运行点定义，描述一个可被监控/参数/下载/通信共用的点位元数据
 */
struct RuntimePointDefinition
{
    QString id;                  ///< 唯一标识（UUID 或业务 key）
    QString name;                ///< 显示名称
    RuntimePointKind kind = RuntimePointKind::Variable; ///< 点类型
    QString dataType;            ///< 数据类型（REAL, INT, BOOL 等）
    QString unit;                ///< 物理单位
    RuntimePointAccess access = RuntimePointAccess::ReadWrite;
    QString sourceModule;        ///< 来源模块标识（如 "dsl", "monitor", "hardware"）
    QString bindingPath;         ///< 绑定路径（信号路径、通道名等）
    QVariantMap addressing;      ///< 寄存器/地址信息
    QVariant defaultValue;       ///< 默认值
    QVariantMap metadata;        ///< 扩展元数据
    QString opcTagName;
    QString opcTagDescription;
    QString opcTagGroup;
    QString opcServerName;
    QString opcItemName;
    QString opcIoGroup;
    QString opcTagAccess;
    QVariantMap opcMetadata;
};

/**
 * @brief RuntimePointAddressing
 * 统一的运行点地址/映射描述，作为 runtime_points.json 中 addressing 字段的稳定 schema。
 *
 * 说明：
 * - 对于尚未绑定真实控制器地址的点，address 允许为 -1
 * - 旧代码仍然可按 QVariantMap 读取，但输出字段应保持一致
 */
struct RuntimePointAddressing
{
    QString schemaVersion = QStringLiteral("lh.runtimePointAddressing.v1");
    QString role;                 ///< variable / parameter / resource / provider / dsl_mapping / hardware
    QString protocol = QStringLiteral("none");
    QString mode;
    QString area;
    qint64 address = -1;
    int bitOffset = 0;
    int elementCount = 1;
    QString dataType;
    QString byteOrder = QStringLiteral("BigEndian");
    QString wordOrder = QStringLiteral("BigEndian");
    double scale = 1.0;
    double offset = 0.0;
    QString pollGroup;
    int unitId = 1;
    QString channel;
    QString sourceModule;
    QString kind;
    QString server;
    QString item;
    QString ioGroup;
    QString tagName;
    QString tagAccess;
    QString tagDescription;
    QVariantMap extras;

    QVariantMap toVariantMap() const
    {
        QVariantMap map;
        map.insert(QStringLiteral("schemaVersion"), schemaVersion);
        map.insert(QStringLiteral("role"), role);
        map.insert(QStringLiteral("protocol"), protocol);
        map.insert(QStringLiteral("mode"), mode);
        map.insert(QStringLiteral("area"), area);
        map.insert(QStringLiteral("address"), address);
        map.insert(QStringLiteral("bitOffset"), bitOffset);
        map.insert(QStringLiteral("elementCount"), elementCount);
        map.insert(QStringLiteral("dataType"), dataType);
        map.insert(QStringLiteral("byteOrder"), byteOrder);
        map.insert(QStringLiteral("wordOrder"), wordOrder);
        map.insert(QStringLiteral("scale"), scale);
        map.insert(QStringLiteral("offset"), offset);
        map.insert(QStringLiteral("pollGroup"), pollGroup);
        map.insert(QStringLiteral("unitId"), unitId);
        map.insert(QStringLiteral("channel"), channel);
        map.insert(QStringLiteral("sourceModule"), sourceModule);
        map.insert(QStringLiteral("kind"), kind);
        map.insert(QStringLiteral("server"), server);
        map.insert(QStringLiteral("item"), item);
        map.insert(QStringLiteral("ioGroup"), ioGroup);
        map.insert(QStringLiteral("tagName"), tagName);
        map.insert(QStringLiteral("tagAccess"), tagAccess);
        map.insert(QStringLiteral("tagDescription"), tagDescription);

        for (auto it = extras.constBegin(); it != extras.constEnd(); ++it) {
            if (!map.contains(it.key()))
                map.insert(it.key(), it.value());
        }
        return map;
    }
};

inline RuntimePointAddressing makeAddressing(const QString& role,
                                            const QString& sourceModule,
                                            RuntimePointKind kind)
{
    RuntimePointAddressing addr;
    addr.role = role;
    addr.sourceModule = sourceModule;
    switch (kind) {
    case RuntimePointKind::Variable:  addr.kind = QStringLiteral("Variable"); break;
    case RuntimePointKind::Parameter: addr.kind = QStringLiteral("Parameter"); break;
    case RuntimePointKind::Status:    addr.kind = QStringLiteral("Status"); break;
    case RuntimePointKind::Alarm:     addr.kind = QStringLiteral("Alarm"); break;
    case RuntimePointKind::Resource:  addr.kind = QStringLiteral("Resource"); break;
    }
    return addr;
}

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
    auto addressing = makeAddressing(QStringLiteral("variable"), pt.sourceModule, pt.kind);
    addressing.mode = QStringLiteral("binding");
    addressing.area = v.scope;
    addressing.channel = v.binding;
    addressing.dataType = v.dataType;
    addressing.tagName = v.name;
    addressing.tagAccess = runtimePointAccessToString(pt.access);
    addressing.server = QStringLiteral("LH");
    addressing.item = v.binding.isEmpty() ? v.id : v.binding;
    addressing.ioGroup = v.scope;
    addressing.tagDescription = v.name;
    addressing.extras = v.metadata;
    pt.addressing = addressing.toVariantMap();
    pt.metadata = v.metadata;
    pt.opcTagName = v.name;
    pt.opcTagDescription = v.name;
    pt.opcTagGroup = QStringLiteral("Variables");
    pt.opcServerName = QStringLiteral("LH");
    pt.opcItemName = v.binding.isEmpty() ? v.id : v.binding;
    pt.opcIoGroup = v.scope;
    pt.opcTagAccess = runtimePointAccessToString(pt.access);
    pt.opcMetadata = v.metadata;
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
    auto addressing = makeAddressing(QStringLiteral("parameter"), pt.sourceModule, pt.kind);
    addressing.mode = p.onlineEditable ? QStringLiteral("editable") : QStringLiteral("readonly");
    addressing.area = QStringLiteral("parameters");
    addressing.channel = p.name;
    addressing.dataType = p.dataType;
    addressing.unitId = 0;
    addressing.tagName = p.name;
    addressing.tagAccess = runtimePointAccessToString(pt.access);
    addressing.server = QStringLiteral("LH");
    addressing.item = p.name;
    addressing.ioGroup = QStringLiteral("parameters");
    addressing.tagDescription = p.name;
    addressing.extras = pt.metadata;
    pt.addressing = addressing.toVariantMap();
    pt.opcTagName = p.name;
    pt.opcTagDescription = p.name;
    pt.opcTagGroup = QStringLiteral("Parameters");
    pt.opcServerName = QStringLiteral("LH");
    pt.opcItemName = p.name;
    pt.opcIoGroup = QStringLiteral("parameters");
    pt.opcTagAccess = runtimePointAccessToString(pt.access);
    pt.opcMetadata = pt.metadata;
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
    auto addressing = makeAddressing(QStringLiteral("resource"), pt.sourceModule, pt.kind);
    addressing.mode = QStringLiteral("resource-binding");
    addressing.area = r.resourceType;
    addressing.channel = r.channel;
    addressing.tagName = r.resourceName;
    addressing.tagAccess = runtimePointAccessToString(pt.access);
    addressing.server = QStringLiteral("LH");
    addressing.item = r.channel;
    addressing.ioGroup = r.resourceType;
    addressing.tagDescription = r.resourceName;
    addressing.extras.insert(QStringLiteral("resourceType"), r.resourceType);
    addressing.extras.insert(QStringLiteral("owner"), r.owner);
    addressing.extras.insert(QStringLiteral("exclusive"), r.exclusive);
    pt.addressing = addressing.toVariantMap();
    pt.metadata = r.metadata;
    pt.opcTagName = r.resourceName;
    pt.opcTagDescription = r.resourceName;
    pt.opcTagGroup = QStringLiteral("Resources");
    pt.opcServerName = QStringLiteral("LH");
    pt.opcItemName = r.channel;
    pt.opcIoGroup = r.resourceType;
    pt.opcTagAccess = runtimePointAccessToString(pt.access);
    pt.opcMetadata = r.metadata;
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
    auto addressing = makeAddressing(QStringLiteral("provider"), pt.sourceModule, pt.kind);
    addressing.mode = QStringLiteral("poll");
    addressing.area = QStringLiteral("monitoring");
    addressing.channel = prov.channelName;
    addressing.pollGroup = QStringLiteral("default");
    addressing.tagName = prov.channelName;
    addressing.tagAccess = runtimePointAccessToString(pt.access);
    addressing.server = QStringLiteral("LH");
    addressing.item = prov.channelName;
    addressing.ioGroup = QStringLiteral("status");
    addressing.tagDescription = prov.channelName;
    addressing.extras.insert(QStringLiteral("periodMs"), prov.periodMs);
    addressing.extras.insert(QStringLiteral("priority"), prov.priority);
    pt.addressing = addressing.toVariantMap();
    pt.metadata = prov.metadata;
    pt.opcTagName = prov.channelName;
    pt.opcTagDescription = prov.channelName;
    pt.opcTagGroup = QStringLiteral("Status");
    pt.opcServerName = QStringLiteral("LH");
    pt.opcItemName = prov.channelName;
    pt.opcIoGroup = QStringLiteral("status");
    pt.opcTagAccess = runtimePointAccessToString(pt.access);
    pt.opcMetadata = prov.metadata;
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
    auto addressing = makeAddressing(QStringLiteral("dsl_mapping"), pt.sourceModule, pt.kind);
    addressing.mode = QStringLiteral("mapping");
    addressing.area = QStringLiteral("dsl");
    addressing.channel = m.channelName;
    addressing.pollGroup = QStringLiteral("default");
    addressing.tagName = m.snippetName.isEmpty() ? m.channelName : m.snippetName;
    addressing.tagAccess = runtimePointAccessToString(pt.access);
    addressing.server = QStringLiteral("LH");
    addressing.item = m.channelName;
    addressing.ioGroup = QStringLiteral("dsl");
    addressing.tagDescription = m.snippetName.isEmpty() ? m.channelName : m.snippetName;
    addressing.extras.insert(QStringLiteral("snippetId"), m.snippetId);
    addressing.extras.insert(QStringLiteral("channelName"), m.channelName);
    addressing.extras.insert(QStringLiteral("signalPath"), m.signalPath);
    addressing.extras.insert(QStringLiteral("periodMs"), m.periodMs);
    addressing.extras.insert(QStringLiteral("lineNumber"), m.lineNumber);
    pt.addressing = addressing.toVariantMap();
    pt.metadata = m.metadata;
    pt.opcTagName = m.snippetName.isEmpty() ? m.channelName : m.snippetName;
    pt.opcTagDescription = pt.opcTagName;
    pt.opcTagGroup = QStringLiteral("DSL");
    pt.opcServerName = QStringLiteral("LH");
    pt.opcItemName = m.channelName;
    pt.opcIoGroup = QStringLiteral("dsl");
    pt.opcTagAccess = runtimePointAccessToString(pt.access);
    pt.opcMetadata = m.metadata;
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

inline OpcTagDefinition runtimePointToOpcTag(const RuntimePointDefinition& pt)
{
    OpcTagDefinition tag;
    tag.tagName = pt.opcTagName.isEmpty() ? pt.name : pt.opcTagName;
    tag.dataType = pt.dataType;
    tag.tagDescription = pt.opcTagDescription.isEmpty() ? pt.name : pt.opcTagDescription;
    tag.tagGroup = pt.opcTagGroup.isEmpty() ? QStringLiteral("General") : pt.opcTagGroup;
    tag.tagAccess = pt.opcTagAccess.isEmpty() ? runtimePointAccessToString(pt.access) : pt.opcTagAccess;
    tag.server = pt.opcServerName.isEmpty() ? QStringLiteral("LH") : pt.opcServerName;
    tag.item = pt.opcItemName.isEmpty() ? pt.bindingPath : pt.opcItemName;
    tag.ioGroup = pt.opcIoGroup.isEmpty() ? QStringLiteral("default") : pt.opcIoGroup;
    tag.unit = pt.unit;
    tag.addressing = pt.addressing;
    tag.metadata = pt.opcMetadata.isEmpty() ? pt.metadata : pt.opcMetadata;
    return tag;
}

inline QList<OpcTagDefinition> runtimePointsToOpcTags(const QList<RuntimePointDefinition>& points)
{
    QList<OpcTagDefinition> tags;
    tags.reserve(points.size());
    for (const auto& point : points) {
        tags.append(runtimePointToOpcTag(point));
    }
    return tags;
}

} // namespace RuntimePointConverter

inline QString runtimePointQualityToString(RuntimePointQuality quality)
{
    switch (quality) {
    case RuntimePointQuality::Good: return QStringLiteral("Good");
    case RuntimePointQuality::Simulated: return QStringLiteral("Simulated");
    case RuntimePointQuality::Stale: return QStringLiteral("Stale");
    case RuntimePointQuality::Bad: return QStringLiteral("Bad");
    case RuntimePointQuality::Offline: return QStringLiteral("Offline");
    case RuntimePointQuality::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

inline RuntimePointQuality runtimePointQualityFromBackendError(const CommError& error,
                                                               bool backendOnline)
{
    if (!error.isError()) {
        return backendOnline ? RuntimePointQuality::Good : RuntimePointQuality::Offline;
    }

    switch (error.code) {
    case CommErrorCode::ConnectionTimeout:
    case CommErrorCode::ReceiveTimeout:
        return backendOnline ? RuntimePointQuality::Stale : RuntimePointQuality::Offline;
    case CommErrorCode::ConnectionLost:
    case CommErrorCode::ConnectionFailed:
    case CommErrorCode::DeviceNotFound:
    case CommErrorCode::DeviceBusy:
        return RuntimePointQuality::Offline;
    case CommErrorCode::PermissionDenied:
    case CommErrorCode::InvalidAddress:
    case CommErrorCode::InvalidParameter:
    case CommErrorCode::UnsupportedProtocol:
    case CommErrorCode::ProtocolError:
    case CommErrorCode::InvalidResponse:
    case CommErrorCode::AddressMismatch:
    case CommErrorCode::ResourceError:
    case CommErrorCode::InternalError:
        return RuntimePointQuality::Bad;
    default:
        return backendOnline ? RuntimePointQuality::Bad : RuntimePointQuality::Offline;
    }
}

#endif // RUNTIME_POINT_TYPES_H
