# LH 项目整改计划

## Goal

在保持功能兼容和硬件协议安全的前提下，按 P0 → P1 → P2 → P3 修复已确认问题；每批只做最小范围修改，并由总控独立静态验收。

## Current Snapshot

- **当前阶段：** Phase 5.4，历史查询/导出分页与流式处理只读分析完成（第九批待授权实施）。
- **当前模式：** 本地默认静态验收；Qt/CMake/CTest、Windows、硬件和真实 OPC 行为统一标记为“需要运行验证”。
- **下一步：** 在可用环境运行 `DiagnosticSnapshotTest`；第九批仅在调度界面或有效回执明确证明 `gpt-5.6-luna / max` 后派发，否则阻断且不得实施。
- **历史档案：** 详细批次记录见 `docs/planning-history-2026-08-14.md`，日常任务不读取。

## Status Convention

- **完成：** 实现和必要静态检查完成。
- **静态完成-待运行验证：** 静态路径已复查，运行环境不可用。
- **冻结/待授权：** 已识别但当前不处理。
- **待处理：** 尚未实施。
- **阻断：** 缺少外部环境或决策。

## Active Tasks

### P0/P1

- **P0-1 DSL 编译器语义：冻结/待授权。** 不修改 `src/compiler/**` 和 `third_party/custom_dsp_language/compile/**`。
- **P0-2 无 profile 下载：完成，待运行验证。**
- **P0-3 项目保存/关闭数据丢失：完成，待运行验证。**
- 运行状态、通信迁移、串口缓冲、监控失败回灌、QObject 生命周期、安装部署链、OPC/监控线程模型：静态验收完成，运行验证待外部环境。

### 当前第八批：诊断快照脱敏与原子导出

- **状态：** static_review_passed_runtime_verification_required。
- **目标：** 对配置、OPC 配置和状态扩展递归脱敏；采用原子提交并传播写入/提交失败。
- **授权文件：**
  - `src/diagnostics/DiagnosticSnapshotService.cpp`
  - `tests/diagnostic_snapshot_test.cpp`
  - `tests/CMakeLists.txt`
- **禁止修改：** DSL 编译器目录、`src/common/ConfigTypes.h`、`src/designer/MainWindow.cpp`、构建产物、规划文件及其他既有用户改动。
- **测试重点：** 混合键名、嵌套数组、输入不变、JSON 可解析、写入失败和提交失败。
- **验收：** 递归脱敏、非敏感字段保留、`QSaveFile` 原子提交和专项测试注册已由总控独立静态复核；待运行 `DiagnosticSnapshotTest`。

### 第九批候选：历史查询/导出分页与流式处理

- **状态：** 只读分析完成，待授权实施；尚未创建或继续 worker。
- **目标：** 为数据库历史查询增加稳定的 `(timestamp, id)` 游标分页；数据库导出按页读取并用 `QSaveFile` 流式写入，消除完整历史列表、完整导出包和完整文件缓冲同时驻留，同时保持内存来源和现有公开导出 API兼容。
- **关键取舍：** 采用 keyset/cursor 而非 `OFFSET`；保留旧 `queryHistory()` 和完整包导出接口，新增分页/流式接口；只让数据库来源走端到端分页，内存来源保持现状；不持有数据库锁覆盖整个导出，不承诺并发回填记录的事务级快照，但先 flush 并固定查询结束时间；CSV/TSV 字段与对齐语义保持，JSON 保持结构和值兼容但不把缩进空白作为契约。
- **拟授权文件：**
  - `src/core/DataManager.h`
  - `src/core/DataManager.cpp`
  - `src/monitor/MonitorManager.h`
  - `src/monitor/MonitorManager.cpp`
  - `src/monitor/MonitorExportHelper.h`
  - `src/monitor/MonitorExportHelper.cpp`
  - `src/monitor/MonitorWidget.cpp`
  - `tests/data_manager_history_paging_test.cpp`（新增）
  - `tests/monitor_export_test.cpp`
  - `tests/CMakeLists.txt`
- **禁止修改：** `src/compiler/**`、`third_party/custom_dsp_language/compile/**`；schema 版本、建表/迁移 SQL 和索引；配置 schema、通用参数校验、`MonitorTypes`、`RingBuffer`、诊断快照、无关 UI、构建产物、规划文件及其他用户改动；不得删除或改签名破坏现有完整查询/导出 API，不得整文件格式化或归一化行尾。
- **测试要求：** 分页同时间戳跨页无重无漏、升序和闭区间边界、空页/末页/未初始化/SQL 错误可区分；三格式多页导出、对齐/非对齐、重复时间戳、无效值/质量、计数和 JSON 可解析；页面提供器中途失败及提交失败保留旧目标；空数据和旧 API回归；大样本测试确认每次请求不超过页大小并完成多页，而不以脆弱的进程内存绝对值断言代替结构性验证。
- **运行验证：** 构建并运行 `DataManagerHistoryPagingTest`、`MonitorExportTest`；Qt/CMake/CTest 不可用时结论只能是“静态验收通过，编译与运行测试需要验证”。

### 后续 backlog

- 配置 schema、参数校验和运行点索引；
- 历史查询/导出分页与流式处理（第九批已分析、待授权）；
- 其他维护性重构必须建立回归保护后再授权。

## Runtime Verification Queue

- Qt/CMake/CTest 构建和目标测试；
- Python 依赖、干净安装和 DSL 运行时发现；
- Windows `windeployqt`、Qt 插件和 Debug/Release 安装；
- 真实串口、控制器、OPC DA/COM 回调和停止竞态；
- TSAN、性能和大规模监控数据验证。

## Stable Constraints

- 保持公共接口和既有项目格式兼容。
- 不清理、覆盖或回退用户已有改动。
- 保留原文件编码、BOM 和行尾，避免无关格式化。
- 当前环境默认静态验收；不可运行项必须明确标记。
- 详细历史不写回本文件，完成批次只保留状态摘要。

## Next Handoff

1. 在 Qt/CMake/CTest 环境运行 `DiagnosticSnapshotTest`。
2. 派发前确认界面或有效调度回执明确显示 `gpt-5.6-luna / max`；仍为“中”或无法证明时立即报告阻断。
3. 获授权后仅在第九批拟授权文件内实施并自检；worker 运行期间总控只等待，完成后总控独立验收并更新三份规划文件。
