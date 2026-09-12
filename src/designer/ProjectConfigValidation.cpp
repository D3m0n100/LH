#include "ProjectConfigValidation.h"
#include <QJsonArray>
#include <cmath>
#include <limits>

namespace ProjectConfigValidation {
enum class JsonShapeType {
    String,
    Boolean,
    Integer,
    Object,
    Array
};

QString jsonShapeTypeName(JsonShapeType type)
{
    switch (type) {
    case JsonShapeType::String:
        return QStringLiteral("字符串");
    case JsonShapeType::Boolean:
        return QStringLiteral("布尔值");
    case JsonShapeType::Integer:
        return QStringLiteral("整数");
    case JsonShapeType::Object:
        return QStringLiteral("对象");
    case JsonShapeType::Array:
        return QStringLiteral("数组");
    }
    return QStringLiteral("合法值");
}

QString jsonValueDescription(const QJsonValue& value)
{
    if (value.isUndefined()) {
        return QStringLiteral("<未定义>");
    }
    if (value.isNull()) {
        return QStringLiteral("null");
    }
    if (value.isString()) {
        return QStringLiteral("'%1'").arg(value.toString());
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble(), 'g', 15);
    }
    if (value.isObject()) {
        return QStringLiteral("<对象>");
    }
    if (value.isArray()) {
        return QStringLiteral("<数组>");
    }
    return QStringLiteral("<未知值>");
}

bool isJsonInteger(const QJsonValue& value)
{
    if (!value.isDouble()) {
        return false;
    }

    const double number = value.toDouble();
    return std::isfinite(number)
            && std::floor(number) == number
            && number >= static_cast<double>(std::numeric_limits<int>::min())
            && number <= static_cast<double>(std::numeric_limits<int>::max());
}

void appendUniqueError(QStringList& errors, const QString& error)
{
    if (!error.isEmpty() && !errors.contains(error)) {
        errors.append(error);
    }
}

void checkJsonField(const QJsonObject& object,
                   const QString& key,
                   const QString& path,
                   JsonShapeType expected,
                   QStringList& errors)
{
    if (!object.contains(key)) {
        return;
    }

    const QJsonValue value = object.value(key);
    bool valid = false;
    switch (expected) {
    case JsonShapeType::String:
        valid = value.isString();
        break;
    case JsonShapeType::Boolean:
        valid = value.isBool();
        break;
    case JsonShapeType::Integer:
        valid = isJsonInteger(value);
        break;
    case JsonShapeType::Object:
        valid = value.isObject();
        break;
    case JsonShapeType::Array:
        valid = value.isArray();
        break;
    }

    if (!valid) {
        appendUniqueError(errors,
                          QStringLiteral("配置字段 '%1' 的值 %2 类型错误，应为%3")
                              .arg(path, jsonValueDescription(value), jsonShapeTypeName(expected)));
    }
}

void validateProviderShape(const QJsonObject& object,
                           const QString& path,
                           QStringList& errors)
{
    checkJsonField(object, QStringLiteral("id"), path + QStringLiteral(".id"),
                   JsonShapeType::String, errors);
    checkJsonField(object, QStringLiteral("channelName"), path + QStringLiteral(".channelName"),
                   JsonShapeType::String, errors);
    checkJsonField(object, QStringLiteral("unit"), path + QStringLiteral(".unit"),
                   JsonShapeType::String, errors);
    checkJsonField(object, QStringLiteral("periodMs"), path + QStringLiteral(".periodMs"),
                   JsonShapeType::Integer, errors);
    checkJsonField(object, QStringLiteral("priority"), path + QStringLiteral(".priority"),
                   JsonShapeType::Integer, errors);
    checkJsonField(object, QStringLiteral("metadata"), path + QStringLiteral(".metadata"),
                   JsonShapeType::Object, errors);
}

void validateMappingShape(const QJsonObject& object,
                          const QString& path,
                          QStringList& errors)
{
    const QStringList stringFields = {
        QStringLiteral("id"), QStringLiteral("snippetId"), QStringLiteral("snippetName"),
        QStringLiteral("channelName"), QStringLiteral("signalPath"), QStringLiteral("unit"),
        QStringLiteral("generatedCode"), QStringLiteral("createTime")
    };
    for (const QString& field : stringFields) {
        checkJsonField(object, field, path + QLatin1Char('.') + field,
                       JsonShapeType::String, errors);
    }
    checkJsonField(object, QStringLiteral("periodMs"), path + QStringLiteral(".periodMs"),
                   JsonShapeType::Integer, errors);
    checkJsonField(object, QStringLiteral("lineNumber"), path + QStringLiteral(".lineNumber"),
                   JsonShapeType::Integer, errors);
    checkJsonField(object, QStringLiteral("metadata"), path + QStringLiteral(".metadata"),
                   JsonShapeType::Object, errors);
}

