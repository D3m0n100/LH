/**
 * @file ProjectController.cpp
 * @brief 项目控制器实现
 */

#include "ProjectController.h"
#include "DslScriptEditor.h"
#include "Common.h"
#include "TextEncoding.h"

#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSettings>
#include <QTextStream>
#include <QUuid>
#include <QDateTime>
#include <QSet>
#include <QMap>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

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

bool controllerConfigurationEnabled(const ProjectRuntimeConfig& cfg)
{
    return !cfg.controller.model.trimmed().isEmpty()
            || !cfg.controller.routingParameters.isEmpty()
            || cfg.controller.targetRoutingMode.compare(QStringLiteral("unknown"), Qt::CaseInsensitive) != 0
            || cfg.controller.modbusSlaveId != ControllerConfig().modbusSlaveId
            || cfg.target.linkProtocol.compare(QStringLiteral("unknown"), Qt::CaseInsensitive) != 0;
}

bool isContainedProjectPath(const QString& projectPath, const QString& configuredPath)
{
    if (projectPath.trimmed().isEmpty() || configuredPath.trimmed().isEmpty())
        return false;

    const QString root = QFileInfo(projectPath).canonicalFilePath();
    if (root.isEmpty())
        return false;
    const QString absolute = QFileInfo(configuredPath).isRelative()
            ? QDir(root).absoluteFilePath(configuredPath)
            : QFileInfo(configuredPath).absoluteFilePath();
    const QString clean = QDir::cleanPath(absolute);
    const QString relative = QDir(root).relativeFilePath(clean);
    if (relative == QStringLiteral("..")
            || relative.startsWith(QStringLiteral("../"))
            || relative.startsWith(QStringLiteral("..\\"))
            || QDir::isAbsolutePath(relative)) {
        return false;
    }

    const QFileInfo info(clean);
    if (info.exists()) {
        const QString canonical = info.canonicalFilePath();
        const QString canonicalRelative = QDir(root).relativeFilePath(canonical);
        return !canonical.isEmpty()
                && canonicalRelative != QStringLiteral("..")
                && !canonicalRelative.startsWith(QStringLiteral("../"))
                && !canonicalRelative.startsWith(QStringLiteral("..\\"))
                && !QDir::isAbsolutePath(canonicalRelative);
    }

    QDir parent = info.dir();
    while (!parent.exists() && parent.absolutePath() != parent.dir().absolutePath())
        parent = parent.dir();
    const QString canonicalParent = parent.canonicalPath();
    const QString parentRelative = QDir(root).relativeFilePath(canonicalParent);
    return !canonicalParent.isEmpty()
            && parentRelative != QStringLiteral("..")
            && !parentRelative.startsWith(QStringLiteral("../"))
            && !parentRelative.startsWith(QStringLiteral("..\\"))
            && !QDir::isAbsolutePath(parentRelative);
}

QString relativeProjectPath(const QString& projectPath, const QString& configuredPath)
{
    if (!isContainedProjectPath(projectPath, configuredPath))
        return QString();
    const QString absolute = QFileInfo(configuredPath).isRelative()
            ? QDir(projectPath).absoluteFilePath(configuredPath)
            : QFileInfo(configuredPath).absoluteFilePath();
    return QDir(projectPath).relativeFilePath(QDir::cleanPath(absolute));
}

bool normalizeDownloadArtifactPaths(const QString& projectPath,
                                    ProjectRuntimeConfig* config,
                                    QString* errorMessage)
{
    if (!config)
        return true;

    QString normalizedFilePath;
    if (!config->downloadArtifact.filePath.trimmed().isEmpty()) {
        normalizedFilePath = relativeProjectPath(projectPath,
                                                  config->downloadArtifact.filePath);
        if (normalizedFilePath.isEmpty()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("下载产物路径越出项目根目录：%1")
                        .arg(config->downloadArtifact.filePath);
            return false;
        }
    }

    QVariantMap normalizedMetadata = config->downloadArtifact.metadata;
    const QStringList pathKeys = {
        QStringLiteral("downloadProfilePath"),
        QStringLiteral("downloadProfileSourcePath"),
        QStringLiteral("profileJsonPath"),
        QStringLiteral("profilePath"),
        QStringLiteral("profileSourcePath"),
        QStringLiteral("sourceProfilePath"),
        QStringLiteral("runtimeManifestPath"),
        QStringLiteral("manifestPath"),
        QStringLiteral("runtimePointsPath")
    };
    for (const QString& key : pathKeys) {
        const QString value = normalizedMetadata.value(key).toString().trimmed();
        if (value.isEmpty())
            continue;
        const QString relative = relativeProjectPath(projectPath, value);
        if (relative.isEmpty()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("下载发布路径越出项目根目录（%1）：%2")
                        .arg(key, value);
            return false;
        }
        normalizedMetadata.insert(key, relative);
    }

    if (!normalizedFilePath.isEmpty())
        config->downloadArtifact.filePath = normalizedFilePath;
    config->downloadArtifact.metadata = normalizedMetadata;
    return true;
}

} // namespace

// ================= 构造 / 析构 =================

ProjectController::ProjectController(QObject* parent)
    : QObject(parent)
    , m_defaultProjectDir(QDir::homePath())
{
    LOG_DEBUG("ProjectController 已创建");
}

ProjectController::~ProjectController()
{
    LOG_DEBUG("ProjectController 已销毁");
}


// ================= DSL 编辑器绑定 =================

void ProjectController::setDslEditor(DslScriptEditor* editor)
{
    m_dslEditor = editor;
    // Sync mappings to editor once the editor is available.
    syncDslMappingsToEditor();
}

void ProjectController::setCurrentScriptFile(const QString& scriptFile)
{
    if (m_currentScriptFile == scriptFile) {
        return;
    }

    m_currentScriptFile = scriptFile;
    syncScriptConfigFields();
}

// ================= 项目状态 =================

void ProjectController::setModified(bool modified)
{
    if (m_modified != modified) {
        m_modified = modified;
        emit modifiedChanged(modified);
    }
}

// ================= 最近项目列表 =================

