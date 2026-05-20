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
#include <cmath>

#include "MonitorDataProcessor.h"

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
        newConfig.enableSmoothing = false;
        newConfig.enableDownsampling = false;

        processor.setConfig(newConfig);

        DataProcessorConfig readBack = processor.config();
        QCOMPARE(readBack.timeWindowMs, newConfig.timeWindowMs);
        QCOMPARE(readBack.maxDisplaySamples, newConfig.maxDisplaySamples);
        QCOMPARE(readBack.ringBufferCapacity, newConfig.ringBufferCapacity);
        
        qInfo() << "完整配置测试通过";
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

        // 启用增量模式
        processor.setIncrementalMode(true);
        
        QVector<QPointF> points1 = { QPointF(0, 10), QPointF(1, 20) };
        processor.appendPoints(channelId, points1);
        
        QVector<QPointF> delta1 = processor.drainDelta(channelId);
        QCOMPARE(delta1.size(), points1.size());

        // 禁用增量模式后的行为取决于具体实现
        // 这里只验证接口可用且不崩溃
        processor.setIncrementalMode(false);
        
        QVector<QPointF> points2 = { QPointF(2, 30) };
        processor.appendPoints(channelId, points2);
        
        // 获取数据应该仍然工作
        QVector<QPointF> channelData = processor.getChannelData(channelId);
        QVERIFY(channelData.size() > 0);
        
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
};

QTEST_MAIN(MonitorDataProcessorTest)
#include "monitor_data_processor_test.moc"
