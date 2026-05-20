// File: src/communication/ControllerBridge.h
// Bridge layer: Modbus master on PC -> controller -> target device.

#ifndef CONTROLLERBRIDGE_H
#define CONTROLLERBRIDGE_H

#include <QObject>
#include <QVariantMap>
#include <QByteArray>
#include <QVector>

#include <atomic>

#include "CommTypes.h"

class ModbusInterface;
class DownloadProfile;

class ControllerBridge : public QObject
{
    Q_OBJECT
public:
    explicit ControllerBridge(ModbusInterface* modbus, QObject* parent = nullptr);

    void setBridgeConfig(const QVariantMap& cfg);
    QVariantMap bridgeConfig() const { return m_cfg; }

    bool handshake();
    bool probeTarget();
    void requestAbort();

    bool download(const DownloadProfile& profile, const QByteArray& payload);

    bool controllerOnline() const { return m_controllerOnline; }
    bool targetOnline() const { return m_targetOnline; }

signals:
    void controllerOnlineChanged(bool online);
    void targetOnlineChanged(bool online);
    void downloadProgress(int percent, int sentBytes, int totalBytes, int packetIndex, int packetCount);
    void logLine(const QString& line);

private:
    QVariantMap subMap(const QString& key) const;
    static bool parseExpected(const QVariant& v, int count, QVector<quint16>& out);
    static quint16 calcCrc16Modbus(const QByteArray& bytes);

    enum class Layer { Controller, Target };
    Layer parseLayer(const QVariantMap& params) const;
    int resolveSlaveId(Layer layer, const QVariantMap& params) const;

    enum class AddressingMode {
        TargetAsSlaveId,
        ControllerOnlyWithTargetSelectReg
    };
    AddressingMode addressingMode() const;
    bool selectTargetIfNeeded(int targetId);

    bool stepEnterOrFinalize(const QVariantMap& params);
    bool stepPoll(const QVariantMap& params);
    bool stepSendChunk(const QVariantMap& params, const QByteArray& payload);
    bool stepQueryResult(const QVariantMap& params);
    bool isAbortRequested() const;

private:
    ModbusInterface* m_modbus = nullptr; // Not owned.
    QVariantMap m_cfg;

    bool m_controllerOnline = false;
    bool m_targetOnline = false;
    std::atomic_bool m_abortRequested{false};
};

#endif // CONTROLLERBRIDGE_H