void ProjectController::loadRecentProjects()
{
    QSettings settings("ServoValve", "ControlPlatform");
    m_recentProjects = settings.value("recentProjects").toStringList();
    LOG_DEBUG(QString("已加载 %1 个最近项目").arg(m_recentProjects.size()));
}

void ProjectController::saveRecentProjects()
{
    QSettings settings("ServoValve", "ControlPlatform");
    settings.setValue("recentProjects", m_recentProjects);
    LOG_DEBUG("已保存最近项目列表");
}

void ProjectController::addToRecentProjects(const QString& path)
{
    m_recentProjects.removeAll(path);
    m_recentProjects.prepend(path);
    
    while (m_recentProjects.size() > MAX_RECENT_PROJECTS) {
        m_recentProjects.removeLast();
    }
    
    emit recentProjectsChanged(m_recentProjects);
}

// ================= 项目操作 =================

void ProjectController::createNewProject()
{
    QString projectName;
    bool accepted = false;
    emit projectNameRequired(projectName, accepted);

    if (!accepted || projectName.isEmpty()) {
        return;
    }

    const QString normalizedProjectName = projectName.trimmed();
    if (normalizedProjectName.isEmpty() || normalizedProjectName == QStringLiteral(".")
            || normalizedProjectName == QStringLiteral("..")
            || normalizedProjectName.contains(QLatin1Char('/'))
            || normalizedProjectName.contains(QLatin1Char('\\'))) {
        emit errorOccurred("创建失败", "项目名称不能包含路径分隔符或仅由点组成。");
        return;
    }

    QString projectDir;
    emit directorySelectionRequired("选择项目保存位置", m_defaultProjectDir,
                                    projectDir, accepted);

    if (!accepted || projectDir.isEmpty()) {
        return;
    }

    const QString fullPath = QDir(projectDir).absoluteFilePath(normalizedProjectName);
    if (QFileInfo::exists(fullPath)) {
        emit errorOccurred("创建失败", "同名项目已存在，未覆盖现有文件。");
        return;
    }
    if (!confirmPendingChanges()) {
        return;
    }
    QDir dir;
    if (!dir.mkpath(fullPath)) {
        emit errorOccurred("错误", "无法创建项目目录");
        return;
    }

    const QString previousProject = m_currentProject;
    const QString previousScript = m_currentScriptFile;
    const ProjectRuntimeConfig previousConfig = m_runtimeConfig;
    const auto restorePrevious = [&]() {
        m_currentProject = previousProject;
        m_currentScriptFile = previousScript;
        m_runtimeConfig = previousConfig;
    };
    m_runtimeConfig.clear();
    m_runtimeConfig.projectName = normalizedProjectName;
    m_runtimeConfig.applyEmptyTemplates();
    m_runtimeConfig.lastModified = QDateTime::currentDateTime();

    const QString scriptPath = fullPath + "/main.lh";
    QSaveFile scriptFile(scriptPath);
    if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&scriptFile);
        TextEncoding::setUtf8(stream);
        stream << QString::fromUtf8(u8"// ") << normalizedProjectName << QString::fromUtf8(u8" - LH脚本\n");
        stream << QString::fromUtf8(u8"// 创建时间: ")
               << QDateTime::currentDateTime().toString(Qt::ISODate)
               << QString::fromUtf8(u8"\n");
        stream << "\n";
        stream << QString::fromUtf8(u8"// 示例：从左侧函数列表拖拽功能块，或按以下 LH 结构编写\n");
        stream << QString::fromUtf8(u8"PROGRAM Main\n");
        stream << QString::fromUtf8(u8"VAR\n");
        stream << QString::fromUtf8(u8"    system_1 : System;\n");
        stream << QString::fromUtf8(u8"    drv_ai_1 : DrvAI;\n");
        stream << QString::fromUtf8(u8"    add_1 : Add;\n");
        stream << QString::fromUtf8(u8"END_VAR\n\n");
        stream << QString::fromUtf8(u8"system_1(\n");
        stream << QString::fromUtf8(u8"    Author := 1,\n");
        stream << QString::fromUtf8(u8"    Config := 100,\n");
        stream << QString::fromUtf8(u8"    Date := 2601\n");
        stream << QString::fromUtf8(u8");\n\n");
        stream << QString::fromUtf8(u8"drv_ai_1(\n");
        stream << QString::fromUtf8(u8"    NumChannels := 1,\n");
        stream << QString::fromUtf8(u8"    InputNum := 0,\n");
        stream << QString::fromUtf8(u8"    DivisionNum := 4096\n");
        stream << QString::fromUtf8(u8");\n\n");
        stream << QString::fromUtf8(u8"add_1();\n\n");
        stream << QString::fromUtf8(u8"END_PROGRAM\n");
        stream.flush();
        if (stream.status() != QTextStream::Ok || !scriptFile.commit()) {
            scriptFile.cancelWriting();
            restorePrevious();
            const bool cleaned = QDir(projectDir).rmdir(normalizedProjectName);
            emit errorOccurred("创建失败", cleaned
                               ? QStringLiteral("无法写入初始 DSL 脚本，已清理项目目录。")
                               : QStringLiteral("无法写入初始 DSL 脚本，项目目录可能残留: %1").arg(fullPath));
            return;
        }
    } else {
        restorePrevious();
        const bool cleaned = QDir(projectDir).rmdir(normalizedProjectName);
        const QString message = QString("无法创建初始 DSL 脚本: %1").arg(scriptFile.errorString());
        emit errorOccurred("创建失败", cleaned
                           ? message + QStringLiteral("，已清理项目目录。")
                           : message + QStringLiteral("，项目目录可能残留: %1").arg(fullPath));
        return;
    }

    m_runtimeConfig.dslScriptPath = scriptPath;
    m_runtimeConfig.mainScriptPath = scriptPath;
    m_runtimeConfig.scriptFiles = QStringList{scriptPath};
    m_currentProject = fullPath;
    m_currentScriptFile = scriptPath;
    if (!saveProjectConfig(fullPath)) {
        restorePrevious();
        const bool scriptRemoved = !QFile::exists(scriptPath) || QFile::remove(scriptPath);
        const bool directoryRemoved = scriptRemoved && QDir(projectDir).rmdir(normalizedProjectName);
        emit errorOccurred("创建失败", scriptRemoved && directoryRemoved
                           ? QStringLiteral("项目配置写入失败，已清理本次创建的项目文件。")
                           : QStringLiteral("项目配置写入失败，项目未打开，可能残留: %1").arg(fullPath));
        return;
    }

    QFile file(scriptPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString content = TextEncoding::decodeUtf8WithLocalFallback(file.readAll());
        file.close();
        emit scriptLoadRequired(scriptPath, content);
    }

    setModified(false);
    addToRecentProjects(fullPath);

    emit logMessage(timestampedMessage(QString("已创建新项目: %1").arg(normalizedProjectName)));
    emit projectCreated(fullPath, normalizedProjectName);
}
void ProjectController::openProject()
{
    QString projectPath;
    bool accepted = false;
    emit directorySelectionRequired("打开项目", m_defaultProjectDir,
                                     projectPath, accepted);
    
    if (!accepted || projectPath.isEmpty()) {
        return;
    }
    
    openProjectFromPath(projectPath);
}

