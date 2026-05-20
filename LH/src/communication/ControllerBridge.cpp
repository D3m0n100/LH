// File: src/communication/ControllerBridge.cpp

#include "ControllerBridge.h"

#include "DownloadProfile.h"
#include "ModbusInterface.h"

#include <QElapsedTimer>
#include <QThread>
#include <QVariantList>

namespace {
QVector<quint16> bytesToWordsBE(const QByteArray& chunk)
{
    QVector<quint16> out;
    out.reserve((chunk.size() + 1) / 2);
    for (int i = 0; i < chunk.size(); i += 2) {
        const quint8 hi = static_cast<quint8>(chunk.at(i));
        const quint8 lo = (i + 1 < chunk.size()) ? static_cast<quint8>(chunk.at(i + 1)) : 0;
        out.push_back((static_cast<quint16>(hi) << 8) | static_cast<quint16>(lo));
    }
    return out;
}

QVector<quint16> bytesToWordsLE(const QByteArray& chunk)
{
    QVector<quint16> out;
    out.reserve((chunk.size() + 1) / 2);
    for (int i = 0; i < chunk.size(); i += 2) {
        const quint8 lo = static_cast<quint8>(chunk.at(i));
        const quint8 hi = (i + 1 < chunk.size()) ? static_cast<quint8>(chunk.at(i + 1)) : 0;
        out.push_back((static_cast<quint16>(hi) << 8) | static_cast<quint16>(lo));
    }
    return out;
}
} // namespace

ControllerBridge::ControllerBridge(ModbusInterface* modbus, QObject* parent)
    : QObject(parent)
    , m_modbus(modbus)
{
}

void ControllerBridge::setBridgeConfig(const QVariantMap& cfg)
{
    m_cfg = cfg;
}

QVariantMap ControllerBridge::subMap(const QString& key) const
{
    QVariant v = m_cfg;
    const QStringList parts = key.split('.', Qt::SkipEmptyParts);
    for (const auto& p : parts) {
        v = v.toMap().value(p);
    }
    return v.toMap();
}

bool ControllerBridge::parseExpected(const QVariant& v, int count, QVector<quint16>& out)
{
    out.clear();
    if (!v.isValid() || v.isNull()) {
        return false;
    }

    if (v.canConvert<qulonglong>()) {
        out = QVector<quint16>(count, static_cast<quint16>(v.toULongLong()));
        return true;
    }

    if (v.type() == QVariant::List) {
        const QVariantList list = v.toList();
        if (list.size() < count) {
            return false;
        }
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            out.push_back(static_cast<quint16>(list[i].toUInt()));
        }
        return true;
    }
    return false;
}

quint16 ControllerBridge::calcCrc16Modbus(const QByteArray& bytes)
{
    quint16 crc = 0xFFFF;
    for (char c : bytes) {
        crc ^= static_cast<quint8>(c);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) {
                crc = static_cast<quint16>((crc >> 1) ^ 0xA001);
            } else {
                crc = static_cast<quint16>(crc >> 1);
            }
        }
    }
    return crc;
}

void ControllerBridge::requestAbort()
{
    m_abortRequested = true;
}

bool ControllerBridge::isAbortRequested() const
{
    return m_abortRequested.load();
}

bool ControllerBridge::handshake()
{
    if (!m_modbus) {
        return false;
    }

    const bool enableHandshake = m_cfg.value("enableHandshake", true).toBool();
    if (!enableHandshake) {
        if (!m_controllerOnline) {
            m_controllerOnline = true;
            emit controllerOnlineChanged(true);
        }
        return true;
    }

    if (!m_modbus->isConnected()) {
        if (m_controllerOnline) {
            m_controllerOnline = false;
            emit controllerOnlineChanged(false);
        }
        m_modbus->reportCommError(CommErrorCode::ConnectionLost, "Serial not connected");
        return false;
    }

    const QVariantMap hs = subMap("handshake");
    const int slaveId = hs.value("slaveId", m_modbus->stationAddress()).toInt();
    const int addr = hs.value("address", 0).toInt();
    const int count = hs.value("count", 1).toInt();

    const int oldId = m_modbus->stationAddress();
    m_modbus->setStationAddress(slaveId);
    const bool ok = m_modbus->readHoldingRegisters(addr, count);
    m_modbus->setStationAddress(oldId);

    if (!ok) {
        if (m_controllerOnline) {
            m_controllerOnline = false;
            emit controllerOnlineChanged(false);
        }
        m_modbus->reportCommError(CommErrorCode::DeviceNotFound,
                                  "Controller handshake failed",
                                  QString("slaveId=%1 addr=%2 count=%3").arg(slaveId).arg(addr).arg(count));
        return false;
    }

    if (!m_controllerOnline) {
        m_controllerOnline = true;
        emit controllerOnlineChanged(true);
    }
    return true;
}

