# LH 项目整改交接

更新时间：2026-08-26

## 1. 当前目标与执行口径

当前唯一需求合同为仓库根目录 `LH_current_tasks_requirements_v4.2_mac.md`。合同已在 2026-08-26 合并最新工程审查结果，并精简为“未完成实施项 + 条件验证 + 延期风险台账”；已完成任务不再保留在需求合同任务表中，其完成证据继续由本文件与 Git 历史保存。

项目规则以根目录 `AGENTS.md` 为准，本文件只记录实际完成状态和验证债务，不重复通用委派规则。当前环境对 C++/Qt/Windows 相关批次默认只做静态验收：

- **L1**：源码、直接调用链、失败路径和测试代码静态验收通过；
- **L2**：目标平台 clean build、CTest 或 UI/脚本运行通过；
- **L3**：Matrikon、真实控制器、旧 CPU 或目标规模数据验证通过。

除 COMP-F1 的独立 Python package 验证外，本轮没有把旧构建目录或旧二进制当作运行证据。

## 2. 当前任务与历史完成状态

| 任务 | 当前状态 | 等级 | 说明 / 后续验证 |
|---|---|---:|---|
| CFG-1 | 已完成，总控验收通过 | L1 | 统一/直接通信入口严格校验；空串口不再枚举或自动选择设备；待 Windows CTest 与真机 VAL-3 |
| MON-CFG-1 | 已完成，总控验收通过 | L1 | `applyConfiguration()` 已改为候选校验后集中交换 runtime channel/provider/backend 映射；非法候选保留旧状态，待 Windows CTest/真机 VAL-1、VAL-3 |
| OPC-1 | 代码实施完成，Windows/Matrikon 阻断 | L1 | start 分阶段失败传播、online/degraded/offline 快照、quality/null/master/per-item HRESULT 和回调矩阵已补；待 VAL-1/VAL-2 |
| MON-IO-1 | 真机阻断 | L0 | 先测量同步 backend I/O 与 UI 阻塞；无证据不改生产代码 |
| NET-1 | 进行中（决策门仍阻断） | L1 | UDP 完整报文与地址输入边界已收口；待确认监听范围、多客户端、消息边界与 keep-alive 合同 |
| DB-RET-1 | 进行中 / L1 | L1 | UTC cutoff、runtime_data/system_logs 同事务清理与失败回滚已实施；待 CTest/目标规模验证 |
| EXP-CSV-1 | 进行中 | L1 | CSV/TSV 普通/分页、单/多通道统一字段编码与公式注入防护已实施；待 Qt/CTest 与目标规模验证 |
| SQL-LOG-1 | 进行中 | L1 | DataManager SQL 错误日志改为模板+参数类型/长度摘要，不记录绑定原值；待 Qt/CTest 验证 |
| BUILD-CANCEL-1 | 进行中 / L1 | L1 | `DSLCompilerInterface` 非阻塞取消、sender/generation 过滤与 `BuildController` Cancelling 单终态已实施；待 Qt/CTest、Windows UI 时序验证 |
| DB-QUERY-1 | 未开始 | L0 | 生产历史查询区分成功空结果、未初始化与 SQL error |
| COMM-LIFE-1 | 未开始，先验证 | L0 | 不预设旧响应污染；核实 timeout reply 生命周期、重入和并发合同 |
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
| VAL-1 | 阻断 | L0 | 需要 Windows clean configure/build/targeted CTest/full CTest |
| VAL-2 | 阻断 | L0 | 需要 Windows + Matrikon OPC DA |
| VAL-3 | 阻断 | L0 | 需要真实控制器、串口和正式/诊断下载链 |
| VAL-4 | 阻断 | L0 | 需要目标规模数据库和长时间导出环境 |

当前 v4.2 合同下仍有可在 macOS 静态闭环的 P1/P2 任务。后续派发顺序以需求合同第 7 节为准；本表中的历史 L1/N/A 行只保存完成证据，不是待实施任务。

## 3. 已完成工作的关键合同

### 3.1 RuntimePoint、参数与通信

