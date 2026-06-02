// 文件：src/communication/ModbusInterface.cpp
// Modbus 通信接口实现（基于 QtSerialBus，RTU/TCP 主站）

#include "ModbusInterface.h"
#include "Common.h"

#include <QEventLoop>
#include <QTimer>

#include <QtSerialBus/QModbusClient>
#include <QtSerialBus/QModbusReply>
#include <QtSerialBus/QModbusDevice>

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
  #include <QtSerialBus/QModbusRtuSerialClient>
#else
  #include <QtSerialBus/QModbusRtuSerialMaster>
#endif
#include <QtSerialBus/QModbusTcpClient>

#include <QtSerialPort/QSerialPort>

namespace {

static QString stateToString(int state)
{
    switch (state) {
    case QModbusDevice::UnconnectedState: return "UnconnectedState";
    case QModbusDevice::ConnectingState:  return "ConnectingState";
    case QModbusDevice::ConnectedState:   return "ConnectedState";
    case QModbusDevice::ClosingState:     return "ClosingState";
    default: return QString("UnknownState(%1)").arg(state);
    }
}

static QString errorToString(int error)
{
    switch (error) {
    case QModbusDevice::NoError:            return "NoError";
    case QModbusDevice::ConnectionError:    return "ConnectionError";
    case QModbusDevice::ConfigurationError: return "ConfigurationError";
    case QModbusDevice::TimeoutError:       return "TimeoutError";
    case QModbusDevice::ProtocolError:      return "ProtocolError";
    case QModbusDevice::ReplyAbortedError:  return "ReplyAbortedError";
    case QModbusDevice::UnknownError:       return "UnknownError";
    default: return QString("UnknownError(%1)").arg(error);
    }
}

} // namespace

ModbusInterface::ModbusInterface(QObject* parent)
    : ICommInterface(parent)
{
    m_protocolType = CommProtocolType::ModbusRTU;
}

ModbusInterface::~ModbusInterface()
{
    close();
}

bool ModbusInterface::open(const QVariantMap& config)
{
    m_config = ModbusConfig::fromMap(config);
    return open(m_config);
}

bool ModbusInterface::open(const ModbusConfig& config)
{
    m_config = config;

    if (!m_config.isValid()) {
        reportError(CommErrorCode::InvalidConfig, "Modbus 配置无效");
        return false;
    }

    m_protocolType = (m_config.mode == ModbusConfig::Mode::TCP)
        ? CommProtocolType::ModbusTCP
        : CommProtocolType::ModbusRTU;

    if (!createClientIfNeeded()) {
        return false;
    }

    applyTimeoutRetry();

    bool ok = false;
    if (m_config.mode == ModbusConfig::Mode::RTU) {
        ok = configureRtuClient();
    } else {
        ok = configureTcpClient();
    }
    if (!ok) {
        return false;
    }

    if (m_config.stationType != ModbusConfig::StationType::Master) {
        reportError(CommErrorCode::NotImplemented, "当前仅实现 Modbus 主站模式（Master）");
        return false;
    }

    if (m_client->state() == QModbusDevice::ConnectedState) {
        m_connected = true;
        emit connectionStateChanged(true);
        return true;
    }

    LOG_INFO(QString("Modbus connectDevice()... mode=%1 addr=%2 timeout=%3ms retry=%4")
                 .arg(m_config.mode == ModbusConfig::Mode::TCP ? "TCP" : "RTU")
                 .arg(m_config.stationAddress)
                 .arg(m_config.responseTimeout)
                 .arg(m_config.retryCount));

    if (!m_client->connectDevice()) {
        reportError(CommErrorCode::ConnectionFailed, "Modbus 连接失败", m_client->errorString());
        m_connected = false;
        emit connectionStateChanged(false);
        return false;
    }

    // 等待进入 ConnectedState 或回到 UnconnectedState（失败）
    const int waitMs = qMax(500, m_config.responseTimeout);
    const bool done = waitForCondition([this]() {
        if (!m_client) return true;
        const auto st = m_client->state();
        return st == QModbusDevice::ConnectedState || st == QModbusDevice::UnconnectedState;
    }, waitMs);

    if (!done || !m_client || m_client->state() != QModbusDevice::ConnectedState) {
        reportError(CommErrorCode::ConnectionTimeout,
                    "Modbus 连接超时（串口可能未打开/参数错误/设备未上电）",
                    m_client ? m_client->errorString() : "client=null");
        if (m_client) m_client->disconnectDevice();
        m_connected = false;
        emit connectionStateChanged(false);
        return false;
    }

    m_connected = true;
    emit connectionStateChanged(true);
    return true;
}

