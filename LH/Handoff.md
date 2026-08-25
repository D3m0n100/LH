# LH 项目整改交接

更新时间：2026-08-23

## 1. 当前目标与执行口径

当前按仓库根目录 `LH_current_tasks_requirements_v4.2_mac.md` 的任务 ID 和合同边界，收口 OPC/监控线程安全、运行数据正确性、历史查询与导出、artifact/下载生命周期、构建脚本及维护性问题。

项目规则以根目录 `AGENTS.md` 为准，本文件只记录实际完成状态和验证债务，不重复通用委派规则。当前环境对 C++/Qt/Windows 相关批次默认只做静态验收：

- **L1**：源码、直接调用链、失败路径和测试代码静态验收通过；
- **L2**：目标平台 clean build、CTest 或 UI/脚本运行通过；
- **L3**：Matrikon、真实控制器、旧 CPU 或目标规模数据验证通过。

除 COMP-F1 的独立 Python package 验证外，本轮没有把旧构建目录或旧二进制当作运行证据。

## 2. 当前任务状态

| 任务 | 当前状态 | 等级 | 说明 / 后续验证 |
|---|---|---:|---|
| COR-B1 | 已完成，总控验收通过 | L1 | typed codec、严格数值转换、access/error、deviceId alias/default/conflict 已闭合；待 Windows CTest 与真机寄存器验证 |
| PAR-B1 | 已完成，总控验收通过 | L1 | partial success、typed readback、backend 销毁安全、OPC 单点 scope/Confirmed 发布已闭合；待 Windows CTest/Matrikon |
| MON-1 | 已完成，总控复验通过 | L1 | MonitorChannel 配置与 threshold snapshot 锁策略已统一；待 Windows CTest |
| DB-1 | 已完成，总控验收通过 | L1 | init/migration/index/cleanup/version hard-failure 语义已闭合；待 Windows CTest 与大库验证 |
| COMM-B2 | 已完成，总控验收通过 | L1 | outer timeout 固定映射 ReceiveTimeout，失败不再产生 NoError；待 Windows CTest/真机 |
| OUT-B1 | 已完成，总控验收通过 | L1 | 两个输出保存入口改为 QSaveFile 原子提交并覆盖 open/write/commit 失败反馈；待 Windows UI 复验 |
| EXP-1 | 已完成，总控验收通过 | L1 | 普通 CSV/JSON/TSV commit failure 保留旧文件回归已补齐；待 Windows CTest |
| HIST-B1 | 已完成，总控验收通过 | L1 | DB → Sample → 分页流式导出的 quality/error/origin 完整性保护已补齐；待 Windows CTest/大库 |
| EXP-D1 | 决策已确认 | N/A | 逐 Sample 无损导出与 aligned 视图保持不同语义，不重复决策 |
| OPC-D1 | 决策已确认 | N/A | HRESULT/null/quality/generation 合同已确定，OPC-1 必须沿用 |
| DL-D1 | 决策已确认 | N/A | 正式项目下载链与诊断下载链职责已确定，不重复决策 |
| COMP-S1 | 调查与合同已确认 | N/A | generation bundle、commit point、consumer/path 合同已追踪，不重复调查 |
| COMP-1 | 已完成，总控验收通过 | L1 | generation staging、一致性发布、manifest commit 与 portable path 已实现；待 Windows CTest/真机 |
| COMP-F1 | 已完成，总控验收通过 | L1 | standalone wheel/子包/console entry 已验证；未实现的 compile/check 明确失败；Python CLI 测试 2/2 通过 |
| DL-1A | 第一实施轮完成，总控验收通过 | L1 | 端口 owner、DeviceBusy、操作互斥、借用 backend 生命周期及诊断取消已收敛；待真机 VAL-3 |
| P3-1 | 已完成，总控验收通过 | L1 | MainWindowOutput/MainWindowMonitor、MonitorManagerHistory/Polling 已拆分并注册；待 Windows clean build |
| BUILD-B1 | 已完成，总控验收通过 | L1 | monitor 自定义 `-O3 -march=native` Release 块已删除；待 Windows/旧 CPU build/run |
| WIN-B1 | 已完成，总控验收通过 | L1 | 根与 communication CMake 统一 Qt5，README 只声明 Qt 5.15+；待 Windows clean configure/build |
| WIN-B2 | 已完成，总控复验通过 | L1 | CMake target 与三个 Windows 脚本均使用 `LH.exe`；待 Windows 脚本实跑 |
| WIN-B3 | 已完成，总控验收通过 | L1 | stdout/stderr 同时异步消费，真实退出码与报告语义保留；待 Windows 大量双流输出实测 |
| CLEAN-1 | 已完成，总控验收通过 | L1 | 已删除无构建/无引用的 `CANCommon.cpp` 与 `project_controller_stub.cpp`；待 Windows clean build |
| MON-I1 | 调查关闭 | N/A | 现有 render timer 主动 drain，无生产缺陷证据；只有新测量证据才重开 |
| OPC-1 | 未实施，Windows 阻断 | L0 | 需要 Windows 条件源码可编译后补完整 OPC 测试矩阵 |
| VAL-1 | 阻断 | L0 | 需要 Windows clean configure/build/targeted CTest/full CTest |
| VAL-2 | 阻断 | L0 | 需要 Windows + Matrikon OPC DA |
| VAL-3 | 阻断 | L0 | 需要真实控制器、串口和正式/诊断下载链 |
| VAL-4 | 阻断 | L0 | 需要目标规模数据库和长时间导出环境 |