- RuntimePoint 寄存器 codec 统一处理 BOOL、整数和 REAL/FLOAT32，严格校验字符串、范围、寄存器宽度、byte/word order、scale/offset；非法值不得 fallback 为 0。
- `elementCount` 表示逻辑元素数，寄存器数量由目标类型宽度推导。
- 显式 `slaveId/stationAddress/serverAddress` 优先于默认 `unitId=1`；冲突或非法 alias 在 I/O 前 hard-fail。
- mapped access violation 返回 `PermissionDenied`，transport error 保留原错误，不得覆盖为地址或参数错误。
- Modbus 外层等待超时固定为 `ReceiveTimeout`；所有失败路径禁止留下 `CommErrorCode::NoError`。
- Serial/Modbus/Ethernet 配置统一使用严格整数解析与枚举白名单；端口在窄化前校验 `1..65535`，非法 timeout/buffer 和未知显式 mode/role 在 I/O 前返回 `InvalidConfig`。
- 空串口名不再调用设备枚举或自动选择第一台设备；Serial 原有 `baudRate=0` 自动波特率检测仍保留。
- 参数批量提交允许成功点继续回读，最终结果以本次目标集合计算；BOOL/整数/REAL 按目标类型比较，REAL 使用 float32 representation-aware 语义。
- 借用 backend 用 QPointer/destroyed guard 隔离异步回读；销毁或替换后 pending 必须一次失败收口，不发布 Good。
- OPC 单点写只作用于该 pointId；只有 readback `Confirmed` 后才能记录成功并发布新 Good 值。

### 3.2 历史数据、数据库与导出

- 历史查询使用 `(timestamp, id)` keyset 分页，并固定查询结束时间。
- 历史记录持久化 `quality`、`origin`、`error_code`、`error_text`；通信失败不能作为正常数值 0 传播。
- DataManager batch insert 保持事务语义；初始化必要 SQL、版本读取、required index、schema version 更新与 connection cleanup 均按 hard failure 处理。
- DB-RET-1 清理使用 UTC cutoff；`runtime_data` 与 `system_logs` 在同一事务中删除，任一 DELETE/提交失败回滚并返回 `-1`，失败不记录持久化清理完成；Monitor 清理定时器仅在 DataManager 已初始化且数据库记录启用时触发。
- 非整数、负数或未来 schema version 被拒绝，不新增 schema v5。
- CSV/JSON/TSV 大批量导出采用有界分页流式写入和 QSaveFile；provider、写入或 commit 失败保留旧文件。
- EXP-CSV-1 的 CSV/TSV 普通与分页导出共用字段编码：分隔符、双引号、CR/LF、空字段和 Unicode 可安全落盘；用户文本首字符为 `= + - @` 时加单引号，数值列不受影响；JSON 路径保持原语义。
- SQL-LOG-1 的统一 SQL 错误日志仅保留操作描述、prepared SQL 模板、参数名/类型/长度、native code 和必要错误文本；不记录 `executedQuery()` 扩展值、绑定原值或批量业务变量名。
- aligned CSV/TSV 是按时间戳对齐视图，不承诺同一 channel/timestamp 多条 Sample 逐条无损；行式导出承担逐 Sample 无损语义。

### 3.3 Monitor、输出保存与结构拆分

- MonitorChannel 的公开 config getter、threshold 读取和 samples 修改统一受 mutex 保护。
- threshold evaluation 使用锁内复制的配置快照，解锁后判断并 emit，不在持锁状态触发外部可重入 signal。
- `MonitorManager::applyConfiguration()` 先构建和校验无副作用候选，成功后一次交换 runtime channel/provider/backend 映射；失败不停止旧监控、不发集合变更信号，成功只发一次 `channelsChanged`，并保留旧 point id 的最后通道映射语义。
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
- BUILD-CANCEL-1 已在一次性最小授权范围内完成 macOS 静态实施：异步 QProcess 取消不再同步等待，旧 sender 与迟到 generation 回调不能改变新编译状态；最小取消/新 generation 测试已注册，待目标环境运行。
- DL-1A 的正式下载仍为同步调用。诊断链可取消不等于正式链可即时取消；只有 VAL-3 提供真实 UI/设备失败证据后才允许定义 DL-1B。

### 3.5 Windows/构建前置整理

- 当前产品基线是 Qt 5.15+，不支持 Qt6 构建；communication 子模块不得自行选择另一 Qt major。
- target 名称是 `LH`，`build.bat`、`run_platform.bat`、`tools/workflow_dev.ps1` 均定位 `bin\LH.exe`。
- monitor 不再使用 host-specific `-march=native`，依赖 CMake/toolchain 标准 Release flags，warning flags 保留。
- `Invoke-Logged` 在 `WaitForExit()` 前同时启动 stdout/stderr `ReadToEndAsync()`，避免一侧管道填满造成父子进程互等；输出仍写控制台和报告，非零退出仍失败。

