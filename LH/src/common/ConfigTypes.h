/**
 * @file ConfigTypes.h
 * @brief 共享配置类型定义
 *
 * 该文件定义了跨模块共享的数据结构，避免循环依赖。
 * 包括：组态映射、Provider配置、项目运行时配置等。
 */

#ifndef CONFIG_TYPES_H
#define CONFIG_TYPES_H

#include <QString>
#include <QDateTime>
#include <QVariantMap>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

/**
 * @brief DslMappingEntry
 * DSL 组态映射条目，描述 DSL 片段与监控通道/信号的绑定关系
 */
struct DslMappingEntry
{
    QString id;              ///< UUID 唯一标识
    QString snippetId;       ///< DSL 组件 ID（如 "analog_input"）
    QString snippetName;     ///< 组件显示名称
    QString channelName;     ///< 绑定的监控通道名
    QString signalPath;      ///< 信号路径（如 "pressure.input"）
    QString unit;            ///< 物理单位（如 "bar", "mA"）
    int periodMs = 20;       ///< 采样周期（毫秒）
    int lineNumber = -1;     ///< DSL 脚本中的行号
    QString generatedCode;   ///< 生成的代码片段
    QDateTime createTime;    ///< 创建时间
    QVariantMap metadata;    ///< 额外元数据

    /// 序列化为 JSON
    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["id"] = id;
        obj["snippetId"] = snippetId;
        obj["snippetName"] = snippetName;
        obj["channelName"] = channelName;
        obj["signalPath"] = signalPath;
        obj["unit"] = unit;
        obj["periodMs"] = periodMs;
        obj["lineNumber"] = lineNumber;
        obj["generatedCode"] = generatedCode;
        obj["createTime"] = createTime.toString(Qt::ISODate);
        obj["metadata"] = QJsonObject::fromVariantMap(metadata);
        return obj;
    }

    /// 从 JSON 反序列化
    static DslMappingEntry fromJson(const QJsonObject& obj)
    {
        DslMappingEntry entry;
        entry.id = obj["id"].toString();
        entry.snippetId = obj["snippetId"].toString();
        entry.snippetName = obj["snippetName"].toString();
        entry.channelName = obj["channelName"].toString();
        entry.signalPath = obj["signalPath"].toString();
        entry.unit = obj["unit"].toString();
        entry.periodMs = obj["periodMs"].toInt(20);
        entry.lineNumber = obj["lineNumber"].toInt(-1);
        entry.generatedCode = obj["generatedCode"].toString();
        entry.createTime = QDateTime::fromString(obj["createTime"].toString(), Qt::ISODate);
        entry.metadata = obj["metadata"].toObject().toVariantMap();
        return entry;
    }

    /// 生成新的 UUID
    static QString generateId()
    {
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
};

struct VariableDefinition
{
    QString id;
    QString name;
    QString dataType;
    QString scope = "global";
    QString defaultValue;
    bool readOnly = false;
    QString binding;
    QVariantMap metadata;

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["dataType"] = dataType;
        obj["scope"] = scope;
        obj["defaultValue"] = defaultValue;
        obj["readOnly"] = readOnly;
        obj["binding"] = binding;
        obj["metadata"] = QJsonObject::fromVariantMap(metadata);
        return obj;
    }

    static VariableDefinition fromJson(const QJsonObject& obj)
    {
        VariableDefinition def;
        def.id = obj["id"].toString();
        def.name = obj["name"].toString();
        def.dataType = obj["dataType"].toString();
        def.scope = obj["scope"].toString("global");
        def.defaultValue = obj["defaultValue"].toString();
        def.readOnly = obj["readOnly"].toBool(false);
        def.binding = obj["binding"].toString();
        def.metadata = obj["metadata"].toObject().toVariantMap();
        return def;
    }

    static VariableDefinition makeTemplate(const QString& variableName,
                                           const QString& dataType,
                                           const QString& scope = QStringLiteral("global"))
    {
        VariableDefinition def;
        def.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        def.name = variableName;
        def.dataType = dataType;
        def.scope = scope;
        def.defaultValue = QStringLiteral("0");
        return def;
    }
};