bool ProjectController::openProjectFromPath(const QString& projectPath)
{
    // Check whether project config exists.
    QString configPath = projectPath + "/project_config.json";
    if (!QFile::exists(configPath)) {
        emit warningOccurred("警告", 
            "所选目录不是有效项目目录（缺少 project_config.json）。");
        return false;
    }

    ProjectRuntimeConfig loadedConfig;
    if (!loadProjectConfig(projectPath, &loadedConfig)) {
        return false;
    }

    const QString projectRoot = QFileInfo(projectPath).canonicalFilePath();
    if (projectRoot.isEmpty() || !QFileInfo(projectRoot).isDir()) {
        emit errorOccurred("打开失败", QString("项目根目录不存在或不可解析: %1").arg(projectPath));
        return false;
    }

    QString artifactPathError;
    if (!normalizeDownloadArtifactPaths(projectRoot,
                                        &loadedConfig,
                                        &artifactPathError)) {
        emit errorOccurred("打开失败", artifactPathError);
        return false;
    }

    const QDir projectDir(projectRoot);
    const auto isWithinProjectRoot = [&](const QString& path) {
        const QString relativePath = projectDir.relativeFilePath(path);
        return relativePath != QStringLiteral("..")
                && !relativePath.startsWith(QStringLiteral("../"))
                && !relativePath.startsWith(QStringLiteral("..\\"))
                && !QDir::isAbsolutePath(relativePath);
    };
    const auto resolveProjectScript = [&](const QString& configuredPath,
                                          QString* resolvedPath) {
        if (configuredPath.trimmed().isEmpty()) {
            emit errorOccurred("打开失败", "项目脚本路径不能为空。");
            return false;
        }

        const QString absolutePath = QDir::isRelativePath(configuredPath)
                ? projectDir.absoluteFilePath(configuredPath)
                : QFileInfo(configuredPath).absoluteFilePath();
        const QString cleanPath = QDir::cleanPath(absolutePath);
        if (!isWithinProjectRoot(cleanPath)) {
            emit errorOccurred("打开失败",
                               QString("项目脚本路径越出项目根目录: %1").arg(configuredPath));
            return false;
        }

        const QFileInfo candidateInfo(cleanPath);
        QString canonicalPath;
        if (candidateInfo.isSymLink()) {
            canonicalPath = candidateInfo.canonicalFilePath();
            if (canonicalPath.isEmpty() || !isWithinProjectRoot(canonicalPath)) {
                emit errorOccurred("打开失败",
                                   QString("项目脚本路径的符号链接目标无效或越出项目根目录: %1")
                                       .arg(configuredPath));
                return false;
            }
        } else if (candidateInfo.exists()) {
            canonicalPath = candidateInfo.canonicalFilePath();
            if (canonicalPath.isEmpty() || !isWithinProjectRoot(canonicalPath)) {
                emit errorOccurred("打开失败",
                                   QString("项目脚本最终路径无效或越出项目根目录: %1")
                                       .arg(configuredPath));
                return false;
            }
        } else {
            QDir parentDir = candidateInfo.dir();
            bool foundExistingParent = false;
            while (true) {
                const QFileInfo parentInfo(parentDir.absolutePath());
                if (parentInfo.isSymLink()) {
                    const QString parentCanonical = parentInfo.canonicalFilePath();
                    if (parentCanonical.isEmpty() || !isWithinProjectRoot(parentCanonical)) {
                        emit errorOccurred("打开失败",
                                           QString("项目脚本父目录的符号链接目标无效或越出项目根目录: %1")
                                               .arg(configuredPath));
                        return false;
                    }
                }
                if (parentInfo.exists()) {
                    const QString parentCanonical = parentInfo.canonicalFilePath();
                    if (parentCanonical.isEmpty() || !isWithinProjectRoot(parentCanonical)) {
                        emit errorOccurred("打开失败",
                                           QString("项目脚本父目录无效或越出项目根目录: %1")
                                               .arg(configuredPath));
                        return false;
                    }
                    foundExistingParent = true;
                    break;
                }

                const QString currentParent = parentDir.absolutePath();
                const QString nextParent = QFileInfo(currentParent).dir().absolutePath();
                if (nextParent == currentParent)
                    break;
                parentDir.setPath(nextParent);
            }
            if (!foundExistingParent) {
                emit errorOccurred("打开失败",
                                   QString("项目脚本父目录不存在或不可解析: %1").arg(configuredPath));
                return false;
            }
        }

        const QString pathForExtension = canonicalPath.isEmpty() ? cleanPath : canonicalPath;
        if (QFileInfo(pathForExtension).suffix().compare(QStringLiteral("lh"), Qt::CaseInsensitive) != 0) {
            emit errorOccurred("打开失败",
                               QString("项目脚本必须使用 .lh 后缀: %1").arg(configuredPath));
            return false;
        }

        *resolvedPath = canonicalPath.isEmpty() ? cleanPath : canonicalPath;
        return true;
    };

    QString mainScript = loadedConfig.mainScriptPath;
    if (mainScript.isEmpty()) {
        mainScript = QStringLiteral("main.lh");
    }
    if (!resolveProjectScript(mainScript, &mainScript)) {
        return false;
    }
    QFile scriptFile(mainScript);
    if (!QFileInfo::exists(mainScript)
            || !scriptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred("打开失败", QString("主 DSL 脚本不存在或不可读: %1").arg(mainScript));
        return false;
    }
    const QString content = TextEncoding::decodeUtf8WithLocalFallback(scriptFile.readAll());
    scriptFile.close();
    loadedConfig.mainScriptPath = mainScript;
    loadedConfig.dslScriptPath = mainScript;
    QStringList normalizedScripts;
    for (const QString& script : loadedConfig.scriptFiles) {
        QString normalized;
        if (!resolveProjectScript(script, &normalized)) {
            return false;
        }
        normalizedScripts.append(normalized);
    }
    normalizedScripts.removeAll(mainScript);
    normalizedScripts.prepend(mainScript);
    loadedConfig.scriptFiles = normalizedScripts;

    if (!confirmPendingChanges()) {
        return false;
    }

    // Only commit the new project after config and main script have both loaded.
    m_currentProject = projectRoot;
    m_currentScriptFile = mainScript;
    m_runtimeConfig = loadedConfig;
    syncDslMappingsToEditor();
    emit scriptLoadRequired(m_currentScriptFile, content);
    
    setModified(false);
    addToRecentProjects(projectPath);
    
    emit logMessage(timestampedMessage(QString("已打开项目: %1").arg(m_runtimeConfig.projectName)));
    emit projectOpened(m_runtimeConfig);
    
    return true;
}

