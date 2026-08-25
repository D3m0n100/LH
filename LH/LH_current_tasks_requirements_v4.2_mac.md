# LH 项目当前阶段任务需求合同

> 版本：v4.2（2026-08-23 全仓复审合并版）
> 适用仓库：`D3m0n100/LH`
> 当前源码基线：`main@971cd1d85913dea5dd429cd191e08e53588dc4d2`
> 当前工作环境：**macOS**
> 当前阶段：**允许继续实施可静态闭环的代码修复；Windows 编译、CTest、Matrikon 与真实控制器验证统一后移。**
> 文档性质：**当前阶段唯一任务需求合同候选版**。本版制定时以当前 `main` 源码、当前 `Handoff.md` 和已知本地工作树状态共同校正任务状态。**只有本文件正式放入仓库并由当前 `Handoff.md` 明确指向后，v4.2 才成为唯一任务合同**；在此之前，仓库内现行合同仍保持有效，不得仅因本地 Downloads 中存在本文件就宣称合同已经切换。

---

# 1. 文档目标

本合同用于明确：

1. 当前源码已经完成、不得重复实现的能力；
2. 当前在 macOS 环境仍可以继续实施的 correctness / reliability 修复；
3. 哪些事项只是缺少 Windows / Matrikon / 控制器运行验证，而不是必须停止代码工作的阻断项；
4. 哪些任务必须先做产品或架构决策；
5. Compiler 冻结区、正式下载链、设备连接 ownership 等高风险修改的授权边界；
6. Windows 环境恢复后的统一 L2/L3 验证队列。

核心原则：

> **当前不要为了等待 Windows 而停止所有代码工作。能够在 macOS 上通过源码审查、接口推导、单元测试设计和局部实现达到 L1 的任务可以继续；Windows 只负责后续补齐 L2/L3。**

同时：

> **不要为了让 LH 在 macOS 上“能编译”而引入产品无关的平台兼容修改。**

## 1.1 合同激活与仓库落盘

v4.2 只有完成以下两步后才正式接管“唯一任务合同”地位：

1. 将本文件正式放入仓库中的任务合同位置；
2. 更新当前 `Handoff.md`，明确写出当前唯一任务合同文件名/路径与版本 `v4.2`。

2026-08-23 复审时的实际状态：本文件仍是未跟踪工作树文件，`Handoff.md` 仍指向已在当前工作树删除的 v3。因此 v4.2 **尚未激活**；在下一项代码任务派发前，应先提交/跟踪本文件并把 Handoff 的唯一合同指向和任务状态同步到本版。该文档同步不授权修改任何生产代码。

推荐 Handoff 至少包含：

```text
当前唯一任务合同：LH_current_tasks_requirements_v4.2_mac.md
当前合同版本：v4.2
旧 v3/v4/v4.1 仅作历史参考，不得恢复旧任务状态。
```

在完成上述仓库写入前：

- 本文件只作为待激活候选合同；
- 不删除仓库内现有合同；
- 不修改 `Handoff.md` 的历史完成记录；
- 不把“本地文件已经生成”误写成“仓库合同已经切换”。

---

# 2. 工作流状态与验证等级

## 2.1 工作流状态

任务状态只使用：

1. `未开始`
2. `进行中`
3. `阻断`
4. `已完成`

解释：

- `未开始`：该任务尚未进入实施；即使计划后移，也仍使用 `未开始`，不额外发明第五种工作流状态；
- `进行中`：已经存在当前批次的有效实现/调查工作，但尚未满足完成条件；
- `已完成`：当前授权范围内代码/决策已经完成，达到其当前环境允许的验证等级；若仅达到 L1，后续 Windows/目标环境验证债务写在“后续验证”列；
- `阻断`：存在真实前置决策、用户授权、冻结区授权，或任务本身必须依赖当前不可用的 Windows/Matrikon/硬件环境才能执行；
- **Windows 环境暂不可用本身，不再自动把普通、可静态实施的代码修复标记为“阻断”；但 VAL-1~4 这类环境验证任务本身当前就是阻断。**

## 2.2 验证等级

- `L0：未实施 / 未验证`
- `L1：代码完成，静态验收通过`
- `L2：Windows clean configure/build + 相关 CTest 通过`
- `L3：目标环境运行验证通过`
- `N/A：纯产品 / 架构决策`

当前 macOS 环境中：

- 普通 C++ / Qt 逻辑修复可以达到 `L1`；
- CMake、Windows batch、`Q_OS_WIN` / COM 条件分支可以完成静态修改与审查，但只能记作 `L1`，必须附带“Windows 编译未验证”；
- Matrikon OPC DA、真实控制器、串口/Modbus 实机、大数据库真实性能只能在目标环境达到 `L3`；
- 不使用 macOS build/CTest 结果替代 Windows L2。

## 2.3 当前统一验收措辞

普通代码任务在当前环境的最高结论：

> **静态验收通过；Windows 编译与运行测试待补充。**

涉及 `Q_OS_WIN` / COM / Windows 部署脚本：

> **源码静态审查完成；Windows 条件分支的 clean build 与运行验证待补充。**

禁止在尚未完成 Windows/目标环境验证时使用：

- “Windows 已验证”；
- “全部测试通过”；
- “Matrikon 已联调通过”；
- “控制器实机下载已验证”；
- “性能已经达标”。

---

# 3. 当前环境与平台约束

## 3.1 当前环境：macOS

当前允许：

- 读取、分析、修改跨平台源码；
- 修改测试代码并做静态审查；
- 修改 CMake / Windows 脚本，但仅在确有必要时进行；
- 做接口一致性检查、状态机检查、线程安全检查、错误传播检查；
- 完成产品/架构决策；
- 完成 Compiler/下载链只读追踪；
- 生成 Windows 后续验收清单。

当前默认不做：

- 不把“让 LH 在 macOS 成功编译”作为本阶段目标；
- 不为了 macOS 编译引入平台适配；
- 不使用 macOS CMake/CTest 作为正式验收结论；
- 不把 macOS 上不可执行的 Windows/COM 路径当成必须立即修复的阻断条件。

## 3.2 当前暂不可完成

以下统一后移：

- Windows clean configure/build；
- Windows CTest；
- `Q_OS_WIN` 条件分支真实编译；
- `windeployqt` / Windows 安装包验证；
- Matrikon OPC DA COM 联调；
- 真实控制器下载；
- 串口/Modbus/CAN 实际联调；
- 大数据量真实性能与内存测量。

## 3.3 “Windows 可滞后”规则

今后派发任务时按以下优先级判断：

### A. 不依赖 Windows 才能判断正确性的逻辑问题

例如：

- QVariant -> Modbus register 编码；
- 参数状态机；
- partial success；
- 错误码传播；
- 数据竞争；
- SQLite migration 返回值处理。

**现在实施，做到 L1，Windows 后续统一补 L2。**

### B. 主要价值只在 Windows 构建/部署时才能验证的问题

例如：

- `windeployqt`；
- Windows exe 名称；
- Qt5/Qt6 CMake 选择冲突；
- COM callback 条件测试；
- 打包脚本。

**如果不阻塞当前 correctness 工作，可以后移到 Windows 验证批次前集中处理。**

### C. 必须依赖真实外设/软件环境才能决定行为的问题

例如：

- Matrikon HRESULT 边界；
- 控制器下载协议真实兼容；
- 串口/设备 ownership 的最终现场行为；
- 大数据真实吞吐。

**保留设计与静态实现，最终关闭必须到 L3。**

---

# 4. 当前源码基线与漂移规则

## 4.1 当前基线

本合同基于：

```text
main@971cd1d85913dea5dd429cd191e08e53588dc4d2
```

该版本已经晚于 v3 合同中的：

```text
main@bde155ac2899672717fa68dd391d302976b9284b
```

因此 v3 的“未开始”状态不得机械继承。

## 4.2 基线漂移

执行具体批次时如果 `main` 已继续前进：

- 不重新扫描整个仓库；
- 只比较该批次授权文件、直接调用链、相关测试与本基线差异；
- 若问题已被后续 commit 修复，则直接关闭该任务，不重复实现；
- 若接口发生变化，先更新该批次局部合同；
- 不依据历史 `Handoff.md` 恢复旧状态；当前 `Handoff.md` 只用于校正 v4.2 制定时的已完成基线，v4.2 生效后以本合同任务表为准。
- 当前本地工作树若存在未提交修改，必须先做授权文件范围内的 scoped diff；不得因远端 `main` 缺少这些修改而回退、覆盖或重复实现。

---

# 5. 已完成基线：不得重复实现

## 5.1 历史数据 / DataManager

当前源码已具备：

- Schema v4；
- `quality`；
- `valueValid`；
- `origin`；
- `errorCode`；
- `errorText`；
- `(timestamp, id)` keyset pagination；
- history page / DB error 分离；
- latest-history 固定 `end` 上界；
- history count API。

## 5.2 Monitor 历史读取与导出

当前源码已具备：

