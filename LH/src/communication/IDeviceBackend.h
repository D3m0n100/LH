// File: src/communication/IDeviceBackend.h
// 设备后端统一接口 — 运行时设备层抽象
// ICommInterface 保持协议层角色，IDeviceBackend 是运行设备层

#ifndef IDEVICEBACKEND_H
#define IDEVICEBACKEND_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QVariant>
#include <QVariantMap>

class IDeviceBackend : public QObject
{
    Q_OBJECT
public:
    explicit IDeviceBackend(QObject* parent = nullptr)
        : QObject(parent) {}
    virtual ~IDeviceBackend() = default;

    // ── 连接生命周期 ─────────────────────────────────────

    virtual bool connectBackend() = 0;
    virtual void disconnectBackend() = 0;
    virtual bool isOnline() const = 0;

    // ── 点位读写 ─────────────────────────────────────────

    /**
     * @brief 批量读取点位当前值
     * @param pointIds  要读取的点 ID 列表
     * @param values    [out] 成功读到的 id -> value
     * @param errorMessage [out] 错误信息
     * @return 全部成功返回 true
     */
    virtual bool readPoints(const QStringList& pointIds,
                            QHash<QString, QVariant>& values,
                            QString* errorMessage = nullptr) = 0;

    /**
     * @brief 批量写入点位值
     * @param writes      id -> 新值
     * @param errorMessage [out] 错误信息
     * @return 全部成功返回 true
     */
    virtual bool writePoints(const QHash<QString, QVariant>& writes,
                             QString* errorMessage = nullptr) = 0;

    // ── 下载 ─────────────────────────────────────────────

    /**
     * @brief 下载编译产物到设备
     * @param artifactPath  产物文件路径（.code 等）
     * @param options       下载选项（协议参数、超时等）
     * @param errorMessage  [out] 错误信息
     * @return 成功返回 true
     */
    virtual bool downloadArtifact(const QString& artifactPath,
                                  const QVariantMap& options,
                                  QString* errorMessage = nullptr) = 0;

    // ── 状态查询 ─────────────────────────────────────────

    /**
     * @brief 查询设备状态
     * @return 状态键值对（如 "online", "firmware", "errorCount" 等）
     */
    virtual QVariantMap queryStatus() const = 0;

signals:
    void connectionStateChanged(bool connected);
    void pointsChanged(const QHash<QString, QVariant>& updates);
    void errorOccurred(const QString& errorMessage);
};

#endif // IDEVICEBACKEND_H
