#include "MatrikonOpcServer.h"

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <ocidl.h>
#endif

#include <QDateTime>
#include <QMutex>
#include <QSet>
#include <QVarLengthArray>

#include <utility>

namespace {

bool hasOpcPayload(const QVariant& value)
{
    return value.isValid() && !value.isNull();
}

QString opcSubscriptionMode(const OpcServerConfig& config)
{
    const QString mode = config.metadata.value(QStringLiteral("subscriptionMode"))
                                 .toString().trimmed().toLower();
    if (mode == QStringLiteral("disabled")
            || mode == QStringLiteral("off")
            || mode == QStringLiteral("none")) {
        return QStringLiteral("disabled");
    }
    if (mode == QStringLiteral("required")
            || mode == QStringLiteral("mandatory")) {
        return QStringLiteral("required");
    }
    if (config.metadata.contains(QStringLiteral("subscriptionRequired"))) {
        return config.metadata.value(QStringLiteral("subscriptionRequired")).toBool()
                ? QStringLiteral("required")
                : QStringLiteral("optional");
    }
    return QStringLiteral("optional");
}

bool subscriptionIsEnabled(const OpcServerConfig& config)
{
    return opcSubscriptionMode(config) != QStringLiteral("disabled");
}

} // namespace

#ifdef Q_OS_WIN
using OPCHANDLE = DWORD;

enum OPCDATASOURCE
{
    OPC_DS_CACHE = 1,
    OPC_DS_DEVICE = 2
};

enum OPCNAMESPACETYPE
{
    OPC_NS_HIERARCHIAL = 1,
    OPC_NS_FLAT = 2
};

enum OPCBROWSEDIRECTION
{
    OPC_BROWSE_UP = 1,
    OPC_BROWSE_DOWN = 2,
    OPC_BROWSE_TO = 3
};

enum OPCBROWSETYPE
{
    OPC_BRANCH = 1,
    OPC_LEAF = 2,
    OPC_FLAT = 3
};

struct OPCITEMDEF
{
    LPWSTR szAccessPath;
    LPWSTR szItemID;
    BOOL bActive;
    OPCHANDLE hClient;
    DWORD dwBlobSize;
    BYTE* pBlob;
    VARTYPE vtRequestedDataType;
    WORD wReserved;
};

struct OPCITEMRESULT
{
    OPCHANDLE hServer;
    VARTYPE vtCanonicalDataType;
    WORD wReserved;
    DWORD dwAccessRights;
    DWORD dwBlobSize;
    BYTE* pBlob;
};

struct OPCITEMSTATE
{
    OPCHANDLE hClient;
    FILETIME ftTimeStamp;
    WORD wQuality;
    WORD wReserved;
    VARIANT vDataValue;
};

struct IOPCServer : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE AddGroup(LPCWSTR szName,
                                              BOOL bActive,
                                              DWORD dwRequestedUpdateRate,
                                              OPCHANDLE hClientGroup,
                                              LONG* pTimeBias,
                                              FLOAT* pPercentDeadband,
                                              DWORD dwLCID,
                                              OPCHANDLE* phServerGroup,
                                              DWORD* pRevisedUpdateRate,
                                              REFIID riid,
                                              LPUNKNOWN* ppUnk) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetErrorString(HRESULT dwError, LCID dwLocale, LPWSTR* ppString) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetGroupByName(LPCWSTR szName, REFIID riid, LPUNKNOWN* ppUnk) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetStatus(void** ppServerStatus) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveGroup(OPCHANDLE hServerGroup, BOOL bForce) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateGroupEnumerator(DWORD dwScope, REFIID riid, LPUNKNOWN* ppUnk) = 0;
};

struct IOPCItemMgt : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE AddItems(DWORD dwCount,
                                              OPCITEMDEF* pItemArray,
                                              OPCITEMRESULT** ppAddResults,
                                              HRESULT** ppErrors) = 0;
    virtual HRESULT STDMETHODCALLTYPE ValidateItems(DWORD dwCount,
                                                   OPCITEMDEF* pItemArray,
                                                   BOOL bBlobUpdate,
                                                   OPCITEMRESULT** ppValidationResults,
                                                   HRESULT** ppErrors) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveItems(DWORD dwCount,
                                                 OPCHANDLE* phServer,
                                                 HRESULT** ppErrors) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetActiveState(DWORD dwCount,
                                                    OPCHANDLE* phServer,
                                                    BOOL bActive,
                                                    HRESULT** ppErrors) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetClientHandles(DWORD dwCount,
                                                      OPCHANDLE* phServer,
                                                      OPCHANDLE* phClient,
                                                      HRESULT** ppErrors) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDatatypes(DWORD dwCount,
                                                  OPCHANDLE* phServer,
                                                  VARTYPE* pRequestedDatatypes,
                                                  HRESULT** ppErrors) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateEnumerator(REFIID riid, LPUNKNOWN* ppUnk) = 0;
};

struct IOPCSyncIO : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE Read(OPCDATASOURCE dwSource,
                                          DWORD dwCount,
                                          OPCHANDLE* phServer,
                                          OPCITEMSTATE** ppItemValues,
                                          HRESULT** ppErrors) = 0;
    virtual HRESULT STDMETHODCALLTYPE Write(DWORD dwCount,
                                           OPCHANDLE* phServer,
                                           VARIANT* pItemValues,
                                           HRESULT** ppErrors) = 0;
};

struct IOPCBrowseServerAddressSpace : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE QueryOrganization(OPCNAMESPACETYPE* pNameSpaceType) = 0;
    virtual HRESULT STDMETHODCALLTYPE ChangeBrowsePosition(OPCBROWSEDIRECTION dwBrowseDirection,
                                                           LPCWSTR szString) = 0;
    virtual HRESULT STDMETHODCALLTYPE BrowseOPCItemIDs(OPCBROWSETYPE dwBrowseFilterType,
                                                       LPCWSTR szFilterCriteria,
                                                       VARTYPE vtDataTypeFilter,
                                                       DWORD dwAccessRightsFilter,
                                                       LPENUMSTRING* ppIEnumString) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetItemID(LPWSTR szItemDataID, LPWSTR* szItemID) = 0;
    virtual HRESULT STDMETHODCALLTYPE BrowseAccessPaths(LPCWSTR szItemID, LPENUMSTRING* ppIEnumString) = 0;
};

struct IOPCDataCallback : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE OnDataChange(DWORD dwTransid,
                                                  OPCHANDLE hGroup,
                                                  HRESULT hrMasterquality,
                                                  HRESULT hrMastererror,
                                                  DWORD dwCount,
                                                  OPCHANDLE* phClientItems,
                                                  VARIANT* pvValues,
                                                  WORD* pwQualities,
                                                  FILETIME* pftTimeStamps,
                                                  HRESULT* pErrors) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnReadComplete(DWORD dwTransid,
                                                    OPCHANDLE hGroup,
                                                    HRESULT hrMasterquality,
                                                    HRESULT hrMastererror,
                                                    DWORD dwCount,
                                                    OPCHANDLE* phClientItems,
                                                    VARIANT* pvValues,
                                                    WORD* pwQualities,
                                                    FILETIME* pftTimeStamps,
                                                    HRESULT* pErrors) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnWriteComplete(DWORD dwTransid,
                                                     OPCHANDLE hGroup,
                                                     HRESULT hrMastererr,
                                                     DWORD dwCount,
                                                     OPCHANDLE* pClienthandles,
                                                     HRESULT* pErrors) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnCancelComplete(DWORD dwTransid, OPCHANDLE hGroup) = 0;
};

// OPC DA 2.0 interface IDs. The project intentionally keeps a tiny local subset
// because the Core Components redistributable installs runtime DLLs, not SDK headers.
static const IID IID_IOPCServer_LH = {0x39c13a4d, 0x011e, 0x11d0, {0x96, 0x75, 0x00, 0x20, 0xaf, 0xd8, 0xad, 0xb3}};
static const IID IID_IOPCBrowseServerAddressSpace_LH = {0x39c13a4f, 0x011e, 0x11d0, {0x96, 0x75, 0x00, 0x20, 0xaf, 0xd8, 0xad, 0xb3}};
static const IID IID_IOPCItemMgt_LH = {0x39c13a54, 0x011e, 0x11d0, {0x96, 0x75, 0x00, 0x20, 0xaf, 0xd8, 0xad, 0xb3}};
static const IID IID_IOPCSyncIO_LH = {0x39c13a52, 0x011e, 0x11d0, {0x96, 0x75, 0x00, 0x20, 0xaf, 0xd8, 0xad, 0xb3}};
static const IID IID_IOPCDataCallback_LH = {0x39c13a70, 0x011e, 0x11d0, {0x96, 0x75, 0x00, 0x20, 0xaf, 0xd8, 0xad, 0xb3}};
#endif

#ifdef Q_OS_WIN
class MatrikonOpcDataCallback final : public IOPCDataCallback
{
public:
    explicit MatrikonOpcDataCallback(
            const std::shared_ptr<MatrikonOpcServer::CallbackContext>& context)
        : m_context(context)
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
    {
        if (!ppvObject) {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == IID_IOPCDataCallback_LH) {
            *ppvObject = static_cast<IOPCDataCallback*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return InterlockedIncrement(&m_refCount);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) {
            delete this;
        }
        return count;
    }

