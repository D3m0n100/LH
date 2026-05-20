#include "CANCommon.h"

namespace CANHelpers {

quint16 buildCANOpenCOBId(quint8 functionCode, quint8 nodeId)
{
    return static_cast<quint16>(((functionCode & 0x0F) << 7) | (nodeId & 0x7F));
}

quint8 extractCANOpenFunctionCode(quint16 cobId)
{
    return static_cast<quint8>((cobId >> 7) & 0x0F);
}

quint8 extractCANOpenNodeId(quint16 cobId)
{
    return static_cast<quint8>(cobId & 0x7F);
}

bool isPDU1(quint32 pgn)
{
    quint8 pf = static_cast<quint8>((pgn >> 8) & 0xFF);
    return pf < 240;
}

quint32 buildJ1939Identifier(const J1939Frame& frame)
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

    identifier |= (dp & 0x01) << 24;      // Data page
    identifier |= static_cast<quint32>(pf) << 16;
    identifier |= static_cast<quint32>(ps) << 8;
    identifier |= frame.sourceAddress;

    return identifier;
}

J1939Frame parseJ1939Identifier(quint32 identifier, const QByteArray& payload)
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

    return frame;
}

} // namespace CANHelpers




