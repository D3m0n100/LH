#ifndef RUNTIMEPOINTREGISTERCODEC_H
#define RUNTIMEPOINTREGISTERCODEC_H

#include "common/RuntimePointTypes.h"

#include <QVariant>
#include <QVector>

struct RuntimePointRegisterCodecSpec
{
    QString dataType;
    QString byteOrder = QStringLiteral("BigEndian");
    QString wordOrder = QStringLiteral("BigEndian");
    int elementCount = 1;
    int registerCount = 1; // actual mapping register count; typed specs derive it from elementCount
    double scale = 1.0;
    double offset = 0.0;

    bool isTyped() const { return !dataType.isEmpty(); }
};

class RuntimePointRegisterCodec final
{
public:
    static bool buildSpec(const RuntimePointDefinition& point,
                          RuntimePointRegisterCodecSpec* spec,
                          QString* errorMessage = nullptr);

    static bool encode(const RuntimePointRegisterCodecSpec& spec,
                       const QVariant& value,
                       QVector<quint16>* registers,
                       QString* errorMessage = nullptr);
    static bool decode(const RuntimePointRegisterCodecSpec& spec,
                       const QVector<quint16>& registers,
                       QVariant* value,
                       QString* errorMessage = nullptr);
};

#endif // RUNTIMEPOINTREGISTERCODEC_H
