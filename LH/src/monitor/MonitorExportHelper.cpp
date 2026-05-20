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
#include <QFileInfo>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QSvgGenerator>
#include <QPainter>
#include <QSet>
#include <algorithm>

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
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorMsg = tr("无法打开文件进行写入: %1 (%2)")
            .arg(filePath, file.errorString());
        return false;
    }
    
    qint64 bytesWritten = file.write(data);
    if (bytesWritten != data.size()) {
        errorMsg = tr("写入文件不完整: 期望 %1 字节，实际写入 %2 字节")
            .arg(data.size()).arg(bytesWritten);
        file.close();
        return false;
    }
    
    file.close();
    
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
    
    QString sep = m_config.csvSeparator;
    
    // 写入元数据注释（如果启用）
    if (m_config.includeMetadataComments) {
        stream << "# ServoValvePlatform Monitor Data Export\n";
        stream << "# Export Time: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        stream << "# Channel: " << channelName << "\n";
        stream << "# Sample Count: " << samples.size() << "\n";
        if (!samples.isEmpty()) {
            stream << "# Unit: " << samples.first().unit << "\n";
        }
        stream << "#\n";
    }
    
    // 写入表头
    if (m_config.csvIncludeHeader) {
        stream << "timestamp" << sep 
               << "timestamp_ms" << sep 
               << "channel" << sep 
               << "value" << sep 
               << "unit\n";
    }
    
    // 写入数据行
    for (const Monitor::Sample& sample : samples) {
        stream << sample.timestamp.toString(m_config.timestampFormat) << sep
               << sample.timestamp.toMSecsSinceEpoch() << sep
               << sample.channelName << sep
               << sample.value << sep
               << sample.unit << "\n";
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
    
    QString sep = m_config.csvSeparator;
    
    // 写入元数据注释
    if (m_config.includeMetadataComments) {
        stream << "# ServoValvePlatform Monitor Data Export\n";
        stream << "# Export Time: " << package.metadata.exportTime.toString(Qt::ISODate) << "\n";
        stream << "# Time Window: " << package.metadata.timeWindowMs << " ms\n";
        stream << "# Total Channels: " << package.channelInfos.size() << "\n";
        stream << "# Total Samples: " << package.totalSampleCount() << "\n";
        stream << "# Software Version: " << package.metadata.softwareVersion << "\n";
        stream << "#\n";
        stream << "# Channel Definitions:\n";
        int idx = 1;
        for (const ExportChannelInfo& info : package.channelInfos) {
            stream << "# [" << idx++ << "] " << info.channelId 
                   << " | " << info.displayName 
                   << " | " << info.unit 
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
        
        // 构建每个通道的时间戳到值的映射
        QMap<QString, QMap<qint64, double>> channelValueMaps;
        for (auto it = package.channelSamples.constBegin(); 
             it != package.channelSamples.constEnd(); ++it) {
            QMap<qint64, double> valueMap;
            for (const Monitor::Sample& sample : it.value()) {
                valueMap.insert(sample.timestampMs(), sample.value);
            }
            channelValueMaps.insert(it.key(), valueMap);
        }
        
        // 写入表头
        if (m_config.csvIncludeHeader) {
            stream << "timestamp" << sep << "timestamp_ms";
            for (const ExportChannelInfo& info : package.channelInfos) {
                stream << sep << info.channelId;
            }
            stream << "\n";
        }
        
        // 写入数据行
        for (qint64 ts : sortedTimestamps) {
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(ts);
            stream << dt.toString(m_config.timestampFormat) << sep << ts;
            
            for (const ExportChannelInfo& info : package.channelInfos) {
                stream << sep;
                const QMap<qint64, double>& valueMap = channelValueMaps.value(info.channelId);
                if (valueMap.contains(ts)) {
                    stream << valueMap.value(ts);
                }
                // 如果该时间点没有数据，则留空
            }
            stream << "\n";
        }
    } else {
        // 非对齐模式：每个样本一行，包含通道标识
        if (m_config.csvIncludeHeader) {
            stream << "timestamp" << sep 
                   << "timestamp_ms" << sep 
                   << "channel_id" << sep 
                   << "channel_name" << sep 
                   << "value" << sep 
                   << "unit\n";
        }
        
        for (const ExportChannelInfo& info : package.channelInfos) {
            const QList<Monitor::Sample>& samples = package.channelSamples.value(info.channelId);
            for (const Monitor::Sample& sample : samples) {
                stream << sample.timestamp.toString(m_config.timestampFormat) << sep
                       << sample.timestampMs() << sep
                       << info.channelId << sep
                       << info.displayName << sep
                       << sample.value << sep
                       << info.unit << "\n";
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
        sampleObj["value"] = sample.value;
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
            sampleObj["value"] = sample.value;
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
    
    // 写入元数据注释
    if (m_config.includeMetadataComments) {
        stream << "# ServoValvePlatform Monitor Data Export\n";
        stream << "# Export Time: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        stream << "# Channel: " << channelName << "\n";
        stream << "# Sample Count: " << samples.size() << "\n";
        if (!samples.isEmpty()) {
            stream << "# Unit: " << samples.first().unit << "\n";
        }
        stream << "#\n";
    }
    
    // 写入表头（TSV 使用制表符分隔）
    if (m_config.csvIncludeHeader) {
        stream << "timestamp\ttimestamp_ms\tvalue\tunit\n";
    }
    
    // 写入数据行
    for (const Monitor::Sample& sample : samples) {
        stream << sample.timestamp.toString(m_config.timestampFormat) << "\t"
               << sample.timestampMs() << "\t"
               << sample.value << "\t"
               << sample.unit << "\n";
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
    
    // 构建每个通道的时间戳到值的映射
    QMap<QString, QMap<qint64, double>> channelValueMaps;
    for (auto it = package.channelSamples.constBegin(); 
         it != package.channelSamples.constEnd(); ++it) {
        QMap<qint64, double> valueMap;
        for (const Monitor::Sample& sample : it.value()) {
            valueMap.insert(sample.timestampMs(), sample.value);
        }
        channelValueMaps.insert(it.key(), valueMap);
    }
    
    // 写入表头
    if (m_config.csvIncludeHeader) {
        stream << "timestamp\ttimestamp_ms";
        for (const ExportChannelInfo& info : package.channelInfos) {
            stream << "\t" << info.channelId;
        }
        stream << "\n";
    }
    
    // 写入数据行
    for (qint64 ts : sortedTimestamps) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(ts);
        stream << dt.toString(m_config.timestampFormat) << "\t" << ts;
        
        for (const ExportChannelInfo& info : package.channelInfos) {
            stream << "\t";
            const QMap<qint64, double>& valueMap = channelValueMaps.value(info.channelId);
            if (valueMap.contains(ts)) {
                stream << valueMap.value(ts);
            }
        }
        stream << "\n";
    }
    
    return content.toUtf8();
}

QString MonitorExportHelper::generateTsvMetadataHeader(
    const ExportDataPackage& package) const
{
    QString header;
    QTextStream stream(&header);
    
    stream << "# ServoValvePlatform Monitor Data Export\n";
    stream << "# Export Time: " << package.metadata.exportTime.toString(Qt::ISODate) << "\n";
    stream << "# Time Window: " << package.metadata.timeWindowMs << " ms\n";
    stream << "# Total Channels: " << package.channelInfos.size() << "\n";
    stream << "# Total Samples: " << package.totalSampleCount() << "\n";
    stream << "# Software Version: " << package.metadata.softwareVersion << "\n";
    stream << "#\n";
    stream << "# Channel Definitions:\n";
    
    int idx = 1;
    for (const ExportChannelInfo& info : package.channelInfos) {
        stream << "# [" << idx++ << "] " << info.channelId 
               << " | " << info.displayName 
               << " | " << info.unit 
               << " | Period: " << info.samplePeriodMs << "ms\n";
    }
    stream << "#\n";
    
    return header;
}