void ModbusInterface::close()
{
    if (m_client) {
        if (m_client->state() == QModbusDevice::ConnectedState ||
            m_client->state() == QModbusDevice::ConnectingState) {
            m_client->disconnectDevice();
        }
        m_client->deleteLater();
        m_client = nullptr;
    }
    m_connected = false;
    emit connectionStateChanged(false);
}

bool ModbusInterface::isConnected() const
{
    return m_connected && m_client && (m_client->state() == QModbusDevice::ConnectedState);
}

int ModbusInterface::send(const QByteArray& data)
{
    Q_UNUSED(data);
    reportError(CommErrorCode::NotImplemented,
                "ModbusInterface::send 不支持原始帧发送，请使用 read/write API");
    return -1;
}

QByteArray ModbusInterface::receive(int timeout_ms)
{
    Q_UNUSED(timeout_ms);
    QMutexLocker locker(&m_bufferMutex);
    const QByteArray out = m_receiveBuffer;
    m_receiveBuffer.clear();
    return out;
}

QMap<int, QVector<quint16>> ModbusInterface::holdingRegisters() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_holdingRegisters;
}

QMap<int, QVector<quint16>> ModbusInterface::inputRegisters() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_inputRegisters;
}

QMap<int, QVector<bool>> ModbusInterface::coils() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_coils;
}

QMap<int, QVector<bool>> ModbusInterface::discreteInputs() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_discreteInputs;
}

// ========================= Fun3 Read =========================

bool ModbusInterface::readHoldingRegisters(int address, int count)
{
    if (!isConnected()) {
        reportError(CommErrorCode::ConnectionLost, "Modbus 未连接");
        return false;
    }
    if (count <= 0 || count > MAX_REGISTERS) {
        reportError(CommErrorCode::InvalidParameter, "readHoldingRegisters: count 非法");
        return false;
    }
    return readDataUnit(QModbusDataUnit(QModbusDataUnit::HoldingRegisters, address, count),
                        m_config.stationAddress);
}

bool ModbusInterface::readInputRegisters(int address, int count)
{
    if (!isConnected()) {
        reportError(CommErrorCode::ConnectionLost, "Modbus 未连接");
        return false;
    }
    if (count <= 0 || count > MAX_REGISTERS) {
        reportError(CommErrorCode::InvalidParameter, "readInputRegisters: count 非法");
        return false;
    }
    return readDataUnit(QModbusDataUnit(QModbusDataUnit::InputRegisters, address, count),
                        m_config.stationAddress);
}

bool ModbusInterface::readCoils(int address, int count)
{
    if (!isConnected()) {
        reportError(CommErrorCode::ConnectionLost, "Modbus 未连接");
        return false;
    }
    if (count <= 0 || count > MAX_COILS) {
        reportError(CommErrorCode::InvalidParameter, "readCoils: count 非法");
        return false;
    }
    return readDataUnit(QModbusDataUnit(QModbusDataUnit::Coils, address, count),
                        m_config.stationAddress);
}

bool ModbusInterface::readDiscreteInputs(int address, int count)
{
    if (!isConnected()) {
        reportError(CommErrorCode::ConnectionLost, "Modbus 未连接");
        return false;
    }
    if (count <= 0 || count > MAX_COILS) {
        reportError(CommErrorCode::InvalidParameter, "readDiscreteInputs: count 非法");
        return false;
    }
    return readDataUnit(QModbusDataUnit(QModbusDataUnit::DiscreteInputs, address, count),
                        m_config.stationAddress);
}