void validateVariableShape(const QJsonObject& object,
                           const QString& path,
                           QStringList& errors)
{
    const QStringList stringFields = {
        QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("dataType"),
        QStringLiteral("scope"), QStringLiteral("defaultValue"), QStringLiteral("binding")
    };
    for (const QString& field : stringFields) {
        checkJsonField(object, field, path + QLatin1Char('.') + field,
                       JsonShapeType::String, errors);
    }
    checkJsonField(object, QStringLiteral("readOnly"), path + QStringLiteral(".readOnly"),
                   JsonShapeType::Boolean, errors);
    checkJsonField(object, QStringLiteral("metadata"), path + QStringLiteral(".metadata"),
                   JsonShapeType::Object, errors);
}

void validateParameterShape(const QJsonObject& object,
                            const QString& path,
                            QStringList& errors)
{
    const QStringList stringFields = {
        QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("dataType"),
        QStringLiteral("defaultValue"), QStringLiteral("currentValue"),
        QStringLiteral("minValue"), QStringLiteral("maxValue"), QStringLiteral("unit")
    };
    for (const QString& field : stringFields) {
        checkJsonField(object, field, path + QLatin1Char('.') + field,
                       JsonShapeType::String, errors);
    }
    checkJsonField(object, QStringLiteral("onlineEditable"), path + QStringLiteral(".onlineEditable"),
                   JsonShapeType::Boolean, errors);
    checkJsonField(object, QStringLiteral("confirmed"), path + QStringLiteral(".confirmed"),
                   JsonShapeType::Boolean, errors);
    checkJsonField(object, QStringLiteral("metadata"), path + QStringLiteral(".metadata"),
                   JsonShapeType::Object, errors);
}

void validateResourceShape(const QJsonObject& object,
                           const QString& path,
                           QStringList& errors)
{
    const QStringList stringFields = {
        QStringLiteral("id"), QStringLiteral("resourceType"), QStringLiteral("resourceName"),
        QStringLiteral("channel"), QStringLiteral("owner")
    };
    for (const QString& field : stringFields) {
        checkJsonField(object, field, path + QLatin1Char('.') + field,
                       JsonShapeType::String, errors);
    }
    checkJsonField(object, QStringLiteral("exclusive"), path + QStringLiteral(".exclusive"),
                   JsonShapeType::Boolean, errors);
    checkJsonField(object, QStringLiteral("metadata"), path + QStringLiteral(".metadata"),
                   JsonShapeType::Object, errors);
}

void validateTargetShape(const QJsonObject& object,
                         const QString& path,
                         QStringList& errors)
{
    const QStringList stringFields = {
        QStringLiteral("family"), QStringLiteral("model"), QStringLiteral("nodeId"),
        QStringLiteral("linkProtocol")
    };
    for (const QString& field : stringFields) {
        checkJsonField(object, field, path + QLatin1Char('.') + field,
                       JsonShapeType::String, errors);
    }
    checkJsonField(object, QStringLiteral("parameters"), path + QStringLiteral(".parameters"),
                   JsonShapeType::Object, errors);
}

void validateControllerShape(const QJsonObject& object,
                             const QString& path,
                             QStringList& errors)
{
    checkJsonField(object, QStringLiteral("model"), path + QStringLiteral(".model"),
                   JsonShapeType::String, errors);
    checkJsonField(object, QStringLiteral("modbusSlaveId"), path + QStringLiteral(".modbusSlaveId"),
                   JsonShapeType::Integer, errors);
    checkJsonField(object, QStringLiteral("targetRoutingMode"), path + QStringLiteral(".targetRoutingMode"),
                   JsonShapeType::String, errors);
    checkJsonField(object, QStringLiteral("routingParameters"), path + QStringLiteral(".routingParameters"),
                   JsonShapeType::Object, errors);
}

