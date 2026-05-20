// 文件：src/communication/IDownloadTransport.h
// 下载传输接口（可插拔）

#ifndef IDOWNLOADTRANSPORT_H
#define IDOWNLOADTRANSPORT_H

#include <QObject>
#include <QByteArray>
#include <QVariantMap>

/**
 * @brief IDownloadTransport
 *
 * 与具体厂家下载命令集解耦：
 * - 上层可按 DownloadProfile 的 steps 驱动调用这些方法
 */
class IDownloadTransport : public QObject
{
    Q_OBJECT
public:
    explicit IDownloadTransport(QObject* parent = nullptr) : QObject(parent) {}
    ~IDownloadTransport() override = default;

    virtual bool enter(const QVariantMap& params) = 0;
    virtual bool sendChunk(const QByteArray& chunk, const QVariantMap& params) = 0;
    virtual bool finalize(const QVariantMap& params) = 0;

    virtual bool queryResult(QVariantMap& outResult, const QVariantMap& params) = 0;
};

#endif // IDOWNLOADTRANSPORT_H
