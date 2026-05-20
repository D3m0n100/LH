// 文件：src/communication/CANCommon.h
// CAN 通信通用定义 - 消息结构和辅助函数

#ifndef CANCOMMON_H
#define CANCOMMON_H

#include <QByteArray>
#include <QMetaType>
#include <QDateTime>
#include <QtGlobal>
#include <optional>

// ============================================================================
// CAN 消息结构
// ============================================================================

/**
 * @brief 通用 CAN 消息结构
 */
struct CANMessage
{
    quint32 id = 0;             ///< CAN ID
    QByteArray payload;         ///< 数据负载 (最多 8 字节标准 CAN)
    bool extended = false;      ///< 是否为扩展帧 (29-bit ID)
    bool remoteRequest = false; ///< 是否为远程请求帧
    qint64 timestamp = 0;       ///< 接收时间戳 (ms)
    
    bool isValid() const { 
        return payload.size() <= 8 && 
               (extended ? (id <= 0x1FFFFFFF) : (id <= 0x7FF)); 
    }
};

/**
 * @brief CANopen 帧结构
 */
struct CANOpenFrame
{
    quint16 cobId = 0;          ///< COB-ID (11-bit)
    QByteArray payload;         ///< 数据负载
    quint8 functionCode = 0;    ///< 功能码
    quint8 nodeId = 0;          ///< 节点 ID
    qint64 timestamp = 0;       ///< 接收时间戳
};

/**
 * @brief J1939 帧结构
 */
struct J1939Frame
{
    quint8 priority = 6;        ///< 优先级 (0-7)
    quint32 pgn = 0;            ///< 参数组编号
    quint8 sourceAddress = 0x80;///< 源地址
    std::optional<quint8> destinationAddress; ///< 目标地址 (PDU1 时有效)
    QByteArray payload;         ///< 数据负载
    qint64 timestamp = 0;       ///< 接收时间戳
    
    bool isPDU1() const {
        quint8 pf = static_cast<quint8>((pgn >> 8) & 0xFF);
        return pf < 240;
    }
};

// ============================================================================
// CAN 辅助函数命名空间
// ============================================================================

namespace CANHelpers {

/**
 * @brief 构建 CANopen COB-ID
 * @param functionCode 功能码 (4 bits)
 * @param nodeId 节点 ID (7 bits)
 * @return COB-ID (11 bits)
 */
inline quint16 buildCANOpenCOBId(quint8 functionCode, quint8 nodeId)
{
    return static_cast<quint16>(((functionCode & 0x0F) << 7) | (nodeId & 0x7F));
}

/**
 * @brief 从 COB-ID 提取功能码
 */
inline quint8 extractCANOpenFunctionCode(quint16 cobId)
{
    return static_cast<quint8>((cobId >> 7) & 0x0F);
}

/**
 * @brief 从 COB-ID 提取节点 ID
 */
inline quint8 extractCANOpenNodeId(quint16 cobId)
{
    return static_cast<quint8>(cobId & 0x7F);
}

/**
 * @brief 判断 PGN 是否为 PDU1 格式
 */
inline bool isPDU1(quint32 pgn)
{
    quint8 pf = static_cast<quint8>((pgn >> 8) & 0xFF);
    return pf < 240;
}

/**
 * @brief 构建 J1939 标识符 (29-bit)
 */
inline quint32 buildJ1939Identifier(const J1939Frame& frame)
{
    quint8 priority = frame.priority & 0x7;
    quint32 identifier = static_cast<quint32>(priority) << 26;

    quint32 pgn = frame.pgn & 0x3FFFF;
    quint8 pf = static_cast<quint8>((pgn >> 8) & 0xFF);
    quint8 dp = static_cast<quint8>((pgn >> 16) & 0x01);
    quint8 ps;

    if (isPDU1(pgn)) {
        ps = frame.destinationAddress.value_or(0xFF);
    } else {
        ps = static_cast<quint8>(pgn & 0xFF);
    }

    identifier |= (dp & 0x01) << 24;
    identifier |= static_cast<quint32>(pf) << 16;
    identifier |= static_cast<quint32>(ps) << 8;
    identifier |= frame.sourceAddress;

    return identifier;
}

/**
 * @brief 解析 J1939 标识符
 */
inline J1939Frame parseJ1939Identifier(quint32 identifier, const QByteArray& payload)
{
    J1939Frame frame;
    
    frame.priority = static_cast<quint8>((identifier >> 26) & 0x7);
    quint8 dp = static_cast<quint8>((identifier >> 24) & 0x1);
    quint8 pf = static_cast<quint8>((identifier >> 16) & 0xFF);
    quint8 ps = static_cast<quint8>((identifier >> 8) & 0xFF);
    frame.sourceAddress = static_cast<quint8>(identifier & 0xFF);

    if (pf < 240) {
        frame.pgn = (static_cast<quint32>(dp) << 16) | (static_cast<quint32>(pf) << 8);
        frame.destinationAddress = ps;
    } else {
        frame.pgn = (static_cast<quint32>(dp) << 16) | (static_cast<quint32>(pf) << 8) | ps;
        frame.destinationAddress.reset();
    }
    
    frame.payload = payload;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    return frame;
}

} // namespace CANHelpers

// ============================================================================
// Qt 元类型注册
// ============================================================================

Q_DECLARE_METATYPE(CANMessage)
Q_DECLARE_METATYPE(CANOpenFrame)
Q_DECLARE_METATYPE(J1939Frame)

#endif // CANCOMMON_H
