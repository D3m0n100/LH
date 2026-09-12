#include "RuntimePointRegisterCodec.h"

#include <QMetaType>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
QString canonicalDataType(const QString& dataType)
{
    const QString normalized = dataType.trimmed().toUpper();
    if (normalized == QStringLiteral("INT")
            || normalized == QStringLiteral("INT16")) {
        return QStringLiteral("INT16");
    }
    if (normalized == QStringLiteral("DINT")
            || normalized == QStringLiteral("INT32")) {
        return QStringLiteral("INT32");
    }
    if (normalized == QStringLiteral("UINT")
            || normalized == QStringLiteral("WORD")
            || normalized == QStringLiteral("UINT16")) {
        return QStringLiteral("UINT16");
    }
    if (normalized == QStringLiteral("UDINT")
            || normalized == QStringLiteral("DWORD")
            || normalized == QStringLiteral("UINT32")) {
        return QStringLiteral("UINT32");
    }
    if (normalized == QStringLiteral("REAL")
            || normalized == QStringLiteral("FLOAT32")) {
        return QStringLiteral("REAL");
    }
    if (normalized == QStringLiteral("BOOL")) {
        return normalized;
    }
    return QString();
}

int widthForType(const QString& dataType)
{
    const QString type = canonicalDataType(dataType);
    if (type == QStringLiteral("UINT32")
            || type == QStringLiteral("INT32")
            || type == QStringLiteral("REAL")) {
        return 2;
    }
    if (type == QStringLiteral("BOOL")
            || type == QStringLiteral("UINT16")
            || type == QStringLiteral("INT16")) {
        return 1;
    }
    return 0;
}

int registerCountFor(const RuntimePointRegisterCodecSpec& spec)
{
    if (!spec.isTyped()) {
        return spec.registerCount > 0 ? spec.registerCount : 0;
    }
    const int width = widthForType(spec.dataType);
    if (width <= 0 || spec.elementCount <= 0
            || spec.elementCount > std::numeric_limits<int>::max() / width) {
        return 0;
    }
    return spec.elementCount * width;
}

bool isNumericVariant(const QVariant& value)
{
    switch (value.userType()) {
    case QMetaType::Bool:
    case QMetaType::Char:
    case QMetaType::UChar:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Float:
    case QMetaType::Double:
        return true;
    default:
        return false;
    }
}

bool variantToFiniteDouble(const QVariant& value, double* result)
{
    if (!result) {
        return false;
    }
    if (value.userType() == QMetaType::QString) {
        const QString text = value.toString().trimmed();
        if (text.isEmpty()) {
            return false;
        }
        bool ok = false;
        const double number = text.toDouble(&ok);
        if (!ok || !std::isfinite(number)) {
            return false;
        }
        *result = number;
        return true;
    }
    if (!isNumericVariant(value)) {
        return false;
    }
    bool ok = false;
    const double number = value.toDouble(&ok);
    if (!ok || !std::isfinite(number)) {
        return false;
    }
    *result = number;
    return true;
}

bool variantToExactInteger(const QVariant& value, double* result)
{
    if (!variantToFiniteDouble(value, result)) {
        return false;
    }
    return std::trunc(*result) == *result;
}

bool readFiniteNumber(const QVariantMap& map,
                      const QString& key,
                      double defaultValue,
                      double* result,
                      QString* errorMessage)
{
    if (!map.contains(key)) {
        *result = defaultValue;
        return true;
    }

    if (!variantToFiniteDouble(map.value(key), result)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RuntimePoint %1 必须为有限数值").arg(key);
        }
        return false;
    }
    if (key == QStringLiteral("scale") && *result == 0.0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RuntimePoint scale 必须为有限且非零数值");
        }
        return false;
    }
    return true;
}

bool readPositiveCount(const QVariantMap& map,
                       const QString& key,
                       int defaultValue,
                       int* result,
                       QString* errorMessage)
{
    if (!map.contains(key)) {
        *result = defaultValue;
        return true;
    }
    double countValue = 0.0;
    if (map.value(key).userType() == QMetaType::Bool
            || !variantToExactInteger(map.value(key), &countValue)
            || countValue <= 0.0
            || countValue > std::numeric_limits<int>::max()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RuntimePoint %1 必须为正整数").arg(key);
        }
        return false;
    }
    *result = static_cast<int>(countValue);
    return true;
}

