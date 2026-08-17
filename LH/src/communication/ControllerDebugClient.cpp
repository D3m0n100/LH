// File: src/communication/ControllerDebugClient.cpp

#include "ControllerDebugClient.h"

#include "ModbusInterface.h"

#include <QMap>

ModbusControllerDebugTransport::ModbusControllerDebugTransport(ModbusInterface* modbus)
    : m_modbus(modbus)
{
}

bool ModbusControllerDebugTransport::isConnected() const
{
    return m_modbus && m_modbus->isConnected();
}

int ModbusControllerDebugTransport::stationAddress() const
{
    return m_modbus ? m_modbus->stationAddress() : 0;
}

void ModbusControllerDebugTransport::setStationAddress(int deviceId)
{
    if (m_modbus) {
        m_modbus->setStationAddress(deviceId);
    }
}

bool ModbusControllerDebugTransport::readHoldingRegisters(int address, int count)
{
    return m_modbus && m_modbus->readHoldingRegisters(address, count);
}

QVector<quint16> ModbusControllerDebugTransport::holdingRegisterValues(int address) const
{
    if (!m_modbus) {
        return {};
    }
    return m_modbus->holdingRegisters().value(address);
}

bool ModbusControllerDebugTransport::writeMultipleRegisters(int address, const QVector<quint16>& values)
{
    return m_modbus && m_modbus->writeMultipleRegisters(address, values);
}

CommError ModbusControllerDebugTransport::lastError() const
{
    return m_modbus ? m_modbus->lastError() : CommError();
}

ControllerDebugClient::ControllerDebugClient(QObject* parent)
    : QObject(parent)
{
}

ControllerDebugClient::ControllerDebugClient(IControllerDebugTransport* transport, QObject* parent)
    : QObject(parent)
    , m_transport(transport)
{
}

ControllerDebugClient::~ControllerDebugClient() = default;

void ControllerDebugClient::setTransport(IControllerDebugTransport* transport)
{
    m_ownedTransport.reset();
    m_ownedModbus.reset();
    m_transport = transport;
    clearError();
}

void ControllerDebugClient::attachModbus(ModbusInterface* modbus)
{
    m_ownedTransport = std::make_unique<ModbusControllerDebugTransport>(modbus);
    m_ownedModbus.reset();
    m_transport = m_ownedTransport.get();
    clearError();
}

bool ControllerDebugClient::openRtu(const QString& portName, int deviceId)
{
    if (!ControllerDebugProtocol::canReadDeviceId(deviceId)) {
        return fail(CommErrorCode::InvalidAddress,
                    QStringLiteral("Controller read/debug connection requires device id 1..63"),
                    QStringLiteral("deviceId=%1").arg(deviceId));
    }

    auto modbus = std::make_unique<ModbusInterface>();
    const QVariantMap cfg = ControllerDebugProtocol::defaultRtuConfig(portName, deviceId);
    if (!modbus->open(cfg)) {
        const CommError err = modbus->lastError();
        m_lastError = err.isError()
                ? err
                : CommError(CommProtocolType::ModbusRTU, CommErrorCode::ConnectionFailed,
                            QStringLiteral("Controller Modbus RTU open failed"));
        emit errorOccurred(m_lastError);
        return false;
    }

    m_ownedModbus = std::move(modbus);
    m_ownedTransport = std::make_unique<ModbusControllerDebugTransport>(m_ownedModbus.get());
    m_transport = m_ownedTransport.get();
    clearError();
    return true;
}

bool ControllerDebugClient::openRtu(const QVariantMap& config, int deviceId)
{
    if (!ControllerDebugProtocol::canReadDeviceId(deviceId)) {
        return fail(CommErrorCode::InvalidAddress,
                    QStringLiteral("Controller read/debug connection requires device id 1..63"),
                    QStringLiteral("deviceId=%1").arg(deviceId));
    }

    QVariantMap cfg = config;
    cfg.insert(QStringLiteral("protocol"), QStringLiteral("MODBUS"));
    cfg.insert(QStringLiteral("mode"), QStringLiteral("RTU"));
    cfg.insert(QStringLiteral("type"), QStringLiteral("Master"));
    cfg.insert(QStringLiteral("address"), deviceId);

    auto modbus = std::make_unique<ModbusInterface>();
    if (!modbus->open(cfg)) {
        const CommError err = modbus->lastError();
        m_lastError = err.isError()
                ? err
                : CommError(CommProtocolType::ModbusRTU, CommErrorCode::ConnectionFailed,
                            QStringLiteral("Controller Modbus RTU open failed"));
        emit errorOccurred(m_lastError);
        return false;
    }

    m_ownedModbus = std::move(modbus);
    m_ownedTransport = std::make_unique<ModbusControllerDebugTransport>(m_ownedModbus.get());
    m_transport = m_ownedTransport.get();
    clearError();
    return true;
}