- `historyFromDatabasePage()`；
- `historyFromDatabaseLatestPage()`；
- `RuntimeRecord -> Monitor::Sample`；
- quality/valueValid/id 与 metadata 保留；
- CSV / JSON / TSV；
- 单通道和多通道导出；
- paged/stream export；
- `QSaveFile`；
- provider failure / paged commit failure 的旧文件保护；
- `monitor_history_export_integration_test.cpp` 等历史/导出集成保护。

因此 v3 中的 `EXP-1`、`HIST-B1` **不再作为当前 Now 任务重复派发**。

它们当前统一视为：

> **代码/测试基线已存在；等待未来 Windows clean build + CTest 做 L2 复验。**

现有 CSV/JSON/TSV 未声明 `origin/errorCode/errorText` 为正式输出 schema，不得在未做 `EXP-D1` 产品决策前擅自扩展。

## 5.3 OPC/Matrikon

当前已有：

- callback value / quality / timestamp；
- worker -> owner queued delivery；
- stop 后旧 queued callback 丢弃；
- generation/context 隔离；
- Good / Stale / Bad 基本映射；
- sync read / write / probe 基础路径。

不得重新实现 OPC callback 架构。

## 5.4 MainWindow / RuntimeSession

已存在：

- `MainWindow.cpp`
- `MainWindowUi.cpp`
- `MainWindowInspector.cpp`
- `MainWindowExplorer.cpp`
- `RuntimeSessionController.cpp`
- `RuntimeSessionDownload.cpp`
- `RuntimeSessionDebug.cpp`
- `RuntimeSessionOpc.cpp`

因此不再派发“整体拆 MainWindow”任务。

---


## 5.5 已完成决策 / Artifact / 下载生命周期基线

以下任务在当前 `Handoff.md` 与当前源码中已经有明确完成证据，不得在 v4.2 中重新派发为“未开始”：

- `EXP-D1`：产品决策已完成。逐 Sample 无损导出与 aligned 时间对齐视图分开；当前 aligned 模式不承诺同一 channel + 同一 timestamp 多条 Sample 的逐条无损。
- `OPC-D1`：callback master/per-item HRESULT、null payload、quality failure、generation 隔离等业务合同已确认。
- `DL-D1`：正式下载链与诊断下载链职责已确认：
  - `RuntimeSessionController -> ControllerDeviceBackend` = 唯一正式项目下载链 / artifact consumer；
  - `DownloadDockWidget -> DownloadManager -> ControllerBridge` = 工程、诊断、手工下载工具。
- `COMP-S1`：artifact bundle / generation / commit point / 可移植路径只读追踪已完成并确认。
- `COMP-1`：generation staging、一致性发布、`runtime_manifest.json` commit point、manifest-relative / project-relative 路径和旧 generation 保留已完成静态验收，当前为 `L1`，只欠 Windows `L2`。
- `DL-1` 第一实施轮：进程内 RTU port owner、错误 owner release、DeviceBusy、后端操作互斥、借用 backend 的 stop/disconnect ownership、诊断取消等已完成静态验收，当前为 `L1`。PAR-B1 新发现的“对象被外部销毁后异步观察指针失效”属于独立安全缺口，不重新打开 ownership 决策。

因此：

> **不得因为 v4/v3 的旧状态把上述任务重新实现。后续只补它们尚未完成的 Windows/目标环境验证，或在 VAL-3 暴露新证据时新建第二轮任务。**


# 6. 当前新发现的 correctness 风险

本节来自对当前 `main@971cd1d...` 的静态审查以及当前本地工作树的补充审查。优先级高于纯结构整理，也不因 Windows 暂不可用而暂停。

> 当前远端 `main` 不包含未提交的 `RuntimePointRegisterCodec.*`。根据当前本地工作树审查，`COR-B1` 已有实现进行中；实际派发前必须先对本地授权文件做 scoped diff，确认已有改动后继续，不得从远端基线覆盖本地工作。

2026-08-23 全仓复审继续以同一工作树为准，并补充确认：

- `COR-B1` 的 typed codec/mapped error 主体已有静态实现，但设备号 alias 优先级仍可能把显式 `slaveId` 错认成默认 `unitId=1`；
- `PAR-B1` 原 A~E 已有实现，但回读比较仍缺少数据类型语义，异步路径仍保存可能失效的裸 backend 指针；
- `MON-1` 已达到 L1，`MON-I1` 已完成只读调查且没有形成生产修改任务；
- `DB-1` 原 A~D 已有实现，但仍会接受未来或非法 schema version；
- Windows 名称统一与 P3 定向 implementation split 已有静态完成证据，不得按旧状态重复实施；
- 新的独立问题只在无法由现有批次闭合时新增任务，禁止借复审扩大成通用重构。

## 6.1 COR-B1：RuntimePoint 类型化寄存器 I/O + mapped CommError

### 当前状态

`进行中 / L0`，尚未完成 `COR-B1` 整体静态验收。

更精确的状态说明：

> **v4 `COR-1` 以及 v4.2 原 A~G 的主体已经有静态实现；`COR-B1` 仍在进行中。当前剩余闭环重点是设备号 alias/default 的来源优先级、非法值与冲突检测，以及对应回归测试。**

根据当前本地工作树审查，已有 codec 实现不能被重新覆盖；继续实施前必须先做 scoped diff。远端 `main` 不足以反映本地未提交 codec 状态。

该批合并原 v4 的 `COR-1 + COMM-1`，原因是两者共享 `ControllerDeviceBackendPoints.cpp`、PointMapping 和 codec 语义，拆成两个批次容易造成重复修改或中间状态不一致。

### 问题 A：正式点位 I/O 缺少类型化 codec

RuntimePoint schema 已定义：

```text
dataType
byteOrder
wordOrder
scale
offset
elementCount
```

但正式 Controller backend 仍存在裸 `quint16` / `QVariantList<quint16>` 转换路径。现有 UI/`ParameterController` 把在线参数编辑值作为 `QString` 传给 backend，因此 codec **必须严格解析合法数字字符串**，不能简单拒绝所有 `QString`，更不能让无效字符串静默转 0。

### 问题 B：DSL 类型与 backend 类型命名未统一

当前 DSL 正式类型至少包括：

```text
BOOL
INT    = signed 16-bit
DINT   = signed 32-bit
REAL   = IEEE754 float32
```

Runtime/backend 第一轮采用以下 canonical mapping：

```text
BOOL            -> BOOL
INT / INT16     -> signed 16-bit
DINT / INT32    -> signed 32-bit
UINT / WORD     -> unsigned 16-bit compatibility alias
UDINT / DWORD   -> unsigned 32-bit compatibility alias
REAL / FLOAT32  -> IEEE754 float32
```

其中 `UINT/WORD/UDINT/DWORD/FLOAT32` 是 Runtime/backend 兼容 alias，不得反向写成“DSL 当前已经正式支持这些语法类型”。

### 问题 C：`elementCount` 不能直接等于 Modbus register count

`elementCount` 表示**逻辑元素数**。实际寄存器数必须由类型宽度推导：

```text
registerCount = elementCount × registersPerElement(dataType)
```

第一轮至少：

```text
BOOL / INT / INT16 / UINT / WORD        -> 1 register / element
DINT / INT32 / UDINT / DWORD / REAL     -> 2 registers / element
```

若 legacy/config 中同时存在显式 `registerCount/count/length`，新路径必须检查其与推导结果是否一致；不一致时返回 `InvalidConfig`，不得任意选择其中一个继续写设备。

### 问题 D：`offset` 字段存在双重语义冲突

`lh.runtimePointAddressing.v1` 中：

```text
scale
offset
```

已经是物理量映射字段，因此 `offset` 在 v1 schema 下**只能表示物理偏移**，不得继续作为 `address/regAddress/registerAddress` 的寄存器地址 alias。

兼容规则：

- `lh.runtimePointAddressing.v1` 中，`offset` **永远只表示物理偏移**，绝不作为寄存器地址；
- 当前转换层会为 `RuntimePointAddressing` 自动写入 `schemaVersion=lh.runtimePointAddressing.v1`，并将标准 addressing 字段规范化后再交给 backend，因此 backend 侧已经不能可靠判断“原始配置最初是否缺少 schemaVersion”；
- **在找到真实 legacy 配置证据、并在转换链中显式保留来源/legacy 标记之前，不实现 `offset` 地址 fallback**；
- 需要寄存器地址的旧配置必须显式使用 `address`、`regAddress` 或 `registerAddress`；
- 不得仅为了猜测兼容旧数据而让 `offset` 恢复双重语义。

如未来确有 legacy 文件必须兼容，应新建独立兼容任务：先保存原始 schema/来源信息，再定义有测试保护的迁移或 fallback；不得直接在当前 backend 中凭字段存在性猜测。

### 问题 E：scale/offset 正反向公式必须闭合

正式合同：

```text
physical = raw * scale + offset
raw      = (physical - offset) / scale
```

要求：

