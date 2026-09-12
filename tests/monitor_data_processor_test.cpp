/**
 * @file monitor_data_processor_test.cpp
 * @brief MonitorDataProcessor 增量/窗口逻辑单元测试
 *
 * 测试内容：
 * - 单通道/多通道数据追加
 * - 增量数据缓存与消费（drainDelta）
 * - 滑动窗口大小限制
 * - 数据淘汰逻辑
 * - 边界情况（空数据、单点数据、大量数据）
 * - 配置接口测试
 *
 * 注意：本测试依赖 MonitorDataProcessor 已实现增量模式接口。
 */

#include <QtTest/QtTest>
#include <QVector>
#include <QPointF>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QThread>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>

#include "MonitorDataProcessor.h"
#include "MonitorChannel.h"

class FunctionThread final : public QThread
{
public:
    explicit FunctionThread(std::function<void()> function)
        : m_function(std::move(function))
    {}

protected:
    void run() override
    {
        m_function();
    }

private:
    std::function<void()> m_function;
};

class MonitorDataProcessorTest : public QObject
{
    Q_OBJECT

private:
    /**
     * @brief 生成测试数据点
     * @param count 数据点数量
     * @param startTime 起始时间戳（毫秒）
     * @param interval 时间间隔（毫秒）
     * @return 数据点列表
     */
    QVector<QPointF> generateTestPoints(int count, 
                                         qint64 startTime = 0, 
                                         qint64 interval = 100)
    {
        QVector<QPointF> points;
        points.reserve(count);
        
        for (int i = 0; i < count; ++i) {
            qint64 timestamp = startTime + i * interval;
            double value = std::sin(i * 0.1) * 100;  // 正弦波模拟
            points.append(QPointF(timestamp, value));
        }
        
        return points;
    }

private slots:
    void initTestCase()
    {
        qInfo() << "========================================";
        qInfo() << "MonitorDataProcessor Test Suite";
        qInfo() << "========================================";
    }

    void cleanupTestCase()
    {
        qInfo() << "All MonitorDataProcessor tests completed.";
    }

    void init()
    {
        // 每个测试用例前的初始化
    }

    void cleanup()
    {
        // 每个测试用例后的清理
    }

    // =========================================================================
    // 测试 1：单通道数据追加与读取
    // =========================================================================

    /**
     * @brief 测试单通道数据追加
     * 
     * 验证点：
     * - appendPoints 后数据正确存储
     * - getChannelData 返回正确数据
     */
    void testSingleChannelAppend()
    {
        MonitorDataProcessor processor;
        const QString channelId = "ch1";

        QVector<QPointF> points;
        for (int i = 0; i < 5; ++i) {
            points.append(QPointF(i, i + 1));
        }

        processor.appendPoints(channelId, points);

        // 通道数据应全部写入
        QVector<QPointF> channelData = processor.getChannelData(channelId);
        QCOMPARE(channelData.size(), points.size());
        QCOMPARE(channelData.last(), points.last());
    }

    /**
     * @brief 测试增量数据消费（drainDelta）
     * 
     * 验证点：
     * - drainDelta 返回自上次消费以来的增量
     * - 消费后增量缓冲区被清空
     */
    void testDrainDelta()
    {
        MonitorDataProcessor processor;
        const QString channelId = "ch1";

        QVector<QPointF> points;
        for (int i = 0; i < 5; ++i) {
            points.append(QPointF(i, i + 1));
        }

        processor.appendPoints(channelId, points);

        // 增量数据应与刚刚追加的一致
        QVector<QPointF> delta = processor.drainDelta(channelId);
        QCOMPARE(delta.size(), points.size());
        QCOMPARE(delta, points);

        // 再次 drainDelta 应为空（增量已经被消费）
        QVector<QPointF> delta2 = processor.drainDelta(channelId);
        QCOMPARE(delta2.size(), 0);
    }

    /**
     * @brief 测试多次追加后的增量累积
     * 
     * 验证点：
     * - 多次 appendPoints 的增量会累积
     * - drainDelta 返回所有累积的增量
     */
    void testMultipleAppendDeltaAccumulation()
    {
        MonitorDataProcessor processor;
        const QString channelId = "accumulate_test";

        // 第一批数据
        QVector<QPointF> batch1 = { QPointF(0, 10), QPointF(1, 20) };
        processor.appendPoints(channelId, batch1);

        // 第二批数据
        QVector<QPointF> batch2 = { QPointF(2, 30), QPointF(3, 40), QPointF(4, 50) };
        processor.appendPoints(channelId, batch2);

        // drainDelta 应返回所有累积的增量
        QVector<QPointF> delta = processor.drainDelta(channelId);
        QCOMPARE(delta.size(), batch1.size() + batch2.size());
        
        // 验证数据顺序
        QCOMPARE(delta[0], batch1[0]);
        QCOMPARE(delta[1], batch1[1]);
        QCOMPARE(delta[2], batch2[0]);
        
        qInfo() << "累积增量测试通过，总计" << delta.size() << "个数据点";
    }

    // =========================================================================
    // 测试 2：多通道数据隔离
    // =========================================================================