struct ParameterDefinition
{
    QString id;
    QString name;
    QString dataType;
    QString defaultValue;
    QString currentValue;
    QString minValue;
    QString maxValue;
    QString unit;
    bool onlineEditable = false;
    bool confirmed = false;
    QVariantMap metadata;

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["dataType"] = dataType;
        obj["defaultValue"] = defaultValue;
        obj["currentValue"] = currentValue;
        obj["minValue"] = minValue;
        obj["maxValue"] = maxValue;
        obj["unit"] = unit;
        obj["onlineEditable"] = onlineEditable;
        obj["confirmed"] = confirmed;
        obj["metadata"] = QJsonObject::fromVariantMap(metadata);
        return obj;
    }

    static ParameterDefinition fromJson(const QJsonObject& obj)
    {
        ParameterDefinition def;
        def.id = obj["id"].toString();
        def.name = obj["name"].toString();
        def.dataType = obj["dataType"].toString();
        def.defaultValue = obj["defaultValue"].toString();
        def.currentValue = obj["currentValue"].toString();
        def.minValue = obj["minValue"].toString();
        def.maxValue = obj["maxValue"].toString();
        def.unit = obj["unit"].toString();
        def.onlineEditable = obj["onlineEditable"].toBool(false);
        def.confirmed = obj["confirmed"].toBool(false);
        def.metadata = obj["metadata"].toObject().toVariantMap();
        return def;
    }

    static ParameterDefinition makeTemplate(const QString& parameterName,
                                            const QString& dataType,
                                            const QString& defaultValue = QStringLiteral("0"),
                                            const QString& unit = QString())
    {
        ParameterDefinition def;
        def.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        def.name = parameterName;
        def.dataType = dataType;
        def.defaultValue = defaultValue;
        def.currentValue = defaultValue;
        def.confirmed = true;
        def.unit = unit;
        return def;
    }
};

struct HardwareResourceBinding
{
    QString id;
    QString resourceType;
    QString resourceName;
    QString channel;
    QString owner;
    bool exclusive = true;
    QVariantMap metadata;

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["id"] = id;
        obj["resourceType"] = resourceType;
        obj["resourceName"] = resourceName;
        obj["channel"] = channel;
        obj["owner"] = owner;
        obj["exclusive"] = exclusive;
        obj["metadata"] = QJsonObject::fromVariantMap(metadata);
        return obj;
    }

    static HardwareResourceBinding fromJson(const QJsonObject& obj)
    {
        HardwareResourceBinding binding;
        binding.id = obj["id"].toString();
        binding.resourceType = obj["resourceType"].toString();
        binding.resourceName = obj["resourceName"].toString();
        binding.channel = obj["channel"].toString();
        binding.owner = obj["owner"].toString();
        binding.exclusive = obj["exclusive"].toBool(true);
        binding.metadata = obj["metadata"].toObject().toVariantMap();
        return binding;
    }

    static HardwareResourceBinding makeTemplate(const QString& resourceType,
                                                const QString& resourceName,
                                                const QString& channel,
                                                const QString& owner = QString())
    {
        HardwareResourceBinding binding;
        binding.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        binding.resourceType = resourceType;
        binding.resourceName = resourceName;
        binding.channel = channel;
        binding.owner = owner;
        return binding;
    }
};

/**
 * @brief MonitorProviderRuntimeConfig
 * 监控 Provider 运行时配置
 */
struct MonitorProviderRuntimeConfig
{
    QString id;              ///< Provider 唯一标识
    QString channelName;     ///< 通道名称
    QString unit;            ///< 物理单位
    int periodMs = 20;       ///< 采样周期（毫秒）
    int priority = 128;      ///< 任务优先级
    QVariantMap metadata;    ///< 额外元数据

