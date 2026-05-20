// 文件：src/communication/SerialInterface.cpp
// 串口通信接口实现

#include "SerialInterface.h"
#include <QDebug>
#include <QMutexLocker>
#include <QtSerialPort/QSerialPortInfo>
#include <QEventLoop>
#include <QRegularExpression>
#include "Common.h"

SerialInterface::SerialInterface(QObject* parent)
    : ICommInterface(parent)
    , m_serialPort(new QSerialPort(this))
    , m_baudRateDetectionTimer(new QTimer(this))
    , m_frameProcessingTimer(new QTimer(this))
    , m_protocol(SerialProtocol::Raw)
    , m_currentBaudRateIndex(0)
    , m_bytesReceived(0)
    , m_bytesSent(0)
    , m_frameErrors(0)
{
    m_protocolType = CommProtocolType::Serial;
    
    m_baudRateCandidates << 9600 << 19200 << 38400 << 57600 
                         << 115200 << 230400 << 460800 << 921600;
    
    connect(m_serialPort, &QSerialPort::readyRead, 
            this, &SerialInterface::onReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred,
            this, &SerialInterface::onErrorOccurred);
    
    m_baudRateDetectionTimer->setSingleShot(true);
    connect(m_baudRateDetectionTimer, &QTimer::timeout, 
            this, &SerialInterface::onBaudRateDetectionTimeout);
    
    m_frameProcessingTimer->setSingleShot(true);
    connect(m_frameProcessingTimer, &QTimer::timeout, 
            this, &SerialInterface::processReceivedData);
}

SerialInterface::~SerialInterface()
{
    close();
}

bool SerialInterface::open(const QVariantMap& config)
{
    m_serialConfig = SerialConfig::fromMap(config);
    return open(m_serialConfig);
}

bool SerialInterface::open(const SerialConfig& config)
{
    close();
    
    m_serialConfig = config;
    m_parameters = config.toVariantMap();
    
    QString portName = config.portName;
    if (portName.isEmpty()) {
        QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
        if (!ports.isEmpty()) {
            portName = ports.first().portName();
            LOG_INFO(QString("自动检测到串口: %1").arg(portName));
        } else {
            reportError(CommErrorCode::DeviceNotFound, "未找到可用串口");
            return false;
        }
    }
    
    m_serialPort->setPortName(portName);
    
    if (config.baudRate == 0) {
        LOG_INFO("启动波特率自动检测");
        return autoDetectBaudRate();
    }
    
    if (!m_serialPort->setBaudRate(config.baudRate)) {
        reportError(CommErrorCode::UnsupportedBaudRate, 
                    "设置波特率失败",
                    QString("波特率=%1").arg(config.baudRate));
        return false;
    }
    
    if (!setDataBits(config.dataBits)) {
        return false;
    }
    
    if (!setParity(parityFromString(config.parity))) {
        return false;
    }
    
    QSerialPort::StopBits stopBits = (config.stopBits == 2) ? 
                                      QSerialPort::TwoStop : QSerialPort::OneStop;
    if (!setStopBits(stopBits)) {
        return false;
    }
    
    if (!setFlowControl(flowControlFromString(config.flowControl))) {
        return false;
    }
    
    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        reportError(CommErrorCode::ConnectionFailed, 
                    "打开串口失败",
                    m_serialPort->errorString());
        return false;
    }
    
    m_serialPort->clear();
    
    {
        QMutexLocker locker(&m_bufferMutex);
        m_receiveBuffer.clear();
    }
    
    emit connectionStateChanged(true);
    LOG_INFO(QString("串口已打开: %1 @ %2 bps")
             .arg(portName)
             .arg(config.baudRate));
    return true;
}

void SerialInterface::close()
{
    m_baudRateDetectionTimer->stop();
    m_frameProcessingTimer->stop();
    
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
        
        {
            QMutexLocker locker(&m_bufferMutex);
            m_receiveBuffer.clear();
            m_receiveWaitCondition.wakeAll();
        }
        
        emit connectionStateChanged(false);
        LOG_INFO("串口已关闭");
    }
}

int SerialInterface::send(const QByteArray& data)
{
    if (!isConnected()) {
        reportError(CommErrorCode::SendFailed, "发送失败：串口未打开");
        return -1;
    }
    
    qint64 written = m_serialPort->write(data);
    if (written < 0) {
        reportError(CommErrorCode::SendFailed, 
                    "发送数据失败",
                    m_serialPort->errorString());
        return -1;
    }
    
    if (!m_serialPort->waitForBytesWritten(1000)) {
        reportError(CommErrorCode::SendFailed, "发送超时");
        return -1;
    }
    
    m_bytesSent += written;
    return static_cast<int>(written);
}

