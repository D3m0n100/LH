# 伺服阀控制平台 (ServoValvePlatform)

基于 Qt 的伺服阀控制组态系统，采用 DSL（领域特定语言）进行系统配置和逻辑定义。

## ✨ 功能特点

### 🎯 DSL 组态系统
- **拖拽式组态**: 从函数列表拖拽组件到 DSL 编辑器，自动生成代码片段
- **DSL 脚本编辑**: 支持语法高亮、代码补全、实时错误检查
- **组态校验**: 自动检测配置冲突、资源限制、必填字段
- **Snippet 管理**: 支持内置和工程级 Snippet 配置

### 📊 实时监控
- 多通道数据实时显示
- 可配置的采样周期和时间窗口
- 数据导出（CSV/JSON/TSV 格式）
- 图表导出为图像（PNG/JPG/SVG）

### 🔌 多协议通信
- CAN 总线通信
- RS-485 串口通信
- Modbus 协议支持
- 以太网 TCP/UDP

### 🎮 控制功能
- PID 控制器（通过 DSL 配置）
- 数据滤波器
- 逻辑门组件
- 数学运算组件

## 📁 项目结构

```
ServoValvePlatform/
├── include/                    # 公共头文件
│   └── Common.h               # 通用宏和类型定义
├── src/
│   ├── common/                # 共享类型定义
│   │   └── ConfigTypes.h      # 配置相关结构体
│   ├── core/                  # 核心模块
│   │   ├── DataManager.*      # 数据管理器（SQLite）
│   │   └── TaskScheduler.*    # 任务调度器
│   ├── designer/              # 设计器模块
│   │   ├── MainWindow.*       # 主窗口
│   │   ├── DslScriptEditor.*  # DSL 脚本编辑器
│   │   ├── DslCompletionEngine.*   # 代码补全引擎
│   │   ├── DslDragDropHandler.*    # 拖拽处理器
│   │   ├── SnippetRepository.*     # Snippet 仓库
│   │   ├── OutputPaneController.*  # 输出窗口控制器
│   │   └── MonitorController.*     # 监控控制器
│   ├── communication/         # 通信模块
│   ├── compiler/              # DSL 编译器模块
│   ├── monitor/               # 监控模块
│   │   ├── MonitorManager.*   # 监控管理器
│   │   ├── MonitorWidget.*    # 监控窗口控件
│   │   └── MonitorExportHelper.*  # 数据导出助手
│   └── main.cpp
├── third_party/
│   └── custom_dsp_language/   # DSL 编译器（Python）
│       └── compile/
│           └── requirements.txt
├── tests/                     # 测试用例
├── docs/                      # 文档
└── CMakeLists.txt
```

## 🔧 构建要求

### 依赖项

- **Qt 5.15+** 或 **Qt 6.x**
- **CMake 3.16+**
- **C++17** 兼容编译器
- **Python 3.8+**（用于 DSL 编译器）

### 构建步骤

```bash
# 克隆仓库
git clone https://github.com/your-repo/ServoValvePlatform.git
cd ServoValvePlatform

# 创建构建目录
mkdir build && cd build

# 配置（指定 Qt 路径）
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt

# 编译
cmake --build . --config Release

# 运行测试
ctest --output-on-failure
```

## 🐍 Python 依赖配置

DSL 编译器使用 Python 实现。**请勿提交 Python 虚拟环境目录**，
应使用 `requirements.txt` 安装依赖：

```bash
# 进入 DSL 编译器目录
cd third_party/custom_dsp_language/compile

# 创建虚拟环境
python -m venv venv

# 激活虚拟环境
# Windows:
venv\Scripts\activate
# Linux/macOS:
source venv/bin/activate

# 安装依赖
pip install -r requirements.txt
```

> ⚠️ **注意**: `venv/` 目录已在 `.gitignore` 中排除，不会被提交到版本库。

## 📖 使用说明

### 快速开始

1. **新建项目**: 文件 → 新建项目，输入项目名称并选择保存位置
2. **编辑 DSL 脚本**: 在 DSL 编辑器中编写组态代码，或从左侧函数列表拖拽组件
3. **编译验证**: 按 F7 编译 DSL 脚本，检查语法错误
4. **运行项目**: 按 F9 启动项目，开始数据采集和控制
5. **实时监控**: Ctrl+M 打开监控窗口，查看实时数据
6. **导出数据**: 在监控窗口选择通道，点击导出按钮保存数据

### DSL 拖拽式组态

1. 在左侧 **函数列表** 中浏览可用组件
2. 将组件 **拖拽** 到 DSL 编辑器中的目标位置
3. 系统自动生成对应的 DSL 代码片段
4. 根据需要修改参数（如采样周期、通道名称等）
5. 使用 **代码补全** (Ctrl+Space) 快速输入

### 快捷键

| 快捷键 | 功能 |
|--------|------|
| Ctrl+N | 新建项目 |
| Ctrl+O | 打开项目 |
| Ctrl+S | 保存项目 |
| Ctrl+D | 显示/隐藏 DSL 编辑器 |
| F7 | 编译 DSL 组态 |
| F9 | 运行项目 |
| Shift+F9 | 停止运行 |
| Ctrl+M | 打开监控窗口 |
| F5 | 开始监控 |
| Shift+F5 | 停止监控 |
| Ctrl+Space | 代码补全 |
| Ctrl+F | 查找 |
| Ctrl+H | 查找替换 |

## 🧪 测试

项目包含以下测试用例：

```bash
# 运行所有测试
cd build
ctest --output-on-failure

# 运行特定测试
./tests/monitor_test
./tests/task_scheduler_test
./tests/data_manager_test
./tests/snippet_repository_test
./tests/dsl_completion_engine_test
./tests/monitor_export_test
```

## 📝 开发指南

### 代码规范

- 使用 **C++17** 标准
- 遵循 Qt 编码规范
- 单例类必须提供 `shutdown()` 方法释放资源
- 不依赖静态析构顺序进行资源回收

### 架构设计

MainWindow 采用**职责分离**设计：
- 业务逻辑委托给专门的控制器类
- 便于单独测试和维护
- 详见 `MainWindow.h` 顶部注释

### 生命周期管理

```cpp
// 应用启动时
DataManager::instance().initialize("path/to/db");
TaskScheduler::instance().start();

// 应用运行中...

// 应用退出前
TaskScheduler::instance().shutdown();
DataManager::instance().shutdown();
```

### Snippet 配置

系统支持两级 Snippet 配置：

1. **内置 Snippets**: 从 Qt 资源文件 `:/snippets/default_snippets.json` 加载
2. **工程级 Snippets**: 从项目目录下的 `snippets.json` 加载（覆盖内置）

Snippet JSON 格式示例：

```json
{
  "snippets": [
    {
      "id": "pressure_sensor",
      "name": "压力传感器",
      "category": "input",
      "description": "读取压力传感器数据",
      "templateCode": "pressure_sensor(name = \"$NAME$\", address = $ADDR$, period = 20)",
      "unit": "bar",
      "defaultPeriodMs": 20
    }
  ]
}
```

## 📄 许可证

[MIT License](LICENSE)

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

---

© 2024-2025 伺服阀控制平台开发团队