当前 v4.2 合同下，普通 macOS 静态代码任务已完成；剩余工作集中在 Windows、Matrikon、真实控制器和规模环境验证。不得因需求文档中的实施前状态文字尚未同步而重复执行本表已达到 L1/N/A 的任务。

## 3. 已完成工作的关键合同

### 3.1 RuntimePoint、参数与通信

- RuntimePoint 寄存器 codec 统一处理 BOOL、整数和 REAL/FLOAT32，严格校验字符串、范围、寄存器宽度、byte/word order、scale/offset；非法值不得 fallback 为 0。
- `elementCount` 表示逻辑元素数，寄存器数量由目标类型宽度推导。
- 显式 `slaveId/stationAddress/serverAddress` 优先于默认 `unitId=1`；冲突或非法 alias 在 I/O 前 hard-fail。
- mapped access violation 返回 `PermissionDenied`，transport error 保留原错误，不得覆盖为地址或参数错误。
- Modbus 外层等待超时固定为 `ReceiveTimeout`；所有失败路径禁止留下 `CommErrorCode::NoError`。
- 参数批量提交允许成功点继续回读，最终结果以本次目标集合计算；BOOL/整数/REAL 按目标类型比较，REAL 使用 float32 representation-aware 语义。
- 借用 backend 用 QPointer/destroyed guard 隔离异步回读；销毁或替换后 pending 必须一次失败收口，不发布 Good。
- OPC 单点写只作用于该 pointId；只有 readback `Confirmed` 后才能记录成功并发布新 Good 值。

### 3.2 历史数据、数据库与导出

- 历史查询使用 `(timestamp, id)` keyset 分页，并固定查询结束时间。
- 历史记录持久化 `quality`、`origin`、`error_code`、`error_text`；通信失败不能作为正常数值 0 传播。
- DataManager batch insert 保持事务语义；初始化必要 SQL、版本读取、required index、schema version 更新与 connection cleanup 均按 hard failure 处理。
- 非整数、负数或未来 schema version 被拒绝，不新增 schema v5。
- CSV/JSON/TSV 大批量导出采用有界分页流式写入和 QSaveFile；provider、写入或 commit 失败保留旧文件。
- aligned CSV/TSV 是按时间戳对齐视图，不承诺同一 channel/timestamp 多条 Sample 逐条无损；行式导出承担逐 Sample 无损语义。

### 3.3 Monitor、输出保存与结构拆分

- MonitorChannel 的公开 config getter、threshold 读取和 samples 修改统一受 mutex 保护。
- threshold evaluation 使用锁内复制的配置快照，解锁后判断并 emit，不在持锁状态触发外部可重入 signal。
- MainWindow 与 OutputPaneController 的日志保存均使用 `QSaveFile + QTextStream::status() + commit()`；打开、写入或提交失败不会提示成功，也不会破坏旧目标文件。
- P3-1 只移动 implementation，没有改 API：`MainWindowOutput.cpp`、`MainWindowMonitor.cpp`、`MonitorManagerHistory.cpp`、`MonitorManagerPolling.cpp` 已进入 CMake。
- CLEAN-1 只删除两个确认死文件；`src/compiler/dummy.cpp` 位于冻结区，仍保留。

### 3.4 Artifact、Compiler package 与下载链

- 正式链固定为 `RuntimeSessionController -> RunController artifact validation -> ControllerDeviceBackend`；诊断链保留为工程工具，不是正式 artifact consumer。
- 正式产物按 generation bundle 隔离生成并校验，以 `runtime_manifest.json` 为提交点；失败时旧 generation 继续可用。
- 新路径使用 manifest-relative/project-relative；absolute path、`..` 和越出允许 root 的输入不得成为正式下载依赖。
- standalone Python package 使用 setuptools 子包发现并复用现有 `COMPILER_GUIDE.md`；wheel 中已包含 CLI/后端/功能块子包，console entry 可导入。
- package 的 `compile`/`check` 仍未实现，但现在明确以退出码 1 失败，不再输出成功结论。本任务不等于实现 Python compiler semantics。
- COMP-F1 获得过用户对三个精确文件的狭窄授权；该授权不延伸到后续 `src/compiler/**` 或第三方 Compiler 修改。
- DL-1A 的正式下载仍为同步调用。诊断链可取消不等于正式链可即时取消；只有 VAL-3 提供真实 UI/设备失败证据后才允许定义 DL-1B。

### 3.5 Windows/构建前置整理