    HRESULT STDMETHODCALLTYPE OnDataChange(DWORD dwTransid,
                                          OPCHANDLE hGroup,
                                          HRESULT hrMasterquality,
                                          HRESULT hrMastererror,
                                          DWORD dwCount,
                                          OPCHANDLE* phClientItems,
                                          VARIANT* pvValues,
                                          WORD* pwQualities,
                                          FILETIME* pftTimeStamps,
                                          HRESULT* pErrors) override
    {
        const auto context = m_context;
        if (!context) {
            return S_OK;
        }

        QMutexLocker locker(&context->mutex);
        if (!context->active || !context->owner) {
            return S_OK;
        }

        context->owner->enqueueDataChange(context,
                                          context->generation,
                                          dwTransid,
                                          hGroup,
                                          hrMasterquality,
                                          hrMastererror,
                                          dwCount,
                                          phClientItems,
                                          pvValues,
                                          pwQualities,
                                          pftTimeStamps,
                                          pErrors);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnReadComplete(DWORD dwTransid,
                                            OPCHANDLE hGroup,
                                            HRESULT hrMasterquality,
                                            HRESULT hrMastererror,
                                            DWORD dwCount,
                                            OPCHANDLE* phClientItems,
                                            VARIANT* pvValues,
                                            WORD* pwQualities,
                                            FILETIME* pftTimeStamps,
                                            HRESULT* pErrors) override
    {
        return OnDataChange(dwTransid,
                            hGroup,
                            hrMasterquality,
                            hrMastererror,
                            dwCount,
                            phClientItems,
                            pvValues,
                            pwQualities,
                            pftTimeStamps,
                            pErrors);
    }

    HRESULT STDMETHODCALLTYPE OnWriteComplete(DWORD,
                                             OPCHANDLE,
                                             HRESULT,
                                             DWORD,
                                             OPCHANDLE*,
                                             HRESULT*) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnCancelComplete(DWORD, OPCHANDLE) override
    {
        return S_OK;
    }

private:
    volatile LONG m_refCount = 1;
    std::shared_ptr<MatrikonOpcServer::CallbackContext> m_context;
};
#endif

MatrikonOpcServer::MatrikonOpcServer(QObject* parent)
    : IOpcServer(parent)
    , m_callbackContext(std::make_shared<CallbackContext>())
{
    m_callbackContext->owner = this;
}

MatrikonOpcServer::~MatrikonOpcServer()
{
    invalidateDataCallbackContext();
    stop();
#ifdef Q_OS_WIN
    if (m_comInitialized) {
        CoUninitialize();
        m_comInitialized = false;
    }
#endif
}

bool MatrikonOpcServer::applyConfig(const OpcServerConfig& config, QString* errorMessage)
{
    if (!validateConfig(config, errorMessage)) {
        m_lastErrorCode = CommErrorCode::InvalidConfig;
        m_lastErrorMessage = errorMessage ? *errorMessage : QStringLiteral("OPC DA config invalid");
        emit errorOccurred(m_lastErrorMessage);
        return false;
    }

    const bool restart = m_running && config.opcProgId != m_config.opcProgId;
    if (restart) {
        stop();
    }

    m_config = config;
    m_lastErrorCode = CommErrorCode::NoError;
    m_lastErrorMessage.clear();
    return true;
}

bool MatrikonOpcServer::start(QString* errorMessage)
{
    if (!validateConfig(m_config, errorMessage)) {
        m_lastErrorCode = CommErrorCode::InvalidConfig;
        m_lastErrorMessage = errorMessage ? *errorMessage : QStringLiteral("OPC DA config invalid");
        emit errorOccurred(m_lastErrorMessage);
        return false;
    }

    if (m_running) {
        if (errorMessage) {
            errorMessage->clear();
        }
        return true;
    }

    if (m_server || m_opcServer || m_groupUnknown || m_itemMgt || m_syncIO || m_subscriptionActive) {
        stop();
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    m_lastErrorCode = CommErrorCode::NoError;
    m_lastErrorMessage.clear();
    m_startupIssues.clear();
    m_browseAttempted = false;
    m_browseSucceeded = false;
    m_browsedItemIds.clear();
    m_lastBrowseMessage.clear();
    m_addItemsAttempted = false;
    m_refreshAttempted = false;
    m_refreshOk = false;
    m_lastRefreshMessage.clear();
    m_subscriptionAttempted = false;
    m_lastSubscriptionMessage.clear();
    m_expectedChannelVisible = false;
    m_primaryDeviceVisible = false;
    m_secondaryDeviceVisible = false;
    m_standardItemsVisible = false;
    m_configurationProbeMessage.clear();

    auto noteIssue = [&](CommErrorCode code, const QString& message) {
        if (message.trimmed().isEmpty()) {
            return;
        }
        m_startupIssues.append(message);
        if (m_lastErrorMessage.isEmpty()) {
            m_lastErrorCode = code;
            m_lastErrorMessage = message;
        }
    };

    auto failCoreStage = [&](const QString& message, const QString& fallback) {
        const QString finalMessage = message.trimmed().isEmpty() ? fallback : message;
        m_lastErrorCode = CommErrorCode::ConnectionFailed;
        m_lastErrorMessage = finalMessage;
        m_startupIssues.append(finalMessage);
        if (errorMessage) {
            *errorMessage = finalMessage;
        }
        emit errorOccurred(m_lastErrorMessage);
        stop();
        return false;
    };

    QString phaseError;
    if (!ensureComInitialized(&phaseError)) {
        return failCoreStage(phaseError, QStringLiteral("Failed to initialize COM"));
    }
    phaseError.clear();
    if (!createServerInstance(&phaseError)) {
        return failCoreStage(phaseError, QStringLiteral("Failed to create Matrikon OPC server"));
    }
    phaseError.clear();
    if (!createOpcGroup(&phaseError)) {
        return failCoreStage(phaseError, QStringLiteral("Failed to create Matrikon OPC group"));
    }

    QString browseError;
    m_browseAttempted = true;
    if (!browseServerAddressSpace(&browseError)) {
        noteIssue(CommErrorCode::ConnectionFailed,
                  QStringLiteral("OPC browse failed: %1")
                          .arg(browseError.isEmpty() ? m_lastBrowseMessage : browseError));
    }
    remapItemsFromBrowseResults();

    QString itemError;
    if (!addPendingItems(&itemError)) {
        noteIssue(CommErrorCode::InvalidAddress,
                  QStringLiteral("OPC item setup failed: %1")
                          .arg(itemError.isEmpty() ? QStringLiteral("configured item is unavailable")
                                                   : itemError));
    }

    QString readError;
    if (!refreshItems(&readError)) {
        noteIssue(CommErrorCode::ConnectionFailed,
                  QStringLiteral("OPC initial read failed: %1")
                          .arg(readError.isEmpty() ? m_lastRefreshMessage : readError));
    }

    QString probeError;
    if (!runReadProbe(&probeError)) {
        noteIssue(CommErrorCode::ConnectionFailed,
                  QStringLiteral("OPC read probe failed: %1")
                          .arg(probeError.isEmpty() ? m_readProbeMessage : probeError));
    }

    if (subscriptionIsEnabled(m_config)) {
        m_subscriptionAttempted = true;
        QString subscriptionError;
        if (!subscribeDataChanges(&subscriptionError)) {
            noteIssue(CommErrorCode::ConnectionFailed,
                      QStringLiteral("OPC subscription failed: %1")
                              .arg(subscriptionError.isEmpty() ? m_lastSubscriptionMessage
                                                               : subscriptionError));
        }
    } else {
        m_lastSubscriptionMessage = QStringLiteral("subscription disabled by configuration");
    }

    m_running = true;
    m_lastStatusChangeTime = QDateTime::currentDateTimeUtc();

    if (!m_startupIssues.isEmpty()) {
        if (errorMessage) {
            *errorMessage = m_startupIssues.join(QStringLiteral("; "));
        }
        emit errorOccurred(m_lastErrorMessage);
    } else if (errorMessage) {
        errorMessage->clear();
    }

    emit runningStateChanged(true);
    return true;
}

void MatrikonOpcServer::stop()
{
    const bool wasRunning = m_running;
    const bool hadResources = m_server != nullptr || m_groupUnknown != nullptr
            || m_subscriptionActive;
    releaseOpcGroup();
    releaseServerInstance();
    m_running = false;
    m_lastStatusChangeTime = QDateTime::currentDateTimeUtc();
    if (wasRunning || hadResources) {
        emit runningStateChanged(false);
    }
}

void MatrikonOpcServer::setRuntimePoints(const QList<RuntimePointDefinition>& points)
{
    m_points = points;
    rebuildPendingItems();
    if (m_running) {
        QString itemError;
        if (!addPendingItems(&itemError) && !itemError.isEmpty()) {
            m_lastErrorCode = CommErrorCode::InvalidAddress;
            m_lastErrorMessage = itemError;
            emit errorOccurred(m_lastErrorMessage);
        }
        QString readError;
        if (!refreshItems(&readError) && !readError.isEmpty()) {
            m_lastErrorCode = CommErrorCode::ConnectionFailed;
            m_lastErrorMessage = readError;
            emit errorOccurred(m_lastErrorMessage);
        }
        QString probeError;
        if (!runReadProbe(&probeError) && !probeError.isEmpty()) {
            m_lastErrorCode = CommErrorCode::ConnectionFailed;
            m_lastErrorMessage = probeError;
            emit errorOccurred(m_lastErrorMessage);
        }
    }
}

void MatrikonOpcServer::setOpcTags(const QList<OpcTagDefinition>& tags)
{
    m_tags = tags;
    rebuildPendingItems();
    if (m_running) {
        QString itemError;
        if (!addPendingItems(&itemError) && !itemError.isEmpty()) {
            m_lastErrorCode = CommErrorCode::InvalidAddress;
            m_lastErrorMessage = itemError;
            emit errorOccurred(m_lastErrorMessage);
        }
        QString readError;
        if (!refreshItems(&readError) && !readError.isEmpty()) {
            m_lastErrorCode = CommErrorCode::ConnectionFailed;
            m_lastErrorMessage = readError;
            emit errorOccurred(m_lastErrorMessage);
        }
        QString probeError;
        if (!runReadProbe(&probeError) && !probeError.isEmpty()) {
            m_lastErrorCode = CommErrorCode::ConnectionFailed;
            m_lastErrorMessage = probeError;
            emit errorOccurred(m_lastErrorMessage);
        }
    }
}

void MatrikonOpcServer::updatePointValues(const QList<RuntimePointValue>& values)
{
    for (const RuntimePointValue& value : values) {
        if (!value.pointId.isEmpty()) {
            m_values.insert(value.pointId, value);
            if (m_running) {
                QString errorMessage;
                writeItemValue(value.pointId, value.value, &errorMessage);
            }
        }
    }
}

void MatrikonOpcServer::recordWriteResult(const QString& pointId, bool success, const QString& message)
{
    m_lastWritePointId = pointId;
    m_lastWriteSuccess = success;
    m_lastWriteMessage = message;
    m_lastWriteTime = QDateTime::currentDateTimeUtc();
    m_lastStatusChangeTime = m_lastWriteTime;
    if (success) {
        m_lastSuccessfulWriteTime = m_lastWriteTime;
        m_lastSuccessfulWriteMessage = message;
        ++m_successfulWriteCount;
    } else {
        m_lastFailedWriteTime = m_lastWriteTime;
        m_lastFailedWriteMessage = message;
        ++m_failedWriteCount;
    }
}

BackendStatusSnapshot MatrikonOpcServer::statusSnapshot() const
{
    const int configuredItemCount = m_itemsByPointId.size();
    const int currentActiveItemCount = activeItemCount();
    const bool serverConnected = m_server != nullptr && m_opcServer != nullptr;
    const bool groupReady = m_groupUnknown != nullptr
            && m_itemMgt != nullptr
            && m_syncIO != nullptr;
    const bool configuredItemsAvailable = configuredItemCount == 0
            || currentActiveItemCount == configuredItemCount;
    const bool readProbeAvailable = m_readProbeAttempted && m_readProbeOk;
    const QString subscriptionMode = opcSubscriptionMode(m_config);
    const bool subscriptionEnabled = subscriptionMode != QStringLiteral("disabled");
    const bool subscriptionRequired = subscriptionMode == QStringLiteral("required");
    const bool subscriptionAvailable = !subscriptionRequired
            || (m_subscriptionAttempted && m_subscriptionActive);
    const bool probeQualityDegraded = m_readProbeOk
            && qualityToRuntimeQuality(m_readProbeQuality) != RuntimePointQuality::Good;
    const bool capabilityDegraded = (m_browseAttempted && !m_browseSucceeded)
            || (m_refreshAttempted && !m_refreshOk)
            || (subscriptionEnabled && m_subscriptionAttempted && !m_subscriptionActive)
            || probeQualityDegraded;
    const bool online = m_running
            && serverConnected
            && groupReady
            && configuredItemsAvailable
            && readProbeAvailable
            && subscriptionAvailable;
    const bool operational = online && !capabilityDegraded && m_startupIssues.isEmpty();
    const bool degraded = m_running && !operational;

    BackendStatusSnapshot snapshot;
    snapshot.online = online;
    snapshot.backendType = QStringLiteral("matrikon-opc-da");
    snapshot.downloading = false;
    snapshot.downloadPercent = 0;
    snapshot.lastErrorCode = m_lastErrorCode;
    snapshot.lastErrorMessage = m_lastErrorMessage;
    snapshot.lastErrorDetails = m_startupIssues.join(QStringLiteral("; "));
    snapshot.partialSuccess = m_running && serverConnected && degraded;
    snapshot.timestamp = QDateTime::currentDateTimeUtc();
    snapshot.extras.insert(QStringLiteral("opcProgId"), m_config.opcProgId);
    snapshot.extras.insert(QStringLiteral("classicServerName"), m_config.classicServerName);
    snapshot.extras.insert(QStringLiteral("pointCount"), m_points.size());
    snapshot.extras.insert(QStringLiteral("tagCount"), m_tags.size());
    snapshot.extras.insert(QStringLiteral("valueCount"), m_values.size());
    snapshot.extras.insert(QStringLiteral("comInitialized"), m_comInitialized);
    snapshot.extras.insert(QStringLiteral("serverCreated"), m_server != nullptr);
    snapshot.extras.insert(QStringLiteral("serverConnected"), serverConnected);
    snapshot.extras.insert(QStringLiteral("groupCreated"), m_groupUnknown != nullptr);
    snapshot.extras.insert(QStringLiteral("groupReady"), groupReady);
    snapshot.extras.insert(QStringLiteral("itemMgtReady"), m_itemMgt != nullptr);
    snapshot.extras.insert(QStringLiteral("syncIoReady"), m_syncIO != nullptr);
    snapshot.extras.insert(QStringLiteral("itemCount"), configuredItemCount);
    snapshot.extras.insert(QStringLiteral("configuredItemCount"), configuredItemCount);
    snapshot.extras.insert(QStringLiteral("activeItemCount"), currentActiveItemCount);
    snapshot.extras.insert(QStringLiteral("configuredItemsAvailable"), configuredItemsAvailable);
    snapshot.extras.insert(QStringLiteral("addItemsAttempted"), m_addItemsAttempted);
    snapshot.extras.insert(QStringLiteral("browseSucceeded"), m_browseSucceeded);
    snapshot.extras.insert(QStringLiteral("browseAttempted"), m_browseAttempted);
    snapshot.extras.insert(QStringLiteral("browsedItemCount"), m_browsedItemIds.size());
    snapshot.extras.insert(QStringLiteral("lastBrowseMessage"), m_lastBrowseMessage);
    snapshot.extras.insert(QStringLiteral("refreshAttempted"), m_refreshAttempted);
    snapshot.extras.insert(QStringLiteral("refreshOk"), m_refreshOk);
    snapshot.extras.insert(QStringLiteral("lastRefreshMessage"), m_lastRefreshMessage);
    snapshot.extras.insert(QStringLiteral("subscriptionMode"), subscriptionMode);
    snapshot.extras.insert(QStringLiteral("subscriptionRequired"), subscriptionRequired);
    snapshot.extras.insert(QStringLiteral("subscriptionAttempted"), m_subscriptionAttempted);
    snapshot.extras.insert(QStringLiteral("subscriptionActive"), m_subscriptionActive);
    snapshot.extras.insert(QStringLiteral("subscriptionCookie"), static_cast<qulonglong>(m_subscriptionCookie));
    snapshot.extras.insert(QStringLiteral("lastSubscriptionMessage"), m_lastSubscriptionMessage);
    snapshot.extras.insert(QStringLiteral("expectedChannelVisible"), m_expectedChannelVisible);
    snapshot.extras.insert(QStringLiteral("primaryDeviceVisible"), m_primaryDeviceVisible);
    snapshot.extras.insert(QStringLiteral("secondaryDeviceVisible"), m_secondaryDeviceVisible);
    snapshot.extras.insert(QStringLiteral("standardItemsVisible"), m_standardItemsVisible);
    snapshot.extras.insert(QStringLiteral("configurationProbeOk"),
                           m_expectedChannelVisible || m_primaryDeviceVisible || m_standardItemsVisible);
    snapshot.extras.insert(QStringLiteral("configurationProbeMessage"), m_configurationProbeMessage);
    snapshot.extras.insert(QStringLiteral("callbackCount"), m_callbackCount);
    snapshot.extras.insert(QStringLiteral("lastCallbackTime"),
                           m_lastCallbackTime.isValid() ? m_lastCallbackTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("matchedItemCount"), m_matchedItemCount);
    snapshot.extras.insert(QStringLiteral("unmatchedItemCount"), m_unmatchedItemCount);
    snapshot.extras.insert(QStringLiteral("lastMatchedItemId"), m_lastMatchedItemId);
    snapshot.extras.insert(QStringLiteral("lastUnmatchedItemId"), m_lastUnmatchedItemId);
    snapshot.extras.insert(QStringLiteral("successfulItemCount"), m_successfulItemCount);
    snapshot.extras.insert(QStringLiteral("failedItemCount"), m_failedItemCount);
    snapshot.extras.insert(QStringLiteral("successfulReadCount"), m_successfulReadCount);
    snapshot.extras.insert(QStringLiteral("failedReadCount"), m_failedReadCount);
    snapshot.extras.insert(QStringLiteral("lastReadPointId"), m_lastReadPointId);
    snapshot.extras.insert(QStringLiteral("lastReadItemId"), m_lastReadItemId);
    snapshot.extras.insert(QStringLiteral("lastReadMessage"), m_lastReadMessage);
    snapshot.extras.insert(QStringLiteral("lastQuality"), m_lastQuality);
    snapshot.extras.insert(QStringLiteral("lastQualityText"), m_lastQualityText);
    snapshot.extras.insert(QStringLiteral("lastTimestampSource"), m_lastTimestampSource);
    snapshot.extras.insert(QStringLiteral("lastReadTime"),
                           m_lastReadTime.isValid() ? m_lastReadTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("lastSuccessfulReadTime"),
                           m_lastSuccessfulReadTime.isValid() ? m_lastSuccessfulReadTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("lastFailedReadTime"),
                           m_lastFailedReadTime.isValid() ? m_lastFailedReadTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("readProbeAttempted"), m_readProbeAttempted);
    snapshot.extras.insert(QStringLiteral("readProbeOk"), m_readProbeOk);
    snapshot.extras.insert(QStringLiteral("readProbeAvailable"), readProbeAvailable);
    snapshot.extras.insert(QStringLiteral("readProbePointId"), m_readProbePointId);
    snapshot.extras.insert(QStringLiteral("readProbeItemId"), m_readProbeItemId);
    snapshot.extras.insert(QStringLiteral("readProbeValue"), m_readProbeValue);
    snapshot.extras.insert(QStringLiteral("readProbeQuality"), m_readProbeQuality);
    snapshot.extras.insert(QStringLiteral("readProbeQualityText"), m_readProbeQualityText);
    snapshot.extras.insert(QStringLiteral("readProbeTimestamp"),
                           m_readProbeTimestamp.isValid() ? m_readProbeTimestamp.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("readProbeTime"),
                           m_readProbeTime.isValid() ? m_readProbeTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("readProbeMessage"), m_readProbeMessage);
    snapshot.extras.insert(QStringLiteral("lastStatusChangeTime"),
                           m_lastStatusChangeTime.isValid() ? m_lastStatusChangeTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("lastWritePointId"), m_lastWritePointId);
    snapshot.extras.insert(QStringLiteral("lastWriteItemId"), m_lastWriteItemId);
    snapshot.extras.insert(QStringLiteral("lastWriteValue"), m_lastWriteValue);
    snapshot.extras.insert(QStringLiteral("lastWriteTime"),
                           m_lastWriteTime.isValid() ? m_lastWriteTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("lastWriteSuccess"), m_lastWriteSuccess);
    snapshot.extras.insert(QStringLiteral("lastWriteMessage"), m_lastWriteMessage);
    snapshot.extras.insert(QStringLiteral("lastSuccessfulWriteTime"),
                           m_lastSuccessfulWriteTime.isValid() ? m_lastSuccessfulWriteTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("lastFailedWriteTime"),
                           m_lastFailedWriteTime.isValid() ? m_lastFailedWriteTime.toString(Qt::ISODate) : QString());
    snapshot.extras.insert(QStringLiteral("lastSuccessfulWriteMessage"), m_lastSuccessfulWriteMessage);
    snapshot.extras.insert(QStringLiteral("lastFailedWriteMessage"), m_lastFailedWriteMessage);
    snapshot.extras.insert(QStringLiteral("successfulWriteCount"), m_successfulWriteCount);
    snapshot.extras.insert(QStringLiteral("failedWriteCount"), m_failedWriteCount);
    snapshot.extras.insert(QStringLiteral("online"), snapshot.online);
    snapshot.extras.insert(QStringLiteral("operational"), operational);
    snapshot.extras.insert(QStringLiteral("degraded"), degraded);
    snapshot.extras.insert(QStringLiteral("lifecycleStatus"),
                           !m_running ? QStringLiteral("offline")
                                      : operational ? QStringLiteral("operational")
                                                    : QStringLiteral("degraded"));
    snapshot.extras.insert(QStringLiteral("startupIssues"), m_startupIssues);
    snapshot.extras.insert(QStringLiteral("impl"), QStringLiteral("matrikon-opc-da"));
    return snapshot;
}

int MatrikonOpcServer::activeItemCount() const
{
    int count = 0;
    for (auto it = m_itemsByPointId.constBegin(); it != m_itemsByPointId.constEnd(); ++it) {
        if (it->active && it->serverHandle != 0) {
            ++count;
        }
    }
    return count;
}

bool MatrikonOpcServer::ensureComInitialized(QString* errorMessage)
{
#ifndef Q_OS_WIN
    if (errorMessage) {
        *errorMessage = QStringLiteral("OPC DA is only available on Windows COM");
    }
    return false;
#else
    if (m_comInitialized) {
        return true;
    }

    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("COM init failed: %1").arg(hresultToString(hr));
        }
        return false;
    }

