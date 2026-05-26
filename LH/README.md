# 伺服阀控制平台 (ServoValvePlatform)

基于 Qt 的伺服阀控制组态系统，使用 DSL（领域特定语言）进行系统配置和逻辑定义。

## 功能特点

### DSL 组态
- 拖拽式组态
- DSL 脚本编辑
- 语法高亮、代码补全、实时错误检查
- 组态校验
- Snippet 管理

### 实时监控
- 多通道数据实时显示
- 可配置采样周期和时间窗
- 数据导出（CSV / JSON / TSV）
- 图表导出为 PNG / JPG / SVG

### 多协议通信
- CAN 总线通信
- RS-485 串口通信
- Modbus 协议支持
- 以太网 TCP / UDP

### 控制功能
- PID 控制器
- 数据滤波
- 逻辑门组态
- 数学运算组件

## 项目结构

```
ServoValvePlatform/
├── include/
│   └── Common.h
├── src/
│   ├── common/
│   ├── core/
│   ├── designer/
│   ├── communication/
│   ├── compiler/
│   └── monitor/
├── third_party/
│   └── custom_dsp_language/
│       └── compile/
├── tests/
├── docs/
└── CMakeLists.txt
```

## 构建要求

- Qt 5.15+ 或 Qt 6.x
- CMake 3.16+
- C++17 兼容编译器
- Python 3.8+

## 构建步骤

```bash
git clone https://github.com/your-repo/ServoValvePlatform.git
cd ServoValvePlatform
mkdir build_current_mingw && cd build_current_mingw
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build . --config Release
ctest --output-on-failure
```

## Python 依赖配置

DSL 编译器使用 Python 实现，请不要提交虚拟环境目录，依赖通过 `requirements.txt` 安装。

```bash
cd third_party/custom_dsp_language/compile
python -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
```

## 使用说明

1. 新建项目
2. 编辑 DSL 脚本
3. 编译验证
4. 运行项目
5. 实时监控
6. 导出数据

## 快捷键

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

## 测试

```bash
ctest --output-on-failure
./tests/monitor_test
./tests/task_scheduler_test
./tests/data_manager_test
./tests/snippet_repository_test
./tests/dsl_completion_engine_test
./tests/monitor_export_test
```

## 开发指引

- 使用 C++17 标准
- 遵循 Qt 编码规范
- 单例类必须提供 `shutdown()` 方法释放资源
- 不依赖静态析构顺序进行资源回收

## 许可

MIT License

## 贡献

欢迎提交 Issue 和 Pull Request。

© 2024-2025 伺服阀控制平台开发团队
