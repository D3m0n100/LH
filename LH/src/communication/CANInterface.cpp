// 文件：src/communication/CANInterface.cpp
// CAN 通信接口实现

#include "CANInterface.h"
#include <QCanBus>
#include <QCanBusFrame>
#include <QDateTime>
#include "Common.h"

CANInterface::CANInterface(QObject *parent)
    : ICommInterface(parent)
    , m_device(nullptr)
{
    m_protocolType = CommProtocolType::CAN;
    
    qRegisterMetaType<CANMessage>("CANMessage");
    qRegisterMetaType<CommError>("CommError");
}

CANInterface::~CANInterface()
{
    close();
}

bool CANInterface::open(const QVariantMap& config)
{
    m_config = CanConfig::fromMap(config);
    return open(m_config);
}

bool CANInterface::open(const CanConfig& config)
{
    if (isConnected()) {
        close();
    }
    
    m_config = config;
    m_parameters = config.toVariantMap();
    
    if (!config.isValid()) {
        reportError(CommErrorCode::InvalidConfig, 
                    "无效的 CAN 配置",
                    QString("plugin=%1, interface=%2").arg(config.plugin, config.interface));
        return false;
    }
    
    QString errorString;
    m_device = QCanBus::instance()->createDevice(config.plugin, config.interface, &errorString);
    
    if (!m_device) {
        reportError(CommErrorCode::DeviceNotFound,
                    "创建 CAN 设备失败",
                    errorString);
        return false;
    }
    
    m_device->setConfigurationParameter(QCanBusDevice::BitRateKey, config.bitrate);
    
    connect(m_device, &QCanBusDevice::framesReceived, 
            this, &CANInterface::onFramesReceived);
    connect(m_device, &QCanBusDevice::errorOccurred, 
            this, &CANInterface::onDeviceError);
    connect(m_device, &QCanBusDevice::stateChanged,
            this, &CANInterface::onDeviceStateChanged);
    
    if (!m_device->connectDevice()) {
        reportError(CommErrorCode::ConnectionFailed,
                    "打开 CAN 设备失败",
                    m_device->errorString());
        delete m_device;
        m_device = nullptr;
        return false;
    }
    
    emit connectionStateChanged(true);
    LOG_INFO(QString("CAN 接口已打开: %1 @ %2 bps")
             .arg(config.interface)
             .arg(config.bitrate));
    return true;
}

void CANInterface::close()
{
    if (m_device) {
        m_device->disconnectDevice();
        delete m_device;
        m_device = nullptr;
        
        {
            QMutexLocker locker(&m_receiveMutex);
            m_receiveBuffer.clear();
            m_frameAvailable.wakeAll();
        }
        
        emit connectionStateChanged(false);
        LOG_INFO("CAN 接口已关闭");
    }
}

int CANInterface::send(const QByteArray& data)
{
    CANMessage frame;
    frame.id = m_config.defaultFrameId;
    frame.payload = data;
    frame.extended = m_config.extendedFrame;
    
    return sendFrame(frame) ? data.size() : -1;
}

bool CANInterface::sendFrame(const CANMessage& frame)
{
    if (!isConnected()) {
        reportError(CommErrorCode::SendFailed, "发送失败：设备未连接");
        return false;
    }
    
    if (frame.payload.size() > 8) {
        reportError(CommErrorCode::DataTooLarge, 
                    "CAN 数据长度超限",
                    QString("长度=%1，最大=8").arg(frame.payload.size()));
        return false;
    }
    
    QCanBusFrame busFrame;
    busFrame.setFrameId(frame.id);
    busFrame.setPayload(frame.payload);
    busFrame.setExtendedFrameFormat(frame.extended);
    busFrame.setFrameType(frame.remoteRequest ? 
                          QCanBusFrame::RemoteRequestFrame : 
                          QCanBusFrame::DataFrame);
    
    if (m_device->writeFrame(busFrame)) {
        return true;
    }
    
    reportError(CommErrorCode::SendFailed, 
                "发送 CAN 帧失败",
                m_device->errorString());
    return false;
}

QByteArray CANInterface::receive(int timeout_ms)
{
    auto frame = takeFrame(timeout_ms);
    if (!frame.has_value()) {
        return QByteArray();
    }
    return frame->payload;
}

std::optional<CANMessage> CANInterface::takeFrame(int timeout_ms)
{
    QMutexLocker locker(&m_receiveMutex);
    
    if (m_receiveBuffer.isEmpty() && timeout_ms > 0) {
        m_frameAvailable.wait(&m_receiveMutex, static_cast<unsigned long>(timeout_ms));
    }
    
    if (m_receiveBuffer.isEmpty()) {
        return std::nullopt;
    }
    
    return m_receiveBuffer.dequeue();
}

bool CANInterface::hasFrameAvailable() const
{
    QMutexLocker locker(&m_receiveMutex);
    return !m_receiveBuffer.isEmpty();
}

bool CANInterface::isConnected() const
{
    return m_device && m_device->state() == QCanBusDevice::ConnectedState;
}

CanConfig CANInterface::currentConfig() const
{
    return m_config;
}

void CANInterface::onFramesReceived()
{
    while (m_device && m_device->framesAvailable() > 0) {
        QCanBusFrame busFrame = m_device->readFrame();
        
        CANMessage message;
        message.id = busFrame.frameId();
        message.payload = busFrame.payload();
        message.extended = busFrame.hasExtendedFrameFormat();
        message.remoteRequest = (busFrame.frameType() == QCanBusFrame::RemoteRequestFrame);
        message.timestamp = QDateTime::currentMSecsSinceEpoch();
        
        {
            QMutexLocker locker(&m_receiveMutex);
            
            if (m_receiveBuffer.size() >= MAX_BUFFER_SIZE) {
                m_receiveBuffer.dequeue();
                LOG_WARN("CAN 接收缓冲区溢出，丢弃最旧数据");
            }
            
            m_receiveBuffer.enqueue(message);
            m_frameAvailable.wakeOne();
        }
        
        emit dataReceived(message.payload);
        emit frameReceived(message);
    }
}

void CANInterface::onDeviceError(QCanBusDevice::CanBusError error)
{
    if (error == QCanBusDevice::NoError) {
        return;
    }
    
    CommErrorCode code = CommErrorCode::UnknownError;
    switch (error) {
        case QCanBusDevice::ReadError:
            code = CommErrorCode::ReceiveFailed;
            break;
        case QCanBusDevice::WriteError:
            code = CommErrorCode::SendFailed;
            break;
        case QCanBusDevice::ConnectionError:
            code = CommErrorCode::ConnectionFailed;
            break;
        case QCanBusDevice::ConfigurationError:
            code = CommErrorCode::InvalidConfig;
            break;
        case QCanBusDevice::TimeoutError:
            code = CommErrorCode::ReceiveTimeout;
            break;
        default:
            break;
    }
    
    reportError(code, "CAN 设备错误", m_device ? m_device->errorString() : "");
}

void CANInterface::onDeviceStateChanged(QCanBusDevice::CanBusDeviceState state)
{
    switch (state) {
        case QCanBusDevice::ConnectedState:
            emit connectionStateChanged(true);
            LOG_INFO("CAN 设备已连接");
            break;
        case QCanBusDevice::UnconnectedState:
            emit connectionStateChanged(false);
            LOG_INFO("CAN 设备已断开");
            break;
        case QCanBusDevice::ConnectingState:
            LOG_INFO("CAN 设备连接中...");
            break;
        case QCanBusDevice::ClosingState:
            LOG_INFO("CAN 设备关闭中...");
            break;
    }
}