    m_comInitialized = hr != RPC_E_CHANGED_MODE;
    return true;
#endif
}

bool MatrikonOpcServer::createServerInstance(QString* errorMessage)
{
#ifndef Q_OS_WIN
    Q_UNUSED(errorMessage)
    return false;
#else
    if (m_server) {
        return true;
    }

    CLSID clsid;
    const std::wstring progId = m_config.opcProgId.toStdWString();
    HRESULT hr = CLSIDFromProgID(progId.c_str(), &clsid);
    if (FAILED(hr)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("OPC ProgID not registered: %1 (%2)")
                    .arg(m_config.opcProgId, hresultToString(hr));
        }
        return false;
    }

    hr = CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, IID_IUnknown,
                          reinterpret_cast<void**>(&m_server));
    if (FAILED(hr) || !m_server) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create OPC server %1: %2")
                    .arg(m_config.opcProgId, hresultToString(hr));
        }
        m_server = nullptr;
        return false;
    }

    hr = m_server->QueryInterface(IID_IOPCServer_LH, reinterpret_cast<void**>(&m_opcServer));
    if (FAILED(hr) || !m_opcServer) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("OPC server does not expose IOPCServer: %1")
                    .arg(hresultToString(hr));
        }
        releaseServerInstance();
        return false;
    }

    hr = m_server->QueryInterface(IID_IOPCBrowseServerAddressSpace_LH, reinterpret_cast<void**>(&m_browser));
    if (FAILED(hr)) {
        m_browser = nullptr;
        m_lastBrowseMessage = QStringLiteral("OPC server does not expose browser: %1")
                .arg(hresultToString(hr));
    }

    return true;
#endif
}