void validateTransportShape(const QJsonObject& object,
                            const QString& path,
                            QStringList& errors)
{
    checkJsonField(object, QStringLiteral("protocol"), path + QStringLiteral(".protocol"),
                   JsonShapeType::String, errors);
    checkJsonField(object, QStringLiteral("mode"), path + QStringLiteral(".mode"),
                   JsonShapeType::String, errors);
    checkJsonField(object, QStringLiteral("parameters"), path + QStringLiteral(".parameters"),
                   JsonShapeType::Object, errors);
}

void validateKnownTransportParameterValues(const QJsonObject& parameters,
                                           const QString& path,
                                           QStringList& errors)
{
    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        const QString key = it.key().trimmed().toLower();
        const bool isKnownParameter = key == QStringLiteral("timeout")
                || key == QStringLiteral("timeoutms")
                || key == QStringLiteral("responsetimeout")
                || key == QStringLiteral("retry")
                || key == QStringLiteral("retries")
                || key == QStringLiteral("retrycount")
                || key == QStringLiteral("databits")
                || key == QStringLiteral("stopbits")
                || key == QStringLiteral("baudrate")
                || key == QStringLiteral("baud");
        if (!isKnownParameter) {
            continue;
        }

        const QJsonValue value = it.value();
        if (!value.isString() && !value.isDouble()) {
            appendUniqueError(errors,
                              QStringLiteral("配置字段 '%1.parameters.%2' 的值 %3 类型错误，应为字符串或数字")
                                  .arg(path, it.key(), jsonValueDescription(value)));
        }
    }
}

void validateTransportParameterShape(const QJsonObject& object,
                                     const QString& path,
                                     QStringList& errors)
{
    const QJsonValue parametersValue = object.value(QStringLiteral("parameters"));
    if (parametersValue.isObject()) {
        validateKnownTransportParameterValues(parametersValue.toObject(),
                                              path + QStringLiteral(".parameters"), errors);
    }
}

void validateBridgeShape(const QJsonObject& object,
                         const QString& path,
                         QStringList& errors)
{
    checkJsonField(object, QStringLiteral("pcToController"), path + QStringLiteral(".pcToController"),
                   JsonShapeType::String, errors);
    checkJsonField(object, QStringLiteral("controllerToTarget"), path + QStringLiteral(".controllerToTarget"),
                   JsonShapeType::String, errors);
    checkJsonField(object, QStringLiteral("parameters"), path + QStringLiteral(".parameters"),
                   JsonShapeType::Object, errors);
}

void validateDownloadArtifactShape(const QJsonObject& object,
                                   const QString& path,
                                   QStringList& errors)
{
    const QStringList stringFields = {
        QStringLiteral("artifactType"), QStringLiteral("filePath"),
        QStringLiteral("formatVersion"), QStringLiteral("checksum")
    };
    for (const QString& field : stringFields) {
        checkJsonField(object, field, path + QLatin1Char('.') + field,
                       JsonShapeType::String, errors);
    }
    checkJsonField(object, QStringLiteral("metadata"), path + QStringLiteral(".metadata"),
                   JsonShapeType::Object, errors);
}

void validateOpcServerShape(const QJsonObject& object,
                            const QString& path,
                            QStringList& errors)
{
    const QStringList booleanFields = {
        QStringLiteral("enabled"), QStringLiteral("exposeVariables"),
        QStringLiteral("exposeParameters"), QStringLiteral("exposeStatus"),
        QStringLiteral("exposeAlarms"), QStringLiteral("exposeTagTable")
    };
    for (const QString& field : booleanFields) {
        checkJsonField(object, field, path + QLatin1Char('.') + field,
                       JsonShapeType::Boolean, errors);
    }
    const QStringList integerFields = {
        QStringLiteral("publishIntervalMs"), QStringLiteral("timeoutMs"),
        QStringLiteral("reconnectDelayMs"), QStringLiteral("retries"),
        QStringLiteral("maxRegistersPerRequest")
    };
    for (const QString& field : integerFields) {
        checkJsonField(object, field, path + QLatin1Char('.') + field,
                       JsonShapeType::Integer, errors);
    }
    const QStringList stringFields = {
        QStringLiteral("channelName"), QStringLiteral("deviceName"),
        QStringLiteral("serialMode"), QStringLiteral("rootDescription"),
        QStringLiteral("classicServerName"), QStringLiteral("opcProgId")
    };
    for (const QString& field : stringFields) {
        checkJsonField(object, field, path + QLatin1Char('.') + field,
                       JsonShapeType::String, errors);
    }
    checkJsonField(object, QStringLiteral("metadata"), path + QStringLiteral(".metadata"),
                   JsonShapeType::Object, errors);
}

