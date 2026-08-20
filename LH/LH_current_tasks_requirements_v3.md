# LH 项目当前阶段任务需求合同

> 版本：v3（2026-08-17）  
> 适用仓库：`D3m0n100/LH`  
> 适用阶段：Windows 编译/运行验证环境恢复前  
> 文档性质：**当前阶段唯一任务需求合同**。旧的 `task_plan.md`、`progress.md`、`findings.md`、`Handoff.md` 不再持续维护，只可作为历史背景参考，不得据其旧状态重复实现已存在功能。  
> 源码基线：`main@bde155ac2899672717fa68dd391d302976b9284b`。后续执行某批次时若相关源码已变化，按本文件第 4.2 节执行基线漂移检查。

---

## 1. 文档目标、工作流状态与验证等级

本文件用于明确：

- 当前已经完成且不得重复实现的基线；
- 当前允许实施的任务；
- 每个任务的执行顺序、风险等级、模型强度、授权范围与验收口径；
- 当前无法在 macOS 环境闭环验证、必须等待 Windows/Matrikon/真实控制器环境的事项；
- 需要先做产品决策或用户授权、不得直接进入代码修改的决策门。

### 1.1 工作流状态

任务本身的推进状态只使用：

1. `未开始`
2. `进行中`
3. `阻断`
4. `已完成`

说明：

- “决策任务”在决策合同被用户确认后可标记为 `已完成`；
- “代码任务”在授权范围内实现并完成静态验收后，可标记为 `已完成`，但这**不代表**已经完成 Windows 或目标环境验证；
- 环境不可用、缺少用户授权、前置产品合同未确认时使用 `阻断`。

### 1.2 验证等级

验证成熟度与工作流状态分开记录：

- `L0：未实施/未验证`
- `L1：代码完成，静态验收通过`
- `L2：Windows clean build + 相关 CTest 通过`
- `L3：目标环境运行验证通过`

当前环境下：

- 普通代码任务最高只能达到 `L1`；
- Windows 条件代码在当前环境只能做源码静态审查，不能视为已经证明其 Windows 分支可编译；
- Matrikon OPC DA、真实控制器下载、大数据库真实性能等只有达到 `L3` 才能最终关闭目标环境风险；
- 纯产品/架构决策任务的验证等级记为 `N/A`。

### 1.3 当前阶段统一验收措辞

在 Windows 验证恢复以前，代码任务只允许使用：

> **静态验收通过，编译与运行测试需要验证。**

涉及 `Q_OS_WIN` 的任务更精确地写为：

> **源码静态审查完成；Windows 条件分支的编译与运行验证待补充。**

不得使用：

- “测试已通过”；
- “Windows 已验证”；
- “OPC 已联调通过”；
- “实机下载已验证”；
- 其他暗示运行环境已经验证的表述。

## 2. 当前环境与平台约束

### 2.1 当前环境

当前工作环境为 macOS，但本阶段：

- 不在 macOS 上编译 LH；
- 不执行 macOS CMake configure/build；
- 不执行 macOS CTest；
- 不为了让 LH 在 macOS 编译而引入平台兼容性修改。

### 2.2 当前暂不可用环境

当前无法完成：

- Windows clean configure/build；
- Windows CTest；
- `Q_OS_WIN` 条件分支的真实编译验证；
- Matrikon OPC DA COM 实际联调；
- 真实控制器下载；
- 串口/Modbus/设备实际联调；
- 大数据量真实性能与内存测量。

因此，涉及 Windows 条件代码的任务只能做到：

> **源码静态审查完成；Windows 条件分支的编译与运行验证待补充。**

---

## 3. 已完成基线：不得重复实现

接手者必须先以当前源码确认以下能力仍然存在；确认后不得因历史工作文档落后而重复实现。

### 3.1 历史数据与 DataManager

当前已存在：

- 数据库 Schema v4；
- `quality`；
- `valueValid`；
- `origin`；
- `errorCode`；
- `errorText`；
- `(timestamp, id)` keyset pagination；
- history page 与数据库错误状态分离；
- latest history 固定 `end` 上界能力；
- history count API。

### 3.2 MonitorManager 历史读取

当前已存在：

- `historyFromDatabasePage()`；
- `historyFromDatabaseLatestPage()`；
- history count 接口；
- `RuntimeRecord -> Monitor::Sample` 转换；
- `quality/valueValid/id` 写入 Sample；
- `origin/errorCode/errorText` 通过 Sample metadata 保留。

### 3.3 Monitor 导出

当前已存在：

- CSV / JSON / TSV；
- 单通道普通导出；
- 多通道 package 普通导出；
- paged/stream export；
- page-size 有界分页；
- `QSaveFile`；
- `setDirectWriteFallback(false)`；
- provider failure 时旧文件保留测试；
- paged commit failure 时旧文件保留测试；
- Good/Bad/invalid value 导出覆盖。

当前普通 CSV/JSON/TSV **没有声明输出** `origin/errorCode/errorText`。这些字段当前属于 Sample metadata，不得在测试中擅自把它们当成现有导出 schema。

### 3.4 OPC/Matrikon

当前已有测试或实现保护至少包括：

- callback value；
- callback quality；
- callback timestamp；
- worker thread callback -> owner thread queued delivery；
- stop 后 queued callback 丢弃；
- callback generation/context 机制；
- Good / Stale / Bad 基本 quality 映射；
- sync read / write / probe 基础路径。

不得把下一阶段定义成“重新实现 OPC callback 架构”。

### 3.5 测试注册

当前源码已经包含一批已注册测试目标，包括但不限于：

- `CoreDataManagerTest`；
- `CoreTaskSchedulerTest`；
- `DslCompilerSemanticsTest`；
- `DslScriptEditorSaveTest`；
- history paging；
- monitor export；
- OPC server config；
- diagnostics；
- project/runtime/controller 相关测试。