bool MatrikonOpcServer::createOpcGroup(QString* errorMessage)
{
#ifndef Q_OS_WIN
    Q_UNUSED(errorMessage)
    return false;
#else
    if (m_groupUnknown && m_itemMgt && m_syncIO) {
        return true;
    }

    if (!m_opcServer) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("IOPCServer is not available");
        }
        return false;
    }

    DWORD revisedUpdateRate = 0;
    const QString groupName = QStringLiteral("LH_%1").arg(reinterpret_cast<quintptr>(this), 0, 16);
    const std::wstring wideGroupName = groupName.toStdWString();
    HRESULT hr = m_opcServer->AddGroup(wideGroupName.c_str(),
                                       TRUE,
                                       static_cast<DWORD>(qMax(10, m_config.publishIntervalMs)),
                                       1,
                                       nullptr,
                                       nullptr,
                                       0,
                                       &m_groupServerHandle,
                                       &revisedUpdateRate,
                                       IID_IUnknown,
                                       &m_groupUnknown);
    if (FAILED(hr) || !m_groupUnknown) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to add OPC DA group: %1").arg(hresultToString(hr));
        }
        m_groupUnknown = nullptr;
        return false;
    }

    hr = m_groupUnknown->QueryInterface(IID_IOPCItemMgt_LH, reinterpret_cast<void**>(&m_itemMgt));
    if (FAILED(hr) || !m_itemMgt) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("OPC DA group does not expose IOPCItemMgt: %1")
                    .arg(hresultToString(hr));
        }
        releaseOpcGroup();
        return false;
    }

    hr = m_groupUnknown->QueryInterface(IID_IOPCSyncIO_LH, reinterpret_cast<void**>(&m_syncIO));
    if (FAILED(hr) || !m_syncIO) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("OPC DA group does not expose IOPCSyncIO: %1")
                    .arg(hresultToString(hr));
        }
        releaseOpcGroup();
        return false;
    }

    return true;
#endif
}

bool MatrikonOpcServer::browseServerAddressSpace(QString* errorMessage)
{
    m_browseAttempted = true;
    m_browsedItemIds.clear();
    m_browseSucceeded = false;
#ifndef Q_OS_WIN
    m_lastBrowseMessage = QStringLiteral("OPC DA browse is only available on Windows COM");
    if (errorMessage) {
        *errorMessage = m_lastBrowseMessage;
    }
    return false;
#else
    if (!m_browser) {
        m_lastBrowseMessage = QStringLiteral("IOPCBrowseServerAddressSpace is not available");
        if (errorMessage) {
            *errorMessage = m_lastBrowseMessage;
        }
        return false;
    }

    OPCNAMESPACETYPE namespaceType = OPC_NS_FLAT;
    HRESULT hr = m_browser->QueryOrganization(&namespaceType);
    if (FAILED(hr)) {
        m_lastBrowseMessage = QStringLiteral("QueryOrganization failed: %1").arg(hresultToString(hr));
        if (errorMessage) {
            *errorMessage = m_lastBrowseMessage;
        }
        return false;
    }

    LPENUMSTRING enumString = nullptr;
    hr = m_browser->BrowseOPCItemIDs(OPC_FLAT, L"", VT_EMPTY, 0, &enumString);
    if (FAILED(hr) || !enumString) {
        hr = m_browser->BrowseOPCItemIDs(OPC_LEAF, L"", VT_EMPTY, 0, &enumString);
    }
    if (FAILED(hr) || !enumString) {
        m_lastBrowseMessage = QStringLiteral("BrowseOPCItemIDs failed: %1").arg(hresultToString(hr));
        if (errorMessage) {
            *errorMessage = m_lastBrowseMessage;
        }
        return false;
    }

    constexpr ULONG kBatchSize = 16;
    LPOLESTR names[kBatchSize] = {};
    ULONG fetched = 0;
    while (enumString->Next(kBatchSize, names, &fetched) == S_OK || fetched > 0) {
        for (ULONG i = 0; i < fetched; ++i) {
            if (names[i]) {
                m_browsedItemIds.append(QString::fromWCharArray(names[i]));
                CoTaskMemFree(names[i]);
                names[i] = nullptr;
            }
        }
        if (m_browsedItemIds.size() >= 512 || fetched < kBatchSize) {
            break;
        }
        fetched = 0;
    }
    enumString->Release();

    m_browseSucceeded = true;
    m_lastBrowseMessage = QStringLiteral("browse ok");
    updateConfigurationProbe();
    return true;
#endif
}

void MatrikonOpcServer::updateConfigurationProbe()
{
    const QString expectedChannel = m_config.channelName.trimmed().isEmpty()
            ? QStringLiteral("CommPort")
            : m_config.channelName.trimmed();
    const QString primaryDevice = m_config.metadata.value(QStringLiteral("primaryDeviceName"),
                                                          QStringLiteral("InitDevParamnt")).toString().trimmed();
    const QString secondaryDevice = m_config.metadata.value(QStringLiteral("secondaryDeviceName"),
                                                            QStringLiteral("InitDevConfig")).toString().trimmed();

    auto containsToken = [&](const QString& token) {
        if (token.isEmpty()) {
            return false;
        }
        for (const QString& itemId : std::as_const(m_browsedItemIds)) {
            if (itemId.compare(token, Qt::CaseInsensitive) == 0
                    || itemId.contains(token + QStringLiteral("."))
                    || itemId.contains(token + QStringLiteral("/"))
                    || itemId.contains(QStringLiteral(".") + token)
                    || itemId.contains(QStringLiteral("/") + token)) {
                return true;
            }
        }
        return false;
    };

    m_expectedChannelVisible = containsToken(expectedChannel);
    m_primaryDeviceVisible = containsToken(primaryDevice);
    m_secondaryDeviceVisible = containsToken(secondaryDevice);
    m_standardItemsVisible = false;
    for (const QString& itemId : std::as_const(m_browsedItemIds)) {
        if (itemId == QStringLiteral("#Enabled")
                || itemId == QStringLiteral("#OfflineMode")
                || itemId == QStringLiteral("#MonitorACLFile")
                || itemId.endsWith(QStringLiteral(".#Enabled"))
                || itemId.endsWith(QStringLiteral(".#OfflineMode"))
                || itemId.endsWith(QStringLiteral(".@Connected"))) {
            m_standardItemsVisible = true;
            break;
        }
    }

    m_configurationProbeMessage = QStringLiteral("channel=%1 primary=%2 secondary=%3 standardItems=%4 browsed=%5")
            .arg(m_expectedChannelVisible ? QStringLiteral("visible") : QStringLiteral("missing"),
                 m_primaryDeviceVisible ? QStringLiteral("visible") : QStringLiteral("missing"),
                 m_secondaryDeviceVisible ? QStringLiteral("visible") : QStringLiteral("missing"),
                 m_standardItemsVisible ? QStringLiteral("visible") : QStringLiteral("missing"),
                 QString::number(m_browsedItemIds.size()));
}

bool MatrikonOpcServer::subscribeDataChanges(QString* errorMessage)
{
    m_subscriptionAttempted = true;
#ifndef Q_OS_WIN
    m_lastSubscriptionMessage = QStringLiteral("OPC DA subscription is only available on Windows COM");
    if (errorMessage) {
        *errorMessage = m_lastSubscriptionMessage;
    }
    return false;
#else
    if (m_subscriptionActive) {
        return true;
    }
    if (!m_groupUnknown) {
        m_lastSubscriptionMessage = QStringLiteral("OPC DA group is not available");
        if (errorMessage) {
            *errorMessage = m_lastSubscriptionMessage;
        }
        return false;
    }

    IConnectionPointContainer* container = nullptr;
    HRESULT hr = m_groupUnknown->QueryInterface(IID_IConnectionPointContainer,
                                                reinterpret_cast<void**>(&container));
    if (FAILED(hr) || !container) {
        m_lastSubscriptionMessage = QStringLiteral("Group does not expose IConnectionPointContainer: %1")
                .arg(hresultToString(hr));
        if (errorMessage) {
            *errorMessage = m_lastSubscriptionMessage;
        }
        return false;
    }

    hr = container->FindConnectionPoint(IID_IOPCDataCallback_LH, &m_dataCallbackConnectionPoint);
    container->Release();
    if (FAILED(hr) || !m_dataCallbackConnectionPoint) {
        m_lastSubscriptionMessage = QStringLiteral("IOPCDataCallback connection point unavailable: %1")
                .arg(hresultToString(hr));
        if (errorMessage) {
            *errorMessage = m_lastSubscriptionMessage;
        }
        m_dataCallbackConnectionPoint = nullptr;
        return false;
    }

    m_dataCallback = static_cast<IUnknown*>(new MatrikonOpcDataCallback(m_callbackContext));
    hr = m_dataCallbackConnectionPoint->Advise(m_dataCallback, &m_subscriptionCookie);
    if (FAILED(hr)) {
        m_lastSubscriptionMessage = QStringLiteral("Advise IOPCDataCallback failed: %1")
                .arg(hresultToString(hr));
        if (errorMessage) {
            *errorMessage = m_lastSubscriptionMessage;
        }
        unsubscribeDataChanges();
        return false;
    }

    m_subscriptionActive = true;
    activateDataCallbackContext();
    m_lastSubscriptionMessage = QStringLiteral("subscription ok");
    return true;
#endif
}

void MatrikonOpcServer::unsubscribeDataChanges()
{
    deactivateDataCallbackContext();
#ifdef Q_OS_WIN
    if (m_dataCallbackConnectionPoint && m_subscriptionCookie != 0) {
        m_dataCallbackConnectionPoint->Unadvise(m_subscriptionCookie);
    }
    m_subscriptionCookie = 0;
    m_subscriptionActive = false;
    if (m_dataCallbackConnectionPoint) {
        m_dataCallbackConnectionPoint->Release();
        m_dataCallbackConnectionPoint = nullptr;
    }
    if (m_dataCallback) {
        m_dataCallback->Release();
        m_dataCallback = nullptr;
    }
#else
    m_subscriptionCookie = 0;
    m_subscriptionActive = false;
#endif
}

void MatrikonOpcServer::releaseOpcGroup()
{
    unsubscribeDataChanges();
#ifdef Q_OS_WIN
    if (m_syncIO) {
        m_syncIO->Release();
        m_syncIO = nullptr;
    }
    if (m_itemMgt) {
        m_itemMgt->Release();
        m_itemMgt = nullptr;
    }
    if (m_groupUnknown) {
        m_groupUnknown->Release();
        m_groupUnknown = nullptr;
    }
    if (m_opcServer && m_groupServerHandle != 0) {
        m_opcServer->RemoveGroup(m_groupServerHandle, TRUE);
    }
#endif
    m_groupServerHandle = 0;
    for (auto it = m_itemsByPointId.begin(); it != m_itemsByPointId.end(); ++it) {
        it->serverHandle = 0;
        it->active = false;
    }
}

void MatrikonOpcServer::releaseServerInstance()
{
#ifdef Q_OS_WIN
    if (m_browser) {
        m_browser->Release();
        m_browser = nullptr;
    }
    if (m_opcServer) {
        m_opcServer->Release();
        m_opcServer = nullptr;
    }
    if (m_server) {
        m_server->Release();
        m_server = nullptr;
    }
#endif
}