bool ProjectController::saveProject()
{
    if (m_currentProject.isEmpty()) {
        emit warningOccurred("警告", "没有打开的项目。");
        return false;
    }
    
    // Sync mappings from editor first.
    syncDslMappingsFromEditor();
    syncDslMappingsToEditor();

    if (!m_currentScriptFile.isEmpty()
            && !isContainedProjectPath(m_currentProject, m_currentScriptFile)) {
        emit errorOccurred("保存失败", "当前 DSL 脚本路径越出项目根目录，已取消保存。");
        return false;
    }
    const QStringList configuredScriptPaths = {
        m_runtimeConfig.mainScriptPath,
        m_runtimeConfig.dslScriptPath
    };
    for (const QString& script : configuredScriptPaths) {
        if (!script.isEmpty() && !isContainedProjectPath(m_currentProject, script)) {
            emit errorOccurred("保存失败", QString("项目脚本路径越出项目根目录，已取消保存: %1").arg(script));
            return false;
        }
    }
    for (const QString& script : m_runtimeConfig.scriptFiles) {
        if (!isContainedProjectPath(m_currentProject, script)) {
            emit errorOccurred("保存失败", QString("项目脚本路径越出项目根目录，已取消保存: %1").arg(script));
            return false;
        }
    }

    QString artifactPathError;
    if (!normalizeDownloadArtifactPaths(m_currentProject,
                                        &m_runtimeConfig,
                                        &artifactPathError)) {
        emit errorOccurred("保存失败", artifactPathError);
        return false;
    }

    const bool hasDslScript = m_dslEditor && !m_currentScriptFile.isEmpty();
    QString savedScript;
    QByteArray previousScriptBytes;
    const bool scriptExistedBefore = hasDslScript && QFile::exists(m_currentScriptFile);
    bool hadPreviousScript = false;
    if (scriptExistedBefore) {
        QFile previousScript(m_currentScriptFile);
        if (!previousScript.open(QIODevice::ReadOnly)) {
            emit errorOccurred("保存失败", QString("无法读取原 DSL 脚本，已取消保存: %1")
                               .arg(previousScript.errorString()));
            return false;
        }
        previousScriptBytes = previousScript.readAll();
        hadPreviousScript = true;
    }
    if (!saveDslScript(savedScript)) {
        return false;
    }

    // 保存项目配置
    if (!saveProjectConfig(m_currentProject)) {
        bool recovered = true;
        if (hadPreviousScript) {
            QSaveFile rollback(m_currentScriptFile);
            if (!rollback.open(QIODevice::WriteOnly)
                    || rollback.write(previousScriptBytes) != previousScriptBytes.size()
                    || !rollback.commit()) {
                recovered = false;
            }
        } else if (hasDslScript && !scriptExistedBefore && !m_currentScriptFile.isEmpty()) {
            recovered = QFile::remove(m_currentScriptFile);
        }
        if (!recovered) {
            emit errorOccurred("保存失败", "项目配置保存失败，脚本回滚或清理失败；请保留当前编辑内容并检查磁盘文件。");
        }
        return false;
    }

    if (hasDslScript && m_dslEditor) {
        if (savedScript != m_dslEditor->currentScript()) {
            m_dslEditor->setScript(savedScript);
        }
        m_dslEditor->setModified(false);
    }

    setModified(false);
    
    emit logMessage(timestampedMessage("项目已保存"));
    emit projectSaved();
    
    return true;
}

bool ProjectController::closeProject()
{
    if (!confirmPendingChanges()) {
        return false;
    }
    
    m_currentProject.clear();
    m_currentScriptFile.clear();
    m_runtimeConfig.clear();
    
    emit editorClearRequired();
    
    setModified(false);
    
    emit logMessage(timestampedMessage("项目已关闭"));
    emit projectClosed();

    return true;
}

void ProjectController::openRecentProject(const QString& path)
{
    if (!QDir(path).exists()) {
        emit warningOccurred("警告", "项目目录不存在: " + path);
        m_recentProjects.removeAll(path);
        emit recentProjectsChanged(m_recentProjects);
        return;
    }
    
    openProjectFromPath(path);
}