bool ControllerBridge::probeTarget()
{
    if (!m_modbus) {
        return false;
    }

    const bool enableProbe = m_cfg.value("enableTargetProbe", true).toBool();
    if (!enableProbe) {
        if (!m_targetOnline) {
            m_targetOnline = true;
            emit targetOnlineChanged(true);
        }
        return true;
    }

    if (!m_controllerOnline && !handshake()) {
        return false;
    }

    const QVariantMap tp = subMap("targetProbe");
    const int addr = tp.value("address", 0).toInt();
    const int count = tp.value("count", 1).toInt();
    const int defaultTarget = m_cfg.value("target").toMap().value("deviceId", 1).toInt();
    const int slaveId = tp.value("slaveId", defaultTarget).toInt();
    const QVariant expectedVar = tp.value("expected");

    const int oldId = m_modbus->stationAddress();
    m_modbus->setStationAddress(slaveId);
    const bool ok = m_modbus->readHoldingRegisters(addr, count);
    m_modbus->setStationAddress(oldId);
    if (!ok) {
        if (m_targetOnline) {
            m_targetOnline = false;
            emit targetOnlineChanged(false);
        }
        m_modbus->reportCommError(CommErrorCode::DeviceNotFound,
                                  "Target probe failed",
                                  QString("slaveId=%1 addr=%2 count=%3").arg(slaveId).arg(addr).arg(count));
        return false;
    }

    QVector<quint16> expected;
    const bool hasExpected = parseExpected(expectedVar, count, expected);
    if (hasExpected) {
        const auto got = m_modbus->holdingRegisters().value(addr);
        if (got.size() < count) {
            m_modbus->reportCommError(CommErrorCode::InvalidResponse,
                                      "Target probe result length invalid",
                                      QString("addr=%1 expectedCount=%2 got=%3").arg(addr).arg(count).arg(got.size()));
            return false;
        }
        for (int i = 0; i < count; ++i) {
            if (got[i] != expected[i]) {
                m_modbus->reportCommError(CommErrorCode::InvalidResponse,
                                          "Target probe expected mismatch",
                                          QString("addr=%1 idx=%2 expected=%3 got=%4")
                                              .arg(addr).arg(i).arg(expected[i]).arg(got[i]));
                return false;
            }
        }
    }

    if (!m_targetOnline) {
        m_targetOnline = true;
        emit targetOnlineChanged(true);
    }
    return true;
}

ControllerBridge::Layer ControllerBridge::parseLayer(const QVariantMap& params) const
{
    const QString s = params.value("layer", "Target").toString().trimmed().toLower();
    if (s == "controller" || s == "ctrl") {
        return Layer::Controller;
    }
    return Layer::Target;
}

int ControllerBridge::resolveSlaveId(Layer layer, const QVariantMap& params) const
{
    if (params.contains("slaveId")) {
        return params.value("slaveId").toInt();
    }
    const QVariantMap ctrl = m_cfg.value("controller").toMap();
    const QVariantMap tgt = m_cfg.value("target").toMap();
    if (layer == Layer::Controller) {
        return ctrl.value("slaveId", 1).toInt();
    }
    return tgt.value("deviceId", 1).toInt();
}

ControllerBridge::AddressingMode ControllerBridge::addressingMode() const
{
    const QVariantMap addr = m_cfg.value("addressing").toMap();
    const QString mode = addr.value("mode", "TargetAsSlaveId").toString();
    if (mode.compare("ControllerOnlyWithTargetSelectReg", Qt::CaseInsensitive) == 0) {
        return AddressingMode::ControllerOnlyWithTargetSelectReg;
    }
    return AddressingMode::TargetAsSlaveId;
}