void MatrikonOpcServer::rebuildPendingItems()
{
    QHash<QString, ItemBinding> rebuilt;
    m_pointIdByItemId.clear();
    m_pointIdByClientHandle.clear();

    for (const RuntimePointDefinition& point : std::as_const(m_points)) {
        if (point.id.trimmed().isEmpty()) {
            continue;
        }
        const OpcTagDefinition tag = RuntimePointConverter::runtimePointToOpcTag(point);
        const QStringList candidates = itemIdCandidatesForTag(tag);

        ItemBinding binding = m_itemsByPointId.value(point.id);
        const QString itemId = candidates.isEmpty() ? QString() : candidates.first();
        if (binding.itemId != itemId) {
            binding.serverHandle = 0;
            binding.active = false;
        }
        binding.pointId = point.id;
        binding.candidateItemIds = candidates;
        binding.itemId = itemId;
        if (binding.clientHandle == 0) {
            binding.clientHandle = m_nextClientHandle++;
        }
        rebuilt.insert(point.id, binding);
        for (const QString& candidate : candidates) {
            m_pointIdByItemId.insert(candidate, point.id);
        }
        m_pointIdByClientHandle.insert(binding.clientHandle, point.id);
    }

    for (const OpcTagDefinition& tag : std::as_const(m_tags)) {
        const QStringList candidates = itemIdCandidatesForTag(tag);
        const QString itemId = candidates.isEmpty() ? QString() : candidates.first();
        const QString pointId = tag.tagName.trimmed().isEmpty() ? itemId : tag.tagName.trimmed();
        if (pointId.isEmpty() || rebuilt.contains(pointId)) {
            continue;
        }

        ItemBinding binding = m_itemsByPointId.value(pointId);
        if (binding.itemId != itemId) {
            binding.serverHandle = 0;
            binding.active = false;
        }
        binding.pointId = pointId;
        binding.candidateItemIds = candidates;
        binding.itemId = itemId;
        if (binding.clientHandle == 0) {
            binding.clientHandle = m_nextClientHandle++;
        }
        rebuilt.insert(pointId, binding);
        for (const QString& candidate : candidates) {
            m_pointIdByItemId.insert(candidate, pointId);
        }
        m_pointIdByClientHandle.insert(binding.clientHandle, pointId);
    }

    m_itemsByPointId = rebuilt;
}

bool MatrikonOpcServer::addPendingItems(QString* errorMessage)
{
    m_addItemsAttempted = true;
    if (errorMessage) {
        errorMessage->clear();
    }
#ifndef Q_OS_WIN
    if (errorMessage) {
        *errorMessage = QStringLiteral("OPC DA item setup is only available on Windows COM");
    }
    return false;
#else
    if (!m_itemMgt) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("IOPCItemMgt is not available");
        }
        return false;
    }

    QList<QString> pointIds;
    QList<QString> itemIds;
    QList<unsigned long> clientHandles;
    QStringList failures;
    int unresolvedCount = 0;
    for (auto it = m_itemsByPointId.begin(); it != m_itemsByPointId.end(); ++it) {
        if (it->active) {
            continue;
        }
        if (it->itemId.trimmed().isEmpty()) {
            ++unresolvedCount;
            failures << QStringLiteral("%1=no item ID").arg(it.key());
            it->serverHandle = 0;
            it->active = false;
            continue;
        }
        pointIds.append(it.key());
        itemIds.append(it->itemId);
        clientHandles.append(it->clientHandle);
    }

    if (pointIds.isEmpty()) {
        if (unresolvedCount > 0) {
            m_failedItemCount += unresolvedCount;
            if (errorMessage) {
                *errorMessage = QStringLiteral("OPC DA item setup failed: %1")
                        .arg(failures.join(QStringLiteral("; ")));
            }
            return false;
        }
        return true;
    }

    QVarLengthArray<std::wstring> wideItemIds(pointIds.size());
    QVarLengthArray<OPCITEMDEF> itemDefs(pointIds.size());
    for (int i = 0; i < pointIds.size(); ++i) {
        wideItemIds[i] = itemIds.at(i).toStdWString();
        OPCITEMDEF def = {};
        def.szAccessPath = nullptr;
        def.szItemID = const_cast<LPWSTR>(wideItemIds[i].c_str());
        def.bActive = TRUE;
        def.hClient = clientHandles.at(i);
        def.vtRequestedDataType = VT_EMPTY;
        itemDefs[i] = def;
    }

    OPCITEMRESULT* results = nullptr;
    HRESULT* errors = nullptr;
    const HRESULT hr = m_itemMgt->AddItems(static_cast<DWORD>(itemDefs.size()),
                                           itemDefs.data(),
                                           &results,
                                           &errors);

    auto freeResults = [&]() {
        if (results) {
            for (int i = 0; i < pointIds.size(); ++i) {
                if (results[i].pBlob) {
                    CoTaskMemFree(results[i].pBlob);
                }
            }
            CoTaskMemFree(results);
            results = nullptr;
        }
        if (errors) {
            CoTaskMemFree(errors);
            errors = nullptr;
        }
    };

    if (FAILED(hr)) {
        for (const QString& pointId : std::as_const(pointIds)) {
            ItemBinding binding = m_itemsByPointId.value(pointId);
            binding.lastResult = hr;
            binding.serverHandle = 0;
            binding.active = false;
            m_itemsByPointId.insert(pointId, binding);
            failures << QStringLiteral("%1=%2").arg(binding.itemId, hresultToString(hr));
        }
        freeResults();
        m_failedItemCount += pointIds.size() + unresolvedCount;
        if (errorMessage) {
            *errorMessage = QStringLiteral("IOPCItemMgt::AddItems failed: %1 (%2)")
                    .arg(hresultToString(hr), failures.join(QStringLiteral("; ")));
        }
        return false;
    }

    int successCount = 0;
    int failedCount = unresolvedCount;
    for (int i = 0; i < pointIds.size(); ++i) {
        ItemBinding binding = m_itemsByPointId.value(pointIds.at(i));
        const HRESULT itemHr = errors ? errors[i] : S_OK;
        binding.lastResult = itemHr;
        if (SUCCEEDED(itemHr) && results && results[i].hServer != 0) {
            binding.serverHandle = results[i].hServer;
            binding.active = true;
            ++successCount;
        } else {
            binding.serverHandle = 0;
            binding.active = false;
            ++failedCount;
            failures << QStringLiteral("%1=%2").arg(binding.itemId, hresultToString(itemHr));
        }
        m_itemsByPointId.insert(pointIds.at(i), binding);
    }

    freeResults();

    m_successfulItemCount += successCount;
    m_failedItemCount += failedCount;
    if (failedCount > 0 && errorMessage) {
        *errorMessage = QStringLiteral("OPC DA item add partial failure: %1").arg(failures.join(QStringLiteral("; ")));
    }
    return failedCount == 0;
#endif
}

bool MatrikonOpcServer::refreshItems(QString* errorMessage)
{
    m_refreshAttempted = true;
    m_refreshOk = false;
    m_lastRefreshMessage.clear();
    if (errorMessage) {
        errorMessage->clear();
    }
#ifndef Q_OS_WIN
    m_lastRefreshMessage = QStringLiteral("OPC DA initial read is only available on Windows COM");
    if (errorMessage) {
        *errorMessage = m_lastRefreshMessage;
    }
    return false;
#else
    if (!m_syncIO) {
        m_lastRefreshMessage = QStringLiteral("IOPCSyncIO is not available");
        if (errorMessage) {
            *errorMessage = m_lastRefreshMessage;
        }
        return false;
    }

    QList<QString> pointIds;
    QVarLengthArray<OPCHANDLE> handles;
    for (auto it = m_itemsByPointId.constBegin(); it != m_itemsByPointId.constEnd(); ++it) {
        if (it->active && it->serverHandle != 0) {
            pointIds.append(it.key());
            handles.append(it->serverHandle);
        }
    }
    if (handles.isEmpty()) {
        m_refreshOk = true;
        m_lastRefreshMessage = QStringLiteral("OPC DA initial read skipped: no active configured items");
        return true;
    }

    OPCITEMSTATE* states = nullptr;
    HRESULT* errors = nullptr;
    const HRESULT hr = m_syncIO->Read(OPC_DS_DEVICE,
                                      static_cast<DWORD>(handles.size()),
                                      handles.data(),
                                      &states,
                                      &errors);
    m_lastReadTime = QDateTime::currentDateTimeUtc();

    auto freeReadResults = [&]() {
        if (states) {
            for (int i = 0; i < pointIds.size(); ++i) {
                VariantClear(&states[i].vDataValue);
            }
            CoTaskMemFree(states);
            states = nullptr;
        }
        if (errors) {
            CoTaskMemFree(errors);
            errors = nullptr;
        }
    };

    if (FAILED(hr)) {
        freeReadResults();
        ++m_failedReadCount;
        m_lastFailedReadTime = m_lastReadTime;
        m_lastReadMessage = QStringLiteral("IOPCSyncIO::Read failed: %1").arg(hresultToString(hr));
        m_lastRefreshMessage = m_lastReadMessage;
        if (errorMessage) {
            *errorMessage = m_lastReadMessage;
        }
        return false;
    }

    bool allSuccess = true;
    QStringList failures;
    for (int i = 0; i < pointIds.size(); ++i) {
        ItemBinding binding = m_itemsByPointId.value(pointIds.at(i));
        const HRESULT itemHr = errors ? errors[i] : S_OK;
        binding.lastResult = itemHr;
        m_lastReadPointId = pointIds.at(i);
        m_lastReadItemId = binding.itemId;
        const QVariant value = states ? variantToQVariant(&states[i].vDataValue) : QVariant();
        if (states) {
            binding.lastQuality = states[i].wQuality;
            binding.lastTimestamp = fileTimeToDateTime(&states[i].ftTimeStamp);
            m_lastQuality = binding.lastQuality;
            m_lastQualityText = qualityToString(binding.lastQuality);
            m_lastTimestampSource = binding.lastTimestamp.isValid()
                    ? QStringLiteral("opc-da")
                    : QStringLiteral("local");
        }
        if (SUCCEEDED(itemHr) && states && hasOpcPayload(value)) {
            binding.lastValue = value;
            RuntimePointValue current;
            current.pointId = pointIds.at(i);
            current.value = binding.lastValue;
            current.quality = qualityToRuntimeQuality(binding.lastQuality);
            current.timestamp = binding.lastTimestamp.isValid() ? binding.lastTimestamp : m_lastReadTime;
            current.origin = QStringLiteral("opc-da-read");
            m_values.insert(current.pointId, current);
            ++m_successfulReadCount;
            m_lastSuccessfulReadTime = m_lastReadTime;
        } else {
            allSuccess = false;
            binding.lastValue.clear();
            m_values.remove(pointIds.at(i));
            ++m_failedReadCount;
            m_lastFailedReadTime = m_lastReadTime;
            m_lastReadMessage = QStringLiteral("OPC DA read failed for %1: %2")
                    .arg(binding.itemId,
                         SUCCEEDED(itemHr)
                                 ? QStringLiteral("null or unsupported value")
                                 : hresultToString(itemHr));
            failures << m_lastReadMessage;
        }
        m_itemsByPointId.insert(pointIds.at(i), binding);
    }

    freeReadResults();
    m_refreshOk = allSuccess;
    m_lastRefreshMessage = allSuccess
            ? QStringLiteral("OPC DA initial read ok")
            : QStringLiteral("OPC DA initial read partial failure: %1")
                      .arg(failures.join(QStringLiteral("; ")));
    if (!allSuccess && errorMessage) {
        *errorMessage = m_lastRefreshMessage;
    }
    return allSuccess;
#endif
}

bool MatrikonOpcServer::runReadProbe(QString* errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    m_readProbeAttempted = true;
    m_readProbeOk = false;
    m_readProbePointId.clear();
    m_readProbeItemId.clear();
    m_readProbeValue.clear();
    m_readProbeQuality = 0;
    m_readProbeQualityText.clear();
    m_readProbeTimestamp = QDateTime();
    m_readProbeTime = QDateTime::currentDateTimeUtc();
    m_readProbeMessage.clear();

#ifndef Q_OS_WIN
    m_readProbeMessage = QStringLiteral("OPC DA read probe is only available on Windows COM");
    if (errorMessage) {
        *errorMessage = m_readProbeMessage;
    }
    return false;
#else
    if (!m_syncIO) {
        m_readProbeMessage = QStringLiteral("IOPCSyncIO is not available");
        if (errorMessage) {
            *errorMessage = m_readProbeMessage;
        }
        return false;
    }

    QString lastProbeError;
    for (auto it = m_itemsByPointId.constBegin(); it != m_itemsByPointId.constEnd(); ++it) {
        if (it->active && it->serverHandle != 0) {
            QString probeError;
            if (readProbeHandle(it.key(), it->itemId, it->serverHandle, true, &probeError)) {
                return true;
            }
            if (!probeError.isEmpty()) {
                lastProbeError = probeError;
            }
        }
    }

    const QStringList candidates = readProbeItemCandidates();
    for (const QString& itemId : candidates) {
        QString probeError;
        if (readTemporaryProbeItem(itemId, &probeError)) {
            if (errorMessage) {
                errorMessage->clear();
            }
            return true;
        }
        if (!probeError.isEmpty()) {
            lastProbeError = probeError;
        }
    }

    m_readProbeMessage = lastProbeError.isEmpty()
            ? QStringLiteral("No readable OPC DA probe item was found")
            : lastProbeError;
    if (errorMessage) {
        *errorMessage = m_readProbeMessage;
    }
    return false;
#endif
}

