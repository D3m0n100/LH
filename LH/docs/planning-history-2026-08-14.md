# 规划文件历史快照（2026-08-14）

本文件保存压缩前三个规划文件的完整内容，仅用于历史追溯；日常任务不读取本文件。

## task_plan.md（压缩前快照）

# LH 项目审查整改计划

## Goal

在保持现有功能兼容和硬件协议安全的前提下，按照 P0 → P1 → P2 → P3 顺序逐项修复已确认问题，并在每项修改后复查调用链、补充测试和验证回归。

## Next Step

用户已授权继续整改除 DSL 编译器语义外的未实施项。先只读核对 `DataManager` 批量写入结果、历史数据调用链和现有测试，确定第八批最小文件边界；其余安全、导出和维护性项目按风险串行推进。既有各批运行验证继续保留，DSL 编译器目录继续冻结。

## Current Phase

Phase 5.3：诊断快照脱敏与原子导出（第八批实施）

## Status Convention

- `完成`：实现和必要静态检查均已完成。
- `静态完成-待运行验证`：代码路径已复查，但当前环境无法完成编译、运行或平台验证。
- `冻结/待授权`：已识别但当前范围明确不处理。
- `待处理`：尚未实施。
- `阻断`：缺少外部环境或用户决策，无法继续。

## Phases

### Phase 1：完整项目审查

- [x] 盘点目录、源码、配置、资源、依赖和测试
- [x] 梳理入口、模块职责、调用关系和数据流
- [x] 审查架构、业务逻辑、异常、性能、安全和测试
- [x] 形成 P0 → P3 分级报告及功能覆盖矩阵
- **Status:** complete

### Phase 2：整改准备与逐项授权

- [x] 建立持久化计划、发现和进度文件
- [x] 确认本轮整改任务及精确修改范围
- [x] 建立文件级实施顺序和验收标准
- [x] 修改前再次核对相关调用链和现有测试
- **Status:** complete

### Phase 3：P0 安全与正确性修复

- [ ] P0-1：修复 DSL 编译器控制流、赋值和表达式语义（冻结/待授权）
- [x] P0-2：禁止无 profile 假下载并统一下载行为
- [x] P0-3：修复保存/关闭项目导致的 DSL 数据丢失
- **Status:** 部分完成；P0-1 冻结/待授权

### Phase 4：P1 核心运行链修复

- [x] 统一运行/下载状态机（本批次实施完成，待运行验证）
- [x] 修复默认 transport 占位导致的旧工程通信迁移错误
- [x] 修复串口接收缓冲和重入风险（静态验收通过，待可用 Qt 环境运行测试）
- [x] 修复监控数据库失败回灌突破缓冲硬上限
- [x] 收紧 QObject 生命周期和编译产物发布（本批实施完成，待运行验证）
- [x] 补齐安装部署链（本批实施完成，待安装与平台运行验证）
- **Status:** pending

### 四项整改残余问题闭环

- [x] 项目候选先验证后确认，保存/新建失败执行受限回滚与清理
- [x] 下载 profile 与执行入口统一校验 16 位值域和地址区间
- [x] 历史默认 transport 占位可按保守规则迁移，显式 transport 保持优先
- [x] 监控失败批次回灌后维持硬上限并报告丢弃数量
- [x] 总控完成静态调用链和新增测试审查
- [ ] 在具备 Qt/CMake 的环境运行定向测试
- **Status:** in_progress

### Phase 4.1：运行点索引与项目脚本边界

- [x] 重注册运行点时清理旧 name/kind 索引并重置值状态
- [x] 项目配置加载替换旧运行点集合，保留显式增量加载接口语义
- [x] 项目主脚本和附属脚本执行项目根目录、最终目标和 `.lh` 边界校验
- [x] 补充运行点和项目生命周期边界测试
- [ ] 在具备 Qt/CMake 的环境运行目标测试
- **Status:** implementation_complete_pending_verification

### Phase 4.2：TaskScheduler 完成时间与任务身份

- [x] FixedDelay 使用执行器真实完成时间，覆盖同 tick 排队和异常返回路径
- [x] FixedRate 使用真实完成时刻推进计划并在落后时重同步
- [x] stop/restart 与执行器重入的任务身份、禁用和统计回写保持一致
- [x] 补充优先级、周期、异常、停止/重启和注销/同名替换测试
- [ ] 在具备 Qt/CMake 的环境运行目标测试
- **Status:** implementation_complete_pending_verification

### Phase 4.3：QObject worker 生命周期与应用层产物原子发布

- [x] 参数和通信产物整组暂存、校验、发布、回滚和清理
- [x] DownloadManager worker 使用自动失效句柄并隔离旧 worker 延迟信号
- [x] 补充参数/通信失败保护和 DownloadManager 连续启动/析构测试并注册
- [ ] 在具备 Qt/CMake/CTest 的环境运行目标测试
- **Status:** implementation_complete_pending_verification

### Phase 4.4：Python DSL 运行时与 Qt 安装部署链

- [x] 以现有可执行文件相对发现路径安装 `lmc.py`、`requirements.txt`、ANTLR 生成文件和必要 `lh_compiler` 包
- [x] 通过白名单目录与 `.py` 过滤排除 venv、缓存、字节码、测试、示例、文档和输出内容
- [x] Windows 默认启用并校验 `windeployqt`、Qt 平台插件和 SQLite SQL 插件；非 Windows 不调用 windeployqt
- [x] 新增安装到构建目录临时前缀的布局回归测试并注册到 CTest，兼容单配置/多配置生成器
- [x] README 记录 Python 版本、requirements/本地虚拟环境配置、缺依赖诊断和平台 Qt 责任
- [ ] 在具备 Qt/CMake/CTest/Python 的环境运行安装布局、Windows 部署和定向回归测试
- **Status:** implementation_complete_pending_verification

### Phase 4.4 返修记录：安装布局测试安全边界与主程序断言

- [x] 安装布局测试在删除前规范化构建目录和测试前缀，拒绝空值、根目录、构建目录本身、目录外、符号链接和 `../` 逃逸路径
- [x] 从测试注册向脚本传入 `$<TARGET_FILE_NAME:${PROJECT_NAME}>`，断言安装前缀 `bin` 下真实目标文件存在且不是目录
- [x] 移除 README 末行本批新增的独立 CR，保留 UTF-8 BOM 和其余行尾
- [ ] 在具备 Qt/CMake/CTest/Windows 环境运行返修后的安装布局和 Qt 部署测试
- **Status:** implementation_complete_pending_verification

### Phase 4.4 第二轮返修记录：Windows Qt Debug/Release 安装插件

- [x] 安装脚本依据 `CMAKE_INSTALL_CONFIG_NAME` 归一化 Debug 与 Release 模式，空配置按 Release 处理并显式传递给 `windeployqt`
- [x] 安装脚本和布局测试按同一模式选择 `qwindows[d].dll` 与 `qsqlite[d].dll`，错误信息包含配置和期望路径
- [ ] 在具备 Windows/Qt/CMake/CTest 环境运行 Debug、Release 和空配置安装验证
- **Status:** implementation_complete_pending_verification

### Phase 5：P2/P3 稳定性与维护性改进

- [ ] 强化配置 schema、参数校验和运行点索引
- [ ] 修复调度、监控配置热更新和 OPC 线程模型
- [ ] 增加历史查询/导出分页与流式处理
- [ ] 在回归保护下拆分巨型类、清理占位代码和文档漂移
- **Status:** pending

### Phase 5.1：MonitorDataProcessor 配置与并发安全

- [x] 统一 `setConfig` 与全部 setter 的数值、容量和 clamp 规范化
- [x] 使 `m_config` 读写、处理快照、缓存失效和外部信号保持一致同步
- [x] 容量热更新保留各通道最新原始点，缩小 delta 保留最新数据，增量关闭/重开隔离旧数据
- [x] 补充配置边界、迁移、缓存、直连重入、并发超时与信号顺序测试
- [x] 完成授权范围静态检查并确认目标注册/失败返回；Qt/CMake/CTest/TSAN 缺失，运行验证待执行
- [x] 总控独立复核修改范围、冻结目录、配置/缓存锁序、信号顺序、热迁移语义和测试注册
- [ ] 在具备 Qt/CMake/CTest 的环境运行 `MonitorDataProcessorTest`，需要时补充 TSAN 运行
- **Status:** implementation_complete_pending_verification

### Phase 5.2：OPC 与监控线程模型

- [x] 为 `MonitorManager` 增加可显式调用且幂等的 `shutdown()`，按停止采样、断开观察连接、刷新并关闭日志器的顺序收敛生命周期
- [x] backend 轮询按 point ID 去重并按各点 `periodMs` 到期筛选，避免慢点被最短周期重复读取；停止后不再读取
- [x] Matrikon OPC 回调 payload 在 COM 回调线程深拷贝，排队到 server QObject 线程，并以回调代次隔离停止/取消订阅后的迟到数据
- [x] 补充 MonitorManager 生命周期、backend 销毁/切换、重复 point、周期和停止测试，以及 Matrikon 回调线程/停止隔离测试
- [x] 总控独立复查授权范围、冻结目录、轮询调用链、回调生命周期、非 Windows 预处理路径和测试语义
- [ ] 在具备 Qt/CMake/CTest 环境运行 `MonitorManagerBackendTest`、`OpcServerConfigTest`；在 Windows/COM 环境运行真实 OPC 回调验证
- **Status:** static_review_passed_runtime_verification_required

### Phase 5.3：诊断快照脱敏与原子导出

- [ ] 对项目配置、OPC 配置和状态扩展中的对象/数组递归脱敏，保留结构和非敏感诊断字段
- [ ] 采用原子文件提交并检查写入/提交错误，不留下被截断的成功快照
- [ ] 新增专项测试，覆盖混合键名、嵌套数组、输入不变、JSON 可解析和失败路径
- [ ] 总控独立验收修改范围、脱敏边界、原子写入和测试注册
- **Status:** in_progress