// ================= 配置读写 =================
bool ProjectController::loadProjectConfig(const QString& projectDir, ProjectRuntimeConfig* loadedConfig)
{
    QString configPath = projectDir + "/project_config.json";
    QFile file(configPath);
    
    if (!file.exists()) {
        emit logMessage(timestampedMessage(QString("项目配置文件不存在: %1").arg(configPath)));
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        emit logMessage(timestampedMessage(QString("无法打开项目配置文件: %1").arg(file.errorString())));
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        emit logMessage(timestampedMessage(QString("解析项目配置文件失败: %1").arg(error.errorString())));
        return false;
    }
    if (!doc.isObject()) {
        const QString actualType = doc.isArray() ? QStringLiteral("<数组>") : QStringLiteral("null");
        const QString details = QStringLiteral("配置字段 'project_config' 的值 %1 类型错误，应为对象")
                .arg(actualType);
        emit logMessage(timestampedMessage(QStringLiteral("项目配置结构校验失败: %1").arg(details)));
        emit errorOccurred(QStringLiteral("加载失败"), details);
        return false;
    }

    QStringList shapeErrors;
    if (!validateProjectConfigShape(doc.object(), shapeErrors)) {
        const QString details = shapeErrors.join(QStringLiteral("\n"));
        emit logMessage(timestampedMessage(QStringLiteral("项目配置结构校验失败: %1").arg(details)));
        emit errorOccurred(QStringLiteral("加载失败"), details);
        return false;
    }
    
    ProjectRuntimeConfig parsed = ProjectRuntimeConfig::fromJson(doc.object());
    
    emit logMessage(timestampedMessage(
        QString("已加载项目配置: %1 (包含 %2 个监控通道, %3 个组态映射)")
            .arg(parsed.projectName)
            .arg(parsed.providers.size())
            .arg(parsed.dslMappings.size())));
    if (loadedConfig) {
        *loadedConfig = parsed;
    } else {
        m_runtimeConfig = parsed;
        syncDslMappingsToEditor();
    }
    
    return true;
}

bool ProjectController::saveProjectConfig(const QString& projectDir)
{
    syncScriptConfigFields();
    QString artifactPathError;
    if (!normalizeDownloadArtifactPaths(projectDir,
                                        &m_runtimeConfig,
                                        &artifactPathError)) {
        emit errorOccurred("保存失败", artifactPathError);
        return false;
    }
    const QDateTime previousLastModified = m_runtimeConfig.lastModified;
    m_runtimeConfig.lastModified = QDateTime::currentDateTime();
    
    QString configPath = projectDir + "/project_config.json";
    QSaveFile file(configPath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        m_runtimeConfig.lastModified = previousLastModified;
        emit logMessage(timestampedMessage(QString("无法保存项目配置文件: %1").arg(file.errorString())));
        return false;
    }
    QJsonDocument doc(m_runtimeConfig.toJson());
    const QByteArray data = doc.toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        const QString error = file.errorString();
        file.cancelWriting();
        m_runtimeConfig.lastModified = previousLastModified;
        emit logMessage(timestampedMessage(QString("无法保存项目配置文件: %1").arg(error)));
        return false;
    }

    if (!file.commit()) {
        m_runtimeConfig.lastModified = previousLastModified;
        emit logMessage(timestampedMessage(QString("无法保存项目配置文件: %1").arg(file.errorString())));
        return false;
    }
    
    emit logMessage(timestampedMessage("项目配置已保存"));
    
    return true;
}

bool ProjectController::saveDslScript(QString& savedScript)
{
    savedScript.clear();
    if (!m_dslEditor) {
        return true;
    }

    if (m_currentScriptFile.isEmpty()) {
        if (m_dslEditor->isModified()) {
            emit errorOccurred("保存失败", "当前 DSL 脚本没有可保存的文件路径。");
            return false;
        }
        return true;
    }

    savedScript = m_dslEditor->scriptForSave();
    QSaveFile file(m_currentScriptFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred("保存失败",
                           QString("无法保存 DSL 脚本: %1\n%2")
                               .arg(m_currentScriptFile, file.errorString()));
        return false;
    }

    QTextStream stream(&file);
    TextEncoding::setUtf8(stream);
    stream << savedScript;
    stream.flush();
    if (stream.status() != QTextStream::Ok) {
        file.cancelWriting();
        emit errorOccurred("保存失败",
                           QString("写入 DSL 脚本失败: %1").arg(m_currentScriptFile));
        return false;
    }

    if (!file.commit()) {
        emit errorOccurred("保存失败",
                           QString("提交 DSL 脚本失败: %1\n%2")
                               .arg(m_currentScriptFile, file.errorString()));
        return false;
    }

    return true;
}

bool ProjectController::confirmPendingChanges()
{
    if (!hasOpenProject() || !m_modified) {
        return true;
    }

    bool shouldSave = false;
    bool cancelled = false;
    emit saveConfirmationRequired(shouldSave, cancelled);

    if (cancelled) {
        return false;
    }

    return !shouldSave || saveProject();
}

void ProjectController::syncScriptConfigFields()
{
    auto isLhScript = [](const QString& path) {
        return QFileInfo(path).suffix().compare(QStringLiteral("lh"), Qt::CaseInsensitive) == 0;
    };

    if (!m_currentScriptFile.isEmpty()) {
        const QString relative = relativeProjectPath(m_currentProject, m_currentScriptFile);
        if (!relative.isEmpty())
            m_runtimeConfig.mainScriptPath = relative;
    }

    if (!m_runtimeConfig.mainScriptPath.isEmpty() && !isLhScript(m_runtimeConfig.mainScriptPath)) {
        m_runtimeConfig.mainScriptPath.clear();
    }

    if (m_runtimeConfig.mainScriptPath.isEmpty() && !m_currentProject.isEmpty()) {
        const QString mainLh = QDir(m_currentProject).absoluteFilePath(QStringLiteral("main.lh"));
        if (QFileInfo::exists(mainLh)) {
            m_runtimeConfig.mainScriptPath = QStringLiteral("main.lh");
        }
    }

    QStringList lhScripts;
    for (const QString& script : m_runtimeConfig.scriptFiles) {
        const QString relative = relativeProjectPath(m_currentProject, script);
        if (isLhScript(relative))
            lhScripts.append(relative);
    }
    m_runtimeConfig.scriptFiles = lhScripts;

    if (!m_runtimeConfig.mainScriptPath.isEmpty()) {
        m_runtimeConfig.scriptFiles.removeAll(m_runtimeConfig.mainScriptPath);
        m_runtimeConfig.scriptFiles.prepend(m_runtimeConfig.mainScriptPath);
    }

    m_runtimeConfig.dslScriptPath = m_runtimeConfig.mainScriptPath;
}