bool readByteOrder(const QVariantMap& map,
                   const QString& key,
                   QString* result,
                   QString* errorMessage)
{
    const QString value = map.value(key, QStringLiteral("BigEndian")).toString().trimmed();
    if (value.compare(QStringLiteral("BigEndian"), Qt::CaseInsensitive) != 0
            && value.compare(QStringLiteral("LittleEndian"), Qt::CaseInsensitive) != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RuntimePoint %1 必须为 BigEndian 或 LittleEndian：%2")
                    .arg(key, value);
        }
        return false;
    }
    *result = value.compare(QStringLiteral("LittleEndian"), Qt::CaseInsensitive) == 0
            ? QStringLiteral("LittleEndian")
            : QStringLiteral("BigEndian");
    return true;
}

bool isLittleEndian(const QString& order)
{
    return order.compare(QStringLiteral("LittleEndian"), Qt::CaseInsensitive) == 0;
}

bool validEndianOrder(const QString& order)
{
    return order.compare(QStringLiteral("BigEndian"), Qt::CaseInsensitive) == 0
            || order.compare(QStringLiteral("LittleEndian"), Qt::CaseInsensitive) == 0;
}

bool validateSpec(const RuntimePointRegisterCodecSpec& spec, QString* errorMessage)
{
    if (!validEndianOrder(spec.byteOrder) || !validEndianOrder(spec.wordOrder)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RuntimePoint byteOrder/wordOrder 必须为 BigEndian 或 LittleEndian");
        }
        return false;
    }
    if (spec.elementCount <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RuntimePoint elementCount 必须为正整数");
        }
        return false;
    }
    if (!std::isfinite(spec.scale) || spec.scale == 0.0
            || !std::isfinite(spec.offset)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RuntimePoint scale 必须为有限且非零数值，offset 必须为有限数值");
        }
        return false;
    }
    return true;
}

quint16 swapBytes(quint16 value)
{
    return static_cast<quint16>((value << 8) | (value >> 8));
}

QVector<QVariant> valueElements(const QVariant& value, int elementCount, QString* errorMessage)
{
    QVector<QVariant> elements;
    if (value.type() == QVariant::List || value.type() == QVariant::StringList) {
        const QVariantList list = value.toList();
        if (list.size() != elementCount) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("值的数组元素数量不匹配：expected=%1 got=%2")
                        .arg(elementCount).arg(list.size());
            }
            return {};
        }
        elements.reserve(list.size());
        for (const QVariant& item : list) {
            elements.append(item);
        }
        return elements;
    }

    if (value.canConvert<QVector<quint16>>()) {
        const QVector<quint16> vector = value.value<QVector<quint16>>();
        if (vector.size() != elementCount) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("值的数组元素数量不匹配：expected=%1 got=%2")
                        .arg(elementCount).arg(vector.size());
            }
            return {};
        }
        elements.reserve(vector.size());
        for (quint16 item : vector) {
            elements.append(item);
        }
        return elements;
    }

    if (elementCount != 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("值的数组元素数量不匹配：expected=%1 got=1")
                    .arg(elementCount);
        }
        return {};
    }
    elements.append(value);
    return elements;
}

bool encodeInteger(const QString& type,
                   const QVariant& value,
                   double scale,
                   double offset,
                   quint32* raw,
                   QString* errorMessage)
{
    double physical = 0.0;
    if (!variantToFiniteDouble(value, &physical)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 值必须为有限数值或严格数字字符串").arg(type);
        }
        return false;
    }
    const double unscaled = (physical - offset) / scale;
    if (!std::isfinite(unscaled) || std::trunc(unscaled) != unscaled) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 原始值必须为有限整数：%2").arg(type).arg(unscaled);
        }
        return false;
    }

    double minimum = 0.0;
    double maximum = 0.0;
    if (type == QStringLiteral("UINT16")) {
        maximum = std::numeric_limits<quint16>::max();
    } else if (type == QStringLiteral("INT16")) {
        minimum = std::numeric_limits<qint16>::min();
        maximum = std::numeric_limits<qint16>::max();
    } else if (type == QStringLiteral("UINT32")) {
        maximum = 4294967295.0;
    } else {
        minimum = std::numeric_limits<qint32>::min();
        maximum = std::numeric_limits<qint32>::max();
    }
    if (unscaled < minimum || unscaled > maximum) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 原始值超出范围：%2").arg(type).arg(unscaled);
        }
        return false;
    }
    *raw = static_cast<quint32>(static_cast<qint64>(unscaled));
    return true;
}