// ========================= Fun16/Fun15 Write =========================

bool ModbusInterface::writeSingleRegister(int address, quint16 value)
{
    return writeMultipleRegisters(address, QVector<quint16>{value});
}

bool ModbusInterface::writeMultipleRegisters(int address, const QVector<quint16>& values)
{
    if (!isConnected()) {
        reportError(CommErrorCode::ConnectionLost, "Modbus 未连接");
        return false;
    }
    if (values.isEmpty() || values.size() > MAX_REGISTERS) {
        reportError(CommErrorCode::InvalidParameter, "writeMultipleRegisters: values 数量非法");
        return false;
    }

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, address, values.size());
    for (int i = 0; i < values.size(); ++i) {
        unit.setValue(i, values[i]);
    }
    return writeDataUnit(unit, m_config.stationAddress);
}

bool ModbusInterface::writeSingleCoil(int address, bool value)
{
    return writeMultipleCoils(address, QVector<bool>{value});
}

bool ModbusInterface::writeMultipleCoils(int address, const QVector<bool>& values)
{
    if (!isConnected()) {
        reportError(CommErrorCode::ConnectionLost, "Modbus 未连接");
        return false;
    }
    if (values.isEmpty() || values.size() > MAX_COILS) {
        reportError(CommErrorCode::InvalidParameter, "writeMultipleCoils: values 数量非法");
        return false;
    }

    QModbusDataUnit unit(QModbusDataUnit::Coils, address, values.size());
    for (int i = 0; i < values.size(); ++i) {
        unit.setValue(i, values[i] ? 1 : 0);
    }
    return writeDataUnit(unit, m_config.stationAddress);
}

// ========================= Internal helpers =========================

bool ModbusInterface::createClientIfNeeded()
{
    // 若已有 client 但模式变化，重建
    if (m_client) {
        const bool shouldBeTcp = (m_config.mode == ModbusConfig::Mode::TCP);
        const bool isTcp = (qobject_cast<QModbusTcpClient*>(m_client.data()) != nullptr);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        const bool isRtu = (qobject_cast<QModbusRtuSerialClient*>(m_client.data()) != nullptr);
#else
        const bool isRtu = (qobject_cast<QModbusRtuSerialMaster*>(m_client.data()) != nullptr);
#endif
        if ((shouldBeTcp && isTcp) || (!shouldBeTcp && isRtu)) {
            return true;
        }
        close();
    }

    if (m_config.mode == ModbusConfig::Mode::TCP) {
        m_client = new QModbusTcpClient(this);
    } else {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        m_client = new QModbusRtuSerialClient(this);
#else
        m_client = new QModbusRtuSerialMaster(this);
#endif
    }

    if (!m_client) {
        reportError(CommErrorCode::ResourceError, "创建 Modbus Client 失败");
        return false;
    }

    connect(m_client, &QModbusClient::stateChanged, this, &ModbusInterface::onStateChanged);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    connect(m_client, &QModbusClient::errorOccurred, this, &ModbusInterface::onDeviceError);
#else
    connect(m_client, SIGNAL(error(QModbusDevice::Error)), this, SLOT(onDeviceError(int)));
#endif

    return true;
}

void ModbusInterface::applyTimeoutRetry()
{
    if (!m_client) return;

    // QtSerialBus：numberOfRetries 表示“额外重试次数”（不含第一次发送）
    const int retries = qMax(0, m_config.retryCount - 1);
    m_client->setTimeout(qMax(50, m_config.responseTimeout));
    m_client->setNumberOfRetries(retries);
}

