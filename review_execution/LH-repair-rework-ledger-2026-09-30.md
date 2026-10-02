# LH 第二轮验收返修记录 · 2026-09-30

基线 `bed4d849ebebd9e6232a8d727e037005ec507d00`，源码根 `D:/Table/LH`。

R01–R07 已返修，相关本地验证通过，待总控独立验收。完整 all 构建通过；Python 82 项全部通过；六个相关正式 CTest 均通过。额外设计器套件仍有两处失败，整库统一验收不能标为通过。本轮未提交或推送。

## R01 · 补齐查询命名空间并保留错误返回契约

状态：已返修，本地验证通过，待总控验收。关联原任务：T24、T25、V01。

位置：[src/core/DataManager.cpp:1345](D:/Table/LH/src/core/DataManager.cpp:1345)；[src/core/HistoryQuery.cpp:43](D:/Table/LH/src/core/HistoryQuery.cpp:43)；[tests/data_manager_history_paging_test.cpp:82](D:/Table/LH/tests/data_manager_history_paging_test.cpp:82)。

- 四个入口显式使用 Core::HistoryQuery，查询参数、分页方向、UTC 和 maxId 规则不变。
- 正式测试暴露非法 pageSize 的 errorText 为空；共享查询层补齐分页和 latest N 参数错误文本，保留状态/错误码并补充回归。

兼容性：仅补齐类型限定和错误诊断；不放宽参数范围，不改变查询结果。

验证：完整 all 构建通过。 DataManagerHistoryPagingTest：7 通过、0 失败。 AsyncDatabaseWorkerTest 正式 CTest 通过。

仍待验收：总控继续按两个 adapter 的完整 UTC、maxId、latest N、count 矩阵独立验收。

## R02 · 诊断快照正式目标链接生产日志实现

状态：已返修，本地验证通过，待总控验收。关联原任务：T13、T32、V01。

位置：[tests/CMakeLists.txt:409](D:/Table/LH/tests/CMakeLists.txt:409)。

- diagnostic_snapshot_test 链接 core_module，使用真实 AppLogging::overloadStatus 实现。

兼容性：测试沿用生产日志库及其 QtCore/QtSql 依赖；未加入假实现或重复日志源码。

验证：正式 diagnostic_snapshot_test 链接成功。 DiagnosticSnapshotTest 正式 CTest 通过。 完整 all 构建通过。

## R03 · 参数批次先更新、后通知，守护代次和寿命

状态：已返修，本地验证通过，待总控验收。关联原任务：T07、T30。

位置：[src/runtime/ParameterController.cpp:244](D:/Table/LH/src/runtime/ParameterController.cpp:244)；[src/runtime/ParameterController.cpp:306](D:/Table/LH/src/runtime/ParameterController.cpp:306)；[src/runtime/ParameterController.cpp:762](D:/Table/LH/src/runtime/ParameterController.cpp:762)；[src/runtime/ParameterController.cpp:1034](D:/Table/LH/src/runtime/ParameterController.cpp:1034)；[src/runtime/ParameterController.h:93](D:/Table/LH/src/runtime/ParameterController.h:93)；[tests/parameter_controller_test.cpp:82](D:/Table/LH/tests/parameter_controller_test.cpp:82)；[tests/parameter_controller_test.cpp:230](D:/Table/LH/tests/parameter_controller_test.cpp:230)。

- 回读、写入及超时转换先完成整批状态修改，再用拥有独立数据的通知列表发信号，不携带 m_states 迭代器跨过通知。
- 每次通知后用 QPointer、操作代次和定义代次检查生命周期；同步入口检查写入、回读和事件等待后的失效；异步入口收到失效结果立即停止旧批次。
- clear、参数身份/类型/可编辑属性变更和取消使旧代次失效；loadDefinitions/clear 本身在取消通知重入后不覆盖更新的定义。
- 同点位、同类型、同可编辑属性的值刷新保留在途批次，兼容主窗口 statesChanged -> refreshInspectorPanel -> loadDefinitions 调用链。
- 48 个多参数回归覆盖外部回读、同步/异步轮询，Confirmed/Mismatch/Timeout 内清空、切工程、取消、删除对象及相同定义刷新；另覆盖主窗口式聚合通知刷新。

