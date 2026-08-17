# 项目整改交接

更新时间：2026-08-14

## 1. 我们在做什么

当前任务是对 LH 项目进行分批代码整改，目标分为五组：

1. OPC 与监控线程模型
   - 收紧 `MonitorManager` 后端/处理器生命周期和裸指针风险。
   - 明确 OPC COM 对象线程归属，保证回调线程安全并隔离停止后的迟到回调。
   - 按 provider 周期分组轮询，避免全部点位按最短周期重复 IO。
   - 在 Windows + Matrikon OPC 环境做真实互操作验证。
2. 运行数据完整性与配置校验
   - 明确 `DataManager::logRuntimeDataBatch()` 部分失败时的事务语义。
   - 通信读取失败不得再伪装成正常数值 `0`。
   - 历史数据持久化 `quality/error/origin`。
   - 加强配置 schema、字段类型、参数边界和可定位的错误提示。
3. 历史查询、导出和安全写入
   - 历史查询使用稳定分页和流式处理。
   - 大批量导出避免一次性构造全部内容。
   - 配置、manifest、诊断快照等采用原子写入。
   - 诊断快照敏感字段脱敏。
   - 清理 `.code` 元数据中的本机绝对路径。
4. 测试体系补全
   - 注册遗漏测试，将 `src/core/tests` 纳入构建。
   - 增加 Python DSL 编译语义测试，但不修改 DSL 编译器实现。
   - 补齐 OPC、数据质量、分页导出和原子写入回归测试。
5. P3 可维护性收口
   - 继续拆分 `MainWindow`、`MonitorManager` 的重职责。
   - 收拢两套下载入口的重复逻辑。
   - 清理未接入控制器、占位代码和失效路径。
   - 修正文档与实际功能不一致的问题。

整改必须遵守根目录 `AGENTS.md`：总控负责派发和独立验收；同一批次仅一个实施子智能体写共享目录；默认实施参数显式为 `gpt-5.6-luna / xhigh`，只有 P0/P1、并发、数据一致性、安全、生命周期或跨模块高风险任务使用 `max`。

## 2. 已经完成了什么

以下结论均为当前对话中的总控独立静态验收结果。由于本机缺少 Qt、CMake、CTest、Windows 和真实硬件，除非特别说明，仍需要编译和运行验证。

### 2.1 历史查询分页与流式导出

- 历史查询使用 `(timestamp, id)` keyset 分页，处理相同时间戳时不会漏项或重复。
- 最近数据分页固定查询结束时间，避免导出过程中插入的新数据改变结果集。
- 增加记录数量查询接口，并贯通到 `MonitorManager` 和导出页提供器。
- CSV、JSON、TSV 大批量导出改为有界分页流式写入，不再一次性构造完整内存内容。
- 导出使用 `QSaveFile`，关闭 direct-write fallback；provider 失败或 commit 失败时保留旧文件。
- 已有多页、大数据、质量/无效值、provider 失败和 commit 失败测试保护。

### 2.2 历史数据质量、错误和来源

- 数据库 schema 已升级到 v4。
- 历史记录持久化 `quality`、`origin`、`error_code`、`error_text`。
- 批量插入和五条历史查询路径已带上这些字段。
- `MonitorDataLogger` 会写入数据来源和错误信息，`MonitorManager` 会恢复这些元数据。
- 已补迁移和分页查询相关测试。

### 2.3 配置 schema、结构和参数边界

- `ProjectRuntimeConfig` 已集中定义最低/当前 schema 版本，缺失版本按当前版本处理，兼容 schema 1、2、3，拒绝未来版本和非整数版本。
- 加载 JSON 前先做结构校验；字段类型错误会包含完整字段路径和实际值。
- 加载失败不会覆盖当前项目路径、modified 状态或当前运行配置。
- 已增加 provider、映射、controller、参数范围、OPC 和 transport 参数边界校验。
- 非法值只报告错误，不静默钳制或改写配置。
- 总控额外要求并验收了独立结构错误测试，避免“未来 schema 错误”掩盖其它字段类型错误。
- 总控额外要求并验收了独立边界矩阵，每次恢复合法基线后只触发一个非法值。

