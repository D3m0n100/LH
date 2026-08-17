# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

LH 是基于 Qt5 的伺服阀控制组态平台，核心功能是通过 DSL（领域特定语言）进行系统配置和逻辑定义，支持多协议通信（CAN/RS-485/Modbus/TCP-UDP）和实时数据监控。

## 构建命令

```bash
# 配置（MinGW）
cd build_current_mingw
cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/mingw81_64 ..

# 构建
cmake --build . -j4

# 运行所有测试
ctest --output-on-failure

# 运行单个测试
./build_current_mingw/tests/monitor_test
```

或直接运行 `build.bat` 一键构建。

## 架构

**C++ 主程序**（Qt5 Widgets）分为五个模块：

- `src/core/` — 核心层：DataManager（SQLite 数据管理，单例）、TaskScheduler（任务调度，单例）
- `src/communication/` — 通信层：ICommInterface 抽象接口，CommFactory 工厂模式创建 CAN/Serial/Modbus/Ethernet/J1939 实现，ControllerBridge 桥接控制器
- `src/compiler/` — DSL 编译接口层：DSLCompilerInterface 通过 QProcess 调用 Python 编译器，支持同步/异步编译
- `src/designer/` — UI 设计层：MainWindow（主窗口，88KB 最大文件）、DslScriptEditor（DSL 编辑器）、BuildController/RunController/MonitorController 分离构建/运行/监控控制逻辑、ProjectController 项目管理、SnippetRepository 代码片段管理
- `src/monitor/` — 实时监控层：MonitorManager（单例）管理监控生命周期、ChartWidget/MonitorChartView 图表显示、MonitorExportHelper 数据导出（CSV/JSON/TSV/PNG/JPG/SVG）、RingBuffer 环形缓冲、MonitorDataProcessor 数据处理

**Python DSL 编译器**（`third_party/custom_dsp_language/compile/`）：
- 入口：`lmc.py`，语法定义在 `grammar/` 目录
- C++ 端通过 `DSLCompilerInterface` 调用 `python lmc.py compile` 命令
- Python 环境需要单独安装依赖：`pip install -r requirements.txt`

**启动流程**：`main.cpp` → DataManager 初始化 SQLite → TaskScheduler 启动 → MainWindow 创建 → aboutToQuit 信号中按序 shutdown 各单例

## 关键约定

- C++17 标准，Qt 编码规范
- 单例使用 `Common.h` 中的 `SINGLETON` 宏，**必须提供 `shutdown()` 方法**，不依赖静态析构顺序
- 日志使用 `LOG_INFO/LOG_WARN/LOG_ERROR/LOG_DEBUG` 宏（基于 qDebug/qInfo/qWarning/qCritical）
- 通信协议枚举在 `Platform::CommProtocol` 中定义
- Qt5 模块依赖：Core, Widgets, Network, Sql, SerialBus, SerialPort, Charts, Svg, Test
- 测试使用 Qt Test 框架，每个测试是独立可执行文件

## 测试

tests/ 目录下每个测试文件编译为独立可执行文件，通过 CTest 注册。测试名称对应 CMakeLists.txt 中的 `add_test(NAME ...)`。新测试需同时在 `tests/CMakeLists.txt` 中添加 `add_executable` 和 `add_test`。