bool MatrikonOpcServer::readProbeHandle(const QString& pointId,
                                        const QString& itemId,
                                        unsigned long serverHandle,
                                        bool updateRuntimeValue,
                                        QString* errorMessage)
{
#ifndef Q_OS_WIN
    Q_UNUSED(pointId)
    Q_UNUSED(itemId)
    Q_UNUSED(serverHandle)
    Q_UNUSED(updateRuntimeValue)
    if (errorMessage) {
        *errorMessage = QStringLiteral("OPC DA read probe is only available on Windows COM");
    }
    return false;
#else
    if (errorMessage) {
        errorMessage->clear();
    }
    m_readProbeAttempted = true;
    m_readProbeOk = false;
    m_readProbePointId = pointId;
    m_readProbeItemId = itemId;
    m_readProbeValue.clear();
    m_readProbeQuality = 0;
    m_readProbeQualityText.clear();
    m_readProbeTimestamp = QDateTime();
    m_readProbeTime = QDateTime::currentDateTimeUtc();
    m_readProbeMessage.clear();

    if (!m_syncIO || serverHandle == 0) {
        m_readProbeMessage = QStringLiteral("OPC DA read probe target is not active: %1").arg(itemId);
        if (errorMessage) {
            *errorMessage = m_readProbeMessage;
        }
        return false;
    }

    OPCHANDLE handle = static_cast<OPCHANDLE>(serverHandle);
    OPCITEMSTATE* state = nullptr;
    HRESULT* errors = nullptr;
    const HRESULT hr = m_syncIO->Read(OPC_DS_DEVICE, 1, &handle, &state, &errors);
    m_lastReadTime = m_readProbeTime;

    const HRESULT itemHr = errors ? errors[0] : hr;
    const QVariant value = state ? variantToQVariant(&state[0].vDataValue) : QVariant();
    const bool ok = SUCCEEDED(hr) && SUCCEEDED(itemHr) && state && hasOpcPayload(value);
    if (state) {
        m_readProbeQuality = state[0].wQuality;
        m_readProbeQualityText = qualityToString(m_readProbeQuality);
    }
    if (ok) {
        m_readProbeOk = true;
        m_readProbeValue = value;
        m_readProbeTimestamp = fileTimeToDateTime(&state[0].ftTimeStamp);
        m_readProbeMessage = QStringLiteral("OPC DA read probe ok: %1").arg(itemId);

        m_lastReadPointId = pointId;
        m_lastReadItemId = itemId;
        m_lastReadMessage = m_readProbeMessage;
        m_lastQuality = m_readProbeQuality;
        m_lastQualityText = m_readProbeQualityText;
        m_lastTimestampSource = m_readProbeTimestamp.isValid() ? QStringLiteral("opc-da") : QStringLiteral("local");
        m_lastSuccessfulReadTime = m_readProbeTime;
        ++m_successfulReadCount;

        if (updateRuntimeValue && !pointId.isEmpty()) {
            ItemBinding binding = m_itemsByPointId.value(pointId);
            binding.lastValue = m_readProbeValue;
            binding.lastQuality = m_readProbeQuality;
            binding.lastTimestamp = m_readProbeTimestamp;
            binding.lastResult = itemHr;
            m_itemsByPointId.insert(pointId, binding);

            RuntimePointValue current;
            current.pointId = pointId;
            current.value = m_readProbeValue;
            current.quality = qualityToRuntimeQuality(m_readProbeQuality);
            current.timestamp = m_readProbeTimestamp.isValid() ? m_readProbeTimestamp : m_readProbeTime;
            current.origin = QStringLiteral("opc-da-read-probe");
            m_values.insert(pointId, current);
        }
    } else {
        m_readProbeMessage = QStringLiteral("OPC DA read probe failed for %1: %2")
                .arg(itemId,
                     SUCCEEDED(hr) && SUCCEEDED(itemHr)
                             ? QStringLiteral("null or unsupported value")
                             : QStringLiteral("%1 / %2")
                                       .arg(hresultToString(hr), hresultToString(itemHr)));
        m_lastReadPointId = pointId;
        m_lastReadItemId = itemId;
        m_lastReadMessage = m_readProbeMessage;
        m_lastFailedReadTime = m_readProbeTime;
        if (state) {
            m_lastQuality = m_readProbeQuality;
            m_lastQualityText = m_readProbeQualityText;
        }
        if (!pointId.isEmpty()) {
            m_values.remove(pointId);
        }
        ++m_failedReadCount;
        if (errorMessage) {
            *errorMessage = m_readProbeMessage;
        }
    }

    if (state) {
        VariantClear(&state[0].vDataValue);
        CoTaskMemFree(state);
    }
    if (errors) {
        CoTaskMemFree(errors);
    }
    return ok;
#endif
}

bool MatrikonOpcServer::readTemporaryProbeItem(const QString& itemId, QString* errorMessage)
{
#ifndef Q_OS_WIN
    Q_UNUSED(itemId)
    Q_UNUSED(errorMessage)
    return false;
#else
    if (!m_itemMgt) {
        m_readProbeMessage = QStringLiteral("IOPCItemMgt is not available");
        if (errorMessage) {
            *errorMessage = m_readProbeMessage;
        }
        return false;
    }

    const std::wstring wideItemId = itemId.toStdWString();
    OPCITEMDEF def = {};
    def.szItemID = const_cast<LPWSTR>(wideItemId.c_str());
    def.bActive = TRUE;
    def.hClient = 0;
    def.vtRequestedDataType = VT_EMPTY;

    OPCITEMRESULT* results = nullptr;
    HRESULT* errors = nullptr;
    HRESULT hr = m_itemMgt->AddItems(1, &def, &results, &errors);
    const HRESULT itemHr = errors ? errors[0] : hr;
    const bool added = SUCCEEDED(hr) && SUCCEEDED(itemHr) && results && results[0].hServer != 0;
    OPCHANDLE handle = added ? results[0].hServer : 0;

    if (results) {
        if (results[0].pBlob) {
            CoTaskMemFree(results[0].pBlob);
        }
        CoTaskMemFree(results);
    }
    if (errors) {
        CoTaskMemFree(errors);
    }

    if (!added) {
        m_readProbeMessage = QStringLiteral("OPC DA read probe item add failed for %1: %2 / %3")
                .arg(itemId, hresultToString(hr), hresultToString(itemHr));
        if (errorMessage) {
            *errorMessage = m_readProbeMessage;
        }
        return false;
    }

    const bool ok = readProbeHandle(QString(), itemId, handle, false, errorMessage);
    HRESULT* removeErrors = nullptr;
    m_itemMgt->RemoveItems(1, &handle, &removeErrors);
    if (removeErrors) {
        CoTaskMemFree(removeErrors);
    }
    return ok;
#endif
}

QStringList MatrikonOpcServer::readProbeItemCandidates() const
{
    QStringList candidates;
    auto addCandidate = [&](const QString& value) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty() && !candidates.contains(trimmed)) {
            candidates.append(trimmed);
        }
    };

    for (const QString& itemId : m_browsedItemIds) {
        if (itemId == QStringLiteral("#Enabled")
                || itemId == QStringLiteral("#OfflineMode")
                || itemId == QStringLiteral("#MonitorACLFile")
                || itemId.endsWith(QStringLiteral(".#Enabled"))
                || itemId.endsWith(QStringLiteral(".#OfflineMode"))
                || itemId.endsWith(QStringLiteral(".#MonitorACLFile"))
                || itemId.endsWith(QStringLiteral(".@Connected"))) {
            addCandidate(itemId);
        }
    }

    addCandidate(QStringLiteral("#Enabled"));
    addCandidate(QStringLiteral("#OfflineMode"));

    const QString channel = m_config.channelName.trimmed().isEmpty()
            ? QStringLiteral("CommPort")
            : m_config.channelName.trimmed();
    const QString primaryDevice = m_config.metadata.value(QStringLiteral("primaryDeviceName"),
                                                          QStringLiteral("InitDevParamnt")).toString().trimmed();
    const QString secondaryDevice = m_config.metadata.value(QStringLiteral("secondaryDeviceName"),
                                                            QStringLiteral("InitDevConfig")).toString().trimmed();

    if (!channel.isEmpty()) {
        if (!primaryDevice.isEmpty()) {
            addCandidate(QStringLiteral("%1.%2.#Enabled").arg(channel, primaryDevice));
            addCandidate(QStringLiteral("%1.%2.#OfflineMode").arg(channel, primaryDevice));
        }
        if (!secondaryDevice.isEmpty() && secondaryDevice != primaryDevice) {
            addCandidate(QStringLiteral("%1.%2.#Enabled").arg(channel, secondaryDevice));
            addCandidate(QStringLiteral("%1.%2.#OfflineMode").arg(channel, secondaryDevice));
            addCandidate(QStringLiteral("%1.%2.@Connected").arg(channel, secondaryDevice));
        }
    }

    return candidates;
}

bool MatrikonOpcServer::writeItemValue(const QString& pointId, const QVariant& value, QString* errorMessage)
{
#ifndef Q_OS_WIN
    const QString message = QStringLiteral("OPC DA write is unsupported on this platform");
    m_lastWritePointId = pointId;
    m_lastWriteValue = value;
    m_lastWriteSuccess = false;
    m_lastWriteMessage = message;
    m_lastWriteTime = QDateTime::currentDateTimeUtc();
    m_lastStatusChangeTime = m_lastWriteTime;
    m_lastFailedWriteTime = m_lastWriteTime;
    m_lastFailedWriteMessage = message;
    ++m_failedWriteCount;
    if (errorMessage) {
        *errorMessage = message;
    }
    return false;
#else
    auto recordFailure = [&](const QString& message) {
        m_lastWritePointId = pointId;
        m_lastWriteValue = value;
        m_lastWriteSuccess = false;
        m_lastWriteMessage = message;
        m_lastWriteTime = QDateTime::currentDateTimeUtc();
        m_lastStatusChangeTime = m_lastWriteTime;
        m_lastFailedWriteTime = m_lastWriteTime;
        m_lastFailedWriteMessage = message;
        ++m_failedWriteCount;
        if (errorMessage) {
            *errorMessage = message;
        }
    };

    if (!m_syncIO) {
        recordFailure(QStringLiteral("IOPCSyncIO is not available"));
        return false;
    }

    const auto it = m_itemsByPointId.constFind(pointId);
    if (it == m_itemsByPointId.constEnd() || !it->active || it->serverHandle == 0) {
        recordFailure(QStringLiteral("OPC DA item is not active for point %1").arg(pointId));
        return false;
    }
    m_lastWriteItemId = it->itemId;

    VARIANT variant;
    VariantInit(&variant);
    if (!setVariantValue(value, &variant, errorMessage)) {
        recordFailure(errorMessage && !errorMessage->isEmpty()
                      ? *errorMessage
                      : QStringLiteral("OPC DA value conversion failed for point %1").arg(pointId));
        VariantClear(&variant);
        return false;
    }

    OPCHANDLE handle = it->serverHandle;
    HRESULT* errors = nullptr;
    const HRESULT hr = m_syncIO->Write(1, &handle, &variant, &errors);
    VariantClear(&variant);

    const HRESULT itemHr = errors ? errors[0] : hr;
    if (errors) {
        CoTaskMemFree(errors);
    }

    const bool ok = SUCCEEDED(hr) && SUCCEEDED(itemHr);
    m_lastWritePointId = pointId;
    m_lastWriteValue = value;
    m_lastWriteSuccess = ok;
    m_lastWriteTime = QDateTime::currentDateTimeUtc();
    m_lastStatusChangeTime = m_lastWriteTime;
    if (ok) {
        m_lastWriteMessage = QStringLiteral("OPC DA write ok");
        m_lastSuccessfulWriteTime = m_lastWriteTime;
        m_lastSuccessfulWriteMessage = m_lastWriteMessage;
        ++m_successfulWriteCount;
        QString readbackError;
        refreshItems(&readbackError);
    } else {
        m_lastWriteMessage = QStringLiteral("OPC DA write failed: %1 / %2")
                .arg(hresultToString(hr), hresultToString(itemHr));
        m_lastFailedWriteTime = m_lastWriteTime;
        m_lastFailedWriteMessage = m_lastWriteMessage;
        ++m_failedWriteCount;
        if (errorMessage) {
            *errorMessage = m_lastWriteMessage;
        }
    }
    return ok;
#endif
}

