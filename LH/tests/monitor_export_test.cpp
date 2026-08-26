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
#include <QHash>
#include <QFileInfo>
#include <QSaveFile>
#include <functional>

#include "MonitorExportHelper.h"
#include "MonitorTypes.h"

class CommitFailingExportHelper final : public MonitorExportHelper
{
public:
    explicit CommitFailingExportHelper(QObject* parent = nullptr)
        : MonitorExportHelper(parent)
    {
    }

protected:
    bool commitSaveFile(QSaveFile&) override
    {
        return false;
    }
};

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

    void testDelimitedFieldEncodingAndFormulaProtection()
    {
        MonitorExportHelper helper;
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString channelId = QStringLiteral("=通道, \"A\"\n第二行");
        const QString displayName = QStringLiteral("@显示, \"名称\"\n第二行");
        const QString unit = QStringLiteral("+单位\t\"B\"\r\n");
        const QString projectName = QStringLiteral("-项目, \"项目\"\n下一行");
        const QString customKey = QStringLiteral("@key");
        const QString customValue = QStringLiteral("=value, \"值\"\r\n");

        Monitor::Sample sample;
        sample.channelName = channelId;
        sample.unit = unit;
        sample.timestamp = QDateTime::fromMSecsSinceEpoch(1000, Qt::UTC);
        sample.value = -12.5;
        sample.valueValid = true;

        Monitor::Sample emptySample = sample;
        emptySample.channelName = QStringLiteral("plain");
        emptySample.unit.clear();
        emptySample.valueValid = false;
        emptySample.timestamp = sample.timestamp.addMSecs(1);
        const QList<Monitor::Sample> samples{sample, emptySample};

        const auto readText = [](const QString& path) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                return QString();
            }
            return QString::fromUtf8(file.readAll());
        };
        const QString csvField = QStringLiteral("\"'=通道, \"\"A\"\"\n第二行\"");
        const QString tsvUnitField = QStringLiteral("\"'+单位\t\"\"B\"\"\r\n\"");

        const QString singleCsvPath = tempDir.path() + QStringLiteral("/special_single.csv");
        const ExportResult singleCsv = helper.exportDataAsCsvToFile(
            channelId, samples, singleCsvPath);
        QVERIFY2(singleCsv.success, qPrintable(singleCsv.errorMessage));
        const QString singleCsvContent = readText(singleCsvPath);
        QVERIFY(singleCsvContent.contains(csvField));
        QVERIFY(singleCsvContent.contains(tsvUnitField));
        QVERIFY(singleCsvContent.contains(QStringLiteral(",-12.5,\"'+单位")));
        QVERIFY(!singleCsvContent.contains(QStringLiteral(",'-12.5")));
        QVERIFY(singleCsvContent.contains(QStringLiteral(",plain,,,Good,0")));

        const QString singleTsvPath = tempDir.path() + QStringLiteral("/special_single.tsv");
        const ExportResult singleTsv = helper.exportDataAsTsvToFile(
            channelId, samples, singleTsvPath);
        QVERIFY2(singleTsv.success, qPrintable(singleTsv.errorMessage));
        const QString singleTsvContent = readText(singleTsvPath);
        QVERIFY(singleTsvContent.contains(csvField));
        QVERIFY(singleTsvContent.contains(tsvUnitField));
        QVERIFY(singleTsvContent.contains(QStringLiteral("\t\t\tGood\t0")));

        ExportDataPackage package;
        package.metadata.projectName = projectName;
        package.metadata.customFields.insert(customKey, customValue);
        package.metadata.totalChannels = 1;
        package.metadata.totalSamples = samples.size();
        package.channelInfos.append(ExportChannelInfo(channelId, displayName, unit));
        package.channelSamples.insert(channelId, samples);

        const QString packageCsvPath = tempDir.path() + QStringLiteral("/special_package.csv");
        const ExportResult packageCsv = helper.exportPackageAsCsv(package, packageCsvPath);
        QVERIFY2(packageCsv.success, qPrintable(packageCsv.errorMessage));
        const QString packageCsvContent = readText(packageCsvPath);
        QVERIFY(packageCsvContent.contains(csvField));
        QVERIFY(packageCsvContent.contains(QStringLiteral("# Project: \"'-项目, \"\"项目\"\"\n下一行\"")));
        QVERIFY(packageCsvContent.contains(QStringLiteral("'@key")));
        QVERIFY(packageCsvContent.contains(QStringLiteral("\"'=value, \"\"值\"\"\r\n\"")));

        ExportConfig nonAlignedConfig = helper.config();
        nonAlignedConfig.alignMultiChannelByTime = false;
        helper.setConfig(nonAlignedConfig);
        const QString nonAlignedPath = tempDir.path() + QStringLiteral("/special_non_aligned.csv");
        const ExportResult nonAligned = helper.exportPackageAsCsv(package, nonAlignedPath);
        QVERIFY2(nonAligned.success, qPrintable(nonAligned.errorMessage));
        const QString nonAlignedContent = readText(nonAlignedPath);
        QVERIFY(nonAlignedContent.contains(csvField));
        QVERIFY(nonAlignedContent.contains(QStringLiteral("\"'+单位\t\"\"B\"\"\r\n\"")));

        nonAlignedConfig.alignMultiChannelByTime = true;
        helper.setConfig(nonAlignedConfig);
        const QString packageTsvPath = tempDir.path() + QStringLiteral("/special_package.tsv");
        const ExportResult packageTsv = helper.exportPackageAsTsv(package, packageTsvPath);
        QVERIFY2(packageTsv.success, qPrintable(packageTsv.errorMessage));
        const QString packageTsvContent = readText(packageTsvPath);
        QVERIFY(packageTsvContent.contains(csvField));
        QVERIFY(packageTsvContent.contains(tsvUnitField));

        const ExportPageProvider provider = [samples](const QString&, const ExportCursor&, int) {
            ExportPage page;
            page.samples = samples;
            return page;
        };
        const QString pagedCsvPath = tempDir.path() + QStringLiteral("/special_paged.csv");
        const ExportResult pagedCsv = helper.exportPackagePaged(
            package.channelInfos, package.metadata, provider, pagedCsvPath, 2);
        QVERIFY2(pagedCsv.success, qPrintable(pagedCsv.errorMessage));
        const QString pagedCsvContent = readText(pagedCsvPath);
        QVERIFY(pagedCsvContent.contains(csvField));
        QVERIFY(pagedCsvContent.contains(QStringLiteral(",-12.5,Good,1")));

        const QString pagedTsvPath = tempDir.path() + QStringLiteral("/special_paged.tsv");
        const ExportResult pagedTsv = helper.exportPackagePaged(
            package.channelInfos, package.metadata, provider, pagedTsvPath, 2);
        QVERIFY2(pagedTsv.success, qPrintable(pagedTsv.errorMessage));
        const QString pagedTsvContent = readText(pagedTsvPath);
        QVERIFY(pagedTsvContent.contains(csvField));
        QVERIFY(pagedTsvContent.contains(tsvUnitField));
    }

    void testCommitFailurePreservesExistingTargets()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QList<Monitor::Sample> samples = generateTestSamples("pressure", 2);
        const ExportDataPackage package = buildTestPackage();
        using ExportCall = std::function<ExportResult(CommitFailingExportHelper&, const QString&)>;
        struct ExportCase {
            const char* name;
            const char* extension;
            ExportCall call;
        };
        const QList<ExportCase> cases = {
            {"single-csv", "csv", [&](CommitFailingExportHelper& helper, const QString& path) {
                return helper.exportDataAsCsvToFile("pressure", samples, path);
            }},
            {"single-json", "json", [&](CommitFailingExportHelper& helper, const QString& path) {
                return helper.exportDataAsJsonToFile("pressure", samples, path);
            }},
            {"single-tsv", "tsv", [&](CommitFailingExportHelper& helper, const QString& path) {
                return helper.exportDataAsTsvToFile("pressure", samples, path);
            }},
            {"package-csv", "csv", [&](CommitFailingExportHelper& helper, const QString& path) {
                return helper.exportPackageAsCsv(package, path);
            }},
            {"package-json", "json", [&](CommitFailingExportHelper& helper, const QString& path) {
                return helper.exportPackageAsJson(package, path);
            }},
            {"package-tsv", "tsv", [&](CommitFailingExportHelper& helper, const QString& path) {
                return helper.exportPackageAsTsv(package, path);
            }}
        };
        const QByteArray oldContent = QByteArrayLiteral("pre-existing-target-content");

        for (const ExportCase& testCase : cases) {
            const QString filePath = tempDir.path() + QStringLiteral("/commit_failure_%1.%2")
                .arg(QString::fromLatin1(testCase.name),
                     QString::fromLatin1(testCase.extension));
            QFile oldFile(filePath);
            QVERIFY(oldFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QCOMPARE(oldFile.write(oldContent), qint64(oldContent.size()));
            oldFile.close();

            CommitFailingExportHelper helper;
            const ExportResult result = testCase.call(helper, filePath);
            QVERIFY2(!result.success, testCase.name);
            QVERIFY2(!result.errorMessage.isEmpty(), testCase.name);

            QFile preservedFile(filePath);
            QVERIFY2(QFileInfo::exists(filePath), testCase.name);
            QVERIFY2(preservedFile.open(QIODevice::ReadOnly), testCase.name);
            QCOMPARE(preservedFile.readAll(), oldContent);
        }
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

    void testPagedStreamingFormatsAndAtomicFailure()
    {
        MonitorExportHelper helper;
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QDateTime base = QDateTime::fromMSecsSinceEpoch(1000, Qt::UTC);
        QList<Monitor::Sample> pressure;
        constexpr int pressureCount = 1205;
        constexpr int flowCount = 803;
        for (int i = 0; i < pressureCount; ++i) {
            Monitor::Sample sample;
            sample.channelName = QStringLiteral("pressure");
            sample.unit = QStringLiteral("bar");
            sample.timestamp = base.addMSecs(i * 100);
            sample.value = 10.0 + i;
            pressure.append(sample);
        }
        // 将重复时间戳放在页边界，验证跨页时同通道最后一条覆盖前一条，
        // 但计数仍包含输入样本。
        pressure[17].timestamp = pressure[16].timestamp;
        pressure[17].value = 99.0;

        QList<Monitor::Sample> flow;
        for (int i = 0; i < flowCount; ++i) {
            Monitor::Sample sample;
            sample.channelName = QStringLiteral("flow");
            sample.unit = QStringLiteral("L/min");
            sample.timestamp = base.addMSecs(i * 200);
            sample.value = 20.0 + i;
            flow.append(sample);
        }
        flow[1].quality = RuntimePointQuality::Bad;
        flow[1].valueValid = false;

        QList<ExportChannelInfo> infos;
        infos.append(ExportChannelInfo(QStringLiteral("pressure"), QStringLiteral("压力"),
                                       QStringLiteral("bar"), 100));
        infos.append(ExportChannelInfo(QStringLiteral("flow"), QStringLiteral("流量"),
                                       QStringLiteral("L/min"), 100));
        infos[0].sampleCount = pressure.size();
        infos[1].sampleCount = flow.size();
        ExportMetadata metadata;
        metadata.exportTime = base;
        metadata.totalChannels = 2;
        metadata.totalSamples = pressure.size() + flow.size();

        QHash<QString, int> offsets;
        int requestCount = 0;
        int maxRequested = 0;
        int maxReturned = 0;
        bool requestExceededPageSize = false;
        bool responseExceededPageSize = false;
        const auto provider = [&, pressure, flow](const QString& channelId,
                                                   const ExportCursor& cursor,
                                                   int pageSize) -> ExportPage {
            Q_UNUSED(cursor);
            ++requestCount;
            maxRequested = qMax(maxRequested, pageSize);
            if (pageSize <= 0 || pageSize > 17) {
                requestExceededPageSize = true;
            }
            const QList<Monitor::Sample>& source = channelId == QStringLiteral("pressure")
                ? pressure : flow;
            const int offset = offsets.value(channelId);
            ExportPage page;
            const int count = qMin(pageSize, source.size() - offset);
            maxReturned = qMax(maxReturned, count);
            if (count > pageSize) {
                responseExceededPageSize = true;
            }
            if (count > 0) {
                page.samples = source.mid(offset, count);
                offsets.insert(channelId, offset + count);
                page.hasMore = offset + count < source.size();
                if (page.hasMore) {
                    page.nextCursor.timestamp = page.samples.last().timestamp;
                    page.nextCursor.id = offset + count;
                }
            }
            return page;
        };

        const int pageSize = 17;
        const auto resetPagingStats = [&]() {
            offsets.clear();
            requestCount = 0;
            maxRequested = 0;
            maxReturned = 0;
            requestExceededPageSize = false;
            responseExceededPageSize = false;
        };
        const auto pagingStatsValid = [&]() {
            return !requestExceededPageSize && !responseExceededPageSize
                && maxRequested == pageSize && maxReturned <= pageSize
                && requestCount > 100;
        };

        const QString csvPath = tempDir.path() + QStringLiteral("/paged.csv");
        resetPagingStats();
        ExportResult csv = helper.exportPackagePaged(infos, metadata, provider, csvPath,
                                                     pageSize);
        QVERIFY2(csv.success, qPrintable(csv.errorMessage));
        QCOMPARE(csv.exportedCount, pressure.size() + flow.size());
        QVERIFY2(pagingStatsValid(), "paged provider statistics are invalid");
        QCOMPARE(maxRequested, pageSize);
        QVERIFY(maxReturned <= pageSize);
        QVERIFY(requestCount > 100);
        QFile csvFile(csvPath);
        QVERIFY(csvFile.open(QIODevice::ReadOnly));
        const QString csvContent = QString::fromUtf8(csvFile.readAll());
        csvFile.close();
        QVERIFY(csvContent.contains(QStringLiteral("99")));
        QVERIFY(csvContent.contains(QStringLiteral(",2600,99,Good,1")));
        QVERIFY(!csvContent.contains(QStringLiteral(",2600,26,Good,1")));
        QVERIFY(csvContent.contains(QStringLiteral("Bad")));
        QVERIFY(csvContent.contains(QStringLiteral("Bad,0")));
        QVERIFY(csvContent.contains(QStringLiteral("# Total Samples: 2008")));

        resetPagingStats();
        const QString jsonPath = tempDir.path() + QStringLiteral("/paged.json");
        ExportResult json = helper.exportPackagePaged(infos, metadata, provider, jsonPath,
                                                      pageSize);
        QVERIFY2(json.success, qPrintable(json.errorMessage));
        QCOMPARE(json.exportedCount, pressure.size() + flow.size());
        QVERIFY2(pagingStatsValid(), "paged provider statistics are invalid");
        QCOMPARE(maxRequested, pageSize);
        QVERIFY(maxReturned <= pageSize);
        QVERIFY(requestCount > 100);
        QFile jsonFile(jsonPath);
        QVERIFY(jsonFile.open(QIODevice::ReadOnly));
        const QJsonDocument document = QJsonDocument::fromJson(jsonFile.readAll());
        jsonFile.close();
        QVERIFY(!document.isNull());
        const QJsonObject root = document.object();
        const QJsonObject jsonMetadata = root.value(QStringLiteral("metadata")).toObject();
        QCOMPARE(jsonMetadata.value(QStringLiteral("totalSamples")).toInt(),
                 metadata.totalSamples);
        const QJsonArray channels = root.value(QStringLiteral("channels")).toArray();
        QCOMPARE(channels.size(), 2);
        const QJsonObject pressureChannel = channels.at(0).toObject();
        const QJsonArray pressureSamples = pressureChannel.value(QStringLiteral("samples"))
                                               .toArray();
        QCOMPARE(pressureChannel.value(QStringLiteral("sampleCount")).toInt(),
                 pressure.size());
        QCOMPARE(pressureSamples.size(), pressure.size());
        QCOMPARE(pressureSamples.at(17).toObject().value(QStringLiteral("value")).toDouble(),
                 99.0);
        const QJsonObject flowChannel = channels.at(1).toObject();
        const QJsonArray flowSamples = flowChannel.value(QStringLiteral("samples")).toArray();
        QCOMPARE(flowChannel.value(QStringLiteral("sampleCount")).toInt(), flow.size());
        QCOMPARE(flowSamples.size(), flow.size());
        const QJsonObject invalidFlowSample = flowSamples.at(1).toObject();
        QVERIFY(invalidFlowSample.value(QStringLiteral("value")).isNull());
        QCOMPARE(invalidFlowSample.value(QStringLiteral("quality")).toString(),
                 QStringLiteral("Bad"));
        QVERIFY(!invalidFlowSample.value(QStringLiteral("valueValid")).toBool());

        resetPagingStats();
        const QString tsvPath = tempDir.path() + QStringLiteral("/paged.tsv");
        ExportResult tsv = helper.exportPackagePaged(infos, metadata, provider, tsvPath,
                                                     pageSize);
        QVERIFY2(tsv.success, qPrintable(tsv.errorMessage));
        QCOMPARE(tsv.exportedCount, pressure.size() + flow.size());
        QVERIFY2(pagingStatsValid(), "paged provider statistics are invalid");
        QCOMPARE(maxRequested, pageSize);
        QVERIFY(maxReturned <= pageSize);
        QVERIFY(requestCount > 100);
        QVERIFY(QFileInfo::exists(tsvPath));
        QFile tsvFile(tsvPath);
        QVERIFY(tsvFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString tsvContent = QString::fromUtf8(tsvFile.readAll());
        tsvFile.close();
        QVERIFY(tsvContent.contains(QStringLiteral("Bad")));
        QVERIFY(tsvContent.contains(QStringLiteral("\t2600\t99\tGood\t1")));
        QVERIFY(!tsvContent.contains(QStringLiteral("\t2600\t26\tGood\t1")));
        QVERIFY(tsvContent.contains(QStringLiteral("Bad\t0")));
        QVERIFY(tsvContent.contains(QStringLiteral("# Total Samples: 2008")));

        ExportConfig nonAlignedConfig = helper.config();
        nonAlignedConfig.alignMultiChannelByTime = false;
        helper.setConfig(nonAlignedConfig);
        resetPagingStats();
        const QString nonAlignedPath = tempDir.path() + QStringLiteral("/paged_non_aligned.csv");
        const ExportResult nonAligned = helper.exportPackagePaged(
            infos, metadata, provider, nonAlignedPath, pageSize);
        QVERIFY2(nonAligned.success, qPrintable(nonAligned.errorMessage));
        QCOMPARE(nonAligned.exportedCount, pressure.size() + flow.size());
        QVERIFY2(pagingStatsValid(), "paged provider statistics are invalid");
        QCOMPARE(maxRequested, pageSize);
        QVERIFY(maxReturned <= pageSize);
        QVERIFY(requestCount > 100);
        QFile nonAlignedFile(nonAlignedPath);
        QVERIFY(nonAlignedFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString nonAlignedContent = QString::fromUtf8(nonAlignedFile.readAll());
        nonAlignedFile.close();
        QVERIFY(nonAlignedContent.contains(QStringLiteral("channel_id")));
        QVERIFY(nonAlignedContent.contains(QStringLiteral("pressure")));
        QVERIFY(nonAlignedContent.contains(QStringLiteral("Bad")));
        QVERIFY(nonAlignedContent.contains(QStringLiteral(",Bad,0")));

        const QString atomicPath = tempDir.path() + QStringLiteral("/atomic.csv");
        QFile oldFile(atomicPath);
        QVERIFY(oldFile.open(QIODevice::WriteOnly));
        oldFile.write("old-target");
        oldFile.close();
        int calls = 0;
        const ExportPageProvider failingProvider =
            [&](const QString&, const ExportCursor&, int) {
                ExportPage page;
                if (calls++ == 0) {
                    page.samples = pressure.mid(0, 1);
                    page.hasMore = true;
                } else {
                    page.success = false;
                    page.errorMessage = QStringLiteral("injected page failure");
                }
                return page;
            };
        ExportResult failed = helper.exportPackagePaged(
            {infos.first()}, metadata, failingProvider, atomicPath, 1);
        QVERIFY(!failed.success);
        QVERIFY(!failed.errorMessage.isEmpty());
        QFile preservedFile(atomicPath);
        QVERIFY(preservedFile.open(QIODevice::ReadOnly));
        QCOMPARE(preservedFile.readAll(), QByteArray("old-target"));

        QFile commitOldFile(atomicPath);
        QVERIFY(commitOldFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        commitOldFile.write("old-before-commit-failure");
        commitOldFile.close();
        const ExportPageProvider successfulProvider =
            [&](const QString&, const ExportCursor&, int) {
                ExportPage page;
                page.samples = pressure.mid(0, 1);
                return page;
            };
        CommitFailingExportHelper commitFailingHelper;
        const ExportResult commitFailed = commitFailingHelper.exportPackagePaged(
            {infos.first()}, metadata, successfulProvider, atomicPath, pageSize);
        QVERIFY(!commitFailed.success);
        QVERIFY(!commitFailed.errorMessage.isEmpty());
        QFile preservedAfterCommitFailure(atomicPath);
        QVERIFY(preservedAfterCommitFailure.open(QIODevice::ReadOnly));
        QCOMPARE(preservedAfterCommitFailure.readAll(),
                 QByteArray("old-before-commit-failure"));

        const ExportResult normalAfterCommitFailure = helper.exportPackagePaged(
            {infos.first()}, metadata, successfulProvider,
            tempDir.path() + QStringLiteral("/normal_after_commit.csv"), pageSize);
        QVERIFY2(normalAfterCommitFailure.success,
                 qPrintable(normalAfterCommitFailure.errorMessage));
    }

    void testQualityAndInvalidValueExport()
    {
        MonitorExportHelper helper;
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        Monitor::Sample sample;
        sample.channelName = QStringLiteral("pressure");
        sample.unit = QStringLiteral("bar");
        sample.timestamp = QDateTime::currentDateTimeUtc();
        sample.quality = RuntimePointQuality::Bad;
        sample.valueValid = false;

        const QList<Monitor::Sample> samples{sample};

        const QString csvPath = tempDir.path() + QStringLiteral("/quality.csv");
        const ExportResult csvResult = helper.exportDataAsCsvToFile(
            sample.channelName, samples, csvPath);
        QVERIFY2(csvResult.success, qPrintable(csvResult.errorMessage));

        QFile csvFile(csvPath);
        QVERIFY(csvFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QStringList csvLines = QString::fromUtf8(csvFile.readAll())
                                         .split('\n', Qt::SkipEmptyParts);
        csvFile.close();
        QStringList csvDataLines;
        for (const QString& line : csvLines) {
            if (!line.startsWith('#')) {
                csvDataLines.append(line);
            }
        }
        QCOMPARE(csvDataLines.size(), 2);
        QVERIFY(csvDataLines.first().contains(QStringLiteral("quality")));
        QVERIFY(csvDataLines.first().contains(QStringLiteral("value_valid")));
        const QStringList csvFields = csvDataLines.last().split(',');
        QCOMPARE(csvFields.size(), 7);
        QVERIFY(csvFields.at(3).isEmpty());
        QCOMPARE(csvFields.at(5), QStringLiteral("Bad"));
        QCOMPARE(csvFields.at(6), QStringLiteral("0"));

        const QString jsonPath = tempDir.path() + QStringLiteral("/quality.json");
        const ExportResult jsonResult = helper.exportDataAsJsonToFile(
            sample.channelName, samples, jsonPath);
        QVERIFY2(jsonResult.success, qPrintable(jsonResult.errorMessage));

        QFile jsonFile(jsonPath);
        QVERIFY(jsonFile.open(QIODevice::ReadOnly));
        const QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonFile.readAll());
        jsonFile.close();
        const QJsonObject jsonSample = jsonDocument.object()
                                           .value(QStringLiteral("samples"))
                                           .toArray()
                                           .at(0)
                                           .toObject();
        QVERIFY(jsonSample.value(QStringLiteral("value")).isNull());
        QCOMPARE(jsonSample.value(QStringLiteral("quality")).toString(), QStringLiteral("Bad"));
        QVERIFY(!jsonSample.value(QStringLiteral("valueValid")).toBool());

        ExportDataPackage package;
        package.metadata.exportTime = QDateTime::currentDateTimeUtc();
        package.channelInfos.append(
            ExportChannelInfo(sample.channelName, QStringLiteral("压力"), sample.unit, 100));
        package.channelSamples.insert(sample.channelName, samples);

        const QString multiPath = tempDir.path() + QStringLiteral("/quality_multi.tsv");
        const ExportResult multiResult = helper.exportPackageAsTsv(package, multiPath);
        QVERIFY2(multiResult.success, qPrintable(multiResult.errorMessage));

        QFile multiFile(multiPath);
        QVERIFY(multiFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString multiContent = QString::fromUtf8(multiFile.readAll());
        multiFile.close();
        QVERIFY(multiContent.contains(QStringLiteral("pressure_quality")));
        QVERIFY(multiContent.contains(QStringLiteral("pressure_value_valid")));
        QVERIFY(multiContent.contains(QStringLiteral("\t\tBad\t0")));
    }
};

QTEST_MAIN(MonitorExportTest)
#include "monitor_export_test.moc"
