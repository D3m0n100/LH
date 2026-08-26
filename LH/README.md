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

- Qt 5.15+
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

Windows：

```bash
cd third_party/custom_dsp_language/compile
python -m venv venv
venv\Scripts\activate
python -m pip install -r requirements.txt
```

Linux/macOS 使用对应的本地虚拟环境激活方式：

```bash
cd third_party/custom_dsp_language/compile
python3 -m venv venv
. venv/bin/activate
python -m pip install -r requirements.txt
```

安装包不会复制仓库中的 `venv`，也不会在 CMake 安装阶段联网安装 Python 或第三方包。安装后的程序会在可执行文件所在目录的上级查找
`third_party/custom_dsp_language/compile`；运行 DSL 编译前，请在该目录准备 Python 3.8+ 的本地 `venv` 并按上面的方式安装
`requirements.txt`。若找不到 Python 3、虚拟环境依赖或 `antlr4-python3-runtime`，编译会失败并在错误输出中说明缺失项，不会静默报告成功。

### 安装与 Qt 运行库

安装规则只包含 DSL 编译所需的 `lmc.py`、`requirements.txt`、ANTLR 生成 Python 文件，以及 `src/lh_compiler` 的前端、后端和功能块定义；
不会安装虚拟环境、缓存、字节码、测试/示例/文档或编译输出目录。

Windows 安装默认启用 Qt 部署：配置时会定位与当前 Qt 匹配的 `windeployqt`，安装时复制 Qt 运行库并检查 `platforms/qwindows.dll`
和 `sqldrivers/qsqlite.dll`。工具缺失、执行失败或关键插件缺失都会使安装失败。特殊打包环境若由外部流程提供 Qt，可显式使用
`-DLH_ENABLE_QT_DEPLOYMENT=OFF`，此时必须由该外部流程补齐 Qt 运行库和插件。非 Windows 平台不调用 `windeployqt`，请由操作系统包管理器或平台打包流程提供 Qt 动态库。

### 运行期数据库

运行期数据库 `platform.db` 使用 Qt `QStandardPaths::AppDataLocation` 提供的平台用户应用数据目录，首次启动会创建所需父目录；创建或打开失败时程序会明确报错并退出，不会写入安装目录或临时目录。

从旧版本升级时，若可执行文件上级安装前缀下的 `data/platform.db` 存在且用户数据目录尚无目标库，程序会先复制到目标目录的 staging 文件并原子提交，再按现有 Schema 迁移。目标库已存在时以目标库为准，旧路径不再被应用写入；迁移失败不会覆盖已有目标库，也不会回退到旧路径继续运行。

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