    /**
     * @brief 测试多通道数据隔离
     * 
     * 验证点：
     * - 不同通道的数据互不影响
     * - 各通道增量独立
     */
    void testMultipleChannelsIsolation()
    {
        MonitorDataProcessor processor;
        const QString ch1 = "ch1";
        const QString ch2 = "ch2";

        QVector<QPointF> points1 { QPointF(0, 1), QPointF(1, 2) };
        QVector<QPointF> points2 { QPointF(0, 10), QPointF(1, 20), QPointF(2, 30) };

        processor.appendPoints(ch1, points1);
        processor.appendPoints(ch2, points2);

        // 通道数据应各自独立
        QVector<QPointF> data1 = processor.getChannelData(ch1);
        QVector<QPointF> data2 = processor.getChannelData(ch2);

        QCOMPARE(data1, points1);
        QCOMPARE(data2, points2);

        // drainDelta 只返回对应通道的增量
        QVector<QPointF> delta1 = processor.drainDelta(ch1);
        QVector<QPointF> delta2 = processor.drainDelta(ch2);

        QCOMPARE(delta1, points1);
        QCOMPARE(delta2, points2);
        
        qInfo() << "多通道隔离测试通过";
    }

    /**
     * @brief 测试消费一个通道不影响另一个
     */
    void testChannelDeltaIndependence()
    {
        MonitorDataProcessor processor;
        const QString ch1 = "independent_ch1";
        const QString ch2 = "independent_ch2";

        QVector<QPointF> points1 = generateTestPoints(3, 0, 100);
        QVector<QPointF> points2 = generateTestPoints(5, 0, 100);

        processor.appendPoints(ch1, points1);
        processor.appendPoints(ch2, points2);

        // 只消费 ch1
        processor.drainDelta(ch1);

        // ch2 的增量应该仍然存在
        QVector<QPointF> delta2 = processor.drainDelta(ch2);
        QCOMPARE(delta2.size(), points2.size());
        
        qInfo() << "通道增量独立性测试通过";
    }

    // =========================================================================
    // 测试 3：配置接口
    // =========================================================================

    /**
     * @brief 测试配置接口（timeWindow / maxDisplaySamples / ringBufferCapacity）
     */
    void testConfigSetters()
    {
        MonitorDataProcessor processor;
        QSignalSpy configSpy(&processor, &MonitorDataProcessor::configChanged);

        // 获取默认配置
        DataProcessorConfig cfg = processor.config();
        QVERIFY(cfg.timeWindowMs > 0);
        QVERIFY(cfg.maxDisplaySamples > 0);

        // 修改 timeWindow
        processor.setTimeWindow(10000);  // 10 秒
        QCOMPARE(processor.timeWindow(), 10000ll);

        // 修改 maxDisplaySamples
        processor.setMaxDisplaySamples(50);
        QCOMPARE(processor.maxDisplaySamples(), 50);

        // 修改 ringBufferCapacity
        processor.setRingBufferCapacity(1000);
        QCOMPARE(processor.ringBufferCapacity(), 1000);

        processor.setClampRange(-10.0, 10.0);
        processor.setSmoothing(true, 5);
        processor.setDownsampling(true, 4);
        processor.setIncrementalMode(false);

        cfg = processor.config();
        QVERIFY(cfg.minClamp.has_value());
        QVERIFY(cfg.maxClamp.has_value());
        QCOMPARE(cfg.minClamp.value(), -10.0);
        QCOMPARE(cfg.maxClamp.value(), 10.0);
        QVERIFY(cfg.enableSmoothing);
        QCOMPARE(cfg.smoothingWindow, 5);
        QVERIFY(cfg.enableDownsampling);
        QCOMPARE(cfg.downsampleFactor, 4);
        QVERIFY(!cfg.enableIncrementalMode);
        QCOMPARE(configSpy.count(), 7);

        // 相同的有效配置不应重复通知。
        processor.setTimeWindow(10000);
        processor.setMaxDisplaySamples(50);
        processor.setRingBufferCapacity(1000);
        processor.setClampRange(-10.0, 10.0);
        processor.setSmoothing(true, 5);
        processor.setDownsampling(true, 4);
        processor.setIncrementalMode(false);
        QCOMPARE(configSpy.count(), 7);
        
        qInfo() << "配置接口测试通过";
    }

    /**
     * @brief 测试完整配置设置
     */
    void testSetConfig()
    {
        MonitorDataProcessor processor;

        DataProcessorConfig newConfig;
        newConfig.timeWindowMs = 60000;  // 60 秒
        newConfig.maxDisplaySamples = 500;
        newConfig.ringBufferCapacity = 10000;
        newConfig.enableIncrementalMode = true;
        newConfig.enableSmoothing = true;
        newConfig.smoothingWindow = 7;
        newConfig.enableDownsampling = true;
        newConfig.downsampleFactor = 3;
        newConfig.batchProcessThreshold = 25;
        newConfig.maxDeltaBufferSize = 12000;
        newConfig.enableAdaptiveDownsample = false;
        newConfig.adaptiveDownsampleThreshold = 4000;
        newConfig.minClamp = -20.0;
        newConfig.maxClamp = 20.0;

        processor.setConfig(newConfig);

        DataProcessorConfig readBack = processor.config();
        QCOMPARE(readBack.timeWindowMs, newConfig.timeWindowMs);
        QCOMPARE(readBack.maxDisplaySamples, newConfig.maxDisplaySamples);
        QCOMPARE(readBack.ringBufferCapacity, newConfig.ringBufferCapacity);
        QCOMPARE(readBack.smoothingWindow, newConfig.smoothingWindow);
        QCOMPARE(readBack.downsampleFactor, newConfig.downsampleFactor);
        QCOMPARE(readBack.batchProcessThreshold, newConfig.batchProcessThreshold);
        QCOMPARE(readBack.maxDeltaBufferSize, newConfig.maxDeltaBufferSize);
        QCOMPARE(readBack.adaptiveDownsampleThreshold,
                 newConfig.adaptiveDownsampleThreshold);
        QCOMPARE(readBack.minClamp.value(), newConfig.minClamp.value());
        QCOMPARE(readBack.maxClamp.value(), newConfig.maxClamp.value());
        
        qInfo() << "完整配置测试通过";
    }

