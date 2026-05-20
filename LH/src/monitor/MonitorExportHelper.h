// 文件：src/monitor/MonitorExportHelper.h
// 监控数据导出助手
// 
// 功能完善版本：
// - 支持 CSV / JSON / TSV 三种格式导出
// - 完整的通道元信息和时间戳语义
// - 健壮的错误处理和日志输出

#ifndef MONITOR_EXPORT_HELPER_H
#define MONITOR_EXPORT_HELPER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QPointF>
#include <QPixmap>
#include <QDir>
#include <QRegularExpression>
#include <QMap>
#include <QDateTime>
#include <functional>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace Monitor {
struct Sample;
}

class MonitorChartView;
struct ProcessedChannelData;

// ============================================================================
// 导出配置
// ============================================================================

/**
 * @brief 导出配置
 */
struct ExportConfig
{
    QString defaultPath;                ///< 默认导出路径
    QString defaultFileName;            ///< 默认文件名前缀
    bool showProgressDialog = false;    ///< 是否显示进度对话框
    bool overwriteExisting = false;     ///< 是否覆盖已存在文件
    int imageQuality = 90;              ///< 图像质量 (1-100)
    int csvPrecision = 6;               ///< CSV 数值精度
    QString csvSeparator = ",";         ///< CSV 分隔符
    bool csvIncludeHeader = true;       ///< CSV 是否包含表头
    QString timestampFormat = "yyyy-MM-dd hh:mm:ss.zzz"; ///< 时间戳格式
    bool includeMetadataComments = true; ///< 是否在文件头包含元数据注释
    bool alignMultiChannelByTime = true; ///< 多通道导出时是否按时间对齐
};

// ============================================================================
// 导出结果
// ============================================================================

/**
 * @brief 导出结果
 */
struct ExportResult
{
    bool success = false;
    QString filePath;
    QString errorMessage;
    int exportedCount = 0;              ///< 导出的数据点/行数
    qint64 fileSizeBytes = 0;           ///< 文件大小
    QString exportFormat;               ///< 导出格式
    
    /// 是否成功
    operator bool() const { return success; }
};

// ============================================================================
// 导出数据结构（新增）
// ============================================================================

/**
 * @brief 单个通道的元信息
 */
struct ExportChannelInfo
{
    QString channelId;          ///< 通道唯一标识
    QString displayName;        ///< 通道显示名称
    QString unit;               ///< 数据单位
    int samplePeriodMs = 100;   ///< 采样周期（毫秒）
    int sampleCount = 0;        ///< 样本数量
    
    ExportChannelInfo() = default;
    ExportChannelInfo(const QString& id, const QString& name, 
                      const QString& u, int period = 100)
        : channelId(id), displayName(name), unit(u), samplePeriodMs(period) {}
};

/**
 * @brief 导出元数据
 */
struct ExportMetadata
{
    QString projectName;        ///< 项目名称（可选）
    QDateTime exportTime;       ///< 导出时间
    qint64 timeWindowMs = 30000;///< 数据时间窗口（毫秒）
    QString exportFormat;       ///< 导出格式 (CSV/JSON/TSV)
    int totalChannels = 0;      ///< 通道总数
    int totalSamples = 0;       ///< 样本总数
    QString softwareVersion;    ///< 软件版本
    QVariantMap customFields;   ///< 自定义扩展字段
    
    ExportMetadata() 
        : exportTime(QDateTime::currentDateTime())
        , softwareVersion("1.0.0") {}
};

/**
 * @brief 完整导出数据包
 * 
 * 封装导出所需的所有数据，包括：
 * - 元数据信息
 * - 通道定义列表
 * - 各通道的采样数据
 */
struct ExportDataPackage
{
    ExportMetadata metadata;
    QList<ExportChannelInfo> channelInfos;
    QMap<QString, QList<Monitor::Sample>> channelSamples;
    
    /// 检查数据包是否有效
    bool isValid() const {
        return !channelInfos.isEmpty() && !channelSamples.isEmpty();
    }
    
