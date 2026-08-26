// 文件：src/monitor/MonitorExportHelper.cpp
// 监控数据导出助手实现
//
// 功能完善版本：
// - 支持 CSV / JSON / TSV 三种格式导出
// - 完整的通道元信息和时间戳语义
// - 健壮的错误处理和日志输出

#include "MonitorExportHelper.h"
#include "MonitorChartView.h"
#include "MonitorTypes.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QSvgGenerator>
#include <QPainter>
#include <QSet>
#include <cmath>
#include <algorithm>

namespace {

QString encodeDelimitedField(QString value,
                             const QString& separator,
                             bool protectFormula)
{
    if (protectFormula && !value.isEmpty()) {
        const QChar first = value.at(0);
        if (first == QLatin1Char('=') || first == QLatin1Char('+')
            || first == QLatin1Char('-') || first == QLatin1Char('@')) {
            value.prepend(QLatin1Char('\''));
        }
    }

    const bool needsQuotes = (!separator.isEmpty() && value.contains(separator))
        || value.contains(QLatin1Char('"'))
        || value.contains(QLatin1Char('\r'))
        || value.contains(QLatin1Char('\n'));
    if (!needsQuotes) {
        return value;
    }

    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"") + value + QStringLiteral("\"");
}

}

// ============================================================================
// 构造 / 析构
// ============================================================================

MonitorExportHelper::MonitorExportHelper(QObject* parent)
    : QObject(parent)
    , m_parentWidget(nullptr)
{
    qDebug() << "[MonitorExportHelper] 导出助手已创建";
}

MonitorExportHelper::~MonitorExportHelper()
{
    qDebug() << "[MonitorExportHelper] 导出助手已销毁";
}

bool MonitorExportHelper::commitSaveFile(QSaveFile& file)
{
    return file.commit();
}

void MonitorExportHelper::setConfig(const ExportConfig& config)
{
    m_config = config;
}

void MonitorExportHelper::setParentWidget(QWidget* parent)
{
    m_parentWidget = parent;
}

// ============================================================================
// 图像导出（带对话框）
// ============================================================================