- `scale != 0`，否则 `InvalidConfig`；
- decode 使用第一式；
- encode 使用第二式；
- 对整数 raw 类型，若反算结果超范围或不能按合同精确表示，返回 `InvalidParameter`；
- 第一轮不得静默使用 `round/floor/truncate` 发明舍入策略。

### 问题 F：byteOrder / wordOrder 必须有单一语义

- `byteOrder`：单个 16-bit register 内的字节顺序；
- `wordOrder`：32-bit 值在多个 16-bit register 间的 word 顺序；
- 不支持的字符串值 -> `InvalidConfig`；
- 16-bit 单寄存器类型不得错误套用 32-bit word swap。

### 问题 G：mapped point 错误分类当前不精确

读写都必须区分：

```text
point/mapping 不存在                 -> InvalidAddress
point 存在但当前 access 不允许       -> PermissionDenied
mapping/schema 配置非法              -> InvalidConfig
输入值/范围/逻辑元素数量非法         -> InvalidParameter
真实 Modbus/transport 失败           -> 保留 currentDebugError()
```

尤其禁止把 `SendFailed / ReceiveTimeout / ConnectionLost` 覆盖成 `InvalidAddress`。

### 问题 H：设备号 alias 可能被自动默认值抢占

`RuntimePointAddressing::toVariantMap()` 会固定写入默认：

```text
unitId = 1
```

backend 合并 addressing 与 metadata 后若按：

```text
unitId -> slaveId -> stationAddress -> serverAddress
```

取第一个正数，则显式 metadata `slaveId=2` 可能被自动生成的 `unitId=1` 抢先采用，最终访问错误设备。

正式合同：

- 用户显式 metadata/config 值优先于转换层自动生成的默认值；
- `unitId/slaveId/stationAddress/serverAddress` 是同一语义的 alias，多个显式 alias 值冲突时返回 `InvalidConfig`，不得按顺序静默选一个；
- 任一显式 alias 存在但类型非法、不是严格正整数或超出设备号范围时返回 `InvalidConfig`，不得回退到 backend 默认 deviceId；
- 只有完全没有显式设备号时，才允许使用规范化 addressing/default deviceId；
- 不改变当前设备号有效范围，也不引入新的地址 schema。

### 授权范围

实施前先检查当前本地工作树，预计允许：

```text
src/communication/RuntimePointRegisterCodec.h
src/communication/RuntimePointRegisterCodec.cpp
src/communication/ControllerDeviceBackendPoints.cpp
src/communication/ControllerDeviceBackend.h
src/communication/CMakeLists.txt
src/common/RuntimePointTypes.h                    # 仅 alias/default 来源无法在 backend 内正确区分时
相关 controller backend / codec tests
```

`src/common/RuntimePointTypes.h` 仅在确有必要补 helper 时单独列入，不默认修改 schema。

若本地已经存在 codec 文件，必须在其现有改动基础上补齐，不得重建覆盖。

### 最低验收

至少覆盖：

```text
REAL QString "2.0" -> 2 registers -> round-trip
INT / INT16 QString "-1"
DINT / INT32 负数 round-trip
UINT16 65535
UINT16 65536 -> InvalidParameter
非法数字字符串 "abc" -> InvalidParameter
REAL byteOrder / wordOrder
scale/offset round-trip
scale == 0 -> InvalidConfig
elementCount = 2 的 REAL -> 4 registers
显式 registerCount 与推导数量不一致 -> InvalidConfig
ReadOnly write -> PermissionDenied
WriteOnly read -> PermissionDenied
真实 mapped transport failure -> 保留底层 CommError
v1 `offset` 不作为寄存器地址
metadata 仅含 slaveId=2 -> 最终 deviceId=2，不被默认 unitId=1 覆盖
多个显式设备号 alias 同值 -> 接受
多个显式设备号 alias 冲突 -> InvalidConfig
显式设备号 alias 非法/越界 -> InvalidConfig，不回退默认值
```

当前目标：完成 `L1`；Windows CTest 后到 `L2`，真实控制器后到相关 `L3`。

---

## 6.2 PAR-B1：Parameter partial-success + readback + OPC 写确认

### 当前状态

`进行中 / L0`。该批合并原 v4 的 `PAR-1 + PAR-2`，并纳入同一调用链中的 OPC 写确认问题。

当前工作树已经实现原问题 A~E 的主要状态机与测试，但本次复审发现问题 F/G 尚未闭合，因此不得提前标记 L1，也不得覆盖当前已有实现重新开始。

### 问题 A：partial-success 没有闭环

`IDeviceBackend::writePoints()` 的正式合同允许：

```text
overall bool = false
同时部分 point 实际写成功
pointErrors 只列失败点
```

当前 `ParameterController` 未完整消费 `pointErrors`，且同步/异步 wrapper 在整体写失败后会立即返回，导致成功点无法继续 readback。

### 正式状态合同

对本次**目标参数集合**逐点处理：

```text
写成功点 -> PendingReadback
写失败点 -> ApplyFailed + 对应逐点 CommError
```

即使 batch overall write 为 false：

- 成功点仍必须继续回读；
- 失败点不得参与后续 readback；
- mixed-result 的**最终 batch 结果**必须为 failure；
- 成功点仍应有机会最终达到 `Confirmed`。

### 同步 / 异步 API 返回语义

必须明确区分“最终结果”和“异步任务是否成功启动”：

```text
同步 API bool
    = 最终 batch 结果

异步 API bool
    = 本次 apply/readback 流程是否成功启动
      不是最终业务成功结果
```

具体要求：

- mixed-result 且至少一个成功点需要回读：异步 API **立即返回 `true`**，继续对成功点回读；最终通过 `readbackFinished(false, ...)` 报告批次失败；
- 全部目标点写失败：异步 API **立即返回 `false`**，不启动 readback；
- 全部写成功：异步 API 返回 `true`，最终成功/失败仍由 readback 结果决定；
- 同步 API 必须等待其现有同步流程完成，并直接返回最终 batch success/failure；
- 调用方不得把异步 API 的 `true` 解释成“设备值已经确认写入成功”。

### 问题 B：同步/异步 readback 语义不一致

统一为共享 decision contract：

```text
所有目标点 Confirmed
且不存在 ApplyFailed / Mismatch / Timeout
    -> batch success

存在 PendingReadback
    -> 继续当前 retry policy

出现 Mismatch
    -> batch failure（不得因为 Pending 为空误报 success）

retry exhausted 且仍 Pending
    -> Timeout + batch failure
```

第一轮保持现有异步路径对 mismatch 的失败语义，不额外发明新的 mismatch retry 产品策略。

同步与异步不得各自维护一套互相漂移的判定条件；优先抽取小型内部 evaluator/helper。

### 问题 C：OPC 写在回读确认前提前宣布成功

`RuntimeSessionOpc` 当前在 `applyModifiedParametersWithReadbackAsync()` 仅“成功启动”后就可能立即：

```text
recordWriteResult(success=true)
publish RuntimePointQuality::Good
```

正式合同必须改为：

```text
收到 OPC write
 ↓
只编辑/应用该 pointId
 ↓
backend write
 ↓
readback
 ↓
Confirmed
 ↓
recordWriteResult(true)
publish confirmed value / Good
```

如果最终为：

```text
ApplyFailed
Mismatch
Timeout
```

则：

- `recordWriteResult(false, ...)`；
- 不发布新的 `Good` 值；
- 不覆盖上一次已确认值。

### 问题 D：OPC 单点写不能隐式 flush 其他 UI Modified 参数

当前 `applyModifiedParameters()` 语义是应用**所有 Modified 参数**。OPC 单点写必须限定目标 scope：

```text
OPC 写 point A
```

不得顺带把 UI 中尚未提交的：

```text
point B / point C == Modified
```

一起下发。

实现可以增加内部“目标参数集合/目标 pointIds”能力，但不要求固定某个公开 API 名称。原则是：

- UI “应用全部修改”仍可作用于所有 Modified；
- OPC 单点写只作用于本次 pointId；
- 不为 OPC 单点写清空或改变其他 Modified 参数状态。

OPC 侧还必须保存**本次待确认 pointId / operation context**，用于把 `readbackFinished` 与当前请求关联，并遵守：

- 成功或失败完成后清理；
- OPC server stop 时清理；
- controller/backend 切换或 RuntimeSession 重置时清理；
- 新请求不得错误继承上一请求的 pending point；
- 如果当前实现不支持并发多个 OPC 参数写确认，则第一轮应显式串行化/拒绝重入，而不是靠单个隐式成员覆盖前一请求。

### 问题 E：最终发布值必须来自确认后的新状态

不得继续使用 OPC 请求开始前缓存的旧 `ParameterStateInfo.appliedValue` 发布结果。成功发布应读取本次 readback 后的确认状态/值。

### 问题 F：回读比较丢失参数数据类型

当前状态只保存编辑值/应用值字符串，readback 又统一转成 `QString` 后用“尝试转 double，否则字符串相等”比较。这会造成至少两类误判：