void ControllerDebugClient::close()
{
    if (m_ownedModbus) {
        m_ownedModbus->close();
    }
    m_ownedTransport.reset();
    m_ownedModbus.reset();
    m_transport = nullptr;
}

bool ControllerDebugClient::isConnected() const
{
    return m_transport && m_transport->isConnected();
}

bool ControllerDebugClient::readRegister(ControllerSystemRegister reg, quint16* value)
{
    if (!value) {
        return fail(CommErrorCode::InvalidParameter, QStringLiteral("Controller read target is null"));
    }
    if (!ensureTransport()) {
        return false;
    }

    const int addr = ControllerDebugProtocol::address(reg);
    if (!m_transport->readHoldingRegisters(addr, 1)) {
        return importTransportError(QStringLiteral("Controller register read failed"));
    }

    const QVector<quint16> values = m_transport->holdingRegisterValues(addr);
    if (values.isEmpty()) {
        return fail(CommErrorCode::InvalidResponse,
                    QStringLiteral("Controller register read returned no data"),
                    QStringLiteral("address=%1").arg(addr));
    }

    *value = values.first();
    clearError();
    return true;
}

bool ControllerDebugClient::writeRegister(ControllerSystemRegister reg, quint16 value)
{
    return writeRegisters(reg, {value});
}

bool ControllerDebugClient::writeRegisters(ControllerSystemRegister firstReg, const QVector<quint16>& values)
{
    if (!ensureTransport()) {
        return false;
    }
    if (values.isEmpty()) {
        return fail(CommErrorCode::InvalidParameter, QStringLiteral("Controller write values are empty"));
    }

    const int addr = ControllerDebugProtocol::address(firstReg);
    if (!m_transport->writeMultipleRegisters(addr, values)) {
        return importTransportError(QStringLiteral("Controller register write failed"));
    }

    clearError();
    return true;
}

bool ControllerDebugClient::readHoldingRegisters(int deviceId,
                                                 int address,
                                                 int count,
                                                 QVector<quint16>* values)
{
    if (!values) {
        return fail(CommErrorCode::InvalidParameter, QStringLiteral("Controller read target is null"));
    }
    values->clear();
    if (!ensureTransport()) {
        return false;
    }
    if (!ControllerDebugProtocol::canReadDeviceId(deviceId)) {
        return fail(CommErrorCode::InvalidAddress,
                    QStringLiteral("Controller read requires device id 1..63"),
                    QStringLiteral("deviceId=%1").arg(deviceId));
    }
    if (address < 0 || count <= 0) {
        return fail(CommErrorCode::InvalidParameter,
                    QStringLiteral("Controller register range is invalid"),
                    QStringLiteral("address=%1 count=%2").arg(address).arg(count));
    }

    const int previousDeviceId = m_transport->stationAddress();
    m_transport->setStationAddress(deviceId);
    const bool ok = m_transport->readHoldingRegisters(address, count);
    const QVector<quint16> readValues = ok ? m_transport->holdingRegisterValues(address) : QVector<quint16>();
    m_transport->setStationAddress(previousDeviceId);

    if (!ok) {
        return importTransportError(QStringLiteral("Controller holding register read failed"));
    }
    if (readValues.size() < count) {
        return fail(CommErrorCode::InvalidResponse,
                    QStringLiteral("Controller register read returned too few values"),
                    QStringLiteral("deviceId=%1 address=%2 expected=%3 got=%4")
                        .arg(deviceId).arg(address).arg(count).arg(readValues.size()));
    }

    *values = readValues.mid(0, count);
    clearError();
    return true;
}

