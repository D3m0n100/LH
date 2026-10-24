#pragma once
#include <QByteArray>
#include <QVector>
#include <QVariantMap>
#include <QStringList>

namespace Communication {
inline quint16 modbusCrc16(const QByteArray& bytes) {
    quint16 crc = 0xffff;
    for (char value : bytes) {
        crc ^= static_cast<quint8>(value);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 1) ? (crc >> 1) ^ 0xa001 : crc >> 1;
    }
    return crc;
}
// An odd trailing byte is padded with zero, preserving its configured byte order.
inline QVector<quint16> bytesToRegisters(const QByteArray& bytes, bool littleEndian = false) {
    QVector<quint16> words;
    words.reserve((bytes.size() + 1) / 2);
    for (int i = 0; i < bytes.size(); i += 2) {
        const quint8 first = static_cast<quint8>(bytes.at(i));
        const quint8 second = i + 1 < bytes.size() ? static_cast<quint8>(bytes.at(i + 1)) : 0;
        words.append(littleEndian ? (second << 8) | first : (first << 8) | second);
    }
    return words;
}
inline bool readPositiveInt(const QVariantMap& map, const QStringList& keys, int* value) {
    for (const auto& key : keys) {
        bool ok = false;
        const int candidate = map.value(key).toInt(&ok);
        if (ok && candidate > 0) { *value = candidate; return true; }
    }
    return false;
}
}
