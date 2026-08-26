# LH 项目当前阶段任务需求合同

> 版本：v4.2（2026-08-26 未完成任务精简合并版）
> 适用仓库：`D3m0n100/LH`
> 当前源码基线：`main@911910e8e4bb2185a773b3abae8a654b6ce5a2c6`
> 当前工作环境：macOS
> 当前唯一任务范围：本文件只记录未完成任务、条件性验证和延期风险；已完成任务及其实施清单已删除。

---

# 1. 文档目标与执行口径

本合同只回答四件事：

1. 当前仍需实施的 correctness / reliability 修复；
2. 必须依赖 Windows、Matrikon、真实设备或规模环境的验证；
3. 尚无生产缺陷证据、但必须保留触发条件的延期风险；
4. 每项任务允许修改到什么边界、如何验收。

已达到 L1/L2/N/A 的历史任务不再出现在任务表、派发顺序或“不得重复实施”清单中。历史完成证据保留在 Git 历史与 `Handoff.md`，不得根据旧版 v3/v4/v4.1 的状态恢复任务。

执行原则：

- 先追踪当前生产调用链，再决定是否修改；
- 没有缺陷证据的验证项不得自动扩大为重构；
- 备用/公开 API 没有仓库内调用方，不等于确认没有外部调用方；
- 能通过删除、隐藏、严格拒绝或复用现有实现解决时，不新增框架；
- Windows 暂不可用不阻止跨平台逻辑修复达到 L1，但 Windows/COM/真机任务本身可保持阻断；
- 当前工作树中的用户修改不得被回退、覆盖或全局格式化。

---

# 2. 状态与验证等级

任务状态只使用：

- `未开始`：尚未实施；
- `进行中`：已有本批有效工作，但未满足完成条件；
- `阻断`：必须等待产品决策、外部兼容确认、Windows/Matrikon、真实设备或规模环境；
- `已完成`：任务完成后从本合同任务表删除，并在 Handoff/Git 历史记录证据。

验证等级：

- `L0`：未实施或未验证；
- `L1`：代码与直接调用链静态验收通过；
- `L2`：Windows clean configure/build 与相关 CTest/UI/脚本运行通过；
- `L3`：Matrikon、真实控制器、网络部署或目标规模环境验证通过；
- `N/A`：纯产品/API 决策。

当前环境缺少可用的 CMake/CTest/Qt 构建链，且未连接真实设备。因此不得把 L1 写成“编译通过”“测试通过”或“设备联调通过”。

---

# 3. 唯一未完成任务表

| ID | 任务 | 优先级 | 当前状态 | 当前目标 | 后续验证 |
|---|---|---:|---|---|---|
| OPC-1 | OPC 真实可用状态与 Windows 回调/读写测试矩阵 | P1 | 进行中 / L1 | 阶段状态、失败传播、quality/null/master/per-item HRESULT 已完成静态实施 | VAL-1、VAL-2 |
| MON-IO-1 | 监控后端同步 I/O 超时与 UI 阻塞验证 | P1 条件项 | 阻断 / L0 | 先测量，不预设重构 | VAL-3 |
| NET-1 | Ethernet 监听、安全边界、多客户端与消息合同 | P1/P2 条件项 | 进行中（决策门仍阻断） / L1 | UDP 完整报文与地址输入边界已收口；待确认部署边界，再做 TCP/API 最小实现 | VAL-1、目标网络环境 |
| DB-RET-1 | 生产数据库清理、UTC 与事务 | P2 | 进行中 / L1 | UTC cutoff、跨表事务、失败回滚与生产调用已实施 | VAL-1、VAL-4 |
| EXP-CSV-1 | CSV/TSV 字段转义与公式注入防护 | P2 | 进行中 / L1 | 普通/分页、单/多通道共享字段编码与公式防护已静态实施 | VAL-1、VAL-4 |
| SQL-LOG-1 | SQL 错误日志脱敏 | P2 | 进行中 / L1 | DataManager 统一错误日志仅保留 SQL 模板、参数摘要与必要错误文本 | VAL-1 |
| BUILD-CANCEL-1 | 编译取消的 generation/单终态保护 | P2 | 进行中 / L1 | QProcess 非阻塞取消、generation 过滤、Cancelling/Cancelled 单终态与最小回归测试已实施 | VAL-1、Windows UI |
| DB-QUERY-1 | 历史查询区分成功空结果与查询失败 | P2 | 未开始 / L0 | 收敛生产调用边界 | VAL-1、VAL-4 |
| COMM-LIFE-1 | Modbus 超时 reply 生命周期、重入与并发验证 | P2 条件项 | 未开始 / L0 | 先验证；有证据才改生产代码 | VAL-1、VAL-3 |
| VAL-1 | Windows clean configure/build/CTest | 验收 | 阻断 / L0 | 等待 Windows | 达到 L2 |
| VAL-2 | Windows + Matrikon OPC DA | 目标环境 | 阻断 / L0 | 等待 Windows + Matrikon | 达到 L3 |
| VAL-3 | 真实控制器、串口、Modbus、CAN 与下载链 | 目标环境 | 阻断 / L0 | 等待真实设备 | 达到 L3 |
| VAL-4 | 大数据库、长期采集与长时间导出 | 性能 | 阻断 / L0 | 等待目标规模环境 | 达到 L3 |

