#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QThread>
#include <QVariant>
#include <QVector>

#include <memory>

#include "common/ConfigTypes.h"
#include "common/RuntimePointTypes.h"
#include "communication/ClassicOpcServer.h"
#include "communication/IOpcServer.h"
#define private public
#include "communication/MatrikonOpcServer.h"
#undef private
#include "communication/OpcServerFactory.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class OpcServerConfigTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultValues()
    {
        OpcServerConfig cfg;
        QVERIFY(!cfg.enabled);
        QCOMPARE(cfg.publishIntervalMs, 200);
        QCOMPARE(cfg.channelName, QStringLiteral("CommPort"));
        QCOMPARE(cfg.deviceName, QStringLiteral("COM1"));
        QCOMPARE(cfg.serialMode, QStringLiteral("19200,N,8,1"));
        QCOMPARE(cfg.timeoutMs, 2000);
        QCOMPARE(cfg.maxRegistersPerRequest, 1);
        QCOMPARE(cfg.rootDescription, QStringLiteral("Modbus Root"));
        QCOMPARE(cfg.classicServerName, QStringLiteral("LH Compatible OPC"));
        QCOMPARE(cfg.opcProgId, QStringLiteral("Matrikon.OPC.Modbus"));
    }

    void jsonRoundTrip()
    {
        OpcServerConfig in;
        in.enabled = true;
        in.publishIntervalMs = 500;
        in.exposeVariables = true;
        in.exposeParameters = false;
        in.exposeStatus = true;
        in.exposeAlarms = true;
        in.channelName = QStringLiteral("CommPort");
        in.deviceName = QStringLiteral("COM3");
        in.serialMode = QStringLiteral("9600,N,8,1");
        in.timeoutMs = 5000;
        in.reconnectDelayMs = 20;
        in.retries = 3;
        in.maxRegistersPerRequest = 8;
        in.rootDescription = QStringLiteral("Modbus Root");
        in.classicServerName = QStringLiteral("LH Compatible OPC");
        in.opcProgId = QStringLiteral("Matrikon.OPC.Modbus.1");
        in.exposeTagTable = false;
        in.metadata.insert(QStringLiteral("tag"), QStringLiteral("ci"));

        const QJsonObject json = in.toJson();
        const OpcServerConfig out = OpcServerConfig::fromJson(json);

        QCOMPARE(out.enabled, in.enabled);
        QCOMPARE(out.publishIntervalMs, in.publishIntervalMs);
        QCOMPARE(out.exposeVariables, in.exposeVariables);
        QCOMPARE(out.exposeParameters, in.exposeParameters);
        QCOMPARE(out.exposeStatus, in.exposeStatus);
        QCOMPARE(out.exposeAlarms, in.exposeAlarms);
        QCOMPARE(out.channelName, in.channelName);
        QCOMPARE(out.deviceName, in.deviceName);
        QCOMPARE(out.serialMode, in.serialMode);
        QCOMPARE(out.timeoutMs, in.timeoutMs);
        QCOMPARE(out.reconnectDelayMs, in.reconnectDelayMs);
        QCOMPARE(out.retries, in.retries);
        QCOMPARE(out.maxRegistersPerRequest, in.maxRegistersPerRequest);
        QCOMPARE(out.rootDescription, in.rootDescription);
        QCOMPARE(out.classicServerName, in.classicServerName);
        QCOMPARE(out.opcProgId, in.opcProgId);
        QCOMPARE(out.exposeTagTable, in.exposeTagTable);
        QCOMPARE(out.metadata.value(QStringLiteral("tag")).toString(), QStringLiteral("ci"));
    }

    void projectRuntimeConfigCarriesOpcConfig()
    {
        ProjectRuntimeConfig cfg;
        cfg.projectName = QStringLiteral("opc_project");
        cfg.opcServer.enabled = true;
        cfg.opcServer.channelName = QStringLiteral("CommPort");
        cfg.opcServer.deviceName = QStringLiteral("COM4");
        cfg.opcServer.serialMode = QStringLiteral("9600,N,8,1");

        const QJsonObject json = cfg.toJson();
        const ProjectRuntimeConfig restored = ProjectRuntimeConfig::fromJson(json);

        QCOMPARE(restored.projectName, cfg.projectName);
        QVERIFY(restored.opcServer.enabled);
        QCOMPARE(restored.opcServer.channelName, QStringLiteral("CommPort"));
        QCOMPARE(restored.opcServer.deviceName, QStringLiteral("COM4"));
        QCOMPARE(restored.opcServer.serialMode, QStringLiteral("9600,N,8,1"));
    }

    void runtimePointMapsToOpcTag()
    {
        VariableDefinition variable = VariableDefinition::makeTemplate(QStringLiteral("pump_speed"),
                                                                       QStringLiteral("REAL"));
        variable.binding = QStringLiteral("pump.speed");
        variable.metadata.insert(QStringLiteral("opcItemId"), QStringLiteral("CommPort.InitDevParamnt.4:12"));
        variable.metadata.insert(QStringLiteral("opcDevice"), QStringLiteral("InitDevParamnt"));

        const RuntimePointDefinition point = RuntimePointConverter::fromVariable(variable);
        const OpcTagDefinition tag = RuntimePointConverter::runtimePointToOpcTag(point);

        QCOMPARE(tag.tagName, QStringLiteral("pump_speed"));
        QCOMPARE(tag.tagGroup, QStringLiteral("Variables"));
        QCOMPARE(tag.server, QStringLiteral("LH"));
        QCOMPARE(tag.item, QStringLiteral("pump.speed"));
        QCOMPARE(tag.tagAccess, QStringLiteral("ReadWrite"));
        QCOMPARE(tag.metadata.value(QStringLiteral("opcItemId")).toString(),
                 QStringLiteral("CommPort.InitDevParamnt.4:12"));
        QCOMPARE(tag.addressing.value(QStringLiteral("opcDevice")).toString(),
                 QStringLiteral("InitDevParamnt"));
    }

    void factorySelectsMatrikonBackend()
    {
        OpcServerConfig cfg;
        cfg.enabled = true;
        cfg.opcProgId = QStringLiteral("Matrikon.OPC.Modbus");

        IOpcServer* server = OpcServerFactory::createForConfig(cfg);
        QVERIFY(server != nullptr);
        QCOMPARE(server->statusSnapshot().backendType, QStringLiteral("matrikon-opc-da"));
        QVERIFY(server->objectName().contains(QStringLiteral("MatrikonOpcServer")));
        delete server;
    }

    void factoryCanStillSelectLegacyClassicBackend()
    {
        OpcServerConfig cfg;
        cfg.metadata.insert(QStringLiteral("backend"), QStringLiteral("classic-modbus"));

        IOpcServer* server = OpcServerFactory::createForConfig(cfg);
        QVERIFY(server != nullptr);
        QCOMPARE(server->statusSnapshot().backendType, QStringLiteral("classic-modbus"));
        QVERIFY(server->objectName().contains(QStringLiteral("ClassicOpcServer")));
        delete server;
    }

    void matrikonBackendCachesValuesAndCountsWrites()
    {
        MatrikonOpcServer server;

        RuntimePointDefinition point;
        point.id = QStringLiteral("param.kp");
        point.name = QStringLiteral("Kp");
        point.kind = RuntimePointKind::Parameter;
        point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadWrite;
        point.opcTagName = QStringLiteral("Kp");
        point.opcTagGroup = QStringLiteral("Parameters");

        server.setRuntimePoints({point});

        RuntimePointValue value;
        value.pointId = point.id;
        value.value = 3.5;
        value.quality = RuntimePointQuality::Good;
        value.timestamp = QDateTime::currentDateTimeUtc();
        value.origin = QStringLiteral("opc-da");

        server.updatePointValues({value});
        server.recordWriteResult(point.id, true, QStringLiteral("write ok"));

        const BackendStatusSnapshot snapshot = server.statusSnapshot();
        QCOMPARE(snapshot.backendType, QStringLiteral("matrikon-opc-da"));
        QCOMPARE(snapshot.extras.value(QStringLiteral("opcProgId")).toString(), QStringLiteral("Matrikon.OPC.Modbus"));
        QCOMPARE(snapshot.extras.value(QStringLiteral("valueCount")).toInt(), 1);
        QCOMPARE(snapshot.extras.value(QStringLiteral("successfulWriteCount")).toInt(), 1);
        QCOMPARE(snapshot.extras.value(QStringLiteral("failedWriteCount")).toInt(), 0);
        QCOMPARE(snapshot.extras.value(QStringLiteral("lastWritePointId")).toString(), point.id);
        QCOMPARE(snapshot.extras.value(QStringLiteral("lastWriteSuccess")).toBool(), true);
    }

    void matrikonBackendBuildsPendingOpcItems()
    {
        MatrikonOpcServer server;

        RuntimePointDefinition point;
        point.id = QStringLiteral("param.kp");
        point.name = QStringLiteral("Kp");
        point.kind = RuntimePointKind::Parameter;
        point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadWrite;
        point.opcItemName = QStringLiteral("Channel.Device.Kp");
        point.opcTagName = QStringLiteral("Kp");
        point.opcTagGroup = QStringLiteral("Parameters");

        server.setRuntimePoints({point});

        const BackendStatusSnapshot snapshot = server.statusSnapshot();
        QCOMPARE(snapshot.backendType, QStringLiteral("matrikon-opc-da"));
        QCOMPARE(snapshot.extras.value(QStringLiteral("itemCount")).toInt(), 1);
        QCOMPARE(snapshot.extras.value(QStringLiteral("groupCreated")).toBool(), false);
        QCOMPARE(snapshot.extras.value(QStringLiteral("syncIoReady")).toBool(), false);
        QCOMPARE(snapshot.extras.value(QStringLiteral("browseSucceeded")).toBool(), false);
        QCOMPARE(snapshot.extras.value(QStringLiteral("subscriptionActive")).toBool(), false);
        QCOMPARE(snapshot.extras.value(QStringLiteral("callbackCount")).toInt(), 0);
        QCOMPARE(snapshot.extras.value(QStringLiteral("matchedItemCount")).toInt(), 0);
        QCOMPARE(snapshot.extras.value(QStringLiteral("unmatchedItemCount")).toInt(), 0);
    }

    void matrikonBackendReportsOfflineCapabilityState()
    {
        MatrikonOpcServer server;

        RuntimePointDefinition point;
        point.id = QStringLiteral("unresolved.point");
        point.kind = RuntimePointKind::Status;
        point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadOnly;
        server.setRuntimePoints({point});

        OpcServerConfig config;
        config.metadata.insert(QStringLiteral("subscriptionMode"), QStringLiteral("required"));
        QString errorMessage;
        QVERIFY(server.applyConfig(config, &errorMessage));

        const BackendStatusSnapshot snapshot = server.statusSnapshot();
        QVERIFY(!snapshot.online);
        QCOMPARE(snapshot.extras.value(QStringLiteral("lifecycleStatus")).toString(),
                 QStringLiteral("offline"));
        QCOMPARE(snapshot.extras.value(QStringLiteral("configuredItemCount")).toInt(), 1);
        QCOMPARE(snapshot.extras.value(QStringLiteral("activeItemCount")).toInt(), 0);
        QVERIFY(!snapshot.extras.value(QStringLiteral("configuredItemsAvailable")).toBool());
        QCOMPARE(snapshot.extras.value(QStringLiteral("subscriptionMode")).toString(),
                 QStringLiteral("required"));
        QVERIFY(snapshot.extras.value(QStringLiteral("subscriptionRequired")).toBool());
        QVERIFY(!snapshot.extras.value(QStringLiteral("readProbeAvailable")).toBool());
    }

    void matrikonBackendBuildsLhStyleModbusItemCandidates()
    {
        MatrikonOpcServer server;

        RuntimePointDefinition point;
        point.id = QStringLiteral("controller.state");
        point.name = QStringLiteral("Controller State");
        point.kind = RuntimePointKind::Status;
        point.dataType = QStringLiteral("UINT16");
        point.access = RuntimePointAccess::ReadOnly;
        point.addressing.insert(QStringLiteral("area"), QStringLiteral("holding"));
        point.addressing.insert(QStringLiteral("address"), 10);
        point.addressing.insert(QStringLiteral("opcChannel"), QStringLiteral("CommPort"));
        point.addressing.insert(QStringLiteral("opcDevice"), QStringLiteral("InitDevParamnt"));
        point.opcItemName = QStringLiteral("4:10");
        point.opcTagName = QStringLiteral("controller.state");

        const OpcTagDefinition tag = RuntimePointConverter::runtimePointToOpcTag(point);
        const QStringList candidates = server.itemIdCandidatesForTag(tag);

        QVERIFY(candidates.contains(QStringLiteral("4:10")));
        QVERIFY(candidates.contains(QStringLiteral("CommPort.InitDevParamnt.4:10")));
        QVERIFY(candidates.contains(QStringLiteral("CommPort.InitDevConfig.4:10")));
    }

    void matrikonBackendBuildsCandidatesFromLegacyAddressFields()
    {
        MatrikonOpcServer server;

        RuntimePointDefinition point;
        point.id = QStringLiteral("legacy.input.bit");
        point.name = QStringLiteral("Legacy Input Bit");
        point.kind = RuntimePointKind::Status;
        point.dataType = QStringLiteral("BOOL");
        point.access = RuntimePointAccess::ReadOnly;
        point.addressing.insert(QStringLiteral("registerType"), QStringLiteral("input"));
        point.addressing.insert(QStringLiteral("regAddress"), 27);
        point.addressing.insert(QStringLiteral("bitIndex"), 3);
        point.addressing.insert(QStringLiteral("length"), 2);
        point.addressing.insert(QStringLiteral("opcChannel"), QStringLiteral("CommPort"));
        point.addressing.insert(QStringLiteral("opcDevice"), QStringLiteral("InitDevParamnt"));
        point.opcTagName = QStringLiteral("legacy.input.bit");

        const OpcTagDefinition tag = RuntimePointConverter::runtimePointToOpcTag(point);
        const QStringList candidates = server.itemIdCandidatesForTag(tag);

        QVERIFY2(candidates.contains(QStringLiteral("3:27")), qPrintable(candidates.join(QStringLiteral(","))));
        QVERIFY(candidates.contains(QStringLiteral("3:27:2")));
        QVERIFY(candidates.contains(QStringLiteral("3:27/3")));
        QVERIFY(candidates.contains(QStringLiteral("CommPort.InitDevParamnt.3:27/3")));
        QVERIFY(candidates.contains(QStringLiteral("CommPort.InitDevParamnt.3:27:2")));
    }

    void runtimePointMetadataPassesThroughLegacyOpcAddressFields()
    {
        VariableDefinition variable = VariableDefinition::makeTemplate(QStringLiteral("legacy_register"),
                                                                       QStringLiteral("UINT16"));
        variable.metadata.insert(QStringLiteral("regAddress"), 42);
        variable.metadata.insert(QStringLiteral("bitIndex"), 1);
        variable.metadata.insert(QStringLiteral("length"), 4);
        variable.metadata.insert(QStringLiteral("registerType"), QStringLiteral("holding"));

        const RuntimePointDefinition point = RuntimePointConverter::fromVariable(variable);

        QCOMPARE(point.addressing.value(QStringLiteral("regAddress")).toInt(), 42);
        QCOMPARE(point.addressing.value(QStringLiteral("bitIndex")).toInt(), 1);
        QCOMPARE(point.addressing.value(QStringLiteral("length")).toInt(), 4);
        QCOMPARE(point.addressing.value(QStringLiteral("registerType")).toString(),
                 QStringLiteral("holding"));
    }

    void matrikonBackendReportsConfigurationProbe()
    {
        MatrikonOpcServer server;
        server.m_browsedItemIds = QStringList {
            QStringLiteral("CommPort.InitDevParamnt.#Enabled"),
            QStringLiteral("CommPort.InitDevParamnt.4:10"),
            QStringLiteral("CommPort.InitDevConfig.@Connected")
        };
        server.updateConfigurationProbe();

        const BackendStatusSnapshot snapshot = server.statusSnapshot();
        QVERIFY(snapshot.extras.value(QStringLiteral("expectedChannelVisible")).toBool());
        QVERIFY(snapshot.extras.value(QStringLiteral("primaryDeviceVisible")).toBool());
        QVERIFY(snapshot.extras.value(QStringLiteral("secondaryDeviceVisible")).toBool());
        QVERIFY(snapshot.extras.value(QStringLiteral("standardItemsVisible")).toBool());
        QVERIFY(snapshot.extras.value(QStringLiteral("configurationProbeOk")).toBool());
        QVERIFY(snapshot.extras.value(QStringLiteral("configurationProbeMessage")).toString()
                .contains(QStringLiteral("channel=visible")));
    }

    void matrikonBackendBuildsReadProbeCandidates()
    {
        MatrikonOpcServer server;
        server.m_browsedItemIds = QStringList {
            QStringLiteral("CommPort.InitDevParamnt.#Enabled"),
            QStringLiteral("CommPort.InitDevConfig.@Connected")
        };

        const QStringList candidates = server.readProbeItemCandidates();

        QVERIFY(candidates.contains(QStringLiteral("CommPort.InitDevParamnt.#Enabled")));
        QVERIFY(candidates.contains(QStringLiteral("CommPort.InitDevConfig.@Connected")));
        QVERIFY(candidates.contains(QStringLiteral("#Enabled")));
    }

    void matrikonBackendReadProbeFailsSafelyWithoutSyncIo()
    {
        MatrikonOpcServer server;

        QString errorMessage;
        QVERIFY(!server.runReadProbe(&errorMessage));

        const BackendStatusSnapshot snapshot = server.statusSnapshot();
        QVERIFY(snapshot.extras.value(QStringLiteral("readProbeAttempted")).toBool());
        QVERIFY(!snapshot.extras.value(QStringLiteral("readProbeOk")).toBool());
        QVERIFY(!snapshot.extras.value(QStringLiteral("readProbeMessage")).toString().isEmpty());
        QVERIFY(!errorMessage.isEmpty());
    }

    void matrikonBackendWriteFailureUpdatesStatus()
    {
        MatrikonOpcServer server;

        RuntimePointDefinition point;
        point.id = QStringLiteral("param.write");
        point.name = QStringLiteral("Write Parameter");
        point.kind = RuntimePointKind::Parameter;
        point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadWrite;
        point.opcItemName = QStringLiteral("CommPort.InitDevParamnt.4:20");

        server.setRuntimePoints({point});

        QString errorMessage;
        QVERIFY(!server.writeItemValue(point.id, 12.5, &errorMessage));

        const BackendStatusSnapshot snapshot = server.statusSnapshot();
        QCOMPARE(snapshot.extras.value(QStringLiteral("failedWriteCount")).toInt(), 1);
        QCOMPARE(snapshot.extras.value(QStringLiteral("successfulWriteCount")).toInt(), 0);
        QCOMPARE(snapshot.extras.value(QStringLiteral("lastWritePointId")).toString(), point.id);
        QCOMPARE(snapshot.extras.value(QStringLiteral("lastWriteValue")).toDouble(), 12.5);
        QCOMPARE(snapshot.extras.value(QStringLiteral("lastWriteSuccess")).toBool(), false);
        QCOMPARE(snapshot.extras.value(QStringLiteral("lastFailedWriteMessage")).toString(), errorMessage);
        QVERIFY(!snapshot.extras.value(QStringLiteral("lastFailedWriteTime")).toString().isEmpty());
        QVERIFY(!errorMessage.isEmpty());
    }