    /// 序列化为 JSON
    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["id"] = id;
        obj["channelName"] = channelName;
        obj["unit"] = unit;
        obj["periodMs"] = periodMs;
        obj["priority"] = priority;
        obj["metadata"] = QJsonObject::fromVariantMap(metadata);
        return obj;
    }

    /// 从 JSON 反序列化
    static MonitorProviderRuntimeConfig fromJson(const QJsonObject& obj)
    {
        MonitorProviderRuntimeConfig cfg;
        cfg.id = obj["id"].toString();
        cfg.channelName = obj["channelName"].toString();
        cfg.unit = obj["unit"].toString();
        cfg.periodMs = obj["periodMs"].toInt(20);
        cfg.priority = obj["priority"].toInt(128);
        cfg.metadata = obj["metadata"].toObject().toVariantMap();
        return cfg;
    }
};

struct TargetDeviceConfig
{
    QString family = "TMS2812";
    QString model = "TMS320F2812";
    QString nodeId;
    QString linkProtocol = "unknown";
    QVariantMap parameters;

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["family"] = family;
        obj["model"] = model;
        obj["nodeId"] = nodeId;
        obj["linkProtocol"] = linkProtocol;
        obj["parameters"] = QJsonObject::fromVariantMap(parameters);
        return obj;
    }

    static TargetDeviceConfig fromJson(const QJsonObject& obj)
    {
        TargetDeviceConfig cfg;
        cfg.family = obj["family"].toString("TMS2812");
        cfg.model = obj["model"].toString("TMS320F2812");
        cfg.nodeId = obj["nodeId"].toString();
        cfg.linkProtocol = obj["linkProtocol"].toString("unknown");
        cfg.parameters = obj["parameters"].toObject().toVariantMap();
        return cfg;
    }
};

struct ControllerConfig
{
    QString model;
    int modbusSlaveId = 1;
    QString targetRoutingMode = "unknown";
    QVariantMap routingParameters;

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["model"] = model;
        obj["modbusSlaveId"] = modbusSlaveId;
        obj["targetRoutingMode"] = targetRoutingMode;
        obj["routingParameters"] = QJsonObject::fromVariantMap(routingParameters);
        return obj;
    }

    static ControllerConfig fromJson(const QJsonObject& obj)
    {
        ControllerConfig cfg;
        cfg.model = obj["model"].toString();
        cfg.modbusSlaveId = obj["modbusSlaveId"].toInt(1);
        cfg.targetRoutingMode = obj["targetRoutingMode"].toString("unknown");
        cfg.routingParameters = obj["routingParameters"].toObject().toVariantMap();
        return cfg;
    }
};

struct TransportConfig
{
    QString protocol = "modbus";
    QString mode = "rtu";
    QVariantMap parameters;

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["protocol"] = protocol;
        obj["mode"] = mode;
        obj["parameters"] = QJsonObject::fromVariantMap(parameters);
        return obj;
    }

    static TransportConfig fromJson(const QJsonObject& obj)
    {
        TransportConfig cfg;
        cfg.protocol = obj["protocol"].toString("modbus");
        cfg.mode = obj["mode"].toString("rtu");
        cfg.parameters = obj["parameters"].toObject().toVariantMap();
        return cfg;
    }
};

struct BridgeConfig
{
    QString pcToController = "modbus";
    QString controllerToTarget = "unknown";
    QVariantMap parameters;

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["pcToController"] = pcToController;
        obj["controllerToTarget"] = controllerToTarget;
        obj["parameters"] = QJsonObject::fromVariantMap(parameters);
        return obj;
    }

    static BridgeConfig fromJson(const QJsonObject& obj)
    {
        BridgeConfig cfg;
        cfg.pcToController = obj["pcToController"].toString("modbus");
        cfg.controllerToTarget = obj["controllerToTarget"].toString("unknown");
        cfg.parameters = obj["parameters"].toObject().toVariantMap();
        return cfg;
    }
};