任务表之外的事项默认不得抢占 P1/P2。延期风险见第 8 节。

---

# 4. P1 任务合同

## 4.1 OPC-1：真实可用状态与 Windows 验证

### 已确认问题

`MatrikonOpcServer::start()` 已收口为分阶段执行：COM/server/group 创建仍 hard-fail，browse、AddItems、refresh、read probe、subscription 的失败会保留在错误与状态快照中；`m_running` 只表示生命周期，`online` 按实际能力计算，Windows/COM 行为仍待目标环境验证。

### 正式合同

OPC 状态至少区分：

```text
COM initialized
server connected
group created
configured items available
read probe available
subscription active
degraded / operational / offline
```

- `isRunning()` 只表示生命周期运行时，不得自动等同于业务 `online`；
- `statusSnapshot.online` 必须依据配置所需的最小可用条件；
- 显式配置的必需 tag 添加失败，不得被清成 `NoError`；
- browse 在显式 itemId 已足够时可以是可选能力，但其失败必须进入状态快照；
- subscription 是否必需由配置模式决定，不得无条件忽略失败；
- read probe、quality、null value、master/per-item HRESULT 与 generation 隔离沿用现有行为合同；
- 不在 macOS 上声称 Windows COM 分支已验证。

### Windows 最低验收

- activation/server/group/AddItems 各阶段独立失败；
- 无 tag、有全部合法 tag、部分 tag 失败；
- sync read/write、subscription、quality/null payload；
- master/per-item HRESULT；
- stop/reconnect 后迟到 callback 被 generation 丢弃；
- running、online、degraded 和 last error 与实际阶段一致。

## 4.2 MON-IO-1：同步 I/O 先测量后决定

### 风险

监控轮询定时器在 MonitorManager 所在线程直接调用 backend `readPoints()`。若真实设备调用阻塞，同一 Qt 事件循环中的 UI、定时器和其他操作都会延迟。

### 调查合同

- 在真实 Controller/Modbus/OPC backend 记录 `readPoints()` 正常、超时、断线和重连耗时；
- 同时观察 UI 事件循环响应、轮询重入和其他任务延迟；
- 确认 backend 是否已有严格非阻塞合同或内部异步实现；
- 没有可见阻塞或超时超出采样预算的证据时关闭调查，不改生产代码；
- 有证据时优先在共享设备 I/O 边界做最小异步/串行化，不重构整个调度器。

## 4.3 NET-1：Ethernet 部署与数据合同

### 决策门

先确认：

- TCP server/UDP 是否为正式产品能力；
- 是否允许非本机客户端；
- 部署网络是否可信；
- server send 是广播还是单客户端响应；
- 上层消费的是字节流、数据报还是业务帧。

