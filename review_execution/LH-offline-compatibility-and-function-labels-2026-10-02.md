# 函数名称显示与历史离线编译修复

本记录保留为上一方案的验证快照。当前编译入口和生成行为已按用户后续要求调整，以[编译器生成行为恢复](LH-compiler-rollback-2026-10-02.md)为准。

日期：2026-10-02。代码库：`D:\Table\LH`。保留既有工作区修复；未提交、未推送。

## 函数列表

项目侧栏的函数库树和编辑器内的函数列表继续保留全部函数条目。名称直接使用原名称，如 `KeyScan`、`M600TextDisp`，不再追加“（未完成）”。搜索、分类、函数 ID 保持原行为。能力状态、禁止插入及编译器拒绝未确认功能块的规则继续有效。

现有界面集成回归检查全部条目的 ID 和名称，并验证不支持条目的拖拽限制、双击行为及支持条目的实际插入。检查通过；界面证据来自 Qt offscreen，不代表物理桌面或控制器验收。

## 历史兼容编译

“构建”菜单和“编译LH”下拉菜单增加同一个“历史兼容编译（离线）”动作。普通编译入口和快捷键保留严格检查。Python 两个 CLI 入口均支持显式参数 `--legacy-constants-offline`。

兼容模式仅复现历史常量赋值的独立 ConstBuild 块输出。解析器、类型与范围检查、重复声明、路径及输入文件别名保护复用现有代码。初始化语法、变量复制、运行时表达式和条件跳转仍会报错。此模式没有实现目标变量真实写回，不确认目标固件指令契约。

输出 `.code` 带 ASCII 离线标记，`.rep` 带 `compile_only=1`、兼容模式和执行未确认状态。IDE 将其归为 `compiled_code`，清理旧下载引用，不发布下载 manifest，即使项目配置了有效 DownloadProfile 也保持离线。运行前检查、控制器下载后端、DownloadManager 和独立 Python 下载工具均拒绝带标记产物；控制器后端检查实际传输快照，正常下载和 dry-run 均在写入前拒绝。

安装白名单补齐 `lh_compiler/cli`，防止已统一的公共 CLI 模块在安装布局中缺失；安装布局回归同时检查公共 CLI 和离线后端模块。

## 本机验证

- 全量原生构建成功。CTest 45/45 通过；补齐安装白名单后，InstallLayoutTest 和 ModuleBoundaryCheck 再次 2/2 通过。
- Python 单元检查 109 项通过、1 项跳过，其中新增离线兼容回归 16 项。
- `D:\Table\test1\main.lh` 在严格模式仍拒绝未确认的赋值写回；在离线兼容模式成功生成 11 条指令，与 2026-09-16 历史输出的指令逐行相同。该项目现有配置仅选择 `main.lh`，此结果不覆盖未选择的四个子脚本。
- 源码入口和安装入口分别验证上述结果；用户项目 75 个文件没有改变。
- 安装版 `--version` 和 `--smoke-test` 成功，子进程 PATH 仅含 Windows 系统目录，使用部署的 Qt 插件。冒烟前后原库 schemaVersion=5、runtimeRecords=16531，保持一致。
- 对开始前记录的 359 个源码/配置/测试文件进行哈希核对，原文件未删除，变化限于本次涉及文件。原 `bin\LH.exe` 的哈希、原 CMake 链接脚本和 Git HEAD 保持一致。

日志和产物目录：[验证证据](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-legacy-offline-and-catalog-2026-10-02)。其中 `ctest.log`、`ctest-install-final.log`、`python-final.log`、`test1-results.json`、`delivery-verification.json` 和 `preservation-results.json` 记录具体结果。

## 使用新版

[本机新版 LH](D:/Table/LH/build_current_mingw/delivery_20261002/bin/LH.exe) 已部署 Qt 依赖与本机 Python 编译环境。Python 环境使用本机现有解释器基础目录，该目录作为本机开发交付使用。

当前运行的旧窗口未中断，也未覆盖其可执行文件。保存并关闭旧窗口后启动上述新版，即可看到函数名称变化及离线兼容编译入口。没有进行真实固件执行或设备下载验收；目标赋值、运算和跳转规则仍需固件契约证据。