struct DownloadArtifactConfig
{
    QString artifactType = "dsl_custom";
    QString filePath;
    QString formatVersion;
    QString checksum;
    QVariantMap metadata;

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["artifactType"] = artifactType;
        obj["filePath"] = filePath;
        obj["formatVersion"] = formatVersion;
        obj["checksum"] = checksum;
        obj["metadata"] = QJsonObject::fromVariantMap(metadata);
        return obj;
    }

    static DownloadArtifactConfig fromJson(const QJsonObject& obj)
    {
        DownloadArtifactConfig cfg;
        cfg.artifactType = obj["artifactType"].toString("dsl_custom");
        cfg.filePath = obj["filePath"].toString();
        cfg.formatVersion = obj["formatVersion"].toString();
        cfg.checksum = obj["checksum"].toString();
        cfg.metadata = obj["metadata"].toObject().toVariantMap();
        return cfg;
    }
};

/**
 * @brief ProjectRuntimeConfig
 * 项目运行时配置，支持 JSON 序列化
 */
struct ProjectRuntimeConfig
{
    int schemaVersion = 3;
    TargetDeviceConfig target;
    ControllerConfig controller;
    TransportConfig transport;
    BridgeConfig bridge;
    DownloadArtifactConfig downloadArtifact;
    QString projectName;                           ///< 项目名称
    QString protocol;                              ///< 通信协议（CAN/RS485/Ethernet）
    QVariantMap commParameters;                    ///< 通信参数
    QString dslScriptPath;                         ///< 旧版单脚本路径（兼容字段）
    QString mainScriptPath;                        ///< 主入口脚本路径
    QStringList scriptFiles;                       ///< 工程脚本清单（相对/绝对路径均可）
    QDateTime lastModified;                        ///< 最后修改时间
    QList<MonitorProviderRuntimeConfig> providers; ///< 监控 Provider 列表
    QList<DslMappingEntry> dslMappings;            ///< DSL 组态映射列表
    QList<VariableDefinition> variables;           ///< 变量定义列表
    QList<ParameterDefinition> parameters;         ///< 参数定义列表
    QList<HardwareResourceBinding> resources;      ///< 硬件资源绑定列表

    /// 序列化为 JSON
    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["schemaVersion"] = schemaVersion;
        obj["projectName"] = projectName;
        obj["protocol"] = protocol;
        obj["commParameters"] = QJsonObject::fromVariantMap(commParameters);
        obj["dslScriptPath"] = dslScriptPath;
        obj["mainScriptPath"] = mainScriptPath;
        QJsonArray scriptFilesArray;
        for (const auto& script : scriptFiles) {
            scriptFilesArray.append(script);
        }
        obj["scriptFiles"] = scriptFilesArray;
        obj["lastModified"] = lastModified.toString(Qt::ISODate);
        obj["target"] = target.toJson();
        obj["controller"] = controller.toJson();
        obj["transport"] = transport.toJson();
        obj["bridge"] = bridge.toJson();
        obj["downloadArtifact"] = downloadArtifact.toJson();

        QJsonArray providersArray;
        for (const auto& p : providers) {
            providersArray.append(p.toJson());
        }
        obj["providers"] = providersArray;

        QJsonArray mappingsArray;
        for (const auto& m : dslMappings) {
            mappingsArray.append(m.toJson());
        }
        obj["dslMappings"] = mappingsArray;

        QJsonArray variablesArray;
        for (const auto& v : variables) {
            variablesArray.append(v.toJson());
        }
        obj["variables"] = variablesArray;

        QJsonArray parametersArray;
        for (const auto& p : parameters) {
            parametersArray.append(p.toJson());
        }
        obj["parameters"] = parametersArray;

        QJsonArray resourcesArray;
        for (const auto& r : resources) {
            resourcesArray.append(r.toJson());
        }
        obj["resources"] = resourcesArray;