### Phase 6：全链路验证与交付

- [ ] 运行完整 CTest 和 Python 测试
- [ ] 在干净安装目录验证启动和 DSL 编译
- [ ] 标记并执行需要真实串口、控制器和 OPC 环境的验证
- [ ] 汇总兼容性变化、遗留风险和最终交付说明
- **Status:** pending

## Key Questions

1. 控制器实际支持哪些控制流和运行时表达式指令？无法静态确认的部分必须先采用“拒绝编译”，还是已有协议文档可实现完整代码生成？
2. F2812 下载协议应以 `ControllerBridge` 的 packet index/length/CRC/offset 语义为准，还是以 `ControllerDeviceBackend` 的固定 `dataAddress` 分块语义为准？需要硬件或协议文档确认。
3. 旧项目中的绝对脚本路径、旧下载 profile 和历史数据库需要兼容到什么版本？
4. Python 编译器部署应采用随安装包捆绑的运行时，还是要求系统预装 Python？

## Decisions Made

| Decision | Rationale |
|----------|-----------|
| 第一轮只做只读审查 | 用户明确要求先理解项目和形成报告，不直接大规模修改 |
| 整改严格按 P0 → P1 → P2 → P3 推进 | 先处理控制语义、假下载和数据丢失等安全/正确性问题 |
| 每个任务单独确认修改文件 | 遵守项目 AGENTS 规则，并限制变更范围 |
| 每项修改后复查调用链并运行相关测试 | 防止局部修复造成运行、下载、监控或配置回归 |
| 无法静态确认的硬件/COM 行为标记“需要运行验证” | 不对真实设备协议和线程行为作无依据假设 |
| 先最小止血，再渐进收口架构 | 一次性重写缺少硬件回归基线，风险过高 |
| 本轮由一个 luna-高子对话串行实施 | 避免多个任务同时改动共享配置、通信和测试文件 |
| 子对话只实施并返回验收指令 | 独立验收统一由总控完成 |
| backend 轮询保留单一唤醒定时器、按点到期筛选 | 以最小接口改动消除慢点过频读取和重复 point IO，同时保持现有私有 slot 入口兼容 |
| Matrikon 回调使用深拷贝、QObject 队列和代次隔离 | 避免 COM 线程直接访问 Qt 状态，并让 stop/unsubscribe 后的迟到回调安全丢弃 |
| 暂不修改 DSL 编译器 | 用户明确要求谨慎处理且当前不属于负责范围 |

## 当前已确认修改范围

- `src/common/ConfigTypes.h`
- `tests/communication_routing_test.cpp`
- `src/communication/DownloadProfile.cpp`
- `src/communication/ControllerBridge.cpp`
- `src/communication/ControllerDeviceBackendDownload.cpp`
- `tests/controller_device_backend_test.cpp`
- `src/monitor/MonitorDataLogger.h`
- `src/monitor/MonitorDataLogger.cpp`
- `tests/monitor_data_logger_test.cpp`
- `src/designer/ProjectController.cpp`
- `tests/project_save_close_test.cpp`

## 第七批授权与实际修改范围

- `src/monitor/MonitorManager.h`
- `src/monitor/MonitorManager.cpp`
- `src/communication/MatrikonOpcServer.h`
- `src/communication/MatrikonOpcServer.cpp`
- `tests/monitor_manager_backend_test.cpp`
- `tests/opc_server_config_test.cpp`

## 第八批授权范围

- `src/diagnostics/DiagnosticSnapshotService.cpp`
- `tests/diagnostic_snapshot_test.cpp`
- `tests/CMakeLists.txt`

禁止修改 DSL 编译器、`third_party/custom_dsp_language/compile/**`、`src/common/ConfigTypes.h`、`src/designer/MainWindow.cpp`、构建产物、规划文件和其他既有用户改动。

## Errors Encountered

| Error | Attempt | Resolution |
|-------|---------|------------|
| 暂无 | 1 | — |
| `ProjectController.cpp` 完整 `git diff --check` 仍报告历史行尾/空白噪声 | 1 | 与既有混合 CRLF/差异状态一致，未做全文件规范化；其余目标文件差异检查通过 |
| 一次检查命令使用了错误的工作目录 | 1 | 立即改用项目根目录重试，未产生文件修改 |

## Constraints

- 所有输出和项目文档使用中文。
- 修改前必须说明涉及文件、范围和验证要求。
- 保护 `.env`、API Key、Token 和其他敏感信息。
- 只修改当前任务直接相关文件，保持最小改动。
- 不碰备份文件、构建产物和历史残留，除非用户单独授权清理。
- 给方案时先说明权衡，再给建议。


## findings.md（压缩前快照）

# LH 项目审查发现与决策

## Requirements

- 对整个项目进行架构、模块、数据流、功能、代码、异常、性能、安全、维护性和测试的系统审查。
- 先完成理解和报告，再按优先级逐项修改。
- 区分确定问题、条件性风险和“需要运行验证”。
- 每个修改任务完成后复查相关调用链并运行相关测试。
- 保持现有功能兼容，避免为套用设计模式而重写。

## Project Overview

- 项目根目录：`/Users/demon1/Documents/11/LXui (6)/LH`
- 技术栈：C++17、Qt 5 Widgets、SQLite、Python 3、ANTLR4、Windows OPC DA/COM。
- 主要模块：`common`、`core`、`compiler`、`designer`、`communication`、`monitor`、`diagnostics`。
- 一方 C/C++ 约 4.35 万行，Python 编译器约 5500 行；包含测试和 DSL 后约 6.75 万行。
- 主流程：项目配置/DSL 编辑 → Python 编译 → `.code` 产物 → 控制器下载 → 设备监控/参数/OPC → SQLite/导出。

## Current Status Index

| 范围 | 状态 | 说明 |
|------|------|------|
| P0-1 DSL 编译器语义 | 冻结/待授权 | 本轮不修改编译器目录 |
| P0-2 无 profile 下载 | 完成 | 已静态复查，运行测试仍需环境验证 |
| P0-3 项目保存/关闭 | 完成 | 已静态复查，运行测试仍需环境验证 |
| P1/P2 已实施批次 | 静态完成-待运行验证 | 当前缺少 Qt/CMake/CTest/Windows 等环境 |
| 安装与 Qt 部署链 | 静态完成-待运行验证 | Debug/Release 和实际安装仍需平台验证 |
| 第七批 OPC 与监控线程模型 | 静态验收通过-待运行验证 | Qt/CMake/CTest 不可用；Windows COM 回调仍需真实环境验证 |
| 非 DSL 未实施整改 | 已授权-分批处理中 | 优先核对数据完整性，再串行处理安全、导出和维护性；DSL 继续冻结 |

后续带日期的实施与返修章节保留为审查证据和变更历史；本表作为当前状态索引。

## 2026-08-14 非 DSL 遗留整改只读分批

- 用户已授权继续整改除 DSL 编译器语义外的未实施项；后续按数据完整性、安全、导出和维护性风险串行分批。
- 当前 `DataManager::logRuntimeDataBatch()` 已在事务中逐条插入，任一执行失败立即回滚并返回失败；提交成功后才更新缓存和发送信号，提交失败也回滚。原“部分插入失败仍可返回整体成功”发现与现状不符，不能据此重复修改实现。
- 当前已注册的 `tests/data_manager_test.cpp` 没有批量失败回滚、缓存和信号断言；`src/core/tests/DataManagerTest.cpp` 虽有基础批量成功测试，但入口被注释且未作为当前目标编译。下一步需核对现有差异和最小可测试边界后再决定第八批。
- 事务回滚与 quality/valueValid 持久化位于当前 `DataManager` 既有未提交差异中，应保留并补验收，不作为新批重复实现；`tests/CMakeLists.txt` 当前已注册 `DslScriptEditorSaveTest`，原“创建但未注册”发现也已过期。
- 安全清单中“项目名路径逃逸”仍有现状依据：`ProjectController` 新建入口接收项目名后直接形成项目目录。该问题优先于维护性重构，下一步只读确认名称规则、根目录边界和生命周期测试入口。
- 继续核对后确认项目名逃逸已由当前 `ProjectController` 既有差异修复：名称会 trim，并拒绝空值、`.`、`..`、正反路径分隔符和同名覆盖；原安全发现改为已闭环，不再重复修改。
- `DiagnosticSnapshotService::exportSnapshot()` 仍直接导出完整 `ProjectRuntimeConfig::toJson()`、OPC 配置和 `opcExtras`，没有敏感键脱敏，也没有专项测试。该问题现状明确、接口可保持兼容，选为第八批候选。

## Critical Findings

### P0：确定存在

1. `third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py`
   - `_process_control_flow()` 无条件遍历并输出所有分支和循环体，丢失 IF/ELSE/循环语义。
   - 标识符表达式固定求值为 0；成员赋值和多种动态 RHS 被静默跳过。
   - 生成的 `.code` 可能与 DSL 源码控制逻辑不等价。

2. `src/communication/ControllerDeviceBackendDownload.cpp`
   - 无下载 profile 时只复位控制器并读取状态，没有打开或传输产物，却返回成功。
   - `tests/controller_device_backend_test.cpp` 把该错误行为固化成了测试预期。

3. `src/designer/MainWindow.cpp`、`src/designer/ProjectController.cpp`
   - DSL 文件写入失败后仍可能清除 modified 状态。
   - 关闭项目选择“保存”时，`ProjectController::saveProject()` 只保存配置，不保存编辑器中的 DSL。
   - 保存失败后仍可能关闭项目，存在确定的数据丢失路径。

### P1：核心高风险