旧构建目录、旧测试二进制或旧 CTest 缓存 **不能证明当前源码中的测试已经被最新构建注册**。Windows 环境恢复后必须使用 clean configure/build 重新确认。

### 3.6 MainWindow / RuntimeSession 已有拆分

当前项目已经存在：

- `MainWindow.cpp`；
- `MainWindowUi.cpp`；
- `MainWindowInspector.cpp`；
- `MainWindowExplorer.cpp`；
- `RuntimeSessionController.cpp`；
- `RuntimeSessionDownload.cpp`；
- `RuntimeSessionDebug.cpp`；
- `RuntimeSessionOpc.cpp`。

因此后续不得再派发模糊的“拆 MainWindow”任务，只能做明确、局部、行为不变的 implementation split。

---

## 4. 本合同特有的执行约束

通用的工作树保护、子智能体调度、写入并发、冻结范围和验收职责统一遵循项目根目录 `AGENTS.md`，本合同不重复维护。任务表中的风险与模型列只记录本阶段各任务的具体选择。

### 4.1 测试临时文件

测试不得使用：

- 固定仓库输出目录；
- 用户真实数据库；
- 固定系统临时文件名；
- 依赖上一轮测试遗留文件。

数据库、导出文件、Compiler artifact 测试统一优先使用：

```text
QTemporaryDir
```

或同等隔离的临时路径。

### 4.2 源码基线漂移

本合同制定时的源码基线为：

```text
main@bde155ac2899672717fa68dd391d302976b9284b
```

如果后续执行某批次时仓库已经前进：

- 不重新扫描全项目；
- 不因为 commit 变化就废弃整份合同；
- 只比较该批次授权文件、直接调用链、相关测试和当前基线之间的差异；
- 如果相关接口/行为未变化，继续按本合同执行；
- 如果相关接口/行为已经变化，先更新该批次的局部合同、授权清单和验收条件，再实施；
- 不根据四个历史工作文档恢复旧任务状态。

## 5. Compiler 授权门

本合同不授予任何冻结目录写权限。`COMP-S1` 只读；`COMP-1` 必须在调查完成后，由用户按最终最小文件清单单独授权。冻结目录及越权处理以项目 `AGENTS.md` 为准。

---

# 6. 唯一任务表

> **本表是唯一执行顺序来源。** 后文章节只解释各任务，不再维护第二套顺序。
>
> “依赖/决策门”一列只记录真实技术依赖、授权依赖或产品决策依赖；“因为同一共享目录只能串行写入”属于调度规则，不写成技术依赖。

| ID | 任务 | 执行顺序 | 实施风险 | 子智能体 | 真实依赖/决策门 | 初始授权范围 | 工作流状态 | 当前验证等级 | 最终运行验证 |
|---|---|---|---|---|---|---|---|---|---|
| EXP-1 | 普通非分页导出 commit failure 回归 | Now | Test-only | luna/xhigh | 无 | `tests/monitor_export_test.cpp` | 未开始 | L0 | Windows CTest -> L2 |
| HIST-B1 | HIST-1 + HIST-2：DB -> Sample 完整性与分页导出集成保护 | Now | 数据一致性 / P1 | luna/max | 无技术依赖；与 EXP-1 按单写入规则串行 | 新测试文件 + `tests/CMakeLists.txt` | 未开始 | L0 | Windows CTest -> L2 |
| EXP-D1 | 导出 schema 与 aligned 同时间戳碰撞语义决策 | Later | 产品合同 / P2 | luna/xhigh | 不阻塞 HIST-B1 的无损模式测试 | 只读分析；无代码授权 | 未开始 | N/A | 若实施则 Windows CTest |
| OPC-D1 | 明确 master/per-item HRESULT、null payload、quality failure 业务合同 | Next | 生命周期 / P1 | luna/max | 无 | 只读分析；无代码写入 | 未开始 | N/A | 不适用 |
| DL-D1 | 正式下载链与诊断下载链职责、唯一正式 artifact consumer 决策 | Next | 架构合同 / P2 | luna/xhigh | 无技术依赖；按任务表顺序在 OPC-D1 后处理 | 只读分析；无代码写入 | 未开始 | N/A | 不适用 |
| COMP-S1 | Compiler artifact bundle 输出链/staging/发布单元/路径合同追踪 | Next | 安全 / P1 | luna/max | DL-D1 已确认正式 consumer | 只读，冻结目录不得写 | 未开始 | N/A | 不适用 |
| COMP-1 | Artifact bundle staging + 一致性发布 + 可移植路径实现 | Blocked | 安全 / 数据丢失 / P1 | luna/max | COMP-S1 + bundle publication contract + 用户明确狭窄授权 | 以 COMP-S1 最终授权清单为准 | 阻断 | L0 | Windows build + CTest -> L2 |
| OPC-1 | HRESULT / VARIANT / Quality / generation Windows 条件测试 | Blocked | 生命周期 / P1 | luna/max | OPC-D1 已确认 + Windows Qt/CMake 编译环境可用 | 默认 `tests/opc_server_config_test.cpp`；实际写入前重新确认 | 阻断 | L0 | Windows 条件编译 + CTest -> L2 |
| DL-1 | 设备连接 ownership / 互斥 / 生命周期收敛 | Later | 生命周期 / 跨模块 P1 | luna/max | DL-D1 + COMP-1 的 consumer/path 合同稳定 | 实施前单独列文件 | 阻断 | L0 | Windows + 真机 -> L3 |
| P3-1 | MainWindow/MonitorManager 定向 implementation split | Blocked | P3 | luna/xhigh | 前述功能/安全批次稳定 + Windows clean build 环境恢复 | 实施前单独列文件 | 阻断 | L0 | Windows build + CTest -> L2 |
| VAL-1 | Windows clean configure/build/CTest | Blocked | 验收 | 不属于当前代码批次 | Windows Qt/CMake/CTest 环境 | 无代码修改 | 阻断 | L0 | 达到 L2 |
| VAL-2 | Windows + Matrikon OPC DA 联调 | Blocked | 目标环境验收 | 不属于当前代码批次 | Windows + Matrikon | 无代码修改 | 阻断 | L0 | 达到 L3 |
| VAL-3 | 真实控制器下载验证 | Blocked | 目标环境验收 | 不属于当前代码批次 | 控制器/串口环境 | 无代码修改 | 阻断 | L0 | 达到 L3 |
| VAL-4 | 大数据库/长时间导出性能 | Blocked | 性能验收 | 不属于当前代码批次 | 可运行目标规模环境 | 无代码修改 | 阻断 | L0 | 达到 L3 |