## 4. 当前阻断与未验证项

### 4.1 OPC-1 的 Windows 验证仍阻断

OPC-1 的源码实施已完成 macOS 可静态验收：阶段失败会进入状态快照，`isRunning()` 与 `online` 已分离，回调已覆盖 master/per-item HRESULT、更多 VARIANT 类型和 quality/null payload。macOS 静态阅读不能证明 COM 分支可编译，也不能替代 Matrikon 行为；仍需 Windows clean build、Matrikon 联调及 stop/reconnect 后迟到回调验证。

### 4.2 L1 不能写成运行通过

当前 C++ 工作树没有对应的 clean configure/build/CTest 证据。必须在目标环境补做：

- VAL-1：Windows clean configure/build、targeted CTest、full CTest；
- VAL-2：Matrikon activation、browse、group/item、同步读写、订阅、断线重连和 late callback；
- VAL-3：真实寄存器编码、设备号 alias、端口互斥、retry/reconnect、取消和 artifact generation 消费；
- CFG-1：Windows 运行 communication routing/serial interface tests，并用真实串口确认合法配置、自动波特率与失败前无误开设备；
- VAL-4：大数据库分页稳定性、内存上界、吞吐和导出失败保留旧文件；
- BUILD-CANCEL-1：Qt/CTest 或 Windows UI 验证取消前、进程启动后、临近完成、立即重启及迟到 finished/error 回调隔离；
- WIN-B3：同时大量 stdout/stderr 且 child 非零退出的 PowerShell 实测；
- BUILD-B1：目标部署 CPU 运行，确认无 illegal instruction。

### 4.3 当前工作树口径

当前工作树包含 CFG-1 的 5 个通信源码、2 个既有通信测试、MON-CFG-1 的 MonitorManager 与 backend 回归测试、DB-RET-1 的 DataManager/MonitorManager 与数据库回归测试、EXP-CSV-1 的 MonitorExportHelper 与导出回归测试、SQL-LOG-1 的 DataManager 与错误日志回归测试、BUILD-CANCEL-1 的 BuildController/DSLCompiler 异步取消实现与最小回归测试，以及本需求合同和 Handoff 更新。后续仍不得为“清理”执行全局回退、恢复或格式化。

## 5. 下一步计划

1. DB-RET-1、EXP-CSV-1、SQL-LOG-1 与 BUILD-CANCEL-1 已完成 macOS 静态实施；继续处理共享 DataManager 的 DB-QUERY-1。
2. COMM-LIFE-1 先做生命周期/重入验证；没有证据不得重写 ModbusInterface。
3. NET-1 已完成不依赖部署决策的 UDP 报文/地址边界收口；仍需先确认服务端部署与外部 API 边界，再决定 TCP 多客户端、广播/定向发送、分帧与 keep-alive。
4. 准备 Windows 新环境执行 VAL-1，并在 Windows/Matrikon 环境完成 OPC-1 的 L2/L3 验证与 VAL-2。
5. 使用真实控制器完成 MON-IO-1、VAL-3；使用目标规模数据完成 VAL-4。
6. 每项完成后从需求合同任务表删除，在本文件和 Git 历史保留证据。

## 6. 绝对不要再踩的坑

1. 不要把子智能体完成报告当成总控验收；必须复核 scoped diff、直接调用链和失败路径。
2. 不要把 L1 写成“编译/测试通过”，也不要使用旧 build、旧 CTest 注册或旧二进制作证据。
3. 不要回退或格式化整个脏工作树；只操作当前任务精确授权文件。
4. 不要恢复 v3/v4/v4.1 的旧任务状态；以 v4.2 合同边界和本文件当前完成证据为准。
5. 不要重复执行 CFG-1、MON-1、P3-1、WIN-B2、COMP-1、DL-1A、已确认决策或本轮已达到 L1 的 correctness/build 批次。
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
3. 若仍在 macOS，从 DB-QUERY-1 开始；条件性任务必须先完成合同规定的调查或决策门。
4. 若已在 Windows，仍先完成当前跨平台逻辑任务，再使用全新 build 目录执行 VAL-1；只修真实 blocker，不顺带做 Qt6 migration 或结构整理。
5. 每批完成后更新本文件的任务状态、验证等级、证据与剩余阻断。
