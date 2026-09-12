#pragma once
#include <QPointF>
#include <QDateTime>
#include <QVariantMap>
#include "common/RuntimePointTypes.h"
namespace Monitor {
struct Sample {
    QString channelName;
    double value = 0.0;
    QString unit;
    QDateTime timestamp;
    RuntimePointQuality quality = RuntimePointQuality::Good;
    bool valueValid = true;
    QVariantMap metadata;

    Sample() = default;
    Sample(const QString& name,
           double val,
           const QString& unitStr,
           const QDateTime& time,
           const QVariantMap& meta = {})
        : channelName(name)
        , value(val)
        , unit(unitStr)
        , timestamp(time)
        , quality(meta.contains(QStringLiteral("quality"))
                      ? runtimePointQualityFromString(meta.value(QStringLiteral("quality")).toString())
                      : RuntimePointQuality::Good)
        , valueValid(meta.value(QStringLiteral("valueValid"), true).toBool())
        , metadata(meta)
    {}
    
    QPointF toPoint() const {
        return QPointF(static_cast<double>(timestamp.toMSecsSinceEpoch()), value);
    }
    
    /// 获取时间戳（毫秒）
    qint64 timestampMs() const {
        return timestamp.toMSecsSinceEpoch();
    }
};
}