// ================= DSL 映射同步 =================

void ProjectController::syncDslMappingsFromEditor()
{
    if (!m_dslEditor) {
        return;
    }

    // Get insert records from editor (including mappingId).
    QList<DslInsertRecord> records = m_dslEditor->insertRecords();
    if (records.isEmpty()) {
        return;
    }

    for (const auto& record : records) {
        const QString rid = record.mappingId;

        auto it = std::find_if(m_runtimeConfig.dslMappings.begin(), m_runtimeConfig.dslMappings.end(),
                               [&](const DslMappingEntry& m) {
                                   if (!rid.isEmpty()) {
                                       return m.id == rid;
                                   }
                                   return m.snippetId == record.snippetId && m.lineNumber == record.lineNumber;
                               });

        if (it == m_runtimeConfig.dslMappings.end()) {
            m_runtimeConfig.dslMappings.append(createMappingFromInsertRecord(record));
        } else {
            // 更新已有映射字段，防止信息过期
            if (!rid.isEmpty()) {
                it->id = rid;
            }
            if (!record.snippetId.isEmpty()) {
                it->snippetId = record.snippetId;
            }
            if (!record.snippetName.isEmpty()) {
                it->snippetName = record.snippetName;
            }
            if (record.lineNumber > 0) {
                it->lineNumber = record.lineNumber;
            }
            if (!record.generatedCode.isEmpty()) {
                it->generatedCode = record.generatedCode;
            }
        }
    }

    // De-duplicate by mapping id.
    QSet<QString> seen;
    QList<DslMappingEntry> dedup;
    dedup.reserve(m_runtimeConfig.dslMappings.size());

    for (const auto& m : m_runtimeConfig.dslMappings) {
        DslMappingEntry copy = m;
        if (copy.id.isEmpty()) {
            copy.id = DslMappingEntry::generateId();
        }
        if (seen.contains(copy.id)) {
            continue;
        }
        seen.insert(copy.id);
        dedup.append(copy);
    }
    m_runtimeConfig.dslMappings = dedup;
}

void ProjectController::syncDslMappingsToEditor()
{
    if (!m_dslEditor) {
        return;
    }

    // 1) Scan script markers: mappingId -> marker line (1-based).
    QMap<QString, int> markerMap = m_dslEditor->scanDslMappingMarkers();

    // 2) Repair/align runtimeConfig.dslMappings and ensure ids.
    QSet<QString> mappingIds;
    mappingIds.reserve(m_runtimeConfig.dslMappings.size());

    for (auto& mapping : m_runtimeConfig.dslMappings) {
        if (mapping.id.isEmpty()) {
            mapping.id = DslMappingEntry::generateId();
        }
        mappingIds.insert(mapping.id);

        // 若脚本中存在 marker，则以 marker 行号为准校正 lineNumber，代码通常在下一行。
        if (markerMap.contains(mapping.id)) {
            const int markerLine = markerMap.value(mapping.id);
            mapping.lineNumber = markerLine + 1;
            continue;
        }

        // Marker comments are legacy-only and must not be written back to the DSL script.
    }

    // 3) Recover minimal mappings from markers found in script.
    const QStringList lines = m_dslEditor->currentScript().split('\n');

    auto parseDslCall = [&](int codeLine, DslMappingEntry& out) {
        if (codeLine < 1 || codeLine > lines.size()) {
            return;
        }
        const QString line = lines.at(codeLine - 1);

    // 轻量解析：xxx = analog_input(...)
        static const QRegularExpression re(R"(^\s*([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)\s*\()");
        QRegularExpressionMatch m = re.match(line);
        if (m.hasMatch()) {
            out.channelName = m.captured(1);
            out.snippetId = m.captured(2);
            out.snippetName = out.snippetId;
        }
    };

    for (auto it = markerMap.begin(); it != markerMap.end(); ++it) {
        const QString id = it.key();
        const int markerLine = it.value();
        if (mappingIds.contains(id)) {
            continue;
        }

        DslMappingEntry entry;
        entry.id = id;
        entry.lineNumber = markerLine + 1;
        entry.createTime = QDateTime::currentDateTime();
        entry.metadata["recoveredFromScript"] = true;

        parseDslCall(entry.lineNumber, entry);

        m_runtimeConfig.dslMappings.append(entry);
        mappingIds.insert(id);
    }

    // 4) Sync mappings back to editor for highlighting/navigation.
    m_dslEditor->setDslMappings(m_runtimeConfig.dslMappings);
}

DslMappingEntry ProjectController::createMappingFromInsertRecord(const DslInsertRecord& record)
{
    DslMappingEntry entry;
        entry.id = record.mappingId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : record.mappingId;
    entry.snippetId = record.snippetId;
    entry.snippetName = record.snippetName;
    entry.lineNumber = record.lineNumber;
    entry.generatedCode = record.generatedCode;
    entry.createTime = record.insertTime;
    
    // 从 snippet 获取额外信息
    if (m_dslEditor) {
        FunctionSnippet snippet = m_dslEditor->snippetById(record.snippetId);
        entry.unit = snippet.unit;
        entry.periodMs = snippet.defaultPeriodMs;
        
        // 根据组件类型推断通道名和信号路径
        if (!snippet.name.isEmpty()) {
            entry.channelName = QString("%1_%2").arg(snippet.name).arg(record.lineNumber);
            entry.signalPath = QString("%1.%2").arg(snippet.category).arg(snippet.name);
        }
    }
    
    return entry;
}

