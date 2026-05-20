// 文件：src/communication/DownloadProfile.cpp

#include "DownloadProfile.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

static QVariantMap jsonObjectToVariantMap(const QJsonObject& obj)
{
    return obj.toVariantMap();
}

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
    out.name = root.value("name").toString();
    out.slaveId = root.value("slaveId").toInt(out.slaveId);

    out.steps.clear();
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
        out.steps.push_back(s);
    }

    if (out.steps.isEmpty()) {
        if (err) *err = "steps 为空";
        return false;
    }

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