- 主下载实现忽略示例 profile 中的 `chunkWords`、packet index/length/CRC/offset，并反复写相同 `dataAddress`。
- `RuntimeSessionController` 和 `MainWindow` 的运行状态不同步；下载失败后 UI 仍可显示“运行中”。
- 类型化 Modbus TCP、Ethernet UDP 配置可能被通信工厂创建成错误协议实例。
- 串口帧缓冲发出后不消费，且持锁发信号，存在重复帧和重入死锁风险。
- `MonitorDataLogger` 禁用时无法完成承诺的 flush，数据库写失败后整批样本永久丢失。
- 设备读取失败会记录数值 0；质量只在 metadata 中，SQLite 历史数据无法区分真实零值和通信故障。
- `MonitorManager` 持有裸 backend/processor 指针，销毁时可能留下悬空指针。
- 编译、项目配置、诊断和导出多处直接截断写入，缺少原子提交。
- CMake 安装规则没有部署 Python DSL 编译器和运行依赖，安装版本可能无法编译 DSL。

### P2/P3：稳定性和维护性

- `RuntimePointTable` 覆盖同一 ID 时不清理旧 name/kind 索引。
- `DataManager::logRuntimeDataBatch()` 部分插入失败仍可返回整体成功。
- `TaskScheduler` 在对象线程同步执行任务；FixedDelay 完成时间计算对排队任务不准确。
- 监控所有后端点按最短 provider 周期一起轮询，产生重复 IO。
- `MonitorDataProcessor` 的配置边界和跨线程访问不完整。
- `MainWindow.cpp`、`MatrikonOpcServer.cpp`、`MonitorManager.cpp` 等职责过多。
- 两套下载实现、多个运行/监控状态源和未接入的 `MonitorController` 造成重复抽象。
- README 声明的实时错误检查、真正图形化组态、HMI 显示等功能尚未完整实现。

## Security Findings