bool ControllerBridge::selectTargetIfNeeded(int targetId)
{
    if (!m_modbus) {
        return false;
    }
    if (addressingMode() != AddressingMode::ControllerOnlyWithTargetSelectReg) {
        return true;
    }

    const QVariantMap sel = m_cfg.value("addressing").toMap().value("targetSelect").toMap();
    const int selectReg = sel.value("selectReg", 0).toInt();
    if (selectReg <= 0) {
        m_modbus->reportCommError(CommErrorCode::InvalidConfig,
                                  "targetSelect.selectReg is required");
        return false;
    }

    const int controllerSlave = m_cfg.value("controller").toMap().value("slaveId", 1).toInt();
    const int oldId = m_modbus->stationAddress();
    m_modbus->setStationAddress(controllerSlave);
    const bool ok = m_modbus->writeMultipleRegisters(selectReg, { static_cast<quint16>(targetId) });
    m_modbus->setStationAddress(oldId);
    return ok;
}

bool ControllerBridge::stepEnterOrFinalize(const QVariantMap& params)
{
    if (!m_modbus) {
        return false;
    }
    if (isAbortRequested()) {
        m_modbus->reportCommError(CommErrorCode::OperationCancelled, "Download cancelled");
        return false;
    }

    const Layer layer = parseLayer(params);
    const int slaveId = resolveSlaveId(layer, params);
    const QString op = params.value("op", "writeRegs").toString();
    const int addr = params.value("address").toInt();
    const bool needResp = params.value("needResponse", true).toBool();

    if (layer == Layer::Target) {
        const int targetId = m_cfg.value("target").toMap().value("deviceId", slaveId).toInt();
        if (!selectTargetIfNeeded(targetId)) {
            return false;
        }
    }

    const int oldId = m_modbus->stationAddress();
    m_modbus->setStationAddress(slaveId);

    bool ok = false;
    if (op.compare("writecoils", Qt::CaseInsensitive) == 0) {
        const QVariantList vs = params.value("values").toList();
        QVector<bool> coils;
        coils.reserve(vs.size());
        for (const auto& v : vs) {
            coils.push_back(v.toInt() != 0);
        }
        ok = m_modbus->writeMultipleCoils(addr, coils);
    } else {
        const QVariantList vs = params.value("values").toList();
        QVector<quint16> regs;
        regs.reserve(vs.size());
        for (const auto& v : vs) {
            regs.push_back(static_cast<quint16>(v.toUInt()));
        }
        ok = m_modbus->writeMultipleRegisters(addr, regs);
        if (!ok && slaveId == 0 && !needResp) {
            ok = true;
        }
    }

    m_modbus->setStationAddress(oldId);
    return ok;
}

bool ControllerBridge::stepPoll(const QVariantMap& params)
{
    if (!m_modbus) {
        return false;
    }

    const Layer layer = parseLayer(params);
    const int slaveId = resolveSlaveId(layer, params);
    const int addr = params.value("address").toInt();
    const int count = params.value("count", 1).toInt();
    const int timeoutMs = params.value("timeoutMs", 2000).toInt();
    const int pollMs = params.value("pollIntervalMs", 100).toInt();

    QVector<quint16> expected;
    if (!parseExpected(params.value("expected"), count, expected)) {
        m_modbus->reportCommError(CommErrorCode::InvalidConfig,
                                  "Poll expected is invalid",
                                  QString("addr=%1 count=%2").arg(addr).arg(count));
        return false;
    }

    if (layer == Layer::Target) {
        const int targetId = m_cfg.value("target").toMap().value("deviceId", slaveId).toInt();
        if (!selectTargetIfNeeded(targetId)) {
            return false;
        }
    }

    const int oldId = m_modbus->stationAddress();
    m_modbus->setStationAddress(slaveId);

    QElapsedTimer t;
    t.start();
    while (t.elapsed() < timeoutMs) {
        if (isAbortRequested()) {
            m_modbus->setStationAddress(oldId);
            m_modbus->reportCommError(CommErrorCode::OperationCancelled, "Download cancelled during poll");
            return false;
        }
        if (m_modbus->readHoldingRegisters(addr, count)) {
            const auto got = m_modbus->holdingRegisters().value(addr);
            if (got.size() >= count) {
                bool match = true;
                for (int i = 0; i < count; ++i) {
                    if (got.value(i) != expected.value(i)) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    m_modbus->setStationAddress(oldId);
                    return true;
                }
            }
        }
        QThread::msleep(static_cast<unsigned long>(pollMs));
    }

    m_modbus->setStationAddress(oldId);
    m_modbus->reportCommError(CommErrorCode::ConnectionTimeout,
                              "Poll timeout",
                              QString("slave=%1 addr=%2").arg(slaveId).arg(addr));
    return false;
}