主要相关文件：

- `src/common/ConfigTypes.h`
- `src/designer/ProjectController.cpp`
- `tests/project_save_close_test.cpp`

### 2.4 测试体系本批补全

- `dsl_script_editor_save_test` 已有 target，并已注册为 `DslScriptEditorSaveTest`。
- `src/core/tests/DataManagerTest.cpp` 和 `TaskSchedulerTest.cpp` 已恢复独立 `QTEST_MAIN`。
- 两份 core 测试已用不冲突的 target/CTest 名称接入：
  - `core_data_manager_test` / `CoreDataManagerTest`
  - `core_task_scheduler_test` / `CoreTaskSchedulerTest`
- 新增 `tests/dsl_compiler_semantics_test.cpp`，通过公开 `DSLCompilerInterface` 测试：
  - 基础 `PROGRAM/VAR` 成功编译；
  - legacy `_DrvDO` 归一化与成功编译；
  - 语法错误失败；
  - 未知功能块失败；
  - 成功时发布 `.code/.list/.typ/.rep`；
  - 失败时不得发布下载产物。
- Python/ANTLR/入口脚本等明确环境依赖缺失时返回 77，CTest 配置 `SKIP_RETURN_CODE 77`。
- 编译超时不再被当成 skipped，而是测试失败。
- DSL 测试使用 `QTemporaryDir`，每次运行隔离，避免旧产物污染结果。
- 本批未修改 `src/compiler/**` 或 Python DSL 编译器实现。

主要相关文件：

- `tests/CMakeLists.txt`
- `src/core/tests/DataManagerTest.cpp`
- `src/core/tests/TaskSchedulerTest.cpp`
- `tests/dsl_compiler_semantics_test.cpp`

### 2.5 已确认但尚需目标平台验证的 OPC/质量保护

- 现有 OPC 单元测试已覆盖配置序列化、后端选择、点位映射、读写失败和部分 Windows 回调行为。
- Windows 条件测试包含回调线程排队、停止后丢弃迟到回调、Good quality 和 UTC 时间戳等保护。
- 数据质量已在运行点表、处理器、日志、MonitorManager、历史分页和导出多个层面有测试。
- 这些不能替代真实 Matrikon OPC DA 的 COM activation、browse、AddGroup/AddItems、同步读写、订阅、重连和竞态验证。

## 3. 当前卡在哪里

### 3.1 本机无法完成编译和运行验收

当前环境没有可用的 Qt、CMake、CTest，也不是 Windows，无法运行新增/修改的 C++ 测试。当前准确结论只能是：

> 静态验收通过，编译与运行测试需要验证。

后续需要在具备 Qt/CMake/CTest 的构建环境重新配置构建目录并运行测试，不能把旧的 `build_codex_verify_mingw` 中的 `CTestTestfile.cmake` 当作当前源码已注册/已运行的证据。

### 3.2 `.code`、manifest 原子写入和绝对路径受冻结约束

只读审计确认 `src/compiler/DSLCompilerArtifacts.cpp` 中仍存在：

- 生成 `.code` 后直接 `Truncate` 重写元数据头，失败可能破坏最终产物；
- `runtime_points.json`、`runtime_manifest.json` 直接截断写入且缺少完整写入/commit 检查；
- `.code` 头、manifest 和编译 metadata 中持久化本机绝对路径；
- `RunController` 会把部分编译 metadata 合并进项目配置，形成跨机器路径泄露链。

但根目录 `AGENTS.md` 冻结 `src/compiler/**`，而用户同时明确要求“暂不修改 DSL 编译器实现”。因此不能擅自修改这些文件。继续此项前需要用户明确授权冻结目录的最小例外，并说明该授权是否仅限产物写入/元数据，不涉及编译语义实现。

