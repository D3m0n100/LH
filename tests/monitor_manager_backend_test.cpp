/**
 * @file monitor_manager_backend_test.cpp
 * @brief MonitorManager backend 切换测试
 */

#include <QtTest/QtTest>

#include <utility>
#include <limits>

#include "common/ConfigTypes.h"
#include "communication/IDeviceBackend.h"
#include "communication/RuntimePointQualityMapper.h"
#include "communication/VirtualDeviceBackend.h"
#include "monitor/MonitorDataProcessor.h"
#include "monitor/MonitorManager.h"
#include "monitor/IMonitorHistoryStore.h"

using namespace Monitor;

class FailingReadBackend final : public IDeviceBackend
{
public:
    bool connectBackend() override { return true; }
    void disconnectBackend() override {}
    bool isOnline() const override { return true; }

    bool readPoints(const QStringList& pointIds,
                    QHash<QString, QVariant>& values,
                    QString* errorMessage,
                    QHash<QString, CommError>* pointErrors) override
    {
        values.clear();
        const CommError error(CommProtocolType::Custom,
                              CommErrorCode::InvalidAddress,
                              QStringLiteral("read failed"));
        if (errorMessage) {
            *errorMessage = error.message;
        }
        if (pointErrors) {
            for (const QString& pointId : pointIds) {
                pointErrors->insert(pointId, error);
            }
        }
        return false;
    }

    bool writePoints(const QHash<QString, QVariant>&,
                     QString* errorMessage,
                     QHash<QString, CommError>*) override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("unsupported");
        }
        return false;
    }

    bool downloadArtifact(const QString&,
                          const QVariantMap&,
                          QString* errorMessage,
                          CommError*) override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("unsupported");
        }
        return false;
    }

    BackendStatusSnapshot statusSnapshot() const override
    {
        BackendStatusSnapshot status;
        status.online = true;
        status.backendType = QStringLiteral("failing-read");
        status.lastErrorCode = CommErrorCode::InvalidAddress;
        status.lastErrorMessage = QStringLiteral("read failed");
        return status;
    }
};

class RecordingReadBackend final : public IDeviceBackend
{
public:
    bool connectBackend() override
    {
        m_online = true;
        return true;
    }

    void disconnectBackend() override { m_online = false; }
    bool isOnline() const override { return m_online; }

    bool readPoints(const QStringList& pointIds,
                    QHash<QString, QVariant>& values,
                    QString* errorMessage,
                    QHash<QString, CommError>* pointErrors) override
    {
        Q_UNUSED(errorMessage);
        if (pointErrors) {
            pointErrors->clear();
        }
        requests.append(pointIds);
        values.clear();
        for (const QString& pointId : pointIds) {
            if (pointValues.contains(pointId)) {
                values.insert(pointId, pointValues.value(pointId));
            }
        }
        return true;
    }

    bool writePoints(const QHash<QString, QVariant>&,
                     QString* errorMessage,
                     QHash<QString, CommError>*) override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("unsupported");
        }
        return false;
    }

    bool downloadArtifact(const QString&,
                          const QVariantMap&,
                          QString* errorMessage,
                          CommError*) override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("unsupported");
        }
        return false;
    }

    BackendStatusSnapshot statusSnapshot() const override
    {
        BackendStatusSnapshot status;
        status.online = m_online;
        status.backendType = m_backendType;
        status.lastErrorCode = m_lastErrorCode;
        status.lastErrorMessage = m_lastErrorMessage;
        return status;
    }

    void emitPointsChanged(const QHash<QString, QVariant>& updates)
    {
        emit pointsChanged(updates);
    }

    QList<QStringList> requests;
    QHash<QString, QVariant> pointValues;
    bool m_online = true;
    QString m_backendType = QStringLiteral("recording");
    CommErrorCode m_lastErrorCode = CommErrorCode::NoError;
    QString m_lastErrorMessage;
};

class InMemoryHistoryStore final : public IMonitorHistoryStore
{
public:
    bool m_available = true;
    bool m_cancelled = false;
    bool m_simulateSqlError = false;
    bool m_cancelCalled = false;
    QList<RuntimeRecord> m_records;

    bool isAvailable() const override { return m_available; }

    QList<RuntimeRecord> getLatestRecords(const QString& channelName, int count) override
    {
        if (!m_available || count <= 0) return {};
        QList<RuntimeRecord> matching;
        for (const auto& r : m_records) {
            if (r.variableName == channelName) {
                matching.append(r);
            }
        }
        QList<RuntimeRecord> out;
        for (int i = matching.size() - 1; i >= 0 && out.size() < count; --i) {
            out.append(matching.at(i));
        }
        return out;
    }