# 7. EXP-1：普通非分页导出 commit failure 回归

## 7.1 目标

证明所有公开普通导出入口在最终 `QSaveFile::commit()` 失败时：

- 明确返回失败；
- 不损坏已存在的目标文件；
- 不需要新增全局失败 hook；
- 不修改产品导出实现，除非测试明确暴露独立真实 bug。

## 7.2 公开入口范围

普通非分页导出存在六条直接指定文件路径的公开入口。

### 单通道

```text
exportDataAsCsvToFile
exportDataAsJsonToFile
exportDataAsTsvToFile
```

### 多通道 package

```text
exportPackageAsCsv
exportPackageAsJson
exportPackageAsTsv
```

这六条均视为公开合同。

## 7.3 测试方式

推荐使用**数据驱动测试**覆盖六条入口，避免复制六套近似代码。

现有：

```text
CommitFailingExportHelper
```

已经可以通过覆写 `commitSaveFile()` 注入失败，不新增全局 hook、不修改生产接口。

### 每条入口必须验证

```text
预先创建 old target file
        ↓
调用普通公开导出入口
        ↓
commitSaveFile() 注入失败
        ↓
ExportResult.success == false
        ↓
old target 仍存在且内容字节级不变
```

还需确认：

- 不存在部分替换后的最终文件；
- errorMessage 非空或符合现有失败合同；
- 现有正常导出测试仍覆盖正常路径。

如果现有正常测试已经充分覆盖六条入口，不重复添加冗余 success case；只补缺口。

## 7.4 授权范围

默认只修改：

```text
LH/tests/monitor_export_test.cpp
```

禁止默认修改：

```text
LH/src/monitor/MonitorExportHelper.cpp
LH/src/monitor/MonitorExportHelper.h
```

若测试暴露生产 bug：

1. 停止扩大修改；
2. 输出根因和影响；
3. 单独申请修复授权。

---

# 8. HIST-B1：历史数据完整性与当前导出 schema 集成保护

## 8.1 实施批次与目标拆分

`HIST-1` 与 `HIST-2` 保留为两个验收子目标，但作为**同一个写入实施批次 `HIST-B1`** 派发。两者共享 fixture、新测试文件与 `tests/CMakeLists.txt`，不为它们启动两个重复写入代理。

本批次不要求当前导出格式输出其未声明的 metadata。

### HIST-1：DB -> Sample 元数据完整性

验证：

```text
SQLite
 ↓
DataManager / RuntimeRecord
 ↓
MonitorManager
 ↓
Monitor::Sample
```

必须完整保留：

- `timestamp`；
- `value`；
- `quality`；
- `valueValid`；
- `unit`；
- `id`；
- `origin`；
- `errorCode`；
- `errorText`。

其中：

- `origin/errorCode/errorText` 当前验证到 **Sample metadata** 即为本合同终点；
- 不要求它们继续出现在 CSV/JSON/TSV 文件中。

### HIST-2：当前导出 schema 完整性

最终 CSV/JSON/TSV 只验证**当前格式已经声明支持**的字段，例如：

- `timestamp` / timestamp milliseconds；
- `channel`；
- `value`；
- `unit`；
- `quality`；
- `valueValid`。

不得在 HIST-2 中断言：

```text
origin
errorCode
errorText
```

必须存在于最终导出文件。

是否扩展 schema 由独立决策 `EXP-D1` 处理。

---

## 8.2 建议文件

新增：

```text
LH/tests/monitor_history_export_integration_test.cpp
```

修改：

```text
LH/tests/CMakeLists.txt
```

默认不修改：

```text
LH/src/core/DataManager.cpp
LH/src/monitor/MonitorManager.cpp
LH/src/monitor/MonitorExportHelper.cpp
```

---

## 8.3 单例与测试隔离合同

`DataManager` 与 `MonitorManager` 均存在全局/单例生命周期，因此测试必须显式清理。

### 每个测试或测试 fixture 必须

1. 使用独立 `QTemporaryDir`；
2. 数据库文件放入该临时目录；
3. `DataManager::initialize()` 前先确保上一连接已经 `shutdown()`；
4. cleanup 阶段再次 `DataManager::shutdown()`；
5. `MonitorManager` 不启动 `startMonitoring()`；
6. 不注册真实 backend；
7. 不启动真实 provider 采样；
8. 如测试注册 channel，结束后使用现有 `removeChannel()` 清除；
9. 如测试注册 provider，结束后使用现有 `unregisterProvider()` 清除；
10. 测试结束时确保 MonitorManager 处于停止/无测试资源残留状态；
11. 不依赖 sleep；
12. 不依赖系统当前时间；
13. 所有业务时间戳显式使用 UTC。

不得为了让测试方便而新增 production-only 的“test reset API”，除非现有公开生命周期能力确实不足，并且另行授权。

---

## 8.4 测试数据

至少使用多个 channel，例如：

```text
pressure
flow
temperature
```

必须覆盖：

