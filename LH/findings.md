# LH 当前发现与风险台账

## Current Status Index

| 范围 | 当前状态 | 说明 |
|---|---|---|
| P0-1 DSL 编译器语义 | 冻结/待授权 | 本轮不修改编译器目录 |
| P0-2 无 profile 下载 | 静态完成-待运行验证 | 已拒绝假下载，需可用 Qt/CMake 环境验证 |
| P0-3 项目保存/关闭 | 静态完成-待运行验证 | 已收紧保存、回滚和候选验证 |
| P1 运行/通信/监控批次 | 静态完成-待运行验证 | 需目标测试、硬件或平台环境 |
| OPC/监控线程模型 | 静态完成-待运行验证 | Windows COM/Matrikon 仍需真实环境 |
| 第八批诊断快照 | 静态完成-待运行验证 | 递归脱敏、原子提交和专项测试已完成 |

## Open Findings

### 下一批候选：历史查询/导出分页与流式处理（只读分析中）

- `DataManager::queryHistory()` 当前按时间升序读取整个闭区间并一次返回 `QList<RuntimeRecord>`；没有页大小、游标或错误结果，SQL 失败与“无数据”在调用方均表现为空列表。
- `MonitorManager::historyFromDatabase(start, end)` 一次取得全部 `RuntimeRecord`，随后再复制转换成完整 `QList<Sample>`；数据库锁覆盖整个查询和逐行物化过程。
- `MonitorWidget::onExportData()` 在选择文件前先构造完整 `ExportDataPackage`；数据库来源会先 flush，再为所选通道汇总完整历史数据。
- `MonitorExportHelper` 的 CSV/TSV 路径先构造完整 `QString`、再转为完整 `QByteArray` 后一次写入；JSON 还先构建完整 `QJsonObject/QJsonArray` 树。多通道对齐格式另建全量时间戳集合、排序列表和通道时间戳映射，峰值内存高于原始数据包。
- 当前写入使用 `QFile` 截断目标并一次写入；若中途失败，目标可能已被截断或留下部分文件。分页、流式写入与原子提交需要作为同一数据完整性边界权衡，但尚未决定授权范围。
- `MonitorWidget::buildExportDataPackage()` 对数据库来源按通道查询当前图表时间窗；若该窗无数据，再按采样周期估算数量并取最近 `max(50, estimate)` 条。分页改造必须保持这两个分支的范围和回退语义。
- 当前历史 SQL 仅 `ORDER BY timestamp`。游标分页若只用时间戳，在同毫秒多记录时会遗漏或重复；稳定边界至少需要 `(timestamp, id)` 复合顺序/游标。`OFFSET` 虽改动较小，但深页成本增长且并发插入时页边界可能漂移，不宜作为大历史导出的默认方案。
- `ExportDataPackage` 把所有通道样本保存在 `QMap<QString, QList<Sample>>`；当前公开导出 API均接受完整包。仅在 `MonitorExportHelper` 内分块写文件只能去掉完整文本缓冲，不能消除查询结果和数据包的全量驻留。
- `monitor_export_test.cpp` 已覆盖三种格式、单/多通道、质量/无效值、空数据、无效路径和自动格式选择，但数据量很小，且没有分页无重无漏、同时间戳稳定排序、分块写失败、已有目标保全或大数据内存边界测试。
- `src/core/tests/DataManagerTest.cpp` 有基础历史升序测试，但当前 `tests/CMakeLists.txt` 注册的是另一套 `tests/data_manager_test.cpp`，后者没有历史查询测试；新增分页测试应放入已注册且可执行的专项测试，或明确注册现有 QtTest 套件，不能把未注册源码当作回归证据。
- `runtime_data` 已有 `id INTEGER PRIMARY KEY AUTOINCREMENT` 和 `(variable_name, timestamp)` 索引。本批可用 `(timestamp, id)` 作为稳定排序/游标而不升级 schema；新增索引或 schema 版本会扩大迁移风险，应留在后续 schema 批次，并通过大数据运行验证判断现有索引性能是否足够。
- 候选实现文件目前均含既有未提交改动，尤其 `MonitorManager.cpp` 为 UTF-8 BOM 且存在 CRLF/LF 混合行尾，`MonitorWidget.cpp` 也有混合行尾。实施必须基于当前内容做局部补丁，禁止整文件重写、格式化或行尾归一化。
- 端到端内存收敛不能只改 `DataManager` 或只改 `MonitorExportHelper`：数据库导出路径还需调用方在选择目标路径后按页取数，并把页提供器交给流式导出；内存来源及现有完整包公开 API应保持兼容。
- 推荐由流式导出使用 `QSaveFile`，在全部页面、格式尾部和 `commit()` 成功后才替换目标。页面提供器在中途返回查询错误时应取消写入并保留旧目标，用确定性测试覆盖该失败路径。
- 只读分析结论：推荐端到端方案为“DataManager 稳定游标页 → MonitorManager 转换页 → MonitorWidget 固定时间窗并提供按页回调 → MonitorExportHelper 分格式流式写入”。只改其中一层不能充分解决数据库历史导出的峰值内存问题。
- 一致性取舍：继续导出前 flush 并固定 `end`，每页短暂持有数据库锁，避免长时间阻塞采集写入；这能保证既有记录按游标无重无漏，但不提供跨整次导出的事务快照，极少数带回填时间戳的并发新记录可能不在本次结果中。若要求严格快照，需要独立只读连接/事务与 SQLite 模式验证，超出本批最小范围。
- 多通道对齐流式实现必须保留当前“同一通道同一毫秒最后一条覆盖前一条”的行为，并跨页保留必要的单条前瞻/时间戳分组状态；不能重新建立全量时间戳集合或全量通道映射。
- 本批不改 schema/索引；若真实大库性能验证显示 `(variable_name, timestamp)` 索引不足，再在后续 schema 批次评估 `(variable_name, timestamp, id)` 索引和版本迁移。