```text
BOOL 写入 "1"，backend 回读 true
    -> "1" != "true"，错误进入 Mismatch

REAL 写入十进制值，设备按 float32 编解码后回读
    -> 固定绝对 1e-6 可能把合法量化误差判成 Mismatch
```

正式合同：

- `ParameterStateInfo` 或等价内部状态必须保留定义中的 canonical `dataType`；
- BOOL 按布尔语义规范化比较，合法的数值布尔输入与 backend `QVariant(bool)` 必须一致；
- INT/DINT/无符号兼容类型按目标整数语义精确比较，不得经 double 模糊比较；
- REAL/FLOAT32 必须按设备实际 float32 表示能力比较，可采用 codec 一致的规范化值，或有明确上限的 absolute + relative tolerance；不得继续对所有数值固定使用绝对 `1e-6`；
- 非法/不可转换 readback 不得被宽松字符串规则伪装为 Confirmed；
- 不为此复制一套 RuntimePoint codec，也不修改 Compiler 类型系统。

### 问题 G：异步回读保存的裸 backend 指针可能失效

`ParameterController` 的 timer/retry 会跨事件循环保存 backend；`RuntimeSessionController` 同样保存外部注入/借用 backend。QObject signal 自动断开不能让普通 C++ 裸指针自动清空，因此 backend 在 pending readback 期间被外部 owner 销毁时，下一次 timer 可能访问已释放对象。

正式合同：

- 异步持有的 QObject backend 必须使用 `QPointer` 或等价的 `destroyed` 清理，不取得其 ownership，也不得主动 delete 外部对象；
- backend 销毁时立即取消属于该 backend 的 pending readback/OPC confirmation，清空指针与 operation context，并报告一次确定的失败；
- backend A 被替换为 B 后，A 的迟到 `destroyed` 不得清空 B；
- timer/retry 在每次访问 backend 前验证对象仍有效；
- 销毁/替换路径不得发布 success/Good，不得留下永久 `PendingReadback`。

### 授权范围

预计：

```text
src/designer/ParameterController.cpp
src/designer/ParameterController.h          # 内部状态/dataType/QPointer/target scope
src/designer/RuntimeSessionController.cpp   # 仅 backend destroyed/replacement 清理
src/designer/RuntimeSessionController.h     # 仅安全观察 backend 生命周期
src/designer/RuntimeSessionOpc.cpp
相关 parameter_controller tests
相关 runtime_session_controller / OPC write tests
```

默认不修改 `IDeviceBackend` 公共接口；它已有 `pointErrors` 合同。

### 最低验收

必须至少覆盖：

```text
mixed write:
  A success -> readback -> Confirmed
  B failure -> ApplyFailed
  C success -> readback -> Confirmed
  batch final == false

同步 API:
  mixed-result -> 最终返回 false
  全部 Confirmed -> 最终返回 true

异步 API:
  mixed-result 且有成功点待回读 -> immediate true
  最终 -> readbackFinished(false)
  全部写失败 -> immediate false，不启动 readback

同步 mismatch -> false
异步 mismatch -> readbackFinished(false)
readback exhausted -> Timeout + false

OPC 单点写：
  readback Confirmed 前不得 record success / publish Good
  Confirmed 后才 success
  Mismatch/Timeout/ApplyFailed -> failure，不发布新 Good
  不得提交其他 UI Modified 参数
  发布值来自本次 Confirmed/readback，不来自旧 stateInfo
  pending point/context 在完成、stop、backend/controller 切换时清理
  若不支持并发 OPC write confirmation，则明确拒绝/串行化重入

类型化回读：
  BOOL "1" 与 backend true -> Confirmed
  BOOL "0" 与 backend false -> Confirmed
  整数必须精确匹配
  REAL 合法 float32 量化回读 -> Confirmed
  REAL 超出合同容差 -> Mismatch
  非法 readback -> failure，不得 Confirmed

backend 生命周期：
  schedule 后、timer 执行前销毁 backend -> 不崩溃、不解引用失效对象
  retry 中销毁 backend -> pending 结束并只报告一次 failure
  backend A 替换为 B 后销毁 A -> B 保持有效
  销毁路径不得发布 OPC Good/success
```

当前目标：`L1`；Windows CTest 后到 `L2`。

---

## 6.3 MON-1：MonitorChannel 配置并发数据竞争

### 当前状态

`已完成 / L1`。当前工作树已经统一 getter/config/threshold snapshot 的锁策略，并保持在解锁后做 threshold evaluation 与 emit；静态审查未发现持锁外部回调。只欠 Windows CTest L2，不得重复实现。

### 问题

`MonitorChannel` 使用 `m_mutex` 保护 samples/config 修改，但多个 inline getter、`thresholds()` 和 `checkThresholds()` 读取 `m_config` 时未统一加锁/快照；与 `updateConfig/addThreshold/removeThreshold` 并发时存在 data race 风险。

### 目标

- `name/displayName/unit/config/thresholds` 等公开 getter 在锁内读取并返回值/snapshot；
- `appendSample/appendSamples` 对 samples 的修改保持锁保护；
- threshold evaluation 使用锁内复制出的 channel/config/threshold snapshot；
- 解锁后再执行 threshold 判断及 emit；
- 不在持锁状态触发外部可重入 signal。

### 授权范围

```text
src/monitor/MonitorChannel.h
src/monitor/MonitorChannel.cpp
相关 tests
```

当前 `L1` 已完成；Windows CTest 后到 `L2`。

---

## 6.4 DB-1：DataManager 初始化 / migration / connection cleanup

### 当前状态

`进行中 / L0`。当前工作树已经实现原问题 A~D 的主要修复与测试，但问题 E 尚未闭合，因此 DB-1 整体仍不能标记 L1。

### 问题 A：初始化 SQL 失败被忽略

当前初始化中的以下结果需要纳入 hard failure：

```text
PRAGMA foreign_keys = ON
CREATE_VERSION_TABLE
```

其中版本表创建失败不能继续调用 `getDatabaseVersion()` 并假装为新数据库。

### 问题 B：数据库版本读取错误与“无版本记录”混在一起

当前 `getDatabaseVersion()` 在 query 执行失败时也可能返回 `0`。

正式合同必须区分：

```text
query 成功但没有 version row -> 0（新数据库）
query 执行失败               -> initialize/migration hard failure
```

不得把 SQL 错误降级成“全新数据库”。

### 问题 C：required index failure 未阻断 migration

以下都属于 schema migration 的必要组成：

- initial runtime indexes；
- initial log indexes；
- v2 composite index；
- 后续被合同声明为 required 的 index。

任一 required index 创建失败：

```text
migration false
↓
rollback
↓
不得提升 schema_version
```

### 问题 D：connection cleanup 路径不一致

open/migration/init failure 与 reinitialize/shutdown 应统一遵循：

```text
close
↓
m_db = QSqlDatabase()
↓
QSqlDatabase::removeDatabase(connectionName)
↓
reset initialized/schema state
```

优先提取内部 helper，避免不同失败路径再次漂移。

### 问题 E：未来或非法 schema version 被当成当前版本接受

当前初始化只在：

```text
currentVersion < CURRENT_SCHEMA_VERSION
```

时执行 migration；若数据库版本为 5、999 等未来版本，会跳过 migration，随后仍把内存状态写成当前版本 4。版本字段转换也没有检查 `toInt()` 是否成功。

正式合同：

- version row 存在时必须是可严格转换的整数；
- `version < 0`、非整数/溢出值以及 `version > CURRENT_SCHEMA_VERSION` 均为 initialize hard failure；
- 未来版本失败必须发生在任何 schema 写入/migration 之前，不得把版本表改回当前版本；
- 失败后执行既有统一 connection cleanup，并保持 `m_initialized=false`；
- version row 不存在仍按现有合同表示新数据库版本 0；
- 不新增 schema v5，也不猜测向后迁移策略。

### 目标

- 初始化必要 SQL 全部检查结果；
- 版本读取失败有明确失败语义；
- required migration/index step 失败回滚；
- schema version 只在该 version 全部步骤成功后更新；
- 统一连接清理；
- 严格拒绝非法、负数与未来 schema version；
- 不新增 schema v5。

### 授权范围

```text
src/core/DataManager.cpp
src/core/DataManager.h      # 仅 helper / 返回结构必要时
相关 CoreDataManager tests
```

最低新增测试：

```text
schema version = CURRENT_SCHEMA_VERSION + 1 -> initialize false，版本行不被改写
schema version = 非整数/溢出值 -> initialize false
schema version < 0 -> initialize false
失败后 initialized=false，连接可按既有合同重新初始化
```

当前目标：`L1`；Windows CTest 后到 `L2`，真实性能仍由 VAL-4。

---

## 6.5 MON-I1：MonitorDataProcessor delta signal 语义调查

### 当前判断

原 v4 的 `MON-2` 不应直接作为生产代码修改任务。

虽然 `appendSample/appendPoint` 与 `appendSamples/appendPoints` 对：

```text
enableIncrementalMode
batchProcessThreshold
deltaDataReady
```

的触发条件并不完全一致，但当前主要生产消费者 `MonitorChartView` 在每个 render timer 周期都会主动：