- 不同 timestamp；
- 相同 timestamp、不同数据库 id；
- Good；
- Stale；
- Bad；
- `valueValid=true`；
- `valueValid=false`；
- `origin`；
- `errorCode`；
- `errorText`。

时间戳全部使用固定 UTC 值，例如：

```text
2026-01-01T00:00:00.000Z
2026-01-01T00:00:00.001Z
...
```

---

## 8.5 Keyset pagination 合同

统一使用一个小 page size（建议 `2`），并强制产生三页以上。多个 page size 的通用分页行为已有 DataManager 单元测试保护，本集成测试不重复覆盖。

必须确认：

- 无重复；
- 无遗漏；
- 总记录数一致；
- 顺序严格为：

```text
(timestamp ASC, id ASC)
```

必须显式覆盖多个记录拥有相同 timestamp 的情况。

---

## 8.6 固定 endTime 合同

禁止使用：

```text
记录 currentDateTimeUtc()
马上插入
```

这种依赖执行速度和时间精度的测试。

必须使用固定显式时间：

```text
固定 endTime = T
数据库中预先存在多页 timestamp <= T 的数据
第一页读取完成
插入 timestamp = T + 1 ms 的新记录
继续使用同一个 endTime = T 读取后续页
```

必须确认：

- `T + 1ms` 新记录不进入本轮结果；
- 原本 `<= T` 的后续记录仍全部返回；
- 不因插入新数据产生重复/漏行。

至少一个回归 case 必须直接走：

```text
MonitorManager::historyFromDatabaseLatestPage(..., fixedEnd = T)
```

并使用固定 `maxCount`、小 `pageSize`、多页数据。普通 `[start, end]` 分页测试不能替代该 latest-history 固定上界合同。

---

## 8.7 HIST-1：RuntimeRecord -> Sample metadata

至少逐条断言：

```text
RuntimeRecord.id        -> sample.metadata["id"]
RuntimeRecord.origin    -> Sample metadata
RuntimeRecord.errorCode -> Sample metadata
RuntimeRecord.errorText -> Sample metadata
RuntimeRecord.quality   -> sample.quality
RuntimeRecord.valueValid-> sample.valueValid
RuntimeRecord.timestamp -> sample.timestamp
RuntimeRecord.unit      -> sample.unit
```

核心业务要求：

> 数据库中的失败/无效/Bad/Stale 语义在转换成 Sample 时不能被降级成“普通有效数值 0”。

---

## 8.8 HIST-2：分页导出当前 schema

通过 DataManager / MonitorManager 的分页 API 适配 `ExportPageProvider`，再调用当前 paged export。

### A. 无损数据完整性模式

用于证明“数据库记录 -> Sample -> 导出文件”不丢记录的测试，必须避免 aligned 模式的时间戳碰撞语义干扰。

要求：

- JSON paged export：逐 Sample 验证；
- CSV/TSV paged export：显式设置 `alignMultiChannelByTime=false`；
- 数据中必须包含“相同 timestamp、不同数据库 id”的记录；
- 每条 Sample 均应在导出结果中有独立表示；
- 文件中的逻辑记录数与输入 Sample 数一致；
- 无重复；
- 无遗漏；
- 顺序与当前 non-aligned schema 合同一致；
- `quality` 正确；
- `valueValid=false` 正确保留；
- invalid value 不被输出成正常有效数值 `0`；
- unit/channel/timestamp 与当前 schema 一致。

### B. aligned CSV/TSV 当前模式

当前 `alignMultiChannelByTime=true` 的 CSV/TSV 是“按 timestamp 对齐”格式，而不是“一条 Sample 一行”的格式。

因此在 `EXP-D1` 决定“同一 channel + 同一 timestamp 出现多条 Sample 时应如何表示”之前：

- HIST-B1 不得使用重复 timestamp 去断言 aligned CSV/TSV “逐 Sample 无遗漏”；
- aligned CSV/TSV 的现有 schema 测试只使用每个 channel 内唯一 timestamp；
- 不把当前实现的“同时间戳碰撞时保留某一条”自动固化为长期产品合同；
- 如果未来产品要求 aligned 模式也逐 Sample 无损，则这是导出格式/语义变更，必须在 `EXP-D1` 决策后单独授权生产代码修改。

### C. 分页提供器约束

所有格式仍需验证：

- provider 被多次调用；
- 单次返回数量不超过 page size；
- page cursor 正确推进；
- 测试不依赖 sleep 或系统当前时间。

只需补一条 `DataManager/MonitorManager` 查询失败能够映射为 `ExportPage.success=false` 的链路断言；ExportHelper 通用 provider failure 与成功空页行为已有测试，不在本批次重复。

### 明确不属于本批次

以下内容不属于 HIST-B1：

```text
把 origin/errorCode/errorText 新增到 CSV
把 origin/errorCode/errorText 新增到 JSON
把 origin/errorCode/errorText 新增到 TSV
修改 aligned 模式对同一 channel + 同一 timestamp 多条 Sample 的产品语义
```

# 9. EXP-D1：导出 schema 与 aligned 碰撞语义决策

这是独立**产品合同决策**，不是 HIST-B1 的测试缺陷修复。

需要决定两类问题。

## 9.1 是否扩展诊断字段

需要决定：

1. CSV 是否新增 `origin/error_code/error_text` 列；
2. JSON 是否新增同等字段；
3. TSV 是否新增同等字段；
4. 单通道与多通道格式是否都扩展；
5. 是否需要 schema/version 标记；
6. 对现有消费者是否构成兼容性变化；
7. aligned multi-channel CSV/TSV 应如何表示每个通道的错误元数据。

## 9.2 aligned 模式同时间戳碰撞

必须明确：

当 `alignMultiChannelByTime=true` 且同一 channel 在同一 timestamp 存在多条 Sample 时，产品合同究竟是：