若服务端不会对外提供，最小处理是默认只允许 loopback，并拒绝含糊的空 bind 地址；不得提前建设 TLS/账号系统。

### 正式合同

- `Any` 监听必须由明确配置触发，空 host 不得隐式扩大监听范围；
- 对非可信网络开放时，必须在发布前明确 TLS/鉴权/ACL 的责任边界；
- TCP 多客户端接收状态必须保留 sender 身份或按客户端隔离，不得共用一个无法归属的同步缓冲；
- TCP 是字节流，业务消息边界必须由上层 framing 合同明确；
- UDP 每个 datagram 保持独立，不得在无合同情况下拼接为一帧；
- server send 的广播语义必须显式；若需要请求/响应，应提供明确目标客户端；
- keep-alive 要么使用可验证的现有实现，要么删除/禁用对应配置和空转定时器；
- 不新增与部署需求无关的通用网络框架。

### 最低验收

- loopback、显式 Any、非法 bind 地址；
- 两个 TCP 客户端交错发送时不混淆归属；
- 分段/合并 TCP read 不被误写成业务帧保证；
- 多个 UDP datagram 不被拼接；
- 广播或定向发送符合选定合同；
- keep-alive 配置与实际行为一致。

---

# 5. P2 任务合同

## 5.1 DB-RET-1：生产清理、UTC 与事务

- 生产清理定时器除内存 purge 外，还必须在 DataManager 已初始化且数据库记录启用时触发持久化清理；
- 内存和数据库使用同一 UTC cutoff 语义；
- `runtime_data` 与 `system_logs` 删除在同一事务中提交，任一步失败整体回滚并返回明确失败；
- 不把定期 `VACUUM` 纳入清理路径；VACUUM 是否需要只能由 VAL-4 数据决定；
- 第一轮保持简单同步删除；只有 VAL-4 证明锁持有或删除耗时不可接受时，才增加分批清理。

最低验收：生产调用存在；时区边界一致；第二个 DELETE 失败时第一个 DELETE 回滚；失败不记录“清理完成”。

## 5.2 EXP-CSV-1：字段编码与公式注入

- 为 CSV/TSV 普通、分页、单通道、多通道路径复用一个字段编码函数；
- 正确处理分隔符、双引号、CR/LF 和空字段；
- 双引号按 CSV 规则加倍，必要字段整体引用；
- 对 channel/displayName/unit/projectName/custom metadata 等用户可控文本，若原值首字符为 `= + - @`，第一轮统一在原值前加单引号，再执行 CSV/TSV 引用；
- 数值列保持数值，不把合法负数当作用户文本公式；
- JSON 语义不随本任务改变。

最低验收：逗号、引号、换行、Unicode、公式前缀、空字符串及普通文本在所有导出路径一致。

## 5.3 SQL-LOG-1：错误日志脱敏

- 保留操作描述、SQL 模板/语句类型、数据库 native error code 与必要错误文本；
- 默认不记录全部绑定值；
- 如确需定位参数，仅记录参数名、类型、长度或经过统一脱敏/截断的值；
- 设备地址、业务值、token/password/cookie/key 等不得以原值进入日志；
- 不为此引入新的日志框架。

## 5.4 BUILD-CANCEL-1：取消单终态

- 每次编译具有 generation/operation id 或等价状态保护；
- 取消进入明确的 Cancelling/Cancelled 语义，在底层进程真正终止前不得允许旧回调污染新编译；
- `finished/error` 迟到回调只能完成所属 generation；
- 一次取消只能产生一个终态，不得随后再发成功或普通失败；
- 取消不得在 GUI 线程同步等待数秒；
- 不要求建设通用作业系统，复用现有 QProcess 状态和最小成员即可。

最低验收：取消前、启动后、即将正常完成时取消；取消后立即启动新编译；旧 finished/error 不改变新任务状态。

