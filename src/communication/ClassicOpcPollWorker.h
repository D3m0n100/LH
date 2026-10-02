#pragma once
#include "ModbusInterface.h"
#include "RuntimePointRegisterCodec.h"
#include "../common/RuntimePointTypes.h"
#include <functional>
#include <memory>

struct ClassicPollPoint {
    RuntimePointDefinition point;
    RuntimePointRegisterCodecSpec codec;
    QString area;
    int unit = 1;
    int address = -1;
    int bit = 0;
    QString error;
    int width() const { return area == "coil" || area == "discrete" ? codec.elementCount : codec.registerCount; }
};
struct ClassicReadBatch {
    QString area;
    int unit = 1;
    int address = 0;
    int width = 0;
    QList<ClassicPollPoint> points;
};
QList<ClassicReadBatch> planClassicReads(QList<ClassicPollPoint> points, int maxRegisters = ModbusLimits::ReadRegisters);

// Construct, use and destroy this object and its transport on one I/O thread.
class ClassicOpcPollWorker : public QObject {
public:
    explicit ClassicOpcPollWorker(std::unique_ptr<ModbusInterface> transport = {});
    ~ClassicOpcPollWorker() override;
    void poll(ModbusConfig config, QList<ClassicPollPoint> points, int budgetMs, int maxRegisters,
              std::shared_ptr<std::atomic_bool> cancelled,
              std::function<void(QList<RuntimePointValue>, QString, bool)> completed);
    void closeWhenIdle(std::shared_ptr<std::atomic_bool> session);
private:
    void closeTransport();
    std::unique_ptr<ModbusInterface> m_modbus;
    bool m_busy = false;
    std::shared_ptr<std::atomic_bool> m_session;
    QString m_owner;
    QString m_claimedPort;
    QVariantMap m_openConfig;
};