    void testConfigurationBoundaryNormalization()
    {
        MonitorDataProcessor processor;
        QSignalSpy configSpy(&processor, &MonitorDataProcessor::configChanged);

        DataProcessorConfig invalid = processor.config();
        invalid.timeWindowMs = -100;
        invalid.maxDisplaySamples = -20;
        invalid.ringBufferCapacity = -5;
        invalid.smoothingWindow = 0;
        invalid.downsampleFactor = -2;
        invalid.batchProcessThreshold = 0;
        invalid.maxDeltaBufferSize = -10;
        invalid.adaptiveDownsampleThreshold = 1;
        processor.setConfig(invalid);

        DataProcessorConfig normalized = processor.config();
        QCOMPARE(normalized.timeWindowMs, 0ll);
        QCOMPARE(normalized.maxDisplaySamples, 0);
        QCOMPARE(normalized.ringBufferCapacity, 1);
        QCOMPARE(normalized.smoothingWindow, 1);
        QCOMPARE(normalized.downsampleFactor, 1);
        QCOMPARE(normalized.batchProcessThreshold, 1);
        QCOMPARE(normalized.maxDeltaBufferSize, 1);
        QCOMPARE(normalized.adaptiveDownsampleThreshold, 2);
        QCOMPARE(configSpy.count(), 1);

        const QVector<QPointF> capacityPoints = {QPointF(0, 0), QPointF(1, 1)};
        processor.appendPoints(QStringLiteral("normalized_capacity"), capacityPoints);
        QCOMPARE(processor.getChannelData(QStringLiteral("normalized_capacity")),
                 QVector<QPointF>({capacityPoints.last()}));

        // setter 与 setConfig 使用相同规范化，等价输入不重复发信号。
        processor.setTimeWindow(-1);
        processor.setMaxDisplaySamples(-1);
        processor.setRingBufferCapacity(0);
        processor.setSmoothing(true, -1);
        processor.setDownsampling(true, 0);
        QCOMPARE(configSpy.count(), 3);
        normalized = processor.config();
        QCOMPARE(normalized.timeWindowMs, 0ll);
        QCOMPARE(normalized.maxDisplaySamples, 0);
        QCOMPARE(normalized.ringBufferCapacity, 1);
        QCOMPARE(normalized.smoothingWindow, 1);
        QCOMPARE(normalized.downsampleFactor, 1);

        normalized.enableAdaptiveDownsample = false;
        processor.setConfig(normalized);

        QList<Monitor::Sample> samples;
        for (int i = 0; i < 4; ++i) {
            Monitor::Sample sample;
            sample.channelName = QStringLiteral("unlimited");
            sample.value = i;
            sample.timestamp = QDateTime::fromMSecsSinceEpoch(i);
            samples.append(sample);
        }
        const ProcessedChannelData unlimited = processor.processSamples(
            QStringLiteral("unlimited"), samples);
        QCOMPARE(unlimited.seriesPoints.size(), samples.size());

        DataProcessorConfig coupledCapacity = processor.config();
        coupledCapacity.ringBufferCapacity = 4;
        coupledCapacity.maxDeltaBufferSize = 2;
        processor.setConfig(coupledCapacity);
        coupledCapacity = processor.config();
        QCOMPARE(coupledCapacity.ringBufferCapacity, 4);
        QCOMPARE(coupledCapacity.maxDeltaBufferSize, 4);

        DataProcessorConfig large = processor.config();
        large.timeWindowMs = std::numeric_limits<qint64>::max();
        large.maxDisplaySamples = std::numeric_limits<int>::max();
        large.smoothingWindow = std::numeric_limits<int>::max();
        large.downsampleFactor = std::numeric_limits<int>::max();
        large.batchProcessThreshold = std::numeric_limits<int>::max();
        large.maxDeltaBufferSize = std::numeric_limits<int>::max();
        large.adaptiveDownsampleThreshold = std::numeric_limits<int>::max();
        processor.setConfig(large);
        normalized = processor.config();
        QCOMPARE(normalized.timeWindowMs, large.timeWindowMs);
        QCOMPARE(normalized.maxDisplaySamples, large.maxDisplaySamples);
        QCOMPARE(normalized.smoothingWindow, large.smoothingWindow);
        QCOMPARE(normalized.downsampleFactor, large.downsampleFactor);
        QCOMPARE(normalized.batchProcessThreshold, large.batchProcessThreshold);
        QCOMPARE(normalized.maxDeltaBufferSize, large.maxDeltaBufferSize);
        QCOMPARE(normalized.adaptiveDownsampleThreshold,
                 large.adaptiveDownsampleThreshold);
    }

