// 文件：src/communication/ModbusInterface.h
// Modbus 通信接口（基于 QtSerialBus，RTU/TCP 主站）

#ifndef MODBUSINTERFACE_H
#define MODBUSINTERFACE_H

#include "ICommInterface.h"
#include "CommTypes.h"

#include <QMap>
#include <QMutex>
#include <QPointer>
#include <QVector>

#include <QtSerialBus/QModbusDataUnit>

QT_BEGIN_NAMESPACE
class QModbusClient;
class QModbusReply;
QT_END_NAMESPACE

/**
 * @brief Modbus 通信接口
 *
 * 目标：工业现场可用的 Modbus RTU/TCP 主站
 * - 优先使用 QtSerialBus 的 QModbusRtuSerialMaster / QModbusRtuSerialClient
 * - 支持 Fun3 / Fun15 / Fun16
 * - 支持 responseTimeout + retryCount（来自 ModbusConfig）
 * - 错误统一映射到 CommErrorCode，并通过 ICommInterface::reportError 上报
 */
class ModbusInterface : public ICommInterface
{
    Q_OBJECT
public:
    using ModbusMode = ModbusConfig::Mode;
    using StationType = ModbusConfig::StationType;

    explicit ModbusInterface(QObject* parent = nullptr);
    ~ModbusInterface() override;

    // ========================================================================
    // ICommInterface
    // ========================================================================
    bool open(const QVariantMap& config) override;
    void close() override;

    int send(const QByteArray& data) override;                 // 不建议：保留兼容
    QByteArray receive(int timeout_ms = 1000) override;        // 不建议：保留兼容
    bool isConnected() const override;

    // ========================================================================
    // Modbus Master APIs
    // ========================================================================
    bool open(const ModbusConfig& config);
    ModbusConfig currentConfig() const { return m_config; }

    // Fun3
    bool readHoldingRegisters(int address, int count);
    bool readInputRegisters(int address, int count);
    bool readCoils(int address, int count);
    bool readDiscreteInputs(int address, int count);

    // Fun16 / Fun15（验收要求）
    bool writeSingleRegister(int address, quint16 value);
    bool writeMultipleRegisters(int address, const QVector<quint16>& values);
    bool writeSingleCoil(int address, bool value);
    bool writeMultipleCoils(int address, const QVector<bool>& values);

    void setStationAddress(int address) { m_config.stationAddress = address; }
    int stationAddress() const { return m_config.stationAddress; }

    void setModbusMode(ModbusMode mode) { m_config.mode = mode; }
    ModbusMode modbusMode() const { return m_config.mode; }

    void setStationType(StationType type) { m_config.stationType = type; }
    StationType stationType() const { return m_config.stationType; }

    void setResponseTimeout(int ms) { m_config.responseTimeout = ms; applyTimeoutRetry(); }
    int responseTimeout() const { return m_config.responseTimeout; }

    void setRetryCount(int n) { m_config.retryCount = n; applyTimeoutRetry(); }
    int retryCount() const { return m_config.retryCount; }

    // 供桥接层/上层做“语义化报错”的入口（reportError 在基类中是 protected）
    void reportCommError(CommErrorCode code, const QString& message, const QString& details = QString()) { reportError(code, message, details); }

    // 简单缓存访问（便于上层同步读取）
    QMap<int, QVector<quint16>> holdingRegisters() const;
    QMap<int, QVector<bool>> coils() const;

signals:
    void registerDataReceived(int address, const QVector<quint16>& values);
    void coilDataReceived(int address, const QVector<bool>& values);
    void writeCompleted(int address, int count);

private slots:
    void onStateChanged(int state);
    void onDeviceError(int error);

private:
    bool createClientIfNeeded();
    bool configureRtuClient();
    bool configureTcpClient();
    void applyTimeoutRetry();

    CommErrorCode mapQtErrorToCommError(int qtError) const;
    CommErrorCode mapReplyErrorToCommError(int replyError) const;

    // 同步等待 reply 完成（为了对接现有同步 bool API）
    bool waitForReply(QModbusReply* reply, QString& outErr);

    bool readDataUnit(const QModbusDataUnit& unit, int serverAddress);
    bool writeDataUnit(const QModbusDataUnit& unit, int serverAddress);

private:
    ModbusConfig m_config;

    QPointer<QModbusClient> m_client;
    bool m_connected = false;

    // 与旧接口兼容：保留 buffer（一般不会用到）
    QByteArray m_receiveBuffer;
    mutable QMutex m_bufferMutex;

    // 数据缓存
    QMap<int, QVector<quint16>> m_holdingRegisters;
    QMap<int, QVector<quint16>> m_inputRegisters;
    QMap<int, QVector<bool>> m_coils;
    QMap<int, QVector<bool>> m_discreteInputs;
    mutable QMutex m_dataMutex;

    static constexpr int MAX_REGISTERS = 125; // Modbus 标准限制
    static constexpr int MAX_COILS = 2000;
};

#endif // MODBUSINTERFACE_H