```text
consumeAndRender()
↓
drainAllDeltas()
```

当前数据 drain 基本不依赖 `deltaDataReady` 才发生，因此暂时只能证明“API 语义不一致”，尚不能证明存在用户可见 correctness bug。

### 调查目标

只读确认：

- 全仓库 `deltaDataReady` 生产消费者；
- 是否存在除 `MonitorChartView` 外依赖该 signal 才 drain 的路径；
- signal 是否承担低延迟唤醒、统计或其他隐藏合同；
- 当前不一致是否造成 CPU wakeup / UI latency / 丢刷新等实际问题。

### 状态

`已完成 / N/A`。全仓只读调查确认生产 consumer 只有 `MonitorChartView`，其 render timer 无论 signal 是否到达都会执行 `consumeAndRender()` / `drainAllDeltas()`；当前没有 correctness 或可测性能证据支持生产修改。

默认不授权修改：

```text
src/monitor/MonitorDataProcessor.cpp
```

只有调查形成具体行为缺陷或性能证据后，才新建独立代码任务；不得仅为了“实现看起来一致”修改生产语义。

---

## 6.6 COMM-B2：Modbus 外层等待超时错误语义

### 当前状态

`未开始 / L0`。

### 问题

`waitForReply()` 的本地 guard timer 超时后返回 false，但 read/write 调用方继续从尚未完成的 `QModbusReply` 读取 `reply->error()`。该值可能仍为 `QModbusDevice::NoError`，形成：

```text
请求失败
但 CommErrorCode == NoError
```

这会让上层日志、质量映射或错误分类得到自相矛盾的结果。

### 正式合同

- 外层 guard timer 到期必须明确映射为 `CommErrorCode::ReceiveTimeout`；
- reply 已完成且携带 Qt Modbus error 时，继续使用既有 `mapReplyErrorToCommError()`；
- send request 返回 null 仍为 `SendFailed`；
- 失败路径不得报告 `NoError`；
- 只收敛等待结果/错误传播，不重写 Modbus client 或引入新的协议抽象。

### 授权范围

```text
src/communication/ModbusInterface.cpp
src/communication/ModbusInterface.h       # 仅最小 wait result/测试 seam 必要时
相关最小 timeout test / communication CMake 注册
```

### 最低验收

```text
外层 timer 先到期、reply 尚未完成 -> ReceiveTimeout
reply 正常完成 -> success
reply 携带 protocol/connection error -> 保留既有 mapped error
所有 false 返回路径 -> error code != NoError
```

当前目标：`L1`；Windows CTest 后到 `L2`，真实串口行为由 VAL-3 达到 L3。

---

## 6.7 OUT-B1：输出日志原子保存与失败反馈

### 当前状态

`未开始 / L0`。

### 问题

`MainWindowOutput` 与 `OutputPaneController` 使用 `QFile::WriteOnly` 直接截断目标文件，且没有完整检查 `QTextStream`/close 后错误；部分失败路径无提示，写入失败后仍可能报告“保存成功”。

### 正式合同

- 复用项目已有 `QSaveFile + QTextStream::status() + commit()` 模式；
- open、write 或 commit 任一失败均报告明确失败，不得显示成功消息；
- 失败不得破坏目标位置已有文件；
- UTF-8 与现有文本内容保持不变；
- 两个入口语义保持一致，不引入新的通用文件服务或额外依赖。

### 授权范围

```text
src/designer/MainWindowOutput.cpp
src/designer/OutputPaneController.cpp
仅在现有测试结构可直接覆盖时补最小保存失败测试
```

最低验收：成功保存内容一致；open/write/commit failure 不报告成功且旧文件保留。当前目标：`L1`；Windows UI 复验后到 `L2`。

---

# 7. Windows/构建相关问题：允许后移

以下问题已经识别，但**不阻塞第 6 节 correctness 修复**。

## 7.1 WIN-B1：Qt major 配置统一

根 CMake 当前固定 Qt5；当前工作树已经让 `src/communication/CMakeLists.txt` 跟随 Qt5，但 README 仍声明“Qt 5.15+ 或 Qt 6.x”，产品基线与使用说明尚未完全一致。

目标：在当前 Qt5 产品基线中统一 Qt major，避免 Windows 同机安装 Qt5/Qt6 时 communication 子模块选择不同 major。

建议：

- 不做 Qt6 migration；
- 只让 communication 跟随根项目 Qt5；
- README/构建说明只声明当前真实支持的 Qt major；
- Windows clean configure 时最终验证。

状态：`进行中 / L0`。CMake 修改已有静态证据；文档一致性与 Windows clean configure 尚未闭合。

## 7.2 WIN-B2：Windows 脚本目标名称一致性

当前 CMake target 为 `LH`；当前工作树已经把 `build.bat`、`run_platform.bat` 与 `tools/workflow_dev.ps1` 的 executable 路径统一为 `LH.exe`。

目标：统一到真实 target 名称，避免 Windows 构建成功但脚本误判 executable 不存在。

状态：`已完成 / L1`。只欠 Windows clean build/script 运行达到 L2，不得重新实施名称替换。

## 7.3 BUILD-B1：移除发布二进制的 host-specific `-march=native`

`src/monitor/CMakeLists.txt` 在非 MSVC Release 下额外加入：

```text
-O3 -march=native
```

这会把最终应用中的 monitor 代码编译为构建机器 CPU 专用指令，在较旧部署机器上可能出现 illegal instruction；同时重复覆盖 CMake/toolchain 已有 Release 优化策略。

正式合同：

- 删除 monitor target 自定义的 `-O3 -march=native` Release 分支，依赖 toolchain/CMake 的标准 Release flags；
- 保留现有 warning flags；
- 不新增“可配置 native optimization”选项，除非未来有独立 benchmark 与仅本机构建产品需求；
- 不借此调整其他 target 的优化级别。

授权范围：

```text
src/monitor/CMakeLists.txt
```

状态：`未开始 / L0`。静态目标为 L1；Windows/目标部署机器 clean build 后为 L2/L3。

## 7.4 WIN-B3：workflow_dev 双重定向输出死锁

`Invoke-Logged` 同时重定向 stdout/stderr，却依次阻塞调用 `ReadToEnd()`。当 child 的另一个 pipe 先写满时，parent 与 child 可能互相等待，导致 verbose build/test 永久卡住。

正式合同：

- stdout/stderr 必须同时消费，或使用 PowerShell 原生调用并合并 `2>&1`；
- 保留当前日志文件、控制台输出与真实 exit code；
- 不引入新的脚本框架或外部依赖；
- 最低验证包含同时大量输出 stdout/stderr 的 child process，workflow 必须完成且保留非零退出码。

授权范围：

```text
tools/workflow_dev.ps1
```

状态：`未开始 / L0`。macOS 可静态实施到 L1，Windows PowerShell 实际运行后到 L2。

## 7.5 Windows/构建类任务的执行策略

这些任务可以：

- 在 Mac 上静态修改后标记 L1；或
- 直接等到 Windows 环境恢复前一批集中修改。

默认先完成第 6 节 correctness 批次，再处理 `BUILD-B1/WIN-B1/WIN-B3`；`WIN-B2` 只补后续 L2，不重复修改。

---

# 8. 唯一任务表

> **本表在 v4.2 正式放入仓库并由当前 `Handoff.md` 指向后，成为唯一执行顺序与任务状态来源。**
>
> `进行中` 的 `COR-B1` 状态来自当前本地工作树静态审查；实际写入前必须用本地 scoped diff 再确认。远端 `main` 只作为已提交基线，不得覆盖未提交工作。