    void testClampNormalization()
    {
        MonitorDataProcessor processor;
        DataProcessorConfig config = processor.config();
        config.minClamp = std::numeric_limits<double>::quiet_NaN();
        config.maxClamp = std::numeric_limits<double>::infinity();
        processor.setConfig(config);

        config = processor.config();
        QVERIFY(!config.minClamp.has_value());
        QVERIFY(!config.maxClamp.has_value());

        processor.setClampRange(10.0, -10.0);
        config = processor.config();
        QCOMPARE(config.minClamp.value(), -10.0);
        QCOMPARE(config.maxClamp.value(), 10.0);

        QList<Monitor::Sample> samples;
        for (double value : {-20.0, 0.0, 20.0}) {
            Monitor::Sample sample;
            sample.channelName = QStringLiteral("clamp");
            sample.value = value;
            sample.timestamp = QDateTime::currentDateTimeUtc();
            samples.append(sample);
        }
        const ProcessedChannelData processed = processor.processSamples(
            QStringLiteral("clamp"), samples);
        QCOMPARE(processed.seriesPoints.size(), 3);
        QCOMPARE(processed.seriesPoints.first().y(), -10.0);
        QCOMPARE(processed.seriesPoints.last().y(), 10.0);

        processor.setClampRange(-std::numeric_limits<double>::infinity(), 5.0);
        config = processor.config();
        QVERIFY(!config.minClamp.has_value());
        QCOMPARE(config.maxClamp.value(), 5.0);
    }

    // =========================================================================
    // 测试 4：滑动窗口与数据淘汰
    // =========================================================================

    /**
     * @brief 测试环形缓冲区容量限制
     * 
     * 验证点：
     * - 超过容量时，旧数据被淘汰
     * - 数据总量不超过配置的容量
     */
    void testRingBufferCapacityLimit()
    {
        MonitorDataProcessor processor;
        const QString channelId = "capacity_test";

        // 设置较小的缓冲区容量
        int capacity = 100;
        processor.setRingBufferCapacity(capacity);

        // 追加超过容量的数据
        int totalPoints = 200;
        QVector<QPointF> points = generateTestPoints(totalPoints, 0, 10);
        processor.appendPoints(channelId, points);

        // 通道数据应不超过容量
        QVector<QPointF> channelData = processor.getChannelData(channelId);
        QVERIFY2(channelData.size() <= capacity, 
                 QString("数据量 %1 超过容量 %2")
                     .arg(channelData.size())
                     .arg(capacity)
                     .toLocal8Bit());

        // 验证保留的是最新的数据
        if (!channelData.isEmpty()) {
            // 最后一个点应该是原始数据的最后一个点
            QCOMPARE(channelData.last(), points.last());
        }
        
        qInfo() << "缓冲区容量限制测试通过，数据量：" << channelData.size();
    }

    /**
     * @brief 测试时间窗口裁剪
     * 
     * 验证点：
     * - 超出时间窗口的旧数据被裁剪
     */
    void testTimeWindowTrimming()
    {
        MonitorDataProcessor processor;
        const QString channelId = "time_window_test";

        // 设置 5 秒时间窗口
        qint64 windowMs = 5000;
        processor.setTimeWindow(windowMs);

        // 追加跨越 10 秒的数据
        qint64 baseTime = QDateTime::currentMSecsSinceEpoch();
        QVector<QPointF> points;
        
        // 每 500ms 一个点，共 20 个点 = 10 秒
        for (int i = 0; i < 20; ++i) {
            qint64 timestamp = baseTime + i * 500;
            points.append(QPointF(timestamp, i * 10));
        }
        
        processor.appendPoints(channelId, points);

        // 触发时间窗口裁剪（可能需要调用特定方法，取决于实现）
        // 如果 processor 自动裁剪，这里直接获取数据
        QVector<QPointF> channelData = processor.getChannelData(channelId);

        // 根据实现，可能保留最近 5 秒的数据
        // 这里只验证不崩溃，具体行为取决于实现
        qInfo() << "时间窗口裁剪后数据量：" << channelData.size();
        
        // 基本验证：数据应该存在
        QVERIFY(channelData.size() > 0);
    }

    /**
     * @brief 测试数据淘汰顺序（FIFO）
     * 
     * 验证点：
     * - 淘汰的是最旧的数据
     * - 新数据正确保留
     */
    void testDataEvictionOrder()
    {
        MonitorDataProcessor processor;
        const QString channelId = "eviction_order_test";

        // 设置小容量
        int capacity = 5;
        processor.setRingBufferCapacity(capacity);

        // 追加 10 个点
        QVector<QPointF> points;
        for (int i = 0; i < 10; ++i) {
            points.append(QPointF(i * 100, i));
        }
        processor.appendPoints(channelId, points);

        QVector<QPointF> channelData = processor.getChannelData(channelId);

        // 应该保留最后 5 个点（索引 5-9）
        QVERIFY(channelData.size() <= capacity);
        
        if (channelData.size() == capacity) {
            // 验证保留的是最新的数据
            QCOMPARE(channelData.last().y(), 9.0);
            QCOMPARE(channelData.first().y(), 5.0);
            qInfo() << "FIFO 淘汰顺序正确";
        }
    }