#ifdef Q_OS_WIN
    void matrikonBackendDataCallbackUpdatesValueQualityAndTimestamp()
    {
        MatrikonOpcServer server;

        RuntimePointDefinition point;
        point.id = QStringLiteral("status.feedback");
        point.name = QStringLiteral("Feedback");
        point.kind = RuntimePointKind::Status;
        point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadOnly;
        point.opcItemName = QStringLiteral("CommPort.InitDevParamnt.4:21");

        server.setRuntimePoints({point});

        MatrikonOpcServer::ItemBinding binding;
        binding.pointId = point.id;
        binding.itemId = point.opcItemName;
        binding.clientHandle = 42;
        binding.serverHandle = 1001;
        binding.active = true;
        server.m_itemsByPointId.insert(point.id, binding);
        server.m_pointIdByClientHandle.insert(binding.clientHandle, point.id);

        VARIANT values[1];
        VariantInit(&values[0]);
        values[0].vt = VT_R8;
        values[0].dblVal = 18.25;

        SYSTEMTIME systemTime = {};
        systemTime.wYear = 2026;
        systemTime.wMonth = 6;
        systemTime.wDay = 10;
        systemTime.wHour = 8;
        systemTime.wMinute = 30;
        systemTime.wSecond = 15;

        FILETIME fileTimes[1] = {};
        QVERIFY(SystemTimeToFileTime(&systemTime, &fileTimes[0]));

        const unsigned long clientHandles[1] = {binding.clientHandle};
        const unsigned short qualities[1] = {0xC0};
        const long errors[1] = {S_OK};

        server.handleDataChange(1,
                                0,
                                0,
                                S_OK,
                                1,
                                clientHandles,
                                values,
                                qualities,
                                fileTimes,
                                errors);

        const RuntimePointValue current = server.m_values.value(point.id);
        QCOMPARE(current.pointId, point.id);
        QCOMPARE(current.value.toDouble(), 18.25);
        QCOMPARE(current.quality, RuntimePointQuality::Good);
        QCOMPARE(current.origin, QStringLiteral("opc-da-callback"));
        QCOMPARE(current.timestamp,
                 QDateTime(QDate(2026, 6, 10), QTime(8, 30, 15), Qt::UTC));

        const BackendStatusSnapshot snapshot = server.statusSnapshot();
        QCOMPARE(snapshot.extras.value(QStringLiteral("valueCount")).toInt(), 1);
        QCOMPARE(snapshot.extras.value(QStringLiteral("callbackCount")).toInt(), 1);
        QCOMPARE(snapshot.extras.value(QStringLiteral("lastReadPointId")).toString(), point.id);
        QCOMPARE(snapshot.extras.value(QStringLiteral("lastReadItemId")).toString(), point.opcItemName);
        QCOMPARE(snapshot.extras.value(QStringLiteral("lastQuality")).toUInt(), 0xC0u);
        QCOMPARE(snapshot.extras.value(QStringLiteral("lastQualityText")).toString(), QStringLiteral("Good(0x00C0)"));
        QCOMPARE(snapshot.extras.value(QStringLiteral("lastTimestampSource")).toString(), QStringLiteral("opc-da"));
        QVERIFY(!snapshot.extras.value(QStringLiteral("lastCallbackTime")).toString().isEmpty());

        VariantClear(&values[0]);
    }

    void matrikonBackendRejectsCallbackMasterItemAndNullFailures()
    {
        MatrikonOpcServer server;

        RuntimePointDefinition point;
        point.id = QStringLiteral("status.invalid-callback");
        point.name = QStringLiteral("Invalid Callback");
        point.kind = RuntimePointKind::Status;
        point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadOnly;
        point.opcItemName = QStringLiteral("CommPort.InitDevParamnt.4:24");
        server.setRuntimePoints({point});

        MatrikonOpcServer::ItemBinding binding;
        binding.pointId = point.id;
        binding.itemId = point.opcItemName;
        binding.clientHandle = 45;
        binding.serverHandle = 1004;
        binding.active = true;
        server.m_itemsByPointId.insert(point.id, binding);
        server.m_pointIdByClientHandle.clear();
        server.m_pointIdByClientHandle.insert(binding.clientHandle, point.id);

        VARIANT value;
        VariantInit(&value);
        value.vt = VT_R8;
        value.dblVal = 18.25;
        const unsigned long clientHandles[1] = {binding.clientHandle};
        const unsigned short qualities[1] = {0x40};
        long itemErrors[1] = {S_OK};

        auto dispatch = [&](long masterQuality, long masterError) {
            server.handleDataChange(4,
                                    0,
                                    masterQuality,
                                    masterError,
                                    1,
                                    clientHandles,
                                    &value,
                                    qualities,
                                    nullptr,
                                    itemErrors);
        };

        dispatch(S_OK, S_OK);
        QCOMPARE(server.m_values.value(point.id).quality, RuntimePointQuality::Stale);

        dispatch(S_OK, E_FAIL);
        QVERIFY(!server.m_values.contains(point.id));
        QVERIFY(server.m_lastReadMessage.contains(QStringLiteral("master status")));

        itemErrors[0] = E_FAIL;
        dispatch(S_OK, S_OK);
        QVERIFY(!server.m_values.contains(point.id));
        QVERIFY(server.m_lastReadMessage.contains(QStringLiteral("item failed")));

        itemErrors[0] = S_OK;
        VariantClear(&value);
        VariantInit(&value);
        value.vt = VT_EMPTY;
        dispatch(S_OK, S_OK);
        QVERIFY(!server.m_values.contains(point.id));
        QVERIFY(server.m_lastReadMessage.contains(QStringLiteral("null or unsupported")));
        QCOMPARE(server.m_failedReadCount, 3);
        QVERIFY(server.statusSnapshot().lastErrorMessage.contains(QStringLiteral("null or unsupported")));

        VariantClear(&value);
    }

    void matrikonBackendSupportsOpcVariantTypes()
    {
        VARIANT variant;
        VariantInit(&variant);

        variant.vt = VT_I8;
        variant.llVal = 1234567890123LL;
        QCOMPARE(MatrikonOpcServer::variantToQVariant(&variant).toLongLong(), 1234567890123LL);

        variant.vt = VT_UI8;
        variant.ullVal = 1234567890123ULL;
        QCOMPARE(MatrikonOpcServer::variantToQVariant(&variant).toULongLong(), 1234567890123ULL);

        variant.vt = VT_R4;
        variant.fltVal = 1.25f;
        QCOMPARE(MatrikonOpcServer::variantToQVariant(&variant).toFloat(), 1.25f);

        variant.vt = VT_EMPTY;
        QVERIFY(!MatrikonOpcServer::variantToQVariant(&variant).isValid());

        QString errorMessage;
        QVERIFY(MatrikonOpcServer::setVariantValue(QVariant::fromValue<qlonglong>(42),
                                                   &variant,
                                                   &errorMessage));
        QCOMPARE(variant.vt, static_cast<VARTYPE>(VT_I8));
        QCOMPARE(variant.llVal, 42LL);
        QVERIFY(errorMessage.isEmpty());

        QVERIFY(!MatrikonOpcServer::setVariantValue(QVariant(), &variant, &errorMessage));
        QVERIFY(errorMessage.contains(QStringLiteral("null value")));

        VariantClear(&variant);
    }

    void matrikonBackendQueuesCallbackToOwnerThread()
    {
        MatrikonOpcServer server;

        RuntimePointDefinition point;
        point.id = QStringLiteral("status.queued");
        point.name = QStringLiteral("Queued Feedback");
        point.kind = RuntimePointKind::Status;
        point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadOnly;
        point.opcItemName = QStringLiteral("CommPort.InitDevParamnt.4:22");
        server.setRuntimePoints({point});

        MatrikonOpcServer::ItemBinding binding;
        binding.pointId = point.id;
        binding.itemId = point.opcItemName;
        binding.clientHandle = 43;
        binding.serverHandle = 1002;
        binding.active = true;
        server.m_itemsByPointId.insert(point.id, binding);
        server.m_pointIdByClientHandle.insert(binding.clientHandle, point.id);
        server.activateDataCallbackContext();

        VARIANT values[1];
        VariantInit(&values[0]);
        values[0].vt = VT_R8;
        values[0].dblVal = 21.5;
        const unsigned long clientHandles[1] = {binding.clientHandle};
        const unsigned short qualities[1] = {0xC0};
        const long errors[1] = {S_OK};

        QThread callbackThread;
        QObject::connect(&callbackThread, &QThread::started, &callbackThread,
                         [&server, &callbackThread, &clientHandles, &values, &qualities, &errors]() {
                             server.enqueueDataChange(2,
                                                       0,
                                                       0,
                                                       S_OK,
                                                       1,
                                                       clientHandles,
                                                       values,
                                                       qualities,
                                                       nullptr,
                                                       errors);
                             callbackThread.quit();
                         },
                         Qt::DirectConnection);
        callbackThread.start();
        QVERIFY(callbackThread.wait(500));

        QVERIFY(!server.m_values.contains(point.id));
        values[0].dblVal = -7.0;
        QTRY_VERIFY_WITH_TIMEOUT(server.m_values.contains(point.id), 500);
        QCOMPARE(server.m_values.value(point.id).value.toDouble(), 21.5);
        QCOMPARE(server.m_values.value(point.id).origin, QStringLiteral("opc-da-callback"));
        QCOMPARE(server.m_callbackCount, 1);
        QVERIFY(server.thread() == QThread::currentThread());

        VariantClear(&values[0]);
    }

    void matrikonBackendStopDropsQueuedCallback()
    {
        MatrikonOpcServer server;

        RuntimePointDefinition point;
        point.id = QStringLiteral("status.stopped");
        point.name = QStringLiteral("Stopped Feedback");
        point.kind = RuntimePointKind::Status;
        point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadOnly;
        point.opcItemName = QStringLiteral("CommPort.InitDevParamnt.4:23");
        server.setRuntimePoints({point});

        MatrikonOpcServer::ItemBinding binding;
        binding.pointId = point.id;
        binding.itemId = point.opcItemName;
        binding.clientHandle = 44;
        binding.serverHandle = 1003;
        binding.active = true;
        server.m_itemsByPointId.insert(point.id, binding);
        server.m_pointIdByClientHandle.insert(binding.clientHandle, point.id);
        server.activateDataCallbackContext();

        VARIANT values[1];
        VariantInit(&values[0]);
        values[0].vt = VT_R8;
        values[0].dblVal = 99.0;
        const unsigned long clientHandles[1] = {binding.clientHandle};
        const unsigned short qualities[1] = {0xC0};
        const long errors[1] = {S_OK};

        QThread callbackThread;
        QObject::connect(&callbackThread, &QThread::started, &callbackThread,
                         [&server, &callbackThread, &clientHandles, &values, &qualities, &errors]() {
                             server.enqueueDataChange(3,
                                                       0,
                                                       0,
                                                       S_OK,
                                                       1,
                                                       clientHandles,
                                                       values,
                                                       qualities,
                                                       nullptr,
                                                       errors);
                             callbackThread.quit();
                         },
                         Qt::DirectConnection);
        callbackThread.start();
        QVERIFY(callbackThread.wait(500));

        server.stop();
        QCoreApplication::processEvents();
        QVERIFY(!server.m_values.contains(point.id));
        QCOMPARE(server.m_callbackCount, 0);

        VariantClear(&values[0]);
    }