    /// 获取所有通道的样本总数
    int totalSampleCount() const {
        int total = 0;
        for (auto it = channelSamples.constBegin(); it != channelSamples.constEnd(); ++it) {
            total += it.value().size();
        }
        return total;
    }
    
    /// 清空数据
    void clear() {
        channelInfos.clear();
        channelSamples.clear();
        metadata = ExportMetadata();
    }
};

// ============================================================================
// MonitorExportHelper 类
// ============================================================================

/**
 * @brief MonitorExportHelper
 * 
 * 监控数据导出助手，职责：
 * - 图表图像导出（PNG/JPG/SVG）
 * - 数据导出（CSV/JSON/TSV）
 * - 文件对话框管理
 * - 统一的错误处理和结果通知
 * 
 * 【设计原则】
 * - 分离"获取数据"和"写文件"步骤
 * - 所有导出方法返回 ExportResult
 * - 通过信号通知导出进度和结果
 * - 易于扩展新的导出格式
 * 
 * 【支持的格式】
 * - CSV: 逗号分隔，适合 Excel 和通用数据交换
 * - JSON: 结构化数据，适合程序间交换
 * - TSV: 制表符分隔，适合直接用 Excel 打开
 * - PNG/JPG/SVG: 图表图像导出
 */
class MonitorExportHelper : public QObject
{
    Q_OBJECT

public:
    explicit MonitorExportHelper(QObject* parent = nullptr);
    ~MonitorExportHelper() override;

    // ===== 配置 =====
    
    ExportConfig config() const { return m_config; }
    void setConfig(const ExportConfig& config);
    void setParentWidget(QWidget* parent);

    // =========================================================================
    // 图像导出（带文件对话框）
    // =========================================================================
    
    ExportResult exportChartAsPng(MonitorChartView* chartView,
                                   const QString& suggestedFileName = QString());
    ExportResult exportChartAsJpg(MonitorChartView* chartView,
                                   const QString& suggestedFileName = QString());
    ExportResult exportChartAsSvg(MonitorChartView* chartView,
                                   const QString& suggestedFileName = QString());

    // =========================================================================
    // 图像导出（指定路径）
    // =========================================================================
    
    ExportResult exportChartAsPngToFile(MonitorChartView* chartView,
                                         const QString& filePath);
    ExportResult exportChartAsJpgToFile(MonitorChartView* chartView,
                                         const QString& filePath);
    ExportResult exportChartAsSvgToFile(MonitorChartView* chartView,
                                         const QString& filePath);
    
    /// 导出 QPixmap
    ExportResult exportPixmap(const QPixmap& pixmap,
                               const QString& filePath,
                               const QString& format = "PNG");

    // =========================================================================
    // 数据导出（带文件对话框）- 单通道
    // =========================================================================
    
    ExportResult exportDataAsCsv(const QString& channelName,
                                  const QList<Monitor::Sample>& samples,
                                  const QString& suggestedFileName = QString());
    
    ExportResult exportDataAsJson(const QString& channelName,
                                   const QList<Monitor::Sample>& samples,
                                   const QString& suggestedFileName = QString());
    
    ExportResult exportDataAsTsv(const QString& channelName,
                                  const QList<Monitor::Sample>& samples,
                                  const QString& suggestedFileName = QString());

    // =========================================================================
    // 数据导出（指定路径）- 单通道
    // =========================================================================
    
    ExportResult exportDataAsCsvToFile(const QString& channelName,
                                        const QList<Monitor::Sample>& samples,
                                        const QString& filePath);
    
    ExportResult exportDataAsJsonToFile(const QString& channelName,
                                         const QList<Monitor::Sample>& samples,
                                         const QString& filePath);
    
    ExportResult exportDataAsTsvToFile(const QString& channelName,
                                        const QList<Monitor::Sample>& samples,
                                        const QString& filePath);

    // =========================================================================
    // 数据导出（完整数据包）- 多通道支持
    // =========================================================================
    
    /**
     * @brief 导出完整数据包为 CSV
     * @param package 导出数据包
     * @param filePath 目标文件路径
     * @return 导出结果
     */
    ExportResult exportPackageAsCsv(const ExportDataPackage& package,
                                     const QString& filePath);
    