ExportResult MonitorExportHelper::exportChartAsPng(
    MonitorChartView* chartView,
    const QString& suggestedFileName)
{
    ExportResult result;
    result.exportFormat = "PNG";
    
    if (!chartView) {
        result.errorMessage = tr("图表视图无效");
        qWarning() << "[MonitorExportHelper] 导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QString fileName = suggestedFileName.isEmpty() 
        ? generateDefaultFileName(chartView->activeChannel(), "png")
        : suggestedFileName;
    
    QString filePath = showSaveImageDialog(fileName, tr("PNG 图像 (*.png)"));
    if (filePath.isEmpty()) {
        result.errorMessage = tr("用户取消导出");
        return result;
    }
    
    return exportChartAsPngToFile(chartView, filePath);
}

ExportResult MonitorExportHelper::exportChartAsJpg(
    MonitorChartView* chartView,
    const QString& suggestedFileName)
{
    ExportResult result;
    result.exportFormat = "JPG";
    
    if (!chartView) {
        result.errorMessage = tr("图表视图无效");
        qWarning() << "[MonitorExportHelper] 导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QString fileName = suggestedFileName.isEmpty() 
        ? generateDefaultFileName(chartView->activeChannel(), "jpg")
        : suggestedFileName;
    
    QString filePath = showSaveImageDialog(fileName, tr("JPEG 图像 (*.jpg *.jpeg)"));
    if (filePath.isEmpty()) {
        result.errorMessage = tr("用户取消导出");
        return result;
    }
    
    return exportChartAsJpgToFile(chartView, filePath);
}

ExportResult MonitorExportHelper::exportChartAsSvg(
    MonitorChartView* chartView,
    const QString& suggestedFileName)
{
    ExportResult result;
    result.exportFormat = "SVG";
    
    if (!chartView) {
        result.errorMessage = tr("图表视图无效");
        qWarning() << "[MonitorExportHelper] 导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QString fileName = suggestedFileName.isEmpty() 
        ? generateDefaultFileName(chartView->activeChannel(), "svg")
        : suggestedFileName;
    
    QString filePath = showSaveImageDialog(fileName, tr("SVG 矢量图 (*.svg)"));
    if (filePath.isEmpty()) {
        result.errorMessage = tr("用户取消导出");
        return result;
    }
    
    return exportChartAsSvgToFile(chartView, filePath);
}

// ============================================================================
// 图像导出（指定路径）
// ============================================================================

ExportResult MonitorExportHelper::exportChartAsPngToFile(
    MonitorChartView* chartView,
    const QString& filePath)
{
    ExportResult result;
    result.filePath = ensureExtension(filePath, "png");
    result.exportFormat = "PNG";
    
    if (!chartView) {
        result.errorMessage = tr("图表视图无效");
        qWarning() << "[MonitorExportHelper] PNG导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QString errorMsg;
    if (!validateFilePath(result.filePath, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] PNG导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    // 获取图表截图
    QPixmap pixmap = chartView->grab();
    if (pixmap.isNull()) {
        result.errorMessage = tr("无法获取图表截图");
        qWarning() << "[MonitorExportHelper] PNG导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    // 保存图像
    if (!pixmap.save(result.filePath, "PNG", m_config.imageQuality)) {
        result.errorMessage = tr("保存 PNG 文件失败: %1").arg(result.filePath);
        qWarning() << "[MonitorExportHelper] PNG导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    // 获取文件大小
    QFileInfo fileInfo(result.filePath);
    result.fileSizeBytes = fileInfo.size();
    result.success = true;
    result.exportedCount = 1;
    
    qDebug() << "[MonitorExportHelper] PNG导出成功:" << result.filePath 
             << "大小:" << result.fileSizeBytes << "字节";
    emit exportFinished(result);
    return result;
}

ExportResult MonitorExportHelper::exportChartAsJpgToFile(
    MonitorChartView* chartView,
    const QString& filePath)
{
    ExportResult result;
    result.filePath = ensureExtension(filePath, "jpg");
    result.exportFormat = "JPG";
    
    if (!chartView) {
        result.errorMessage = tr("图表视图无效");
        qWarning() << "[MonitorExportHelper] JPG导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QString errorMsg;
    if (!validateFilePath(result.filePath, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] JPG导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QPixmap pixmap = chartView->grab();
    if (pixmap.isNull()) {
        result.errorMessage = tr("无法获取图表截图");
        qWarning() << "[MonitorExportHelper] JPG导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    if (!pixmap.save(result.filePath, "JPG", m_config.imageQuality)) {
        result.errorMessage = tr("保存 JPG 文件失败: %1").arg(result.filePath);
        qWarning() << "[MonitorExportHelper] JPG导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QFileInfo fileInfo(result.filePath);
    result.fileSizeBytes = fileInfo.size();
    result.success = true;
    result.exportedCount = 1;
    
    qDebug() << "[MonitorExportHelper] JPG导出成功:" << result.filePath;
    emit exportFinished(result);
    return result;
}

ExportResult MonitorExportHelper::exportChartAsSvgToFile(
    MonitorChartView* chartView,
    const QString& filePath)
{
    ExportResult result;
    result.filePath = ensureExtension(filePath, "svg");
    result.exportFormat = "SVG";
    
    if (!chartView) {
        result.errorMessage = tr("图表视图无效");
        qWarning() << "[MonitorExportHelper] SVG导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QString errorMsg;
    if (!validateFilePath(result.filePath, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] SVG导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    // 使用 QSvgGenerator 生成 SVG
    QSvgGenerator generator;
    generator.setFileName(result.filePath);
    generator.setSize(chartView->size());
    generator.setViewBox(chartView->rect());
    generator.setTitle(tr("监控数据图表"));
    generator.setDescription(tr("由 ServoValvePlatform 导出"));
    
    QPainter painter(&generator);
    chartView->render(&painter);
    painter.end();
    
    QFileInfo fileInfo(result.filePath);
    if (!fileInfo.exists() || fileInfo.size() == 0) {
        result.errorMessage = tr("生成 SVG 文件失败");
        qWarning() << "[MonitorExportHelper] SVG导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    result.fileSizeBytes = fileInfo.size();
    result.success = true;
    result.exportedCount = 1;
    
    qDebug() << "[MonitorExportHelper] SVG导出成功:" << result.filePath;
    emit exportFinished(result);
    return result;
}

ExportResult MonitorExportHelper::exportPixmap(const QPixmap& pixmap,
                                                const QString& filePath,
                                                const QString& format)
{
    ExportResult result;
    result.filePath = filePath;
    result.exportFormat = format.toUpper();
    
    if (pixmap.isNull()) {
        result.errorMessage = tr("图像为空");
        qWarning() << "[MonitorExportHelper] Pixmap导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QString errorMsg;
    if (!validateFilePath(filePath, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] Pixmap导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    if (!pixmap.save(filePath, format.toUtf8().constData(), m_config.imageQuality)) {
        result.errorMessage = tr("保存图像失败: %1").arg(filePath);
        qWarning() << "[MonitorExportHelper] Pixmap导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QFileInfo fileInfo(filePath);
    result.fileSizeBytes = fileInfo.size();
    result.success = true;
    result.exportedCount = 1;
    
    qDebug() << "[MonitorExportHelper] Pixmap导出成功:" << filePath;
    emit exportFinished(result);
    return result;
}

// ============================================================================
// 数据导出（带对话框）- 单通道
// ============================================================================

ExportResult MonitorExportHelper::exportDataAsCsv(
    const QString& channelName,
    const QList<Monitor::Sample>& samples,
    const QString& suggestedFileName)
{
    ExportResult result;
    result.exportFormat = "CSV";
    
    QString fileName = suggestedFileName.isEmpty()
        ? generateDefaultFileName(channelName, "csv")
        : suggestedFileName;
    
    QString filePath = showSaveDataDialog(fileName, tr("CSV 文件 (*.csv)"));
    if (filePath.isEmpty()) {
        result.errorMessage = tr("用户取消导出");
        return result;
    }
    
    return exportDataAsCsvToFile(channelName, samples, filePath);
}

ExportResult MonitorExportHelper::exportDataAsJson(
    const QString& channelName,
    const QList<Monitor::Sample>& samples,
    const QString& suggestedFileName)
{
    ExportResult result;
    result.exportFormat = "JSON";
    
    QString fileName = suggestedFileName.isEmpty()
        ? generateDefaultFileName(channelName, "json")
        : suggestedFileName;
    
    QString filePath = showSaveDataDialog(fileName, tr("JSON 文件 (*.json)"));
    if (filePath.isEmpty()) {
        result.errorMessage = tr("用户取消导出");
        return result;
    }
    
    return exportDataAsJsonToFile(channelName, samples, filePath);
}

ExportResult MonitorExportHelper::exportDataAsTsv(
    const QString& channelName,
    const QList<Monitor::Sample>& samples,
    const QString& suggestedFileName)
{
    ExportResult result;
    result.exportFormat = "TSV";
    
    QString fileName = suggestedFileName.isEmpty()
        ? generateDefaultFileName(channelName, "tsv")
        : suggestedFileName;
    
    QString filePath = showSaveDataDialog(fileName, tr("TSV 文件 (*.tsv)"));
    if (filePath.isEmpty()) {
        result.errorMessage = tr("用户取消导出");
        return result;
    }
    
    return exportDataAsTsvToFile(channelName, samples, filePath);
}

// ============================================================================
// 数据导出（指定路径）- 单通道
// ============================================================================

ExportResult MonitorExportHelper::exportDataAsCsvToFile(
    const QString& channelName,
    const QList<Monitor::Sample>& samples,
    const QString& filePath)
{
    ExportResult result;
    result.filePath = ensureExtension(filePath, "csv");
    result.exportFormat = "CSV";
    
    // 边界检查
    if (samples.isEmpty()) {
        result.errorMessage = tr("没有可导出的数据");
        qWarning() << "[MonitorExportHelper] CSV导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QString errorMsg;
    if (!validateFilePath(result.filePath, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] CSV导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    // 生成 CSV 内容
    QByteArray content = generateCsvContent(channelName, samples);
    
    // 写入文件
    qint64 fileSize = 0;
    if (!writeFileAndGetSize(result.filePath, content, fileSize, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] CSV导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    result.success = true;
    result.exportedCount = samples.size();
    result.fileSizeBytes = fileSize;
    
    qDebug() << "[MonitorExportHelper] CSV导出成功:" << result.filePath 
             << "样本数:" << result.exportedCount;
    emit exportFinished(result);
    return result;
}

ExportResult MonitorExportHelper::exportDataAsJsonToFile(
    const QString& channelName,
    const QList<Monitor::Sample>& samples,
    const QString& filePath)
{
    ExportResult result;
    result.filePath = ensureExtension(filePath, "json");
    result.exportFormat = "JSON";
    
    if (samples.isEmpty()) {
        result.errorMessage = tr("没有可导出的数据");
        qWarning() << "[MonitorExportHelper] JSON导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QString errorMsg;
    if (!validateFilePath(result.filePath, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] JSON导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QByteArray content = generateJsonContent(channelName, samples);
    
    qint64 fileSize = 0;
    if (!writeFileAndGetSize(result.filePath, content, fileSize, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] JSON导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    result.success = true;
    result.exportedCount = samples.size();
    result.fileSizeBytes = fileSize;
    
    qDebug() << "[MonitorExportHelper] JSON导出成功:" << result.filePath;
    emit exportFinished(result);
    return result;
}

ExportResult MonitorExportHelper::exportDataAsTsvToFile(
    const QString& channelName,
    const QList<Monitor::Sample>& samples,
    const QString& filePath)
{
    ExportResult result;
    result.filePath = ensureExtension(filePath, "tsv");
    result.exportFormat = "TSV";
    
    if (samples.isEmpty()) {
        result.errorMessage = tr("没有可导出的数据");
        qWarning() << "[MonitorExportHelper] TSV导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QString errorMsg;
    if (!validateFilePath(result.filePath, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] TSV导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QByteArray content = generateTsvContent(channelName, samples);
    
    qint64 fileSize = 0;
    if (!writeFileAndGetSize(result.filePath, content, fileSize, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] TSV导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    result.success = true;
    result.exportedCount = samples.size();
    result.fileSizeBytes = fileSize;
    
    qDebug() << "[MonitorExportHelper] TSV导出成功:" << result.filePath;
    emit exportFinished(result);
    return result;
}

// ============================================================================
// 数据导出（完整数据包）- 多通道支持
// ============================================================================

ExportResult MonitorExportHelper::exportPackageAsCsv(
    const ExportDataPackage& package,
    const QString& filePath)
{
    ExportResult result;
    result.filePath = ensureExtension(filePath, "csv");
    result.exportFormat = "CSV";
    
    if (!package.isValid()) {
        result.errorMessage = tr("导出数据包无效：没有通道或样本数据");
        qWarning() << "[MonitorExportHelper] 数据包CSV导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QString errorMsg;
    if (!validateFilePath(result.filePath, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] 数据包CSV导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QByteArray content = generateMultiChannelCsvContent(package);
    
    qint64 fileSize = 0;
    if (!writeFileAndGetSize(result.filePath, content, fileSize, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] 数据包CSV导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    result.success = true;
    result.exportedCount = package.totalSampleCount();
    result.fileSizeBytes = fileSize;
    
    qDebug() << "[MonitorExportHelper] 多通道CSV导出成功:" << result.filePath
             << "通道数:" << package.channelInfos.size()
             << "样本数:" << result.exportedCount;
    emit exportFinished(result);
    return result;
}

ExportResult MonitorExportHelper::exportPackageAsJson(
    const ExportDataPackage& package,
    const QString& filePath)
{
    ExportResult result;
    result.filePath = ensureExtension(filePath, "json");
    result.exportFormat = "JSON";
    
    if (!package.isValid()) {
        result.errorMessage = tr("导出数据包无效：没有通道或样本数据");
        qWarning() << "[MonitorExportHelper] 数据包JSON导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QString errorMsg;
    if (!validateFilePath(result.filePath, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] 数据包JSON导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QByteArray content = generateMultiChannelJsonContent(package);
    
    qint64 fileSize = 0;
    if (!writeFileAndGetSize(result.filePath, content, fileSize, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] 数据包JSON导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    result.success = true;
    result.exportedCount = package.totalSampleCount();
    result.fileSizeBytes = fileSize;
    
    qDebug() << "[MonitorExportHelper] 多通道JSON导出成功:" << result.filePath;
    emit exportFinished(result);
    return result;
}

ExportResult MonitorExportHelper::exportPackageAsTsv(
    const ExportDataPackage& package,
    const QString& filePath)
{
    ExportResult result;
    result.filePath = ensureExtension(filePath, "tsv");
    result.exportFormat = "TSV";
    
    if (!package.isValid()) {
        result.errorMessage = tr("导出数据包无效：没有通道或样本数据");
        qWarning() << "[MonitorExportHelper] 数据包TSV导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QString errorMsg;
    if (!validateFilePath(result.filePath, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] 数据包TSV导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QByteArray content = generateMultiChannelTsvContent(package);
    
    qint64 fileSize = 0;
    if (!writeFileAndGetSize(result.filePath, content, fileSize, errorMsg)) {
        result.errorMessage = errorMsg;
        qWarning() << "[MonitorExportHelper] 数据包TSV导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    result.success = true;
    result.exportedCount = package.totalSampleCount();
    result.fileSizeBytes = fileSize;
    
    qDebug() << "[MonitorExportHelper] 多通道TSV导出成功:" << result.filePath;
    emit exportFinished(result);
    return result;
}

// ============================================================================
// 通用导出接口
// ============================================================================

ExportResult MonitorExportHelper::exportPackageAuto(
    const ExportDataPackage& package,
    const QString& filePath)
{
    QString format = formatFromPath(filePath).toLower();
    
    if (format == "json") {
        return exportPackageAsJson(package, filePath);
    } else if (format == "tsv") {
        return exportPackageAsTsv(package, filePath);
    } else {
        // 默认使用 CSV
        return exportPackageAsCsv(package, filePath);
    }
}

ExportResult MonitorExportHelper::exportPackageWithDialog(
    const ExportDataPackage& package,
    const QString& suggestedFileName)
{
    ExportResult result;
    
    if (!package.isValid()) {
        result.errorMessage = tr("导出数据包无效：没有通道或样本数据");
        qWarning() << "[MonitorExportHelper] 导出失败:" << result.errorMessage;
        emit exportError(result.errorMessage);
        return result;
    }
    
    QString fileName = suggestedFileName.isEmpty()
        ? generateMultiChannelFileName("csv")
        : suggestedFileName;
    
    QString filePath = showSaveDataDialog(fileName, dataFormatFilter());
    if (filePath.isEmpty()) {
        result.errorMessage = tr("用户取消导出");
        return result;
    }
    
    return exportPackageAuto(package, filePath);
}

// ============================================================================
// 工具方法
// ============================================================================

QString MonitorExportHelper::generateDefaultFileName(
    const QString& channelName,
    const QString& extension) const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString safeName = channelName;
    safeName.replace(QRegularExpression("[^a-zA-Z0-9_\\-\\u4e00-\\u9fa5]"), "_");
    
    if (safeName.isEmpty()) {
        safeName = "monitor_data";
    }
    
    return QString("%1_%2.%3").arg(safeName, timestamp, extension);
}

QString MonitorExportHelper::generateMultiChannelFileName(
    const QString& extension) const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QString("monitor_export_%1.%2").arg(timestamp, extension);
}

QString MonitorExportHelper::dataFormatFilter()
{
    return tr("CSV 文件 (*.csv);;"
              "JSON 文件 (*.json);;"
              "TSV 文件 (*.tsv);;"
              "所有文件 (*)");
}

QString MonitorExportHelper::imageFormatFilter()
{
    return tr("PNG 图像 (*.png);;"
              "JPEG 图像 (*.jpg *.jpeg);;"
              "SVG 矢量图 (*.svg);;"
              "所有文件 (*)");
}

QString MonitorExportHelper::formatFromPath(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.suffix().toLower();
}

// ============================================================================
// 内部实现方法
// ============================================================================

QString MonitorExportHelper::showSaveImageDialog(
    const QString& suggestedFileName,
    const QString& filter)
{
    QString defaultPath = m_config.defaultPath.isEmpty()
        ? QDir::homePath()
        : m_config.defaultPath;
    
    QString fullPath = QDir(defaultPath).absoluteFilePath(suggestedFileName);
    
    return QFileDialog::getSaveFileName(
        m_parentWidget,
        tr("导出图表图像"),
        fullPath,
        filter
    );
}

QString MonitorExportHelper::showSaveDataDialog(
    const QString& suggestedFileName,
    const QString& filter)
{
    QString defaultPath = m_config.defaultPath.isEmpty()
        ? QDir::homePath()
        : m_config.defaultPath;
    
    QString fullPath = QDir(defaultPath).absoluteFilePath(suggestedFileName);
    
    return QFileDialog::getSaveFileName(
        m_parentWidget,
        tr("导出监控数据"),
        fullPath,
        filter
    );
}

QString MonitorExportHelper::ensureExtension(
    const QString& filePath,
    const QString& extension) const
{
    QFileInfo fileInfo(filePath);
    if (fileInfo.suffix().isEmpty()) {
        return filePath + "." + extension;
    }
    return filePath;
}

bool MonitorExportHelper::validateFilePath(
    const QString& filePath,
    QString& errorMsg) const
{
    if (filePath.isEmpty()) {
        errorMsg = tr("文件路径为空");
        return false;
    }
    
    QFileInfo fileInfo(filePath);
    QDir parentDir = fileInfo.dir();
    
    if (!parentDir.exists()) {
        errorMsg = tr("目录不存在: %1").arg(parentDir.absolutePath());
        return false;
    }
    
    // 检查是否可写（通过尝试创建临时文件）
    QString testPath = parentDir.absoluteFilePath(".write_test_" + 
        QString::number(QDateTime::currentMSecsSinceEpoch()));
    QFile testFile(testPath);
    if (!testFile.open(QIODevice::WriteOnly)) {
        errorMsg = tr("目录不可写: %1").arg(parentDir.absolutePath());
        return false;
    }
    testFile.close();
    testFile.remove();
    
    // 检查文件是否已存在且不允许覆盖
    if (fileInfo.exists() && !m_config.overwriteExisting) {
        // 在 GUI 模式下，文件对话框通常会处理覆盖确认
        // 这里仅作为额外保护
    }
    
    return true;
}

bool MonitorExportHelper::writeFileAndGetSize(
    const QString& filePath,
    const QByteArray& data,
    qint64& fileSize,
    QString& errorMsg)
{
    QSaveFile file(filePath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorMsg = tr("无法打开文件进行写入: %1 (%2)")
            .arg(filePath, file.errorString());
        return false;
    }

    qint64 totalWritten = 0;
    while (totalWritten < data.size()) {
        const qint64 bytesWritten = file.write(data.constData() + totalWritten,
                                               data.size() - totalWritten);
        if (bytesWritten <= 0) {
            errorMsg = tr("写入文件不完整: 期望 %1 字节，实际写入 %2 字节 (%3)")
                .arg(data.size()).arg(totalWritten).arg(file.errorString());
            return false;
        }
        totalWritten += bytesWritten;
    }

    if (!commitSaveFile(file)) {
        errorMsg = tr("提交文件失败: %1 (%2)").arg(filePath, file.errorString());
        return false;
    }

    QFileInfo fileInfo(filePath);
    fileSize = fileInfo.size();
    
    return true;
}

// ============================================================================
// CSV 生成
// ============================================================================

QByteArray MonitorExportHelper::generateCsvContent(
    const QString& channelName,
    const QList<Monitor::Sample>& samples)
{
    QString content;
    QTextStream stream(&content);
    
    // 设置数值精度
    stream.setRealNumberPrecision(m_config.csvPrecision);
    stream.setRealNumberNotation(QTextStream::SmartNotation);
    
    const QString sep = m_config.csvSeparator;
    const auto field = [&sep](const QString& value, bool protectFormula) {
        return encodeDelimitedField(value, sep, protectFormula);
    };
    
    // 写入元数据注释（如果启用）
    if (m_config.includeMetadataComments) {
        stream << "# ServoValvePlatform Monitor Data Export\n";
        stream << "# Export Time: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        stream << "# Channel: " << field(channelName, true) << "\n";
        stream << "# Sample Count: " << samples.size() << "\n";
        if (!samples.isEmpty()) {
            stream << "# Unit: " << field(samples.first().unit, true) << "\n";
        }
        stream << "#\n";
    }
    
    // 写入表头
    if (m_config.csvIncludeHeader) {
        stream << field(QStringLiteral("timestamp"), false) << sep
               << field(QStringLiteral("timestamp_ms"), false) << sep
               << field(QStringLiteral("channel"), false) << sep
               << field(QStringLiteral("value"), false) << sep
               << field(QStringLiteral("unit"), false) << sep
               << field(QStringLiteral("quality"), false) << sep
               << field(QStringLiteral("value_valid"), false) << '\n';
    }
    
    // 写入数据行
    for (const Monitor::Sample& sample : samples) {
        stream << field(sample.timestamp.toString(m_config.timestampFormat), false) << sep
               << field(QString::number(sample.timestamp.toMSecsSinceEpoch()), false) << sep
               << field(sample.channelName, true) << sep
               << field(sample.valueValid
                               ? QString::number(sample.value, 'g', m_config.csvPrecision)
                               : QString(), false) << sep
               << field(sample.unit, true) << sep
               << field(runtimePointQualityToString(sample.quality), false) << sep
               << field(sample.valueValid ? QStringLiteral("1") : QStringLiteral("0"), false)
               << '\n';
    }
    
    return content.toUtf8();
}

QByteArray MonitorExportHelper::generateMultiChannelCsvContent(
    const ExportDataPackage& package)
{
    QString content;
    QTextStream stream(&content);
    
    stream.setRealNumberPrecision(m_config.csvPrecision);
    stream.setRealNumberNotation(QTextStream::SmartNotation);
    
    const QString sep = m_config.csvSeparator;
    const auto field = [&sep](const QString& value, bool protectFormula) {
        return encodeDelimitedField(value, sep, protectFormula);
    };
    
    // 写入元数据注释
    if (m_config.includeMetadataComments) {
        stream << "# ServoValvePlatform Monitor Data Export\n";
        stream << "# Export Time: " << package.metadata.exportTime.toString(Qt::ISODate) << "\n";
        stream << "# Project: " << field(package.metadata.projectName, true) << "\n";
        for (auto it = package.metadata.customFields.constBegin();
             it != package.metadata.customFields.constEnd(); ++it) {
            stream << "# Custom: " << field(it.key(), true) << "="
                   << field(it.value().toString(), true) << "\n";
        }
        stream << "# Time Window: " << package.metadata.timeWindowMs << " ms\n";
        stream << "# Total Channels: " << package.channelInfos.size() << "\n";
        stream << "# Total Samples: " << package.totalSampleCount() << "\n";
        stream << "# Software Version: " << field(package.metadata.softwareVersion, true) << "\n";
        stream << "#\n";
        stream << "# Channel Definitions:\n";
        int idx = 1;
        for (const ExportChannelInfo& info : package.channelInfos) {
            stream << "# [" << idx++ << "] " << field(info.channelId, true)
                   << " | " << field(info.displayName, true)
                   << " | " << field(info.unit, true)
                   << " | Period: " << info.samplePeriodMs << "ms\n";
        }
        stream << "#\n";
    }
    
    if (m_config.alignMultiChannelByTime) {
        // 按时间对齐模式：收集所有时间戳，每行一个时间点
        QSet<qint64> allTimestamps;
        for (auto it = package.channelSamples.constBegin(); 
             it != package.channelSamples.constEnd(); ++it) {
            for (const Monitor::Sample& sample : it.value()) {
                allTimestamps.insert(sample.timestampMs());
            }
        }
        
        QList<qint64> sortedTimestamps = allTimestamps.values();
        std::sort(sortedTimestamps.begin(), sortedTimestamps.end());
        
        // 构建每个通道的时间戳到完整样本的映射
        QMap<QString, QMap<qint64, Monitor::Sample>> channelSampleMaps;
        for (auto it = package.channelSamples.constBegin(); 
             it != package.channelSamples.constEnd(); ++it) {
            QMap<qint64, Monitor::Sample> sampleMap;
            for (const Monitor::Sample& sample : it.value()) {
                sampleMap.insert(sample.timestampMs(), sample);
            }
            channelSampleMaps.insert(it.key(), sampleMap);
        }
        
        // 写入表头
        if (m_config.csvIncludeHeader) {
            stream << field(QStringLiteral("timestamp"), false) << sep
                   << field(QStringLiteral("timestamp_ms"), false);
            for (const ExportChannelInfo& info : package.channelInfos) {
                stream << sep << field(info.channelId, true)
                       << sep << field(info.channelId + QStringLiteral("_quality"), true)
                       << sep << field(info.channelId + QStringLiteral("_value_valid"), true);
            }
            stream << '\n';
        }
        
        // 写入数据行
        for (qint64 ts : sortedTimestamps) {
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(ts);
            stream << field(dt.toString(m_config.timestampFormat), false) << sep
                   << field(QString::number(ts), false);
            
            for (const ExportChannelInfo& info : package.channelInfos) {
                const QMap<qint64, Monitor::Sample>& sampleMap = channelSampleMaps.value(info.channelId);
                if (sampleMap.contains(ts)) {
                    const Monitor::Sample& sample = sampleMap.value(ts);
                    stream << sep
                           << field(sample.valueValid
                                        ? QString::number(sample.value, 'g', m_config.csvPrecision)
                                        : QString(), false)
                           << sep << field(runtimePointQualityToString(sample.quality), false)
                           << sep << field(sample.valueValid ? QStringLiteral("1")
                                                               : QStringLiteral("0"), false);
                } else {
                    stream << sep << field(QString(), false)
                           << sep << field(QString(), false)
                           << sep << field(QString(), false);
                }
            }
            stream << '\n';
        }
    } else {
        // 非对齐模式：每个样本一行，包含通道标识
        if (m_config.csvIncludeHeader) {
            stream << field(QStringLiteral("timestamp"), false) << sep
                   << field(QStringLiteral("timestamp_ms"), false) << sep
                   << field(QStringLiteral("channel_id"), false) << sep
                   << field(QStringLiteral("channel_name"), false) << sep
                   << field(QStringLiteral("value"), false) << sep
                   << field(QStringLiteral("unit"), false) << sep
                   << field(QStringLiteral("quality"), false) << sep
                   << field(QStringLiteral("value_valid"), false) << '\n';
        }
        
        for (const ExportChannelInfo& info : package.channelInfos) {
            const QList<Monitor::Sample>& samples = package.channelSamples.value(info.channelId);
            for (const Monitor::Sample& sample : samples) {
                stream << field(sample.timestamp.toString(m_config.timestampFormat), false) << sep
                       << field(QString::number(sample.timestampMs()), false) << sep
                       << field(info.channelId, true) << sep
                       << field(info.displayName, true) << sep
                       << field(sample.valueValid
                                       ? QString::number(sample.value, 'g', m_config.csvPrecision)
                                       : QString(), false) << sep
                       << field(info.unit, true) << sep
                       << field(runtimePointQualityToString(sample.quality), false) << sep
                       << field(sample.valueValid ? QStringLiteral("1") : QStringLiteral("0"), false)
                       << '\n';
            }
        }
    }
    
    return content.toUtf8();
}

// ============================================================================
// JSON 生成
// ============================================================================

QByteArray MonitorExportHelper::generateJsonContent(
    const QString& channelName,
    const QList<Monitor::Sample>& samples)
{
    QJsonObject root;
    
    // 元数据
    QJsonObject metadata;
    metadata["exportTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    metadata["exportTimeMs"] = QDateTime::currentDateTime().toMSecsSinceEpoch();
    metadata["channelName"] = channelName;
    metadata["sampleCount"] = samples.size();
    metadata["softwareVersion"] = "1.0.0";
    
    if (!samples.isEmpty()) {
        metadata["unit"] = samples.first().unit;
        metadata["startTime"] = samples.first().timestamp.toString(Qt::ISODate);
        metadata["endTime"] = samples.last().timestamp.toString(Qt::ISODate);
    }
    
    root["metadata"] = metadata;
    
    // 样本数据
    QJsonArray samplesArray;
    for (const Monitor::Sample& sample : samples) {
        QJsonObject sampleObj;
        sampleObj["timestamp"] = sample.timestamp.toString(Qt::ISODate);
        sampleObj["timestampMs"] = sample.timestampMs();
        sampleObj["value"] = sample.valueValid ? QJsonValue(sample.value) : QJsonValue(QJsonValue::Null);
        sampleObj["quality"] = runtimePointQualityToString(sample.quality);
        sampleObj["valueValid"] = sample.valueValid;
        samplesArray.append(sampleObj);
    }
    root["samples"] = samplesArray;
    
    QJsonDocument doc(root);
    return doc.toJson(QJsonDocument::Indented);
}

QByteArray MonitorExportHelper::generateMultiChannelJsonContent(
    const ExportDataPackage& package)
{
    QJsonObject root;
    
    // 元数据
    QJsonObject metadata;
    metadata["projectName"] = package.metadata.projectName;
    metadata["exportTime"] = package.metadata.exportTime.toString(Qt::ISODate);
    metadata["exportTimeMs"] = package.metadata.exportTime.toMSecsSinceEpoch();
    metadata["timeWindowMs"] = package.metadata.timeWindowMs;
    metadata["exportFormat"] = "JSON";
    metadata["totalChannels"] = package.channelInfos.size();
    metadata["totalSamples"] = package.totalSampleCount();
    metadata["softwareVersion"] = package.metadata.softwareVersion;
    
    // 添加自定义字段
    for (auto it = package.metadata.customFields.constBegin();
         it != package.metadata.customFields.constEnd(); ++it) {
        metadata[it.key()] = QJsonValue::fromVariant(it.value());
    }
    
    root["metadata"] = metadata;
    
    // 通道数据
    QJsonArray channelsArray;
    for (const ExportChannelInfo& info : package.channelInfos) {
        QJsonObject channelObj;
        channelObj["channelId"] = info.channelId;
        channelObj["displayName"] = info.displayName;
        channelObj["unit"] = info.unit;
        channelObj["samplePeriodMs"] = info.samplePeriodMs;
        
        const QList<Monitor::Sample>& samples = package.channelSamples.value(info.channelId);
        channelObj["sampleCount"] = samples.size();
        
        QJsonArray samplesArray;
        for (const Monitor::Sample& sample : samples) {
            QJsonObject sampleObj;
            sampleObj["timestamp"] = sample.timestamp.toString(Qt::ISODate);
            sampleObj["timestampMs"] = sample.timestampMs();
            sampleObj["value"] = sample.valueValid ? QJsonValue(sample.value)
                                                   : QJsonValue(QJsonValue::Null);
            sampleObj["quality"] = runtimePointQualityToString(sample.quality);
            sampleObj["valueValid"] = sample.valueValid;
            samplesArray.append(sampleObj);
        }
        channelObj["samples"] = samplesArray;
        
        channelsArray.append(channelObj);
    }
    root["channels"] = channelsArray;
    
    QJsonDocument doc(root);
    return doc.toJson(QJsonDocument::Indented);
}

// ============================================================================
// TSV 生成
// ============================================================================

QByteArray MonitorExportHelper::generateTsvContent(
    const QString& channelName,
    const QList<Monitor::Sample>& samples)
{
    QString content;
    QTextStream stream(&content);
    
    stream.setRealNumberPrecision(m_config.csvPrecision);
    stream.setRealNumberNotation(QTextStream::SmartNotation);
    
    const QString sep = QStringLiteral("\t");
    const auto field = [&sep](const QString& value, bool protectFormula) {
        return encodeDelimitedField(value, sep, protectFormula);
    };

    // 写入元数据注释
    if (m_config.includeMetadataComments) {
        stream << "# ServoValvePlatform Monitor Data Export\n";
        stream << "# Export Time: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        stream << "# Channel: " << field(channelName, true) << "\n";
        stream << "# Sample Count: " << samples.size() << "\n";
        if (!samples.isEmpty()) {
            stream << "# Unit: " << field(samples.first().unit, true) << "\n";
        }
        stream << "#\n";
    }
    
    // 写入表头（TSV 使用制表符分隔）
    if (m_config.csvIncludeHeader) {
        stream << field(QStringLiteral("timestamp"), false) << sep
               << field(QStringLiteral("timestamp_ms"), false) << sep
               << field(QStringLiteral("value"), false) << sep
               << field(QStringLiteral("unit"), false) << sep
               << field(QStringLiteral("quality"), false) << sep
               << field(QStringLiteral("value_valid"), false) << '\n';
    }
    
    // 写入数据行
    for (const Monitor::Sample& sample : samples) {
        stream << field(sample.timestamp.toString(m_config.timestampFormat), false) << sep
               << field(QString::number(sample.timestampMs()), false) << sep
               << field(sample.valueValid
                               ? QString::number(sample.value, 'g', m_config.csvPrecision)
                               : QString(), false) << sep
               << field(sample.unit, true) << sep
               << field(runtimePointQualityToString(sample.quality), false) << sep
               << field(sample.valueValid ? QStringLiteral("1") : QStringLiteral("0"), false)
               << '\n';
    }
    
    return content.toUtf8();
}

QByteArray MonitorExportHelper::generateMultiChannelTsvContent(
    const ExportDataPackage& package)
{
    QString content;
    QTextStream stream(&content);
    
    stream.setRealNumberPrecision(m_config.csvPrecision);
    stream.setRealNumberNotation(QTextStream::SmartNotation);
    const QString sep = QStringLiteral("\t");
    const auto field = [&sep](const QString& value, bool protectFormula) {
        return encodeDelimitedField(value, sep, protectFormula);
    };

    // 写入元数据头
    stream << generateTsvMetadataHeader(package);
    
    // 收集所有时间戳并排序
    QSet<qint64> allTimestamps;
    for (auto it = package.channelSamples.constBegin(); 
         it != package.channelSamples.constEnd(); ++it) {
        for (const Monitor::Sample& sample : it.value()) {
            allTimestamps.insert(sample.timestampMs());
        }
    }
    
    QList<qint64> sortedTimestamps = allTimestamps.values();
    std::sort(sortedTimestamps.begin(), sortedTimestamps.end());
    
    // 构建每个通道的时间戳到完整样本的映射
    QMap<QString, QMap<qint64, Monitor::Sample>> channelSampleMaps;
    for (auto it = package.channelSamples.constBegin(); 
         it != package.channelSamples.constEnd(); ++it) {
        QMap<qint64, Monitor::Sample> sampleMap;
        for (const Monitor::Sample& sample : it.value()) {
            sampleMap.insert(sample.timestampMs(), sample);
        }
        channelSampleMaps.insert(it.key(), sampleMap);
    }
    
    // 写入表头
    if (m_config.csvIncludeHeader) {
        stream << field(QStringLiteral("timestamp"), false) << sep
               << field(QStringLiteral("timestamp_ms"), false);
        for (const ExportChannelInfo& info : package.channelInfos) {
            stream << sep << field(info.channelId, true)
                   << sep << field(info.channelId + QStringLiteral("_quality"), true)
                   << sep << field(info.channelId + QStringLiteral("_value_valid"), true);
        }
        stream << '\n';
    }
    
    // 写入数据行
    for (qint64 ts : sortedTimestamps) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(ts);
        stream << field(dt.toString(m_config.timestampFormat), false) << sep
               << field(QString::number(ts), false);
        
        for (const ExportChannelInfo& info : package.channelInfos) {
            const QMap<qint64, Monitor::Sample>& sampleMap = channelSampleMaps.value(info.channelId);
            if (sampleMap.contains(ts)) {
                const Monitor::Sample& sample = sampleMap.value(ts);
                stream << sep
                       << field(sample.valueValid
                                    ? QString::number(sample.value, 'g', m_config.csvPrecision)
                                    : QString(), false)
                       << sep << field(runtimePointQualityToString(sample.quality), false)
                       << sep << field(sample.valueValid ? QStringLiteral("1")
                                                           : QStringLiteral("0"), false);
            } else {
                stream << sep << field(QString(), false)
                       << sep << field(QString(), false)
                       << sep << field(QString(), false);
            }
        }
        stream << '\n';
    }
    
    return content.toUtf8();
}

QString MonitorExportHelper::generateTsvMetadataHeader(
    const ExportDataPackage& package) const
{
    QString header;
    QTextStream stream(&header);
    const QString sep = QStringLiteral("\t");
    const auto field = [&sep](const QString& value, bool protectFormula) {
        return encodeDelimitedField(value, sep, protectFormula);
    };
    
    stream << "# ServoValvePlatform Monitor Data Export\n";
    stream << "# Export Time: " << package.metadata.exportTime.toString(Qt::ISODate) << "\n";
    stream << "# Project: " << field(package.metadata.projectName, true) << "\n";
    for (auto it = package.metadata.customFields.constBegin();
         it != package.metadata.customFields.constEnd(); ++it) {
        stream << "# Custom: " << field(it.key(), true) << "="
               << field(it.value().toString(), true) << "\n";
    }
    stream << "# Time Window: " << package.metadata.timeWindowMs << " ms\n";
    stream << "# Total Channels: " << package.channelInfos.size() << "\n";
    stream << "# Total Samples: " << package.totalSampleCount() << "\n";
    stream << "# Software Version: " << field(package.metadata.softwareVersion, true) << "\n";
    stream << "#\n";
    stream << "# Channel Definitions:\n";
    
    int idx = 1;
    for (const ExportChannelInfo& info : package.channelInfos) {
        stream << "# [" << idx++ << "] " << field(info.channelId, true)
               << " | " << field(info.displayName, true)
               << " | " << field(info.unit, true)
               << " | Period: " << info.samplePeriodMs << "ms\n";
    }
    stream << "#\n";
    
    return header;
}

// ============================================================================
// 分页/流式导出
// ============================================================================

ExportResult MonitorExportHelper::exportPackagePaged(
    const QList<ExportChannelInfo>& channelInfos,
    const ExportMetadata& metadata,
    const ExportPageProvider& provider,
    const QString& filePath,
    int pageSize)
{
    const QString format = formatFromPath(filePath).toLower();
    if (format == QStringLiteral("json")) {
        return exportPagedPackageToFile(channelInfos, metadata, provider,
                                        filePath, QStringLiteral("JSON"), pageSize);
    }
    if (format == QStringLiteral("tsv")) {
        return exportPagedPackageToFile(channelInfos, metadata, provider,
                                        filePath, QStringLiteral("TSV"), pageSize);
    }
    return exportPagedPackageToFile(channelInfos, metadata, provider,
                                    filePath, QStringLiteral("CSV"), pageSize);
}

ExportResult MonitorExportHelper::exportPackageStream(
    const QList<ExportChannelInfo>& channelInfos,
    const ExportMetadata& metadata,
    const ExportPageProvider& provider,
    const QString& filePath,
    int pageSize)
{
    return exportPackagePaged(channelInfos, metadata, provider, filePath, pageSize);
}

ExportResult MonitorExportHelper::exportDataAsCsvPaged(
    const QString& channelName,
    const ExportPageProvider& provider,
    const QString& filePath,
    int pageSize)
{
    ExportChannelInfo info(channelName, channelName, QString());
    ExportMetadata metadata;
    metadata.totalChannels = 1;
    metadata.exportFormat = QStringLiteral("CSV");
    return exportPagedPackageToFile({info}, metadata, provider, filePath,
                                    QStringLiteral("CSV"), pageSize);
}

ExportResult MonitorExportHelper::exportDataAsJsonPaged(
    const QString& channelName,
    const ExportPageProvider& provider,
    const QString& filePath,
    int pageSize)
{
    ExportChannelInfo info(channelName, channelName, QString());
    ExportMetadata metadata;
    metadata.totalChannels = 1;
    metadata.exportFormat = QStringLiteral("JSON");
    return exportPagedPackageToFile({info}, metadata, provider, filePath,
                                    QStringLiteral("JSON"), pageSize);
}

ExportResult MonitorExportHelper::exportDataAsTsvPaged(
    const QString& channelName,
    const ExportPageProvider& provider,
    const QString& filePath,
    int pageSize)
{
    ExportChannelInfo info(channelName, channelName, QString());
    ExportMetadata metadata;
    metadata.totalChannels = 1;
    metadata.exportFormat = QStringLiteral("TSV");
    return exportPagedPackageToFile({info}, metadata, provider, filePath,
                                    QStringLiteral("TSV"), pageSize);
}

ExportResult MonitorExportHelper::exportPagedPackageToFile(
    const QList<ExportChannelInfo>& channelInfos,
    const ExportMetadata& metadata,
    const ExportPageProvider& provider,
    const QString& filePath,
    const QString& format,
    int pageSize)
{
    ExportResult result;
    result.filePath = ensureExtension(filePath, format.toLower());
    result.exportFormat = format.toUpper();

    auto fail = [&](const QString& message) {
        result.errorMessage = message;
        emit exportError(result.errorMessage);
        return result;
    };

    if (channelInfos.isEmpty()) {
        return fail(tr("导出数据包无效：没有通道"));
    }
    if (!provider) {
        return fail(tr("分页数据提供器无效"));
    }
    if (pageSize <= 0) {
        return fail(tr("分页大小必须大于 0"));
    }

    QString pathError;
    if (!validateFilePath(result.filePath, pathError)) {
        return fail(pathError);
    }

    QSaveFile file(result.filePath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return fail(tr("无法打开文件进行写入: %1 (%2)")
                        .arg(result.filePath, file.errorString()));
    }

    auto writeChunk = [&](const QByteArray& bytes) -> bool {
        qint64 offset = 0;
        while (offset < bytes.size()) {
            const qint64 written = file.write(bytes.constData() + offset,
                                              bytes.size() - offset);
            if (written <= 0) {
                result.errorMessage = tr("写入文件失败: %1")
                    .arg(file.errorString());
                return false;
            }
            offset += written;
        }
        return true;
    };

    auto writeText = [&](const QString& text) -> bool {
        return writeChunk(text.toUtf8());
    };

    // 仅为一个字段/样本构造 JSON 片段，绝不建立完整 QJsonArray/树。
    auto jsonQuote = [](const QString& value) -> QByteArray {
        QJsonArray array;
        array.append(value);
        const QByteArray encoded = QJsonDocument(array).toJson(QJsonDocument::Compact).trimmed();
        return encoded.mid(1, encoded.size() - 2);
    };
    auto jsonVariant = [&](const QVariant& value) -> QByteArray {
        if (!value.isValid() || value.isNull()) {
            return QByteArrayLiteral("null");
        }
        switch (value.type()) {
        case QVariant::Bool:
            return value.toBool() ? QByteArrayLiteral("true") : QByteArrayLiteral("false");
        case QVariant::Int:
        case QVariant::UInt:
        case QVariant::LongLong:
        case QVariant::ULongLong:
            return QByteArray::number(value.toLongLong());
        case QVariant::Double: {
            const double number = value.toDouble();
            return std::isfinite(number)
                ? QByteArray::number(number, 'g', m_config.csvPrecision)
                : QByteArrayLiteral("null");
        }
        case QVariant::String:
            return jsonQuote(value.toString());
        default:
            return jsonQuote(value.toString());
        }
    };
    auto jsonSample = [&](const Monitor::Sample& sample) -> QByteArray {
        QByteArray out("{\"timestamp\":");
        out += jsonQuote(sample.timestamp.toString(Qt::ISODate));
        out += ",\"timestampMs\":";
        out += QByteArray::number(sample.timestampMs());
        out += ",\"value\":";
        if (sample.valueValid && std::isfinite(sample.value)) {
            out += QByteArray::number(sample.value, 'g', m_config.csvPrecision);
        } else {
            out += QByteArrayLiteral("null");
        }
        out += ",\"quality\":";
        out += jsonQuote(runtimePointQualityToString(sample.quality));
        out += ",\"valueValid\":";
        out += sample.valueValid ? QByteArrayLiteral("true") : QByteArrayLiteral("false");
        out += '}';
        return out;
    };

    struct ChannelState {
        QString channelId;
        QList<Monitor::Sample> samples;
        int index = 0;
        ExportCursor cursor;
        bool done = false;
        int emptyPages = 0;
        QString error;
    };

    QVector<ChannelState> states;
    states.reserve(channelInfos.size());
    for (const ExportChannelInfo& info : channelInfos) {
        ChannelState state;
        state.channelId = info.channelId;
        states.append(std::move(state));
    }

    auto ensureCurrent = [&](ChannelState& state) -> bool {
        while (state.index >= state.samples.size() && !state.done) {
            const ExportPage page = provider(state.channelId, state.cursor, pageSize);
            if (!page.success) {
                state.error = page.errorMessage.isEmpty()
                    ? tr("分页数据提供器失败: %1").arg(state.channelId)
                    : page.errorMessage;
                return false;
            }
            if (page.samples.size() > pageSize) {
                state.error = tr("分页数据提供器返回了超过页大小的记录: %1")
                    .arg(state.channelId);
                return false;
            }
            state.samples = page.samples;
            state.index = 0;
            state.cursor = page.nextCursor;
            state.done = !page.hasMore;
            if (state.samples.isEmpty()) {
                ++state.emptyPages;
                if (state.done) {
                    break;
                }
                // 防止错误提供器返回无限空页；正常数据库分页不会触发该限制。
                if (state.emptyPages > 1024) {
                    state.error = tr("分页数据提供器连续返回空页: %1").arg(state.channelId);
                    return false;
                }
            } else {
                state.emptyPages = 0;
            }
        }
        return state.index < state.samples.size();
    };

    const QString sep = format == QStringLiteral("TSV")
        ? QStringLiteral("\t") : m_config.csvSeparator;
    const auto field = [&sep](const QString& value, bool protectFormula) {
        return encodeDelimitedField(value, sep, protectFormula);
    };
    const bool isJson = format == QStringLiteral("JSON");

    if (isJson) {
        if (!writeText(QStringLiteral("{\"metadata\":{\"projectName\":")
                       + QString::fromUtf8(jsonQuote(metadata.projectName))
                       + QStringLiteral(",\"exportTime\":")
                       + QString::fromUtf8(jsonQuote(metadata.exportTime.toString(Qt::ISODate)))
                       + QStringLiteral(",\"exportTimeMs\":")
                       + QString::number(metadata.exportTime.toMSecsSinceEpoch())
                       + QStringLiteral(",\"timeWindowMs\":")
                       + QString::number(metadata.timeWindowMs)
                       + QStringLiteral(",\"exportFormat\":")
                       + QString::fromUtf8(jsonQuote(format))
                       + QStringLiteral(",\"totalChannels\":")
                       + QString::number(channelInfos.size())
                       + QStringLiteral(",\"totalSamples\":")
                       + QString::number(metadata.totalSamples)
                       + QStringLiteral(",\"softwareVersion\":")
                       + QString::fromUtf8(jsonQuote(metadata.softwareVersion)))) {
            return result;
        }

        for (auto it = metadata.customFields.constBegin();
             it != metadata.customFields.constEnd(); ++it) {
            if (!writeText(QStringLiteral(",") + QString::fromUtf8(jsonQuote(it.key()))
                           + QStringLiteral(":") + QString::fromUtf8(jsonVariant(it.value())))) {
                return result;
            }
        }

        if (!writeText(QStringLiteral("},\"channels\":["))) {
            return result;
        }

        bool firstChannel = true;
        qint64 exportedCount = 0;
        for (int channelIndex = 0; channelIndex < channelInfos.size(); ++channelIndex) {
            const ExportChannelInfo& info = channelInfos.at(channelIndex);
            if (!firstChannel && !writeText(QStringLiteral(","))) {
                return result;
            }
            firstChannel = false;
            if (!writeText(QStringLiteral("{\"channelId\":")
                           + QString::fromUtf8(jsonQuote(info.channelId))
                           + QStringLiteral(",\"displayName\":")
                           + QString::fromUtf8(jsonQuote(info.displayName))
                           + QStringLiteral(",\"unit\":")
                           + QString::fromUtf8(jsonQuote(info.unit))
                           + QStringLiteral(",\"samplePeriodMs\":")
                           + QString::number(info.samplePeriodMs)
                           + QStringLiteral(",\"sampleCount\":")
                           + QString::number(info.sampleCount)
                           + QStringLiteral(",\"samples\":["))) {
                return result;
            }

            bool firstSample = true;
            ChannelState& state = states[channelIndex];
            while (ensureCurrent(state)) {
                const Monitor::Sample sample = state.samples.at(state.index++);
                if (!firstSample && !writeText(QStringLiteral(","))) {
                    return result;
                }
                firstSample = false;
                if (!writeChunk(jsonSample(sample))) {
                    return result;
                }
                ++exportedCount;
            }
            if (!state.error.isEmpty()) {
                return fail(state.error);
            }
            if (!writeText(QStringLiteral("]}"))) {
                return result;
            }
        }

        if (!writeText(QStringLiteral("]}"))) {
            return result;
        }

        if (!commitSaveFile(file)) {
            return fail(tr("提交文件失败: %1 (%2)")
                            .arg(result.filePath, file.errorString()));
        }
        result.success = true;
        result.exportedCount = static_cast<int>(exportedCount);
    } else {
        if (m_config.includeMetadataComments) {
            QString comments;
            QTextStream commentStream(&comments);
            commentStream << "# ServoValvePlatform Monitor Data Export\n"
                          << "# Export Time: " << metadata.exportTime.toString(Qt::ISODate) << "\n"
                          << "# Project: " << field(metadata.projectName, true) << "\n";
            for (auto it = metadata.customFields.constBegin();
                 it != metadata.customFields.constEnd(); ++it) {
                commentStream << "# Custom: " << field(it.key(), true) << "="
                              << field(it.value().toString(), true) << "\n";
            }
            commentStream << "# Time Window: " << metadata.timeWindowMs << " ms\n"
                          << "# Total Channels: " << channelInfos.size() << "\n"
                          << "# Total Samples: " << metadata.totalSamples << "\n"
                          << "# Software Version: " << field(metadata.softwareVersion, true) << "\n"
                          << "#\n# Channel Definitions:\n";
            int definitionIndex = 1;
            for (const ExportChannelInfo& info : channelInfos) {
                commentStream << "# [" << definitionIndex++ << "] "
                              << field(info.channelId, true) << " | "
                              << field(info.displayName, true) << " | "
                              << field(info.unit, true) << " | Period: "
                              << info.samplePeriodMs << "ms\n";
            }
            commentStream << "#\n";
            if (!writeText(comments)) {
                return result;
            }
        }

        if (m_config.csvIncludeHeader) {
            QString header = field(QStringLiteral("timestamp"), false) + sep
                           + field(QStringLiteral("timestamp_ms"), false);
            if (m_config.alignMultiChannelByTime) {
                for (const ExportChannelInfo& info : channelInfos) {
                    header += sep + field(info.channelId, true)
                           + sep + field(info.channelId + QStringLiteral("_quality"), true)
                           + sep + field(info.channelId + QStringLiteral("_value_valid"), true);
                }
            } else {
                header += sep + field(QStringLiteral("channel_id"), false) + sep
                       + field(QStringLiteral("channel_name"), false) + sep
                       + field(QStringLiteral("value"), false) + sep
                       + field(QStringLiteral("unit"), false) + sep
                       + field(QStringLiteral("quality"), false) + sep
                       + field(QStringLiteral("value_valid"), false);
            }
            header += QLatin1Char('\n');
            if (!writeText(header)) {
                return result;
            }
        }

        qint64 exportedCount = 0;
        if (m_config.alignMultiChannelByTime) {
            while (true) {
                bool anyCurrent = false;
                qint64 minimumTimestamp = 0;
                for (ChannelState& state : states) {
                    if (ensureCurrent(state)) {
                        const qint64 timestamp = state.samples.at(state.index).timestampMs();
                        if (!anyCurrent || timestamp < minimumTimestamp) {
                            minimumTimestamp = timestamp;
                            anyCurrent = true;
                        }
                    }
                    if (!state.error.isEmpty()) {
                        return fail(state.error);
                    }
                }
                if (!anyCurrent) {
                    break;
                }

                QString row = field(QDateTime::fromMSecsSinceEpoch(minimumTimestamp)
                                        .toString(m_config.timestampFormat), false)
                            + sep + field(QString::number(minimumTimestamp), false);
                for (ChannelState& state : states) {
                    Monitor::Sample latest;
                    bool found = false;
                    while (ensureCurrent(state)
                           && state.samples.at(state.index).timestampMs() == minimumTimestamp) {
                        latest = state.samples.at(state.index++);
                        found = true;
                        ++exportedCount;
                    }
                    if (!state.error.isEmpty()) {
                        return fail(state.error);
                    }
                    if (!found) {
                        row += sep + field(QString(), false)
                             + sep + field(QString(), false)
                             + sep + field(QString(), false);
                        continue;
                    }
                    row += sep + field(latest.valueValid && std::isfinite(latest.value)
                                           ? QString::number(latest.value, 'g', m_config.csvPrecision)
                                           : QString(), false)
                         + sep + field(runtimePointQualityToString(latest.quality), false)
                         + sep + field(latest.valueValid ? QStringLiteral("1")
                                                          : QStringLiteral("0"), false);
                }
                row += QLatin1Char('\n');
                if (!writeText(row)) {
                    return result;
                }
            }
        } else {
            for (ChannelState& state : states) {
                const int infoIndex = static_cast<int>(&state - states.data());
                const ExportChannelInfo& info = channelInfos.at(infoIndex);
                while (ensureCurrent(state)) {
                    const Monitor::Sample sample = state.samples.at(state.index++);
                    QString row = field(sample.timestamp.toString(m_config.timestampFormat), false)
                               + sep + field(QString::number(sample.timestampMs()), false)
                               + sep + field(info.channelId, true)
                               + sep + field(info.displayName, true)
                               + sep + field(sample.valueValid && std::isfinite(sample.value)
                                                 ? QString::number(sample.value, 'g', m_config.csvPrecision)
                                                 : QString(), false)
                               + sep + field(info.unit, true)
                               + sep + field(runtimePointQualityToString(sample.quality), false)
                               + sep + field(sample.valueValid ? QStringLiteral("1")
                                                               : QStringLiteral("0"), false)
                               + QLatin1Char('\n');
                    if (!writeText(row)) {
                        return result;
                    }
                    ++exportedCount;
                }
                if (!state.error.isEmpty()) {
                    return fail(state.error);
                }
            }
        }

        if (!commitSaveFile(file)) {
            return fail(tr("提交文件失败: %1 (%2)")
                            .arg(result.filePath, file.errorString()));
        }
        result.success = true;
        result.exportedCount = static_cast<int>(exportedCount);
    }

    result.fileSizeBytes = QFileInfo(result.filePath).size();
    emit exportFinished(result);
    return result;
}
