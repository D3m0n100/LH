# LH 项目整改交接

更新时间：2026-08-19

## 1. 当前目标与验收口径

当前按 `LH_current_tasks_requirements_v3.md` 分批整改 OPC/监控线程模型、运行数据完整性、历史查询与导出、artifact 安全发布、下载连接生命周期及测试体系。

项目规则以根目录 `AGENTS.md` 为准，本文件不重复通用委派规则。当前环境默认只做静态验收；缺少 Windows、Qt/CMake/CTest、Matrikon 或真实控制器时，结论最高为 **L1：静态验收通过，编译与运行验证待补**。

## 2. 需求合同任务状态

| 任务 | 当前状态 | 验收等级 | 说明 |
|---|---|---:|---|
| EXP-1 | 已完成，总控验收通过 | L1 | 普通 CSV/JSON/TSV 导出 commit failure 保留旧文件回归已补齐 |
| HIST-B1 | 已完成，总控验收通过 | L1 | DB → Sample → 分页流式导出的 quality/error/origin 完整性保护已补齐 |
| EXP-D1 | 决策已完成并确认 | N/A | 逐 Sample 无损导出与 aligned 视图分开；不把同时间戳碰撞误当成 HIST-B1 缺陷 |
| OPC-D1 | 决策已完成并确认 | N/A | master/per-item HRESULT、null payload、quality failure 和 callback generation 合同已确定 |
| DL-D1 | 决策已完成并确认 | N/A | 正式项目下载链与诊断下载链的产品职责已确定 |
| COMP-S1 | 已完成并确认 | N/A | artifact bundle、generation、commit point 和可移植路径合同已追踪 |
| COMP-1 | 已完成，总控静态验收通过 | L1 | generation staging、一致性发布、manifest commit point 和可移植路径已实现 |
| OPC-1 | 未实施，目标环境阻断 | L0 | 合同要求 Windows Qt/CMake 条件分支可编译后再写/验收 |
| DL-1 | 第一实施轮完成，总控静态验收通过 | L1 | 连接 owner、端口互斥、DeviceBusy、后端操作互斥及诊断取消已收敛 |
| P3-1 | 未实施，仍阻断 | L0 | 等前述功能稳定且 Windows clean build 恢复后重新评估 |
| VAL-1 | 未执行 | L0 | 需要 Windows clean configure/build/CTest |
| VAL-2 | 未执行 | L0 | 需要 Windows + Matrikon OPC DA |
| VAL-3 | 未执行 | L0 | 需要真实控制器和串口环境 |
| VAL-4 | 未执行 | L0 | 需要目标规模数据库和长时间导出环境 |

## 3. 已完成工作的当前合同

### 3.1 历史数据与导出

- 历史查询使用 `(timestamp, id)` keyset 分页，并固定最近数据查询结束时间。
- 历史记录持久化 `quality`、`origin`、`error_code`、`error_text`；通信失败不再作为正常数值 `0` 传播。
- CSV、JSON、TSV 大批量导出采用有界分页流式写入和 `QSaveFile`；provider、写入或 commit 失败时保留旧文件。
- 普通非分页导出的 commit failure 已有直接回归。
- `tests/monitor_history_export_integration_test.cpp` 覆盖 DB → Sample → 分页导出链，`tests/monitor_export_test.cpp` 覆盖普通和流式导出失败语义。
- aligned CSV/TSV 是按时间戳对齐的视图，不承诺在同一 channel、同一 timestamp 出现多条 Sample 时逐条无损；逐 Sample 无损需求由行式导出承担。

### 3.2 配置与测试体系

- 配置 schema、字段类型、provider/映射/controller/OPC/transport 参数边界和可定位错误提示已加强。
- 配置加载失败不会覆盖当前项目路径、modified 状态或有效运行配置。
- `dsl_script_editor_save_test`、core DataManager/TaskScheduler 测试已纳入构建。
- Python DSL 编译语义测试已补齐，未修改第三方/Python DSL 编译语义；环境依赖缺失返回 77，超时仍为失败。
- 测试 fixture 使用临时目录，避免旧 artifact 造成假绿或假红。

### 3.3 OPC 合同

OPC-D1 已确认以下方向，OPC-1 后续测试必须以此为准：

