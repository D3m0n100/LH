#ifndef MATRIKONOPCSERVER_H
#define MATRIKONOPCSERVER_H

#include "IOpcServer.h"

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QVariant>
#include <QVector>

#include <memory>

struct IOPCItemMgt;
struct IOPCBrowseServerAddressSpace;
struct IOPCServer;
struct IOPCSyncIO;
struct IConnectionPoint;
struct IUnknown;

class MatrikonOpcDataCallback;

class MatrikonOpcServer : public IOpcServer
{
    Q_OBJECT
public:
    explicit MatrikonOpcServer(QObject* parent = nullptr);
    ~MatrikonOpcServer() override;

    bool applyConfig(const OpcServerConfig& config, QString* errorMessage = nullptr) override;
    bool start(QString* errorMessage = nullptr) override;
    void stop() override;
    bool isRunning() const override { return m_running; }

    void setRuntimePoints(const QList<RuntimePointDefinition>& points) override;
    void setOpcTags(const QList<OpcTagDefinition>& tags) override;
    void updatePointValues(const QList<RuntimePointValue>& values) override;
    void recordWriteResult(const QString& pointId, bool success, const QString& message) override;
    BackendStatusSnapshot statusSnapshot() const override;

private:
    friend class MatrikonOpcDataCallback;

    struct ItemBinding
    {
        QString pointId;
        QString itemId;
        QStringList candidateItemIds;
        unsigned long serverHandle = 0;
        unsigned long clientHandle = 0;
        bool active = false;
        long lastResult = 0;
        QVariant lastValue;
        unsigned short lastQuality = 0;
        QDateTime lastTimestamp;
    };

    struct DataChangePayload
    {
        unsigned long transactionId = 0;
        unsigned long groupClientHandle = 0;
        long masterQuality = 0;
        long masterError = 0;
        QVector<unsigned long> clientHandles;
        QVector<QVariant> values;
        QVector<unsigned short> qualities;
        QVector<QDateTime> timestamps;
        QVector<long> errors;
    };

    struct CallbackContext
    {
        QMutex mutex;
        MatrikonOpcServer* owner = nullptr;
        quint64 generation = 0;
        bool active = false;
    };

    bool ensureComInitialized(QString* errorMessage);
    bool createServerInstance(QString* errorMessage);
    bool createOpcGroup(QString* errorMessage);
    bool browseServerAddressSpace(QString* errorMessage = nullptr);
    bool subscribeDataChanges(QString* errorMessage = nullptr);
    void unsubscribeDataChanges();
    void releaseOpcGroup();
    void releaseServerInstance();
    void rebuildPendingItems();
    bool addPendingItems(QString* errorMessage = nullptr);
    bool refreshItems(QString* errorMessage = nullptr);
    bool runReadProbe(QString* errorMessage = nullptr);
    bool readProbeHandle(const QString& pointId,
                         const QString& itemId,
                         unsigned long serverHandle,
                         bool updateRuntimeValue,
                         QString* errorMessage = nullptr);
    bool readTemporaryProbeItem(const QString& itemId, QString* errorMessage = nullptr);
    QStringList readProbeItemCandidates() const;
    bool writeItemValue(const QString& pointId, const QVariant& value, QString* errorMessage = nullptr);
    void updateConfigurationProbe();
    void handleDataChange(unsigned long transactionId,
                          unsigned long groupClientHandle,
                          long masterQuality,
                          long masterError,
                          unsigned long count,
                          const unsigned long* clientHandles,
                          const void* values,
                          const unsigned short* qualities,
                          const void* timestamps,
                          const long* errors);
    void enqueueDataChange(unsigned long transactionId,
                           unsigned long groupClientHandle,
                           long masterQuality,
                           long masterError,
                           unsigned long count,
                           const unsigned long* clientHandles,
                           const void* values,
                           const unsigned short* qualities,
                           const void* timestamps,
                           const long* errors);
    void enqueueDataChange(const std::shared_ptr<CallbackContext>& context,
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
                           const long* errors);
    DataChangePayload copyDataChangePayload(unsigned long transactionId,
                                            unsigned long groupClientHandle,
                                            long masterQuality,
                                            long masterError,
                                            unsigned long count,
                                            const unsigned long* clientHandles,
                                            const void* values,
                                            const unsigned short* qualities,
                                            const void* timestamps,
                                            const long* errors) const;
    void applyDataChange(const DataChangePayload& payload);
    void activateDataCallbackContext();
    void deactivateDataCallbackContext();
    void invalidateDataCallbackContext();
    void remapItemsFromBrowseResults();
    QStringList itemIdCandidatesForTag(const OpcTagDefinition& tag) const;
    QString itemIdForTag(const OpcTagDefinition& tag) const;
    QString pointIdForClientHandle(unsigned long clientHandle) const;
    static bool validateConfig(const OpcServerConfig& config, QString* errorMessage);
    static QString hresultToString(long hr);
    static RuntimePointQuality qualityToRuntimeQuality(unsigned short quality);
    static QString qualityToString(unsigned short quality);
    static QDateTime fileTimeToDateTime(const void* fileTime);
    static QVariant variantToQVariant(const void* variant);
    static bool setVariantValue(const QVariant& value, void* variant, QString* errorMessage = nullptr);

