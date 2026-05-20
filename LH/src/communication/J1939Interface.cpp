// 文件：src/communication/J1939Interface.cpp
// SAE J1939 协议接口实现

#include "J1939Interface.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include "Common.h"

J1939Interface::J1939Interface(QObject* parent)
    : CANInterface(parent)
{
    m_protocolType = CommProtocolType::J1939;
    
    qRegisterMetaType<J1939Frame>("J1939Frame");
    
    connect(this, &CANInterface::frameReceived, 
            this, &J1939Interface::onRawFrameReceived);
}

bool J1939Interface::open(const QVariantMap& config)
{
    m_j1939Config = J1939Config::fromMap(config);
    return open(m_j1939Config);
}

bool J1939Interface::open(const J1939Config& config)
{
    m_j1939Config = config;
    m_j1939Config.extendedFrame = true;
    
    m_parameters = config.toVariantMap();
    m_parameters["extended"] = true;
    
    LOG_INFO(QString("J1939 配置: SA=0x%1, PGN=0x%2, Priority=%3")
             .arg(config.sourceAddress, 2, 16, QChar('0'))
             .arg(config.pgn, 5, 16, QChar('0'))
             .arg(config.priority));
    
    return CANInterface::open(static_cast<const CanConfig&>(config));
}

int J1939Interface::send(const QByteArray& data)
{
    if (data.size() > 8) {
        reportError(CommErrorCode::DataTooLarge, 
                    "J1939 数据长度超限",
                    QString("长度=%1，最大=8").arg(data.size()));
        return -1;
    }

    J1939Frame frame;
    frame.priority = m_j1939Config.priority;
    frame.pgn = m_parameters.value("pgn", m_j1939Config.pgn).toUInt();
    frame.sourceAddress = static_cast<quint8>(
        m_parameters.value("sourceAddress", m_j1939Config.sourceAddress).toUInt());
    
    if (CANHelpers::isPDU1(frame.pgn)) {
        quint8 destAddr = static_cast<quint8>(
            m_parameters.value("destinationAddress", m_j1939Config.destinationAddress).toUInt());
        frame.destinationAddress = destAddr;
    } else {
        frame.destinationAddress.reset();
    }
    
    frame.payload = data;

    return sendJ1939Frame(frame) ? data.size() : -1;
}

bool J1939Interface::sendJ1939Frame(const J1939Frame& frame)
{
    if (frame.payload.size() > 8) {
        reportError(CommErrorCode::DataTooLarge, 
                    "J1939 帧数据超限",
                    QString("长度=%1").arg(frame.payload.size()));
        return false;
    }
    
    CANMessage canMessage;
    canMessage.id = CANHelpers::buildJ1939Identifier(frame);
    canMessage.payload = frame.payload;
    canMessage.extended = true;
    
    return sendFrame(canMessage);
}

bool J1939Interface::sendToPGN(quint32 pgn, const QByteArray& data, 
                                std::optional<quint8> destAddr)
{
    J1939Frame frame;
    frame.priority = m_j1939Config.priority;
    frame.pgn = pgn;
    frame.sourceAddress = m_j1939Config.sourceAddress;
    frame.destinationAddress = destAddr;
    frame.payload = data;
    
    return sendJ1939Frame(frame);
}

QByteArray J1939Interface::receive(int timeout_ms)
{
    auto frame = takeFrame(timeout_ms);
    if (!frame.has_value()) {
        return {};
    }

    if (!frame->extended) {
        return {};
    }

    J1939Frame decoded = CANHelpers::parseJ1939Identifier(frame->id, frame->payload);

    QJsonObject obj;
    obj["protocol"] = "J1939";
    obj["priority"] = static_cast<int>(decoded.priority);
    obj["pgn"] = static_cast<qint64>(decoded.pgn);
    obj["sourceAddress"] = static_cast<int>(decoded.sourceAddress);
    
    if (decoded.destinationAddress.has_value()) {
        obj["destinationAddress"] = static_cast<int>(decoded.destinationAddress.value());
    }
    
    obj["payload"] = QString::fromLatin1(decoded.payload.toHex());
    obj["timestamp"] = decoded.timestamp;

    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);
}

void J1939Interface::onRawFrameReceived(const CANMessage& frame)
{
    if (!frame.extended) {
        return;
    }
    
    J1939Frame decoded = CANHelpers::parseJ1939Identifier(frame.id, frame.payload);
    decoded.timestamp = frame.timestamp;
    
    emit j1939FrameReceived(decoded);
}