void checkObjectArray(const QJsonObject& object,
                      const QString& key,
                      const QString& path,
                      void (*validateElement)(const QJsonObject&, const QString&, QStringList&),
                      QStringList& errors)
{
    if (!object.contains(key)) {
        return;
    }
    const QJsonValue value = object.value(key);
    if (!value.isArray()) {
        appendUniqueError(errors,
                          QStringLiteral("配置字段 '%1' 的值 %2 类型错误，应为数组")
                              .arg(path, jsonValueDescription(value)));
        return;
    }
    const QJsonArray array = value.toArray();
    for (int i = 0; i < array.size(); ++i) {
        const QJsonValue element = array.at(i);
        const QString elementPath = QStringLiteral("%1[%2]").arg(path).arg(i);
        if (!element.isObject()) {
            appendUniqueError(errors,
                              QStringLiteral("配置字段 '%1' 的值 %2 类型错误，应为对象")
                                  .arg(elementPath, jsonValueDescription(element)));
            continue;
        }
        validateElement(element.toObject(), elementPath, errors);
    }
}

void checkStringArray(const QJsonObject& object,
                      const QString& key,
                      const QString& path,
                      QStringList& errors)
{
    if (!object.contains(key)) {
        return;
    }
    const QJsonValue value = object.value(key);
    if (!value.isArray()) {
        appendUniqueError(errors,
                          QStringLiteral("配置字段 '%1' 的值 %2 类型错误，应为数组")
                              .arg(path, jsonValueDescription(value)));
        return;
    }
    const QJsonArray array = value.toArray();
    for (int i = 0; i < array.size(); ++i) {
        const QJsonValue element = array.at(i);
        if (!element.isString()) {
            const QString elementPath = QStringLiteral("%1[%2]").arg(path).arg(i);
            appendUniqueError(errors,
                              QStringLiteral("配置字段 '%1' 的值 %2 类型错误，应为字符串")
                                  .arg(elementPath, jsonValueDescription(element)));
        }
    }
}