| ID | 任务 | 执行顺序 | 风险 | 真实依赖 / 决策门 | 当前授权原则 | 工作流状态 | 当前验证等级 | 后续验证 |
|---|---|---|---|---|---|---|---|---|
| COR-B1 | RuntimePoint typed codec + mapped CommError + deviceId alias | **Now-1** | 设备数据正确性 / P1 | 先确认当前本地 codec/Points diff | 在现有 codec/Points/tests 上补 alias/default 闭环 | **进行中** | L0（原 A~G 主体已有实现，H 未闭合） | Windows CTest L2 + 真机相关 L3 |
| PAR-B1 | partial-success + typed readback + backend lifetime + OPC 确认/scope | **Now-2** | 状态一致性/生命周期 / P1 | COR-B1 的 backend value/error contract 稳定 | ParameterController + RuntimeSession + OPC + tests 最小清单 | **进行中** | L0（原 A~E 主体已有实现，F/G 未闭合） | Windows CTest L2 |
| DB-1 | DataManager init/migration/index/cleanup/version guard | **Now-3** | 数据可靠性 / P1 | 无 | DataManager + tests；不新增 schema v5 | **进行中** | L0（原 A~D 主体已有实现，E 未闭合） | Windows CTest L2 + VAL-4 |
| COMM-B2 | Modbus outer timeout error semantics | **Now-4** | 错误/质量语义 / P2 | 无 | ModbusInterface + 最小 timeout test | 未开始 | L0 | Windows CTest L2 + VAL-3 |
| OUT-B1 | 输出日志原子保存与失败反馈 | **Now-5** | 文件数据保护 / P2 | 无 | 两个现有保存入口，复用 QSaveFile 模式 | 未开始 | L0 | Windows UI 复验 L2 |
| MON-1 | MonitorChannel config race | Baseline | 线程安全 / P1 | 已实施 | 不重复实现 | 已完成 | L1 | Windows CTest L2 |
| MON-I1 | MonitorDataProcessor delta signal 调查 | Baseline | 性能/行为 / P2 | 调查已完成，无缺陷证据 | 默认无生产写入授权 | 已完成 | N/A | 只有新测量证据才另建任务 |
| EXP-1 | 普通导出 commit failure 回归 | Baseline | 测试 | 已完成 | 不重复实施 | 已完成 | L1 | Windows CTest L2 |
| HIST-B1 | DB -> Sample -> paged export 集成保护 | Baseline | 数据一致性 | 已完成 | 不重复实施 | 已完成 | L1 | Windows CTest L2 |
| EXP-D1 | export schema / aligned collision 决策 | Baseline | 产品合同 | 已确认 | 不重复决策 | 已完成 | N/A | 新产品变更另建任务 |
| OPC-D1 | callback HRESULT/null/quality/generation 合同 | Baseline | 生命周期 | 已确认 | 不重复决策 | 已完成 | N/A | OPC-1/VAL-2 使用既定合同 |
| DL-D1 | 正式/诊断下载链职责与 consumer 决策 | Baseline | 架构合同 | 已确认 | 不重复决策 | 已完成 | N/A | VAL-3 使用既定合同 |
| COMP-S1 | artifact bundle/generation/path 调查 | Baseline | 安全 | 已确认 | 冻结区不重复调查 | 已完成 | N/A | COMP-1 已按合同实施 |
| COMP-1 | generation staging + 一致性发布 + portable path | Baseline | 安全/数据丢失 / P1 | 已实施 | 不重复实现 | 已完成 | L1 | Windows CTest L2 |
| COMP-F1 | standalone Python compiler package/CLI release gate | Release gate | 误导性成功/损坏发行包 / P2 | 仅独立发布产品需求 + Compiler 写授权 | 冻结区；当前主程序不依赖 | 阻断 | L0 | 授权后 package build + CLI tests |
| DL-1A | Device connection ownership 第一实施轮 | Baseline | 生命周期 / P1 | 已实施 | 不重复实现 | 已完成 | L1 | 真机 VAL-3 达 L3 |
| BUILD-B1 | 删除 monitor host-specific `-march=native` | **After-Now** | 部署兼容性 / P2 | 第 6 节 correctness 稳定 | 单文件 CMake 最小删除 | 未开始 | L0 | Windows/旧 CPU build/run L2/L3 |
| WIN-B1 | Qt major CMake/README 统一 | **After-Now** | 构建合同 / P2 | 无 | 完成已有 CMake diff与文档一致性 | 进行中 | L0 | Windows clean build L2 |
| WIN-B2 | Windows exe/script 名称统一 | Baseline | 构建 / P1 | 已实施 | 不重复实现 | 已完成 | L1 | Windows clean build/script L2 |
| WIN-B3 | workflow_dev stdout/stderr 死锁 | **After-Now** | 开发工作流可靠性 / P2 | 第 6 节 correctness 稳定 | 单脚本最小修改 | 未开始 | L0 | Windows PowerShell L2 |
| OPC-1 | HRESULT/VARIANT/Quality/generation Windows 条件测试 | **Deferred-Windows** | Windows/COM P1 | OPC-D1 已完成 | 默认等 Windows | 阻断 | L0 | Windows CTest L2 |
| P3-1 | MainWindow/MonitorManager 定向 implementation split | Baseline | P3 | 已实施 | 不重复移动实现 | 已完成 | L1 | Windows CTest L2 |
| CLEAN-1 | 删除未构建重复/陈旧源文件 | Final | P3 | correctness 与当前 diff 稳定 | 只删除确认无调用的文件 | 未开始 | L0 | Windows clean build L2 |
| VAL-1 | Windows clean configure/build/CTest | Validation | 验收 | Windows 环境 | 无代码修改 | 阻断 | L0 | Windows 环境恢复后达到 L2 |
| VAL-2 | Windows + Matrikon OPC DA | Validation | 目标环境 | Windows + Matrikon | 无代码修改 | 阻断 | L0 | 目标环境恢复后达到 L3 |
| VAL-3 | 真实控制器 / 串口 / 下载链 | Validation | 目标环境 | 控制器/串口 | 无代码修改 | 阻断 | L0 | 目标环境恢复后达到 L3 |
| VAL-4 | 大数据库/长时间导出性能 | Validation | 性能 | 可运行规模环境 | 无代码修改 | 阻断 | L0 | 目标规模环境恢复后达到 L3 |

说明：

- 工作流只使用 `未开始 / 进行中 / 阻断 / 已完成` 四态；Windows/真机验证债务只记录在“后续验证”和验证等级中。
- `MON-1/WIN-B2/P3-1` 已有静态完成证据，记为 `已完成/L1`，只保留后续 Windows L2；不得按 v4.2 原始旧状态重复实施。
- `WIN-B1` 的 CMake 主体已经修改，但 README 合同尚未同步，因此仍为 `进行中/L0`。
- `VAL-1~4` 因其任务本身需要当前不可用环境，所以是 `阻断/L0`。
- `DL-1A` 是为了区分**已经完成的第一实施轮**与未来可能存在的第二轮。只有 VAL-3 提供真实证据后，才允许新建 `DL-1B`；当前不存在自动的第二轮实施任务。
- `COR-B1` 未完成前，不应让 PAR-B1 根据旧的裸 register 语义固化测试预期；PAR-B1 已有实现只能在现有 diff 上补齐，不能回退重写。
- `MON-I1` 调查已关闭；没有新的测量证据时不再派发。
- `COMP-F1` 是独立 Python package 的 release gate，不是当前主程序实施任务，也不解除 Compiler 冻结。

---

# 9. 参数与通信 correctness 批次合同

## 9.1 执行顺序

```text
COR-B1
typed codec + mapped error semantics + deviceId alias/default
      ↓
PAR-B1
partial-success + typed readback + backend lifetime + OPC confirmation/scope
```

`COMM-1` 不再单列；其内容已经并入 `COR-B1`。

`PAR-1/PAR-2` 不再分批；两者必须作为同一个状态机批次 `PAR-B1` 闭环，否则会出现“成功点进入 PendingReadback，但 overall false 让 wrapper 提前退出”的中间错误状态。

## 9.2 COR-B1 不允许扩大

禁止：

- 重写整个 Controller backend；
- 引入新的设备协议框架；
- 修改 Compiler；
- 修改真实下载协议；
- 扩展 RuntimePointValue 成新的错误模型；
- 一次实现大量项目当前不用的工业数据类型；
- 借 codec 修改顺带改变 DownloadProfile 的既有合同。

本次只能在已有 codec/Points 实现上补设备号 alias/default 规则；不得为了区分来源重写整个 RuntimePoint schema。若 backend 内可正确区分显式 metadata 与规范化 addressing，就不修改 `RuntimePointTypes.h`。

## 9.3 `QString` 数值输入合同

数值 RuntimePoint 接受 `QString` 的前提：

- trim 后非空；
- 整个字符串必须能被目标数值类型严格解析；
- 不允许 `"2.0abc"`、空字符串、仅部分 parse；
- 范围必须满足目标类型；
- 失败返回 `InvalidParameter`；
- 禁止 fallback 0。

`ParameterController` 当前以字符串保存用户编辑值，因此不能以“QVariant 是 QString”为由直接拒绝合法输入。

## 9.4 PAR-B1 的 batch / target scope

ParameterController 必须区分：

```text
UI Apply All
    -> 所有本次目标 Modified 参数

OPC Single Point Write
    -> 仅该 pointId
```

状态机以“本次目标参数集合”为边界计算最终成功/失败，不得把不属于本次请求的其他 Modified 参数纳入 OPC batch。

## 9.5 OPC 最终确认原则

OPC write 的“请求已受理/异步回读已启动”不是 write success。

只有参数达到：

```text
Confirmed
```

才允许：

```text
recordWriteResult(true)
publish new RuntimePointValue
quality = Good
```

失败状态不得覆盖上一已确认 Good 值。

## 9.6 参数回读比较必须由目标类型决定

```text
BOOL          -> canonical boolean
INT/DINT 等   -> target integer exact match
REAL/FLOAT32  -> float32 representation-aware match
```

不得继续把所有数值统一转 double 后套固定绝对 `1e-6`。比较失败必须表示真实设备值差异或非法 readback，而不是字符串格式差异。

## 9.7 借用 backend 的异步安全边界

backend ownership 仍属于注入方；本批只保证观察安全：

```text
QPointer/destroyed guard
      ↓
cancel pending readback/context
      ↓
failure once, no Good publish
```