本轮 macOS L1 实施：`DSLCompilerInterface` 为异步进程分配 operation generation，取消时立即解绑并终止旧 `QProcess`，按 sender identity 丢弃迟到信号；`BuildController` 使用 `Idle/Compiling/Cancelling` 状态并只接受当前 generation 的完成回调；已补最小取消与新 generation 回归测试。Qt/CTest、取消时序和 Windows UI 仍待目标环境验证。

## 5.5 DB-QUERY-1：空结果与失败分离

- 先迁移生产使用的历史查询入口到现有 result/status-bearing page/count API；
- 成功空列表、未初始化和 SQL error 必须可区分；
- 不要求一次删除全部 legacy list-returning API；无生产调用方的旧 API 记录到延期台账；
- UI/导出不得把数据库故障显示成“没有历史数据”。

## 5.6 COMM-LIFE-1：Modbus 生命周期与并发验证

当前只确认：外层 guard timeout 已映射为 `ReceiveTimeout`，每次请求有独立 `QModbusReply`；尚无证据证明旧响应会污染新缓存。

本任务先验证：

- timeout 后 reply 的父对象、最终释放和信号连接生命周期；
- 嵌套事件循环期间同一接口是否可重入发起第二个 read/write；
- 同线程重入、跨线程误用和连续 timeout 的行为；
- 迟到 reply 是否可能触发 cache/signal/operation state 更新；
- Qt Modbus client 是否已经提供足够的队列串行语义。

验收结论只能是以下之一：

1. 证明现有行为安全，补线程/串行契约与最小测试后关闭；
2. 发现可复现风险，在共享 operation 边界增加最小 in-flight guard、generation 或 reply 清理；
3. 只能由真机复现，保留到 VAL-3。

不得在无证据时把它描述成“旧响应必然污染新请求”，也不得借此重写 ModbusInterface。

---

# 6. 验证队列

## 6.1 VAL-1：Windows clean build + CTest

必须使用全新 build 目录：

```text
clean configure
  -> clean build
  -> targeted CTest
  -> full CTest
```

重点覆盖：

- 本合同所有达到 L1 的 C++/Qt/CMake 修改；
- RuntimePoint typed codec、参数 readback/OPC confirmation、DataManager migration；
- Modbus timeout 错误语义；
- 分页历史与 QSaveFile 导出回归；
- artifact generation、正式/诊断下载 ownership；
- Windows 脚本与 Qt5 构建合同；
- OPC Windows 条件源文件至少可编译。

不得使用旧 build、旧 CTest 注册或旧二进制作证据。

## 6.2 VAL-2：Windows + Matrikon

执行 OPC-1 的完整最低验收，并记录：

- Windows、Qt、Matrikon 版本；
- ProgID、连接配置和 callback 线程；
- activation、browse、group/item、读写、订阅；
- partial/master failure、quality/null、disconnect/reconnect、late callback；
- running/online/degraded 状态与真实能力的一致性。

## 6.3 VAL-3：真实设备

至少覆盖：

- 空串口配置在 I/O 前拒绝；
- 明确设备号 alias 访问正确从站，冲突/非法值在 I/O 前拒绝；
- REAL/INT/DINT、byte/word order、scale/offset、elementCount；
- Modbus timeout、连续请求、reply 迟到、断线、retry、reconnect；
- Monitor 与 Download 竞争；
- MON-IO-1 的 backend 耗时与 UI 响应测量；
- 正式 artifact generation -> manifest -> precheck -> download -> verify；
- 诊断取消与端口 ownership。

## 6.4 VAL-4：目标规模数据

至少覆盖：

- 长时间数据库记录与自动清理；
- UTC retention 边界与清理事务失败；
- 大量 channel、历史 keyset 分页、小 page size；
- 峰值内存、查询耗时、导出吞吐；
- CSV/TSV 特殊字段与公式防护；
- provider/write/commit failure 保留旧文件；
- 清理、查询、导出并发时的数据库响应。

---

# 7. 执行顺序与文件边界

## 7.1 当前可执行顺序

```text
DB-RET-1
  -> DB-QUERY-1
  -> EXP-CSV-1
  -> SQL-LOG-1
  -> BUILD-CANCEL-1
  -> COMM-LIFE-1（先验证）
```