    QList<RuntimeRecord> queryHistory(const QString& channelName,
                                      const QDateTime& start,
                                      const QDateTime& end) override
    {
        if (!m_available) return {};
        QList<RuntimeRecord> out;
        for (const auto& r : m_records) {
            if (r.variableName == channelName &&
                r.timestamp >= start && r.timestamp <= end) {
                out.append(r);
            }
        }
        return out;
    }

    RuntimeHistoryPage queryHistoryPage(const QString& channelName,
                                        const QDateTime& start,
                                        const QDateTime& end,
                                        int pageSize,
                                        const RuntimeHistoryCursor& cursor) override
    {
        RuntimeHistoryPage page;
        if (m_cancelled) {
            page.status = RuntimeHistoryPageStatus::Cancelled;
            page.errorCode = QStringLiteral("CANCELLED");
            page.errorText = QStringLiteral("Operation cancelled");
            return page;
        }
        if (!m_available) {
            page.status = RuntimeHistoryPageStatus::NotInitialized;
            page.errorCode = QStringLiteral("NOT_INITIALIZED");
            page.errorText = QStringLiteral("Store unavailable");
            return page;
        }
        if (m_simulateSqlError) {
            page.status = RuntimeHistoryPageStatus::SqlError;
            page.errorCode = QStringLiteral("SQL_ERROR");
            page.errorText = QStringLiteral("Simulated SQL error");
            return page;
        }

        QList<RuntimeRecord> matching;
        for (const auto& r : m_records) {
            if (r.variableName != channelName) continue;
            if (r.timestamp < start || r.timestamp > end) continue;
            if (cursor.maxId >= 0 && r.id > cursor.maxId) continue;
            if (cursor.isValid()) {
                if (r.timestamp < cursor.timestamp) continue;
                if (r.timestamp == cursor.timestamp && r.id <= cursor.id) continue;
            }
            matching.append(r);
        }

        page.status = RuntimeHistoryPageStatus::Success;
        const int countToTake = qMin(pageSize, matching.size());
        for (int i = 0; i < countToTake; ++i) {
            page.records.append(matching.at(i));
        }
        page.hasMore = matching.size() > pageSize;
        if (!page.records.isEmpty()) {
            page.nextCursor.timestamp = page.records.last().timestamp;
            page.nextCursor.id = page.records.last().id;
            page.nextCursor.maxId = cursor.maxId;
        }
        return page;
    }

    RuntimeHistoryPage queryLatestHistoryPage(const QString& channelName,
                                              int maxCount,
                                              int pageSize,
                                              const RuntimeHistoryCursor& cursor,
                                              const QDateTime& end) override
    {
        RuntimeHistoryPage page;
        if (m_cancelled) {
            page.status = RuntimeHistoryPageStatus::Cancelled;
            page.errorCode = QStringLiteral("CANCELLED");
            page.errorText = QStringLiteral("Operation cancelled");
            return page;
        }
        if (!m_available) {
            page.status = RuntimeHistoryPageStatus::NotInitialized;
            page.errorCode = QStringLiteral("NOT_INITIALIZED");
            page.errorText = QStringLiteral("Store unavailable");
            return page;
        }
        if (m_simulateSqlError) {
            page.status = RuntimeHistoryPageStatus::SqlError;
            page.errorCode = QStringLiteral("SQL_ERROR");
            page.errorText = QStringLiteral("Simulated SQL error");
            return page;
        }

        QList<RuntimeRecord> matching;
        for (const auto& r : m_records) {
            if (r.variableName != channelName) continue;
            if (end.isValid() && r.timestamp > end) continue;
            matching.append(r);
        }

        if (maxCount > 0 && matching.size() > maxCount) {
            matching = matching.mid(matching.size() - maxCount);
        }

        QList<RuntimeRecord> afterCursor;
        for (const auto& r : matching) {
            if (cursor.maxId >= 0 && r.id > cursor.maxId) continue;
            if (cursor.isValid()) {
                if (r.timestamp < cursor.timestamp) continue;
                if (r.timestamp == cursor.timestamp && r.id <= cursor.id) continue;
            }
            afterCursor.append(r);
        }

        page.status = RuntimeHistoryPageStatus::Success;
        const int countToTake = qMin(pageSize, afterCursor.size());
        for (int i = 0; i < countToTake; ++i) {
            page.records.append(afterCursor.at(i));
        }
        page.hasMore = afterCursor.size() > pageSize;
        if (!page.records.isEmpty()) {
            page.nextCursor.timestamp = page.records.last().timestamp;
            page.nextCursor.id = page.records.last().id;
            page.nextCursor.maxId = cursor.maxId;
        }
        return page;
    }

