# LH 当前进度

## Current Snapshot

- **状态：** 第八批已通过总控独立静态验收；前七批静态结论继续保留。
- **当前环境：** 默认静态验收；Qt/CMake/CTest、Windows、硬件和真实 OPC 验证待外部环境。
- **当前范围：** 第九批历史查询/导出分页与流式处理只读分析；DSL 编译器继续冻结。
- **下一交接：** 运行 `DiagnosticSnapshotTest`；第九批待授权，派发前必须证明 `gpt-5.6-luna / max` 实际生效。

## Recent Batches

### 第七批：OPC 与监控线程模型

- `MonitorManager::shutdown()` 生命周期收口并保持幂等。
- backend 轮询按 point ID 去重、按周期到期筛选。
- Matrikon 回调采用 payload 深拷贝、QObject 队列和 generation 隔离迟到数据。
- 静态验收通过；真实 Windows COM/Matrikon 和目标测试待验证。

### 第六批：MonitorDataProcessor

- 配置、缓存、容量热更新和锁序完成收敛。
- 补充配置边界、迁移、重入、并发和信号顺序测试。
- 静态验收通过；Qt/CTest/TSAN 待验证。

### 第五批：Python DSL 运行时与安装部署

- 安装白名单、Python 运行时布局、Windows Qt 部署和 Debug/Release 插件分支完成。
- 安装布局测试和 README 已同步。
- 静态验收通过；安装、Qt 插件、Python 依赖和 Windows 测试待验证。

### 前四批：运行、下载、通信、监控和项目生命周期

- 完成运行状态、下载边界、通信迁移、串口缓冲、监控失败回灌、项目保存/关闭和脚本边界整改。
- 目标调用链和新增回归测试已静态复查。
- 编译和运行测试均待可用环境。

## Current Batch: 第八批

- 已确认诊断快照服务仍原样导出项目配置、OPC 配置和状态扩展。
- 授权范围限定为服务实现、专项测试和测试注册。
- 唯一 `luna_worker` 的创建请求曾显式填写 `gpt-5.6-luna / max`，但子智能体界面仍显示“中”，调度回执也未返回实际生效强度；因此上一批不得认定实际运行于 `max`。后续创建或继续 worker 前，必须由界面或有效回执明确证明模型为 `gpt-5.6-luna`、推理强度为 `max`，否则立即阻断且不得实施。
- 总控独立复查实际差异、敏感键边界、非敏感字段、输入不变、失败传播、测试注册和冻结范围，结论为“静态验收通过，编译与运行测试需要验证”。

## Verification

- 已完成：授权范围、冻结目录、关键调用链、冲突标记和静态测试注册检查。
- 已完成：第九批只读核对 `DataManager`、`MonitorManager`、`MonitorWidget`、`MonitorExportHelper` 及现有历史/导出测试；确认全量查询、二次转换、完整数据包、完整格式树/文本和一次写入构成端到端峰值内存链路。
- 已完成：形成第九批 keyset 分页、数据库来源流式写入、原子提交、兼容 API、精确文件边界和专项测试要求；未修改产品代码或测试，未创建/继续 worker。
- 未执行：Qt/CMake/CTest 编译运行、Windows Qt 部署、真实硬件/OPC、TSAN。
- 旧构建产物不作为当前批次证据。

## Errors

- 一次检查命令使用了错误工作目录，已改用项目根目录重试；未产生文件修改。
- 一次规划更新因文件已被压缩整理、预期上下文不匹配而未应用；读取当前内容后改用小范围补丁，未产生部分写入。

## Next Handoff

1. 在可用 Qt/CMake/CTest 环境运行 `DiagnosticSnapshotTest`。
2. 第九批实施前核验实际调度界面或有效回执；只有明确显示模型 `gpt-5.6-luna`、推理强度 `max` 才能让唯一 worker 开始。
3. 若仍显示“中”或无法证明实际生效为 `max`，立即报告“阻断，需要用户选择或外部环境”，不得实施。

详细历史记录见 `docs/planning-history-2026-08-14.md`。