    OpcServerConfig m_config;
    bool m_running = false;
    bool m_comInitialized = false;
    IUnknown* m_server = nullptr;
    IOPCServer* m_opcServer = nullptr;
    IOPCBrowseServerAddressSpace* m_browser = nullptr;
    IUnknown* m_groupUnknown = nullptr;
    IOPCItemMgt* m_itemMgt = nullptr;
    IOPCSyncIO* m_syncIO = nullptr;
    IConnectionPoint* m_dataCallbackConnectionPoint = nullptr;
    IUnknown* m_dataCallback = nullptr;
    unsigned long m_subscriptionCookie = 0;
    std::shared_ptr<CallbackContext> m_callbackContext;
    unsigned long m_groupServerHandle = 0;
    QList<RuntimePointDefinition> m_points;
    QList<OpcTagDefinition> m_tags;
    QHash<QString, RuntimePointValue> m_values;
    QHash<QString, ItemBinding> m_itemsByPointId;
    QHash<QString, QString> m_pointIdByItemId;
    QHash<unsigned long, QString> m_pointIdByClientHandle;
    QStringList m_browsedItemIds;
    QDateTime m_lastStatusChangeTime;
    QDateTime m_lastReadTime;
    QDateTime m_lastSuccessfulReadTime;
    QDateTime m_lastFailedReadTime;
    QDateTime m_lastWriteTime;
    QDateTime m_lastSuccessfulWriteTime;
    QDateTime m_lastFailedWriteTime;
    CommErrorCode m_lastErrorCode = CommErrorCode::NoError;
    QString m_lastErrorMessage;
    bool m_browseSucceeded = false;
    QString m_lastBrowseMessage;
    bool m_subscriptionActive = false;
    QString m_lastSubscriptionMessage;
    bool m_expectedChannelVisible = false;
    bool m_primaryDeviceVisible = false;
    bool m_secondaryDeviceVisible = false;
    bool m_standardItemsVisible = false;
    QString m_configurationProbeMessage;
    int m_callbackCount = 0;
    QDateTime m_lastCallbackTime;
    int m_matchedItemCount = 0;
    int m_unmatchedItemCount = 0;
    QString m_lastMatchedItemId;
    QString m_lastUnmatchedItemId;
    QString m_lastReadPointId;
    QString m_lastReadItemId;
    QString m_lastReadMessage;
    unsigned short m_lastQuality = 0;
    QString m_lastQualityText;
    QString m_lastTimestampSource;
    QString m_lastWritePointId;
    QString m_lastWriteItemId;
    QVariant m_lastWriteValue;
    bool m_lastWriteSuccess = false;
    QString m_lastWriteMessage;
    int m_successfulReadCount = 0;
    int m_failedReadCount = 0;
    bool m_readProbeAttempted = false;
    bool m_readProbeOk = false;
    QString m_readProbePointId;
    QString m_readProbeItemId;
    QVariant m_readProbeValue;
    unsigned short m_readProbeQuality = 0;
    QString m_readProbeQualityText;
    QDateTime m_readProbeTimestamp;
    QDateTime m_readProbeTime;
    QString m_readProbeMessage;
    QString m_lastSuccessfulWriteMessage;
    QString m_lastFailedWriteMessage;
    int m_successfulWriteCount = 0;
    int m_failedWriteCount = 0;
    int m_successfulItemCount = 0;
    int m_failedItemCount = 0;
    unsigned long m_nextClientHandle = 1;
};

#endif // MATRIKONOPCSERVER_H
