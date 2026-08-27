# LH 伺服阀控制平台

LH 是一个基于 Qt 5 的桌面组态与控制应用。它提供 DSL 组态编辑、编译、运行、控制器通信、实时监控与数据导出能力。

## 功能

- DSL 脚本编辑：语法高亮、补全、片段与拖拽组态。
- 工程工作流：新建、打开、保存、校验、编译和运行工程。
- 控制器工作流：参数调试、构建产物下载、连接诊断，以及可选的 OPC DA 服务。
- 通信：串口、Modbus RTU/TCP、CAN/CANopen 和以太网 TCP/UDP。
- 实时监控：多通道数据显示与数据库记录；数据可导出为 CSV、JSON、TSV，图表可导出为 PNG、JPG、SVG。

## 构建要求

- CMake 3.15 或更高版本
- 支持 C++17 的编译器
- Qt 5.15（Core、Widgets、Network、Sql、SerialBus、SerialPort、Charts、Svg、Test）
- Python 3：仅在使用 DSL 编译功能时需要

## 构建与测试

从仓库根目录执行：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

生成的主程序目标为 `LH`。单配置生成器通常输出到 `build/bin/LH`；多配置生成器会在对应配置子目录下输出。

## DSL 编译环境

应用会优先使用 `third_party/custom_dsp_language/compile/venv` 中的 Python；若不存在，则查找 `PYTHON`、`python3` 或 `python`。在使用 DSL 编译前，请在该目录准备依赖：

```bash
cd third_party/custom_dsp_language/compile
python3 -m venv venv
. venv/bin/activate
python -m pip install -r requirements.txt
```

Windows 请使用 `venv\Scripts\activate` 激活虚拟环境。不要提交 `venv`、缓存或字节码文件。

## 安装与运行时数据

```bash
cmake --install build --prefix /path/to/install
```

安装包会包含 DSL 编译运行时所需的 Python 源文件和 `requirements.txt`，但不会包含虚拟环境或在安装时下载 Python 依赖。安装后，需在可执行文件上级的 `third_party/custom_dsp_language/compile` 中准备上述虚拟环境。

运行期 SQLite 数据库 `platform.db` 位于 Qt 的用户应用数据目录。旧安装目录 `data/platform.db` 仅会在用户数据目录尚无数据库时被迁移；应用不会继续写入旧路径。

在 Windows 上，安装默认调用与当前 Qt 匹配的 `windeployqt` 部署 Qt 运行库和关键插件。若外部打包流程已提供 Qt，可在配置时使用 `-DLH_ENABLE_QT_DEPLOYMENT=OFF`；非 Windows 平台由系统包管理器或平台打包流程提供 Qt 动态库。

## 项目结构

```
include/                 公共头文件
src/core/                数据管理与任务调度
src/compiler/            DSL 编译接口
src/communication/       工业通信与下载支持
src/designer/            主界面、工程与运行控制
src/monitor/             实时监控与导出
tests/                   CTest 测试套件
third_party/custom_dsp_language/compile/
                         Python DSL 编译运行时
```

## 常用快捷键

| 快捷键 | 功能 |
| --- | --- |
| Ctrl+N / Ctrl+O / Ctrl+S | 新建 / 打开 / 保存工程 |
| F7 / F8 | 编译 LH / 编译并运行 |
| F9 / Shift+F9 | 运行 / 停止工程 |
| Ctrl+M | 打开监控 |
| F5 / Shift+F5 | 开始 / 停止监控 |
| Ctrl+Shift+M | 打开调参窗口 |

## 许可

MIT License