- 明确规定 last-wins/first-wins；
- 拒绝这种输入并返回错误；
- 扩展 schema，使同一 timestamp 能表达多条 Sample；
- 或取消该模式的逐 Sample 无损承诺，仅把它定义为“时间对齐视图”。

在该决策确认以前，不得用测试把当前偶然实现行为固化为长期合同。

## 9.3 决策前限制

在用户明确决定以前：

- 不修改 `MonitorExportHelper.cpp/.h`；
- 不把 metadata 未导出写成 HIST-B1 测试失败；
- 不把 aligned duplicate-timestamp 行为写成强制产品预期；
- 不扩大 HIST-B1 批次。

若决定扩展或改变 aligned 语义，另建独立产品变更批次并单独授权。
# 10. OPC-D1：必须先明确的 callback 业务合同

## 10.1 为什么必须先决策

当前 callback payload 已保存：

- `masterQuality`；
- `masterError`；
- per-item error；
- values；
- qualities；
- timestamps。

但现有 apply 路径主要按 per-item error 更新值，不能直接把“当前实现行为”自动定义成最终业务语义。

因此 OPC-1 测试之前必须先确认以下合同。

---

## 10.2 推荐合同（待用户确认）

> 本节是**推荐默认语义**，在用户确认前不得据此修改生产代码。

### A. `FAILED(masterError)`

推荐：

- 整批 callback 的 value 更新不可信；
- 不使用某个 per-item `S_OK` 覆盖 master failure；
- 不覆盖已保存的最后有效值；
- 记录整体 read/callback failure 诊断；
- 不把失败批次发布为新的正常 RuntimePointValue。

### B. `SUCCEEDED(masterError)`，部分 per-item error 失败

推荐：

- 成功 item 正常更新；
- 失败 item 不更新 value/timestamp；
- 保留该 item 上一次有效状态；
- backend diagnostic 记录 item failure；
- 失败 item 不污染成功 item。

当前 `RuntimePointValue` 本身没有 `valueValid/errorCode/errorText` 字段，因此**不要在本批次擅自扩展 RuntimePointValue 数据模型**。如果产品希望“发布新的无效 RuntimePointValue”，必须作为另一项模型合同变更讨论。

### C. `FAILED(masterQuality)`

需要明确 quality 字段不可被无条件信任。

推荐：

- value 是否可更新由 masterError + per-item error + value payload 决定；
- quality 不使用无效 master quality 数据冒充 Good；
- 可将本次 quality 视为 `Unknown`，同时记录诊断；
- 不把缺失 quality 的默认 `0` 自动等价成一个真实 OPC Bad 状态，除非协议合同明确要求。

此项可能需要生产实现调整，必须在确认后才能写测试预期。

### D. `pErrors == nullptr`

推荐：

- 若 `masterError` 成功，则按“无 per-item error 数组”处理，不凭空制造 item failure；
- 若 `masterError` 失败，则整批仍按 A 处理。

### E. `pvValues == nullptr` 或单项 value 无法转换

推荐：

- 不覆盖旧值；
- 不把 invalid QVariant 当作一次正常成功更新；
- 记录 callback payload/value conversion failure。

### F. `pwQualities == nullptr`

推荐：

- 不将缺省 `0` 无条件视为真实 Bad quality；
- quality 使用 `Unknown` 或按已确认合同处理；
- value 是否更新与 quality 缺失分开判断。

### G. 无效 FILETIME

推荐维持现有兼容策略：

- value 可以正常更新；
- timestamp fallback 到 callback 接收时刻；
- 诊断/来源能够区分 OPC timestamp 与 local fallback。

### H. `phClientItems == nullptr` / client handle 无法匹配

推荐：

- 不更新任何未知 point；
- 不错误映射到默认 point；
- 记录 unmatched/invalid callback diagnostics。

---

## 10.3 OPC-D1 输出

用户确认后，必须形成一张确定的 callback 行为矩阵，至少包含：

| masterError | masterQuality | itemError | value | quality | timestamp | 期望行为 |
|---|---|---|---|---|---|---|
| success | success | success | valid | valid | valid | 正常更新 |
| success | success | failure | any | any | any | 按确认合同 |
| failure | any | success | valid | valid | valid | 按确认合同 |
| success | failure | success | valid | unavailable | valid | 按确认合同 |
| success | success | success | null/invalid | valid | valid | 按确认合同 |
| success | success | success | valid | valid | invalid | 按确认合同 |
| success | success | no pErrors | valid | valid | valid | 按确认合同 |

只有该矩阵确认后，OPC-1 才可进入写测试阶段。

---

# 11. OPC-1：HRESULT / VARIANT / Quality / generation 边界测试

> 当前工作流状态：**阻断**。  
> 前置：`OPC-D1` 已确认，并且 Windows Qt/CMake 编译环境恢复。当前阶段可以完成测试矩阵设计和只读接口核对，但不在无法编译 `Q_OS_WIN` 分支的环境中直接写入并宣称静态闭环。

## 11.1 默认授权范围

优先只修改：

```text
LH/tests/opc_server_config_test.cpp
```

如果测试文件规模已经明显阻碍维护，可在派发前重新申请：

```text
LH/tests/matrikon_opc_backend_test.cpp
LH/tests/CMakeLists.txt
```

默认不重构生产 OPC 实现。

如果确认后的业务合同与现有实现不一致：

1. 测试先体现已确认合同；
2. 明确指出生产实现差异；
3. 单独列生产修复文件；
4. 获得授权后再改；
5. 不把“测试无法通过”误当成测试设计问题。

## 11.2 HRESULT 覆盖

至少覆盖：

- master success + all item success；
- master success + partial item failure；
- master failure + item success；
- master failure + item failure；
- `pErrors == nullptr`。

验证内容以 OPC-D1 的最终矩阵为准。

## 11.3 VARIANT 类型矩阵