- master callback 失败与 per-item HRESULT 分层处理；单点失败不能伪装为整批 Good。
- null/不可转换 payload、bad/uncertain quality 必须保留失败或质量语义，不能生成正常值。
- callback 必须绑定 owner/context 和 generation；stop 后旧 callback、旧 generation callback 不得污染当前状态。
- 当前仓库已有部分 Windows 条件保护，但尚未完成 OPC-1 的目标平台编译与最终测试矩阵。

### 3.4 Artifact bundle 与正式消费者

- `RuntimeSessionController -> ControllerDeviceBackend` 是唯一正式项目下载链和正式 artifact consumer。
- `DownloadDockWidget -> DownloadManager -> ControllerBridge` 保留为工程/诊断/手工下载工具，不作为正式项目 artifact consumer。
- 一次编译的正式产物按 generation bundle 发布；新 generation 在隔离位置完成并校验后，以 `runtime_manifest.json` 作为提交点。
- consumer 只读取已提交且完整的 generation，发布失败时旧 generation 保持可用。
- 新写入使用 manifest-relative/project-relative 路径；阻止 absolute path、`..` 或越出允许 root 的路径成为正式下载输入。
- legacy absolute path 只在规范化后仍位于允许 root 内时兼容；工程外绝对路径不再静默成为正式依赖。
- COMP-1 获得过用户对冻结目录的狭窄授权；该授权只覆盖已实施批次，不代表后续可继续任意修改 `src/compiler/**`。

主要实现与回归位于：

- `src/compiler/DSLCompilerArtifacts.cpp`
- `src/compiler/DSLCompilerAsync.cpp`
- `src/compiler/DSLCompilerInterface.cpp`
- `src/designer/RunController.cpp`
- `src/designer/RuntimeSessionDownload.cpp`
- `src/communication/ControllerDeviceBackendDownload.cpp`
- `tests/dsl_legacy_compile_probe.cpp`
- `tests/project_save_close_test.cpp`
- `tests/runtime_session_controller_test.cpp`

### 3.5 DL-1 第一实施轮

- 正式后端与诊断 worker 共用进程内 RTU port owner；错误 owner 不能释放占用。
- Windows 端口名按大小写不敏感处理，Unix/macOS 保留大小写。
- 正式链占用端口时诊断链立即返回 `DEVICE_BUSY`；诊断链占用时正式后端返回 `CommErrorCode::DeviceBusy` 并报告 owner，不强制抢占或断开。
- `ControllerDeviceBackend` 的连接、点位 IO、调试操作和正式下载使用操作级互斥；Monitor 轮询与下载不能同时访问设备。
- `RuntimeSessionController::ensureControllerBackend()` 统一通过 `setDeviceBackend()` 建立信号和 MonitorManager 绑定。
- `requestStop()` 不再断开注入/借用的 backend；只有 owned formal backend 由 RuntimeSession 负责 disconnect/reconnect。
- 正式 transport retry 的内部重连有明确门控，真实下载中掉线仍进入 `Fault/TransportFailed`。
- 诊断下载取消使用独立共享取消句柄，不再依赖被同步 worker 阻塞的 queued slot，也不再跨线程解引用可能已析构的 worker。
- 已补端口 owner、错误 owner release、busy fast-fail、操作锁释放、借用后端停止、下载中真实断线和诊断取消回归。

DL-1 当前仍有明确边界：正式项目下载仍是同步调用，`requestStop()` 只能保护调用前后和状态恢复，尚未提供可从 UI 即时中断正在执行的正式下载。不要把诊断链已修复的中途取消能力误写成正式链也已异步可取消。

## 4. 当前阻断与未验证项

### 4.1 OPC-1 不能在当前 macOS 静态环境闭环

合同明确要求 Windows 条件分支可编译后再实施/验收 OPC-1。剩余内容包括 master/per-item HRESULT 矩阵、更多 `VARIANT` 类型、quality/null payload 和 callback generation 竞态测试。macOS 静态阅读不能替代 Windows 编译，更不能替代 Matrikon 联调。

### 4.2 所有已完成代码批次仍缺 L2/L3

当前没有本轮源码对应的 clean configure/build/CTest 结果。旧构建目录不能证明当前测试已注册或已通过。以下必须在目标环境补做：