bool validateProjectConfigShape(const QJsonObject& object, QStringList& errors)
{
    const QString root = QStringLiteral("project_config");
    bool valid = true;

    if (object.contains(QStringLiteral("schemaVersion"))) {
        const QJsonValue schemaValue = object.value(QStringLiteral("schemaVersion"));
        if (!isJsonInteger(schemaValue)) {
            appendUniqueError(errors,
                              QStringLiteral("配置字段 '%1.schemaVersion' 的值 %2 类型错误，应为整数且范围为%3..%4")
                                  .arg(root, jsonValueDescription(schemaValue))
                                  .arg(ProjectRuntimeConfig::kMinimumSchemaVersion)
                                  .arg(ProjectRuntimeConfig::kCurrentSchemaVersion));
            valid = false;
        } else {
            const int schemaVersion = schemaValue.toInt();
            if (schemaVersion < ProjectRuntimeConfig::kMinimumSchemaVersion
                    || schemaVersion > ProjectRuntimeConfig::kCurrentSchemaVersion) {
                appendUniqueError(errors,
                                  QStringLiteral("配置字段 '%1.schemaVersion' 的值 %2 超出支持范围%3..%4")
                                      .arg(root, jsonValueDescription(schemaValue))
                                      .arg(ProjectRuntimeConfig::kMinimumSchemaVersion)
                                      .arg(ProjectRuntimeConfig::kCurrentSchemaVersion));
                valid = false;
            }
        }
    }

    const QStringList stringFields = {
        QStringLiteral("projectName"), QStringLiteral("protocol"),
        QStringLiteral("dslScriptPath"), QStringLiteral("mainScriptPath"),
        QStringLiteral("lastModified")
    };
    for (const QString& field : stringFields) {
        checkJsonField(object, field, root + QLatin1Char('.') + field,
                       JsonShapeType::String, errors);
    }

    checkJsonField(object, QStringLiteral("commParameters"), root + QStringLiteral(".commParameters"),
                   JsonShapeType::Object, errors);
    checkJsonField(object, QStringLiteral("target"), root + QStringLiteral(".target"),
                   JsonShapeType::Object, errors);
    checkJsonField(object, QStringLiteral("controller"), root + QStringLiteral(".controller"),
                   JsonShapeType::Object, errors);
    checkJsonField(object, QStringLiteral("transport"), root + QStringLiteral(".transport"),
                   JsonShapeType::Object, errors);
    checkJsonField(object, QStringLiteral("bridge"), root + QStringLiteral(".bridge"),
                   JsonShapeType::Object, errors);
    checkJsonField(object, QStringLiteral("downloadArtifact"), root + QStringLiteral(".downloadArtifact"),
                   JsonShapeType::Object, errors);
    checkJsonField(object, QStringLiteral("opcServer"), root + QStringLiteral(".opcServer"),
                   JsonShapeType::Object, errors);

    if (object.value(QStringLiteral("commParameters")).isObject()) {
        validateKnownTransportParameterValues(object.value(QStringLiteral("commParameters")).toObject(),
                                              root + QStringLiteral(".commParameters"), errors);
    }

    if (object.value(QStringLiteral("target")).isObject()) {
        validateTargetShape(object.value(QStringLiteral("target")).toObject(),
                            root + QStringLiteral(".target"), errors);
    }
    if (object.value(QStringLiteral("controller")).isObject()) {
        validateControllerShape(object.value(QStringLiteral("controller")).toObject(),
                                 root + QStringLiteral(".controller"), errors);
    }
    if (object.value(QStringLiteral("transport")).isObject()) {
        validateTransportShape(object.value(QStringLiteral("transport")).toObject(),
                                root + QStringLiteral(".transport"), errors);
        validateTransportParameterShape(object.value(QStringLiteral("transport")).toObject(),
                                        root + QStringLiteral(".transport"), errors);
    }
    if (object.value(QStringLiteral("bridge")).isObject()) {
        validateBridgeShape(object.value(QStringLiteral("bridge")).toObject(),
                            root + QStringLiteral(".bridge"), errors);
    }
    if (object.value(QStringLiteral("downloadArtifact")).isObject()) {
        validateDownloadArtifactShape(object.value(QStringLiteral("downloadArtifact")).toObject(),
                                      root + QStringLiteral(".downloadArtifact"), errors);
    }
    if (object.value(QStringLiteral("opcServer")).isObject()) {
        validateOpcServerShape(object.value(QStringLiteral("opcServer")).toObject(),
                               root + QStringLiteral(".opcServer"), errors);
    }

    checkStringArray(object, QStringLiteral("scriptFiles"), root + QStringLiteral(".scriptFiles"), errors);
    checkObjectArray(object, QStringLiteral("providers"), root + QStringLiteral(".providers"),
                     validateProviderShape, errors);
    checkObjectArray(object, QStringLiteral("dslMappings"), root + QStringLiteral(".dslMappings"),
                     validateMappingShape, errors);
    checkObjectArray(object, QStringLiteral("variables"), root + QStringLiteral(".variables"),
                     validateVariableShape, errors);
    checkObjectArray(object, QStringLiteral("parameters"), root + QStringLiteral(".parameters"),
                     validateParameterShape, errors);
    checkObjectArray(object, QStringLiteral("resources"), root + QStringLiteral(".resources"),
                     validateResourceShape, errors);

    return valid && errors.isEmpty();
}

QString variantValueDescription(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return QStringLiteral("null");
    }
    if (value.canConvert<QString>()) {
        return QStringLiteral("'%1'").arg(value.toString());
    }
    return QStringLiteral("<%1>").arg(QString::fromLatin1(value.typeName()));
}

bool variantToInteger(const QVariant& value, int* result)
{
    if (!value.isValid() || value.isNull()) {
        return false;
    }

    const QString text = value.toString().trimmed();
    bool integerOk = false;
    const qlonglong integer = text.toLongLong(&integerOk);
    if (integerOk
            && integer >= static_cast<qlonglong>(std::numeric_limits<int>::min())
            && integer <= static_cast<qlonglong>(std::numeric_limits<int>::max())) {
        if (result) {
            *result = static_cast<int>(integer);
        }
        return true;
    }

    bool doubleOk = false;
    const double number = value.toDouble(&doubleOk);
    if (!doubleOk || !std::isfinite(number) || std::floor(number) != number
            || number < static_cast<double>(std::numeric_limits<int>::min())
            || number > static_cast<double>(std::numeric_limits<int>::max())) {
        return false;
    }
    if (result) {
        *result = static_cast<int>(number);
    }
    return true;
}