// ================= 组态合法性校验 =================
bool ProjectController::validateConfiguration(QStringList& errors)
{
    errors.clear();

    bool valid = true;

    if (!checkRequiredFields(m_runtimeConfig, errors)) {
        valid = false;
    }

    if (!checkDuplicateChannelBindings(m_runtimeConfig, errors)) {
        valid = false;
    }

    if (!checkHardwareResourceLimits(m_runtimeConfig, errors)) {
        valid = false;
    }

    for (int i = 0; i < m_runtimeConfig.providers.size(); ++i) {
        const MonitorProviderRuntimeConfig& provider = m_runtimeConfig.providers.at(i);
        const QString entity = provider.channelName.trimmed().isEmpty()
                ? QStringLiteral("监控通道 #%1").arg(i + 1)
                : QStringLiteral("监控通道 '%1'").arg(provider.channelName);
        if (provider.priority < 0 || provider.priority > 255) {
            appendIntegerRangeError(errors, entity, QStringLiteral("priority"),
                                    QString::number(provider.priority), 0, 255);
            valid = false;
        }
    }

    for (int i = 0; i < m_runtimeConfig.dslMappings.size(); ++i) {
        const DslMappingEntry& mapping = m_runtimeConfig.dslMappings.at(i);
        const QString entity = mapping.id.trimmed().isEmpty()
                ? QStringLiteral("组态映射 #%1").arg(i + 1)
                : QStringLiteral("组态映射 '%1'").arg(mapping.id);
        if (mapping.periodMs < MIN_SAMPLE_PERIOD_MS || mapping.periodMs > MAX_SAMPLE_PERIOD_MS) {
            appendIntegerRangeError(errors, entity, QStringLiteral("periodMs"),
                                    QString::number(mapping.periodMs),
                                    MIN_SAMPLE_PERIOD_MS, MAX_SAMPLE_PERIOD_MS);
            valid = false;
        }
        if (mapping.lineNumber < -1) {
            appendUniqueError(errors,
                              QStringLiteral("%1 字段 'lineNumber' 值 %2 必须大于等于-1")
                                  .arg(entity)
                                  .arg(mapping.lineNumber));
            valid = false;
        }
    }

    if (controllerConfigurationEnabled(m_runtimeConfig)
            && (m_runtimeConfig.controller.modbusSlaveId < 1
                || m_runtimeConfig.controller.modbusSlaveId > 247)) {
        appendIntegerRangeError(errors, QStringLiteral("controller (控制器)"), QStringLiteral("modbusSlaveId"),
                                QString::number(m_runtimeConfig.controller.modbusSlaveId), 1, 247);
        valid = false;
    }

    const int errorCountBeforeExtendedValidation = errors.size();
    validateParameterRanges(m_runtimeConfig, errors);
    validateOpcServerConfiguration(m_runtimeConfig, errors);
    validateTransportConfiguration(m_runtimeConfig, errors);
    if (errors.size() != errorCountBeforeExtendedValidation) {
        valid = false;
    }

    QStringList uniqueErrors;
    for (const QString& error : errors) {
        appendUniqueError(uniqueErrors, error);
    }
    errors = uniqueErrors;

    if (!valid) {
        emit validationFailed(errors);
    }

    return valid;
}

bool ProjectController::checkRequiredFields(const ProjectRuntimeConfig& cfg, QStringList& errors) const
{
    bool valid = true;
    
    if (cfg.projectName.isEmpty()) {
        errors.append("项目名称不能为空");
        valid = false;
    }
    
    for (int i = 0; i < cfg.providers.size(); ++i) {
        const auto& provider = cfg.providers[i];
        
        if (provider.id.isEmpty()) {
            errors.append(QString("监控通道 #%1: ID 不能为空").arg(i + 1));
            valid = false;
        }
        
        if (provider.channelName.isEmpty()) {
            errors.append(QString("监控通道 #%1: 通道名称不能为空").arg(i + 1));
            valid = false;
        }
        
        if (provider.periodMs < MIN_SAMPLE_PERIOD_MS || provider.periodMs > MAX_SAMPLE_PERIOD_MS) {
            appendUniqueError(errors, QString("监控通道 '%1' 字段 'periodMs' 值 %2 必须在 %3-%4 ms 之间")
                          .arg(provider.channelName.isEmpty() ? QString::number(i + 1) : provider.channelName)
                          .arg(provider.periodMs)
                          .arg(MIN_SAMPLE_PERIOD_MS)
                          .arg(MAX_SAMPLE_PERIOD_MS));
            valid = false;
        }
    }
    
    for (int i = 0; i < cfg.dslMappings.size(); ++i) {
        const auto& mapping = cfg.dslMappings[i];
        
        if (mapping.snippetId.isEmpty()) {
            errors.append(QString("组态映射 #%1: 组件 ID 不能为空").arg(i + 1));
            valid = false;
        }
    }

    QSet<QString> variableNames;
    QSet<QString> variableIds;
    for (int i = 0; i < cfg.variables.size(); ++i) {
        const auto& variable = cfg.variables[i];

        if (variable.id.isEmpty()) {
            errors.append(QString("变量 #%1: ID 不能为空").arg(i + 1));
            valid = false;
        } else if (variableIds.contains(variable.id)) {
            errors.append(QString("重复的变量 ID: '%1'").arg(variable.id));
            valid = false;
        } else {
            variableIds.insert(variable.id);
        }

        if (variable.name.isEmpty()) {
            errors.append(QString("变量 #%1: 名称不能为空").arg(i + 1));
            valid = false;
        } else if (variableNames.contains(variable.name)) {
            errors.append(QString("重复的变量名称: '%1'").arg(variable.name));
            valid = false;
        } else {
            variableNames.insert(variable.name);
        }

        if (variable.dataType.isEmpty()) {
            errors.append(QString("变量 '%1': 数据类型不能为空").arg(variable.name.isEmpty() ? QString::number(i + 1) : variable.name));
            valid = false;
        }
    }

    QSet<QString> parameterNames;
    QSet<QString> parameterIds;
    for (int i = 0; i < cfg.parameters.size(); ++i) {
        const auto& parameter = cfg.parameters[i];

        if (parameter.id.isEmpty()) {
            errors.append(QString("参数 #%1: ID 不能为空").arg(i + 1));
            valid = false;
        } else if (parameterIds.contains(parameter.id)) {
            errors.append(QString("重复的参数 ID: '%1'").arg(parameter.id));
            valid = false;
        } else {
            parameterIds.insert(parameter.id);
        }

        if (parameter.name.isEmpty()) {
            errors.append(QString("参数 #%1: 名称不能为空").arg(i + 1));
            valid = false;
        } else if (parameterNames.contains(parameter.name)) {
            errors.append(QString("重复的参数名称: '%1'").arg(parameter.name));
            valid = false;
        } else {
            parameterNames.insert(parameter.name);
        }

        if (parameter.dataType.isEmpty()) {
            errors.append(QString("参数 '%1': 数据类型不能为空").arg(parameter.name.isEmpty() ? QString::number(i + 1) : parameter.name));
            valid = false;
        }
    }

    QSet<QString> resourceKeys;
    QSet<QString> resourceIds;
    for (int i = 0; i < cfg.resources.size(); ++i) {
        const auto& resource = cfg.resources[i];

        if (resource.id.isEmpty()) {
            errors.append(QString("资源绑定 #%1: ID 不能为空").arg(i + 1));
            valid = false;
        } else if (resourceIds.contains(resource.id)) {
            errors.append(QString("重复的资源绑定 ID: '%1'").arg(resource.id));
            valid = false;
        } else {
            resourceIds.insert(resource.id);
        }

        if (resource.resourceType.isEmpty()) {
            errors.append(QString("资源绑定 #%1: 资源类型不能为空").arg(i + 1));
            valid = false;
        }

        if (resource.channel.isEmpty()) {
            errors.append(QString("资源绑定 #%1: 通道不能为空").arg(i + 1));
            valid = false;
        }

        const QString resourceKey = resource.resourceType + ":" + resource.channel;
        if (resourceKeys.contains(resourceKey)) {
            errors.append(QString("重复的资源占用: '%1'").arg(resourceKey));
            valid = false;
        } else {
            resourceKeys.insert(resourceKey);
        }
    }
    
    return valid;
}