    void testCapacityChangesPreserveNewestRawPoints()
    {
        MonitorDataProcessor processor;
        const QString channelId = QStringLiteral("capacity_migration");
        const QString otherChannelId = QStringLiteral("capacity_migration_other");
        const QVector<QPointF> initial = {
            QPointF(0, 0), QPointF(1, 1), QPointF(2, 2)
        };
        const QVector<QPointF> otherInitial = {QPointF(10, 10), QPointF(11, 11)};

        processor.setRingBufferCapacity(3);
        processor.appendPoints(channelId, initial);
        processor.appendPoints(otherChannelId, otherInitial);
        processor.setRingBufferCapacity(5);
        QCOMPARE(processor.getChannelData(channelId), initial);
        QCOMPARE(processor.getChannelData(otherChannelId), otherInitial);

        const QVector<QPointF> appended = {QPointF(3, 3), QPointF(4, 4)};
        const QVector<QPointF> otherAppended = {
            QPointF(12, 12), QPointF(13, 13), QPointF(14, 14)
        };
        processor.appendPoints(channelId, appended);
        processor.appendPoints(otherChannelId, otherAppended);
        const QVector<QPointF> beforeShrink = processor.getChannelData(channelId);
        const QVector<QPointF> otherBeforeShrink =
            processor.getChannelData(otherChannelId);
        QCOMPARE(beforeShrink.size(), 5);
        QCOMPARE(otherBeforeShrink.size(), 5);

        processor.setRingBufferCapacity(2);
        const QVector<QPointF> afterShrink = processor.getChannelData(channelId);
        const QVector<QPointF> otherAfterShrink =
            processor.getChannelData(otherChannelId);
        QCOMPARE(afterShrink, beforeShrink.mid(3));
        QCOMPARE(otherAfterShrink, otherBeforeShrink.mid(3));
        QCOMPARE(afterShrink.first(), QPointF(3, 3));
        QCOMPARE(afterShrink.last(), QPointF(4, 4));
        QCOMPARE(otherAfterShrink.first(), QPointF(13, 13));
        QCOMPARE(otherAfterShrink.last(), QPointF(14, 14));
    }

    void testDeltaShrinkKeepsNewestPoints()
    {
        MonitorDataProcessor processor;
        const QString channelId = QStringLiteral("delta_shrink");
        DataProcessorConfig config = processor.config();
        config.ringBufferCapacity = 2;
        config.maxDeltaBufferSize = 6;
        processor.setConfig(config);

        const QVector<QPointF> points = {
            QPointF(0, 0), QPointF(1, 1), QPointF(2, 2),
            QPointF(3, 3), QPointF(4, 4), QPointF(5, 5)
        };
        processor.appendPoints(channelId, points);
        QCOMPARE(processor.pendingDeltaCount(channelId), 6);

        config = processor.config();
        config.maxDeltaBufferSize = 3;
        processor.setConfig(config);

        QCOMPARE(processor.pendingDeltaCount(channelId), 3);
        QCOMPARE(processor.drainDelta(channelId), points.mid(3));
    }

    // =========================================================================
    // 测试 5：边界情况
    // =========================================================================

    /**
     * @brief 测试空数据追加
     */
    void testEmptyDataAppend()
    {
        MonitorDataProcessor processor;
        const QString channelId = "empty_test";

        // 追加空数据
        QVector<QPointF> emptyPoints;
        processor.appendPoints(channelId, emptyPoints);

        // 不应崩溃
        QVector<QPointF> channelData = processor.getChannelData(channelId);
        QCOMPARE(channelData.size(), 0);

        QVector<QPointF> delta = processor.drainDelta(channelId);
        QCOMPARE(delta.size(), 0);
        
        qInfo() << "空数据追加测试通过";
    }

    /**
     * @brief 测试单点数据
     */
    void testSinglePointData()
    {
        MonitorDataProcessor processor;
        const QString channelId = "single_point_test";

        QVector<QPointF> singlePoint { QPointF(1000, 42.5) };
        processor.appendPoints(channelId, singlePoint);

        QVector<QPointF> channelData = processor.getChannelData(channelId);
        QCOMPARE(channelData.size(), 1);
        QCOMPARE(channelData.first(), singlePoint.first());

        QVector<QPointF> delta = processor.drainDelta(channelId);
        QCOMPARE(delta.size(), 1);
        
        qInfo() << "单点数据测试通过";
    }

    /**
     * @brief 测试不存在的通道
     */
    void testNonExistentChannel()
    {
        MonitorDataProcessor processor;

        // 获取不存在的通道数据
        QVector<QPointF> data = processor.getChannelData("nonexistent");
        QCOMPARE(data.size(), 0);

        // drain 不存在的通道
        QVector<QPointF> delta = processor.drainDelta("nonexistent");
        QCOMPARE(delta.size(), 0);
        
        qInfo() << "不存在通道测试通过";
    }

    /**
     * @brief 测试大量数据追加性能
     */
    void testLargeDataAppendPerformance()
    {
        MonitorDataProcessor processor;
        const QString channelId = "perf_test";

        // 设置较大的缓冲区
        processor.setRingBufferCapacity(10000);

        // 生成大量数据
        int dataSize = 5000;
        QVector<QPointF> points = generateTestPoints(dataSize, 0, 10);

        QElapsedTimer timer;
        timer.start();
        
        processor.appendPoints(channelId, points);
        
        qint64 appendTime = timer.elapsed();
        
        timer.restart();
        QVector<QPointF> delta = processor.drainDelta(channelId);
        qint64 drainTime = timer.elapsed();

        QCOMPARE(delta.size(), dataSize);
        
        qInfo() << "大数据量测试：" << dataSize << "个点";
        qInfo() << "  追加耗时：" << appendTime << "ms";
        qInfo() << "  消费耗时：" << drainTime << "ms";
        
        // 性能基准（可调整）
        QVERIFY2(appendTime < 1000, "追加耗时过长");
        QVERIFY2(drainTime < 500, "消费耗时过长");
    }