bool ControllerDebugClient::writeHoldingRegisters(int deviceId,
                                                  int address,
                                                  const QVector<quint16>& values)
{
    if (!ensureTransport()) {
        return false;
    }
    if (!ControllerDebugProtocol::canReadDeviceId(deviceId)) {
        return fail(CommErrorCode::InvalidAddress,
                    QStringLiteral("Controller write requires device id 1..63"),
                    QStringLiteral("deviceId=%1").arg(deviceId));
    }
    if (address < 0 || values.isEmpty()) {
        return fail(CommErrorCode::InvalidParameter,
                    QStringLiteral("Controller register write range is invalid"),
                    QStringLiteral("address=%1 count=%2").arg(address).arg(values.size()));
    }

    const int previousDeviceId = m_transport->stationAddress();
    m_transport->setStationAddress(deviceId);
    const bool ok = m_transport->writeMultipleRegisters(address, values);
    m_transport->setStationAddress(previousDeviceId);

    if (!ok) {
        return importTransportError(QStringLiteral("Controller holding register write failed"));
    }

    clearError();
    return true;
}

bool ControllerDebugClient::readStatus(ControllerDebugStatus* status)
{
    if (!status) {
        return fail(CommErrorCode::InvalidParameter, QStringLiteral("Controller status target is null"));
    }

    ControllerDebugStatus next;
    if (!readRegister(ControllerSystemRegister::State, &next.state)) return false;
    if (!readRegister(ControllerSystemRegister::WorkMode, &next.workMode)) return false;
    if (!readRegister(ControllerSystemRegister::ComponentLine, &next.componentLine)) return false;
    if (!readRegister(ControllerSystemRegister::Reset, &next.reset)) return false;
    if (!readRegister(ControllerSystemRegister::Version, &next.version)) return false;

    *status = next;
    return true;
}

bool ControllerDebugClient::readState(quint16* state)
{
    return readRegister(ControllerSystemRegister::State, state);
}

bool ControllerDebugClient::readVersion(quint16* version)
{
    return readRegister(ControllerSystemRegister::Version, version);
}

bool ControllerDebugClient::pause()
{
    return writeRegister(ControllerSystemRegister::DebugPause, 1);
}

bool ControllerDebugClient::resume()
{
    return writeRegister(ControllerSystemRegister::DebugPause, 0);
}

bool ControllerDebugClient::step()
{
    return writeRegister(ControllerSystemRegister::DebugStep, 1);
}

bool ControllerDebugClient::runToCursor(quint16 line)
{
    return writeRegister(ControllerSystemRegister::RunToCursor, line);
}

bool ControllerDebugClient::setBreakpoints(quint16 breakPoint1, quint16 breakPoint2)
{
    return writeRegisters(ControllerSystemRegister::BreakPoint1,
                          ControllerDebugProtocol::breakpointValues(breakPoint1, breakPoint2));
}

bool ControllerDebugClient::reset(ControllerResetCode code)
{
    return writeRegister(ControllerSystemRegister::Reset, static_cast<quint16>(code));
}

bool ControllerDebugClient::saveConfiguration()
{
    return writeRegister(ControllerSystemRegister::ConfSave, 1);
}

bool ControllerDebugClient::writeReadPassword(quint16 password)
{
    return writeRegister(ControllerSystemRegister::ReadPassword, password);
}

bool ControllerDebugClient::ensureTransport()
{
    if (!m_transport) {
        return fail(CommErrorCode::InvalidConfig, QStringLiteral("Controller debug transport is not configured"));
    }
    if (!m_transport->isConnected()) {
        return fail(CommErrorCode::ConnectionLost, QStringLiteral("Controller debug transport is not connected"));
    }
    return true;
}

bool ControllerDebugClient::fail(CommErrorCode code, const QString& message, const QString& details)
{
    m_lastError = CommError(CommProtocolType::ModbusRTU, code, message, details);
    emit errorOccurred(m_lastError);
    return false;
}

bool ControllerDebugClient::importTransportError(const QString& fallbackMessage)
{
    const CommError err = m_transport ? m_transport->lastError() : CommError();
    m_lastError = err.isError()
            ? err
            : CommError(CommProtocolType::ModbusRTU, CommErrorCode::UnknownError, fallbackMessage);
    emit errorOccurred(m_lastError);
    return false;
}