bool ModbusInterface::configureRtuClient()
{
    if (!m_client) return false;

    // portName is configured by ModbusConfig::fromVariantMap("port")
    if (m_config.portName.isEmpty()) {
        reportError(CommErrorCode::InvalidConfig, "未指定串口端口名（port）");
        return false;
    }

    // 串口参数
    m_client->setConnectionParameter(QModbusDevice::SerialPortNameParameter, m_config.portName);
    m_client->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, m_config.baudRate);
    m_client->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, m_config.dataBits);

    // parity
    QSerialPort::Parity parity = QSerialPort::NoParity;
    const QString p = m_config.parity.trimmed().toLower();
    if (p == "even") parity = QSerialPort::EvenParity;
    else if (p == "odd") parity = QSerialPort::OddParity;
    else if (p == "mark") parity = QSerialPort::MarkParity;
    else if (p == "space") parity = QSerialPort::SpaceParity;
    else parity = QSerialPort::NoParity;

    // stop bits
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    if (m_config.stopBits == 2) stopBits = QSerialPort::TwoStop;

    m_client->setConnectionParameter(QModbusDevice::SerialParityParameter, parity);
    m_client->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, stopBits);

    LOG_INFO(QString("Modbus RTU config: port=%1 baud=%2 dataBits=%3 parity=%4 stopBits=%5")
                 .arg(m_config.portName).arg(m_config.baudRate).arg(m_config.dataBits)
                 .arg(m_config.parity).arg(m_config.stopBits));

    return true;
}

bool ModbusInterface::configureTcpClient()
{
    if (!m_client) return false;

    if (m_config.host.isEmpty() || m_config.port <= 0) {
        reportError(CommErrorCode::InvalidConfig, "未指定 TCP host/port（host/tcpPort）");
        return false;
    }

    m_client->setConnectionParameter(QModbusDevice::NetworkAddressParameter, m_config.host);
    m_client->setConnectionParameter(QModbusDevice::NetworkPortParameter, m_config.port);

    LOG_INFO(QString("Modbus TCP config: %1:%2").arg(m_config.host).arg(m_config.port));
    return true;
}

void ModbusInterface::onStateChanged(int state)
{
    LOG_INFO(QString("Modbus stateChanged: %1").arg(stateToString(state)));
    const bool nowConnected = (state == QModbusDevice::ConnectedState);
    if (m_connected != nowConnected) {
        m_connected = nowConnected;
        emit connectionStateChanged(m_connected);
    }
}

void ModbusInterface::onDeviceError(int error)
{
    if (!m_client) return;
    if (error == QModbusDevice::NoError) return;

    LOG_WARN(QString("Modbus deviceError: %1 (%2)")
                 .arg(errorToString(error))
                 .arg(m_client->errorString()));

    reportError(mapQtErrorToCommError(error),
                "Modbus 设备错误",
                m_client->errorString());
}

CommErrorCode ModbusInterface::mapQtErrorToCommError(int qtError) const
{
    switch (static_cast<QModbusDevice::Error>(qtError)) {
    case QModbusDevice::NoError:            return CommErrorCode::NoError;
    case QModbusDevice::TimeoutError:       return CommErrorCode::ReceiveTimeout;
    case QModbusDevice::ConfigurationError: return CommErrorCode::InvalidConfig;
    case QModbusDevice::ConnectionError:    return CommErrorCode::ConnectionFailed;
    case QModbusDevice::ProtocolError:      return CommErrorCode::ProtocolError;
    case QModbusDevice::ReplyAbortedError:  return CommErrorCode::OperationCancelled;
    case QModbusDevice::UnknownError:       return CommErrorCode::UnknownError;
    default:                                return CommErrorCode::UnknownError;
    }
}

CommErrorCode ModbusInterface::mapReplyErrorToCommError(int replyError) const
{
    switch (static_cast<QModbusDevice::Error>(replyError)) {
    case QModbusDevice::NoError:            return CommErrorCode::NoError;
    case QModbusDevice::TimeoutError:       return CommErrorCode::ReceiveTimeout;
    case QModbusDevice::ProtocolError:      return CommErrorCode::ProtocolError;
    case QModbusDevice::ReplyAbortedError:  return CommErrorCode::OperationCancelled;
    case QModbusDevice::ConnectionError:    return CommErrorCode::ConnectionLost;
    case QModbusDevice::ConfigurationError: return CommErrorCode::InvalidConfig;
    default:                                return CommErrorCode::InvalidResponse;
    }
}