QByteArray SerialInterface::receive(int timeout_ms)
{
    QMutexLocker locker(&m_bufferMutex);
    
    if (m_receiveBuffer.isEmpty() && timeout_ms > 0) {
        m_receiveWaitCondition.wait(&m_bufferMutex, static_cast<unsigned long>(timeout_ms));
    }
    
    if (m_receiveBuffer.isEmpty()) {
        return QByteArray();
    }
    
    QByteArray data = m_receiveBuffer;
    m_receiveBuffer.clear();
    return data;
}

bool SerialInterface::isConnected() const
{
    return m_serialPort->isOpen();
}

bool SerialInterface::setBaudRate(int baudRate)
{
    if (!m_serialPort->setBaudRate(baudRate)) {
        reportError(CommErrorCode::UnsupportedBaudRate,
                    "设置波特率失败",
                    QString("波特率=%1").arg(baudRate));
        return false;
    }
    m_serialConfig.baudRate = baudRate;
    return true;
}

bool SerialInterface::setDataBits(int dataBits)
{
    QSerialPort::DataBits bits;
    switch (dataBits) {
        case 5: bits = QSerialPort::Data5; break;
        case 6: bits = QSerialPort::Data6; break;
        case 7: bits = QSerialPort::Data7; break;
        case 8: bits = QSerialPort::Data8; break;
        default:
            reportError(CommErrorCode::InvalidParameter,
                        "无效的数据位数",
                        QString("dataBits=%1").arg(dataBits));
            return false;
    }
    
    if (!m_serialPort->setDataBits(bits)) {
        reportError(CommErrorCode::InvalidConfig, "设置数据位失败");
        return false;
    }
    m_serialConfig.dataBits = dataBits;
    return true;
}

bool SerialInterface::setParity(QSerialPort::Parity parity)
{
    if (!m_serialPort->setParity(parity)) {
        reportError(CommErrorCode::InvalidConfig, "设置校验位失败");
        return false;
    }
    
    switch (parity) {
        case QSerialPort::NoParity:   m_serialConfig.parity = "None"; break;
        case QSerialPort::EvenParity: m_serialConfig.parity = "Even"; break;
        case QSerialPort::OddParity:  m_serialConfig.parity = "Odd"; break;
        default: break;
    }
    return true;
}

bool SerialInterface::setStopBits(QSerialPort::StopBits stopBits)
{
    if (!m_serialPort->setStopBits(stopBits)) {
        reportError(CommErrorCode::InvalidConfig, "设置停止位失败");
        return false;
    }
    m_serialConfig.stopBits = (stopBits == QSerialPort::TwoStop) ? 2 : 1;
    return true;
}

bool SerialInterface::setFlowControl(QSerialPort::FlowControl flowControl)
{
    if (!m_serialPort->setFlowControl(flowControl)) {
        reportError(CommErrorCode::InvalidConfig, "设置流控制失败");
        return false;
    }
    
    switch (flowControl) {
        case QSerialPort::NoFlowControl:       m_serialConfig.flowControl = "None"; break;
        case QSerialPort::HardwareControl:     m_serialConfig.flowControl = "Hardware"; break;
        case QSerialPort::SoftwareControl:     m_serialConfig.flowControl = "Software"; break;
        default: break;
    }
    return true;
}

bool SerialInterface::autoDetectBaudRate()
{
    if (m_baudRateCandidates.isEmpty()) {
        reportError(CommErrorCode::InvalidConfig, "波特率候选列表为空");
        emit baudRateAutoDetectionFailed();
        return false;
    }
    
    m_currentBaudRateIndex = 0;
    m_detectionBuffer.clear();
    m_detectionTimer.start();
    
    return tryBaudRate(m_baudRateCandidates.first());
}

bool SerialInterface::tryBaudRate(int baudRate)
{
    LOG_INFO(QString("尝试波特率: %1").arg(baudRate));
    
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }
    
    if (!m_serialPort->setBaudRate(baudRate)) {
        return false;
    }
    
    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        return false;
    }
    
    m_serialPort->clear();
    m_baudRateDetectionTimer->start(BAUD_DETECTION_TIMEOUT);
    
    return true;
}