- 当前产品基线是 Qt 5.15+，不支持 Qt6 构建；communication 子模块不得自行选择另一 Qt major。
- target 名称是 `LH`，`build.bat`、`run_platform.bat`、`tools/workflow_dev.ps1` 均定位 `bin\LH.exe`。
- monitor 不再使用 host-specific `-march=native`，依赖 CMake/toolchain 标准 Release flags，warning flags 保留。
- `Invoke-Logged` 在 `WaitForExit()` 前同时启动 stdout/stderr `ReadToEndAsync()`，避免一侧管道填满造成父子进程互等；输出仍写控制台和报告，非零退出仍失败。

## 4. 当前阻断与未验证项

### 4.1 OPC-1 只能在 Windows 继续

OPC-1 需要覆盖 master/per-item HRESULT、更多 VARIANT 类型、quality/null payload、callback generation、stop/reconnect 后迟到回调等 Windows 条件测试。macOS 静态阅读不能证明 COM 分支可编译，也不能替代 Matrikon 行为。

### 4.2 L1 不能写成运行通过

当前 C++ 工作树没有对应的 clean configure/build/CTest 证据。必须在目标环境补做：

- VAL-1：Windows clean configure/build、targeted CTest、full CTest；
- VAL-2：Matrikon activation、browse、group/item、同步读写、订阅、断线重连和 late callback；
- VAL-3：真实寄存器编码、设备号 alias、端口互斥、retry/reconnect、取消和 artifact generation 消费；
- VAL-4：大数据库分页稳定性、内存上界、吞吐和导出失败保留旧文件；
- WIN-B3：同时大量 stdout/stderr 且 child 非零退出的 PowerShell 实测；
- BUILD-B1：目标部署 CPU 运行，确认无 illegal instruction。

### 4.3 工作树不是干净基线

当前工作树包含多批已验收修改、用户已有修改、已删除旧需求文档及未跟踪的新实现文件。不得根据整个 `git status` 推断单批来源，不得为“清理”执行全局回退、恢复、格式化或删除未跟踪文件。

## 5. 下一步计划

1. 准备 Windows 新环境并执行 VAL-1；必须使用全新 build 目录。
2. 在 Windows 编译条件恢复后实施 OPC-1，再运行其 targeted CTest。
3. 使用 Matrikon 完成 VAL-2，记录版本、配置、回调线程和断线/重连结果。
4. 连接真实控制器完成 VAL-3；只有真实证据表明同步正式下载不可接受时才新建 DL-1B。
5. 使用目标规模数据完成 VAL-4，并记录分页次数、峰值内存、吞吐和失败恢复。
6. 每项验证完成后只更新状态和证据，不重复实现已有 L1 代码。

## 6. 绝对不要再踩的坑

1. 不要把子智能体完成报告当成总控验收；必须复核 scoped diff、直接调用链和失败路径。
2. 不要把 L1 写成“编译/测试通过”，也不要使用旧 build、旧 CTest 注册或旧二进制作证据。
3. 不要回退或格式化整个脏工作树；只操作当前任务精确授权文件。
4. 不要恢复 v3/v4/v4.1 的旧任务状态；以 v4.2 合同边界和本文件当前完成证据为准。
5. 不要重复执行 MON-1、P3-1、WIN-B2、COMP-1、DL-1A、已确认决策或本轮已达到 L1 的 correctness/build 批次。
6. 不要把 Compiler 的一次狭窄授权扩展成永久写权限；任何新 Compiler 修改必须重新列最终最小文件清单并获批。
7. 不要把 COMP-F1 误写成编译器核心已实现；standalone `compile/check` 当前只保证诚实失败。
8. 不要让通信失败变成正常 0、让失败保留 NoError、让非法 alias 发起 I/O，或让 partial success 提前终止成功点回读。
9. 不要在持有 MonitorChannel mutex 时 emit，不要在 backend 销毁后继续回读，也不要让旧 OPC callback 污染新 generation。
10. 不要把诊断下载可取消等同于正式同步下载可即时取消，也不要重新讨论已确认的正式 consumer 职责。
11. 不要在 macOS 上宣称 Windows COM、PowerShell、windeployqt 或脚本路径已运行通过。
12. 不要重新加入 `CANCommon.cpp` 或 `project_controller_stub.cpp`；前者会与 header inline 实现重定义，后者属于失效测试拓扑。

## 7. 接手后的第一步

1. 完整阅读根目录 `AGENTS.md`、`LH_current_tasks_requirements_v4.2_mac.md` 和本文件。
2. 查看工作树但不回退任何现有修改；按任务 ID 建立 scoped 文件清单。
3. 若仍在 macOS，确认没有新的普通代码任务后停止重复静态扫描，准备 Windows/目标环境验证材料。
4. 若已在 Windows，先执行 VAL-1 clean configure/build，再按失败证据修真实 blocker，不顺带做 Qt6 migration 或结构整理。
5. 每批完成后更新本文件的任务状态、验证等级、证据与剩余阻断。