        return obj;
    }

    /// 从 JSON 反序列化
    static ProjectRuntimeConfig fromJson(const QJsonObject& obj)
    {
        ProjectRuntimeConfig cfg;
        cfg.schemaVersion = obj["schemaVersion"].toInt(3);
        cfg.projectName = obj["projectName"].toString();
        cfg.protocol = obj["protocol"].toString();
        cfg.commParameters = obj["commParameters"].toObject().toVariantMap();
        cfg.dslScriptPath = obj["dslScriptPath"].toString();
        cfg.mainScriptPath = obj["mainScriptPath"].toString();
        QJsonArray scriptFilesArray = obj["scriptFiles"].toArray();
        for (const auto& script : scriptFilesArray) {
            cfg.scriptFiles.append(script.toString());
        }
        // 兼容旧工程：mainScriptPath/scriptFiles 为空时回退到 dslScriptPath。
        if (cfg.mainScriptPath.isEmpty()) {
            cfg.mainScriptPath = cfg.dslScriptPath;
        }
        if (cfg.scriptFiles.isEmpty() && !cfg.mainScriptPath.isEmpty()) {
            cfg.scriptFiles.append(cfg.mainScriptPath);
        }
        // 保持旧字段与新字段一致，避免旧调用链读到空值。
        if (cfg.dslScriptPath.isEmpty()) {
            cfg.dslScriptPath = cfg.mainScriptPath;
        }
        cfg.lastModified = QDateTime::fromString(obj["lastModified"].toString(), Qt::ISODate);
        cfg.target = TargetDeviceConfig::fromJson(obj["target"].toObject());
        cfg.controller = ControllerConfig::fromJson(obj["controller"].toObject());
        cfg.transport = TransportConfig::fromJson(obj["transport"].toObject());
        cfg.bridge = BridgeConfig::fromJson(obj["bridge"].toObject());
        cfg.downloadArtifact = DownloadArtifactConfig::fromJson(obj["downloadArtifact"].toObject());
        if (cfg.protocol.isEmpty()) {
            cfg.protocol = cfg.transport.protocol;
        }

        QJsonArray providersArray = obj["providers"].toArray();
        for (const auto& p : providersArray) {
            cfg.providers.append(MonitorProviderRuntimeConfig::fromJson(p.toObject()));
        }

        QJsonArray mappingsArray = obj["dslMappings"].toArray();
        for (const auto& m : mappingsArray) {
            cfg.dslMappings.append(DslMappingEntry::fromJson(m.toObject()));
        }

        QJsonArray variablesArray = obj["variables"].toArray();
        for (const auto& v : variablesArray) {
            cfg.variables.append(VariableDefinition::fromJson(v.toObject()));
        }

        QJsonArray parametersArray = obj["parameters"].toArray();
        for (const auto& p : parametersArray) {
            cfg.parameters.append(ParameterDefinition::fromJson(p.toObject()));
        }

        QJsonArray resourcesArray = obj["resources"].toArray();
        for (const auto& r : resourcesArray) {
            cfg.resources.append(HardwareResourceBinding::fromJson(r.toObject()));
        }

        return cfg;
    }

    /// 清空配置
    void clear()
    {
        schemaVersion = 3;
        target = TargetDeviceConfig();
        controller = ControllerConfig();
        transport = TransportConfig();
        bridge = BridgeConfig();
        downloadArtifact = DownloadArtifactConfig();
        projectName.clear();
        protocol.clear();
        commParameters.clear();
        dslScriptPath.clear();
        mainScriptPath.clear();
        scriptFiles.clear();
        lastModified = QDateTime();
        providers.clear();
        dslMappings.clear();
        variables.clear();
        parameters.clear();
        resources.clear();
    }

    void applyEmptyTemplates()
    {
        if (variables.isEmpty()) {
            variables.append(VariableDefinition::makeTemplate(QStringLiteral("var_1"), QStringLiteral("REAL")));
        }

        if (parameters.isEmpty()) {
            parameters.append(ParameterDefinition::makeTemplate(QStringLiteral("param_1"), QStringLiteral("REAL")));
        }

        if (resources.isEmpty()) {
            resources.append(HardwareResourceBinding::makeTemplate(QStringLiteral("AI"),
                                                                   QStringLiteral("AI_1"),
                                                                   QStringLiteral("0")));
        }
    }

    /// 检查是否为空
    bool isEmpty() const
    {
        return projectName.isEmpty();
    }
};

#endif // CONFIG_TYPES_H