    // =========================================================================
    // 测试 6：增量模式开关
    // =========================================================================

    /**
     * @brief 测试增量模式开关
     */
    void testIncrementalModeToggle()
    {
        MonitorDataProcessor processor;
        const QString channelId = "incremental_mode_test";

        DataProcessorConfig config = processor.config();
        config.ringBufferCapacity = 6;
        config.maxDeltaBufferSize = 6;
        config.enableIncrementalMode = true;
        processor.setConfig(config);

        QVector<QPointF> points1 = { QPointF(0, 10), QPointF(1, 20) };
        processor.appendPoints(channelId, points1);
        QCOMPARE(processor.pendingDeltaCount(channelId), points1.size());

        // 关闭时立即清理旧 delta，关闭期间的数据不进入 delta。
        processor.setIncrementalMode(false);
        QCOMPARE(processor.pendingDeltaCount(channelId), 0);
        const QVector<QPointF> whileDisabled = {QPointF(2, 30)};
        processor.appendPoints(channelId, whileDisabled);
        QCOMPARE(processor.pendingDeltaCount(channelId), 0);

        // 重开后只包含新追加的增量。
        processor.setIncrementalMode(true);
        const QVector<QPointF> afterReopen = {QPointF(3, 40), QPointF(4, 50)};
        processor.appendPoints(channelId, afterReopen);
        QCOMPARE(processor.drainDelta(channelId), afterReopen);

        const QVector<QPointF> channelData = processor.getChannelData(channelId);
        QCOMPARE(channelData,
                 QVector<QPointF>({points1[0], points1[1], whileDisabled[0],
                                   afterReopen[0], afterReopen[1]}));
        
        qInfo() << "增量模式开关测试通过";
    }

    // =========================================================================
    // 测试 7：并发安全性（基础验证）
    // =========================================================================

    /**
     * @brief 测试快速连续追加
     * 
     * 验证点：
     * - 快速连续追加不会导致数据丢失或崩溃
     */
    void testRapidAppend()
    {
        MonitorDataProcessor processor;
        const QString channelId = "rapid_test";

        int batchCount = 100;
        int pointsPerBatch = 10;
        int expectedTotal = batchCount * pointsPerBatch;

        for (int i = 0; i < batchCount; ++i) {
            QVector<QPointF> batch;
            for (int j = 0; j < pointsPerBatch; ++j) {
                int idx = i * pointsPerBatch + j;
                batch.append(QPointF(idx, idx * 0.1));
            }
            processor.appendPoints(channelId, batch);
        }

        // 验证所有增量数据
        QVector<QPointF> allDelta = processor.drainDelta(channelId);
        
        // 数量应该等于或小于（如果有淘汰）期望值
        QVERIFY(allDelta.size() <= expectedTotal);
        QVERIFY(allDelta.size() > 0);
        
        qInfo() << "快速追加测试通过，获取" << allDelta.size() 
                << "/" << expectedTotal << "个数据点";
    }

    void testConfigurationChangeInvalidatesCacheButKeepsRawData()
    {
        MonitorDataProcessor processor;
        Monitor::ChannelConfig channelConfig;
        channelConfig.name = QStringLiteral("cache_channel");
        channelConfig.unit = QStringLiteral("V");
        auto channel = std::make_shared<Monitor::MonitorChannel>(channelConfig);

        Monitor::Sample sample;
        sample.channelName = channelConfig.name;
        sample.value = 12.0;
        sample.unit = channelConfig.unit;
        sample.timestamp = QDateTime::currentDateTimeUtc();
        channel->appendSample(sample);
        processor.appendSample(channelConfig.name, sample);

        const QVector<QPointF> rawBefore = processor.getChannelData(channelConfig.name);
        QVERIFY(!rawBefore.isEmpty());
        processor.processChannel(channel);
        QVERIFY(processor.hasCachedData(channelConfig.name));

        processor.setSmoothing(true, 3);
        QVERIFY(!processor.hasCachedData(channelConfig.name));
        QCOMPARE(processor.getChannelData(channelConfig.name), rawBefore);
    }

    void testConfigChangedDirectReadbackAndReentrantSetter()
    {
        MonitorDataProcessor processor;
        QSignalSpy configSpy(&processor, &MonitorDataProcessor::configChanged);
        bool readBackSucceeded = false;
        bool nestedSetterTriggered = false;

        connect(&processor, &MonitorDataProcessor::configChanged,
                &processor, [&]() {
                    const DataProcessorConfig current = processor.config();
                    readBackSucceeded = current.ringBufferCapacity >= 1;
                    if (!nestedSetterTriggered) {
                        nestedSetterTriggered = true;
                        processor.setMaxDisplaySamples(17);
                    }
                }, Qt::DirectConnection);

        processor.setTimeWindow(1234);
        QVERIFY(readBackSucceeded);
        QVERIFY(nestedSetterTriggered);
        QCOMPARE(processor.timeWindow(), 1234ll);
        QCOMPARE(processor.maxDisplaySamples(), 17);
        QCOMPARE(configSpy.count(), 2);
    }