bool encodeElement(const RuntimePointRegisterCodecSpec& spec,
                   const QVariant& value,
                   QVector<quint16>* words,
                   QString* errorMessage)
{
    const QString type = spec.dataType;
    const double scale = spec.scale;
    const double offset = spec.offset;
    quint32 raw = 0;

    if (type == QStringLiteral("BOOL")) {
        double physical = 0.0;
        if (!variantToFiniteDouble(value, &physical)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("BOOL 值必须为有限数值或严格数字字符串，且原始值为 0 或 1");
            }
            return false;
        }
        const double unscaled = (physical - offset) / scale;
        if (!std::isfinite(unscaled) || (unscaled != 0.0 && unscaled != 1.0)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("BOOL 原始值必须为 0 或 1");
            }
            return false;
        }
        raw = static_cast<quint32>(unscaled);
    } else if (type == QStringLiteral("REAL")) {
        double physical = 0.0;
        if (!variantToFiniteDouble(value, &physical)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("REAL 值必须为有限数值或严格数字字符串");
            }
            return false;
        }
        const double unscaled = (physical - offset) / scale;
        if (!std::isfinite(unscaled)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("REAL 原始值必须为有限 float32 数值");
            }
            return false;
        }
        const float real = static_cast<float>(unscaled);
        if (!std::isfinite(real)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("REAL 原始值必须为有限 float32 数值");
            }
            return false;
        }
        static_assert(sizeof(real) == sizeof(raw), "float32 must be four bytes");
        std::memcpy(&raw, &real, sizeof(raw));
    } else if (!encodeInteger(type, value, scale, offset, &raw, errorMessage)) {
        return false;
    }

    const int width = widthForType(type);
    QVector<quint16> elementWords;
    elementWords.reserve(width);
    if (width == 1) {
        elementWords.append(static_cast<quint16>(raw));
    } else {
        elementWords.append(static_cast<quint16>((raw >> 16) & 0xffff));
        elementWords.append(static_cast<quint16>(raw & 0xffff));
        if (isLittleEndian(spec.wordOrder)) {
            std::reverse(elementWords.begin(), elementWords.end());
        }
    }
    if (isLittleEndian(spec.byteOrder)) {
        for (quint16& word : elementWords) {
            word = swapBytes(word);
        }
    }
    *words += elementWords;
    return true;
}

