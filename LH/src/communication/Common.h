// 文件：src/communication/Common.h
// 通信模块通用宏定义和辅助函数
// 日志宏已统一到 include/Common.h，此处仅保留通信专用工具

#ifndef COMMUNICATION_COMMON_H
#define COMMUNICATION_COMMON_H

#include <QDebug>
#include <QString>
#include <QDateTime>
#include <QRegularExpression>

#include "../../include/Common.h"

// 日志宏：统一使用 include/Common.h 中的定义
// 不能用 #include "Common.h"，否则同目录下会先包含自己
#include "../../include/Common.h"

// ============================================================================
// 字节操作辅助宏
// ============================================================================

// 高字节 / 低字节
#define HI_BYTE(word)   static_cast<quint8>(((word) >> 8) & 0xFF)
#define LO_BYTE(word)   static_cast<quint8>((word) & 0xFF)

// 组合字节为 16 位字
#define MAKE_WORD(hi, lo) static_cast<quint16>(((static_cast<quint16>(hi) & 0xFF) << 8) | (static_cast<quint16>(lo) & 0xFF))

// 组合字节为 32 位双字
#define MAKE_DWORD(b3, b2, b1, b0) \
    static_cast<quint32>(((static_cast<quint32>(b3) & 0xFF) << 24) | \
                         ((static_cast<quint32>(b2) & 0xFF) << 16) | \
                         ((static_cast<quint32>(b1) & 0xFF) << 8) | \
                         (static_cast<quint32>(b0) & 0xFF))

// ============================================================================
// 通用辅助函数
// ============================================================================

namespace CommUtils {

/**
 * @brief 将字节数组转换为十六进制字符串
 */
inline QString toHexString(const QByteArray& data, const QString& separator = " ")
{
    QStringList hexList;
    for (char byte : data) {
        hexList.append(QString("%1").arg(static_cast<quint8>(byte), 2, 16, QChar('0')).toUpper());
    }
    return hexList.join(separator);
}

/**
 * @brief 从十六进制字符串解析字节数组
 */
inline QByteArray fromHexString(const QString& hexStr)
{
    QString cleaned = hexStr;
    cleaned.remove(QRegularExpression("[^0-9A-Fa-f]"));
    return QByteArray::fromHex(cleaned.toLatin1());
}

/**
 * @brief 计算 CRC16 (Modbus)
 */
inline quint16 crc16Modbus(const QByteArray& data)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); i++) {
        crc ^= static_cast<quint8>(data[i]);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief 计算校验和 (简单累加)
 */
inline quint8 checksum(const QByteArray& data)
{
    quint8 sum = 0;
    for (char byte : data) {
        sum += static_cast<quint8>(byte);
    }
    return sum;
}

/**
 * @brief 计算 XOR 校验
 */
inline quint8 xorChecksum(const QByteArray& data)
{
    quint8 result = 0;
    for (char byte : data) {
        result ^= static_cast<quint8>(byte);
    }
    return result;
}

} // namespace CommUtils

#endif // COMMUNICATION_COMMON_H