只覆盖 `variantToQVariant()` 当前已支持类型，不为了测试扩大功能。

候选：

```text
VT_I2
VT_I4
VT_UI2
VT_UI4
VT_R4
VT_R8
VT_BOOL
VT_BSTR
```

执行前以当前实现实际支持集合为准。

每种类型验证：

- QVariant 类型；
- 值；
- 边界/符号行为（适用时）。

## 11.4 Quality 边界

至少覆盖：

- Good 类子状态；
- Uncertain/Stale；
- Bad；
- limit bits；
- substatus bits；
- masterQuality failure；
- null quality payload。

只验证当前系统实际使用的分类合同，不穷举 OPC DA 全规范。

## 11.5 late callback / generation isolation

必须覆盖：

```text
start generation 1
 ↓
callback A 已入队
 ↓
stop / context deactivate
 ↓
start generation 2
 ↓
generation 1 的 callback A 最后才执行
```

必须确认：

- generation 1 不污染 generation 2；
- stop 后旧 callback 不修改当前状态；
- owner/context 失效不会 use-after-free；
- 新 generation 的正常 callback 仍可被接受。

## 11.6 当前验收限制

由于测试主体涉及 `Q_OS_WIN`：

> **源码静态审查完成；Windows 条件分支编译验证待补充。**

不得声称 macOS 静态检查等价于 Windows 测试可编译。

---

# 12. DL-D1：下载链职责与唯一正式消费者决策

此决策提前到 Compiler artifact 改造之前，因为 artifact 的持久化路径与验证合同应围绕正式消费者定义。

## 12.1 当前两条链

### A. 手工/诊断链

```text
DownloadDockWidget
 ↓
DownloadManager
 ↓
ControllerBridge
```

特点：

- 手工选择 profile；
- 手工选择 payload；
- 独立端口/连接逻辑；
- 更接近工程与诊断工具。

### B. 项目运行链

```text
RuntimeSessionController
 ↓
RunController artifact validation
 ↓
ControllerDeviceBackend
```

特点：

- 从当前项目 artifact 出发；
- precheck；
- state machine；
- retry；
- verify；
- diagnostics；
- dry-run；
- failure classification。

## 12.2 推荐产品定位（待用户确认）

推荐：

- `RuntimeSessionController -> ControllerDeviceBackend` = **唯一正式项目下载链 / artifact 正式消费者**；
- `DownloadDockWidget -> DownloadManager -> ControllerBridge` = **工程、诊断、手工下载工具**。

## 12.3 本决策必须明确

- 谁是正式 artifact consumer；
- 谁拥有 device connection；
- 谁拥有 COM/serial port；
- 谁允许 disconnect；
- 谁负责 reconnect；
- 正式下载时诊断工具是否禁用；
- 诊断工具占用设备时正式下载如何处理；
- Monitor 是否可与 Download 同时访问设备；
- 同一串口的互斥策略；
- retry/cancel/reconnect 的唯一生命周期 owner。

## 12.4 本阶段禁止

在 DL-D1 只做决策时，禁止直接：

- 删除 `DownloadManager`；
- 删除 `ControllerBridge`；
- 让 `DownloadDockWidget` 强行调用 `RuntimeSessionController`；
- 重构全部 backend；
- 同时修改两条链。

---

# 13. COMP-S1：Compiler artifact bundle 输出链只读追踪与授权清单

> `src/compiler/**` 与 `third_party/custom_dsp_language/compile/**` 均冻结。  
> 本步骤只读，不获得授权不得写。

## 13.1 目标

Compiler 当前不是只产生一个 `.code` 文件，而是存在一组相互关联的 artifact。必须把**一次编译生成的一整代 artifact bundle**作为调查对象，而不是只追踪 `.code`。

至少追踪：

```text
C++ 调用 Compiler
 ↓
传给 Python/第三方编译器的输出路径
 ↓
.code
.list
.typ
.rep
 ↓
runtime_points.json
 ↓
runtime_manifest.json
 ↓
CompileResult / artifact metadata
 ↓
RunController / project config 持久化
 ↓
正式下载链读取 / validate
```

重点确认：

- Python/第三方编译器实际会生成哪些文件；
- `.list/.typ/.rep` 是否跟随 `-o` 路径、输出目录或其他命名规则生成；
- 当前 Python 编译器是否先直接写 final `.code`；
- 哪个调用点可以让**整套生成物**进入独立 staging generation；
- 是否无需修改第三方 Compiler 即可改变输出目标；
- 如果必须进入第二冻结目录，必须单独阻断并请求授权；
- 哪些测试直接断言 `.code` 或 sidecar 路径；
- manifest、project config、RunController 各自以什么路径基准解析 artifact；
- 当前 consumer 是否可能在多文件连续覆盖过程中观察到“新 `.code` + 旧 manifest/points”或其他代际混用。

## 13.2 必须形成 Artifact Bundle Publication Contract

COMP-S1 的核心输出不是“怎么给 `.code` 加 QSaveFile”，而是：

> **一次 Compile 的全部正式产物，什么构成同一发布代，以及 consumer 通过什么提交点确认该代完整可用。**

必须比较并选择最小可行策略，例如：

### 方案 A：generation/staging directory + 最终 manifest/pointer commit

```text
旧 generation 保持可用
 ↓
新 generation 在独立目录完整生成
 ↓
验证 bundle 内全部必需 artifact
 ↓
生成相对路径 manifest
 ↓
原子提交最终 manifest / generation pointer
 ↓
consumer 只在提交点之后看到新 generation
```

### 方案 B：固定文件逐个 `QSaveFile` 覆盖

该方案只能提供**单文件原子性**，不能自动提供 bundle 代际一致性。

如果正式 consumer 在发布过程中可能观察多个固定文件，则不能仅因为“每个文件都用了 QSaveFile”就宣称整个 artifact bundle 原子发布。

