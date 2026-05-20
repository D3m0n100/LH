// 文件：src/communication/DownloadProfile.h
// 下载步骤配置（配置驱动框架，便于后续填充厂家命令细节）

#ifndef DOWNLOADPROFILE_H
#define DOWNLOADPROFILE_H

#include <QByteArray>
#include <QVariantMap>
#include <QVector>
#include <QString>

/**
 * @brief DownloadProfile
 *
 * - 本类只描述“步骤”和“参数”，不绑定具体通信实现
 * - 目标：把下载流程做成配置驱动（JSON/ini），便于调整寄存器地址、触发方式、轮询条件等
 */
class DownloadProfile
{
public:
    enum class StepType {
        Enter,      // 进入下载模式（写寄存器/线圈等）
        SendChunk,  // 发送数据块
        Poll,       // 轮询状态寄存器直到满足条件
        Finalize,   // 收尾（写寄存器触发完成）
        QueryResult // 读取结果寄存器
    };

    struct Step {
        StepType type = StepType::Enter;
        QVariantMap params;
    };

    QString name;
    int slaveId = 1;
    QVector<Step> steps;

    static bool fromJson(const QByteArray& json, DownloadProfile& out, QString* err = nullptr);
    static bool fromJsonFile(const QString& path, DownloadProfile& out, QString* err = nullptr);

    static QString stepTypeToString(StepType t);
    static StepType stepTypeFromString(const QString& s, bool* ok = nullptr);
};

#endif // DOWNLOADPROFILE_H