这不是重新打开 DL-1A 的设备 ownership 决策，也不授权 ParameterController/RuntimeSession 删除外部 backend。

---

# 10. Monitor / DataManager correctness 批次合同

## 10.1 MON-1

核心是消除 data race，不改变监控业务语义。

推荐模式：

```text
lock
  ↓
copy config/name/threshold snapshot
  ↓
unlock
  ↓
threshold evaluation / emit
```

要求所有 config getter 与 threshold 读取都遵守同一锁策略，且不在持有 `m_mutex` 时触发外部可重入 signal。

## 10.2 DB-1

数据库变更只处理：

- `PRAGMA foreign_keys` / version table / version query 的失败传播；
- migration required index 完整性；
- schema_version 更新时机；
- connection cleanup；
- 非整数、负数和未来 schema version 的 hard rejection；
- 对应测试。

不新增 schema v5。

## 10.3 MON-I1

只读调查已经完成，不修改生产代码。

`MonitorChartView` 在 render timer 中主动 `drainAllDeltas()`，且没有其他生产 consumer 依赖该 signal 才 drain。只有未来新的测量证据证明延迟、CPU wakeup 或可见刷新问题时，才能新建代码任务。

---

# 11. EXP-D1：已完成产品决策

状态：`已完成 / N/A`，不得重复派发。

已确认当前合同：

- 逐 Sample 无损导出与 aligned 时间对齐视图是不同语义；
- 当前 aligned CSV/TSV 不承诺“同一 channel + 同一 timestamp 多条 Sample”逐条无损；
- 不把当前碰撞行为额外固化成新的 first-wins/last-wins 长期合同；
- `origin/errorCode/errorText` 是否进入 CSV/JSON/TSV 属于未来产品 schema 变更，当前不自动扩展。

若以后确实要改变导出 schema 或 aligned collision 语义，必须新建独立产品变更任务，不恢复 `EXP-D1`。

---

# 12. OPC-D1 已完成；OPC-1 延后 Windows

## 12.1 OPC-D1

状态：`已完成 / N/A`。

后续 OPC 实现与测试沿用已确认合同，包括：

- master callback failure 与 per-item failure 分层；
- null / 不可转换 value 不覆盖旧有效值；
- quality failure/null quality 不伪装 Good；
- generation/context 隔离；
- stop 后旧 callback 不污染当前状态。

不再派发“重新决定 OPC-D1”。

## 12.2 OPC-1

状态：`阻断 / L0`；执行顺序仍为 `Deferred-Windows`。

原因：

- 主体涉及 `Q_OS_WIN` / COM；
- 当前 macOS 无法证明新增 Windows 条件测试真实可编译；
- 它不阻塞 COR-B1 / PAR-B1 / MON-1 / DB-1。

Windows 环境恢复后，以已经确认的 OPC-D1 合同补完整测试矩阵。

---

# 13. DL-D1：已完成正式 consumer / 职责决策

状态：`已完成 / N/A`。

正式定位已经确认：

```text
RuntimeSessionController
 ↓
RunController artifact validation
 ↓
ControllerDeviceBackend
```

= 唯一正式项目下载链 / 正式 artifact consumer。

```text
DownloadDockWidget
 ↓
DownloadManager
 ↓
ControllerBridge
```

= 工程、诊断、手工下载工具。

该决策不得重新打开，除非后续目标环境验证出现与现有职责合同直接冲突的新证据。

---

# 14. COMP-S1：已完成；Compiler 继续冻结

状态：`已完成 / N/A`。

已确认：

- 一次编译按 artifact generation bundle 管理；
- mandatory artifact、staging、manifest commit point、consumer 路径合同已经追踪；
- `runtime_manifest.json` 是 generation 完整发布的重要提交点；
- 新路径按 manifest-relative / project-relative；
- legacy absolute path 只能在安全 root 范围内兼容。

`src/compiler/**` 与 `third_party/custom_dsp_language/compile/**` 仍视为冻结区。

> COMP-S1 已完成不代表以后对 Compiler 拥有永久写权限。任何新的 Compiler 修改仍必须重新给出最小文件清单并获得授权。

## 14.1 COMP-F1：standalone Python package release gate

全仓复审记录到冻结目录中的独立 Python package 当前存在：

- `pyproject.toml` console entry point 指向 `lh_compiler.cli.commands:main`，但 package 列表没有明确包含该子包；
- project metadata 引用当前目录缺失的 README；
- `compile` / `check` 命令会先说明“尚未实现”，随后仍输出成功结论并以成功状态退出。

当前主程序使用仓库根部 `lmc.py`，现有 CMake install 也不发布该 `cli` 子包，因此这不是当前 LH 主程序 blocker。

正式边界：

- 在修复并验证前，不得把 `third_party/custom_dsp_language/compile` 作为可用的独立 Python package/CLI 发布或宣传；
- 只有用户确认存在 standalone package 产品需求并明确授权 Compiler 冻结区后，才启动 `COMP-F1`；
- 授权后的最小验收必须包括 package build、隔离安装、console entry point import，以及未实现命令不能伪报成功；
- 不因本记录修改当前主程序 compiler/下载链。

当前状态：`阻断 / L0`，阻断原因是冻结区写授权与独立发行产品需求均未确认。

---

# 15. COMP-1：已完成 L1，禁止重复实现

状态：`已完成 / L1`。

当前源码已经具备并静态验收：

- 新 generation 隔离生成；
- mandatory artifacts 完整性/校验；
- `runtime_manifest.json` commit；
- manifest `complete/generationId` 校验；
- consumer 对 generation bundle 的完整性校验；
- manifest-relative / project-relative 路径；
- 发布失败旧 generation 继续可用。

当前剩余工作只有：

```text
VAL-1 Windows clean build + CTest
```

以及涉及真实下载的：

```text
VAL-3
```

不得重新实施“Artifact generation 一致性发布”。

---

# 16. DL-1A：第一实施轮已完成 L1

状态：`已完成 / L1`。

当前第一轮已经收敛：

- process-wide RTU port owner；
- owner 匹配才允许 release；
- DeviceBusy / busy fast-fail；
- ControllerDeviceBackend 操作级互斥；
- 正式链与诊断链端口冲突保护；
- 借用/注入 backend 的 stop/disconnect ownership；
- 诊断下载取消生命周期。

当前明确边界：

> 正式项目下载仍是同步调用；是否需要第二轮异步可取消改造，必须由 VAL-3 真机/UI 证据驱动。

因此当前**不存在默认 `DL-1` 第二轮代码任务**。

若 VAL-3 证明同步正式下载导致无法接受的 UI cancel / reconnect 行为，再新建 `DL-1B`，重新列精确文件并授权。

---

# 17. Windows 前置整理批次

当 Windows 环境即将恢复时，再集中执行：

```text
BUILD-B1 删除 host-specific -march=native
 ↓
WIN-B1 完成 Qt5 CMake/README 一致性
 ↓
WIN-B3 修复 workflow_dev 双流输出
 ↓
WIN-B2 只做既有 LH.exe 修改的脚本复验
 ↓
OPC-1 Windows 条件测试
 ↓
VAL-1 clean configure/build
 ↓
targeted CTest
 ↓
full CTest
```

如果期间发现 Windows build error，只修真实 build blocker，不借机做 Qt6 migration 或大范围结构整理。

---

# 18. P3-1：结构整理

状态：`已完成 / L1`。

当前工作树已经完成并注册：

- `MainWindowOutput.cpp` implementation split；
- `MainWindowMonitor.cpp` implementation split；
- `MonitorManagerHistory.cpp` / `MonitorManagerPolling.cpp` implementation split。

静态方法集合与 CMake source 注册已经核对，没有发现漏定义或重复定义。该任务只欠 Windows clean build/CTest L2，不得恢复旧状态再次移动代码。

## 18.1 CLEAN-1：未构建重复/陈旧源文件清理

全仓复审发现：

- `src/communication/CANCommon.cpp` 重复实现 `CANCommon.h` 中已有 inline 函数，且未进入 CMake；把它加入构建反而会产生重定义风险；
- `tests/project_controller_stub.cpp` 属于旧测试拓扑，当前没有 CMake target 或调用方；
- `src/compiler/dummy.cpp` 同样未构建，但位于 Compiler 冻结区，不纳入本任务。

正式合同：

- 只删除已确认无引用、无构建目标且不承担资源/发行用途的前两个文件；
- 不为保留死文件新增 CMake 条目、wrapper 或注释；
- 不触碰 `src/compiler/dummy.cpp`；
- 删除前再次 scoped search，删除后做 clean build/CTest 验证。

状态：`未开始 / L0`，放在 correctness 与当前未提交实现稳定之后；Windows clean build 后到 L2。

---

# 19. Windows 与目标环境验证队列

## 19.1 VAL-1：Windows clean build + CTest

必须使用全新 build dir：

```text
clean configure
 ↓
clean build
 ↓
targeted CTest
 ↓
full CTest
```