#endif

    void matrikonBackendMapsOpcQuality()
    {
        QCOMPARE(MatrikonOpcServer::qualityToRuntimeQuality(0xC0), RuntimePointQuality::Good);
        QCOMPARE(MatrikonOpcServer::qualityToRuntimeQuality(0x40), RuntimePointQuality::Stale);
        QCOMPARE(MatrikonOpcServer::qualityToRuntimeQuality(0x00), RuntimePointQuality::Bad);
        QVERIFY(MatrikonOpcServer::qualityToString(0xC0).startsWith(QStringLiteral("Good")));
        QVERIFY(MatrikonOpcServer::qualityToString(0x40).startsWith(QStringLiteral("Uncertain")));
        QVERIFY(MatrikonOpcServer::qualityToString(0x00).startsWith(QStringLiteral("Bad")));
    }

    void classicBackendCachesValuesAndCountsWrites()
    {
        ClassicOpcServer server;

        RuntimePointDefinition point;
        point.id = QStringLiteral("param.kp");
        point.name = QStringLiteral("Kp");
        point.kind = RuntimePointKind::Parameter;
        point.dataType = QStringLiteral("REAL");
        point.access = RuntimePointAccess::ReadWrite;
        point.addressing.insert(QStringLiteral("area"), QStringLiteral("holding"));
        point.addressing.insert(QStringLiteral("address"), 12);
        point.addressing.insert(QStringLiteral("unitId"), 1);
        point.opcTagName = QStringLiteral("Kp");
        point.opcTagGroup = QStringLiteral("Parameters");

        server.setRuntimePoints({point});

        RuntimePointValue value;
        value.pointId = point.id;
        value.value = 3.5;
        value.quality = RuntimePointQuality::Good;
        value.timestamp = QDateTime::currentDateTimeUtc();
        value.origin = QStringLiteral("opc-write");

        server.updatePointValues({value});
        server.recordWriteResult(point.id, true, QStringLiteral("write ok"));

        const BackendStatusSnapshot snapshot = server.statusSnapshot();
        QCOMPARE(snapshot.backendType, QStringLiteral("classic-modbus"));
        QCOMPARE(snapshot.extras.value(QStringLiteral("valueCount")).toInt(), 1);
        QCOMPARE(snapshot.extras.value(QStringLiteral("successfulWriteCount")).toInt(), 1);
        QCOMPARE(snapshot.extras.value(QStringLiteral("failedWriteCount")).toInt(), 0);
        QCOMPARE(snapshot.extras.value(QStringLiteral("lastWritePointId")).toString(), point.id);
        QCOMPARE(snapshot.extras.value(QStringLiteral("lastWriteSuccess")).toBool(), true);
    }

    void classicBackendResolvesAliasAddressing()
    {
        ClassicOpcServer server;

        RuntimePointDefinition point;
        point.id = QStringLiteral("param.alias");
        point.name = QStringLiteral("Alias");
        point.kind = RuntimePointKind::Parameter;
        point.dataType = QStringLiteral("UINT16");
        point.access = RuntimePointAccess::ReadOnly;
        point.addressing.insert(QStringLiteral("registerType"), QStringLiteral("input"));
        point.addressing.insert(QStringLiteral("regAddress"), 27);
        point.addressing.insert(QStringLiteral("slaveId"), 3);
        point.addressing.insert(QStringLiteral("bitIndex"), 1);
        point.addressing.insert(QStringLiteral("length"), 2);

        server.setRuntimePoints({point});

        const BackendStatusSnapshot snapshot = server.statusSnapshot();
        QCOMPARE(snapshot.extras.value(QStringLiteral("addressedPointCount")).toInt(), 1);
        QCOMPARE(snapshot.extras.value(QStringLiteral("unresolvedPointCount")).toInt(), 0);
    }
};

QTEST_MAIN(OpcServerConfigTest)
#include "opc_server_config_test.moc"