COMP-S1 必须明确：

- 最终采用哪一种发布策略；
- 正式 commit point 是什么；
- consumer 如何避免读取未完成 generation；
- 失败后旧 generation 是否完整可用；
- stale staging 如何清理；
- 是否需要 generation id/version；
- manifest 是否承担“发布完成标志”。

## 13.3 路径合同必须在实施前确定

新写入合同原则上按以下基准设计：

### runtime manifest 内

- `artifactPaths`：相对于 **`runtime_manifest.json` 所在目录**；
- 使用规范化相对路径；
- 新写入不得使用开发机绝对路径；
- 解析后不得允许通过 `..` 越出允许的 artifact root。

### project config 内

- manifest/artifact 的持久化路径：相对于 **project root**；
- project root 以当前项目文件/项目目录的正式定义为准，COMP-S1 必须从现有 Project/RunController 代码中确认，不自行发明第二个 root。

### source script 路径

- `mainScriptPath/scriptFiles` 若属于项目内部源码，优先 project-relative；
- 如果源文件位于 project root 外部，不得把外部机器绝对路径作为正式下载依赖；
- 外部路径若仅用于诊断，应降级为非权威 metadata 或省略，具体由 COMP-S1 给出兼容方案。

## 13.4 旧项目读取兼容

从绝对路径迁移到相对路径时必须遵守：

- **新写入只写可移植路径**；
- legacy absolute path 经 canonicalize 后，只有位于 project root 或已确认 artifact root 内才可作为正式运行/下载输入；
- 工程外绝对路径只能作为非权威诊断信息，确需使用时必须由用户明确确认，不得静默读取或下载；
- 若存在可无歧义推导的 relative/manifest-relative 路径，可做非破坏性 fallback；
- 只读加载不得偷偷重写项目文件；正常保存时可迁移为新格式；
- 不为了兼容旧项目继续把绝对路径复制到新的长期 artifact metadata。

## 13.5 初始预计相关文件

以下只是**预计审查范围，不是写入硬限制，也不是自动授权**：

```text
LH/src/compiler/DSLCompilerArtifacts.cpp
LH/src/designer/RunController.cpp
LH/tests/dsl_compiler_semantics_test.cpp
LH/tests/project_save_close_test.cpp
LH/tests/dsl_legacy_compile_probe.cpp
```

以及只读追踪发现的实际 Compiler invocation/output helper、manifest consumer、project path resolver。

## 13.6 输出要求

COMP-S1 完成后必须先向用户给出：

1. `.code/.list/.typ/.rep/runtime_points/runtime_manifest` 的实际生成链；
2. artifact bundle 中哪些文件属于正式运行/下载必需产物；
3. staging generation 可插入位置；
4. 推荐的 bundle publication strategy 与 commit point；
5. consumer 如何避免代际混用；
6. 必须修改的最小文件集合；
7. 哪些文件位于两个冻结区；
8. 哪些只是测试适配；
9. 是否需要修改 `dsl_legacy_compile_probe.cpp`；
10. 是否需要进入 `third_party/custom_dsp_language/compile/**`；
11. manifest-relative、project-relative 的具体字段表；
12. 旧 absolute-path 项目的读取兼容策略；
13. 风险与兼容性影响。

用户确认该狭窄清单与 publication contract 后才能进入 COMP-1。

# 14. COMP-1：Artifact bundle staging、一致性发布与可移植路径

> 前置：DL-D1 已确认正式 artifact consumer；COMP-S1 已完成；Artifact Bundle Publication Contract 已确认；用户已明确授权最终文件清单。

## 14.1 核心目标

COMP-1 的目标不是只修 `.code`，而是确保：

1. 编译前旧的正式 artifact generation 保持可用；
2. 新 generation 在 staging 中生成和验证；
3. consumer 不会观察到“部分新、部分旧”的 artifact bundle；
4. 只有 bundle 达到已确认完整条件后才进入正式可消费状态；
5. 发布失败时旧 generation 继续可用；
6. 新持久化路径可随项目移动；
7. legacy 项目在安全边界内保持读取兼容。

## 14.2 实施合同生成时点

COMP-1 的 mandatory/optional artifact、staging 插入点、generation 布局、commit point、consumer 切换方式、路径字段和最终授权文件，全部以 COMP-S1 的实际追踪结果和用户确认合同为准。本阶段不提前固化尚未证实的实现方案。

## 14.3 不可降低的验收底线

- Compiler 不得在 bundle commit 前覆盖旧正式 generation；
- 新 generation 必须在隔离 staging 中生成并验证全部 mandatory artifact；
- consumer 只能通过一个已确认的 commit point 切换到完整新 generation；
- 任一生成、校验、写入或 commit 失败时，旧 generation 继续可用；
- 新写入只使用 manifest-relative/project-relative 路径，并阻止 `..`、symlink 或 absolute path 越出允许 root；
- legacy absolute path 只有在 canonical path 位于允许 root 内时可直接使用，工程外路径不得静默成为运行/下载输入；
- 回归测试至少覆盖成功发布、失败保留旧 generation、stale staging 不被消费、工程移动后仍可解析；
- 受路径合同影响的现有 probe/test 必须纳入最终授权清单。

## 14.4 授权

本节不构成写入授权。COMP-S1 输出最小文件清单并经用户确认后，才能派发 COMP-1。
# 15. DL-1：设备连接 ownership 与生命周期收敛

> 前置：DL-D1 已确认产品职责；Compiler artifact 正式 consumer 合同稳定。

## 15.1 目标

逐步形成单一设备连接 ownership，避免：

- 同一串口被重复打开；
- 两条下载链各自维护不一致状态；
- 一个模块 disconnect 另一个模块；
- 下载时 Monitor 仍访问同一设备；
- retry/reconnect 生命周期冲突；
- cancel 后另一模块继续持有过期连接。