兼容性：一批内全部状态在首次通知前已生效；定义身份变化终止旧通知。终态通知期间若对象销毁或批次被替换，不继续发送旧 readbackFinished。相同身份的值刷新继续原批次。

验证：ParameterControllerTest 和 RuntimeSessionServiceTest 在最终代码上正式 CTest 通过。 补充刷新回归前的详细参数运行：88 通过、0 失败；最终正式参数套件包含新增同步/异步刷新回归。

仍待验收：原总控独立重入探针未在其外部 harness 中重跑；总控应以最终源码重链接并复验。 真实设备回读和完整 GUI 在线参数流程仍待总控验收。

## R04 · 用具体契约原因校验 21 个 incomplete 功能块

状态：已返修，本地验证通过，待总控验收。关联原任务：T16、C03。

位置：[third_party/custom_dsp_language/compile/tests/test_cli.py:8](D:/Table/LH/third_party/custom_dsp_language/compile/tests/test_cli.py:8)；[third_party/custom_dsp_language/compile/tests/unit/test_semantics.py:863](D:/Table/LH/third_party/custom_dsp_language/compile/tests/unit/test_semantics.py:863)。

- 两种 CLI 的 describe 输出核对实际缺失原因，兼容 Rich 换行，不依赖 TODO。
- 保留准确 21 项名称枚举、incomplete 状态、非 supported 断言；逐块验证实例调用和直接调用均拒绝，错误包含具体缺失原因。

兼容性：只更新和加强测试，不修改生产 capability 状态或虚构 LH 支持。

验证：Python 完整套件：82 通过、0 失败、0 跳过。

## R05 · 区分非法 literal 与合法 lvalue 输出绑定

状态：已返修，本地验证通过，待总控验收。关联原任务：C02。

位置：[third_party/custom_dsp_language/compile/tests/unit/test_language_boundaries.py:32](D:/Table/LH/third_party/custom_dsp_language/compile/tests/unit/test_language_boundaries.py:32)。

- System/Author 和 PID/Kp 分别验证 => 1 在语法阶段拒绝、=> x 成功建 AST 后在后端因输出目标契约未定义而拒绝。
- 两条路径均检查 .code/.list/.typ 旧产物删除以及 .rep 诊断保留。

兼容性：只修正测试输入和阶段断言，不放宽语法或输出契约限制。

验证：Python 完整套件：82 通过、0 失败、0 跳过。

## R06 · 非法 DSL 使用有效 Profile，缺地址另测

状态：已返修，本地验证通过，待总控验收。关联原任务：T23、V01。

位置：[tests/dsl_compiler_semantics_test.cpp:348](D:/Table/LH/tests/dsl_compiler_semantics_test.cpp:348)；[tests/dsl_compiler_semantics_test.cpp:401](D:/Table/LH/tests/dsl_compiler_semantics_test.cpp:401)。

- 非法 DSL fixture 提供 dataAddress=210、chunkWords=1，确保场景进入编译。
- 另用合法 DSL 配合缺 dataAddress 的 Profile，检查一次 failedToStart、正确代次、无完成信号及无发布产物；保留无 Profile 离线与显式无效 Profile 场景。

兼容性：仅测试 Profile 地址，用于离线测试；未修改生产 Profile 校验，不代表硬件地址有效。

验证：DslCompilerSemanticsTest 详细运行：8 通过、0 失败、0 跳过。 正式 CTest 通过。

## R07 · 按受理与异步完成分别断言

状态：已返修，本地验证通过，待总控验收。关联原任务：T07。

