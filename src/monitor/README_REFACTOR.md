# MonitorWidget 拆分重构说明

## 概述

本次重构将原本约 896 行的 `MonitorWidget.cpp` 按职责拆分为多个独立模块，提高代码可维护性和可测试性。

## 新增模块

### 1. MonitorChartView（图表渲染模块）

**文件**：`MonitorChartView.h` / `MonitorChartView.cpp`

**职责**：
- 封装所有图表渲染相关逻辑
- 管理曲线的添加、删除、更新
- 设置样式、颜色、坐标轴范围
- 提供图表导出为图片的功能

**主要接口**：
```cpp
void addChannel(const QString& name, const ChannelStyle& style = ChannelStyle());
void removeChannel(const QString& name);
void updateChannelData(const QString& name, const QVector<QPointF>& points);
void setActiveChannel(const QString& name);
void setTimeWindow(qint64 windowMs);
bool exportAsPng(const QString& filePath, int width = 0, int height = 0);
bool exportAsJpg(const QString& filePath, int quality = 90);
bool exportAsSvg(const QString& filePath);
```

### 2. MonitorDataProcessor（数据处理模块）

**文件**：`MonitorDataProcessor.h` / `MonitorDataProcessor.cpp`

**职责**：
- 从原始采样数据生成适合绘图的数据结构
- 处理限幅、重采样、平滑、时间窗口裁剪
- 计算统计值（最小值、最大值、平均值）
- 数据过滤和缓存管理

**主要接口**：
```cpp
ProcessedChannelData processChannel(Monitor::MonitorChannel* channel);
ProcessedChannelData processSamples(const QString& channelName, const QList<Monitor::Sample>& samples);
void feedSample(const QString& channel, double timestampMs, double value);
QVector<QPointF> getSeriesData(const QString& channel) const;
QList<Monitor::Sample> filterByTimeRange(const QList<Monitor::Sample>& samples, const QDateTime& startTime, const QDateTime& endTime) const;
QList<Monitor::Sample> filterByRecentSeconds(const QList<Monitor::Sample>& samples, int seconds) const;
static std::optional<double> calculateMin(const QVector<QPointF>& points);
static std::optional<double> calculateMax(const QVector<QPointF>& points);
static std::optional<double> calculateAvg(const QVector<QPointF>& points);
```

**数据处理配置**：
```cpp
struct DataProcessorConfig {
    int maxSamples = 200;           // 最大保留样本数
    qint64 timeWindowMs = 30000;    // 时间窗口（毫秒）
    bool enableSmoothing = false;   // 是否启用平滑
    int smoothingWindow = 3;        // 平滑窗口大小
    bool enableResampling = false;  // 是否启用重采样
    int resampleIntervalMs = 100;   // 重采样间隔
    std::optional<double> minClamp; // 最小限幅值
    std::optional<double> maxClamp; // 最大限幅值
};
```

### 3. MonitorExportHelper（导出功能模块）

**文件**：`MonitorExportHelper.h` / `MonitorExportHelper.cpp`

**职责**：
- 导出图表图像（PNG、JPG、SVG）
- 导出数据为 CSV/JSON/文本
- 管理文件选择对话框
- 导出进度和错误处理

**主要接口**：
```cpp
// 图像导出
ExportResult exportChartAsPng(MonitorChartView* chartView, const QString& suggestedFileName = QString());
ExportResult exportChartAsJpg(MonitorChartView* chartView, const QString& suggestedFileName = QString());
ExportResult exportChartAsSvg(MonitorChartView* chartView, const QString& suggestedFileName = QString());

// 数据导出
ExportResult exportDataAsCsv(const QString& channelName, const QList<Monitor::Sample>& samples, const QString& suggestedFileName = QString());
ExportResult exportDataAsJson(const QString& channelName, const QList<Monitor::Sample>& samples, const QString& suggestedFileName = QString());
ExportResult exportViaManager(Monitor::MonitorManager* manager, const QString& channelName, const QString& format = "csv");

// 文件对话框
QString showSaveImageDialog(const QString& suggestedFileName = QString(), const QString& filter = QString());
QString showSaveDataDialog(const QString& suggestedFileName = QString(), const QString& filter = QString());

// 辅助函数
static QString generateDefaultFileName(const QString& channelName, const QString& extension);
static QString ensureExtension(const QString& filePath, const QString& extension);
```

## 重构后的 MonitorWidget

**文件**：`MonitorWidget.h` / `MonitorWidget.cpp`

**职责变化**：
- 作为总控类，负责 UI 布局和模块协调
- 持有 `MonitorChartView`、`MonitorDataProcessor`、`MonitorExportHelper` 实例
- 将外部信号转发给相应模块
- 保持对外 API 不变

**新增的子模块访问接口**：
```cpp
MonitorChartView* chartView() const;
MonitorDataProcessor* dataProcessor() const;
MonitorExportHelper* exportHelper() const;
```

## 从 MonitorWidget 移出的主要逻辑块

| 原代码位置 | 移动到 | 说明 |
|-----------|--------|------|
| `updateChart()` 中的图表更新逻辑 | `MonitorChartView` | 图表数据更新和渲染 |
| `updateChannelData()` 中的数据转换 | `MonitorDataProcessor` | 样本转点序列、统计计算 |
| 时间窗口过滤逻辑 | `MonitorDataProcessor` | `filterByTimeRange()`, `filterByRecentSeconds()` |
| `onExportData()` 中的文件写入 | `MonitorExportHelper` | CSV/JSON 文件写入 |
| `exportCurrentChartImage()` | `MonitorExportHelper` | 图片导出逻辑 |
| 颜色、样式管理 | `MonitorChartView` | `ChannelStyle` 结构 |

## 文件结构

```
src/monitor/
├── CMakeLists.txt              # 更新后的构建文件
├── MonitorTypes.h              # 类型定义（保持不变）
├── MonitorChannel.h/.cpp       # 通道类（保持不变）
├── MonitorManager.h/.cpp       # 管理器（保持不变）
├── ChartWidget.h/.cpp          # 底层图表组件（保持不变）
├── SampleDataProvider.h/.cpp   # 数据提供者（保持不变）
├── MonitorWidget.h/.cpp        # 重构后的总控类
├── MonitorChartView.h/.cpp     # 新增：图表渲染模块
├── MonitorDataProcessor.h/.cpp # 新增：数据处理模块
└── MonitorExportHelper.h/.cpp  # 新增：导出功能模块
```

## 依赖关系

```
MonitorWidget (总控类)
    ├── MonitorChartView (图表渲染)
    │   └── ChartWidget (底层实现)
    ├── MonitorDataProcessor (数据处理)
    │   └── MonitorChannel (数据源)
    └── MonitorExportHelper (导出功能)
        └── MonitorManager (数据导出接口)
```

## 编译要求

- Qt 5.x（需要 Core、Widgets、Sql、Charts、Svg 模块）
- C++17 标准
- CMake 3.10+

## 兼容性

- 所有公开 API 保持不变
- 外部调用代码无需修改
- 行为与重构前一致

## 扩展建议

1. **多通道叠加显示**：可在 `MonitorChartView` 中扩展多曲线管理
2. **实时平滑**：通过 `DataProcessorConfig` 启用平滑处理
3. **自定义导出格式**：在 `MonitorExportHelper` 中添加新格式支持
4. **数据缓存优化**：`MonitorDataProcessor` 支持增量更新