bool variantToFiniteDouble(const QString& text, double* result)
{
    bool ok = false;
    const double value = text.trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(value)) {
        return false;
    }
    if (result) {
        *result = value;
    }
    return true;
}

bool isNumericDataType(const QString& dataType)
{
    const QString type = dataType.trimmed().toUpper();
    static const QStringList numericTypes = {
        QStringLiteral("INT"), QStringLiteral("INTEGER"), QStringLiteral("SINT"),
        QStringLiteral("USINT"), QStringLiteral("DINT"), QStringLiteral("UDINT"),
        QStringLiteral("LINT"), QStringLiteral("ULINT"), QStringLiteral("UINT"),
        QStringLiteral("WORD"), QStringLiteral("DWORD"), QStringLiteral("LWORD"),
        QStringLiteral("BYTE"), QStringLiteral("REAL"), QStringLiteral("LREAL"),
        QStringLiteral("FLOAT"), QStringLiteral("DOUBLE"), QStringLiteral("DECIMAL"),
        QStringLiteral("NUMBER")
    };
    return numericTypes.contains(type)
            || type.contains(QStringLiteral("INT"))
            || type.contains(QStringLiteral("REAL"))
            || type.contains(QStringLiteral("FLOAT"))
            || type.contains(QStringLiteral("DOUBLE"))
            || type.contains(QStringLiteral("DECIMAL"))
            || type.contains(QStringLiteral("NUMBER"));
}

void appendIntegerRangeError(QStringList& errors,
                             const QString& entity,
                             const QString& field,
                             const QString& value,
                             int minimum,
                             int maximum)
{
    appendUniqueError(errors,
                      QStringLiteral("%1 字段 '%2' 值 %3 必须在%4..%5范围内")
                          .arg(entity, field, value)
                          .arg(minimum)
                          .arg(maximum));
}

void validateParameterRanges(const ProjectRuntimeConfig& cfg, QStringList& errors)
{
    for (int i = 0; i < cfg.parameters.size(); ++i) {
        const ParameterDefinition& parameter = cfg.parameters.at(i);
        const bool hasRange = !parameter.minValue.trimmed().isEmpty()
                || !parameter.maxValue.trimmed().isEmpty();
        if (!isNumericDataType(parameter.dataType) && !hasRange) {
            continue;
        }

        const QString entity = parameter.name.trimmed().isEmpty()
                ? QStringLiteral("参数 #%1").arg(i + 1)
                : QStringLiteral("参数 '%1'").arg(parameter.name);
        double minimum = 0.0;
        double maximum = 0.0;
        bool hasMinimum = false;
        bool hasMaximum = false;
        if (!parameter.minValue.trimmed().isEmpty()) {
            hasMinimum = variantToFiniteDouble(parameter.minValue, &minimum);
            if (!hasMinimum) {
                appendUniqueError(errors,
                                  QStringLiteral("%1 字段 'minValue' 值 '%2' 必须是有效数字")
                                      .arg(entity, parameter.minValue));
            }
        }
        if (!parameter.maxValue.trimmed().isEmpty()) {
            hasMaximum = variantToFiniteDouble(parameter.maxValue, &maximum);
            if (!hasMaximum) {
                appendUniqueError(errors,
                                  QStringLiteral("%1 字段 'maxValue' 值 '%2' 必须是有效数字")
                                      .arg(entity, parameter.maxValue));
            }
        }

        if (hasMinimum && hasMaximum && minimum > maximum) {
            appendUniqueError(errors,
                              QStringLiteral("%1 字段 'minValue/maxValue' 值 '%2/%3' 顺序无效，minValue 不能大于 maxValue")
                                  .arg(entity, parameter.minValue, parameter.maxValue));
        }

        const auto checkValue = [&](const QString& field, const QString& text) {
            if (text.trimmed().isEmpty()) {
                return;
            }
            double value = 0.0;
            if (!variantToFiniteDouble(text, &value)) {
                appendUniqueError(errors,
                                  QStringLiteral("%1 字段 '%2' 值 '%3' 必须是有效数字")
                                      .arg(entity, field, text));
                return;
            }
            if (hasMinimum && value < minimum) {
                appendUniqueError(errors,
                                  QStringLiteral("%1 字段 '%2' 值 '%3' 超出 minValue '%4'..maxValue '%5' 范围")
                                      .arg(entity, field, text, parameter.minValue, parameter.maxValue));
            }
            if (hasMaximum && value > maximum) {
                appendUniqueError(errors,
                                  QStringLiteral("%1 字段 '%2' 值 '%3' 超出 minValue '%4'..maxValue '%5' 范围")
                                      .arg(entity, field, text, parameter.minValue, parameter.maxValue));
            }
        };
        checkValue(QStringLiteral("defaultValue"), parameter.defaultValue);
        checkValue(QStringLiteral("currentValue"), parameter.currentValue);
    }
}