### 3.3 两套下载入口需要产品选择

当前有两条真正独立的设备写入链：

1. `DownloadDockWidget -> DownloadManager -> ControllerBridge`
2. `RuntimeSessionController -> ControllerDeviceBackend`

它们分别拥有连接、握手、重试、取消、进度、错误映射和下载执行逻辑。第二条会话链带项目配置、artifact/manifest 和 dry-run 校验；第一条允许手工选择 profile/payload，更像工程诊断工具。

不能靠简单互调安全收拢。建议先决定：

- 以 RuntimeSession 会话链作为唯一生产下载入口；
- Download Dock 改为会话链 UI，或明确降级为“工程诊断工具”。

在产品定位未确定前，不应删除任一下载实现或改变硬件寻址行为。

### 3.4 删除未接入控制器存在 API 兼容风险

- `MonitorController` 和 `OutputPaneController` 被编译但没有生产实例化引用。
- `MainWindow` 当前直接持有 `MonitorWidget` 和输出文本控件。
- 直接删除这些类会收缩静态库公开 API，可能影响仓库外消费者。

可以先安全地清理 MainWindow 私有死字段、空 Dock fallback 和错误文档；删除公开类前需要确认没有外部调用者。

### 3.5 工作树已有大量历史/用户改动

当前工作树不是干净基线，包含大量已修改和未跟踪文件，冻结目录也有既有改动。不要把 `git status` 中所有变化误认为本轮生成，也不要为了“清理”执行回退。后续必须按授权文件和实际 diff 做范围验收。

## 4. 下一步计划

建议严格串行，完成一批、总控独立验收一批：

1. **先做可用环境的运行验证**
   - 重新配置构建目录。
   - 构建并运行 `DslScriptEditorSaveTest`、`CoreDataManagerTest`、`CoreTaskSchedulerTest`、`DslCompilerSemanticsTest`。
   - 同时运行已有 DataManager、TaskScheduler、历史分页、导出、OPC 配置和诊断快照测试。
   - Python/ANTLR 缺失时确认 DSL 测试显示 skipped，而不是 passed；编译超时必须显示 failed。
2. **补剩余测试保护**
   - 普通非分页 CSV/JSON/TSV 导出的 commit 失败保留旧文件直接测试。
   - DataManager -> MonitorManager -> 流式导出的完整链路测试。
   - OPC per-item HRESULT、更多 `VARIANT` 类型、质量矩阵和迟到回调竞态测试。
   - 真实 OPC DA 互操作测试单独放入 Windows + Matrikon 专项，不混入普通跨平台 CTest 门禁。
3. **处理编译产物安全项**
   - 先取得冻结目录的明确授权。
   - 最小修改 `DSLCompilerArtifacts.cpp`：用 `QSaveFile` 原子发布 `.code` 和 runtime JSON；去除/相对化绝对路径。
   - 补测试保证 `.code`、manifest、project JSON 不含本机绝对路径，同时运行时仍能解析相对 manifest。
4. **做低风险 P3 收口**
   - 先纯文件拆分 `MainWindow`、`MonitorManager` 的成员函数，不改公共接口和运行语义。
   - 清理 MainWindow 私有死字段和永久为空的 Dock fallback。
   - 修正文档中“MonitorController 已接入”“MainWindow 约 900 行”“监控是 Dock”“仅支持 CSV”等失实描述。
5. **最后处理产品/API 决策项**
   - 决定唯一生产下载链和诊断入口定位。
   - 确认无外部消费者后，再决定是否从构建和源码中删除未接入控制器。
6. **目标平台最终验证**
   - Windows + Matrikon OPC DA：activation、browse、group/item、读写、订阅、断线重连、停止后迟到回调。
   - 真实控制器/通信链：读取失败不得生成正常 `0`，下载取消/重试/重连行为正确。
   - 大数据库和大导出：内存有界、分页稳定、原文件在失败时完整保留。

