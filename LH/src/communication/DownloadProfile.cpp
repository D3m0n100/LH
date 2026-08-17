// 文件：src/communication/DownloadProfile.cpp

#include "DownloadProfile.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>

static QVariantMap jsonObjectToVariantMap(const QJsonObject& obj)
{
    return obj.toVariantMap();
}

namespace {
void mergeMissing(QVariantMap& target, const QVariantMap& fallback)
{
    for (auto it = fallback.constBegin(); it != fallback.constEnd(); ++it) {
        if (!target.contains(it.key())) {
            target.insert(it.key(), it.value());
        }
    }
}

bool strictRegister(const QVariant& value, int* out)
{
    if (value.type() == QVariant::Bool) return false;
    bool ok = false;
    const double number = value.toDouble(&ok);
    if (!ok || !qIsFinite(number) || qFloor(number) != number || number < 0 || number > 65535) {
        return false;
    }
    if (out) *out = static_cast<int>(number);
    return true;
}

bool validateRegisterValues(const QVariant& value, int* count)
{
    if (count) *count = 0;
    if (!value.isValid() || value.isNull()) return false;

    QVariantList values;
    if (value.type() == QVariant::List) {
        values = value.toList();
    } else if (value.type() == QVariant::StringList) {
        for (const QString& item : value.toStringList()) values.append(item);
    } else {
        values.append(value);
    }
    if (values.isEmpty()) return false;
    for (const QVariant& item : values) {
        if (!strictRegister(item, nullptr)) return false;
    }
    if (count) *count = values.size();
    return true;
}

bool strictInt(const QVariant& value, int minimum, int maximum, int* out)
{
    int number = 0;
    if (!strictRegister(value, &number) || number < minimum || number > maximum) return false;
    if (out) *out = number;
    return true;
}
} // namespace

QString DownloadProfile::stepTypeToString(DownloadProfile::StepType t)
{
    switch (t) {
    case StepType::Enter: return "enter";
    case StepType::SendChunk: return "sendChunk";
    case StepType::Poll: return "poll";
    case StepType::Finalize: return "finalize";
    case StepType::QueryResult: return "queryResult";
    default: return "unknown";
    }
}

DownloadProfile::StepType DownloadProfile::stepTypeFromString(const QString& s, bool* ok)
{
    const QString k = s.trimmed().toLower();
    if (ok) *ok = true;
    if (k == "enter") return StepType::Enter;
    if (k == "sendchunk") return StepType::SendChunk;
    if (k == "poll") return StepType::Poll;
    if (k == "finalize") return StepType::Finalize;
    if (k == "queryresult") return StepType::QueryResult;
    if (ok) *ok = false;
    return StepType::Enter;
}

bool DownloadProfile::fromJson(const QByteArray& json, DownloadProfile& out, QString* err)
{
    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(json, &pe);
    if (doc.isNull() || !doc.isObject()) {
        if (err) *err = QString("JSON 解析失败: %1 (offset=%2)").arg(pe.errorString()).arg(pe.offset);
        return false;
    }

    const QJsonObject root = doc.object();
    DownloadProfile parsed;
    parsed.name = root.value("name").toString();
    parsed.slaveId = root.value("slaveId").toInt(parsed.slaveId);

    const QJsonArray steps = root.value("steps").toArray();
    for (const auto& v : steps) {
        const QJsonObject o = v.toObject();
        bool ok = false;
        const StepType type = stepTypeFromString(o.value("type").toString(), &ok);
        if (!ok) {
            if (err) *err = "未知 step.type: " + o.value("type").toString();
            return false;
        }
        Step s;
        s.type = type;
        s.params = jsonObjectToVariantMap(o.value("params").toObject());
        parsed.steps.push_back(s);
    }

    if (parsed.steps.isEmpty()) {
        if (err) *err = "steps 为空";
        return false;
    }

    QStringList validationErrors;
    if (!parsed.validate(&validationErrors)) {
        if (err) *err = validationErrors.join(QStringLiteral("; "));
        return false;
    }

    out = parsed;
    return true;
}

bool DownloadProfile::fromJsonFile(const QString& path, DownloadProfile& out, QString* err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = "无法打开 profile 文件: " + path;
        return false;
    }
    return fromJson(f.readAll(), out, err);
}