bool decodeElement(const RuntimePointRegisterCodecSpec& spec,
                   const QVector<quint16>& input,
                   QVariant* value,
                   QString* errorMessage)
{
    QVector<quint16> words = input;
    if (isLittleEndian(spec.byteOrder)) {
        for (quint16& word : words) {
            word = swapBytes(word);
        }
    }
    if (words.size() == 2 && isLittleEndian(spec.wordOrder)) {
        std::reverse(words.begin(), words.end());
    }

    quint32 raw = words.first();
    if (words.size() == 2) {
        raw = (static_cast<quint32>(words.at(0)) << 16) | words.at(1);
    }

    double unscaled = 0.0;
    if (spec.dataType == QStringLiteral("BOOL")) {
        if (raw > 1) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("BOOL 寄存器原始值无效：%1").arg(raw);
            }
            return false;
        }
        unscaled = static_cast<double>(raw);
    } else if (spec.dataType == QStringLiteral("UINT16")) {
        unscaled = static_cast<double>(raw);
    } else if (spec.dataType == QStringLiteral("INT16")) {
        unscaled = static_cast<double>(static_cast<qint16>(raw & 0xffff));
    } else if (spec.dataType == QStringLiteral("UINT32")) {
        unscaled = static_cast<double>(raw);
    } else if (spec.dataType == QStringLiteral("INT32")) {
        unscaled = static_cast<double>(static_cast<qint32>(raw));
    } else if (spec.dataType == QStringLiteral("REAL")) {
        float real = 0.0f;
        static_assert(sizeof(real) == sizeof(raw), "float32 must be four bytes");
        std::memcpy(&real, &raw, sizeof(real));
        if (!std::isfinite(real)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("REAL 寄存器原始值不是有限 float32");
            }
            return false;
        }
        unscaled = static_cast<double>(real);
    } else {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RuntimePoint dataType 不支持：%1").arg(spec.dataType);
        }
        return false;
    }

    const double physical = unscaled * spec.scale + spec.offset;
    if (!std::isfinite(physical)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("解码后的物理值不是有限数值");
        }
        return false;
    }
    const bool transformed = spec.scale != 1.0 || spec.offset != 0.0;
    if (transformed) {
        *value = physical;
    } else if (spec.dataType == QStringLiteral("BOOL")) {
        *value = (raw != 0);
    } else if (spec.dataType == QStringLiteral("UINT16")) {
        *value = static_cast<quint16>(raw);
    } else if (spec.dataType == QStringLiteral("INT16")) {
        *value = static_cast<qint16>(raw & 0xffff);
    } else if (spec.dataType == QStringLiteral("UINT32")) {
        *value = static_cast<quint32>(raw);
    } else if (spec.dataType == QStringLiteral("INT32")) {
        *value = static_cast<qint32>(raw);
    } else {
        *value = static_cast<float>(unscaled);
    }
    return true;
}

bool encodeLegacy(const RuntimePointRegisterCodecSpec& spec,
                  const QVariant& value,
                  QVector<quint16>* registers,
                  QString* errorMessage)
{
    const QVector<QVariant> elements = valueElements(value, spec.registerCount, errorMessage);
    if (elements.isEmpty() && spec.registerCount > 0) {
        return false;
    }
    registers->clear();
    registers->reserve(elements.size());
    for (const QVariant& element : elements) {
        double number = 0.0;
        if (!variantToExactInteger(element, &number)
                || number < 0.0
                || number > std::numeric_limits<quint16>::max()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("legacy raw register 值必须为 0..65535 的整数");
            }
            registers->clear();
            return false;
        }
        registers->append(static_cast<quint16>(number));
    }
    return true;
}

bool decodeLegacy(const RuntimePointRegisterCodecSpec& spec,
                  const QVector<quint16>& registers,
                  QVariant* value,
                  QString* errorMessage)
{
    if (registers.size() != spec.registerCount) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("legacy raw register 数量不匹配：expected=%1 got=%2")
                    .arg(spec.registerCount).arg(registers.size());
        }
        return false;
    }
    if (registers.size() == 1) {
        *value = registers.first();
        return true;
    }
    QVariantList list;
    list.reserve(registers.size());
    for (quint16 registerValue : registers) {
        list.append(registerValue);
    }
    *value = list;
    return true;
}
} // namespace

bool RuntimePointRegisterCodec::buildSpec(const RuntimePointDefinition& point,
                                          RuntimePointRegisterCodecSpec* spec,
                                          QString* errorMessage)
{
    if (!spec) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RuntimePoint codec spec 不能为空");
        }
        return false;
    }

    QVariantMap addressing = point.addressing;
    for (auto it = point.metadata.constBegin(); it != point.metadata.constEnd(); ++it) {
        addressing.insert(it.key(), it.value());
    }

    const QString declaredType = point.dataType.trimmed().isEmpty()
            ? addressing.value(QStringLiteral("dataType")).toString().trimmed()
            : point.dataType.trimmed();
    const QString type = canonicalDataType(declaredType);
    if (!declaredType.isEmpty() && type.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RuntimePoint dataType 不支持：%1").arg(declaredType);
        }
        return false;
    }

    RuntimePointRegisterCodecSpec result;
    result.dataType = type;
    if (!readByteOrder(addressing, QStringLiteral("byteOrder"), &result.byteOrder, errorMessage)
            || !readByteOrder(addressing, QStringLiteral("wordOrder"), &result.wordOrder, errorMessage)) {
        return false;
    }
    if (!readPositiveCount(addressing,
                           QStringLiteral("elementCount"),
                           1,
                           &result.elementCount,
                           errorMessage)) {
        return false;
    }
    if (!readFiniteNumber(addressing,
                          QStringLiteral("scale"),
                          1.0,
                          &result.scale,
                          errorMessage)
            || !readFiniteNumber(addressing,
                                 QStringLiteral("offset"),
                                 0.0,
                                 &result.offset,
                                 errorMessage)) {
        return false;
    }

    if (result.isTyped()) {
        const int width = widthForType(result.dataType);
        if (result.elementCount > std::numeric_limits<int>::max() / width) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("RuntimePoint elementCount 导致寄存器数量溢出");
            }
            return false;
        }
        result.registerCount = result.elementCount * width;
    } else {
        result.registerCount = result.elementCount;
    }
    *spec = result;
    return true;
}