    /**
     * @brief 导出完整数据包为 JSON
     * @param package 导出数据包
     * @param filePath 目标文件路径
     * @return 导出结果
     */
    ExportResult exportPackageAsJson(const ExportDataPackage& package,
                                      const QString& filePath);
    
    /**
     * @brief 导出完整数据包为 TSV
     * @param package 导出数据包
     * @param filePath 目标文件路径
     * @return 导出结果
     */
    ExportResult exportPackageAsTsv(const ExportDataPackage& package,
                                     const QString& filePath);

    // =========================================================================
    // 通用导出接口
    // =========================================================================
    
    /**
     * @brief 根据文件扩展名自动选择导出格式
     * @param package 导出数据包
     * @param filePath 目标文件路径（根据扩展名判断格式）
     * @return 导出结果
     */
    ExportResult exportPackageAuto(const ExportDataPackage& package,
                                    const QString& filePath);
    
    /**
     * @brief 弹出保存对话框并导出数据包
     * @param package 导出数据包
     * @param suggestedFileName 建议的文件名
     * @return 导出结果
     */
    ExportResult exportPackageWithDialog(const ExportDataPackage& package,
                                          const QString& suggestedFileName = QString());

    // =========================================================================
    // 工具方法
    // =========================================================================
    
    /// 生成默认文件名
    QString generateDefaultFileName(const QString& channelName,
                                     const QString& extension) const;
    
    /// 生成多通道默认文件名
    QString generateMultiChannelFileName(const QString& extension) const;
    
    /// 获取支持的数据格式过滤器字符串
    static QString dataFormatFilter();
    
    /// 获取支持的图像格式过滤器字符串
    static QString imageFormatFilter();
    
    /// 从文件路径获取格式类型
    static QString formatFromPath(const QString& filePath);

signals:
    /// 导出进度（0-100）
    void exportProgress(int percent);
    
    /// 导出完成
    void exportFinished(const ExportResult& result);
    
    /// 导出错误
    void exportError(const QString& errorMessage);

private:
    // =========================================================================
    // 内部实现方法
    // =========================================================================
    
    /// 显示保存图像对话框
    QString showSaveImageDialog(const QString& suggestedFileName,
                                 const QString& filter);
    
    /// 显示保存数据对话框
    QString showSaveDataDialog(const QString& suggestedFileName,
                                const QString& filter);
    
    /// 确保文件扩展名正确
    QString ensureExtension(const QString& filePath, 
                            const QString& extension) const;
    
    /// 检查文件路径有效性
    bool validateFilePath(const QString& filePath, QString& errorMsg) const;
    
    /// 写入文件并获取文件大小
    bool writeFileAndGetSize(const QString& filePath, 
                              const QByteArray& data,
                              qint64& fileSize,
                              QString& errorMsg);
    
    // ===== CSV 生成 =====
    
    /// 生成单通道 CSV 内容
    QByteArray generateCsvContent(const QString& channelName,
                                   const QList<Monitor::Sample>& samples);
    
    /// 生成多通道 CSV 内容
    QByteArray generateMultiChannelCsvContent(const ExportDataPackage& package);
    
    // ===== JSON 生成 =====
    
    /// 生成单通道 JSON 内容
    QByteArray generateJsonContent(const QString& channelName,
                                    const QList<Monitor::Sample>& samples);
    
    /// 生成多通道 JSON 内容
    QByteArray generateMultiChannelJsonContent(const ExportDataPackage& package);
    
    // ===== TSV 生成 =====
    
    /// 生成单通道 TSV 内容
    QByteArray generateTsvContent(const QString& channelName,
                                   const QList<Monitor::Sample>& samples);
    
    /// 生成多通道 TSV 内容
    QByteArray generateMultiChannelTsvContent(const ExportDataPackage& package);
    
    /// 生成 TSV 元数据注释头
    QString generateTsvMetadataHeader(const ExportDataPackage& package) const;

private:
    ExportConfig m_config;
    QWidget* m_parentWidget = nullptr;
};

#endif // MONITOR_EXPORT_HELPER_H