void SerialInterface::onBaudRateDetectionTimeout()
{
    if (!m_detectionBuffer.isEmpty() && validateFrame(m_detectionBuffer)) {
        int detectedBaud = m_baudRateCandidates.at(m_currentBaudRateIndex);
        m_serialConfig.baudRate = detectedBaud;
        LOG_INFO(QString("波特率检测成功: %1").arg(detectedBaud));
        emit baudRateDetected(detectedBaud);
        emit connectionStateChanged(true);
        return;
    }
    
    m_currentBaudRateIndex++;
    m_detectionBuffer.clear();
    
    if (m_currentBaudRateIndex < m_baudRateCandidates.size()) {
        tryBaudRate(m_baudRateCandidates.at(m_currentBaudRateIndex));
    } else {
        LOG_WARN("波特率自动检测失败");
        reportError(CommErrorCode::ConnectionFailed, "波特率自动检测失败");
        emit baudRateAutoDetectionFailed();
        close();
    }
}

QList<int> SerialInterface::getSupportedBaudRates() const
{
    return m_baudRateCandidates;
}

void SerialInterface::onReadyRead()
{
    QByteArray data = m_serialPort->readAll();
    if (data.isEmpty()) {
        return;
    }
    
    m_bytesReceived += data.size();
    
    if (m_baudRateDetectionTimer->isActive()) {
        m_detectionBuffer.append(data);
        return;
    }
    
    {
        QMutexLocker locker(&m_bufferMutex);
        
        if (m_receiveBuffer.size() + data.size() > MAX_BUFFER_SIZE) {
            int overflow = m_receiveBuffer.size() + data.size() - MAX_BUFFER_SIZE;
            m_receiveBuffer.remove(0, overflow);
            LOG_WARN("串口接收缓冲区溢出，丢弃旧数据");
        }
        
        m_receiveBuffer.append(data);
        m_receiveWaitCondition.wakeAll();
    }
    
    emit dataReceived(data);
    
    m_frameProcessingTimer->start(m_serialConfig.frameTimeout);
}

void SerialInterface::processReceivedData()
{
    QMutexLocker locker(&m_bufferMutex);
    
    if (m_receiveBuffer.isEmpty()) {
        return;
    }
    
    QByteArray frame = extractFrame(m_receiveBuffer);
    if (!frame.isEmpty()) {
        emit dataFramed(frame);
    }
}

QByteArray SerialInterface::extractFrame(const QByteArray& data)
{
    switch (m_protocol) {
        case SerialProtocol::ModbusRTU:
            if (data.size() >= 4) {
                return data;
            }
            break;
            
        case SerialProtocol::Custom:
            break;
            
        case SerialProtocol::Raw:
        default:
            return data;
    }
    
    return QByteArray();
}

bool SerialInterface::validateFrame(const QByteArray& frame)
{
    if (frame.isEmpty()) {
        return false;
    }
    
    int validBytes = 0;
    for (char c : frame) {
        if ((c >= 0x20 && c <= 0x7E) || c == 0x0D || c == 0x0A || c == 0x09) {
            validBytes++;
        }
    }
    
    return validBytes > frame.size() / 2;
}

void SerialInterface::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }
    
    CommErrorCode code = CommErrorCode::UnknownError;
    switch (error) {
        case QSerialPort::DeviceNotFoundError:
            code = CommErrorCode::DeviceNotFound;
            break;
        case QSerialPort::PermissionError:
            code = CommErrorCode::PermissionDenied;
            break;
        case QSerialPort::OpenError:
            code = CommErrorCode::ConnectionFailed;
            break;
        case QSerialPort::WriteError:
            code = CommErrorCode::SendFailed;
            break;
        case QSerialPort::ReadError:
            code = CommErrorCode::ReceiveFailed;
            break;
        case QSerialPort::ResourceError:
            code = CommErrorCode::ResourceError;
            emit connectionStateChanged(false);
            break;
        case QSerialPort::TimeoutError:
            code = CommErrorCode::ReceiveTimeout;
            break;
        default:
            break;
    }
    
    reportError(code, "串口错误", m_serialPort->errorString());
    m_frameErrors++;
}

QSerialPort::Parity SerialInterface::parityFromString(const QString& str) const
{
    QString upper = str.toUpper();
    if (upper == "EVEN") return QSerialPort::EvenParity;
    if (upper == "ODD") return QSerialPort::OddParity;
    if (upper == "MARK") return QSerialPort::MarkParity;
    if (upper == "SPACE") return QSerialPort::SpaceParity;
    return QSerialPort::NoParity;
}

QSerialPort::FlowControl SerialInterface::flowControlFromString(const QString& str) const
{
    QString upper = str.toUpper();
    if (upper == "HARDWARE" || upper == "RTS/CTS") return QSerialPort::HardwareControl;
    if (upper == "SOFTWARE" || upper == "XON/XOFF") return QSerialPort::SoftwareControl;
    return QSerialPort::NoFlowControl;
}
