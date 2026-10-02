#include "AsyncOpcServer.h"
#include "MatrikonOpcServer.h"
#include "../common/DeferredThreadCleanup.h"
#include <QThread>
#include <QPointer>
#include <QDebug>
#include <atomic>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#include <objbase.h>
#endif

struct AsyncOpcServer::WorkerState {
    QThread* thread = nullptr;
    QObject* executor = nullptr;
    IOpcServer* server = nullptr; // Worker-thread only.
    std::atomic<quint64> generation{0};
    std::atomic<int> pending{0};
    std::atomic_bool closing{false};
    std::atomic_bool stopQueued{false};
    std::atomic_bool statusPending{false};
    std::atomic<int> forwardedWrites{0};
    std::atomic<quint64> droppedWrites{0};
    quint64 activeGeneration = 0; // Worker-thread only; native events belong to this session.
    std::atomic<quintptr> nativeThreadId{0};
    std::atomic_bool cancellationEnabled{false};
    bool comInitialized = false; // Worker-thread only.
};

AsyncOpcServer::AsyncOpcServer(Factory factory, QObject* parent)
    : IOpcServer(parent), m_worker(std::make_shared<WorkerState>())
{
    const auto state = m_worker;
    const QPointer<AsyncOpcServer> owner(this);
    state->thread = new QThread;
    state->executor = new QObject;
    state->executor->moveToThread(state->thread);
    connect(state->thread, &QThread::started, state->executor, [state, owner, factory] {
        state->nativeThreadId = reinterpret_cast<quintptr>(QThread::currentThreadId());
        try {
#ifdef Q_OS_WIN
            const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            if (FAILED(initialized)) throw std::runtime_error("Cannot initialize OPC worker COM apartment");
            state->comInitialized = true;
            state->cancellationEnabled = SUCCEEDED(CoEnableCallCancellation(nullptr));
#endif
            state->server = factory();
            if (!state->server) throw std::runtime_error("OPC worker factory returned null");
            state->server->setParent(state->executor);
            QObject::connect(state->server, &IOpcServer::runningStateChanged, state->executor,
                [state, owner](bool) {
                    auto snapshot = state->server->statusSnapshot();
                    snapshot.extras.insert("serviceRunning", state->server->isRunning());
                    const auto generation = state->activeGeneration;
                    if (owner) QMetaObject::invokeMethod(owner, [owner, generation, snapshot] {
                        if (owner) owner->acceptSnapshot(snapshot, generation, false);
                    }, Qt::QueuedConnection);
                });
            QObject::connect(state->server, &IOpcServer::errorOccurred, state->executor,
                [state, owner](const QString& error) {
                    const auto generation = state->activeGeneration;
                    if (owner) QMetaObject::invokeMethod(owner, [state, owner, generation, error] {
                        if (owner && generation == state->generation) emit owner->errorOccurred(error);
                    }, Qt::QueuedConnection);
                });
            QObject::connect(state->server, &IOpcServer::writeRequestReceived, state->executor,
                [state, owner](const QString& id, const QVariant& value) {
                    if (state->forwardedWrites.fetch_add(1) >= 128) {
                        --state->forwardedWrites; ++state->droppedWrites; return;
                    }
                    const auto generation = state->activeGeneration;
                    if (owner) QMetaObject::invokeMethod(owner, [state, owner, generation, id, value] {
                        --state->forwardedWrites;
                        if (owner && generation == state->generation && owner->isRunning())
                            emit owner->writeRequestReceived(id, value);
                    }, Qt::QueuedConnection);
                    else --state->forwardedWrites;
                });
            auto* statusTimer = new QTimer(state->executor);
            statusTimer->setInterval(250);
            QObject::connect(statusTimer, &QTimer::timeout, state->executor, [state, owner] {
                if (state->closing || !owner || state->statusPending.exchange(true)) return;
                auto snapshot = state->server->statusSnapshot();
                snapshot.extras.insert("serviceRunning", state->server->isRunning());
                const auto generation = state->activeGeneration;
                QMetaObject::invokeMethod(owner, [state, owner, generation, snapshot] {
                    state->statusPending = false;
                    if (owner) owner->acceptSnapshot(snapshot, generation, false);
                }, Qt::QueuedConnection);
            });
            statusTimer->start();
        } catch (const std::exception& error) {
            const QString message = QString::fromUtf8(error.what());
            if (owner) QMetaObject::invokeMethod(owner, [owner, message] {
                if (owner) emit owner->errorOccurred(message);
            }, Qt::QueuedConnection);
        }
    });
    state->thread->start();
    m_startDeadline.setSingleShot(true);
    connect(&m_startDeadline, &QTimer::timeout, this, [this] {
        stop();
        m_snapshot.lastErrorCode = CommErrorCode::ConnectionTimeout;
        m_snapshot.lastErrorMessage = QStringLiteral("OPC 启动超过总期限，已取消当前会话");
        emit errorOccurred(m_snapshot.lastErrorMessage);
    });
}