void validateOpcServerConfiguration(const ProjectRuntimeConfig& cfg, QStringList& errors)
{
    if (!cfg.opcServer.enabled) {
        return;
    }

    const QString entity = QStringLiteral("opcServer (OPC 服务器)");
    if (cfg.opcServer.publishIntervalMs < 10 || cfg.opcServer.publishIntervalMs > 60000) {
        appendIntegerRangeError(errors, entity, QStringLiteral("publishIntervalMs"),
                                QString::number(cfg.opcServer.publishIntervalMs), 10, 60000);
    }
    if (cfg.opcServer.timeoutMs < 1 || cfg.opcServer.timeoutMs > 600000) {
        appendIntegerRangeError(errors, entity, QStringLiteral("timeoutMs"),
                                QString::number(cfg.opcServer.timeoutMs), 1, 600000);
    }
    if (cfg.opcServer.reconnectDelayMs < 0 || cfg.opcServer.reconnectDelayMs > 600000) {
        appendIntegerRangeError(errors, entity, QStringLiteral("reconnectDelayMs"),
                                QString::number(cfg.opcServer.reconnectDelayMs), 0, 600000);
    }
    if (cfg.opcServer.retries < 0 || cfg.opcServer.retries > 100) {
        appendIntegerRangeError(errors, entity, QStringLiteral("retries"),
                                QString::number(cfg.opcServer.retries), 0, 100);
    }
    if (cfg.opcServer.maxRegistersPerRequest < 1 || cfg.opcServer.maxRegistersPerRequest > 125) {
        appendIntegerRangeError(errors, entity, QStringLiteral("maxRegistersPerRequest"),
                                QString::number(cfg.opcServer.maxRegistersPerRequest), 1, 125);
    }
}

void validateTransportConfiguration(const ProjectRuntimeConfig& cfg, QStringList& errors)
{
    QVariantMap parameters = cfg.transport.parameters;
    for (auto it = cfg.commParameters.constBegin(); it != cfg.commParameters.constEnd(); ++it) {
        if (!parameters.contains(it.key())) {
            parameters.insert(it.key(), it.value());
        }
    }

    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        const QString key = it.key().trimmed();
        const QString normalizedKey = key.toLower();
        int minimum = 0;
        int maximum = std::numeric_limits<int>::max();
        bool recognized = true;
        if (normalizedKey == QStringLiteral("timeout")
                || normalizedKey == QStringLiteral("timeoutms")
                || normalizedKey == QStringLiteral("responsetimeout")) {
            minimum = 1;
            maximum = 600000;
        } else if (normalizedKey == QStringLiteral("retry")
                   || normalizedKey == QStringLiteral("retries")
                   || normalizedKey == QStringLiteral("retrycount")) {
            minimum = 0;
            maximum = 100;
        } else if (normalizedKey == QStringLiteral("databits")) {
            minimum = 5;
            maximum = 8;
        } else if (normalizedKey == QStringLiteral("stopbits")) {
            minimum = 1;
            maximum = 2;
        } else if (normalizedKey == QStringLiteral("baudrate")
                   || normalizedKey == QStringLiteral("baud")) {
            minimum = 1;
            maximum = 4000000;
        } else {
            recognized = false;
        }
        if (!recognized) {
            continue;
        }

        int parsedValue = 0;
        const QString entity = QStringLiteral("transport.parameters");
        const QString field = key.isEmpty() ? QStringLiteral("<空字段名>") : key;
        if (!variantToInteger(it.value(), &parsedValue)) {
            appendUniqueError(errors,
                              QStringLiteral("%1 字段 '%2' 值 %3 必须可转换为整数且范围为%4..%5")
                                  .arg(entity, field, variantValueDescription(it.value()))
                                  .arg(minimum)
                                  .arg(maximum));
        } else if (parsedValue < minimum || parsedValue > maximum) {
            appendIntegerRangeError(errors, entity, field,
                                    variantValueDescription(it.value()), minimum, maximum);
        }
    }
}


}