QVariantMap DownloadProfile::resolvedParams(const DownloadProfile::Step& step) const
{
    QVariantMap params = step.params;

    if ((step.type == StepType::Enter || step.type == StepType::Finalize)
            && params.contains(QStringLiteral("writeHolding"))) {
        mergeMissing(params, params.value(QStringLiteral("writeHolding")).toMap());
    }
    if (step.type == StepType::QueryResult
            && params.contains(QStringLiteral("resultRegister"))) {
        mergeMissing(params, params.value(QStringLiteral("resultRegister")).toMap());
    }
    if (step.type == StepType::SendChunk) {
        if (!params.contains(QStringLiteral("dataAddress"))) {
            if (params.contains(QStringLiteral("chunkHoldingAddress"))) {
                params.insert(QStringLiteral("dataAddress"),
                              params.value(QStringLiteral("chunkHoldingAddress")));
            } else if (params.contains(QStringLiteral("address"))) {
                params.insert(QStringLiteral("dataAddress"), params.value(QStringLiteral("address")));
            }
        }
        if (!params.contains(QStringLiteral("chunkWords"))) {
            if (params.contains(QStringLiteral("maxRegisters"))) {
                params.insert(QStringLiteral("chunkWords"), params.value(QStringLiteral("maxRegisters")));
            } else if (params.contains(QStringLiteral("maxRegistersPerChunk"))) {
                params.insert(QStringLiteral("chunkWords"),
                              params.value(QStringLiteral("maxRegistersPerChunk")));
            }
        }
    }
    if (step.type == StepType::Poll
            && !params.contains(QStringLiteral("pollIntervalMs"))
            && params.contains(QStringLiteral("intervalMs"))) {
        params.insert(QStringLiteral("pollIntervalMs"), params.value(QStringLiteral("intervalMs")));
    }
    const QString layer = params.value(QStringLiteral("layer"), QStringLiteral("target")).toString().trimmed().toLower();
    if (!params.contains(QStringLiteral("slaveId")) && slaveId >= 0
            && (layer == QStringLiteral("controller") || layer == QStringLiteral("ctrl"))) {
        params.insert(QStringLiteral("slaveId"), slaveId);
    }
    return params;
}