## 5. 踩过的坑，绝对不要再踩

1. **不要弄错子智能体推理强度。**
   - 默认是 `gpt-5.6-luna / xhigh`，不是 `max`。
   - 只有 P0/P1、并发、数据一致性、安全、生命周期或跨模块高风险任务才用 `max`。
   - 创建时必须显式设置；无法设置或回执不符就阻断。
2. **不要让多个写入子智能体并行。**
   - 同一批次只能有一个实施子智能体写共享目录。
   - 只读审计可以并行，但不能改文件、测试或 CMake。
3. **不要让总控代替实施子智能体写代码。**
   - 总控派发、等待、审查和验收；实施、补测试、自检交给唯一子智能体。
   - 子智能体执行期间总控只等待，不要同时改共享文件。
4. **不要把子智能体交接报告当成验收。**
   - 总控必须实际查看差异、失败路径、调用链、测试可信度、冻结范围和未运行验证。
   - 本轮就曾发现：结构错误测试被未来 schema 错误掩盖；DSL 测试曾把编译超时当 skipped；固定输出目录会被旧产物污染。都是总控验收后退回返修的。
5. **不要用组合非法值掩盖边界测试。**
   - 每个非法场景先恢复完整合法基线，再只改一个字段。
   - 错误断言必须包含字段路径和实际非法值。
6. **不要把环境缺失伪装成测试通过。**
   - Python/ANTLR 明确缺失应返回 77，让 CTest 显示 skipped。
   - 超时、语义错误、项目模块损坏或产物错误必须失败，不能被宽泛的错误字符串吞掉。
7. **不要让测试复用固定输出目录。**
   - 使用 `QTemporaryDir` 或明确清理，防止旧 `.code`/sidecar 造成假绿或假红。
8. **不要用旧构建目录证明当前测试已注册或已通过。**
   - 修改 `tests/CMakeLists.txt` 后必须重新配置。
   - 旧 `CTestTestfile.cmake` 只代表旧源码状态。
9. **不要触碰冻结编译器目录。**
   - `src/compiler/**` 和 `third_party/custom_dsp_language/compile/**` 需要用户明确授权。
   - “补 DSL 语义测试”不等于授权修改编译器实现。
10. **不要清理或回退用户既有改动。**
    - 禁止 `git reset --hard`、`git checkout --` 等破坏性回退。
    - 不要全项目格式化，不要借整改顺带清理历史改动或 BOM/行尾。
11. **不要因为 `git status` 显示冻结目录有改动，就断言本批改了冻结目录。**
    - 工作树本来就脏。按本批授权文件、派发范围和实际差异审查。
12. **不要直接删 `MonitorController`、`OutputPaneController`。**
    - 它们未接入不代表没有仓库外 API 消费者。先清理私有死路径并取得兼容性确认。
13. **不要简单把两套下载实现互相调用。**
    - 它们的连接所有权、校验、硬件寻址和能力不同。先做产品决策，再统一入口。
14. **不要把单元测试等同于真实 Matrikon 验收。**
    - COM activation、browse、订阅、重连和竞态必须在 Windows + Matrikon 环境验证。
15. **不要声称“已完成”而隐去运行限制。**
    - 当前没有 Qt/CMake/CTest/Windows/硬件时，统一结论是“静态验收通过，编译与运行测试需要验证”。

## 6. 接手时的第一步

1. 完整阅读根目录 `AGENTS.md`。
2. 阅读本文件，确认当前批次和冻结/产品决策阻塞。
3. 查看工作树，但不要回退任何既有改动。
4. 若继续实施，先定义一个小而清晰的串行批次，列出唯一授权文件和禁止范围。
5. 新建实施子智能体时显式使用 `gpt-5.6-luna / xhigh`；高风险批次才升到 `max`。
6. 完成后由总控独立验收，再进入下一批。
