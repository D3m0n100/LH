// File: src/communication/ControllerDebugClient.h
// Controller debug client over the documented Modbus subset.

#ifndef CONTROLLERDEBUGCLIENT_H
#define CONTROLLERDEBUGCLIENT_H

#include "CommTypes.h"
#include "ControllerDebugProtocol.h"

#include <QObject>
#include <QVector>

#include <memory>

class ModbusInterface;

class IControllerDebugTransport
{
public:
    virtual ~IControllerDebugTransport() = default;

    virtual bool isConnected() const = 0;
    virtual int stationAddress() const = 0;
    virtual void setStationAddress(int deviceId) = 0;
    virtual bool readHoldingRegisters(int address, int count) = 0;
    virtual QVector<quint16> holdingRegisterValues(int address) const = 0;
    virtual bool writeMultipleRegisters(int address, const QVector<quint16>& values) = 0;
    virtual CommError lastError() const = 0;
};

class ModbusControllerDebugTransport final : public IControllerDebugTransport
{
public:
    explicit ModbusControllerDebugTransport(ModbusInterface* modbus);

    bool isConnected() const override;
    int stationAddress() const override;
    void setStationAddress(int deviceId) override;
    bool readHoldingRegisters(int address, int count) override;
    QVector<quint16> holdingRegisterValues(int address) const override;
    bool writeMultipleRegisters(int address, const QVector<quint16>& values) override;
    CommError lastError() const override;

private:
    ModbusInterface* m_modbus = nullptr;
};

class ControllerDebugClient : public QObject
{
    Q_OBJECT
public:
    explicit ControllerDebugClient(QObject* parent = nullptr);
    explicit ControllerDebugClient(IControllerDebugTransport* transport, QObject* parent = nullptr);
    ~ControllerDebugClient() override;

    void setTransport(IControllerDebugTransport* transport);
    void attachModbus(ModbusInterface* modbus);

    bool openRtu(const QString& portName, int deviceId = 1);
    bool openRtu(const QVariantMap& config, int deviceId);
    void close();
    bool isConnected() const;

    CommError lastError() const { return m_lastError; }
    void clearError() { m_lastError = CommError(); }

    bool readRegister(ControllerSystemRegister reg, quint16* value);
    bool writeRegister(ControllerSystemRegister reg, quint16 value);
    bool writeRegisters(ControllerSystemRegister firstReg, const QVector<quint16>& values);
    bool readHoldingRegisters(int deviceId, int address, int count, QVector<quint16>* values);
    bool writeHoldingRegisters(int deviceId, int address, const QVector<quint16>& values);

    bool readStatus(ControllerDebugStatus* status);
    bool readState(quint16* state);
    bool readVersion(quint16* version);

    bool pause();
    bool resume();
    bool step();
    bool runToCursor(quint16 line);
    bool setBreakpoints(quint16 breakPoint1, quint16 breakPoint2);
    bool reset(ControllerResetCode code = ControllerResetCode::MonitorCommand);
    bool saveConfiguration();
    bool writeReadPassword(quint16 password);

signals:
    void errorOccurred(const CommError& error);

private:
    bool ensureTransport();
    bool fail(CommErrorCode code, const QString& message, const QString& details = QString());
    bool importTransportError(const QString& fallbackMessage);

    IControllerDebugTransport* m_transport = nullptr; // Not owned unless it points to m_ownedTransport.
    std::unique_ptr<ModbusInterface> m_ownedModbus;
    std::unique_ptr<ModbusControllerDebugTransport> m_ownedTransport;
    CommError m_lastError;
};

#endif // CONTROLLERDEBUGCLIENT_H
