#include "ClassicOpcPollWorker.h"
#include "Communication.h"
#include <QDeadlineTimer>
#include <QScopedValueRollback>
#include <QScopeGuard>
#include <QTimer>
#include <QUuid>
#include <algorithm>

QList<ClassicReadBatch> planClassicReads(QList<ClassicPollPoint> points, int maxRegisters)
{
    std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) {
        if (a.unit != b.unit) return a.unit < b.unit;
        if (a.area != b.area) return a.area < b.area;
        return a.address < b.address;
    });
    QList<ClassicReadBatch> batches;
    for (const auto& point : points) {
        const int limit = point.area == "coil" || point.area == "discrete" ? 2000 : qBound(1, maxRegisters, ModbusLimits::ReadRegisters);
        if (!point.error.isEmpty() || point.unit < 1 || point.unit > 247 ||
                (point.area != "coil" && point.area != "discrete" && point.area != "input" && point.area != "holding") ||
                point.width() < 1 || point.width() > limit ||
                point.address < 0 || point.address + point.width() > 65536) continue;
        if (!batches.isEmpty()) {
            auto& batch = batches.last();
            const int end = qMax(batch.address + batch.width, point.address + point.width());
            if (batch.unit == point.unit && batch.area == point.area &&
                    point.address <= batch.address + batch.width && end - batch.address <= limit) {
                batch.width = end - batch.address;
                batch.points.append(point);
                continue;
            }
        }
        batches.append({point.area, point.unit, point.address, point.width(), {point}});
    }
    return batches;
}

ClassicOpcPollWorker::ClassicOpcPollWorker()
    : m_modbus(this), m_owner("classic-opc-" + QUuid::createUuid().toString(QUuid::WithoutBraces)) {}
ClassicOpcPollWorker::~ClassicOpcPollWorker() { closeTransport(); }
void ClassicOpcPollWorker::closeTransport()
{
    m_modbus.close();
    Communication::releaseRtuPort(m_claimedPort, m_owner);
    m_claimedPort.clear();
    m_openConfig.clear();
}
void ClassicOpcPollWorker::closeWhenIdle(std::shared_ptr<std::atomic_bool> session)
{
    if (m_session && m_session != session) return;
    if (m_busy) {
        QTimer::singleShot(10, this, [this, session] { closeWhenIdle(session); });
        return;
    }
    closeTransport();
}
void ClassicOpcPollWorker::poll(ModbusConfig config, QList<ClassicPollPoint> points, int budgetMs, int maxRegisters,
    std::shared_ptr<std::atomic_bool> cancelled,
    std::function<void(QList<RuntimePointValue>, QString, bool)> completed)
{
    if (m_busy) {
        QTimer::singleShot(10, this, [this, config, points, budgetMs, maxRegisters, cancelled, completed] {
            poll(config, points, budgetMs, maxRegisters, cancelled, completed);
        });
        return;
    }
    QScopedValueRollback<bool> active(m_busy, true);
    auto resetBudget = qScopeGuard([this] { m_modbus.setRequestBudget(-1, nullptr); });
    QList<RuntimePointValue> values;
    QString error;
    QDeadlineTimer deadline(qMax(1, budgetMs));
    auto stopped = [&] { return (cancelled && cancelled->load()) || deadline.hasExpired(); };
    if (stopped()) { completed({}, QStringLiteral("Classic poll cancelled"), m_modbus.isConnected()); return; }
    m_session = cancelled;
    config.responseTimeout = qMin(config.responseTimeout, qMax(50, budgetMs));
    if (config.toVariantMap() != m_openConfig) closeTransport();
    if (!m_modbus.isConnected()) {
        if (!Communication::tryClaimRtuPort(config.portName, m_owner)) {
            completed({}, QStringLiteral("Classic OPC RTU port is owned by another session"), false); return;
        }
        m_claimedPort = config.portName;
        m_modbus.setRequestBudget(static_cast<int>(deadline.remainingTime()), cancelled.get());
        if (!m_modbus.open(config)) {
            closeTransport(); completed({}, QStringLiteral("Classic OPC Modbus open failed"), false); return;
        }
        m_openConfig = config.toVariantMap();
    }
    auto result = [&](const ClassicPollPoint& point, const QVariant& value, const QString& issue) {
        RuntimePointValue sample;
        sample.pointId = point.point.id; sample.value = value;
        sample.quality = issue.isEmpty() ? RuntimePointQuality::Good : RuntimePointQuality::Bad;
        sample.timestamp = QDateTime::currentDateTimeUtc(); sample.origin = QStringLiteral("opc-poll");
        values.append(sample);
        if (!issue.isEmpty()) error = issue;
    };
    for (const auto& point : points) if (!point.error.isEmpty()) result(point, {}, point.error);
    for (const auto& batch : planClassicReads(points, maxRegisters)) {
        const bool bits = batch.area == "coil" || batch.area == "discrete";
        bool ok = false;
        QVector<quint16> words;
        QVector<bool> bitValues;
        if (!stopped()) {
            m_modbus.setRequestBudget(static_cast<int>(deadline.remainingTime()), cancelled.get());
            m_modbus.setStationAddress(batch.unit);
            if (batch.area == "coil") ok = m_modbus.readCoils(batch.address, batch.width);
            else if (batch.area == "discrete") ok = m_modbus.readDiscreteInputs(batch.address, batch.width);
            else if (batch.area == "input") ok = m_modbus.readInputRegisters(batch.address, batch.width);
            else ok = m_modbus.readHoldingRegisters(batch.address, batch.width);
            if (bits) bitValues = batch.area == "coil" ? m_modbus.coils().value(batch.address) : m_modbus.discreteInputs().value(batch.address);
            else words = batch.area == "input" ? m_modbus.inputRegisters().value(batch.address) : m_modbus.holdingRegisters().value(batch.address);
            ok = ok && (bits ? bitValues.size() : words.size()) == batch.width;
        }
        for (const auto& point : batch.points) {
            QVariant decoded;
            QString issue;
            if (!ok || stopped()) issue = QStringLiteral("Classic poll failed, cancelled or exceeded its total budget");
            else if (bits) {
                QVariantList list;
                for (int i = 0; i < point.width(); ++i) list.append(bitValues.at(point.address - batch.address + i));
                decoded = list.size() == 1 ? list.first() : QVariant(list);
            } else {
                auto slice = words.mid(point.address - batch.address, point.width());
                if (point.codec.dataType == "BOOL" && slice.size() == 1) slice[0] = (slice[0] >> point.bit) & 1;
                if (!RuntimePointRegisterCodec::decode(point.codec, slice, &decoded, &issue) && issue.isEmpty())
                    issue = QStringLiteral("Classic point decode failed");
            }
            result(point, issue.isEmpty() ? decoded : QVariant(), issue);
        }
    }
    completed(values, error, m_modbus.isConnected());
}