bool ProjectController::checkDuplicateChannelBindings(const ProjectRuntimeConfig& cfg, QStringList& errors) const
{
    bool valid = true;
    
    QSet<QString> channelNames;
    for (const auto& provider : cfg.providers) {
        if (channelNames.contains(provider.channelName)) {
            errors.append(QString("重复的监控通道名称: '%1'").arg(provider.channelName));
            valid = false;
        } else {
            channelNames.insert(provider.channelName);
        }
    }
    
    QSet<QString> providerIds;
    for (const auto& provider : cfg.providers) {
        if (providerIds.contains(provider.id)) {
            errors.append(QString("重复的监控通道 ID: '%1'").arg(provider.id));
            valid = false;
        } else {
            providerIds.insert(provider.id);
        }
    }
    
    QSet<QString> mappingIds;
    for (const auto& mapping : cfg.dslMappings) {
        if (mappingIds.contains(mapping.id)) {
            errors.append(QString("重复的组态映射 ID: '%1'").arg(mapping.id));
            valid = false;
        } else {
            mappingIds.insert(mapping.id);
        }
    }
    
    QMap<QString, int> signalPathCount;
    for (const auto& mapping : cfg.dslMappings) {
        if (!mapping.signalPath.isEmpty()) {
            signalPathCount[mapping.signalPath]++;
        }
    }
    
    for (auto it = signalPathCount.begin(); it != signalPathCount.end(); ++it) {
        if (it.value() > 1) {
            errors.append(QString("信号路径 '%1' 被绑定了 %2 次，可能导致冲突")
                          .arg(it.key())
                          .arg(it.value()));
        }
    }
    
    return valid;
}

bool ProjectController::checkHardwareResourceLimits(const ProjectRuntimeConfig& cfg, QStringList& errors) const
{
    bool valid = true;
    
    int analogInputCount = 0;
    int analogOutputCount = 0;
    int digitalInputCount = 0;
    int digitalOutputCount = 0;
    
    for (const auto& mapping : cfg.dslMappings) {
        const QString snippetId = mapping.snippetId.trimmed();
        if (snippetId.contains("analog_input", Qt::CaseInsensitive)
            || snippetId.compare(QStringLiteral("_DrvAI"), Qt::CaseInsensitive) == 0) {
            analogInputCount++;
        } else if (snippetId.contains("analog_output", Qt::CaseInsensitive)
                   || snippetId.compare(QStringLiteral("_DrvAO"), Qt::CaseInsensitive) == 0) {
            analogOutputCount++;
        } else if (snippetId.contains("digital_input", Qt::CaseInsensitive)
                   || snippetId.compare(QStringLiteral("_DrvDI"), Qt::CaseInsensitive) == 0) {
            digitalInputCount++;
        } else if (snippetId.contains("digital_output", Qt::CaseInsensitive)
                   || snippetId.compare(QStringLiteral("_DrvDO"), Qt::CaseInsensitive) == 0) {
            digitalOutputCount++;
        }
    }

    for (const auto& resource : cfg.resources) {
        const QString type = resource.resourceType.trimmed();
        if (type.compare(QStringLiteral("AI"), Qt::CaseInsensitive) == 0) {
            analogInputCount++;
        } else if (type.compare(QStringLiteral("AO"), Qt::CaseInsensitive) == 0) {
            analogOutputCount++;
        } else if (type.compare(QStringLiteral("DI"), Qt::CaseInsensitive) == 0) {
            digitalInputCount++;
        } else if (type.compare(QStringLiteral("DO"), Qt::CaseInsensitive) == 0) {
            digitalOutputCount++;
        }
    }
    
    if (analogInputCount + analogOutputCount > MAX_ANALOG_CHANNELS) {
        errors.append(QString("模拟量通道数量 (%1) 超出限制 (%2)")
                      .arg(analogInputCount + analogOutputCount)
                      .arg(MAX_ANALOG_CHANNELS));
        valid = false;
    }
    
    if (digitalInputCount + digitalOutputCount > MAX_DIGITAL_CHANNELS) {
        errors.append(QString("数字量通道数量 (%1) 超出限制 (%2)")
                      .arg(digitalInputCount + digitalOutputCount)
                      .arg(MAX_DIGITAL_CHANNELS));
        valid = false;
    }
    
    if (cfg.providers.size() > MAX_MONITOR_CHANNELS) {
        errors.append(QString("监控通道总数 (%1) 超出限制 (%2)")
                      .arg(cfg.providers.size())
                      .arg(MAX_MONITOR_CHANNELS));
        valid = false;
    }
    
    return valid;
}

// ================= 辅助方法 =================
QString ProjectController::timestampedMessage(const QString& msg) const
{
    return QString("[%1] %2")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(msg);
}


