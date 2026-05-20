/**
 * @file monitor_export_test.cpp
 * @brief MonitorExportHelper 单元测试
 *
 * 测试内容：
 * - CSV 导出格式和内容
 * - JSON 导出格式和内容
 * - TSV 导出格式和内容
 * - 多通道数据导出
 * - 边界条件处理
 * - 错误处理
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include "MonitorExportHelper.h"
#include "MonitorTypes.h"

class MonitorExportTest : public QObject
{
    Q_OBJECT

private:
    /**
     * @brief 生成测试采样数据
     */
    QList<Monitor::Sample> generateTestSamples(const QString& channelName,
                                                int count,
                                                const QString& unit = "bar")
    {
        QList<Monitor::Sample> samples;
        QDateTime baseTime = QDateTime::currentDateTime();
        
        for (int i = 0; i < count; ++i) {
            Monitor::Sample sample;
            sample.channelName = channelName;
            sample.value = 100.0 + i * 0.5;
            sample.unit = unit;
            sample.timestamp = baseTime.addMSecs(i * 100);
            samples.append(sample);
        }
        
        return samples;
    }

    /**
     * @brief 构建测试导出数据包
     */
    ExportDataPackage buildTestPackage()
    {
        ExportDataPackage package;
        
        // 设置元数据
        package.metadata.projectName = "测试项目";
        package.metadata.exportTime = QDateTime::currentDateTime();
        package.metadata.timeWindowMs = 30000;
        package.metadata.softwareVersion = "1.0.0";
        
        // 添加通道 1
        ExportChannelInfo info1("pressure", "系统压力", "bar", 100);
        package.channelInfos.append(info1);
        package.channelSamples.insert("pressure", 
            generateTestSamples("pressure", 10, "bar"));
        
        // 添加通道 2
        ExportChannelInfo info2("flow", "系统流量", "L/min", 100);
        package.channelInfos.append(info2);
        package.channelSamples.insert("flow", 
            generateTestSamples("flow", 10, "L/min"));
        
        package.metadata.totalChannels = package.channelInfos.size();
        package.metadata.totalSamples = package.totalSampleCount();
        
        return package;
    }