    RuntimeHistoryCount countHistory(const QString& channelName,
                                     const QDateTime& start,
                                     const QDateTime& end,
                                     qint64 maxRecordId) override
    {
        RuntimeHistoryCount c;
        if (!m_available) {
            c.status = RuntimeHistoryPageStatus::NotInitialized;
            c.errorCode = QStringLiteral("NOT_INITIALIZED");
            return c;
        }
        if (m_simulateSqlError) {
            c.status = RuntimeHistoryPageStatus::SqlError;
            c.errorCode = QStringLiteral("SQL_ERROR");
            return c;
        }
        c.status = RuntimeHistoryPageStatus::Success;
        for (const auto& r : m_records) {
            if (r.variableName == channelName &&
                r.timestamp >= start && r.timestamp <= end &&
                (maxRecordId < 0 || r.id <= maxRecordId)) {
                c.count++;
            }
        }
        return c;
    }

    RuntimeHistoryCount countLatestHistory(const QString& channelName,
                                           int maxCount,
                                           const QDateTime& end,
                                           qint64 maxRecordId) override
    {
        RuntimeHistoryCount c;
        if (!m_available) {
            c.status = RuntimeHistoryPageStatus::NotInitialized;
            c.errorCode = QStringLiteral("NOT_INITIALIZED");
            return c;
        }
        if (m_simulateSqlError) {
            c.status = RuntimeHistoryPageStatus::SqlError;
            c.errorCode = QStringLiteral("SQL_ERROR");
            return c;
        }
        c.status = RuntimeHistoryPageStatus::Success;
        qint64 cnt = 0;
        for (const auto& r : m_records) {
            if (r.variableName == channelName &&
                (!end.isValid() || r.timestamp <= end) &&
                (maxRecordId < 0 || r.id <= maxRecordId)) {
                cnt++;
            }
        }
        c.count = (maxCount > 0) ? qMin(static_cast<qint64>(maxCount), cnt) : cnt;
        return c;
    }

    void cancelPendingRequests() override
    {
        m_cancelCalled = true;
    }
};

class MonitorManagerBackendTest : public QObject
{
    Q_OBJECT

private:
    static RuntimePointDefinition makePoint()
    {
        RuntimePointDefinition point;
        point.id = QStringLiteral("pt1");
        point.name = QStringLiteral("Point1");
        point.kind = RuntimePointKind::Status;
        point.access = RuntimePointAccess::ReadWrite;
        point.dataType = QStringLiteral("REAL");
        point.defaultValue = 0.0;
        return point;
    }

    static ProjectRuntimeConfig makeConfig()
    {
        ProjectRuntimeConfig cfg;
        cfg.projectName = QStringLiteral("backend-switch-test");
        MonitorProviderRuntimeConfig provider;
        provider.id = QStringLiteral("pt1");
        provider.channelName = QStringLiteral("channel.1");
        provider.unit = QStringLiteral("bar");
        provider.periodMs = 50;
        cfg.providers.append(provider);
        return cfg;
    }

private slots:
    void init()
    {
        auto& manager = MonitorManager::instance();
        manager.setDatabaseLoggingEnabled(false);
        manager.stopMonitoring();
        manager.setDataProcessor(nullptr);
        manager.setDeviceBackend(nullptr);
        manager.applyConfiguration(ProjectRuntimeConfig());
        manager.clearAllData();
    }

    void cleanup()
    {
        auto& manager = MonitorManager::instance();
        manager.stopMonitoring();
        manager.setDataProcessor(nullptr);
        manager.setDeviceBackend(nullptr);
        manager.applyConfiguration(ProjectRuntimeConfig());
        manager.clearAllData();
    }

    void switchingBackendDisconnectsOldPushSource()
    {
        auto& manager = MonitorManager::instance();
        VirtualDeviceBackend backend1;
        VirtualDeviceBackend backend2;

        backend1.loadPointDefinitions({makePoint()});
        backend2.loadPointDefinitions({makePoint()});
        backend1.connectBackend();
        backend2.connectBackend();

        manager.setDeviceBackend(&backend1);
        QVERIFY(manager.applyConfiguration(makeConfig()));
        manager.startMonitoring();

        QSignalSpy spy(&manager, &MonitorManager::sampleRecorded);

        QVERIFY(backend1.writePoints({{QStringLiteral("pt1"), 1.0}}, nullptr));
        QCOMPARE(spy.count(), 1);

        manager.setDeviceBackend(&backend2);
        QVERIFY(manager.applyConfiguration(makeConfig()));

        QVERIFY(backend1.writePoints({{QStringLiteral("pt1"), 2.0}}, nullptr));
        QCOMPARE(spy.count(), 1);

        QVERIFY(backend2.writePoints({{QStringLiteral("pt1"), 3.0}}, nullptr));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("channel.1"));
        QCOMPARE(spy.at(1).at(1).toDouble(), 3.0);