bool ControllerBridge::stepSendChunk(const QVariantMap& params, const QByteArray& payload)
{
    if (!m_modbus) {
        return false;
    }

    const Layer layer = parseLayer(params);
    const int slaveId = resolveSlaveId(layer, params);
    const int dataAddr = params.value("dataAddress").toInt();
    const int chunkWords = params.value("chunkWords", 60).toInt();
    const bool needResp = params.value("needResponse", true).toBool();
    const QString byteOrder = params.value("byteOrder",
                                           m_cfg.value("transfer").toMap().value("byteOrder", "BigEndian").toString())
                                  .toString();
    const bool le = (byteOrder.compare("LittleEndian", Qt::CaseInsensitive) == 0);

    const int packetIndexAddr = params.value("packetIndexAddress", -1).toInt();
    const int packetLenAddr = params.value("packetLengthAddress", -1).toInt();
    const int packetCrcAddr = params.value("packetCrcAddress", -1).toInt();
    const int packetOffsetAddr = params.value("packetOffsetAddress", -1).toInt();
    const int packetIndexBase = params.value("packetIndexBase", 0).toInt();

    if (dataAddr <= 0 || chunkWords <= 0) {
        m_modbus->reportCommError(CommErrorCode::InvalidConfig,
                                  "SendChunk config invalid: dataAddress/chunkWords",
                                  QString("dataAddress=%1 chunkWords=%2").arg(dataAddr).arg(chunkWords));
        return false;
    }

    if (layer == Layer::Target) {
        const int targetId = m_cfg.value("target").toMap().value("deviceId", slaveId).toInt();
        if (!selectTargetIfNeeded(targetId)) {
            return false;
        }
    }

    const int oldId = m_modbus->stationAddress();
    m_modbus->setStationAddress(slaveId);

    const int bytesPerChunk = chunkWords * 2;
    const int totalBytes = payload.size();
    const int packetCount = (totalBytes + bytesPerChunk - 1) / bytesPerChunk;
    int sent = 0;

    for (int i = 0; i < packetCount; ++i) {
        if (isAbortRequested()) {
            m_modbus->setStationAddress(oldId);
            m_modbus->reportCommError(CommErrorCode::OperationCancelled, "Download cancelled during transfer");
            return false;
        }

        const int off = i * bytesPerChunk;
        const int len = qMin(bytesPerChunk, totalBytes - off);
        const QByteArray chunk = payload.mid(off, len);
        const QVector<quint16> words = le ? bytesToWordsLE(chunk) : bytesToWordsBE(chunk);
        const quint16 chunkCrc = calcCrc16Modbus(chunk);

        if (packetIndexAddr > 0) {
            const bool ok = m_modbus->writeMultipleRegisters(packetIndexAddr, { static_cast<quint16>(packetIndexBase + i) });
            if (!ok) {
                m_modbus->setStationAddress(oldId);
                m_modbus->reportCommError(CommErrorCode::SendFailed, "Write packetIndex failed");
                return false;
            }
        }
        if (packetLenAddr > 0) {
            const bool ok = m_modbus->writeMultipleRegisters(packetLenAddr, { static_cast<quint16>(len) });
            if (!ok) {
                m_modbus->setStationAddress(oldId);
                m_modbus->reportCommError(CommErrorCode::SendFailed, "Write packetLength failed");
                return false;
            }
        }
        if (packetCrcAddr > 0) {
            const bool ok = m_modbus->writeMultipleRegisters(packetCrcAddr, { chunkCrc });
            if (!ok) {
                m_modbus->setStationAddress(oldId);
                m_modbus->reportCommError(CommErrorCode::SendFailed, "Write packetCrc failed");
                return false;
            }
        }
        if (packetOffsetAddr > 0) {
            const bool ok = m_modbus->writeMultipleRegisters(packetOffsetAddr, { static_cast<quint16>(off) });
            if (!ok) {
                m_modbus->setStationAddress(oldId);
                m_modbus->reportCommError(CommErrorCode::SendFailed, "Write packetOffset failed");
                return false;
            }
        }

        bool ok = m_modbus->writeMultipleRegisters(dataAddr, words);
        if (!ok && slaveId == 0 && !needResp) {
            ok = true;
        }
        if (!ok) {
            m_modbus->setStationAddress(oldId);
            m_modbus->reportCommError(CommErrorCode::ConnectionTimeout,
                                      "Chunk write failed",
                                      QString("slave=%1 pkt=%2/%3").arg(slaveId).arg(i + 1).arg(packetCount));
            return false;
        }

        sent += len;
        const int percent = totalBytes > 0 ? (sent * 100 / totalBytes) : 100;
        emit downloadProgress(percent, sent, totalBytes, i + 1, packetCount);
    }

    m_modbus->setStationAddress(oldId);
    return true;
}