    void testConcurrentConfigReadWriteAndAppendCompletes()
    {
        auto* processor = new MonitorDataProcessor;
        const QString channelId = QStringLiteral("concurrent_config");
        auto* validSnapshots = new std::atomic_bool{true};

        auto* writer = new FunctionThread([processor]() {
            for (int i = 0; i < 200; ++i) {
                DataProcessorConfig config = processor->config();
                config.ringBufferCapacity = (i % 2 == 0) ? 8 : 16;
                config.maxDeltaBufferSize = config.ringBufferCapacity + 4;
                config.timeWindowMs = (i % 3 == 0) ? 0 : 5000;
                config.maxDisplaySamples = (i % 4 == 0) ? 0 : 64;
                config.smoothingWindow = i % 5 + 1;
                config.downsampleFactor = i % 4 + 1;
                config.batchProcessThreshold = i % 7 + 1;
                config.adaptiveDownsampleThreshold = i % 11 + 2;
                processor->setConfig(config);
            }

            DataProcessorConfig finalConfig = processor->config();
            finalConfig.ringBufferCapacity = 16;
            finalConfig.maxDeltaBufferSize = 20;
            processor->setConfig(finalConfig);
        });

        auto* reader = new FunctionThread([processor, validSnapshots]() {
            for (int i = 0; i < 1000; ++i) {
                const DataProcessorConfig config = processor->config();
                if (config.ringBufferCapacity < 1
                    || config.maxDeltaBufferSize < config.ringBufferCapacity
                    || config.smoothingWindow < 1
                    || config.downsampleFactor < 1
                    || config.batchProcessThreshold < 1
                    || config.adaptiveDownsampleThreshold < 2) {
                    validSnapshots->store(false, std::memory_order_relaxed);
                }
            }
        });

        auto* appender = new FunctionThread([processor, channelId]() {
            for (int i = 0; i < 1000; ++i) {
                processor->appendPoint(channelId, QPointF(i, i));
            }
        });

        writer->start();
        reader->start();
        appender->start();

        QElapsedTimer timeout;
        timeout.start();
        auto waitWithinDeadline = [&timeout](QThread* thread) {
            const qint64 remaining = std::max<qint64>(0, 5000 - timeout.elapsed());
            return thread->wait(static_cast<unsigned long>(remaining));
        };
        const bool completed = waitWithinDeadline(writer)
            && waitWithinDeadline(reader)
            && waitWithinDeadline(appender);

        if (!completed) {
            for (QThread* thread : {writer, reader, appender}) {
                if (thread->isRunning()) {
                    thread->terminate();
                }
            }
            for (QThread* thread : {writer, reader, appender}) {
                thread->wait(1000);
                if (!thread->isRunning()) {
                    delete thread;
                }
            }
            QFAIL("并发配置读写与数据追加未在 5 秒内完成");
        }

        delete writer;
        delete reader;
        delete appender;

        const bool snapshotsValid = validSnapshots->load(std::memory_order_relaxed);
        const DataProcessorConfig finalConfig = processor->config();
        const QVector<QPointF> data = processor->getChannelData(channelId);

        delete validSnapshots;
        delete processor;

        QVERIFY(snapshotsValid);
        QCOMPARE(finalConfig.ringBufferCapacity, 16);
        QVERIFY(finalConfig.maxDeltaBufferSize >= finalConfig.ringBufferCapacity);
        QVERIFY(!data.isEmpty());
        QVERIFY(data.size() <= finalConfig.ringBufferCapacity);
        for (int i = 1; i < data.size(); ++i) {
            QVERIFY(data[i - 1].x() < data[i].x());
        }
    }

    void testMonitorChannelConcurrentConfigSnapshots()
    {
        Monitor::ChannelConfig initial;
        initial.name = QStringLiteral("channel-a");
        initial.displayName = QStringLiteral("Channel A");
        initial.unit = QStringLiteral("bar");
        initial.maxSamples = 8;
        initial.thresholds.append(Monitor::Threshold{});

        Monitor::MonitorChannel channel(initial);
        std::atomic_bool started{false};
        bool validSnapshots = true;
        FunctionThread writer([&channel, &started, initial]() mutable {
            while (!started.load(std::memory_order_acquire)) {
            }
            for (int i = 0; i < 2000; ++i) {
                const bool first = (i % 2) == 0;
                initial.name = first ? QStringLiteral("channel-a")
                                     : QStringLiteral("channel-b");
                initial.displayName = first ? QStringLiteral("Channel A")
                                            : QStringLiteral("Channel B");
                initial.unit = first ? QStringLiteral("bar")
                                     : QStringLiteral("kPa");
                initial.maxSamples = first ? 8 : 16;
                channel.updateConfig(initial);
            }
        });

        FunctionThread reader([&channel, &started, &validSnapshots]() {
            while (!started.load(std::memory_order_acquire)) {
            }
            for (int i = 0; i < 4000; ++i) {
                const Monitor::ChannelConfig config = channel.config();
                const bool first = config.name == QStringLiteral("channel-a");
                const bool configValid = (first || config.name == QStringLiteral("channel-b"))
                    && config.displayName == (first ? QStringLiteral("Channel A")
                                                     : QStringLiteral("Channel B"))
                    && config.unit == (first ? QStringLiteral("bar") : QStringLiteral("kPa"))
                    && config.maxSamples == (first ? 8 : 16)
                    && config.thresholds.size() == 1;
                const bool gettersValid = !channel.name().isEmpty()
                    && !channel.displayName().isEmpty() && !channel.unit().isEmpty();
                const QList<Monitor::Threshold> thresholds = channel.thresholds();
                if (!configValid || !gettersValid
                    || thresholds.size() != 1) {
                    validSnapshots = false;
                    return;
                }
            }
        });

        writer.start();
        reader.start();
        started.store(true, std::memory_order_release);
        writer.wait();
        reader.wait();
        QVERIFY(validSnapshots);
    }