        manager.setDeviceBackend(nullptr);
        QVERIFY(backend2.writePoints({{QStringLiteral("pt1"), 4.0}}, nullptr));
        QCOMPARE(spy.count(), 2);
    }

    void backendPollCarriesBackendStatusMetadata()
    {
        auto& manager = MonitorManager::instance();
        VirtualDeviceBackend backend;

        backend.loadPointDefinitions({makePoint()});
        backend.connectBackend();

        manager.setDeviceBackend(&backend);
        QVERIFY(manager.applyConfiguration(makeConfig()));
        manager.startMonitoring();

        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));

        const auto history = manager.history(QStringLiteral("channel.1"), 1);
        QVERIFY(!history.isEmpty());
        const auto sample = history.last();
        QCOMPARE(sample.metadata.value(QStringLiteral("source")).toString(), QStringLiteral("backend_poll"));
        QCOMPARE(sample.metadata.value(QStringLiteral("backendType")).toString(), QStringLiteral("virtual"));
        QVERIFY(sample.metadata.contains(QStringLiteral("backendOnline")));
        QVERIFY(sample.metadata.contains(QStringLiteral("backendLastErrorCode")));
    }

    void qualityAndErrorHelpersAreStable()
    {
        QCOMPARE(commErrorCodeToString(CommErrorCode::ConnectionLost), QStringLiteral("ConnectionLost"));
        QCOMPARE(runtimePointQualityToString(RuntimePointQuality::Good), QStringLiteral("Good"));

        const CommError timeoutError(CommProtocolType::Custom, CommErrorCode::ConnectionTimeout, QStringLiteral("timeout"));
        QCOMPARE(runtimePointQualityFromBackendError(timeoutError, true), RuntimePointQuality::Stale);

        const CommError offlineError(CommProtocolType::Custom, CommErrorCode::ConnectionLost, QStringLiteral("lost"));
        QCOMPARE(runtimePointQualityFromBackendError(offlineError, false), RuntimePointQuality::Offline);

        const CommError badError(CommProtocolType::Custom, CommErrorCode::InvalidAddress, QStringLiteral("bad address"));
        QCOMPARE(runtimePointQualityFromBackendError(badError, true), RuntimePointQuality::Bad);
    }

    void failedPollDoesNotCreateNumericZero()
    {
        auto& manager = MonitorManager::instance();
        FailingReadBackend backend;
        MonitorDataProcessor processor;

        manager.setDataProcessor(&processor);
        manager.setDeviceBackend(&backend);
        QVERIFY(manager.applyConfiguration(makeConfig()));
        manager.startMonitoring();

        QSignalSpy numericSpy(&manager, &MonitorManager::sampleRecorded);
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));

        QCOMPARE(numericSpy.count(), 0);
        QVERIFY(manager.history(QStringLiteral("channel.1"), 1).isEmpty());
        QVERIFY(processor.getChannelData(QStringLiteral("channel.1")).isEmpty());
    }

    void invalidCandidateKeepsExistingRuntimeState()
    {
        auto& manager = MonitorManager::instance();
        const ProjectRuntimeConfig valid = makeConfig();
        QVERIFY(manager.applyConfiguration(valid));
        manager.startMonitoring();

        const QStringList channelsBefore = manager.channelNames();
        const QStringList providersBefore = manager.providerIds();
        QSignalSpy changedSpy(&manager, &MonitorManager::channelsChanged);

        ProjectRuntimeConfig invalid = valid;
        MonitorProviderRuntimeConfig broken;
        broken.id = QStringLiteral("broken");
        broken.channelName.clear();
        broken.periodMs = 50;
        invalid.providers.append(broken);

        QVERIFY(!manager.applyConfiguration(invalid));
        QCOMPARE(manager.channelNames(), channelsBefore);
        QCOMPARE(manager.providerIds(), providersBefore);
        QVERIFY(manager.isMonitoring());
        QCOMPARE(changedSpy.count(), 0);
    }

    void successfulApplyEmitsOneCollectionChange()
    {
        auto& manager = MonitorManager::instance();
        QSignalSpy changedSpy(&manager, &MonitorManager::channelsChanged);

        QVERIFY(manager.applyConfiguration(makeConfig()));
        QCOMPARE(changedSpy.count(), 1);
    }

    void destroyedObserversAreCleared()
    {
        auto& manager = MonitorManager::instance();

        auto* processor = new MonitorDataProcessor;
        manager.setDataProcessor(processor);
        delete processor;
        QVERIFY(manager.dataProcessor() == nullptr);

        auto* backend = new FailingReadBackend;
        manager.setDeviceBackend(backend);
        QVERIFY(manager.applyConfiguration(makeConfig()));
        manager.startMonitoring();
        delete backend;
        QVERIFY(manager.deviceBackend() == nullptr);

        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));
    }

    void duplicatePointIdsAreReadOnceAndKeepLastChannelMapping()
    {
        auto& manager = MonitorManager::instance();
        RecordingReadBackend backend;
        backend.connectBackend();
        backend.pointValues.insert(QStringLiteral("pt1"), 42.0);

        ProjectRuntimeConfig cfg;
        MonitorProviderRuntimeConfig first;
        first.id = QStringLiteral("pt1");
        first.channelName = QStringLiteral("channel.first");
        first.periodMs = 1000;
        cfg.providers.append(first);
        MonitorProviderRuntimeConfig second = first;
        second.channelName = QStringLiteral("channel.last");
        second.periodMs = 1000;
        cfg.providers.append(second);

        manager.setDeviceBackend(&backend);
        QVERIFY(manager.applyConfiguration(cfg));
        manager.startMonitoring();
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));

        QCOMPARE(backend.requests.size(), 1);
        QCOMPARE(backend.requests.first(), QStringList{QStringLiteral("pt1")});
        const auto history = manager.history(QStringLiteral("channel.last"), 1);
        QVERIFY(!history.isEmpty());
        QCOMPARE(history.last().value, 42.0);
        QVERIFY(manager.history(QStringLiteral("channel.first"), 1).isEmpty());
    }

    void backendPointsRespectIndividualPeriods()
    {
        auto& manager = MonitorManager::instance();
        RecordingReadBackend backend;
        backend.connectBackend();
        backend.pointValues.insert(QStringLiteral("fast"), 1.0);
        backend.pointValues.insert(QStringLiteral("slow"), 2.0);

        ProjectRuntimeConfig cfg;
        MonitorProviderRuntimeConfig fast;
        fast.id = QStringLiteral("fast");
        fast.channelName = QStringLiteral("channel.fast");
        fast.periodMs = 30;
        cfg.providers.append(fast);
        MonitorProviderRuntimeConfig slow;
        slow.id = QStringLiteral("slow");
        slow.channelName = QStringLiteral("channel.slow");
        slow.periodMs = 120;
        cfg.providers.append(slow);

        manager.setDeviceBackend(&backend);
        QVERIFY(manager.applyConfiguration(cfg));
        manager.startMonitoring();
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));
        QCOMPARE(backend.requests.size(), 1);

        QTest::qWait(45);
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));
        manager.stopMonitoring();

        QVERIFY(!backend.requests.isEmpty());
        for (int i = 1; i < backend.requests.size(); ++i) {
            QVERIFY2(!backend.requests.at(i).contains(QStringLiteral("slow")),
                     "slow point was read on the shortest provider period");
        }
        bool sawFast = false;
        for (int i = 1; i < backend.requests.size(); ++i) {
            sawFast = sawFast || backend.requests.at(i).contains(QStringLiteral("fast"));
        }
        QVERIFY(sawFast);

        manager.startMonitoring();
        QTRY_VERIFY_WITH_TIMEOUT([&backend]() {
            for (const QStringList& request : std::as_const(backend.requests)) {
                if (request.contains(QStringLiteral("slow"))) {
                    return true;
                }
            }
            return false;
        }(), 150);
        manager.stopMonitoring();
    }

    void repeatedStopPreventsProviderAndBackendReads()
    {
        auto& manager = MonitorManager::instance();
        RecordingReadBackend backend;
        backend.connectBackend();
        backend.pointValues.insert(QStringLiteral("pt1"), 7.0);
        manager.setDeviceBackend(&backend);
        QVERIFY(manager.applyConfiguration(makeConfig()));
        manager.startMonitoring();
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));
        const int readsAtStop = backend.requests.size();

        manager.stopMonitoring();
        manager.stopMonitoring();
        QVERIFY(!manager.isMonitoring());
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));
        QTest::qWait(35);
        QCOMPARE(backend.requests.size(), readsAtStop);
    }

    void explicitShutdownIsIdempotentAndStopsBackendPolling()
    {
        auto& manager = MonitorManager::instance();
        RecordingReadBackend backend;
        backend.connectBackend();
        backend.pointValues.insert(QStringLiteral("pt1"), 7.0);
        manager.setDeviceBackend(&backend);
        QVERIFY(manager.applyConfiguration(makeConfig()));
        manager.startMonitoring();
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));
        const int readsBeforeShutdown = backend.requests.size();

        manager.shutdown();
        manager.shutdown();

        QVERIFY(!manager.isMonitoring());
        QVERIFY(QMetaObject::invokeMethod(&manager, "onBackendPollTimeout", Qt::DirectConnection));
        QTest::qWait(60);
        QCOMPARE(backend.requests.size(), readsBeforeShutdown);
    }

    void repeatedStopStopsProviderTimer()
    {
        auto& manager = MonitorManager::instance();
        manager.setDeviceBackend(nullptr);
        QVERIFY(manager.applyConfiguration(makeConfig()));

        QSignalSpy samples(&manager, &MonitorManager::sampleRecorded);
        manager.startMonitoring();
        QTRY_VERIFY_WITH_TIMEOUT(samples.count() > 0, 250);
        manager.stopMonitoring();
        manager.stopMonitoring();
        const int samplesAtStop = samples.count();
        QTest::qWait(50);
        QCOMPARE(samples.count(), samplesAtStop);
    }

    void providerUnknownExceptionProducesBadSample()
    {
        auto& manager = MonitorManager::instance();
        MonitorDataProcessor processor;
        manager.setDataProcessor(&processor);

        bool qualityObserved = false;
        bool valueValidObserved = true;
        RuntimePointQuality quality = RuntimePointQuality::Unknown;
        connect(&processor, &MonitorDataProcessor::channelQualityUpdated,
                &processor, [&](const QString& channelId,
                                RuntimePointQuality observedQuality,
                                bool observedValueValid) {
                    if (channelId == QStringLiteral("provider.exception")) {
                        qualityObserved = true;
                        quality = observedQuality;
                        valueValidObserved = observedValueValid;
                    }
                }, Qt::DirectConnection);

        QString errorText;
        ProviderConfig provider;
        provider.id = QStringLiteral("provider.exception");
        provider.channelName = QStringLiteral("provider.exception");
        provider.unit = QStringLiteral("bar");
        provider.periodMs = 1000;
        provider.sampler = []() -> double {
            throw 42;
        };
        provider.errorHandler = [&errorText](const QString& error) {
            errorText = error;
        };
        QVERIFY(manager.registerProvider(provider));
        manager.startMonitoring();

        QTimer* providerTimer = nullptr;
        for (QTimer* timer : manager.findChildren<QTimer*>()) {
            if (timer->property("providerId").toString() == provider.id) {
                providerTimer = timer;
                break;
            }
        }
        QVERIFY(providerTimer);

        bool invoked = false;
        try {
            invoked = QMetaObject::invokeMethod(providerTimer, "timeout", Qt::DirectConnection);
        } catch (...) {
            QFAIL("provider exception escaped the Qt event handler");
        }

        QVERIFY(invoked);
        QVERIFY(qualityObserved);
        QCOMPARE(quality, RuntimePointQuality::Bad);
        QVERIFY(!valueValidObserved);
        QCOMPARE(errorText, QStringLiteral("unknown exception"));

        manager.unregisterProvider(provider.id);
        manager.stopMonitoring();
    }

    void pushQualityConsistentWithBackendTypeAndStatus()
    {
        auto& manager = MonitorManager::instance();
        MonitorDataProcessor processor;
        manager.setDataProcessor(&processor);

        RuntimePointQuality lastObservedQuality = RuntimePointQuality::Unknown;
        bool lastObservedValueValid = false;
        int qualitySignalCount = 0;
        connect(&processor, &MonitorDataProcessor::channelQualityUpdated,
                &processor, [&](const QString& channelId,
                                RuntimePointQuality observedQuality,
                                bool observedValueValid) {
                    if (channelId == QStringLiteral("channel.1")) {
                        lastObservedQuality = observedQuality;
                        lastObservedValueValid = observedValueValid;
                        qualitySignalCount++;
                    }
                }, Qt::DirectConnection);

        // 1. Virtual backend push -> Simulated quality
        VirtualDeviceBackend virtualBackend;
        virtualBackend.loadPointDefinitions({makePoint()});
        virtualBackend.connectBackend();
        manager.setDeviceBackend(&virtualBackend);
        QVERIFY(manager.applyConfiguration(makeConfig()));
        manager.startMonitoring();

        QVERIFY(virtualBackend.writePoints({{QStringLiteral("pt1"), 10.5}}, nullptr));
        auto hist = manager.history(QStringLiteral("channel.1"), 1);
        QVERIFY(!hist.isEmpty());
        QCOMPARE(hist.last().value, 10.5);
        QVERIFY(hist.last().valueValid);
        QCOMPARE(hist.last().quality, RuntimePointQuality::Simulated);
        QCOMPARE(hist.last().metadata.value(QStringLiteral("source")).toString(), QStringLiteral("backend_push"));
        QCOMPARE(hist.last().metadata.value(QStringLiteral("quality")).toString(), QStringLiteral("Simulated"));
        QCOMPARE(hist.last().metadata.value(QStringLiteral("backendType")).toString(), QStringLiteral("virtual"));
        QCOMPARE(lastObservedQuality, RuntimePointQuality::Simulated);
        QVERIFY(lastObservedValueValid);

        // 2. Real / custom backend push -> Good quality
        RecordingReadBackend recordingBackend;
        recordingBackend.m_backendType = QStringLiteral("modbus");
        recordingBackend.m_online = true;
        manager.setDeviceBackend(&recordingBackend);
        QVERIFY(manager.applyConfiguration(makeConfig()));

        recordingBackend.emitPointsChanged({{QStringLiteral("pt1"), 42.0}});
        hist = manager.history(QStringLiteral("channel.1"), 1);
        QVERIFY(!hist.isEmpty());
        QCOMPARE(hist.last().value, 42.0);
        QVERIFY(hist.last().valueValid);
        QCOMPARE(hist.last().quality, RuntimePointQuality::Good);
        QCOMPARE(hist.last().metadata.value(QStringLiteral("source")).toString(), QStringLiteral("backend_push"));
        QCOMPARE(hist.last().metadata.value(QStringLiteral("backendType")).toString(), QStringLiteral("modbus"));
        QCOMPARE(lastObservedQuality, RuntimePointQuality::Good);
        QVERIFY(lastObservedValueValid);

        // 3. Non-finite values: NaN and Inf -> Bad quality, valueValid == false
        recordingBackend.emitPointsChanged({{QStringLiteral("pt1"), std::numeric_limits<double>::quiet_NaN()}});
        QCOMPARE(lastObservedQuality, RuntimePointQuality::Bad);
        QVERIFY(!lastObservedValueValid);

        recordingBackend.emitPointsChanged({{QStringLiteral("pt1"), std::numeric_limits<double>::infinity()}});
        QCOMPARE(lastObservedQuality, RuntimePointQuality::Bad);
        QVERIFY(!lastObservedValueValid);

        // 4. Offline backend push -> Offline quality
        recordingBackend.m_online = false;
        recordingBackend.emitPointsChanged({{QStringLiteral("pt1"), 99.0}});
        QCOMPARE(lastObservedQuality, RuntimePointQuality::Offline);

        // 5. Backend switching disallows old backend pushes from reaching monitor
        manager.setDeviceBackend(nullptr);
        const int countBefore = qualitySignalCount;
        recordingBackend.emitPointsChanged({{QStringLiteral("pt1"), 123.0}});
        QCOMPARE(qualitySignalCount, countBefore);
    }

    void historyStoreInjectionAndPaging()
    {
        MonitorManager mgr;
        auto store = std::make_shared<InMemoryHistoryStore>();
        const QDateTime baseTime = QDateTime::fromString(QStringLiteral("2026-09-08T10:00:00.000Z"), Qt::ISODate);

        for (int i = 1; i <= 10; ++i) {
            RuntimeRecord r;
            r.id = i;
            r.timestamp = baseTime.addSecs(i);
            r.variableName = QStringLiteral("ch.inject");
            r.value = static_cast<double>(i) * 10.0;
            r.valueValid = true;
            r.quality = (i % 2 == 0) ? RuntimePointQuality::Good : RuntimePointQuality::Simulated;
            r.unit = QStringLiteral("bar");
            r.origin = QStringLiteral("test_source");
            if (i == 5) {
                r.errorCode = QStringLiteral("ERR_TEST");
                r.errorText = QStringLiteral("Test error message");
            }
            store->m_records.append(r);
        }

        mgr.setHistoryStore(store);
        QVERIFY(mgr.isDatabaseHistoryAvailable());

        // Test latest count
        QList<Sample> latest5 = mgr.historyFromDatabase(QStringLiteral("ch.inject"), 5);
        QCOMPARE(latest5.size(), 5);
        // Returns ASC sorted (reversed from getLatestRecords DESC)
        QCOMPARE(latest5.first().value, 60.0);
        QCOMPARE(latest5.last().value, 100.0);

        // Test keyset paging across pages of size 3
        const QDateTime start = baseTime;
        const QDateTime end = baseTime.addSecs(20);
        QList<Sample> allPagedSamples;
        RuntimeHistoryCursor cursor;
        int pageCount = 0;

        while (true) {
            DatabaseHistoryPage page = mgr.historyFromDatabasePage(QStringLiteral("ch.inject"), start, end, 3, cursor);
            QVERIFY(page.succeeded());
            QVERIFY(page.samples.size() <= 3);
            allPagedSamples.append(page.samples);
            pageCount++;
            if (!page.hasMore) {
                break;
            }
            cursor = page.nextCursor;
        }

        QCOMPARE(allPagedSamples.size(), 10);
        QCOMPARE(pageCount, 4); // 3 + 3 + 3 + 1 = 10 -> 4 pages
        QCOMPARE(allPagedSamples.first().value, 10.0);
        QCOMPARE(allPagedSamples.last().value, 100.0);
        // Verify metadata preservation
        QCOMPARE(allPagedSamples.at(0).metadata.value(QStringLiteral("origin")).toString(), QStringLiteral("test_source"));
        QCOMPARE(allPagedSamples.at(4).metadata.value(QStringLiteral("errorCode")).toString(), QStringLiteral("ERR_TEST"));
        QCOMPARE(allPagedSamples.at(4).metadata.value(QStringLiteral("error")).toString(), QStringLiteral("Test error message"));
        QCOMPARE(allPagedSamples.at(0).metadata.value(QStringLiteral("id")).toLongLong(), 1LL);
        QCOMPARE(allPagedSamples.at(9).metadata.value(QStringLiteral("id")).toLongLong(), 10LL);

        // Test count
        RuntimeHistoryCount count = mgr.historyFromDatabaseCount(QStringLiteral("ch.inject"), start, end);
        QVERIFY(count.succeeded());
        QCOMPARE(count.count, 10LL);
    }

    void historyStoreInstanceIsolation()
    {
        MonitorManager mgrA;
        MonitorManager mgrB;

        auto storeA = std::make_shared<InMemoryHistoryStore>();
        auto storeB = std::make_shared<InMemoryHistoryStore>();

        const QDateTime t = QDateTime::fromString(QStringLiteral("2026-09-08T12:00:00.000Z"), Qt::ISODate);

        for (int i = 1; i <= 5; ++i) {
            RuntimeRecord r;
            r.id = i;
            r.timestamp = t.addSecs(i);
            r.variableName = QStringLiteral("shared.ch");
            r.value = static_cast<double>(i) * 100.0;
            storeA->m_records.append(r);
        }

        for (int i = 1; i <= 3; ++i) {
            RuntimeRecord r;
            r.id = i;
            r.timestamp = t.addSecs(i * 10);
            r.variableName = QStringLiteral("shared.ch");
            r.value = static_cast<double>(i) * -1.0;
            storeB->m_records.append(r);
        }

        mgrA.setHistoryStore(storeA);
        mgrB.setHistoryStore(storeB);

        QList<Sample> samplesA = mgrA.historyFromDatabase(QStringLiteral("shared.ch"), 10);
        QList<Sample> samplesB = mgrB.historyFromDatabase(QStringLiteral("shared.ch"), 10);

        QCOMPARE(samplesA.size(), 5);
        QCOMPARE(samplesB.size(), 3);
        QCOMPARE(samplesA.first().value, 100.0);
        QCOMPARE(samplesB.first().value, -1.0);

        // Mutate storeA: verify storeB and mgrB are unaffected
        storeA->m_records.clear();
        QCOMPARE(mgrA.historyFromDatabase(QStringLiteral("shared.ch"), 10).size(), 0);
        QCOMPARE(mgrB.historyFromDatabase(QStringLiteral("shared.ch"), 10).size(), 3);
    }

    void historyStoreErrorAndCancellationHandling()
    {
        MonitorManager mgr;
        auto store = std::make_shared<InMemoryHistoryStore>();
        mgr.setHistoryStore(store);

        // 1. Uninitialized / Unavailable store
        store->m_available = false;
        QVERIFY(!mgr.isDatabaseHistoryAvailable());
        QVERIFY(mgr.historyFromDatabase(QStringLiteral("ch"), 10).isEmpty());

        DatabaseHistoryPage page = mgr.historyFromDatabasePage(
            QStringLiteral("ch"), QDateTime(), QDateTime(), 10);
        QCOMPARE(page.status, RuntimeHistoryPageStatus::NotInitialized);
        QVERIFY(!page.succeeded());

        RuntimeHistoryCount count = mgr.historyFromDatabaseCount(
            QStringLiteral("ch"), QDateTime(), QDateTime());
        QCOMPARE(count.status, RuntimeHistoryPageStatus::NotInitialized);

        // 2. Simulated SQL Error
        store->m_available = true;
        store->m_simulateSqlError = true;
        QVERIFY(mgr.isDatabaseHistoryAvailable());

        page = mgr.historyFromDatabasePage(
            QStringLiteral("ch"), QDateTime(), QDateTime(), 10);
        QCOMPARE(page.status, RuntimeHistoryPageStatus::SqlError);
        QCOMPARE(page.errorCode, QStringLiteral("SQL_ERROR"));

        count = mgr.historyFromDatabaseCount(
            QStringLiteral("ch"), QDateTime(), QDateTime());
        QCOMPARE(count.status, RuntimeHistoryPageStatus::SqlError);

        // 3. Cancellation
        store->m_simulateSqlError = false;
        store->m_cancelled = true;

        page = mgr.historyFromDatabasePage(
            QStringLiteral("ch"), QDateTime(), QDateTime(), 10);
        QCOMPARE(page.status, RuntimeHistoryPageStatus::Cancelled);

        // 4. Replacing store triggers cancelPendingRequests on old store
        QVERIFY(!store->m_cancelCalled);
        auto newStore = std::make_shared<InMemoryHistoryStore>();
        mgr.setHistoryStore(newStore);
        QVERIFY(store->m_cancelCalled);
    }
};

QTEST_MAIN(MonitorManagerBackendTest)
#include "monitor_manager_backend_test.moc"