目标结构趋近：

```text
              DeviceConnection / Backend
                    ↑
          ┌─────────┴─────────┐
          │                   │
RuntimeSession         Diagnostic Tool
```

## 15.2 第一实施轮不允许“大合并”

优先做：

- ownership 明确；
- busy/occupied 状态；
- UI 互斥；
- connection state 单一来源；
- Monitor 与 Download 访问规则；
- cancel/retry/reconnect owner。

默认不做：

- 一次删除整个旧下载链；
- 一次重写所有 ControllerBackend；
- 为了“统一”牺牲诊断工具的独立价值。

## 15.3 授权

实施前必须重新只读追踪并列出精确文件，不能沿用旧计划中的宽泛文件集合。

该批属于生命周期/跨模块 P1，使用 `luna/max`。

---

# 16. P3-1：低风险定向架构整理

> 当前工作流状态：**阻断**。  
> 不在“完全不编译”的阶段实施纯文件组织重构。只有前述回归保护、安全和生命周期任务稳定，并且 Windows clean build 环境恢复后，才重新评估是否值得实施。

即使环境恢复，也只有在职责频繁修改、现有文件规模确实造成冲突/认知负担、且已有足够回归保护时才执行；不得为了“看起来更整齐”而拆文件。

## 16.1 MainWindow Output

可考虑：

```text
MainWindowOutput.cpp
```

仅移动已有 method definition：

- output context menu；
- copy；
- save；
- clear；
- log pane UI coordination。

要求：

- public API 不变；
- 行为不变；
- 不顺带增加新 controller；
- 不改业务逻辑。

## 16.2 MainWindow Monitor UI

可考虑：

```text
MainWindowMonitor.cpp
```

仅移动：

- open monitor；
- start；
- stop；
- export；
- action enable state。

## 16.3 MonitorController / OutputPaneController

禁止直接删除。

先确认：

- 是否有外部调用；
- 是否被测试引用；
- 是否属于兼容 API；
- 是否未来计划接入 MainWindow。

再决定：

- 正式接入；或
- 删除。

## 16.4 MonitorManager

如果文件规模仍然明显影响维护，可仅拆 implementation：

```text
MonitorManager.cpp
MonitorManagerHistory.cpp
MonitorManagerPolling.cpp
```

保持：

- 同一个 class；
- public API 不变；
- 行为不变。

不立即拆成多个 Manager class。

---

# 17. Blocked：Windows 与目标环境验证

这些任务当前必须保留，但不得伪装成静态闭环已经完成。

## 17.1 VAL-1：Windows clean build + CTest

环境恢复后必须：

```text
clean configure
 ↓
clean build
 ↓
targeted CTest
 ↓
full CTest
```

不得使用旧 build 目录证明当前测试注册或当前源码可编译。

## 17.2 VAL-2：Windows + Matrikon OPC DA

至少验证：

- COM activation；
- server connection；
- browse；
- AddGroup；
- AddItems；
- sync read；
- sync write；
- subscription callback；
- value；
- quality；
- timestamp；
- per-item failure；
- master failure；
- disconnect/reconnect；
- stop 后 late callback；
- server unavailable。

只有这些真实运行验证完成，OPC 相关任务才可达到验证等级 `L3`。

## 17.3 VAL-3：真实控制器下载

完整链路：

```text
Compile
 ↓
Artifact
 ↓
Manifest
 ↓
Precheck
 ↓
Download
 ↓
Verify
```

异常至少覆盖：

- 连接中断；
- retry；
- cancel；
- reconnect；
- device reject；
- verify failure；
- 串口占用；
- 两条工具链连接冲突；
- Monitor/Download 竞争。

## 17.4 VAL-4：大数据库与长时间导出

真实性能测试至少检查：

- 大量历史记录；
- 多 channel；
- 小 page size；
- bounded memory；
- 数据库查询耗时；
- 导出吞吐；
- 无重复；
- 无遗漏；
- provider failure 时旧文件保留；
- commit failure 时旧文件保留。

---

# 18. 批次派发

派发和验收流程遵循项目 `AGENTS.md`。本合同额外要求派发时引用任务 ID，并明确该任务表中的业务合同、授权文件、风险/模型选择和当前验证等级；不另行维护通用派发模板。

---

# 19. 当前派发判定

执行顺序只以第 6 节唯一任务表为准，本节不再维护第二套顺序。

当前可写任务依次为 `EXP-1`、`HIST-B1`；当前可做的只读决策/调查依次为 `OPC-D1`、`DL-D1`、`COMP-S1`。其余任务按第 6 节任务表保持阻断。

任何批次开始前，都先按第 4.2 节检查相关源码是否相对基线发生变化。

# 20. 最终原则

当前阶段的目标不是在缺少 Windows/实机环境时追求“形式上的全部完成”，而是：

1. 不重复实现已经完成的功能；
2. 为数据正确性建立可信回归保护；
3. 把产品语义决策与测试实现分开；
4. 把测试优先级与实施风险分开；
5. 对 Compiler 以 artifact bundle/generation 为发布单元，避免只修 `.code` 或只获得单文件原子性；
6. 在正式 artifact consumer 明确后定义 manifest-relative / project-relative 的唯一解析基准与 legacy absolute-path 兼容；
7. 将 aligned 导出视图与逐 Sample 无损导出合同分开，不用测试固化未确认的碰撞语义；
8. 严格保护冻结目录和用户已有工作树；
9. 所有依赖 Windows/Matrikon/硬件的结论明确留到目标环境验证。

在当前环境中，普通代码批次最高只能达到验证等级 `L1`；涉及 `Q_OS_WIN` 的代码必须额外注明其 Windows 分支未编译。

普通代码批次最高合格结论为：

> **静态验收通过，编译与运行测试需要验证。**