位置：[tests/parameter_controller_test.cpp:694](D:/Table/LH/tests/parameter_controller_test.cpp:694)；[tests/parameter_controller_test.cpp:750](D:/Table/LH/tests/parameter_controller_test.cpp:750)；[tests/parameter_controller_test.cpp:180](D:/Table/LH/tests/parameter_controller_test.cpp:180)；[tests/parameter_controller_test.cpp:269](D:/Table/LH/tests/parameter_controller_test.cpp:269)。

- 全写失败场景断言调用被受理，再有界等待最终失败；计数确认没有异步读请求、ApplyFailed 终态且只完成一次。
- Mismatch 场景有界等待回读终态，再检查唯一失败完成。
- 手动交付取消批次的迟到结果，确认不会影响替代批次；分别阻塞写/读阶段验证 15 秒总时限、超时终态和迟到结果忽略。

兼容性：测试区分 admission bool 与终态结果，生产 API 和 15 秒总时限不变。

验证：最终 ParameterControllerTest 正式 CTest 通过。

## 完整构建中补充的 Qt 5 修正

[ProblemsPanel.cpp:225](D:/Table/LH/src/designer/ui/ProblemsPanel.cpp:225) 改用 `at(size - 1)` 取得 `QChar`，修复 `QCharRef::isHighSurrogate()` 编译错误；不改变 UTF-16 截断行为。完整 all 构建及对应有界详情回归通过。

## 验证证据

- 完整 all 构建：通过。[rework-build-final.log](D:/Table/LH/review_execution/rework-build-final.log)
- 完整 Python 套件：82 passed, 0 failed, 0 skipped。[rework-python-tests.xml](D:/Table/LH/review_execution/rework-python-tests.xml)
- 最终参数与运行时正式 CTest：2/2 通过。[rework-ctest-runtime-final.xml](D:/Table/LH/review_execution/rework-ctest-runtime-final.xml)
- 其余四个相关正式 CTest：DataManagerHistoryPagingTest、AsyncDatabaseWorkerTest、DslCompilerSemanticsTest、DiagnosticSnapshotTest 均通过。[rework-ctest-final.xml](D:/Table/LH/review_execution/rework-ctest-final.xml)；[rework-ctest-history-final.xml](D:/Table/LH/review_execution/rework-ctest-history-final.xml)
- 详细 DSL 套件：8 passed, 0 failed, 0 skipped。[rework-dsl-final.xml](D:/Table/LH/review_execution/rework-dsl-final.xml)
- 详细历史分页套件：7 passed, 0 failed, 0 skipped。[rework-history-final.xml](D:/Table/LH/review_execution/rework-history-final.xml)
- 补充设计器套件，Python 解释器已加入 PATH：21 passed, 2 failed, 0 skipped。[rework-designer-final.xml](D:/Table/LH/review_execution/rework-designer-final.xml)
- git diff --check：通过，只有 CRLF/LF 提示。

## 剩余问题与验收边界

- GUI-01：[testRealCompilerDiagnosticsNavigateThroughPanel(rewritten-legacy-fallback)](D:/Table/LH/tests/designer_p1_robustness_test.cpp:174)：sawFallback 为 false；未发现“源文件位置无法可靠映射”诊断。 本轮七项之外，已记录，未修改。
- GUI-02：[testEditorFocusKeyClickCapture](D:/Table/LH/tests/designer_p1_robustness_test.cpp:515)：Ctrl+W ambiguous shortcut；spyClose.count() 为 0，期望 1。 本轮七项之外，已记录，未修改。
- 未运行完整 39 项 CTest；七个选择的正式套件中六个相关套件通过，设计器套件失败，不能报告整体 CTest 通过。
- 初次分页失败、初次完整构建失败及补充设计器失败证据保留，最终成功证据单独命名。
- 未修改原验收报告或原 implementation ledger。未提交、推送或连接真实控制器。
- 此前 T03、T04、C02、C04–C07、FB01–FB21 的 28 项契约不足任务仍未完成；安全拒绝不能认定功能已经实现。