bool ControllerBridge::stepQueryResult(const QVariantMap& params)
{
    if (!m_modbus) {
        return false;
    }

    const Layer layer = parseLayer(params);
    const int slaveId = resolveSlaveId(layer, params);
    const int addr = params.value("address").toInt();
    const int count = params.value("count", 1).toInt();

    if (layer == Layer::Target) {
        const int targetId = m_cfg.value("target").toMap().value("deviceId", slaveId).toInt();
        if (!selectTargetIfNeeded(targetId)) {
            return false;
        }
    }

    const int oldId = m_modbus->stationAddress();
    m_modbus->setStationAddress(slaveId);
    if (!m_modbus->readHoldingRegisters(addr, count)) {
        m_modbus->setStationAddress(oldId);
        m_modbus->reportCommError(CommErrorCode::ConnectionTimeout,
                                  "QueryResult read failed",
                                  QString("slave=%1 addr=%2 count=%3").arg(slaveId).arg(addr).arg(count));
        return false;
    }

    const auto got = m_modbus->holdingRegisters().value(addr);
    QStringList parts;
    for (quint16 v : got) {
        parts << QString::number(v);
    }
    emit logLine(QString("[QRY] values=%1").arg(parts.join(",")));

    QVector<quint16> expected;
    if (parseExpected(params.value("expected"), count, expected)) {
        if (got.size() < count) {
            m_modbus->setStationAddress(oldId);
            m_modbus->reportCommError(CommErrorCode::InvalidResponse,
                                      "QueryResult size mismatch",
                                      QString("expected=%1 got=%2").arg(count).arg(got.size()));
            return false;
        }
        for (int i = 0; i < count; ++i) {
            if (got[i] != expected[i]) {
                m_modbus->setStationAddress(oldId);
                m_modbus->reportCommError(CommErrorCode::ProtocolError,
                                          "QueryResult expected mismatch",
                                          QString("idx=%1 expected=%2 got=%3").arg(i).arg(expected[i]).arg(got[i]));
                return false;
            }
        }
    }

    m_modbus->setStationAddress(oldId);
    return true;
}

bool ControllerBridge::download(const DownloadProfile& profile, const QByteArray& payload)
{
    if (!m_modbus) {
        return false;
    }

    m_abortRequested = false;

    if (!handshake()) {
        return false;
    }
    if (!probeTarget()) {
        return false;
    }

    emit logLine(QString("[DL] start, steps=%1 payload=%2 bytes").arg(profile.steps.size()).arg(payload.size()));
    emit downloadProgress(0, 0, payload.size(), 0, 0);

    for (int i = 0; i < profile.steps.size(); ++i) {
        if (isAbortRequested()) {
            m_modbus->reportCommError(CommErrorCode::OperationCancelled, "Download cancelled");
            return false;
        }

        const auto& step = profile.steps[i];
        const QVariantMap params = step.params;
        bool ok = false;
        switch (step.type) {
        case DownloadProfile::StepType::Enter:
        case DownloadProfile::StepType::Finalize:
            ok = stepEnterOrFinalize(params);
            break;
        case DownloadProfile::StepType::Poll:
            ok = stepPoll(params);
            break;
        case DownloadProfile::StepType::SendChunk:
            ok = stepSendChunk(params, payload);
            break;
        case DownloadProfile::StepType::QueryResult:
            ok = stepQueryResult(params);
            break;
        default:
            m_modbus->reportCommError(CommErrorCode::NotImplemented,
                                      "Download step type is not implemented",
                                      QString("stepIndex=%1").arg(i));
            return false;
        }

        if (!ok) {
            return false;
        }
        emit logLine(QString("[DL] step %1/%2 ok").arg(i + 1).arg(profile.steps.size()));
    }

    emit downloadProgress(100, payload.size(), payload.size(), 0, 0);
    emit logLine("[DL] finished");
    return true;
}