AsyncOpcServer::~AsyncOpcServer()
{
    const auto state = m_worker;
    state->closing = true;
    ++state->generation;
    QMetaObject::invokeMethod(state->executor, [state] {
        if (state->server) { state->server->stop(); delete state->server; state->server = nullptr; }
#ifdef Q_OS_WIN
        if (state->cancellationEnabled) CoDisableCallCancellation(nullptr);
        if (state->comInitialized) CoUninitialize();
#endif
        state->executor->deleteLater();
        QThread::currentThread()->quit();
    }, Qt::QueuedConnection);
    if (!state->thread->wait(2000)) {
#ifdef Q_OS_WIN
        if (state->cancellationEnabled && state->nativeThreadId)
            CoCancelCall(DWORD(state->nativeThreadId.load()), 0);
#endif
        if (!state->thread->wait(500)) {
            // A native service can ignore RPC cancellation. Keep its apartment alive until return/process exit.
            DeferredThreadCleanup::retain(state->thread);
            qWarning("OPC native call did not finish within shutdown budget; apartment retained until completion");
            return;
        }
    }
    delete state->thread;
}

bool AsyncOpcServer::enqueue(std::function<void(IOpcServer*)> action, bool finishesStart, bool finishesValues)
{
    const auto state = m_worker;
    if (state->closing) return false;
    if (state->pending.fetch_add(1) >= 32) {
        --state->pending;
        emit errorOccurred(QStringLiteral("OPC 请求队列已满或服务正在关闭"));
        return false;
    }
    const auto generation = state->generation.load();
    const QPointer<AsyncOpcServer> owner(this);
    const bool posted = QMetaObject::invokeMethod(state->executor, [state, owner, generation, action, finishesStart, finishesValues] {
        BackendStatusSnapshot snapshot;
        QString error;
        if (!state->closing && generation == state->generation && state->server) {
            state->activeGeneration = generation;
            try {
                action(state->server);
                if (generation != state->generation || state->closing) state->server->stop();
                snapshot = state->server->statusSnapshot();
                snapshot.extras.insert("serviceRunning", state->server->isRunning());
                if (finishesStart && !state->server->isRunning())
                    error = snapshot.lastErrorMessage.isEmpty() ? QStringLiteral("OPC 启动失败") : snapshot.lastErrorMessage;
            }
            catch (...) { error = QStringLiteral("OPC 请求遇到异常"); }
        } else if (!state->server && generation == state->generation) {
            snapshot.lastErrorCode = CommErrorCode::InvalidConfig;
            snapshot.lastErrorMessage = QStringLiteral("OPC 工作线程初始化失败");
            error = snapshot.lastErrorMessage;
        }
        --state->pending;
        if (owner) QMetaObject::invokeMethod(owner, [owner, generation, snapshot, error, finishesStart, finishesValues] {
            if (!owner) return;
            if (finishesValues && generation == owner->m_worker->generation) owner->m_valuesScheduled = false;
            owner->acceptSnapshot(snapshot, generation, finishesStart);
            if (!error.isEmpty() && generation == owner->m_worker->generation) emit owner->errorOccurred(error);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
    if (!posted) --state->pending;
    return posted;
}

void AsyncOpcServer::acceptSnapshot(const BackendStatusSnapshot& snapshot, quint64 generation, bool finishesStart)
{
    if (generation != m_worker->generation || m_worker->closing) return;
    if (m_starting && !finishesStart) return;
    m_snapshot = snapshot;
    if (finishesStart) { m_starting = false; m_startDeadline.stop(); }
    const bool running = snapshot.extras.value("serviceRunning").toBool();
    if (running != m_running) { m_running = running; emit runningStateChanged(running); }
    scheduleValues();
}

bool AsyncOpcServer::applyConfig(const OpcServerConfig& config, QString* error)
{
    if (!MatrikonOpcServer::validateConfig(config, error)) return false;
    stop();
    m_config = config;
    m_configured = true;
    return true;
}

bool AsyncOpcServer::start(QString* error)
{
    if (!m_configured || m_starting) {
        if (error) *error = QStringLiteral("OPC 未配置或正在启动");
        return false;
    }
    if (m_running) return true;
    const auto config = m_config;
    const auto points = m_points;
    const auto tags = m_tags;
    m_starting = true;
    const bool admitted = enqueue([config, points, tags](IOpcServer* server) {
        QString error;
        if (!server->applyConfig(config, &error)) return;
        server->setRuntimePoints(points);
        server->setOpcTags(tags);
        server->start(&error);
    }, true);
    if (admitted) m_startDeadline.start(qBound(1, config.timeoutMs, 30000));
    else m_starting = false;
    return admitted;
}

void AsyncOpcServer::stop()
{
    ++m_worker->generation;
#ifdef Q_OS_WIN
    if (m_worker->cancellationEnabled && m_worker->nativeThreadId && m_worker->pending > 0)
        CoCancelCall(DWORD(m_worker->nativeThreadId.load()), 0);
#endif
    m_starting = false;
    m_startDeadline.stop();
    m_values.clear();
    m_valueBytes = 0;
    m_valuesScheduled = false;
    m_snapshot.online = false;
    if (m_running) { m_running = false; emit runningStateChanged(false); }
    // Cancellation must remain admissible even when the ordinary request queue is full.
    const auto state = m_worker;
    if (!state->closing && !state->stopQueued.exchange(true)) {
        QMetaObject::invokeMethod(state->executor, [state] {
            state->stopQueued = false;
            if (state->server) state->server->stop();
        }, Qt::QueuedConnection);
    }
}

void AsyncOpcServer::setRuntimePoints(const QList<RuntimePointDefinition>& points)
{
    if (points.size() > 4096) { emit errorOccurred(QStringLiteral("OPC 点表超过容量限制")); return; }
    m_points = points;
    if (m_running || m_starting) enqueue([points](IOpcServer* server) { server->setRuntimePoints(points); });
}
void AsyncOpcServer::setOpcTags(const QList<OpcTagDefinition>& tags)
{
    if (tags.size() > 4096) { emit errorOccurred(QStringLiteral("OPC 标签表超过容量限制")); return; }
    m_tags = tags;
    if (m_running || m_starting) enqueue([tags](IOpcServer* server) { server->setOpcTags(tags); });
}
void AsyncOpcServer::updatePointValues(const QList<RuntimePointValue>& values)
{
    for (const auto& value : values) {
        const qint64 bytes = value.pointId.size() * qint64(sizeof(QChar))
            + value.value.toString().size() * qint64(sizeof(QChar)) + 256;
        const auto previous = m_values.constFind(value.pointId);
        const qint64 oldBytes = previous == m_values.constEnd() ? 0
            : previous->pointId.size() * qint64(sizeof(QChar))
                + previous->value.toString().size() * qint64(sizeof(QChar)) + 256;
        if (bytes > 128 * 1024 || m_valueBytes - oldBytes + bytes > 4 * 1024 * 1024
            || (m_values.size() >= 4096 && previous == m_values.constEnd())) {
            ++m_droppedValues;
            continue;
        }
        m_values.insert(value.pointId, value);
        m_valueBytes += bytes - oldBytes;
    }
    scheduleValues();
}
void AsyncOpcServer::scheduleValues()
{
    if (m_valuesScheduled || m_values.isEmpty() || !m_running || m_worker->pending >= 32) return;
    const auto values = m_values.values();
    m_values.clear();
    m_valueBytes = 0;
    m_valuesScheduled = true;
    const bool admitted = enqueue([values](IOpcServer* server) {
        server->updatePointValues(values);
    }, false, true);
    if (!admitted) {
        m_valuesScheduled = false;
        for (const auto& value : values) {
            m_values.insert(value.pointId, value);
            m_valueBytes += value.pointId.size() * qint64(sizeof(QChar))
                + value.value.toString().size() * qint64(sizeof(QChar)) + 256;
        }
    }
}
void AsyncOpcServer::recordWriteResult(const QString& id, bool success, const QString& message)
{
    enqueue([id, success, message](IOpcServer* server) { server->recordWriteResult(id, success, message); });
}
BackendStatusSnapshot AsyncOpcServer::statusSnapshot() const
{
    auto snapshot = m_snapshot;
    snapshot.backendType = QStringLiteral("matrikon-opc-da");
    snapshot.extras.insert("impl", "matrikon-opc-da");
    snapshot.extras.insert("asyncOperations", true);
    snapshot.extras.insert("startPending", m_starting);
    snapshot.extras.insert("pendingRequests", m_worker->pending.load());
    snapshot.extras.insert("pendingValueCount", m_values.size());
    snapshot.extras.insert("pendingValueBytes", m_valueBytes);
    snapshot.extras.insert("droppedValueCount", QVariant::fromValue(m_droppedValues));
    snapshot.extras.insert("pendingForwardedWrites", m_worker->forwardedWrites.load());
    snapshot.extras.insert("droppedForwardedWrites", QVariant::fromValue(m_worker->droppedWrites.load()));
    return snapshot;
}