不得使用旧 build 目录证明当前源码。

重点覆盖：

- COR-B1；
- PAR-B1；
- MON-1；
- DB-1；
- COMM-B2；
- OUT-B1；
- 已完成基线中的 EXP-1 / HIST-B1 / COMP-1 / DL-1A 回归；
- BUILD-B1 / WIN-B1 / WIN-B2 / WIN-B3；
- P3-1，以及 CLEAN-1（若已实施）；
- OPC-1。

## 19.2 VAL-2：Windows + Matrikon OPC DA

至少：

- COM activation；
- server connection；
- browse；
- AddGroup；
- AddItems；
- sync read/write；
- subscription callback；
- value/quality/timestamp；
- partial item failure；
- master failure；
- disconnect/reconnect；
- late callback；
- server unavailable。
- 参数 BOOL/REAL 写入在 readback Confirmed 后才发布 Good；
- pending readback 期间 backend/controller stop 或替换不得崩溃、误报 success 或污染新 session。

## 19.3 VAL-3：真实控制器

完整：

```text
Compile
 ↓
Artifact generation
 ↓
Manifest
 ↓
Precheck
 ↓
Download
 ↓
Verify
```

额外必须验证 COR-B1 的真实寄存器编码与错误语义：

- REAL；
- INT / DINT signed integer；
- multi-register byte/word order；
- scale/offset（若实际配置使用）；
- elementCount -> registerCount；
- read/write access permission；
- 仅配置 `slaveId` 等 alias 时访问正确 deviceId；
- 冲突/非法设备号在发起 I/O 前拒绝；
- invalid value 必须在写设备前被拒绝；
- transport failure 不得被覆盖成 InvalidAddress；
- Modbus outer timeout 必须报告 ReceiveTimeout，不得报告 NoError。

异常：

- 连接中断；
- retry；
- cancel；
- reconnect；
- device reject；
- verify failure；
- 串口占用；
- 两工具链冲突；
- Monitor/Download 竞争。

## 19.4 VAL-4：大数据库 / 长时间导出

至少：

- 大量历史记录；
- 多 channel；
- 小 page size；
- bounded memory；
- DB 查询耗时；
- export throughput；
- 无重复/遗漏；
- provider failure 旧文件保留；
- commit failure 旧文件保留；
- DB-1 required indexes 实际存在；
- DB-1 对非法/未来 schema version hard-fail 且不改写数据库；
- 若 MON-I1 后续形成生产代码任务，再把对应性能/刷新行为加入 VAL-4；否则不把调查项伪装成待验证代码。

---

# 20. 测试与临时文件规则

继续沿用：

- `QTemporaryDir` 优先；
- 不写固定仓库输出目录；
- 不访问用户真实数据库；
- 不依赖上一轮测试遗留文件；
- 不依赖 `sleep`；
- 时间相关测试优先固定 UTC；
- 单例测试必须 cleanup；
- 错误路径测试同时断言操作结果和具体错误码，禁止只断言 `false`；
- QObject 生命周期测试用事件循环/确定性触发销毁，不使用固定 `sleep` 猜时序；
- 文件保存失败测试必须验证旧文件内容仍存在；
- 不为了测试方便新增 production-only reset API，除非现有生命周期确实不足并单独授权。

---

# 21. 冻结区与授权

## 21.1 Compiler

冻结：

```text
src/compiler/**
third_party/custom_dsp_language/compile/**
```

未经用户明确批准最终最小文件清单，不写。

`COMP-F1` 只记录 standalone package release gate，不构成写授权。

## 21.2 高风险跨模块批次

以下实施前先列精确文件：

- COR-B1（尤其是当前本地已有 codec/Points 改动）；
- PAR-B1（跨 ParameterController + RuntimeSessionController + RuntimeSessionOpc 时）；
- 任何新的 Compiler 修改；
- 任何未来 `DL-1B`；
- OPC 生产实现修复；
- 任何试图重新打开已完成 P3-1 的结构移动。

`COMP-1` 与 `DL-1A` 已完成，不得作为“高风险待实施任务”重新列入。

不允许用“顺手整理”为理由扩大范围。

---

# 22. 当前派发顺序

## 22.1 现在的代码实施顺序

```text
COR-B1   （进行中；先 scoped diff，继续现有本地修改）
↓
PAR-B1
↓
DB-1
↓
COMM-B2
↓
OUT-B1
```

其中：

- `COR-B1` 只补当前设备号 alias/default 冲突闭环，不重复已有 codec；
- `PAR-B1` 在现有 partial-success/OPC 实现上补类型化 readback 与 backend 销毁安全；
- `DB-1` 只补非法/未来 version guard 与测试，不新增 schema；
- `COMM-B2`、`OUT-B1` 是独立小修，不扩展成协议或文件服务重构；
- 共享文件禁止并行写入。

## 22.2 correctness 后处理

```text
BUILD-B1
↓
WIN-B1
↓
WIN-B3
↓
CLEAN-1
```

`WIN-B2` 已完成只复验；`CLEAN-1` 只删除两项确认死文件。以上不抢占第 22.1 节。

## 22.3 已完成、不得重复派发

```text
EXP-1
HIST-B1
EXP-D1
OPC-D1
DL-D1
COMP-S1
COMP-1
DL-1A
MON-1
MON-I1
WIN-B2
P3-1
```

## 22.4 当前不抢先做

```text
OPC-1
COMP-F1
VAL-1/2/3/4
```

`OPC-1` 与 VAL 队列依赖 Windows/目标环境；`COMP-F1` 依赖独立发行需求和冻结区授权。当前不应抢占 correctness 修复。

---

# 23. 最终原则

当前阶段统一原则：

> **Mac 上继续修真实 correctness 问题；Windows 负责后续统一 L2/L3 验证。已经完成的 COMP/DL/EXP/OPC 决策与实现不得因旧文档状态回退而重复实施。**

具体要求：

1. v4.2 激活后，当前任务状态以 v4.2 唯一任务表为准，不再从 v3/v4/v4.1 或历史 Handoff 恢复旧“未开始”状态；
2. 当前本地工作树已有改动必须先 scoped diff，再在原改动上继续，禁止用远端 `main` 覆盖；
3. 优先处理会造成设备写错、状态错报、错误码丢失和数据竞争的问题；
4. v4 COR-1 与 COR-B1 原 A~G 主体视为已有实现；当前只补设备号 alias/default 的显式来源、非法值和冲突检测；
5. DSL canonical type 与 backend compatibility alias 必须分开表述；
6. `elementCount` 是逻辑元素数，register count 由类型宽度推导；
7. v1 addressing 的 `offset` 只表示物理偏移；在保存原始 legacy 来源信息之前，不实现 `offset` 寄存器地址 fallback，旧配置必须显式使用 `address/regAddress/registerAddress`；
8. scale/offset encode/decode 必须成对定义，整数不得静默舍入；
9. 显式 `slaveId/stationAddress/serverAddress` 不得被自动 `unitId=1` 抢占；冲突或非法 alias 必须在 I/O 前 hard-fail；
10. mapped read/write 的 access violation 使用 `PermissionDenied`，真实 transport error 必须原样保留；
11. Parameter partial-success 必须让成功点继续 readback；同步 API 返回最终结果，异步 API 的 bool 只表示是否成功启动，mixed-result 最终仍通过 `readbackFinished(false)` 失败；
12. 同步/异步 readback 必须共享一致的最终判定；BOOL/整数/REAL 回读必须按目标类型比较；
13. 借用 backend 销毁或替换时必须安全取消 pending readback，不解引用失效对象、不遗留 Pending、不发布 Good；
14. OPC 单点写只能作用于该 pointId，不能提交其他 UI Modified 参数；
15. OPC 只有 readback `Confirmed` 后才能 record success / publish `Good`，并必须在完成、stop、backend/controller 切换时清理本次 pending point/context；
16. MonitorChannel 线程安全与 P3 split 已完成 L1；`MON-I1` 已关闭且没有生产修改授权；
17. DataManager 初始化必要 SQL、version query、required indexes、非法/未来 version 均必须有 hard-failure 语义；
18. Modbus 任一失败路径不得产生 `CommErrorCode::NoError`，外层等待超时固定为 `ReceiveTimeout`；
19. 输出日志保存使用原子 commit，失败不破坏旧文件且不得提示成功；
20. 发布构建不得使用 `-march=native`，PowerShell workflow 必须并发消费双重定向输出；
21. `WIN-B2/P3-1/COMP-1/DL-1A` 已完成 L1，只补 L2/L3，不重复实现；
22. `COMP-F1` 只是冻结区独立发行 gate，当前不修改、不发布 standalone package；
23. Windows 构建、Matrikon、真机和大规模性能验证统一保留在后续 L2/L3 队列；
24. Windows 暂不可用不再成为普通逻辑修复的阻断理由；
25. 任何涉及 Windows-only 代码的 L1 结论必须明确注明“未完成 Windows 编译”。

当前普通代码批次最高合格结论：

> **静态验收通过；Windows 编译与运行测试待补充。**