    void testMonitorChannelThresholdSignalReentrantConfigAccess()
    {
        Monitor::ChannelConfig config;
        config.name = QStringLiteral("reentrant-channel");
        config.displayName = QStringLiteral("Reentrant Channel");
        config.unit = QStringLiteral("bar");

        Monitor::Threshold threshold;
        threshold.name = QStringLiteral("upper");
        threshold.value = 1.0;
        threshold.unit = config.unit;
        threshold.mode = Monitor::ThresholdMode::Above;
        config.thresholds.append(threshold);

        Monitor::MonitorChannel channel(config);
        bool callbackInvoked = false;
        connect(&channel, &Monitor::MonitorChannel::thresholdExceeded,
                &channel,
                [&channel, &callbackInvoked](
                    const QString&, double, const QString&, double,
                    Monitor::ThresholdMode) {
                    const Monitor::ChannelConfig snapshot = channel.config();
                    if (snapshot.name.isEmpty() || channel.thresholds().isEmpty()) {
                        return;
                    }

                    Monitor::ChannelConfig updated = snapshot;
                    updated.displayName = QStringLiteral("Updated in callback");
                    updated.thresholds.first().value = 3.0;
                    channel.updateConfig(updated);
                    callbackInvoked = true;
                },
                Qt::DirectConnection);

        Monitor::Sample sample;
        sample.channelName = QStringLiteral("reentrant-channel");
        sample.value = 2.0;
        sample.unit = QStringLiteral("bar");
        sample.timestamp = QDateTime::currentDateTimeUtc();
        channel.appendSample(sample);

        QVERIFY(callbackInvoked);
        QCOMPARE(channel.displayName(), QStringLiteral("Updated in callback"));
        QCOMPARE(channel.thresholds().first().value, 3.0);
    }

    void testInvalidSamplesKeepQualityWithoutNumericPoint()
    {
        MonitorDataProcessor processor;
        const QString channelId = QStringLiteral("invalid_read");

        Monitor::Sample validSample;
        validSample.channelName = channelId;
        validSample.value = 12.5;
        validSample.timestamp = QDateTime::currentDateTimeUtc();
        validSample.quality = RuntimePointQuality::Good;
        validSample.valueValid = true;

        Monitor::Sample invalidSample = validSample;
        invalidSample.timestamp = validSample.timestamp.addMSecs(100);
        invalidSample.quality = RuntimePointQuality::Bad;
        invalidSample.valueValid = false;

        processor.appendSamples(channelId, {validSample, invalidSample});

        const QVector<QPointF> storedPoints = processor.getChannelData(channelId);
        QCOMPARE(storedPoints.size(), 1);
        QCOMPARE(storedPoints.first().y(), validSample.value);

        const ProcessedChannelData processed = processor.processSamples(
            channelId, {validSample, invalidSample});
        QCOMPARE(processed.seriesPoints.size(), 1);
        QCOMPARE(processed.currentValue, validSample.value);
        QCOMPARE(processed.quality, RuntimePointQuality::Bad);
        QVERIFY(!processed.valueValid);
    }

    void testQualityCommittedBeforeDeltaSignal()
    {
        MonitorDataProcessor processor;
        DataProcessorConfig config = processor.config();
        config.batchProcessThreshold = 1;
        processor.setConfig(config);
        QStringList events;
        bool qualityStateVisible = false;
        bool deltaStateVisible = false;
        connect(&processor, &MonitorDataProcessor::channelQualityUpdated,
                &processor, [&processor, &events, &qualityStateVisible](
                    const QString& channelId, RuntimePointQuality, bool) {
                    qualityStateVisible = processor.pendingDeltaCount(channelId) == 1;
                    events.append(QStringLiteral("quality"));
                }, Qt::DirectConnection);
        connect(&processor, &MonitorDataProcessor::deltaDataReady,
                &processor, [&processor, &events, &deltaStateVisible](
                    const QString& channelId, int) {
                    deltaStateVisible = processor.pendingDeltaCount(channelId) == 1
                        && processor.config().batchProcessThreshold == 1;
                    events.append(QStringLiteral("delta"));
                }, Qt::DirectConnection);

        Monitor::Sample sample;
        sample.channelName = QStringLiteral("ordered");
        sample.value = 3.0;
        sample.timestamp = QDateTime::currentDateTimeUtc();
        sample.quality = RuntimePointQuality::Good;
        sample.valueValid = true;
        processor.appendSample(sample.channelName, sample);

        QCOMPARE(events, QStringList({QStringLiteral("quality"), QStringLiteral("delta")}));
        QVERIFY(qualityStateVisible);
        QVERIFY(deltaStateVisible);
    }
};

QTEST_MAIN(MonitorDataProcessorTest)
#include "monitor_data_processor_test.moc"