bool DownloadProfile::validate(QStringList* errors) const
{
    QStringList localErrors;
    if (steps.isEmpty()) {
        localErrors << QStringLiteral("profile.steps 为空");
    }

    int sendChunkCount = 0;

    for (int i = 0; i < steps.size(); ++i) {
        const Step& step = steps.at(i);
        const QVariantMap params = resolvedParams(step);
        QStringList stepErrors;
        int address = -1;
        const bool hasAddress = strictInt(params.value(QStringLiteral("address")), 0, 65535, &address);

        const QString layer = params.value(QStringLiteral("layer"), QStringLiteral("target"))
                                      .toString().trimmed().toLower();
        if (layer != QStringLiteral("target")
                && layer != QStringLiteral("controller")
                && layer != QStringLiteral("ctrl")) {
            stepErrors << QStringLiteral("layer 必须为 controller 或 target");
        }

        switch (step.type) {
        case StepType::Enter:
        case StepType::Finalize: {
            const QString op = params.value(QStringLiteral("op"), QStringLiteral("writeRegs"))
                                       .toString().trimmed();
            if (op.compare(QStringLiteral("writeRegs"), Qt::CaseInsensitive) != 0
                    && op.compare(QStringLiteral("writeCoils"), Qt::CaseInsensitive) != 0) {
                stepErrors << QStringLiteral("op 必须为 writeRegs 或 writeCoils");
            }
            if (!hasAddress) {
                stepErrors << QStringLiteral("缺少 address");
            }
            const QVariant values = params.value(QStringLiteral("values"));
            int valueCount = 0;
            if (!validateRegisterValues(values, &valueCount)) {
                stepErrors << QStringLiteral("values 必须为 0..65535 的整数且不能为空");
            }
            if (hasAddress && valueCount > 0 && address + valueCount > 65536) {
                stepErrors << QStringLiteral("address 与 values 长度超出 16 位地址空间");
            }
            break;
        }
        case StepType::Poll: {
            int count = 0;
            if (!hasAddress) {
                stepErrors << QStringLiteral("缺少 address");
            }
            if (!strictInt(params.value(QStringLiteral("count"), 1), 1, 125, &count)
                    || !hasAddress || address + count > 65536) {
                stepErrors << QStringLiteral("count 必须为 1..125");
            }
            const QVariant expected = params.value(QStringLiteral("expected"));
            int expectedCount = 0;
            if (!validateRegisterValues(expected, &expectedCount)) {
                stepErrors << QStringLiteral("expected 必须为 0..65535 的整数且不能为空");
            } else if (expectedCount != count) {
                stepErrors << QStringLiteral("expected 长度必须等于 count");
            }
            if (params.value(QStringLiteral("timeoutMs"), 2000).toInt() <= 0) {
                stepErrors << QStringLiteral("timeoutMs 必须大于 0");
            }
            if (params.value(QStringLiteral("pollIntervalMs"), 100).toInt() <= 0) {
                stepErrors << QStringLiteral("pollIntervalMs 必须大于 0");
            }
            break;
        }
        case StepType::SendChunk: {
            ++sendChunkCount;
            int dataAddress = -1;
            int chunkWords = 0;
            if (!strictInt(params.value(QStringLiteral("dataAddress"), -1), 0, 65535, &dataAddress)) {
                stepErrors << QStringLiteral("缺少 dataAddress");
            }
            if (!strictInt(params.value(QStringLiteral("chunkWords"), 60), 1, 125, &chunkWords)) {
                stepErrors << QStringLiteral("chunkWords 必须为 1..125");
            }
            if (dataAddress >= 0 && chunkWords > 0 && dataAddress + chunkWords > 65536) {
                stepErrors << QStringLiteral("dataAddress 与 chunkWords 超出 16 位地址空间");
            }
            const QString byteOrder = params.value(QStringLiteral("byteOrder"),
                                                   QStringLiteral("BigEndian")).toString();
            if (byteOrder.compare(QStringLiteral("BigEndian"), Qt::CaseInsensitive) != 0
                    && byteOrder.compare(QStringLiteral("LittleEndian"), Qt::CaseInsensitive) != 0) {
                stepErrors << QStringLiteral("byteOrder 必须为 BigEndian 或 LittleEndian");
            }
            const QStringList packetAddressKeys = {
                QStringLiteral("packetIndexAddress"),
                QStringLiteral("packetLengthAddress"),
                QStringLiteral("packetCrcAddress"),
                QStringLiteral("packetOffsetAddress")
            };
            for (const QString& key : packetAddressKeys) {
                if (params.contains(key) && !strictInt(params.value(key), 0, 65535, nullptr)) {
                    stepErrors << QStringLiteral("%1 必须为 0..65535 的整数").arg(key);
                }
            }
            if (params.contains(QStringLiteral("packetIndexBase"))
                    && !strictInt(params.value(QStringLiteral("packetIndexBase")), 0, 65535, nullptr)) {
                stepErrors << QStringLiteral("packetIndexBase 必须为 0..65535 的整数");
            }
            break;
        }
        case StepType::QueryResult: {
            int count = 0;
            if (!hasAddress) {
                stepErrors << QStringLiteral("缺少 address");
            }
            if (!strictInt(params.value(QStringLiteral("count"), 1), 1, 125, &count)
                    || !hasAddress || address + count > 65536) {
                stepErrors << QStringLiteral("count 必须为 1..125");
            }
            if (params.contains(QStringLiteral("expected"))) {
                const QVariant expected = params.value(QStringLiteral("expected"));
                int expectedCount = 0;
                if (!validateRegisterValues(expected, &expectedCount)) {
                    stepErrors << QStringLiteral("expected 必须为 0..65535 的整数且不能为空");
                } else if (expectedCount != count) {
                    stepErrors << QStringLiteral("expected 长度必须等于 count");
                }
            }
            break;
        }
        }

        if (!stepErrors.isEmpty()) {
            localErrors << QStringLiteral("step %1(%2)：%3")
                           .arg(i + 1)
                           .arg(stepTypeToString(step.type))
                           .arg(stepErrors.join(QStringLiteral(", ")));
        }
    }

    if (sendChunkCount == 0) localErrors << QStringLiteral("profile 至少需要一个 sendChunk 步骤");

    if (errors) {
        *errors = localErrors;
    }
    return localErrors.isEmpty();
}