private slots:
    void initTestCase()
    {
        qInfo() << "========================================";
        qInfo() << "MonitorExportHelper Test Suite";
        qInfo() << "========================================";
    }

    void cleanupTestCase()
    {
        qInfo() << "All MonitorExportHelper tests completed.";
    }

    // =========================================================================
    // 测试 1：单通道 CSV 导出
    // =========================================================================

    void testSingleChannelCsvExport()
    {
        MonitorExportHelper helper;
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString filePath = tempDir.path() + "/test_single.csv";
        QList<Monitor::Sample> samples = generateTestSamples("test_channel", 5);
        
        ExportResult result = helper.exportDataAsCsvToFile("test_channel", samples, filePath);
        
        QVERIFY2(result.success, qPrintable(result.errorMessage));
        QCOMPARE(result.exportedCount, 5);
        QVERIFY(result.fileSizeBytes > 0);
        
        // 验证文件内容
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        
        // 检查是否包含表头
        QVERIFY(content.contains("timestamp"));
        QVERIFY(content.contains("timestamp_ms"));
        QVERIFY(content.contains("value"));
        
        // 检查是否包含元数据注释
        QVERIFY(content.contains("# ServoValvePlatform"));
        QVERIFY(content.contains("# Channel: test_channel"));
        
        qInfo() << "单通道 CSV 导出测试通过";
    }

    // =========================================================================
    // 测试 2：单通道 JSON 导出
    // =========================================================================

    void testSingleChannelJsonExport()
    {
        MonitorExportHelper helper;
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString filePath = tempDir.path() + "/test_single.json";
        QList<Monitor::Sample> samples = generateTestSamples("pressure", 5, "bar");
        
        ExportResult result = helper.exportDataAsJsonToFile("pressure", samples, filePath);
        
        QVERIFY2(result.success, qPrintable(result.errorMessage));
        QCOMPARE(result.exportedCount, 5);
        
        // 验证 JSON 结构
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QVERIFY(!doc.isNull());
        QVERIFY(doc.isObject());
        
        QJsonObject root = doc.object();
        QVERIFY(root.contains("metadata"));
        QVERIFY(root.contains("samples"));
        
        QJsonObject metadata = root["metadata"].toObject();
        QCOMPARE(metadata["channelName"].toString(), QString("pressure"));
        QCOMPARE(metadata["sampleCount"].toInt(), 5);
        QCOMPARE(metadata["unit"].toString(), QString("bar"));
        
        QJsonArray samplesArray = root["samples"].toArray();
        QCOMPARE(samplesArray.size(), 5);
        
        // 检查样本格式
        QJsonObject firstSample = samplesArray[0].toObject();
        QVERIFY(firstSample.contains("timestamp"));
        QVERIFY(firstSample.contains("timestampMs"));
        QVERIFY(firstSample.contains("value"));
        
        qInfo() << "单通道 JSON 导出测试通过";
    }

    // =========================================================================
    // 测试 3：单通道 TSV 导出
    // =========================================================================

    void testSingleChannelTsvExport()
    {
        MonitorExportHelper helper;
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString filePath = tempDir.path() + "/test_single.tsv";
        QList<Monitor::Sample> samples = generateTestSamples("temperature", 5, "°C");
        
        ExportResult result = helper.exportDataAsTsvToFile("temperature", samples, filePath);
        
        QVERIFY2(result.success, qPrintable(result.errorMessage));
        QCOMPARE(result.exportedCount, 5);
        
        // 验证文件内容
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        
        // 检查是否使用制表符分隔
        QVERIFY(content.contains("\t"));
        QVERIFY(content.contains("timestamp\t"));
        
        // 检查元数据注释
        QVERIFY(content.contains("# Channel: temperature"));
        
        qInfo() << "单通道 TSV 导出测试通过";
    }

    // =========================================================================
    // 测试 4：多通道 CSV 导出
    // =========================================================================

    void testMultiChannelCsvExport()
    {
        MonitorExportHelper helper;
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString filePath = tempDir.path() + "/test_multi.csv";
        ExportDataPackage package = buildTestPackage();
        
        ExportResult result = helper.exportPackageAsCsv(package, filePath);
        
        QVERIFY2(result.success, qPrintable(result.errorMessage));
        QCOMPARE(result.exportedCount, 20);  // 2 通道 x 10 样本
        
        // 验证文件内容
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        
        // 检查通道定义注释
        QVERIFY(content.contains("# [1] pressure"));
        QVERIFY(content.contains("# [2] flow"));
        
        // 检查表头包含两个通道列
        QVERIFY(content.contains("pressure"));
        QVERIFY(content.contains("flow"));
        
        qInfo() << "多通道 CSV 导出测试通过";
    }

    // =========================================================================
    // 测试 5：多通道 JSON 导出
    // =========================================================================

    void testMultiChannelJsonExport()
    {
        MonitorExportHelper helper;
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString filePath = tempDir.path() + "/test_multi.json";
        ExportDataPackage package = buildTestPackage();
        
        ExportResult result = helper.exportPackageAsJson(package, filePath);
        
        QVERIFY2(result.success, qPrintable(result.errorMessage));
        
        // 验证 JSON 结构
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        
        QJsonObject root = doc.object();
        
        // 检查元数据
        QJsonObject metadata = root["metadata"].toObject();
        QCOMPARE(metadata["totalChannels"].toInt(), 2);
        QCOMPARE(metadata["totalSamples"].toInt(), 20);
        QVERIFY(metadata.contains("exportTime"));
        QVERIFY(metadata.contains("exportTimeMs"));
        
        // 检查通道数组
        QJsonArray channels = root["channels"].toArray();
        QCOMPARE(channels.size(), 2);
        
        // 检查第一个通道
        QJsonObject channel1 = channels[0].toObject();
        QCOMPARE(channel1["channelId"].toString(), QString("pressure"));
        QCOMPARE(channel1["displayName"].toString(), QString("系统压力"));
        QCOMPARE(channel1["unit"].toString(), QString("bar"));
        QCOMPARE(channel1["sampleCount"].toInt(), 10);
        
        QJsonArray samples1 = channel1["samples"].toArray();
        QCOMPARE(samples1.size(), 10);
        
        qInfo() << "多通道 JSON 导出测试通过";
    }

    // =========================================================================
    // 测试 6：多通道 TSV 导出
    // =========================================================================

    void testMultiChannelTsvExport()
    {
        MonitorExportHelper helper;
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString filePath = tempDir.path() + "/test_multi.tsv";
        ExportDataPackage package = buildTestPackage();
        
        ExportResult result = helper.exportPackageAsTsv(package, filePath);
        
        QVERIFY2(result.success, qPrintable(result.errorMessage));
        
        // 验证文件内容
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        
        // 检查通道定义
        QVERIFY(content.contains("# [1] pressure | 系统压力 | bar"));
        QVERIFY(content.contains("# [2] flow | 系统流量 | L/min"));
        
        // 检查表头
        QStringList lines = content.split('\n', Qt::SkipEmptyParts);
        QString headerLine;
        for (const QString& line : lines) {
            if (!line.startsWith('#')) {
                headerLine = line;
                break;
            }
        }
        QVERIFY(headerLine.contains("timestamp"));
        QVERIFY(headerLine.contains("pressure"));
        QVERIFY(headerLine.contains("flow"));
        
        qInfo() << "多通道 TSV 导出测试通过";
    }

    // =========================================================================
    // 测试 7：空数据处理
    // =========================================================================

    void testEmptyDataHandling()
    {
        MonitorExportHelper helper;
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString filePath = tempDir.path() + "/test_empty.csv";
        QList<Monitor::Sample> emptySamples;
        
        ExportResult result = helper.exportDataAsCsvToFile("empty", emptySamples, filePath);
        
        // 应该失败，因为没有数据
        QVERIFY(!result.success);
        QVERIFY(result.errorMessage.contains("没有可导出的数据"));
        
        qInfo() << "空数据处理测试通过";
    }

    // =========================================================================
    // 测试 8：无效路径处理
    // =========================================================================

    void testInvalidPathHandling()
    {
        MonitorExportHelper helper;
        
        QString invalidPath = "/nonexistent_directory_12345/test.csv";
        QList<Monitor::Sample> samples = generateTestSamples("test", 5);
        
        ExportResult result = helper.exportDataAsCsvToFile("test", samples, invalidPath);
        
        // 应该失败，因为目录不存在
        QVERIFY(!result.success);
        QVERIFY(!result.errorMessage.isEmpty());
        
        qInfo() << "无效路径处理测试通过";
    }

    // =========================================================================
    // 测试 9：自动格式选择
    // =========================================================================

    void testAutoFormatSelection()
    {
        MonitorExportHelper helper;
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        ExportDataPackage package = buildTestPackage();
        
        // 测试 CSV 扩展名
        QString csvPath = tempDir.path() + "/auto_test.csv";
        ExportResult csvResult = helper.exportPackageAuto(package, csvPath);
        QVERIFY(csvResult.success);
        QCOMPARE(csvResult.exportFormat, QString("CSV"));
        
        // 测试 JSON 扩展名
        QString jsonPath = tempDir.path() + "/auto_test.json";
        ExportResult jsonResult = helper.exportPackageAuto(package, jsonPath);
        QVERIFY(jsonResult.success);
        QCOMPARE(jsonResult.exportFormat, QString("JSON"));
        
        // 测试 TSV 扩展名
        QString tsvPath = tempDir.path() + "/auto_test.tsv";
        ExportResult tsvResult = helper.exportPackageAuto(package, tsvPath);
        QVERIFY(tsvResult.success);
        QCOMPARE(tsvResult.exportFormat, QString("TSV"));
        
        qInfo() << "自动格式选择测试通过";
    }

    // =========================================================================
    // 测试 10：时间戳格式验证
    // =========================================================================

    void testTimestampFormat()
    {
        MonitorExportHelper helper;
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString filePath = tempDir.path() + "/timestamp_test.json";
        QList<Monitor::Sample> samples = generateTestSamples("test", 1);
        
        ExportResult result = helper.exportDataAsJsonToFile("test", samples, filePath);
        QVERIFY(result.success);
        
        // 验证 JSON 中的时间戳格式
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        
        QJsonArray samplesArray = doc.object()["samples"].toArray();
        QJsonObject sample = samplesArray[0].toObject();
        
        // 检查 ISO 8601 格式
        QString timestamp = sample["timestamp"].toString();
        QVERIFY(timestamp.contains("T"));  // ISO 8601 包含 T 分隔符
        
        // 检查毫秒时间戳
        qint64 timestampMs = sample["timestampMs"].toVariant().toLongLong();
        QVERIFY(timestampMs > 0);
        
        // 验证两个时间戳的一致性
        QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
        QVERIFY(qAbs(dt.toMSecsSinceEpoch() - timestampMs) < 1000);  // 允许 1 秒误差
        
        qInfo() << "时间戳格式验证测试通过";
    }
};

QTEST_MAIN(MonitorExportTest)
#include "monitor_export_test.moc"