void MatrikonOpcServer::activateDataCallbackContext()
{
    if (!m_callbackContext) {
        return;
    }

    QMutexLocker locker(&m_callbackContext->mutex);
    m_callbackContext->owner = this;
    ++m_callbackContext->generation;
    m_callbackContext->active = true;
}

void MatrikonOpcServer::deactivateDataCallbackContext()
{
    if (!m_callbackContext) {
        return;
    }

    QMutexLocker locker(&m_callbackContext->mutex);
    m_callbackContext->active = false;
    ++m_callbackContext->generation;
}

void MatrikonOpcServer::invalidateDataCallbackContext()
{
    if (!m_callbackContext) {
        return;
    }

    QMutexLocker locker(&m_callbackContext->mutex);
    m_callbackContext->active = false;
    ++m_callbackContext->generation;
    m_callbackContext->owner = nullptr;
}

MatrikonOpcServer::DataChangePayload MatrikonOpcServer::copyDataChangePayload(
        unsigned long transactionId,
        unsigned long groupClientHandle,
        long masterQuality,
        long masterError,
        unsigned long count,
        const unsigned long* clientHandles,
        const void* values,
        const unsigned short* qualities,
        const void* timestamps,
        const long* errors) const
{
    DataChangePayload payload;
    payload.transactionId = transactionId;
    payload.groupClientHandle = groupClientHandle;
    payload.masterQuality = masterQuality;
    payload.masterError = masterError;

#ifndef Q_OS_WIN
    Q_UNUSED(count)
    Q_UNUSED(clientHandles)
    Q_UNUSED(values)
    Q_UNUSED(qualities)
    Q_UNUSED(timestamps)
    Q_UNUSED(errors)
#else
    const auto* variants = static_cast<const VARIANT*>(values);
    const auto* fileTimes = static_cast<const FILETIME*>(timestamps);
    const auto* itemErrors = static_cast<const HRESULT*>(errors);
    payload.clientHandles.reserve(static_cast<int>(count));
    payload.values.reserve(static_cast<int>(count));
    payload.qualities.reserve(static_cast<int>(count));
    payload.timestamps.reserve(static_cast<int>(count));
    payload.errors.reserve(static_cast<int>(count));

    for (unsigned long i = 0; i < count; ++i) {
        payload.clientHandles.append(clientHandles ? clientHandles[i] : 0);
        payload.values.append(variants ? variantToQVariant(&variants[i]) : QVariant());
        payload.qualities.append(qualities ? qualities[i] : 0);
        payload.timestamps.append(fileTimes ? fileTimeToDateTime(&fileTimes[i]) : QDateTime());
        payload.errors.append(itemErrors ? itemErrors[i] : S_OK);
    }
#endif
    return payload;
}

void MatrikonOpcServer::enqueueDataChange(unsigned long transactionId,
                                           unsigned long groupClientHandle,
                                           long masterQuality,
                                           long masterError,
                                           unsigned long count,
                                           const unsigned long* clientHandles,
                                           const void* values,
                                           const unsigned short* qualities,
                                           const void* timestamps,
                                           const long* errors)
{
    const auto context = m_callbackContext;
    if (!context) {
        return;
    }

    QMutexLocker locker(&context->mutex);
    if (!context->active || context->owner != this) {
        return;
    }

    enqueueDataChange(context,
                      context->generation,
                      transactionId,
                      groupClientHandle,
                      masterQuality,
                      masterError,
                      count,
                      clientHandles,
                      values,
                      qualities,
                      timestamps,
                      errors);
}

void MatrikonOpcServer::enqueueDataChange(
        const std::shared_ptr<CallbackContext>& context,
        quint64 generation,
        unsigned long transactionId,
        unsigned long groupClientHandle,
        long masterQuality,
        long masterError,
        unsigned long count,
        const unsigned long* clientHandles,
        const void* values,
        const unsigned short* qualities,
        const void* timestamps,
        const long* errors)
{
    if (!context) {
        return;
    }

    const DataChangePayload payload = copyDataChangePayload(transactionId,
                                                            groupClientHandle,
                                                            masterQuality,
                                                            masterError,
                                                            count,
                                                            clientHandles,
                                                            values,
                                                            qualities,
                                                            timestamps,
                                                            errors);
    QMetaObject::invokeMethod(this,
                              [context, generation, payload]() {
                                  QMutexLocker locker(&context->mutex);
                                  if (!context->active
                                          || context->generation != generation
                                          || !context->owner) {
                                      return;
                                  }
                                  context->owner->applyDataChange(payload);
                              },
                              Qt::QueuedConnection);
}

void MatrikonOpcServer::applyDataChange(const DataChangePayload& payload)
{
    ++m_callbackCount;
    m_lastCallbackTime = QDateTime::currentDateTimeUtc();
    const bool masterOk = payload.masterQuality >= 0 && payload.masterError >= 0;
    if (!masterOk) {
        m_lastReadMessage = QStringLiteral("OPC DA callback master failed: quality=%1 error=%2")
                .arg(hresultToString(payload.masterQuality), hresultToString(payload.masterError));
        m_lastErrorCode = CommErrorCode::ConnectionFailed;
        m_lastErrorMessage = m_lastReadMessage;
        m_lastFailedReadTime = m_lastCallbackTime;
    }

    for (int i = 0; i < payload.clientHandles.size(); ++i) {
        const QString pointId = pointIdForClientHandle(payload.clientHandles.at(i));
        if (pointId.isEmpty()) {
            continue;
        }

        const long itemError = i < payload.errors.size() ? payload.errors.at(i) : 0;
        const QVariant value = i < payload.values.size() ? payload.values.at(i) : QVariant();
        ItemBinding binding = m_itemsByPointId.value(pointId);
        binding.lastResult = masterOk
                ? itemError
                : (payload.masterError < 0 ? payload.masterError : payload.masterQuality);
        m_lastReadPointId = pointId;
        m_lastReadItemId = binding.itemId;
        binding.lastQuality = i < payload.qualities.size() ? payload.qualities.at(i) : 0;
        binding.lastTimestamp = i < payload.timestamps.size()
                ? payload.timestamps.at(i)
                : QDateTime();
        m_lastQuality = binding.lastQuality;
        m_lastQualityText = qualityToString(binding.lastQuality);
        m_lastTimestampSource = binding.lastTimestamp.isValid()
                ? QStringLiteral("opc-da")
                : QStringLiteral("local");
        if (masterOk && itemError >= 0 && hasOpcPayload(value)) {
            binding.lastValue = value;

            RuntimePointValue current;
            current.pointId = pointId;
            current.value = binding.lastValue;
            current.quality = qualityToRuntimeQuality(binding.lastQuality);
            current.timestamp = binding.lastTimestamp.isValid()
                    ? binding.lastTimestamp
                    : m_lastCallbackTime;
            current.origin = QStringLiteral("opc-da-callback");
            m_values.insert(pointId, current);
            ++m_successfulReadCount;
            m_lastSuccessfulReadTime = m_lastCallbackTime;
        } else {
            binding.lastValue.clear();
            m_values.remove(pointId);
            if (!masterOk) {
                m_lastReadMessage = QStringLiteral("OPC DA callback item rejected by master status: %1 / %2")
                        .arg(hresultToString(payload.masterQuality),
                             hresultToString(payload.masterError));
            } else if (itemError < 0) {
                m_lastReadMessage = QStringLiteral("OPC DA callback item failed: %1")
                        .arg(hresultToString(itemError));
            } else {
                m_lastReadMessage = QStringLiteral("OPC DA callback item has null or unsupported value");
            }
            m_lastFailedReadTime = m_lastCallbackTime;
            m_lastErrorCode = CommErrorCode::ConnectionFailed;
            m_lastErrorMessage = m_lastReadMessage;
            ++m_failedReadCount;
        }
        m_itemsByPointId.insert(pointId, binding);
    }
}

void MatrikonOpcServer::handleDataChange(unsigned long transactionId,
                                         unsigned long groupClientHandle,
                                         long masterQuality,
                                         long masterError,
                                         unsigned long count,
                                         const unsigned long* clientHandles,
                                         const void* values,
                                         const unsigned short* qualities,
                                         const void* timestamps,
                                         const long* errors)
{
    Q_UNUSED(transactionId)
    Q_UNUSED(groupClientHandle)
    Q_UNUSED(masterQuality)
    Q_UNUSED(masterError)

#ifndef Q_OS_WIN
    Q_UNUSED(count)
    Q_UNUSED(clientHandles)
    Q_UNUSED(values)
    Q_UNUSED(qualities)
    Q_UNUSED(timestamps)
    Q_UNUSED(errors)
#else
    applyDataChange(copyDataChangePayload(transactionId,
                                          groupClientHandle,
                                          masterQuality,
                                          masterError,
                                          count,
                                          clientHandles,
                                          values,
                                          qualities,
                                          timestamps,
                                          errors));
#endif
}

void MatrikonOpcServer::remapItemsFromBrowseResults()
{
    m_matchedItemCount = 0;
    m_unmatchedItemCount = 0;
    m_lastMatchedItemId.clear();
    m_lastUnmatchedItemId.clear();

    if (m_browsedItemIds.isEmpty()) {
        m_unmatchedItemCount = m_itemsByPointId.size();
        return;
    }

    QSet<QString> browsedExact;
    QHash<QString, QString> browsedByLower;
    for (const QString& itemId : std::as_const(m_browsedItemIds)) {
        browsedExact.insert(itemId);
        browsedByLower.insert(itemId.toLower(), itemId);
    }

    for (auto it = m_itemsByPointId.begin(); it != m_itemsByPointId.end(); ++it) {
        QString matched;
        for (const QString& candidate : std::as_const(it->candidateItemIds)) {
            if (browsedExact.contains(candidate)) {
                matched = candidate;
                break;
            }
            const QString lower = candidate.toLower();
            if (browsedByLower.contains(lower)) {
                matched = browsedByLower.value(lower);
                break;
            }
        }

        if (matched.isEmpty()) {
            ++m_unmatchedItemCount;
            m_lastUnmatchedItemId = it->itemId;
            continue;
        }

        it->itemId = matched;
        ++m_matchedItemCount;
        m_lastMatchedItemId = matched;
    }
}