- 未发现硬编码 API Key、Token、私钥或明文密码。
- SQL 主要使用参数绑定，未发现直接 SQL 注入。
- QProcess 使用程序与参数列表，未发现直接 shell 命令注入。
- 项目名未限制 `/`、`\`、`..`，可造成路径逃逸。
- `mainScriptPath/scriptFiles` 可引用项目外文件，恶意项目配置可读取外部 `.lh`。
- 诊断快照会原样导出完整配置和 metadata，缺少敏感字段脱敏。
- `.code` 元数据头泄露本机绝对源码、暂存和输出路径。
- 第三方依赖 CVE 状态尚未联网核对，属于“需要联网验证”。

## Testing Findings

- `tests/CMakeLists.txt` 创建 20 个测试可执行目标，但只注册 19 个；`dsl_script_editor_save_test` 未注册。
- `src/core/tests` 中较完整的 DataManager/TaskScheduler 测试没有参与构建。
- Python pytest 只有功能块注册表测试，没有编译语义测试。
- 历史日志显示 2026-06-11 的 19 个 CTest 项曾通过；本轮未重跑测试，因为确认前禁止写入，且历史通过不能证明业务要求正确。
- 缺少控制流编译、多 chunk 下载、保存失败、通信协议工厂、串口帧、数据库失败重试、OPC 线程和干净安装测试。

## Needs Runtime Verification

- 控制器实际 `.code` 指令集以及对控制流/动态表达式的支持。
- F2812 下载 packet 字段、数据地址递增和控制器侧完整性校验语义。
- Matrikon OPC COM callback 所在线程和真实订阅/写回行为。
- CAN/CANOpen/J1939 与实际 Qt CAN 插件和设备的互操作。
- 安装目录下数据库权限、Qt 插件和 Python 运行时发现。
- 大规模监控数据下的内存、图表帧率和导出性能。

## 2026-08-13 四项整改独立验收

### 已静态确认修复

- 项目切换在候选配置和主脚本加载成功前不再提交新项目状态。
- 下载流程已拒绝无 `sendChunk` 的 profile 和空产物，并按层选择目标站号。
- 控制器后端错误信号已移到互斥锁外；串口采用独立接收/帧缓冲并在锁外发帧。
- 监控处理器先提交质量状态再发送增量通知，无效样本可独立通知质量。

### 仍需闭环

- `ProjectRuntimeConfig::fromJson()` 仅在 `transport` 缺失时迁移旧通信字段；历史默认占位 transport 会阻止 UDP/CAN/TCP 等迁移。
- 下载校验未统一检查 `dataAddress + registerCount <= 65536`，靠近地址上界的多寄存器写仍可能越界。
- `MonitorDataLogger::flushInThread()` 失败回灌后未重新裁剪，多轮并发失败可能突破缓冲硬上限。
- 项目配置保存失败时脚本回滚结果未检查；新建项目失败可能留下半成品目录。
- 项目生命周期测试缺少相对主脚本、无效候选、同名新建和失败回滚场景。

### 验证限制

- 当前环境没有 CMake、CTest、qmake 或 Qt 开发环境；编译与测试执行标记为“需要运行验证”。
- 现有 Windows 构建产物早于本轮源码修改，不可作为本轮验收证据。

## 2026-08-13 四项残余问题闭环结果

- 通信迁移仅把“协议/模式为默认值、参数为空且没有扩展键”的 transport 当作历史占位；带参数或扩展键的显式 transport 不被旧字段覆盖。
- 下载 profile 对标量、列表和字符串列表统一执行有限整数 `0..65535` 校验，并统一检查写入区间；Bridge 和控制器后端在实际转换/写入前保留防御性检查。
- 监控失败批次与新缓冲合并后重新裁剪至硬上限，保留最新记录，并在锁外发送准确的丢弃数量。
- 项目打开在确认当前未保存修改前完整验证候选配置和主脚本；非对象 JSON 被拒绝。保存和新建失败会检查回滚/清理结果，不再误报已清理。
- 总控两轮静态验收发现并推动补齐：监控回灌分支测试、Bridge 标量写契约、项目清理结果、标量寄存器值域以及非对象项目配置。
- 静态检查通过：目标文件无冲突标记；新增/修改目标未出现禁止标识；JSON 文件可解析；本轮未触碰 DSL 编译器。
- 编译和 CTest 仍需在具备 Qt/CMake 的环境运行验证。

## 2026-08-13 运行会话状态收敛实施发现

- `executeRun()` 原先先发布 `Running`，再调用自动下载；自动下载失败时会短暂或最终保留运行态。
- 下载流程需要区分自动下载的连接基线与手动下载的运行基线：自动下载从 `Connected` 开始，成功后才进入 `Running`；手动下载失败且后端仍在线时恢复 `Running`。
- 后端连接信号应由 `RuntimeSessionController` 接管；断开时停止监控和 OPC，并进入 `Fault`，停止流程则先发布 `Idle` 再断开后端，避免停止期间被断开信号反向覆盖。
- `MainWindow` 原先在运行、停止和编译后运行路径直接写入状态文本并直接启停监控控件；本批次改为由会话状态、下载状态和监控信号派生运行状态展示，演示模式保留为独立兼容路径。
- 停止可在下载状态信号重入触发；增加取消标记并在下载调用前后检查，防止停止后下载流程恢复 `Running`。

## 2026-08-13 运行会话状态收敛返修发现

- `MainWindow::refreshRuntimeStatus()` 会为 `Connected`、`Downloading` 和 `Fault` 启用停止动作，但 `onStopProject()` 仍以 `isRunning()` 拦截，导致这些状态无法回到 `Idle`。
- 停止入口应以“会话状态和下载状态均为 `Idle`”作为完全空闲条件；其他会话状态统一委托 `RuntimeSessionController::requestStop()`，演示模式则保留兼容停止路径。
- `MainWindow.cpp` 当前混有 CRLF 与 LF；返修仅规范化为 LF，并保留 UTF-8 BOM，中文正文未做转换。
- 本机仍缺少 Qt/CMake/CTest，返修后的编译与集成测试需要总控在可用环境运行。

## 2026-08-13 第二批整改实施前复查

- `RuntimePointTable::registerPoint()` 覆盖同一 id 时只写入新 name/kind 索引，未清理旧索引；`loadFromProjectConfig()` 直接调用增量 `loadDefinitions()`，连续项目配置加载会残留旧点和值。
- `ProjectController::openProjectFromPath()` 当前仅将配置路径绝对化并过滤非 `.lh` 附属项，没有执行项目根目录边界、最终目标和符号链接校验，越界 `scriptFiles` 会被静默丢弃。
- 候选主脚本和附属脚本的边界校验应在 `confirmPendingChanges()` 前完成；失败路径必须保留当前项目、编辑器内容和 dirty 状态。
- `ProjectController.cpp` 当前存在既有行尾/差异噪声，本批只对目标函数及必要 include 做局部修改，不做全文件行尾规范化。
- 主脚本校验采用 canonical 最终目标；不存在的附属 `.lh` 文件保留项目内规范化路径，兼容现有打开后由编译链处理缺失附属脚本的行为，不在打开阶段读取附属文件。
- 项目根路径使用 canonical 目录，候选路径用目录相对路径判断 `..` 逃逸，避免前缀相似目录误判；所有路径拒绝均发生在保存确认之前。

## 2026-08-13 第二批返修复查

- 单值 name 索引在 `p1/Shared`、`p2/Shared` 后覆盖 `p2` 为新名称时，旧 `Shared` 映射被删除但未从仍保留的定义中回退恢复；回退候选必须排除正在覆盖的 id，且测试不依赖哈希容器顺序。
- 不存在的附属脚本若位于项目内符号链接目录下，当前 cleanPath 词法边界会把外部目标误判为项目内；需要先做词法边界，再沿候选路径向上找到最近存在父目录并校验 canonical 父目录，断裂链接或 canonical 失败直接拒绝。

## 2026-08-13 第三批整改实施前复查

- `TaskScheduler::onTick()` 只用 tick 起始时间收集到期任务，`executeTask()` 以起始时间加自身耗时写入 `lastExecutionTime`，同一 tick 中后续任务的排队等待未进入 FixedDelay 基线。
- FixedRate 当前以起始时间判断落后；任务执行完成后可能仍保留已经过期的计划点，应改用真实完成时间推进和重同步。
- 执行完成回写按任务名重新查找；执行器可重入注销自己或替换同名任务时，旧执行结果可能写入新任务统计。采用仅内部可见的任务代次并在回写时核对名称对应的代次，失配则丢弃旧回写。
- 执行器不持有任务锁；停止、注销、替换和禁用均可重入。`onTick()` 在串行执行每个到期任务前后检查运行标志，避免同一 tick 在 stop 后继续启动新任务。

## 2026-08-13 第三批整改实施结果

- `lastExecutionTime` 记录执行器成功返回或异常捕获后的全局 elapsed；FixedRate 仍按计划点推进，仅在真实完成时刻已越过下一计划点时从完成时刻重新同步。
- 任务代次只作为 `Task` 内部字段和调度器内部计数器使用，不改变公开注册/查询接口；完成回写代次失配时不更新统计、不恢复状态、不发送旧任务完成信号。
- stop 在同一 tick 的任务串行循环中阻止尚未开始的后续任务；执行器内手动禁用不会被完成回写改回 Ready。
- 定时测试使用嵌套 `QEventLoop` 驱动 Qt 定时器，执行器中的短暂 sleep 仅用于模拟慢任务和异常返回；实际实时性仍需具备 Qt/CMake/CTest 的环境验证。

## Technical Decisions

| Decision | Rationale |
|----------|-----------|
| P0-1 优先建议采用“无法正确生成就拒绝编译” | 在未知目标指令能力前，这是比静默生成错误控制逻辑更安全的最小止血方案 |
| 下载 profile 校验和真实执行必须共享同一强类型计划 | 防止 dry-run 通过但执行器忽略字段 |
| 项目与产物统一采用原子写入 | 避免磁盘满、异常退出或附属产物失败破坏有效文件 |
| 运行、下载和监控状态应有唯一事实来源 | 消除 UI、session、widget、manager 各自保存状态导致的不一致 |
| 监控历史记录必须持久化 quality/error/origin | 避免把设备故障误解释为正常零值 |

## Issues Encountered

| Issue | Resolution |
|-------|------------|
| 首轮审查阶段项目规则禁止创建规划文件 | 使用会话内计划完成审查；用户确认后再建立本文件 |
| 项目根目录不是可识别的 Git 工作区 | 不依赖 Git 历史判断正式/残留文件，所有清理工作等待单独授权 |

## Resources

- `README.md`
- `CMakeLists.txt`
- `src/main.cpp`
- `src/common/ConfigTypes.h`
- `src/common/RuntimePointTypes.h`
- `src/compiler/DSLCompilerInterface.*`
- `src/communication/ControllerDeviceBackendDownload.cpp`
- `src/designer/ProjectController.cpp`
- `src/designer/RuntimeSessionController.cpp`
- `src/monitor/MonitorManager.cpp`
- `third_party/custom_dsp_language/compile/src/lh_compiler/`
- `tests/CMakeLists.txt`

## Visual/Browser Findings

- 本轮未使用图像、PDF 视觉检查或网页浏览。

## 2026-08-13 第三批返修复查

- `onTick()` 原到期快照只保存任务名；高优先级任务在同一批次注销并重建低优先级同名任务后，旧快照可能立即执行新代次。
- 到期项现保存任务名和内部代次，执行器开始前在锁内核对代次；注销、禁用和同名重建均不会被旧到期项误启动，执行后既有代次核对继续保留。
- 新增同 tick 同名替换，以及注销后重建两组时间下限和统计隔离测试；Qt/CMake/CTest 当前不可用，仍需运行验证。

## 2026-08-13 第三批动态注册基线返修

- 运行中注册或同名替换的 FixedDelay 任务现以注册时刻初始化 `lastExecutionTime`，避免调度器已运行较久时新任务在下一 tick 立即执行。
- 未运行时仍保留零基线，由 `start()` 重置；FixedRate 仍由 `onTick()` 首次设置计划时间，不复用 FixedDelay 基线。
- 动态注册测试要求完整周期后首次执行；同名替换和注销重建测试将首次执行下限收紧至 150ms 周期的合理容差。

## 2026-08-14 第四批整改实施发现与结果

- `BuildController::generateParameterArtifacts()` 和 `generateCommunicationArtifacts()` 原先直接按顺序写正式路径；中途失败会留下部分新文件并覆盖旧文件。
- 两类应用层产物现统一先写入输出目录内的唯一暂存文件，逐个确认暂存文件和目标路径可用后，再通过备份旧文件、改名发布完成整组提交；发布失败按逆序恢复旧文件，回滚或清理失败会在错误中列出具体残留路径。
- 产物发布成功后再次按最终路径校验 checksum，`CompileResult::artifact.path` 保持正式最终路径；只处理本批产物集合，不删除输出目录中的无关文件。
- `DownloadManager::m_activeWorker` 原为裸 `QObject*`，现改为 `QPointer<QObject>`；worker 完成和线程退出的清理均按 worker 对象身份判断，旧 worker 的延迟信号不能清空新 worker 句柄。
- `BuildController` 发布回滚测试在显式 `LH_ENABLE_TEST_FAILURE_INJECTION=ON` 的测试构建中，整组暂存成功后删除第二个暂存文件，让首个正式文件已发布、后续 `rename` 真实失败；分别验证有旧文件恢复和无旧文件移除新文件。
- `DownloadManager` 生命周期测试使用无效 profile 路径连续启动两次，直接断言每次失败后线程停止、`QPointer` 为空；析构测试通过 `QThread::started/finished` 信号确认销毁完成，不依赖真实设备。
- `tests/CMakeLists.txt` 已注册新增生命周期测试；用户指定的 task scheduler 孤立右括号和 serial interface 重复链接起始行在本批现状中已是正确形式，未重复修改既有修复。
- 已知限制：`cancel()` 仍通过 worker 线程队列请求中止；若底层同步通信调用自身阻塞，队列无法在阻塞期间执行，不能声称本批解决硬件级安全中断。
- 当前环境无 CMake、CTest、qmake 或 Qt 开发环境，编译与运行测试标记为“需要运行验证”。

## 2026-08-14 第五批安装部署链实施发现与结果

- `DSLCompilerInterface` 已从可执行文件所在目录向上查找 `third_party/custom_dsp_language/compile/lmc.py`；安装规则沿用该布局，不修改编译器查找代码。
- 安装白名单包含 `lmc.py`、`requirements.txt`、4 个 ANTLR 生成 Python 文件、`src/__init__.py`、`lh_compiler/compiler.py`，以及 `frontend`、`backend`、`function_blocks`（含功能块定义）下的 Python 包文件。
- 安装规则没有复制仓库虚拟环境，也没有纳入 grammar 源文件、缓存、字节码、测试、示例、文档、脚本或 compiled output/output 目录；CMake 安装阶段不联网安装 Python 依赖。
- Windows 安装默认要求可定位的 `windeployqt`；安装阶段失败或缺少 `platforms/qwindows.dll`、`sqldrivers/qsqlite.dll` 会返回失败。显式关闭选项只适用于由外部流程提供 Qt 的特殊打包环境；非 Windows 不调用该工具。
- 新增安装布局测试会将安装写入构建目录临时前缀，检查必要文件和禁止内容，并在 Windows 启用 Qt 部署时检查关键插件；测试已在 `tests/CMakeLists.txt` 注册。
- 当前环境没有 CMake、CTest、qmake、Qt 开发环境，也未进行 Windows/Python 安装验证；相关项目标记为“需要运行验证”，未把既有构建产物当作证据。

## 2026-08-14 第五批安装布局测试返修

- 原安装布局脚本在删除测试前缀前没有验证调用者传入路径；现先将构建目录解析为既存目录的规范绝对路径，将前缀解析为绝对路径并要求其父目录规范路径就是构建目录、末级名称就是专用 `install_layout_test_prefix`，删除前拒绝空值、根目录、构建目录本身、目录外、符号链接和 `../` 逃逸路径。
- 安装布局测试现接收目标生成表达式产生的真实可执行文件名，并在 Python 运行时检查前先断言 `prefix/bin/<真实文件名>` 存在且不是目录，覆盖 `install(TARGETS ...)` 失效或目标路径错误的正常安装失败场景。
- README 仅移除本批末行独立 CR；未做全文件行尾归一化，保留 UTF-8 BOM。
- CMake、CTest、Qt 和 Windows 环境仍不可用，返修后的安装布局、目标文件名展开和 Qt 插件验证标记为“需要运行验证”。

## 2026-08-14 第五批第二轮返修：Windows Qt 配置插件

- 原 `windeployqt` 调用未传递安装配置，插件断言固定使用发布版文件名；现依据安装时 `CMAKE_INSTALL_CONFIG_NAME`，大小写不敏感识别 Debug，Debug 使用 `--debug` 和 `qwindowsd.dll/qsqlited.dll`，其他及空配置使用 `--release` 和无 `d` 后缀文件。
- `tests/install_layout_test.cmake` 对传入的 `LH_INSTALL_CONFIG` 执行相同的空白剥离、大小写归一化和模式/插件名选择，并将归一后的配置传递给 `cmake --install`；缺插件错误包含配置和期望文件路径。
- 未修改 CMake 目标注册、C++、DSL 冻结目录或其他范围外文件；Windows、Qt、CMake、CTest 当前不可用，Debug/Release/空配置运行验证仍需执行。

## 2026-08-14 第六批 MonitorDataProcessor 实施前复查

- `setConfig` 只对 `maxDeltaBufferSize >= ringBufferCapacity` 做局部修正，setter 未统一边界；负容量可转为极大无符号容量，零/负处理窗口和计数阈值的语义不一致。
- `RingBuffer::setCapacity()` 会清空数据，当前处理器热更新容量会丢失各通道全部原始点；处理器层需重建环形缓冲并按原顺序迁移最新 `min(旧数量, 新容量)` 个点，不修改 `RingBuffer`。
- `m_config` 除局部容量更新外大量无锁读写；处理管线多次直接读配置，热更新时既存在数据竞争，也无法保证单次处理使用同一配置。
- `setConfig`/`setRingBufferCapacity` 持有 `m_bufferLock` 发送 `configChanged`，直连槽回读配置或再次 setter 可死锁；所有外部信号必须在锁外发送。
- 配置变化应只失效 `m_cachedData`，不应借用现有 `clearAllCache()`，因为后者还会清空原始环形缓冲和 delta。缓存写回需按配置代次防止旧快照结果在热更新清理后再写入。
- 增量模式关闭需立即清除旧 delta；容量/最大 delta 缩小时一律丢最旧保最新。现有 `appendSample(s)` 质量信号在 delta 信号之前发送的顺序必须保留。
- 极大正配置值不设未经授权的上限；其可导致大额内存申请或运算成本，作为遗留资源风险记录。

## 2026-08-14 第六批 MonitorDataProcessor 实施结果

- 所有配置更新经过同一规范化/提交路径：负时间窗口和负显示上限归一为保留的“0=不限制”，容量与计数阈值按各自最小值归一，`maxDeltaBufferSize` 始终不小于环形容量。
- clamp 的 NaN/正负无穷端点被移除；两个有限端点反向时确定性交换，正常有序输入保持不变。规范化后无实际变化时不清缓存、不重复发送 `configChanged`。
- `m_bufferLock` 统一保护配置和通道缓冲；公开 getter 返回锁内快照，处理管线在单次调用内只使用同一快照。配置代次防止旧快照的处理结果在热更新清理后重新写入缓存。
- 锁顺序统一为 `m_bufferLock -> m_cacheMutex`，未新增递归锁；配置变化在同一配置写临界区内只清理 `m_cachedData`，原始缓冲和通道元数据保留。
- 环形容量变化会先为所有通道构建替换缓冲，再统一提交；每个通道按原顺序保留最新 `min(旧数量, 新容量)` 个点。delta 缩小丢最旧保最新，关闭增量模式立即清旧 delta，重开后只收集新数据。
- `configChanged`、`channelQualityUpdated`和 `deltaDataReady` 均在锁外发送；既有质量状态变更得到保留，样本追加仍先通知质量、再通知 delta。
- 公开方法签名、合法配置结果与正常信号语义保持兼容；未修改 `RingBuffer`、`MonitorTypes`、Manager/Widget/ChartView 调用方或测试注册。
- 未对极大正值设置任意上限；在活跃通道上设置极大环形容量仍可导致大额内存申请失败，这一资源风险需由上层配置策略或后续单独授权决定。

## 2026-08-14 第六批总控静态验收

- 授权范围内仅三个处理器相关源码/测试文件和三份规划记录在本批时间窗内发生变化；冻结编译器目录、`RingBuffer`、`MonitorTypes`、Manager/Widget 调用方及测试注册文件未被本批触碰。
- 独立复核确认配置读写统一受 `m_bufferLock` 保护，处理链使用单次配置快照，缓存代次阻止旧配置结果回写；锁顺序保持 `m_bufferLock -> m_cacheMutex`，外部信号均在解锁后发送。
- 环形容量扩缩保留各通道最新原始点，delta 缩小丢旧保新，关闭/重开增量模式隔离旧数据；配置变化只失效处理缓存，未清除原始数据。
- 目标差异检查、冲突标记、禁用标识、编码/BOM、测试目标与 CTest 注册静态检查通过；源文件中的既有尾随空白未被本批扩大。
- 当前环境缺少 CMake、CTest、qmake 和 Qt 开发环境，无法提供编译、运行或 TSAN 证据；结论为“静态验收通过，编译与运行测试需要验证”。

## 2026-08-14 第七批 OPC 与监控线程模型实施与总控验收

### 实施发现与结果

- `MonitorManager` 是 QObject 单例，拥有 backend/provider/cleanup 定时器和 `MonitorDataLogger`；生命周期现通过公开、幂等的 `shutdown()` 收口，停止采样定时器后断开 backend/processor 观察连接，再关闭日志器。析构和 `aboutToQuit` 均复用该路径，重复调用不会重复读取或重复发布停止状态。
- backend 轮询仍使用单一最短周期定时器作为唤醒源，但每次只选择已经到期的唯一 point ID；同一 point 被多个 provider 使用时只进入一次 `readPoints`，周期取最快需求，通道映射保留最后一个有效 provider。慢点不再随最短周期重复 IO。
- Matrikon `IOPCDataCallback` 不再直接修改 server QObject 状态。回调线程复制 `VARIANT`、质量、时间戳和错误到独立 payload，借助 QObject queued invocation 在 owner 线程执行；停止/取消订阅递增回调代次，迟到 queued payload 被丢弃。非 Windows 下 OPC 回调类受条件编译保护。
- 新增/加强 `MonitorManagerBackendTest` 的 backend 切换、销毁观察、重复 point、不同周期、重复停止和显式 shutdown 覆盖；`OpcServerConfigTest` 增加回调跨线程排队、owner 线程应用和 stop 后隔离测试，保留原有 Matrikon 配置、候选项、读探针、质量和写入语义测试。

### 总控独立静态验收

- 实际修改范围符合本批六个授权文件；未新增触碰 `src/compiler/**`、`third_party/custom_dsp_language/compile/**`、`tests/CMakeLists.txt` 或公共 `IOpcServer` 接口。冻结目录中的既有用户改动未清理、覆盖或回退。
- 独立确认 `shutdown()` 位于 public 接口且测试直接调用两次；确认轮询 due 集合、去重、停止短路和 backend 销毁清理路径；确认 OPC callback context 的深拷贝、代次检查、取消订阅顺序及非 Windows 预处理边界。
- 目标测试注册沿用现有 `MonitorManagerBackendTest` 与 `OpcServerConfigTest`，未扩大构建配置范围。`MonitorManager.cpp` 保留既有混合 CRLF/LF 差异噪声，未做全文件格式化。
- 总控结论：静态验收通过，编译与运行测试需要验证。当前 `cmake`、`ctest`、`qmake` 和 Qt5Core pkg-config 均不可用；构建目录中的旧测试二进制不作为本批证据。Windows COM 实际回调线程、Unadvise 竞态和真实 Matrikon 服务仍需平台验证。


## progress.md（压缩前快照）

# LH 项目整改进度日志

## Current Snapshot

- **状态：** 用户已授权继续整改除 DSL 编译器语义外的未实施项；当前进入非 DSL 遗留问题的只读分批阶段。既有七批静态结论和外部运行验证要求继续保留。
- **当前范围：** DSL 编译器目录继续冻结；新整改按风险逐批确定精确文件边界，不扩大已完成批次范围。
- **下一步：** 只读核对 `DataManager` 批量写入、历史数据调用链和现有测试，形成第八批权衡、授权文件、禁止范围与测试要求后交给唯一 `luna_worker`。
- **状态记录规则：** 本节只记录当前结论；以下 Session 章节保留实施历史。

## Session: 2026-08-14（非 DSL 遗留整改授权与分批）

- **Status:** read_only_triage_in_progress
- 已读取项目协作规则和三份规划记录，确认 DSL 编译器继续冻结，其余未实施项已获授权并按风险串行推进。
- 首项核对发现 `DataManager::logRuntimeDataBatch()` 当前已有事务回滚、提交后缓存更新和锁外信号语义，旧的部分成功问题不再成立；现有注册测试尚未覆盖批量失败原子性。
- 下一步核对目标文件既有差异和可测试失败路径，不在证据不足时重复修改实现。
- 已确认 `DataManager` 事务修复属于现有质量持久化差异，且 `DslScriptEditorSaveTest` 当前已注册；两项旧发现改为现状纠正，不重复实施。
- 转入只读核对新建项目名路径逃逸，准备第八批最小安全边界。
- 项目名路径逃逸已由当前 `ProjectController` 差异闭环；不重复修改。
- 诊断快照仍原样导出项目配置、OPC 配置和状态扩展，且无专项测试；转入第八批脱敏边界分析。
- 第八批确定为诊断快照递归脱敏与原子导出；授权仅限服务实现、新专项测试和测试注册，公共接口与配置序列化保持不变。

## Session: 2026-08-13

### Phase 1：完整项目审查

- **Status:** complete
- Actions taken:
  - 只读扫描源码、配置、资源、示例、Python 编译器、构建规则和测试。
  - 梳理启动、项目、编译、下载、运行、监控、参数和 OPC 调用链。
  - 输出架构概览、功能覆盖矩阵、P0 → P3 问题和整改任务清单。
  - 区分静态确定问题、条件性风险和“需要运行验证”。
- Files created/modified:
  - 无；审查阶段遵守确认前禁止写入的项目规则。

### Phase 2：整改准备与逐项授权

- **Status:** complete
- **Started:** 2026-08-13
- Actions taken:
  - 用户明确启用 `planning-with-files` 技能。
  - 修改前说明将创建三个规划文件，并取得用户确认。
  - 运行 session catchup；没有发现需要同步的上一会话输出。
  - 读取技能模板并依据现有审查结论建立持久化计划。
- Files created/modified:
  - `task_plan.md`（created）
  - `findings.md`（created）
  - `progress.md`（created）

## Test Results

| Test | Input | Expected | Actual | Status |
|------|-------|----------|--------|--------|
| session catchup | 项目根目录 | 检查上一会话未同步上下文 | 无遗留输出 | ✓ |
| 业务测试 | — | 本阶段不改业务源码，不运行写入型测试 | 未运行 | N/A |

## Session: 2026-08-13（运行会话状态收敛实施）

### Implementation

- **Status:** implementation_complete_pending_verification
- 按已确认范围修改运行会话控制器、下载流程、主窗口和两组测试。
- 自动运行改为下载成功后才进入 `Running`；自动失败恢复 `Connected` 或 `Fault`。
- 手动下载保留运行态恢复规则；后端断开进入 `Fault`；停止先进入 `Idle` 再断开后端。
- 监控启动仅允许从 `Running` 进入 `Monitoring`，主窗口运行/下载/监控展示由控制器信号派生。
- 新增自动下载失败、监控非法入口、下载中停止、手动失败恢复、后端断开和主窗口状态/动作派生测试。

### Files modified

- `src/designer/RuntimeSessionController.h`
- `src/designer/RuntimeSessionController.cpp`
- `src/designer/RuntimeSessionDownload.cpp`
- `src/designer/MainWindow.cpp`
- `tests/runtime_session_controller_test.cpp`
- `tests/main_window_integration_test.cpp`
- `task_plan.md`
- `findings.md`
- `progress.md`

### Verification status

- 已完成目标文件静态调用链复查，待执行可用环境下的编译、目标测试和完整静态检查。
- 当前工作区已有大量无关源码与构建产物改动，未清理、覆盖或回退。
- 静态检查：目标 C++ 文件括号/大括号平衡；目标测试已在 `tests/CMakeLists.txt` 注册；未发现本批次新增的禁用历史标识；补充复查了停止下载的重入顺序。
- 工具检查：本机缺少 `cmake`、`ctest`、`qmake`、Qt `pkg-config` 和 MinGW 编译器，编译与 CTest 标记为“需要运行验证”。

## Session: 2026-08-13（运行会话状态收敛返修）

### Implementation

- **Status:** implementation_complete_pending_verification
- 修正 `MainWindow::onStopProject()`：会话只要不是状态与下载态同时为 `Idle`，均委托 `RuntimeSessionController::requestStop()`；保留演示模式停止兼容逻辑。
- 新增主窗口集成测试，覆盖 `Connected` 和 `Fault` 状态点击停止后回到 `Idle`。
- 将 `src/designer/MainWindow.cpp` 的混合 CRLF/LF 行尾规范化为 LF；验证 UTF-8 BOM 仍存在，中文内容未被转换。
- 根据既有 `findings.md` 与 `progress.md` 事实，将 P0-2、P0-3 标记完成；P0-1 继续冻结且未触碰 DSL 编译器。

### Verification

- 静态检查待返修后执行：行尾指标、括号平衡、停止入口调用链、目标测试注册和变更范围。
- 当前环境缺少 Qt/CMake/CTest，返修后的编译与测试仍需运行验证。
- 返修静态检查完成：`MainWindow.cpp` 与主窗口集成测试无 CRLF/裸 CR，UTF-8 BOM 保留，UTF-8 解码正常且无替换字符；`git diff --check` 无输出；目标 C++ 文件括号/大括号平衡。
- 停止入口调用链确认：`Connected`、`Fault` 和下载态均进入 `requestStop()`；新增测试覆盖 `Connected`、`Fault` 点击停止回到 `Idle`。
- 工具限制再次确认：本机仍缺少 Qt、CMake、CTest、qmake 和 MinGW，未伪造编译或测试通过。

## Error Log

| Timestamp | Error | Attempt | Resolution |
|-----------|-------|---------|------------|
| 2026-08-13 | 无错误 | 1 | — |
| 2026-08-14 | 一次检查命令使用了错误的工作目录 | 1 | 立即改用项目根目录重试，未产生文件修改 |

## 5-Question Reboot Check

| Question | Answer |
|----------|--------|
| Where am I? | Phase 2：整改准备与逐项授权 |
| Where am I going? | 按 P0 → P1 → P2 → P3 逐项修复，并在每项后验证回归 |
| What's the goal? | 在保持兼容和硬件安全的前提下完成项目整改 |
| What have I learned? | 见 `findings.md` |
| What have I done? | 完成全项目审查并建立三个持久化规划文件 |

## Next Handoff

- 尚未修改任何业务源码。
- 推荐下一任务：P0-1 DSL 编译语义修复。
- 开始前必须先列出拟修改文件、权衡、实现边界和测试范围。

## Session: 2026-08-13（四项任务独立验收与下一轮授权）

- **Status:** implementation_authorized
- 总控完成项目生命周期、下载、通信/串口和监控四项任务的独立静态验收。
- 验收结论为“主体通过、仍有残余问题”，未发现新的 P0。
- 用户已确认 11 个业务/测试文件以及 3 个规划文件的写入范围。
- 下一步由一个 luna-高子对话串行完成已确认业务修改；子对话不负责验收，只返回验收指令。
- 总控将在子对话完成后复查相关调用链、差异和可执行测试。
- DSL 编译器目录继续冻结，不在本轮范围内。

### 实施完成

- 一个 luna-高子对话在授权范围内完成修改，未承担最终验收。
- 实际业务/测试修改覆盖通信迁移、下载边界、监控回灌及项目生命周期。
- 总控进行了两轮差异与调用链复核，并要求补齐三个首轮遗漏和两个第二轮数值/配置漏洞。
- 最终静态验收通过；未运行编译和 CTest，原因是当前环境缺少 CMake、CTest、qmake 与 Qt 开发环境。

### 待运行验证

```text
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure -R "CommunicationRoutingTest|ControllerDeviceBackendTest|MonitorDataLoggerTest|ProjectSaveCloseTest"
```

## Session: 2026-08-13（最终规则扫描清理）

- 清理两个指定源文件末行的行尾空白。
- 生成物测试改为分段构造检查字符串，保留“不应包含禁用标识”的断言意图，源码不再出现完整标识。
- 对本批 9 个文件执行不区分大小写、独立词边界扫描，结果无匹配。
- `git diff --check` 无输出；本次未改业务逻辑，未触碰 DSL 编译器。

## Session: 2026-08-13（第二批运行点与项目脚本边界实施）

- **Status:** implementation_complete_pending_verification
- `RuntimePointTable` 重注册同一 id 前按索引当前指向关系清理旧 name/kind，重建新索引，并将值重置为新默认值、Unknown、init、序列零值；空 kind 集合随之移除。
- `loadFromProjectConfig()` 改为替换旧点集，`loadDefinitions()` 仍保持逐项增量注册。
- `ProjectController::openProjectFromPath()` 建立 canonical 项目根目录，对主脚本和附属脚本执行项目内边界、最终目标和 `.lh` 校验；项目内缺失附属脚本保留规范化路径，交由现有后续编译链处理。
- 新增运行点重注册/索引/值状态/连续配置测试，以及项目脚本相对/绝对路径、空路径回退、越界、非 `.lh`、前缀相似目录、符号链接和失败状态保留测试。
- 静态检查：运行点头文件和两份测试括号平衡；非 `ProjectController.cpp` 目标文件 `git diff --check` 无输出；7 文件规则扫描无禁用标识；目标测试已在 CMake 注册表中。
- `ProjectController.cpp` 保留 UTF-8 BOM 与既有混合行尾，未做全文件规范化；其完整 `git diff --check` 仍报告既有行尾/空白噪声，本批未新增清理范围外改动。
- `cmake`、`ctest`、`qmake` 当前不可用，编译与目标测试标记为“需要运行验证”；未触碰 DSL 编译器及本批范围外文件。

### Next Handoff

- 总控应重点验收运行点覆盖索引清理、同名兼容、值状态重置、配置替换，以及项目路径校验发生在确认之前和失败状态保留。
- 建议在可用 Qt/CMake 环境运行 `RuntimePointTableTest` 与 `ProjectSaveCloseTest`；确认 `ProjectController.cpp` 的既有行尾噪声不被误判为本批全文件格式化。

## Session: 2026-08-13（第二批返修）

- **Status:** implementation_complete_pending_verification
- 修复单值 name 索引回退：覆盖旧映射后从仍使用旧 name 的候选中按 id 确定性选择，不依赖 QHash 顺序；旧映射若已指向其他 id 则保持不变。
- 修复不存在脚本的符号链接目录绕过：先做 `../`、`../` 及 `..\\` 词法边界判断，再校验候选/路径父目录的符号链接与 canonical 目标；普通项目内缺失附属 `.lh` 仍保留兼容行为。
- 新增 p2 重注册改名后旧 name 回退测试，以及项目内目录符号链接指向外部时缺失附属脚本在确认前拒绝测试；保留外部符号链接主脚本测试。
- 静态检查完成：运行点与项目测试括号平衡、非 `ProjectController.cpp` 目标差异检查无输出、6 文件禁用标识扫描无匹配、目标测试注册保持存在；`ProjectController.cpp` 保留 BOM 与既有 675 个 CRLF 行，完整差异检查仍受历史行尾噪声影响。
- 当前无 Qt/CMake/CTest，编译与目标测试仍需运行验证；本次未触碰 DSL 编译器和允许范围外文件。

## Session: 2026-08-13（第三批 TaskScheduler 实施）

- **Status:** implementation_complete_pending_verification
- `FixedDelay` 在成功和异常路径均以执行器返回后的 `m_globalTimer.elapsed()` 作为 `lastExecutionTime`；同 tick 后续低优先级任务的排队时间自然进入完成基线。
- `FixedRate` 保持 `nextScheduledTime` 计划驱动，推进和落后重同步均以真实完成时刻判断，避免完成后仍指向过期计划点。
- 为任务增加仅内部使用的代次字段；执行完成按名称和代次双重核对，注销或同名替换后旧结果不写入新任务统计。执行器仍在任务锁外运行，停止后同 tick 不再启动后续任务，执行期间禁用状态得到保留。
- `tests/task_scheduler_test.cpp` 沿用现有 `QCoreApplication` 自定义测试方式，补充优先级排队、慢 FixedDelay、FixedRate、异常、停止/重启、自注销、同名替换和执行期间禁用测试；已有测试失败数继续作为进程退出码。
- 静态检查完成：冲突标记扫描无结果，6 文件禁用标识扫描无匹配，`git diff --check` 无输出；C++ 大括号/括号平衡，TaskScheduler 正式测试目标和注册均存在，`main.cpp` 启动与退出 shutdown 调用链已复查。
- `TaskScheduler.h` 与测试中存在既有空白行尾，但未由本批新增；未做全文件格式化或行尾规范化。冻结目录扫描未发现本批源码新增触碰。
- `cmake`、`ctest`、`qmake` 和 Qt 开发环境不可用，编译、CTest 和定时行为验证标记为“需要运行验证”；`clang++`/`g++` 虽可用但缺少 Qt 头和库，未伪造语法或运行通过。

### Next Handoff

- 总控重点验收真实完成时间、FixedRate 落后重同步、同 tick stop 顺序、任务代次失配回写，以及异常/禁用统计和信号语义。
- 建议命令：`cmake -S . -B build && cmake --build build --parallel && ctest --test-dir build --output-on-failure -R TaskSchedulerTest`。

## Session: 2026-08-13（第三批 TaskScheduler 到期快照返修）

- **Status:** implementation_complete_pending_verification
- `onTick()` 到期快照改为保存任务名和内部代次；`executeTask()` 在执行器启动前核对期望代次，保留执行完成后的代次核对，避免注销或同名重建任务被旧到期项立即执行或污染统计。
- 新增同 tick 同名替换、注销后重建两组确定性时间下限测试，并校验旧执行器未调用及新任务统计与实际执行次数一致；既有自注销、自替换和禁用重入测试保留。
- `AGENTS.md` 与 `task_plan.md` 未修改；本次仅修改调度器、目标测试及本记录文件。
- 静态检查和变更范围复查已完成：`git diff --check` 无输出，冲突标记和禁用标识扫描无匹配，测试注册及主程序启动/退出调用链保持存在；本机缺少 Qt/CMake/CTest，编译、CTest 和定时行为仍需运行验证。

## Session: 2026-08-13（第三批动态注册基线返修）

- **Status:** implementation_complete_pending_verification
- 运行中注册 FixedDelay 任务时，在任务锁内以有效全局计时器的当前 elapsed 初始化首次执行基线；未运行时保持零基线，FixedRate 首次计划逻辑不变。
- 新增运行中动态注册测试，并将同名替换、注销重建的首次执行时间下限收紧到接近完整 150ms 周期；保留旧执行器、代次统计隔离和真实失败退出码。
- 本次仅修改调度器实现、目标测试及本记录文件；未修改 `AGENTS.md`、`task_plan.md`、头文件或冻结目录。
- 静态检查：`git diff --check` 无输出，冲突标记和禁用标识扫描无匹配，目标测试注册和主程序生命周期调用链保持存在；Qt/CMake/CTest 仍不可用，编译与运行测试需要总控验证。

## Session: 2026-08-14（第四批 QObject 生命周期与产物原子发布）

### Implementation

- **Status:** implementation_complete_pending_verification
- `BuildController` 参数和通信产物改为整组暂存、暂存校验、目标校验、备份发布和失败回滚；发布后校验最终文件 checksum，清理失败不报告成功并列出残留路径。
- 保留输出目录中不属于本批产物集合的文件；公开编译接口、产物命名、文件内容和成功/失败信号保持兼容，未进入配置编译或 DSL 编译链。
- `DownloadManager::m_activeWorker` 改为 `QPointer<QObject>`；worker 完成、线程退出和析构路径均不再使用可能悬空的裸指针，延迟旧信号按对象身份过滤。
- 新增仅测试编译宏保护的发布失败注入：首个正式文件发布后删除后续暂存文件，主窗口集成测试覆盖参数有旧文件回滚、参数/通信无旧文件移除新文件、`compileFailed`、失败结果和残留清理。
- DownloadManager 生命周期测试直接断言两次失败后的线程停止与 `QPointer` 为空；析构测试通过线程 started/finished 信号证明销毁结束。
- 新增生命周期测试目标并注册到 `tests/CMakeLists.txt`；增加默认关闭的 `LH_ENABLE_TEST_FAILURE_INJECTION` 测试选项。该文件中用户指定的两处 CMake 语法阻断在现状中已正确，无额外格式化或重写。

### Files modified

- `src/designer/BuildController.cpp`
- `src/communication/DownloadManager.h`
- `src/communication/DownloadManager.cpp`
- `tests/main_window_integration_test.cpp`
- `tests/download_manager_lifecycle_test.cpp`
- `tests/CMakeLists.txt`
- `task_plan.md`
- `findings.md`
- `progress.md`

### Verification

- 已复查 `BuildController` 的参数/通信生成调用链、失败/回滚/清理路径，以及 `DownloadDockWidget → DownloadManager` 的生命周期连接。
- 已检查目标文件括号平衡、冲突标记、BOM、CRLF/裸 CR、授权范围和敏感文件状态；目标文件 `git diff --check` 无输出。
- 已确认未触碰 `src/compiler/**`、`third_party/custom_dsp_language/compile/**`；这些冻结目录的既有用户改动未清理、覆盖或回退。
- `cmake`、`ctest`、`qmake`、Qt 开发环境不可用；编译、CTest、Qt 事件循环和平台失败注入均需运行验证。
- 已知风险：底层同步硬件通信调用若阻塞，队列化取消请求仍不能安全中断该调用；本批未声称解决该限制。

### Suggested verification

```text
cmake -S . -B build -DLH_ENABLE_TEST_FAILURE_INJECTION=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure -R "MainWindowIntegrationTest|DownloadManagerLifecycleTest"
```

## Session: 2026-08-14（第五批 Python DSL 运行时与安装部署链）

### Implementation

- **Status:** implementation_complete_pending_verification
- 复查 `DSLCompilerInterface` 的现有运行时发现逻辑，确认安装布局以 `prefix/third_party/custom_dsp_language/compile` 为基准即可被已安装程序发现，未修改冻结的 DSL 编译器目录。
- 在根 CMake 安装规则中加入严格白名单：`lmc.py`、`requirements.txt`、ANTLR 生成 Python 文件，以及 `lh_compiler` 编译前端、后端和功能块定义包；过滤缓存、字节码、测试和开发输出。
- Windows 默认启用安装阶段 Qt 部署，定位失败、执行失败或关键平台/SQLite SQL 插件缺失均返回安装失败；非 Windows 不调用 `windeployqt`，支持显式关闭以交由外部打包流程提供 Qt。
- 新增安装布局 CMake 测试脚本，使用构建目录临时前缀执行 `cmake --install`，检查必要/禁止内容和 Windows 插件，并在 `tests/CMakeLists.txt` 注册，兼容单配置和多配置生成器。
- README 已补充 Python 3.8+、requirements、本地虚拟环境、缺依赖诊断、安装内容边界和平台 Qt 运行库责任。

### Files modified

- `CMakeLists.txt`
- `README.md`
- `tests/CMakeLists.txt`
- `tests/install_layout_test.cmake`
- `cmake/DeployQt.cmake.in`
- `task_plan.md`
- `findings.md`
- `progress.md`

### Verification status

- 已静态复查安装目标、运行时 Python 导入链、`DSLCompilerInterface` 查找路径、安装白名单、Qt 部署失败传播和 CTest 注册。
- 已确认本批未修改 `src/compiler/**`、`third_party/custom_dsp_language/compile/**`、其他业务源码、构建脚本和构建产物；冻结目录仅作读取。
- CMake、CTest、Qt 开发环境、Windows `windeployqt` 和 Python 运行时依赖未在当前环境执行，安装布局、Windows 插件、编译与测试均标记为“需要运行验证”。

### Suggested verification

```text
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --config Release --parallel
ctest --test-dir build --output-on-failure -R "InstallLayoutTest|MainWindowIntegrationTest|DownloadManagerLifecycleTest"
cmake --install build --prefix /tmp/lh-install --config Release
```

## Session: 2026-08-14（第五批安装布局测试返修）

### Implementation

- **Status:** implementation_complete_pending_verification
- 仅修改 `tests/install_layout_test.cmake`、`tests/CMakeLists.txt`、`README.md` 及三份规划记录；未扩大第五批授权范围。
- 安装布局脚本现在在任何删除前校验规范化构建目录和测试前缀：前缀必须是构建目录的直接子目录 `install_layout_test_prefix`，拒绝空值、根目录、构建目录本身、目录外、符号链接、非目录和 `../` 逃逸路径。
- `InstallLayoutTest` 通过 `$<TARGET_FILE_NAME:${PROJECT_NAME}>` 传递实际目标文件名，脚本在运行 Python 运行时检查前验证 `prefix/bin/<真实文件名>` 存在且不是目录，兼容 Windows 后缀及单配置/多配置生成器。
- README 仅移除末行新增 CR，未修改 BOM 或其他行尾。

### Verification status

- `git diff --check` 对本批已跟踪目标文件无输出；新脚本无冲突标记和尾随空白。
- CMake 脚本与测试注册括号计数平衡；已静态确认失败分支和删除发生顺序，README 保留 UTF-8 BOM 且当前无 CRLF。
- `src/compiler/**` 与 `third_party/custom_dsp_language/compile/**` 仅显示返修前已有用户改动，本批未写入；未执行文件时间或内容改动。
- 当前无 CMake、CTest、Qt 或 Windows 环境，返修后的编译、安装布局、目标文件名展开及 Qt 插件测试均为“需要运行验证”，未伪造结果。

## Session: 2026-08-14（第五批第二轮 Windows Qt 配置返修）

### Implementation

- **Status:** implementation_complete_pending_verification
- 仅修改 `cmake/DeployQt.cmake.in`、`tests/install_layout_test.cmake` 及三份规划记录。
- 安装脚本依据 `CMAKE_INSTALL_CONFIG_NAME` 进行大小写不敏感归一化：Debug 传 `--debug` 并校验 `qwindowsd.dll/qsqlited.dll`；其他配置和空配置传 `--release` 并校验 `qwindows.dll/qsqlite.dll`。
- 安装布局测试复用 `LH_INSTALL_CONFIG` 的同一归一化规则，并将归一后的配置传给 `cmake --install`，Windows 插件错误包含配置和期望文件路径。

### Verification status

- 已静态检查两处脚本的配置分支、插件路径、空配置默认行为和错误传播；括号结构、冲突标记、尾随空白、编码/行尾及授权范围检查均已完成。
- 未修改 `src/compiler/**`、`third_party/custom_dsp_language/compile/**`、C++ 或测试注册文件；冻结目录仅保留既有用户改动。
- 当前缺少 Windows、Qt、CMake 和 CTest，Debug/Release/空配置安装与插件验证均标记为“需要运行验证”。

## Session: 2026-08-14（第五批总控静态验收）

- **Status:** static_review_passed_runtime_verification_required
- 总控独立复核安装白名单、运行时发现路径、Windows Qt 部署失败传播、Debug/Release/空配置分支、安装测试删除边界、主程序断言及 CTest 注册，未发现新的静态阻断项。
- 本批修改范围符合授权；`src/compiler/**` 与 `third_party/custom_dsp_language/compile/**` 的文件修改时间早于第五批，未由本批触碰。
- 本批目标文件 `git diff --check`、冲突标记和禁用标识扫描无结果；README 末行新增 CR 已清除。
- 当前环境仅有系统 Python，缺少 CMake、CTest、Qt、qmake、Windows `windeployqt`；编译、安装、Python 依赖、Windows 插件和真实启动仍需运行验证。

## Session: 2026-08-14（第六批 MonitorDataProcessor 实施）

### Start

- **Status:** in_progress
- 协作接口不支持请求的 `gpt-5.6-luna`；依 `AGENTS.md` 的环境不支持例外，本批由 `gpt-5.6-sol / max` 执行，并在交接中保留该降级原因。
- 已完整读取 `AGENTS.md`、`task_plan.md`、`findings.md`、`progress.md`，并只读复查 `MonitorDataProcessor`、`RingBuffer`、`MonitorTypes`、目标测试、Manager/Widget/ChartView 调用链与 `tests/CMakeLists.txt` 注册。
- 已确认目标三个 C++ 文件存在第一批质量状态的既有未提交改动；本批在其上做局部追加，不清理、覆盖或回退。
- 授权写入仅限 `src/monitor/MonitorDataProcessor.h`、`src/monitor/MonitorDataProcessor.cpp`、`tests/monitor_data_processor_test.cpp` 与三份规划记录；其他调用方、测试注册、编译器和构建文件保持冻结。

### Implementation

- **Status:** implementation_complete_pending_verification
- `setConfig` 和全部现有 setter 收口到同一配置规范化/提交函数；实现无限制零值、各计数最小值、delta/环形容量约束、非有限 clamp 移除和反向 clamp 交换。
- 公开配置 getter、append、通道创建、配置提交和处理管线的 `m_config` 访问均收入 `m_bufferLock` 或单次处理快照；新增配置代次隔离旧处理缓存写回。
- 容量变化为每通道预构建替换环形缓冲并保留最新数据；delta 缩小与增量开关实现丢旧保新/断代。配置变化只失效处理缓存，不清原始数据。
- 配置信号和既有质量/delta 信号均位于锁外；无实际配置变化不重复通知，质量先于 delta 的顺序保持。
- 目标测试新增/加强：合法 setter/整体配置、负/零/极大边界、NaN/Inf/反向 clamp、多通道容量扩缩、delta 缩小、增量关闭重开、缓存失效与原始数据保留、`configChanged` 直连回读/再次 setter、5 秒并发超时与容量断言、锁外质量/delta 顺序。

### Files modified

- `src/monitor/MonitorDataProcessor.h`
- `src/monitor/MonitorDataProcessor.cpp`
- `tests/monitor_data_processor_test.cpp`
- `task_plan.md`
- `findings.md`
- `progress.md`

### Verification

- 静态自检通过：目标差异 `git diff --check` 无输出；无冲突标记、新增 TODO/FIXME/历史禁用名称、本批新增尾随空白、BOM/编码/行尾异常或括号失衡。目标源文件的既有尾随空白保留，未做全文件格式化或行尾归一化。
- 已逐项复查所有 `m_config` 访问、`m_bufferLock -> m_cacheMutex` 锁序、信号解锁位置、环形/delta 迁移顺序、缓存代次和 Manager/Widget/ChartView 调用链。
- `tests/CMakeLists.txt` 中 `MonitorDataProcessorTest` 目标与 CTest 注册保持存在且本批未修改；测试继续使用 `QTEST_MAIN`，断言失败会返回非零。
- 本机无 `cmake`、`ctest`、`qmake` 和 Qt5 开发包，无法编译或运行目标测试/TSAN，标记为“需要运行验证”。`build_current_mingw/bin/monitor_data_processor_test.exe` 是早于本批的现有产物，未作为验证证据。
- 本批工具写入仅落在上述 6 个授权文件；未修改 `RingBuffer`、`MonitorTypes`、Manager/Widget/ChartView、`tests/CMakeLists.txt`、编译器、冻结目录、构建产物或其他已有工作区改动。

### Suggested verification

```text
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --parallel --target monitor_data_processor_test
ctest --test-dir build --output-on-failure -R '^MonitorDataProcessorTest$'
```

TSAN（编译器与 Qt 构建支持时）：使用 `-fsanitize=thread -fno-omit-frame-pointer` 重新配置/构建后，运行同一 CTest 目标。

## Session: 2026-08-14（第六批总控静态验收）

- **Status:** static_review_passed_runtime_verification_required
- 总控独立复核本批修改范围、冻结目录时间、配置规范化、热迁移、缓存代次、锁顺序、锁外信号及 Manager/Widget 调用链，未发现新的静态阻断项。
- 目标差异 `git diff --check` 无输出；冲突标记和禁用标识扫描无结果；文件保持 UTF-8 且未新增 BOM，测试目标和 CTest 注册保持存在。
- 当前环境缺少 CMake、CTest、qmake 和 Qt 开发环境，无法编译或运行目标测试/TSAN；第五批安装部署的动态验证也仍待外部环境完成。

## Session: 2026-08-14（第七批 OPC 与监控线程模型）

### Start

- **Status:** in_progress
- 已按项目要求读取 `AGENTS.md`、`task_plan.md`、`findings.md`、`progress.md`，只读复查 `MonitorManager`、`MatrikonOpcServer`、`IOpcServer`、`RuntimeSessionController`、现有 backend/OPC 测试及测试注册。
- 权衡确定为：MonitorManager 保留单一最短周期唤醒定时器，仅按每个 point 的到期时间筛选读取集合；Matrikon 回调采用 payload 深拷贝、QObject 队列和代次隔离，保持公共接口兼容。
- 授权范围严格限定为 `MonitorManager.h/.cpp`、`MatrikonOpcServer.h/.cpp`、`monitor_manager_backend_test.cpp`、`opc_server_config_test.cpp`；DSL 编译器目录、构建产物、测试注册、公共 OPC 接口和规划文件在 worker 实施期间冻结。
- 唯一实施子智能体为 `luna_worker`，调度角色配置标注 `gpt-5.6-luna / max`；界面曾显示推理档位“中”，因此本记录不把该界面显示与平台角色契约混为实际运行证据。

### Implementation

- `MonitorManager` 增加公开、幂等的 `shutdown()`；析构和 `aboutToQuit` 复用该路径，先停 provider/backend/cleanup 定时器，再断开 backend/processor 观察连接并关闭日志器。
- backend 轮询新增每点 period/due 状态，point ID 去重；同一点多个 provider 取最快周期并保留最后通道映射，慢点不再按最短周期重复 IO，停止后轮询直接短路。
- Matrikon OPC 回调不再直接操作 server QObject 状态；回调线程复制值、质量、时间戳和错误，queued invocation 在 owner 线程应用，取消订阅/停止递增 generation 以丢弃迟到 payload；非 Windows 回调类型受条件编译保护。
- 测试补充 backend 销毁/切换、重复 point、不同周期、重复停止和显式 shutdown；补充 Matrikon 回调跨线程排队、owner 线程应用和 stop 后隔离，保留原配置/候选项/读探针/质量/写入测试。
- 首轮总控静态复核发现 `shutdown()` 错误地位于 private，已复用原 `luna_worker` 限定返修并完成公开接口与直接重复调用测试。

### Verification

- 总控独立确认六个授权文件范围、冻结目录未由本批触碰、`shutdown()` public 位置、轮询去重/到期/停止逻辑、callback context 深拷贝/代次/取消订阅路径及非 Windows 预处理边界。
- 目标测试沿用现有 `MonitorManagerBackendTest` 和 `OpcServerConfigTest` 注册；未修改 `tests/CMakeLists.txt` 或公共接口。
- `cmake`、`ctest`、`qmake` 和 Qt5Core pkg-config 当前不可用；构建目录中的旧 `monitor_manager_backend_test.exe`、`opc_server_config_test.exe` 未作为本批证据。未运行编译、CTest、Windows COM 或真实 Matrikon 服务验证。
- `MonitorManager.cpp` 保留其既有混合 CRLF/LF 差异噪声，未做全文件格式化；本批没有冲突标记或冻结目录写入。

### Status

- **结论：** 静态验收通过，编译与运行测试需要验证。
- **建议验证：**

```text
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --parallel --target monitor_manager_backend_test opc_server_config_test
ctest --test-dir build --output-on-failure -R 'MonitorManagerBackendTest|OpcServerConfigTest'
```

- Windows/COM 额外验证：真实 Matrikon OPC DA 订阅、回调线程、`Unadvise`/`stop()` 竞态、服务退出及重启后的代次隔离。