- `DB-RET-1`、`DB-QUERY-1` 与 `SQL-LOG-1` 共享 DataManager，顺序实施，不并行写；
- `EXP-CSV-1` 必须一次覆盖普通与分页实现，避免继续形成双重字段语义；
- `COMM-LIFE-1` 默认是验证任务，没有证据不得修改生产状态机；
- `NET-1` 先确认部署/API 决策，可与不共享文件的数据库任务独立推进。

## 7.2 条件/环境任务

```text
NET-1 部署决策
MON-IO-1 + VAL-3
OPC-1 + VAL-1 + VAL-2
VAL-4
```

## 7.3 默认文件边界

实施前必须重新做 scoped search，最终文件以当前调用链为准。预计边界：

| 任务 | 默认文件范围 |
|---|---|
| OPC-1 | `MatrikonOpcServer.*`、`IOpcServer.h`、OPC/runtime session tests；Windows 条件实施 |
| NET-1 | `EthernetInterface.*`、通信配置与最小多客户端测试 |
| DB-RET-1 | `DataManager.*`、`MonitorManager.*`、DataManager/monitor tests |
| EXP-CSV-1 | `MonitorExportHelper.*`、普通与分页导出 tests |
| SQL-LOG-1 | `DataManager.*` 及错误日志测试 |
| BUILD-CANCEL-1 | `BuildController.*`、`DSLCompilerInterface.*`、`tests/dsl_compiler_cancellation_test.cpp`、`tests/main_window_integration_test.cpp`、`tests/CMakeLists.txt` |
| DB-QUERY-1 | `DataManager.*`、`MonitorManagerHistory.cpp`、生产调用方与相关测试 |
| COMM-LIFE-1 | 默认只读；有证据后才列 `ModbusInterface.*` 与最小测试 |

`BUILD-CANCEL-1` 若需要修改 `src/compiler/**`，必须先按第 9 节取得新的最小文件授权；不得把 Designer 状态修复扩展成 Compiler 重写。

---

# 8. 延期风险台账

这些项目仍未关闭，但当前没有足够证据或优先级进入 v4.2 P1/P2 实施队列。不得删除风险记录，也不得自动改代码。

| ID | 风险 | 当前判断 | 升级条件 | 默认处理 |
|---|---|---|---|---|
| PROTO-1 | ProtocolDetector 会主动发送探测帧，CANOpen/J1939 帧模型和 timeout 还有问题 | 无仓库内生产调用证据，但属于公开工厂能力 | 确认外部 consumer 或产品继续支持自动探测 | 无外部调用则隐藏/删除；继续支持则增加人工确认/安全合同并修帧 |
| SER-FRAME-1 | SerialInterface 的 ModbusRTU/Custom 分帧不完整 | 正式 Modbus 使用 ModbusInterface；备用公开 API 仍可能有外部 consumer | 产品确认使用该 API 或外部兼容要求 | 无 consumer 则隐藏/删除；否则补长度/CRC/边界 |
| SCHED-1 | TaskScheduler 启动 1 ms timer 并线性扫描 | 当前没有生产任务注册，但 timer 实际空转 | 接入生产任务或测得 CPU/唤醒影响 | 当前 P3；届时提高 tick 或按最近 deadline 调度 |
| THREAD-1 | MonitorDataLogger/MonitorManager/DataManager 线程归属主要靠隐含约定 | 当前正常路径主要在主线程，未确认数据损坏 | logger/backend 迁入 worker，或出现跨线程调用证据 | 先加线程契约/断言；不先做线程池或多连接框架 |
| MON-API-1 | 更新已有 channel 不发 `channelsChanged`；负数 recent count 返回大量数据；异常 channel 可自动注册 | 小型 API 边界问题，当前调用主要受控 | UI stale 可复现、API 外部开放或异常输入可达 | 合并成最小输入/通知修复，不建设新 registry |
| BUILD-HYG-1 | `mkpath()` 结果、静态/共享重复 target、测试 target 偏重 | 构建卫生/维护成本，非当前发布 blocker | Windows clean build 暴露失败或构建时间成为问题 | 最小删除重复 target/检查返回值；不重写测试架构 |
| DB-HYG-1 | `data_summary`、部分索引、同步 VACUUM 与 legacy 查询 API | schema 兼容与维护性风险；无生产 VACUUM 调用 | VAL-4 证明空间/锁/查询问题，或计划 schema 升级 | 不直接删表；随 schema/性能证据处理 |
| ARCH-1 | MainWindow、ConfigTypes、Communication/CommFactory、Monitor API 和日志体系耦合较重 | 架构气味，不是已确认 correctness 缺陷 | 修改冲突、编译成本、缺陷重复或测量指标持续恶化 | 只做有证据的定向拆分/删除，不启动总体架构改造 |
| COMP-META-1 | standalone Python package 仍有占位发布元数据，REPL 未实现 | 主程序不依赖；compile/check 已诚实失败 | 再次对外发布 standalone package | 发布前修元数据/命令合同；Compiler 冻结区需重新授权 |
| LEGACY-1 | 旧错误信号、重复版本字符串、TODO/占位实现 | 兼容与清理债务 | 外部兼容范围确认、正式版本发布或对应功能进入产品路径 | 精确删除/统一，不按 TODO 数量批量立项 |