- Windows clean build 和完整 CTest（VAL-1）；
- Matrikon activation、browse、group/item、同步读写、订阅、断线重连、停止后迟到回调（VAL-2）；
- 正式/诊断端口互斥、下载 retry/reconnect、真实掉线、取消和 artifact generation 消费（VAL-3）；
- 大数据库分页稳定性、内存上界、吞吐及失败保留旧文件（VAL-4）。

### 4.3 DL-1 后续是否扩展需要真机证据

第一轮没有把正式下载改造成异步任务，也没有合并两套下载实现。若 VAL-3 证明同步正式下载导致 UI 无法及时取消，再单独定义 DL-1 第二轮文件清单；不要为了形式统一提前重写 backend 或删除诊断工具。

### 4.4 P3-1 继续阻断

`MainWindow`、`MonitorManager` 的 implementation split、未接入 controller 清理和文档收口都应等 Windows clean build 恢复后再评估。`MonitorController`、`OutputPaneController` 可能存在仓库外 API 消费者，未经确认不得直接删除。

### 4.5 工作树不是干净基线

当前工作树同时包含已验收批次、用户已有修改、删除的历史工作文档以及未跟踪文件。不得根据整个 `git status` 推断某一批的改动来源，不得为“清理”执行回退、恢复或全项目格式化。

## 5. 下一步计划

1. **VAL-1：Windows clean configure/build/CTest**
   - 重新配置构建目录，不复用旧 `CTestTestfile.cmake` 作为证据。
   - 优先构建并运行本合同新增或受影响的导出、历史集成、Compiler artifact、RuntimeSession、ControllerDeviceBackend、DownloadManager 和 OPC 测试。
2. **OPC-1：在 Windows 编译条件恢复后实施**
   - 重新确认精确测试文件；按 OPC-D1 矩阵补测试并完成总控验收。
3. **VAL-2 / VAL-3**
   - 分别完成 Matrikon OPC DA 和真实控制器验证，记录设备、驱动、串口参数、复现步骤和结果。
4. **根据 VAL-3 决定是否需要 DL-1 第二轮**
   - 只有正式下载即时取消或配置切换确有失败证据时，才扩展为异步正式下载或更严格的连接重绑定。
5. **VAL-4 后再评估 P3-1**
   - 只做有回归保护、能降低真实维护成本的定向拆分；不为行数或外观重构。

## 6. 绝对不要再踩的坑

1. 不要把子智能体报告当成总控验收；必须检查本批差异、失败路径和测试是否真的覆盖声称的语义。
2. 不要把 L1 写成“测试通过”；当前所有 C++ 改动仍需 clean build/CTest，Windows/硬件行为仍需 L2/L3。
3. 不要使用旧构建目录、固定输出目录或残留 artifact 证明当前代码正确。
4. 不要回退或格式化整个脏工作树；只审查和修改本批精确授权文件。
5. 不要把 COMP-1 的一次性冻结目录授权扩展到后续任务，也不要修改第三方/Python DSL 编译语义。
6. 不要再讨论下载链产品定位：正式 consumer 与诊断工具职责已经确认；后续只验证和收紧生命周期。
7. 不要让错误 owner release 端口，不要让借用 backend 被 RuntimeSession stop/retry 断开，也不要把内部重连门控用于掩盖真实掉线。
8. 不要把诊断下载可取消等同于正式同步下载可即时取消。
9. 不要在 macOS 上宣称 OPC Windows 分支已编译，更不要把单元测试等同于真实 Matrikon 验收。
10. 不要在 clean build 恢复前启动 P3 文件拆分，也不要未经外部 API 确认删除未接入 controller。

## 7. 接手后的第一步

1. 完整阅读根目录 `AGENTS.md` 和当前需求合同。
2. 查看本文件第 2 节任务状态，不再使用旧 `findings.md`、`progress.md`、`task_plan.md` 恢复状态。
3. 查看工作树但不回退任何现有修改；按具体任务建立精确授权清单。
4. 当前首选动作是准备 Windows VAL-1；若目标环境仍不可用，不要用重复静态扫描冒充进展。
5. 每批完成后更新本文件的任务表、验证等级、未验证项和下一步，不复制整份需求合同。