bool RuntimePointRegisterCodec::encode(const RuntimePointRegisterCodecSpec& spec,
                                       const QVariant& value,
                                       QVector<quint16>* registers,
                                       QString* errorMessage)
{
    if (!registers) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("编码输出寄存器不能为空");
        }
        return false;
    }
    RuntimePointRegisterCodecSpec normalized = spec;
    if (spec.isTyped()) {
        normalized.dataType = canonicalDataType(spec.dataType);
        if (normalized.dataType.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("RuntimePoint dataType 不支持：%1").arg(spec.dataType);
            }
            return false;
        }
    }
    if (!validateSpec(normalized, errorMessage)) {
        return false;
    }
    registers->clear();
    const int expectedCount = registerCountFor(normalized);
    if (expectedCount <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RuntimePoint codec register count 无效");
        }
        return false;
    }
    if (!spec.isTyped()) {
        return encodeLegacy(normalized, value, registers, errorMessage);
    }

    const QVector<QVariant> elements = valueElements(value, normalized.elementCount, errorMessage);
    if (elements.isEmpty() && normalized.elementCount > 0) {
        return false;
    }
    registers->reserve(expectedCount);
    for (const QVariant& element : elements) {
        if (!encodeElement(normalized, element, registers, errorMessage)) {
            registers->clear();
            return false;
        }
    }
    if (registers->size() != expectedCount) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("编码寄存器数量不匹配：expected=%1 got=%2")
                    .arg(expectedCount).arg(registers->size());
        }
        registers->clear();
        return false;
    }
    return true;
}

bool RuntimePointRegisterCodec::decode(const RuntimePointRegisterCodecSpec& spec,
                                       const QVector<quint16>& registers,
                                       QVariant* value,
                                       QString* errorMessage)
{
    if (!value) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("解码输出 QVariant 不能为空");
        }
        return false;
    }
    RuntimePointRegisterCodecSpec normalized = spec;
    if (spec.isTyped()) {
        normalized.dataType = canonicalDataType(spec.dataType);
        if (normalized.dataType.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("RuntimePoint dataType 不支持：%1").arg(spec.dataType);
            }
            return false;
        }
    }
    if (!validateSpec(normalized, errorMessage)) {
        return false;
    }
    const int expectedCount = registerCountFor(normalized);
    if (expectedCount <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RuntimePoint codec register count 无效");
        }
        return false;
    }
    if (!spec.isTyped()) {
        return decodeLegacy(normalized, registers, value, errorMessage);
    }
    if (registers.size() != expectedCount) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("解码寄存器数量不匹配：expected=%1 got=%2")
                    .arg(expectedCount).arg(registers.size());
        }
        return false;
    }

    QVariantList decoded;
    decoded.reserve(normalized.elementCount);
    const int width = widthForType(normalized.dataType);
    for (int elementIndex = 0; elementIndex < normalized.elementCount; ++elementIndex) {
        QVariant element;
        if (!decodeElement(normalized,
                           registers.mid(elementIndex * width, width),
                           &element,
                           errorMessage)) {
            return false;
        }
        decoded.append(element);
    }
    if (normalized.elementCount == 1) {
        *value = decoded.first();
    } else {
        *value = decoded;
    }
    return true;
}