QStringList MatrikonOpcServer::itemIdCandidatesForTag(const OpcTagDefinition& tag) const
{
    QStringList candidates;
    auto addCandidate = [&](const QString& value) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty() && !candidates.contains(trimmed)) {
            candidates.append(trimmed);
        }
    };

    addCandidate(tag.metadata.value(QStringLiteral("opcItemId")).toString());
    addCandidate(tag.addressing.value(QStringLiteral("opcItemId")).toString());

    const QString rawItem = tag.item.trimmed();
    const QString tagName = tag.tagName.trimmed();
    addCandidate(rawItem);
    addCandidate(tagName);

    QStringList leaves;
    auto addLeaf = [&](const QString& value) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty() && !leaves.contains(trimmed)) {
            leaves.append(trimmed);
            addCandidate(trimmed);
        }
    };
    addLeaf(rawItem);
    addLeaf(tagName);

    auto readLongLong = [&](const QStringList& keys, qlonglong defaultValue) {
        for (const QString& key : keys) {
            if (!tag.addressing.contains(key)) {
                continue;
            }
            bool ok = false;
            const qlonglong value = tag.addressing.value(key).toLongLong(&ok);
            if (ok) {
                return value;
            }
        }
        return defaultValue;
    };

    const qlonglong address = readLongLong({
            QStringLiteral("address"),
            QStringLiteral("regAddress"),
            QStringLiteral("register"),
            QStringLiteral("pointAddress"),
            QStringLiteral("offset")
    }, -1);
    if (address >= 0) {
        const QString area = tag.addressing.value(QStringLiteral("area")).toString().trimmed().toLower();
        const QString registerType = tag.addressing.value(QStringLiteral("registerType")).toString().trimmed().toLower();
        QString fileType = QStringLiteral("4");
        if (area == QStringLiteral("coil") || area == QStringLiteral("coils")
                || registerType == QStringLiteral("coil") || registerType == QStringLiteral("coils")) {
            fileType = QStringLiteral("0");
        } else if (area == QStringLiteral("discrete") || area == QStringLiteral("discreteinput")
                   || area == QStringLiteral("discrete-input")
                   || registerType == QStringLiteral("discrete") || registerType == QStringLiteral("discreteinput")
                   || registerType == QStringLiteral("discrete-input")) {
            fileType = QStringLiteral("1");
        } else if (area == QStringLiteral("input") || area == QStringLiteral("inputregister")
                   || area == QStringLiteral("input-register")
                   || registerType == QStringLiteral("input") || registerType == QStringLiteral("inputregister")
                   || registerType == QStringLiteral("input-register")) {
            fileType = QStringLiteral("3");
        }

        const QString baseLeaf = QStringLiteral("%1:%2").arg(fileType).arg(address);
        addLeaf(baseLeaf);

        const qlonglong elementCount = readLongLong({
                QStringLiteral("elementCount"),
                QStringLiteral("length"),
                QStringLiteral("count")
        }, 1);
        if (elementCount > 1) {
            addLeaf(QStringLiteral("%1:%2").arg(baseLeaf).arg(elementCount));
        }

        const qlonglong bitIndex = readLongLong({
                QStringLiteral("bitIndex"),
                QStringLiteral("bitOffset"),
                QStringLiteral("bit")
        }, -1);
        if (bitIndex >= 0) {
            addLeaf(QStringLiteral("%1/%2").arg(baseLeaf).arg(bitIndex));
        }

        if (address == 0) {
            const QString oneBasedLeaf = QStringLiteral("%1:1").arg(fileType);
            addLeaf(oneBasedLeaf);
            if (elementCount > 1) {
                addLeaf(QStringLiteral("%1:%2").arg(oneBasedLeaf).arg(elementCount));
            }
            if (bitIndex >= 0) {
                addLeaf(QStringLiteral("%1/%2").arg(oneBasedLeaf).arg(bitIndex));
            }
        }
    }

    const QString channel = tag.addressing.value(QStringLiteral("opcChannel")).toString().trimmed().isEmpty()
            ? m_config.channelName.trimmed()
            : tag.addressing.value(QStringLiteral("opcChannel")).toString().trimmed();
    const QString primaryDevice = tag.addressing.value(QStringLiteral("opcDevice")).toString().trimmed().isEmpty()
            ? m_config.metadata.value(QStringLiteral("primaryDeviceName"), QStringLiteral("InitDevParamnt")).toString().trimmed()
            : tag.addressing.value(QStringLiteral("opcDevice")).toString().trimmed();
    const QString secondaryDevice = m_config.metadata.value(QStringLiteral("secondaryDeviceName"), QStringLiteral("InitDevConfig")).toString().trimmed();

    if (!channel.isEmpty()) {
        for (const QString& leaf : std::as_const(leaves)) {
            if (!primaryDevice.isEmpty()) {
                addCandidate(QStringLiteral("%1.%2.%3").arg(channel, primaryDevice, leaf));
                addCandidate(QStringLiteral("%1/%2/%3").arg(channel, primaryDevice, leaf));
            }
            if (!secondaryDevice.isEmpty() && secondaryDevice != primaryDevice) {
                addCandidate(QStringLiteral("%1.%2.%3").arg(channel, secondaryDevice, leaf));
                addCandidate(QStringLiteral("%1/%2/%3").arg(channel, secondaryDevice, leaf));
            }
        }
    }

    return candidates;
}

QString MatrikonOpcServer::itemIdForTag(const OpcTagDefinition& tag) const
{
    const QStringList candidates = itemIdCandidatesForTag(tag);
    return candidates.isEmpty() ? QString() : candidates.first();
}

QString MatrikonOpcServer::pointIdForClientHandle(unsigned long clientHandle) const
{
    return m_pointIdByClientHandle.value(clientHandle);
}

bool MatrikonOpcServer::validateConfig(const OpcServerConfig& config, QString* errorMessage)
{
    if (config.opcProgId.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("OPC ProgID must not be empty");
        }
        return false;
    }

    if (config.publishIntervalMs <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Publish interval must be greater than 0");
        }
        return false;
    }

    return true;
}

QString MatrikonOpcServer::hresultToString(long hr)
{
    return QStringLiteral("0x%1").arg(static_cast<unsigned long>(hr), 8, 16, QLatin1Char('0')).toUpper();
}

RuntimePointQuality MatrikonOpcServer::qualityToRuntimeQuality(unsigned short quality)
{
    switch (quality & 0xC0) {
    case 0xC0:
        return RuntimePointQuality::Good;
    case 0x40:
        return RuntimePointQuality::Stale;
    case 0x00:
        return RuntimePointQuality::Bad;
    default:
        return RuntimePointQuality::Unknown;
    }
}

QString MatrikonOpcServer::qualityToString(unsigned short quality)
{
    QString category;
    switch (quality & 0xC0) {
    case 0xC0:
        category = QStringLiteral("Good");
        break;
    case 0x40:
        category = QStringLiteral("Uncertain");
        break;
    case 0x00:
        category = QStringLiteral("Bad");
        break;
    default:
        category = QStringLiteral("Unknown");
        break;
    }
    return QStringLiteral("%1(0x%2)")
            .arg(category,
                 QString::number(quality, 16).rightJustified(4, QLatin1Char('0')).toUpper());
}

QDateTime MatrikonOpcServer::fileTimeToDateTime(const void* fileTime)
{
#ifndef Q_OS_WIN
    Q_UNUSED(fileTime)
    return {};
#else
    const auto* ft = static_cast<const FILETIME*>(fileTime);
    if (!ft || (ft->dwHighDateTime == 0 && ft->dwLowDateTime == 0)) {
        return {};
    }
    ULARGE_INTEGER value;
    value.LowPart = ft->dwLowDateTime;
    value.HighPart = ft->dwHighDateTime;
    const qint64 msecsSince1601 = static_cast<qint64>(value.QuadPart / 10000ULL);
    return QDateTime::fromMSecsSinceEpoch(msecsSince1601 - 11644473600000LL, Qt::UTC);
#endif
}

QVariant MatrikonOpcServer::variantToQVariant(const void* variant)
{
#ifndef Q_OS_WIN
    Q_UNUSED(variant)
    return {};
#else
    const auto* v = static_cast<const VARIANT*>(variant);
    if (!v) {
        return {};
    }
    switch (v->vt) {
    case VT_EMPTY:
    case VT_NULL:
    case VT_ERROR:
        return {};
    case VT_BOOL:
        return v->boolVal == VARIANT_TRUE;
    case VT_I1:
        return static_cast<int>(v->cVal);
    case VT_UI1:
        return static_cast<uint>(v->bVal);
    case VT_I2:
        return static_cast<int>(v->iVal);
    case VT_UI2:
        return static_cast<uint>(v->uiVal);
    case VT_I4:
        return static_cast<int>(v->lVal);
    case VT_UI4:
        return static_cast<uint>(v->ulVal);
    case VT_INT:
        return static_cast<int>(v->intVal);
    case VT_UINT:
        return static_cast<uint>(v->uintVal);
    case VT_I8:
        return static_cast<qlonglong>(v->llVal);
    case VT_UI8:
        return static_cast<qulonglong>(v->ullVal);
    case VT_R4:
        return static_cast<float>(v->fltVal);
    case VT_R8:
        return v->dblVal;
    case VT_BSTR:
        return v->bstrVal ? QString::fromWCharArray(v->bstrVal) : QVariant();
    case VT_LPSTR:
        return v->pszVal ? QString::fromLocal8Bit(v->pszVal) : QVariant();
    case VT_LPWSTR:
        return v->pwszVal ? QString::fromWCharArray(v->pwszVal) : QVariant();
    default:
        return {};
    }
#endif
}

bool MatrikonOpcServer::setVariantValue(const QVariant& value, void* variant, QString* errorMessage)
{
#ifndef Q_OS_WIN
    Q_UNUSED(value)
    Q_UNUSED(variant)
    Q_UNUSED(errorMessage)
    return false;
#else
    auto* v = static_cast<VARIANT*>(variant);
    if (!v) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("VARIANT output is null");
        }
        return false;
    }
    if (!value.isValid() || value.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("OPC DA cannot write a null value");
        }
        return false;
    }

    switch (value.type()) {
    case QVariant::Bool:
        v->vt = VT_BOOL;
        v->boolVal = value.toBool() ? VARIANT_TRUE : VARIANT_FALSE;
        return true;
    case QVariant::Int:
        v->vt = VT_I4;
        v->lVal = value.toInt();
        return true;
    case QVariant::UInt:
        v->vt = VT_UI4;
        v->ulVal = value.toUInt();
        return true;
    case QVariant::LongLong:
        v->vt = VT_I8;
        v->llVal = static_cast<LONGLONG>(value.toLongLong());
        return true;
    case QVariant::ULongLong:
        v->vt = VT_UI8;
        v->ullVal = static_cast<ULONGLONG>(value.toULongLong());
        return true;
    case QVariant::Double:
        v->vt = VT_R8;
        v->dblVal = value.toDouble();
        return true;
    case QVariant::String: {
        v->vt = VT_BSTR;
        const std::wstring text = value.toString().toStdWString();
        v->bstrVal = SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
        if (!v->bstrVal) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("BSTR allocation failed");
            }
            return false;
        }
        return true;
    }
    default:
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unsupported OPC DA write value type: %1")
                    .arg(value.typeName() ? QString::fromLatin1(value.typeName()) : QStringLiteral("unknown"));
        }
        return false;
    }
#endif
}
