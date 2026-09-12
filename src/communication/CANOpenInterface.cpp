// 文件：src/communication/CANOpenInterface.cpp
// CANopen 协议接口实现

#include "CANOpenInterface.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include "Common.h"

CANOpenInterface::CANOpenInterface(QObject* parent)
    : CANInterface(parent)
    , m_defaultCobId(0)
{
    m_protocolType = CommProtocolType::CANOpen;
    
    connect(this, &CANInterface::frameReceived, 
            this, &CANOpenInterface::onRawFrameReceived);
}

bool CANOpenInterface::open(const QVariantMap& config)
{
    m_canopenConfig = CANOpenConfig::fromMap(config);
    return open(m_canopenConfig);
}

bool CANOpenInterface::open(const CANOpenConfig& config)
{
    m_canopenConfig = config;
    
    if (config.cobId != 0) {
        m_defaultCobId = config.cobId;
    } else {
        m_defaultCobId = CANHelpers::buildCANOpenCOBId(config.functionCode, config.nodeId);
    }
    
    m_parameters = config.toVariantMap();
    m_parameters["cobId"] = m_defaultCobId;
    
    LOG_INFO(QString("CANopen 配置: nodeId=%1, functionCode=0x%2, cobId=0x%3")
             .arg(config.nodeId)
             .arg(config.functionCode, 2, 16, QChar('0'))
             .arg(m_defaultCobId, 3, 16, QChar('0')));
    
    return CANInterface::open(static_cast<const CanConfig&>(config));
}

int CANOpenInterface::send(const QByteArray& data)
{
    if (data.size() > 8) {
        reportError(CommErrorCode::DataTooLarge, 
                    "CANopen 数据长度超限",
                    QString("长度=%1，最大=8").arg(data.size()));
        return -1;
    }

    quint16 cobId = static_cast<quint16>(m_parameters.value("cobId", m_defaultCobId).toUInt());
    return sendCANOpenFrame(cobId, data) ? data.size() : -1;
}

bool CANOpenInterface::sendCANOpenFrame(quint16 cobId, const QByteArray& data)
{
    if (data.size() > 8) {
        reportError(CommErrorCode::DataTooLarge, 
                    "CANopen 帧数据超限",
                    QString("长度=%1").arg(data.size()));
        return false;
    }
    
    CANMessage frame;
    frame.id = cobId;
    frame.payload = data;
    frame.extended = false;
    
    return sendFrame(frame);
}

QByteArray CANOpenInterface::receive(int timeout_ms)
{
    auto frame = takeFrame(timeout_ms);
    if (!frame.has_value()) {
        return {};
    }

    if (frame->extended || frame->id > 0x7FF) {
        return {};
    }

    auto cobId = static_cast<quint16>(frame->id & 0x7FF);
    
    QJsonObject obj;
    obj["protocol"] = "CANopen";
    obj["cobId"] = static_cast<int>(cobId);
    obj["functionCode"] = static_cast<int>(CANHelpers::extractCANOpenFunctionCode(cobId));
    obj["nodeId"] = static_cast<int>(CANHelpers::extractCANOpenNodeId(cobId));
    obj["payload"] = QString::fromLatin1(frame->payload.toHex());
    obj["timestamp"] = frame->timestamp;

    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);
}

void CANOpenInterface::onRawFrameReceived(const CANMessage& frame)
{
    if (frame.extended || frame.id > 0x7FF) {
        return;
    }
    
    auto cobId = static_cast<quint16>(frame.id);
    quint8 funcCode = CANHelpers::extractCANOpenFunctionCode(cobId);
    quint8 nodeId = CANHelpers::extractCANOpenNodeId(cobId);
    
    emit canOpenFrameReceived(cobId, funcCode, nodeId, frame.payload);
}