### 第八批：诊断快照（静态验收通过）

- 项目配置、OPC 配置和 `opcExtras` 中的对象/数组现递归脱敏；敏感字段保留键和结构并替换为 `[REDACTED]`，`tokenCount` 等非敏感计数保持原值。
- 导出改用 `QSaveFile`，只有完整写入、刷新和提交成功后才返回输出路径；失败取消临时写入并传播错误。
- `DiagnosticSnapshotTest` 覆盖键名变体、嵌套数组、输入不变、JSON 可解析和无效目标目录失败，CTest 注册唯一。
- 总控确认仅修改三个授权文件；DSL 编译器和其他冻结目录未由本批触碰。Qt/CMake/CTest 不可用，仍需运行验证。

### 仍需运行验证的已实施项

- 下载 profile 多 chunk、地址边界和无 profile 行为；
- 项目保存/关闭、运行状态和脚本路径边界；
- 串口锁外信号、帧缓冲和重入；
- 监控数据库失败回灌、配置热更新和并发；
- QObject worker 连续启动/析构与应用产物发布；
- 安装布局、Qt 插件、Python 运行时发现；
- OPC 回调线程、取消订阅/停止竞态和真实服务互操作。

## P0/P1 证据摘要

- DSL 编译器控制流、赋值和表达式代码生成问题仍存在，但冻结，不在当前授权范围。
- 历史无 profile 假下载、保存/关闭数据丢失、状态不同步、通信迁移、串口重入、监控回灌、QObject 生命周期和安装链问题已完成静态整改。
- 已实施修复保持公共接口兼容；未运行部分不能以旧二进制或静态推断替代。

## 安全与数据边界

- 未发现硬编码 API Key、Token、私钥、明文密码或直接 SQL 注入。
- 项目名路径逃逸已由现有差异修复；项目内脚本和符号链接边界已收紧。
- 诊断快照脱敏已静态闭环，待目标测试运行验证。
- 设备读取失败质量、SQLite 历史质量表达、原子导出和大文件处理仍需运行/后续审查。

## 已纠正的历史发现

- `DataManager::logRuntimeDataBatch()` 当前已有事务回滚和提交后状态更新，旧的“部分成功”结论不再作为新任务依据。
- `DslScriptEditorSaveTest` 当前已注册，旧的“测试未注册”结论已过期。
- 项目名路径逃逸已闭环，不重复修改。

## 冻结与兼容边界

- 冻结：`src/compiler/**`、`third_party/custom_dsp_language/compile/**`。
- 不因本台账顺带重构公共接口、下载协议、`RingBuffer`、`MonitorTypes` 或无关 UI。
- 详细证据、批次实施记录和历史结论见 `docs/planning-history-2026-08-14.md`。

## 验收规则

- 静态路径、授权范围、失败传播、数据安全和测试语义由总控独立复核。
- 当前缺少运行环境的项目统一标记“需要运行验证”，不重复尝试。