bool ModbusInterface::waitForReply(QModbusReply* reply, QString& outErr)
{
    if (!reply) {
        outErr = "reply=null";
        return false;
    }

    if (reply->isFinished()) {
        if (reply->error() != QModbusDevice::NoError) {
            outErr = reply->errorString();
            return false;
        }
        return true;
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    // 超时裕量：timeout * attempts + 200ms
    const int attempts = qMax(1, m_config.retryCount);
    const int waitMs = qMax(50, m_config.responseTimeout) * attempts + 200;
    timer.start(waitMs);

    loop.exec();

    if (!timer.isActive()) {
        outErr = QString("等待应答超时（%1ms）").arg(waitMs);
        return false;
    }

    if (reply->error() != QModbusDevice::NoError) {
        outErr = reply->errorString();
        return false;
    }

    return true;
}

bool ModbusInterface::readDataUnit(const QModbusDataUnit& unit, int serverAddress)
{
    if (!m_client) return false;

    QModbusReply* reply = m_client->sendReadRequest(unit, serverAddress);
    if (!reply) {

        reportError(CommErrorCode::SendFailed, "发送读请求失败", m_client->errorString());
        return false;
    }

    // Ensure reply is cleaned up after it finishes (Qt5 compatibility: no abort())
    connect(reply, &QModbusReply::finished, reply, &QObject::deleteLater);

    QString err;
    if (!waitForReply(reply, err)) {
        const CommErrorCode code = mapReplyErrorToCommError(reply->error());
        reportError(code, "Modbus 读请求失败", err);
        return false;
    }

    const QModbusDataUnit result = reply->result();
    QVector<quint16> values;
    values.reserve(static_cast<int>(result.valueCount()));
    for (uint i = 0; i < result.valueCount(); ++i) {
        values.push_back(result.value(i));
    }

    // cache + signal
    if (result.registerType() == QModbusDataUnit::Coils || result.registerType() == QModbusDataUnit::DiscreteInputs) {
        QVector<bool> bools;
        bools.reserve(values.size());
        for (auto v : values) bools.push_back(v != 0);

        {
            QMutexLocker locker(&m_dataMutex);
            if (result.registerType() == QModbusDataUnit::Coils)
                m_coils[result.startAddress()] = bools;
            else
                m_discreteInputs[result.startAddress()] = bools;
        }
        emit coilDataReceived(result.startAddress(), bools);
        return true;
    }

    {
        QMutexLocker locker(&m_dataMutex);
        if (result.registerType() == QModbusDataUnit::HoldingRegisters)
            m_holdingRegisters[result.startAddress()] = values;
        else if (result.registerType() == QModbusDataUnit::InputRegisters)
            m_inputRegisters[result.startAddress()] = values;
    }

    emit registerDataReceived(result.startAddress(), values);
    return true;
}

bool ModbusInterface::writeDataUnit(const QModbusDataUnit& unit, int serverAddress)
{
    if (!m_client) return false;

    QModbusReply* reply = m_client->sendWriteRequest(unit, serverAddress);
    if (!reply) {

        reportError(CommErrorCode::SendFailed, "发送写请求失败", m_client->errorString());
        return false;
    }

    // Ensure reply is cleaned up after it finishes (Qt5 compatibility: no abort())
    connect(reply, &QModbusReply::finished, reply, &QObject::deleteLater);

    QString err;
    if (!waitForReply(reply, err)) {
        const CommErrorCode code = mapReplyErrorToCommError(reply->error());
        reportError(code, "Modbus 写请求失败", err);
        return false;
    }
    emit writeCompleted(unit.startAddress(), unit.valueCount());
    return true;
}