---

# 9. 冻结区、安全边界与回归不变量

## 9.1 Compiler 冻结区

```text
src/compiler/**
third_party/custom_dsp_language/compile/**
```

任何新写入必须先列最终最小文件清单并获得用户明确授权。过去的狭窄授权不自动延续。

本轮 BUILD-CANCEL-1 一次性最小授权（2026-08-26）：用户明确授权修改 `src/designer/BuildController.h`、`src/designer/BuildController.cpp`、`src/compiler/DSLCompilerInterface.h`、`src/compiler/DSLCompilerAsync.cpp` 及对应最小测试/CTest 注册文件；不解除 `src/compiler/**` 其他文件和 `third_party/custom_dsp_language/compile/**` 的冻结。

## 9.2 高风险任务

以下任务实施前必须确认精确文件和直接调用链：

- OPC-1 的 Windows-only COM 路径；
- NET-1 的外部 API/部署兼容；
- COMM-LIFE-1 在形成生产修改时；
- 任何设备 ownership、下载协议或 Compiler 修改。

## 9.3 不可回退的现有行为

新任务不得破坏：

- RuntimePoint typed codec、严格数值/alias/access/error 语义；
- 参数 partial-success、typed readback、backend 销毁保护和 Confirmed 后发布；
- DataManager migration hard-failure 与 schema version guard；
- Modbus timeout 不产生 `NoError`；
- keyset 历史分页与错误 metadata；
- QSaveFile 导出/日志保存失败时保留旧文件；
- artifact generation/manifest commit 与安全相对路径；
- 正式/诊断下载 ownership 与端口互斥；
- MonitorChannel 锁内快照、锁外 emit；
- Qt 5.15+ 产品基线和现有 Windows target 名称。

---

# 10. 测试与证据规则

- 使用最小、确定性测试；非必要不新增测试框架；
- 优先 `QTemporaryDir`，不写固定仓库输出，不访问用户真实数据库；
- 时间测试使用固定 UTC，不依赖固定 `sleep` 猜时序；
- 错误路径同时断言操作结果、错误码和外部状态未被破坏；
- 配置/文件/数据库事务失败测试必须验证旧状态仍存在；
- QObject 生命周期测试使用事件循环和确定性销毁；
- 多客户端网络测试必须保留 sender/datagram 边界；
- 条件性风险必须记录“已证明安全”“已复现缺陷”或“仍需目标环境”之一，不允许用模糊措辞关闭；
- 每批完成后更新 Handoff，并从本合同任务表删除已完成任务；后续验证债务只保留在 VAL 队列。

当前普通跨平台代码任务的最高合格结论：

> 静态验收通过；Windows 编译与运行测试待补充。
