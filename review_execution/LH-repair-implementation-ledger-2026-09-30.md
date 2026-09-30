# LH 修复实施记录 · 2026-09-30

基线 f3f0242；根 D:/Table/LH。仅实施代码/必要测试与契约；未运行构建、CTest、Python 测试、GUI、安装包或设备验收，未提交/推送。最终验收由总控执行。

本目录为授权范围内的实施交付目录；原审查与任务清单保持原样。实施中的条目可能尚未开始修改，具体以 behavior/files 为准。

## T01 · 已实现待总控验收 · 打开辅助脚本会改写工程主脚本和构建顺序

文件：src/designer/ProjectController.cpp；src/designer/ProjectController.h；src/designer/ProjectExplorerWidget.cpp；src/designer/ProjectExplorerWidget.h；src/designer/MainWindowUi.cpp；tests/project_save_close_test.cpp

行为：当前编辑脚本切换不修改 mainScriptPath/dslScriptPath 或已配置脚本顺序；新增显式设置主脚本菜单与受目录限制的控制器 API；辅助文档仍保存到当前文件

兼容性：setCurrentScriptFile 改为编辑文档选择；显式入口设置改用 setMainScriptFile

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R ProjectSaveCloseTest --output-on-failure`

剩余问题：总控覆盖诊断导航、切换与保存后重载

## T02 · 已实现待总控验收 · NaN/Inf 可以被视为有效采样，误清报警并污染历史批次

文件：src/monitor/MonitorSample.h；src/monitor/MonitorManager.cpp；src/monitor/MonitorChannel.cpp；src/monitor/MonitorHistoryService.cpp；tests/monitor_manager_backend_test.cpp

行为：构造和 manager 单/批边界统一非有限值 Bad/valueValid=false/errorCode；channel 不把无效/非有限样本加入有效缓冲或发 sampleAdded，最后无效样本不清阈值；历史记录转 Sample 也归一化有效性，保留 logger 对 Bad/NULL 历史的契约

兼容性：数据库对有效非法输入的拒绝保留；仅采样入口做质量降级

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R 'MonitorManagerBackendTest|AsyncDatabaseWorkerTest' --output-on-failure`

剩余问题：总控补同批好/坏记录落库检查

## V01 · 受前置依赖限制 · 建立当前工作区和清洁安装包的可复现验收基线

文件：

行为：此任务和全部验收仅由总控执行

兼容性：待具体实现后记录

总控检查（本对话未执行）：

剩余问题：本对话不建立或运行验收基线

## C01 · 已实现待总控验收 · 建立当前 LH 目标编码、地址单位与版本支持证据表

文件：review_execution/lh-target-observed-contract-v1.json；review_execution/target-contract-and-language-boundaries-v1.md

行为：冻结当前 LH 生成器可观察行为与未知固件项，版本化 fixture 明示 downloadableTargetCertified=false；读取真实 SEHC 源/list/typ/code/rep，保存归档条目 SHA256、行号和摘录；40967/751、41148/851 差异可追踪，LM 地址与 ID 不复制到生产默认值

兼容性：这是离线契约取证；不会赋予任何未经确认的设备支持

总控检查（本对话未执行）：
- `总控独立复核 fixture 对照当前源码与程序.zip`
- `取得 LH 固件版本对应指令/操作数、地址/对齐、初始化/写回支持表`

剩余问题：LH 固件支持表、地址单位/对齐、引用/初始化/写回/IQ/任务协议资料仍缺，构成 C02/T03/T04 等生产编码前置

## T03 · 资料不足 · 简单变量初始化通过校验，却没有进入生成产物

文件：third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py；third_party/custom_dsp_language/compile/tests/unit/test_language_boundaries.py；tests/dsl_compiler_semantics_test.cpp

行为：有效简单变量初始值经过类型检查后明确拒绝无契约的初始化写回，功能块初始值同样拒绝；不再成功生成忽略初始值的控制产物，错误路径仍清理旧产物

兼容性：此前静默忽略的初始化现在失败；无初始值声明与 compileOnly 流程保留

总控检查（本对话未执行）：
- `总控执行 language_boundaries/semantics 与离线无 Profile 用例`

剩余问题：缺 LH 固件初始化写回指令、目标地址单位、执行时机，不能从 LM 地址推导生产实现

## T04 · 资料不足 · 常量赋值生成了另一个内存块，没有写入变量地址

文件：third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py；third_party/custom_dsp_language/compile/tests/unit/test_language_boundaries.py；third_party/custom_dsp_language/compile/tests/unit/test_semantics.py

行为：保留常量折叠及类型/数值诊断，阻断使用另一个 _const_var 内存块冒充变量写回；已调整错误的赋值 golden 断言，增加 BOOL/INT/REAL/重复赋值拒绝及旧产物清理用例

兼容性：过去看似成功但未写变量地址的赋值改为显式失败

总控检查（本对话未执行）：
- `总控执行 Python semantics/language_boundaries/runtime_layout 用例`

剩余问题：缺目标变量写回指令及操作数契约，尚不能实现真正赋值

## T05 · 已实现待总控验收 · 工程树创建/递归删除没有工程根目录与链接边界检查

文件：src/common/PathSecurityUtils.h；src/designer/ProjectExplorerWidget.cpp

行为：创建/删除统一校验 canonical containment、父目录跳转、根目录和路径祖先；拒绝符号链接和 Windows reparse point；递归删除前扫描子树；文件创建 NewOnly 避免检查后覆写竞争

兼容性：含链接的目录删除明确拒绝；普通子目录功能保留

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R ProjectSaveCloseTest --output-on-failure`

剩余问题：需总控增加/执行 Windows junction、外部哨兵与同前缀目录回归；检查后恶意并发换链接的 OS 级 TOCTOU 尚未完全消除

## T06 · 已实现待总控验收 · 旧数据库迁移只复制主文件，会漏掉 WAL 中已提交数据

文件：src/core/DataManager.cpp

行为：旧库以只读连接 VACUUM INTO 生成包括已提交 WAL 的一致 staging 快照；staging 完整性检查后不覆盖地提交目标；失败清理并保留旧库

兼容性：要求 Qt SQLite >=3.27；旧驱动明确失败而不退回主文件复制

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R 'AsyncDatabaseWorkerTest|OptimizationAcceptanceTest' --output-on-failure`

剩余问题：总控需覆盖活动 WAL、并发写者、损坏库和目标竞争；本对话未访问用户库

## T07 · 已实现待总控验收 · 参数 Async 下发和回读仍同步阻塞 GUI

文件：src/communication/IDeviceBackend.h；src/communication/ControllerDeviceBackend.h；src/communication/ControllerDeviceBackendPoints.cpp；src/communication/VirtualDeviceBackend.h；src/communication/VirtualDeviceBackend.cpp；src/designer/ParameterController.h；src/designer/ParameterController.cpp；tests/parameter_controller_test.cpp

行为：参数写入与每次回读使用异步完成接口；生产 transport actor 与虚拟定时完成各自实现；15 秒总 deadline、取消令牌、代次和 QObject 上下文隔离；先更新批次状态再发信号，避免回调切工程使容器迭代失效；取消后晚到结果不继续旧操作

兼容性：同步接口保留；异步入口明确拒绝不支持 async write/read 的后端

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R ParameterControllerTest --output-on-failure`
- `总控用延迟生产 transport 验证 GUI 心跳、销毁和总时限`

剩余问题：新增虚拟写入期间心跳/取消回归未执行；生产 transport 队列与 deadline 需专项验收

## T08 · 已实现待总控验收 · Classic Modbus OPC 点位忽略数据类型与多寄存器编码

文件：src/communication/ClassicOpcServer.cpp；src/communication/ClassicOpcServer.h

行为：Classic 读写复用 RuntimePointRegisterCodec；范围/有限值/宽度/短响应先校验；BOOL register 位字段按原地址提取；写入采用保留其它位的 read-modify-write；显式非法地址、unit、bit 和列表数量拒绝；读取失败存 Bad 空值

兼容性：默认 Matrikon 不改动；holding+ReadOnly 不改变寄存器区

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R OpcServerConfigTest --output-on-failure`
- `总控补齐 Classic transport 负 INT、UINT、DINT、REAL、字序、BOOL 和短响应矩阵`

剩余问题：尚未增加完整 Classic transport mock 读写矩阵；T26 异步批量轮询独立实施

## C02 · 资料不足 · 分离构造参数、运行时字段、地址引用和无实例操作

文件：third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/registry.py；third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py；third_party/custom_dsp_language/compile/src/lh_compiler/backend/emitter.py；third_party/custom_dsp_language/compile/tests/unit/test_runtime_layout.py；third_party/custom_dsp_language/compile/src/lh_compiler/backend/operands.py；third_party/custom_dsp_language/compile/tests/unit/test_operand_boundaries.py

行为：构造参数与显式 RuntimeFieldDef 分离；运行时字段验证唯一名、方向、边界和重叠；get_parameter_offset 只查询经校验的运行时字段，默认 offset 从伪零变为未知；list 不再为构造参数伪造地址；typ 明确 constructor_parameter 与 runtime_field，显式字段共享 metadata layout；新增有类型 Immediate/VariableRef/MemberRef/Init/Write/NoInstanceOperation 模型，引用无法进入常量编码器；只读/输出字段/类型不同写回拒绝；Parameter.is_output 和非 IN 构造参数不再静默按输入常量处理；list/typ 对显式字段共享布局

兼容性：旧定义仅描述常量参数，未证明的参数地址留空；没有填造引用编码或无实例操作 ABI

总控检查（本对话未执行）：
- `总控执行 operand_boundaries/runtime_layout/language_boundaries/semantics，验证引用拒绝、输出方向和同字段 sidecar 一致性`

剩余问题：VariableRef/MemberRef 目标编码、可写字段固件证据、合法无实例操作分类仍缺契约，不能标为完整实现

## T09 · 已实现待总控验收 · FC16 写寄存器和下载分块错误地允许 124/125 个寄存器

文件：src/communication/ModbusLimits.h；src/communication/ModbusInterface.cpp；src/communication/ModbusInterface.h；src/communication/DownloadProfile.cpp；src/communication/ControllerBridge.cpp；src/communication/ControllerDeviceBackendDownload.cpp

行为：共享 ReadRegisters=125/WriteRegisters=123/WriteCoils=1968；Profile/正式后端预检拒绝超限块和写入列表；Bridge/底层写入口同步限制

兼容性：125 个寄存器读保持；124/125 写计划提前拒绝

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R 'ControllerDeviceBackendTest|CommunicationRoutingTest' --output-on-failure`

剩余问题：总控需核对 123/124/125 无设备写入预检矩阵

## T10 · 已实现待总控验收 · Classic 后端把只读 holding 点位改成 input 区

文件：src/communication/ClassicOpcServer.cpp

行为：去掉 ReadOnly 对 holding 区的隐式改写；未配置区仍默认 holding

兼容性：访问权限继续用于拒绝写入；依赖旧隐式 input 行为的配置应显式写 area=input

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R OpcServerConfigTest --output-on-failure`

剩余问题：总控覆盖 holding+ReadOnly 与 input 配置

## T11 · 已实现待总控验收 · 业务日志与诊断快照的敏感键脱敏规则不一致

文件：src/common/LogSafety.h；src/core/AppLogging.cpp；src/diagnostics/DiagnosticSnapshotService.cpp；tests/app_logging_test.cpp

行为：统一规范化敏感键后缀策略及递归对象/数组脱敏；保留 tokenCount；支持自由文本 key=value、Bearer 与 URI 密码清洗

兼容性：掩码统一为 [REDACTED]；任意自然语言不能可靠自动判定，调用方禁止传入秘密

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R 'app_logging_test|DiagnosticSnapshotTest' --output-on-failure`

剩余问题：总控覆盖 api_key/cookie/clientSecret 与自由文本回归

## T12 · 已实现待总控验收 · 普通日志保留换行和控制字符，可伪造多条日志外观

文件：src/common/LogSafety.h；src/core/AppLogging.cpp；tests/app_logging_test.cpp

行为：普通 Qt/业务日志统一一次物理行转义，含 CR/LF/TAB、控制字符和 Unicode 行分隔符；保持 UTF-8 完整字符截断

兼容性：多行内容在日志中显示为转义序列

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R app_logging_test --output-on-failure`

剩余问题：总控核对一次转义及控制字符边界

## T13 · 已实现待总控验收 · 日志队列过载后丢弃普通/业务事件，生产端没有丢失提示

文件：src/core/AppLogging.cpp；src/core/AppLogging.h；src/diagnostics/DiagnosticSnapshotService.cpp

行为：普通日志队列保留 64 KiB 给业务终态；终态超载降级 stderr；writer 汇总丢失数和时间范围；诊断快照输出队列和丢失状态

兼容性：队列仍 1 MiB；Critical/Fatal 原保证保留

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R app_logging_test --output-on-failure`

剩余问题：慢 sink 突发与终态降级需总控专项验证

## T14 · 已实现待总控验收 · 采集异常回调和历史任务回调可把异常抛到 Qt 事件循环

文件：src/monitor/MonitorManagerPolling.cpp；src/core/AsyncDatabaseWorker.cpp

行为：errorHandler 再抛异常转稳定警告，不再递归；历史回调异常转 workerError，pending 计数由 scope guard 恢复

兼容性：扩展回调不得抛到 Qt 事件循环；历史回调异常通知通过 workerError

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R 'AsyncDatabaseWorkerTest|MonitorManagerBackendTest' --output-on-failure`

剩余问题：历史请求自身的终态错误模型需结合 T24 检查

## T15 · 已实现待总控验收 · ANTLR 导入失败在模块导入阶段直接 sys.exit()

文件：third_party/custom_dsp_language/compile/src/lh_compiler/compiler.py；third_party/custom_dsp_language/compile/src/lh_compiler/cli/commands.py；third_party/custom_dsp_language/compile/tests/unit/test_dependency_boundary.py

行为：缺解析依赖时允许模块导入，初始化抛可捕获 CompilerDependencyError；退出限制到 CLI；Click/脚本 CLI 给出依赖原因

兼容性：依赖齐全的编译流程保留；库宿主不被 sys.exit 中断

总控检查（本对话未执行）：
- `python -m pytest third_party/custom_dsp_language/compile/tests/unit/test_dependency_boundary.py`

剩余问题：总控在缺 ANTLR/grammar 与依赖完整环境分别执行

## T16 · 已实现待总控验收 · 21 个 incomplete 功能块在默认 Snippet 中全部标成 supported

文件：resources/snippets/default_snippets.json；src/designer/DslCompletionEngine.h；src/designer/DslCompletionEngine.cpp；src/designer/DslDragDropHandler.cpp；src/designer/DslScriptEditor.cpp；src/designer/ProgramBlocksWidget.cpp；src/designer/SnippetRepository.cpp；src/designer/MainWindowUi.cpp

行为：140 个资源的 status/incompleteReason 同步当前定义；21 个 incomplete 可查询但补全/拖拽/双击插入拒绝；未完成状态与原因可见；supported 显示为本地编译契约而非硬件支持；工程覆盖模板不得提升内置 status/compilerName/capability；组件补全模型每次重建过滤

兼容性：保留功能目录及项目新建自定义 Snippet；已有 supported 定义仍可插入

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R DesignerP1RobustnessTest --output-on-failure`
- `总控比对 registry 与资源 140 项并覆盖项目级同名覆盖`

剩余问题：105 个空参数项的完整功能契约未确认，见 C03 能力矩阵

## T17 · 已实现待总控验收 · 日志关键字猜测严重度造成重复和过期的“系统”问题

文件：src/designer/MainWindow.cpp；tests/designer_p1_robustness_test.cpp

行为：普通日志只进入输出面板；不再按 error/warn/失败猜测严重度；问题由明确错误信号或结构化诊断产生，消除构建日志系统副本

兼容性：原始编译输出仍保留；普通文本不自动成为问题

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R DesignerP1RobustnessTest --output-on-failure`

剩余问题：总控覆盖失败后成功和晚到代次隔离

## T18 · 已实现待总控验收 · ProblemsPanel 行数与单条消息长度没有上限

文件：src/designer/ui/ProblemsPanel.cpp；src/designer/ui/ProblemsPanel.h；tests/designer_p1_robustness_test.cpp

行为：每来源保留 250 行、总 1000 行，摘要计数随淘汰修正；单条显示 4096 字符，完整长消息写临时详情文件并提供按钮

兼容性：详情文件随面板销毁清理；全量原始编译输出仍在日志

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R DesignerP1RobustnessTest --output-on-failure`

剩余问题：详情累计 16 MiB 上限且清空时释放；超限明确提示未保留详情；总控验收磁盘/Unicode边界

## T19 · 已实现待总控验收 · Python wheel 不会包含 compiler 依赖的 grammar 包

文件：third_party/custom_dsp_language/compile/pyproject.toml；third_party/custom_dsp_language/compile/.gitignore；third_party/custom_dsp_language/compile/grammar/__init__.py；third_party/custom_dsp_language/compile/grammar/LHLexer.py；third_party/custom_dsp_language/compile/grammar/LHParser.py；third_party/custom_dsp_language/compile/grammar/LHListener.py；third_party/custom_dsp_language/compile/grammar/LHVisitor.py；CMakeLists.txt

行为：wheel 显式包含 grammar 与全部 lh_compiler 子包；解除解析器发布源文件忽略，生成器/运行时锁定 4.13.2；桌面安装额外包含 grammar 初始化模块

兼容性：使用现有本地生成解析器；未执行生成/构建/安装

总控检查（本对话未执行）：
- `python -m build third_party/custom_dsp_language/compile`
- `在隔离环境安装 wheel 并离开源码树执行 lmc compile minimal.lh`

剩余问题：总控核对生成器版本、wheel 内容和离开源码树的入口

## C03 · 已实现待总控验收 · 逐项核实 119 个 supported 标记与空参数定义

文件：review_execution/function-block-capability-matrix-v1.json；review_execution/target-contract-and-language-boundaries-v1.md；resources/snippets/default_snippets.json

行为：140 项逐项记录源码/行号/status/参数数与能力限制；105 空契约项归类为仅现行无参调用、完整功能未核实；不据 supported 数量声称硬件能力；21 missing-contract 和14有参数定义分别归类；未证实无实例操作不伪造实例例外

兼容性：不在审计卡一次实现或禁用105块；明确空定义限制，具体契约需独立证据

总控检查（本对话未执行）：
- `总控核对140/119/21与105/14计数和每项来源`
- `复核真正无参数标记与缺失契约的差别`

剩余问题：没有固件/运行时布局证据的项 hardwareSupported=null；不能标成可下载支持

## C06 · 资料不足 · 支持明确声明的重复参数组和长度可变契约

文件：review_execution/compiler-pending-contract-boundaries.md；review_execution/function-contract-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py

行为：核对既有 AST 与拒绝边界；未知目标不生成伪运算、分支/循环、重复组或数组元素写回；旧调用顺序/成员引用仅保留于离线证据，不将地址编码为数值常量

兼容性：明确拒绝保留；本项生产 lowering/序列化仍未实现

总控检查（本对话未执行）：
- `总控确认缺契约子集的错误路径及旧产物清理`
- `契约明确后再执行本项正向语义与目标编码验收`

剩余问题：每个可变组的明确目标编码与长度限制

## T20 · 已实现待总控验收 · 工程树删除未与打开/未保存的文档协调

文件：src/designer/ProjectExplorerWidget.h；src/designer/ProjectExplorerWidget.cpp；src/designer/ProjectController.h；src/designer/ProjectController.cpp；src/designer/MainWindow.h；src/designer/MainWindowExplorer.cpp；src/designer/MainWindowUi.cpp；src/designer/RuntimeSessionController.h；src/designer/RuntimeSessionController.cpp；tests/project_save_close_test.cpp

行为：工程树只发删除请求；处理全部受影响 dirty 文档的保存/放弃/取消，活动构建/运行时拒绝；工程控制器暂存路径后写配置；失败恢复原路径，成功清理脚本引用及已发布产物指针；成功后关闭辅助文档、清空受影响 DSL 路径及缓冲、移除诊断并使会话编译产物失效；保护工程根和 project_config.json；保留 T05 路径/链接校验

兼容性：删除主脚本后须重新显式配置入口；未清理的暂存目录会明确报告

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R ProjectSaveCloseTest --output-on-failure`
- `总控覆盖目录多文档、保存失败、配置提交失败、取消、暂存恢复失败`

剩余问题：文件系统最终暂存清理失败时逻辑删除已完成，暂存内容保留并警告；需总控故障注入

## T21 · 已实现待总控验收 · 多文件源码转换后诊断位置通常不可导航

文件：src/compiler/DSLCompilerInput.cpp；src/compiler/DSLCompilerAsync.cpp；tests/dsl_compiler_semantics_test.cpp

行为：每行携带 canonical 文件/原始行/列可信度，随旧语法转换、包装、VAR 段搬移、辅助脚本合并流转；合成行不伪造来源；转换行保留行定位但撤回精确列；重复文本不再用子串猜来源；VAR_CONSTANT 分段合并保留只读语义；旧语法单行调用保留同一行参数及结束括号；源与映射原子落盘；映射绑定规范化输入 SHA256，不匹配时拒绝映射

兼容性：诊断来源更精确；不能可靠映射的生成行仍只显示编译输入位置

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R DSLCompilerSemanticsTest --output-on-failure`
- `总控覆盖重复文本、多辅助脚本、VAR_CONSTANT、生成声明和旧语法精确行/列`

剩余问题：新增无编译器进程的组装映射用例未执行；完整异步诊断代次/导航验收由总控完成

## T22 · 已实现待总控验收 · 下载校验的文件与实际发送的字节不是同一快照

文件：src/common/ArtifactSnapshot.h；src/communication/ControllerDeviceBackendDownload.cpp；src/compiler/DSLCompilerArtifacts.cpp；tests/controller_device_backend_test.cpp

行为：下载收集 payload/profile/points/manifest/清单产物不可变快照，每文件 16 MiB/整组 64 MiB；摘要、Profile 解析和实际发送消费同一内存字节；读错、变长和超限先失败；编译 Profile 发布不再验证后重读源文件，发布已验证快照

兼容性：超限产物现在显式拒绝；无 Profile compileOnly 保留

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R ControllerDeviceBackendTest --output-on-failure`
- `总控在验证后替换文件并核对 transport 摘要，注入短读/读错/超限`

剩余问题：新增快照替换和超限回归未执行；传输级零写入/摘要一致需总控专项检查

## T23 · 已实现待总控验收 · Profile 结构校验、发布校验和两套下载执行器的能力不一致

文件：src/communication/DownloadProfile.h；src/communication/DownloadProfile.cpp；src/communication/CMakeLists.txt；src/compiler/CMakeLists.txt；src/compiler/DSLCompilerArtifacts.cpp；src/designer/RunController.cpp；src/communication/ControllerBridge.cpp；src/communication/ControllerDeviceBackendDownload.cpp；tests/controller_device_backend_test.cpp

行为：QtCore download_profile_contract 共享结构解析与 Executor 能力校验；编译发布/运行预检/正式后端拒绝 Controller 不支持的 coils/no-response/target-select mode；Bridge 保留 writeCoils 与无需响应广播写；共享解析拒绝小数 ID、非对象 params、非布尔 needResponse

兼容性：读取仍125/寄存器写123；Controller 为1..63，Bridge 通用范围0..247但广播仅无响应写

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R "ControllerDeviceBackendTest|CommunicationRoutingTest|ProjectSaveCloseTest" --output-on-failure`

剩余问题：后端仍有 payload/设备上下文专项计划检查；总控验收 Bridge 广播与寻址模式矩阵

## T24 · 已实现待总控验收 · 同步/异步数据库访问重复维护且暴露不同的查询契约

文件：src/common/RuntimeHistoryTypes.h；src/core/HistoryQuery.h；src/core/HistoryQuery.cpp；src/core/CMakeLists.txt；src/core/AsyncDatabaseWorker.cpp；src/core/DataManager.cpp；src/monitor/AsyncHistoryStoreAdapter.cpp

行为：共享 HistoryQuery 实现普通/最近 keyset 页、count、UTC、maxId 和错误边界；DataManager/AsyncWorker 委托同一实现；页大小1..10000、最近条数必须正数；空页保留快照游标；连接仍在各自原线程；异步库旧同步列表误用明确 warning，状态式 API 返回错误；请求查询异常转 HISTORY_REQUEST_EXCEPTION 并完成；完成回调异常隔离

兼容性：连接和所有权分别保持原线程；SQL 参数继续绑定

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R "AsyncDatabaseWorkerTest|DataManagerTest|MonitorHistoryServiceTest" --output-on-failure`
- `总控执行两个 adapter 同一分页/计数/UTC/maxId 查询矩阵`

剩余问题：T25 使用同一查询实现；总控验收两 adapter 查询矩阵和连接所有权，不共享连接跨线程

## T25 · 已实现待总控验收 · 历史导出长期占用唯一数据库 worker，阻止实时写入

文件：src/monitor/ReadOnlyHistorySnapshot.h；src/monitor/ReadOnlyHistorySnapshot.cpp；src/monitor/MonitorWidget.cpp；src/monitor/MonitorExportHelper.h；src/monitor/MonitorExportHelper.cpp；src/monitor/CMakeLists.txt；src/core/HistoryQuery.h；src/core/HistoryQuery.cpp；tests/monitor_history_export_integration_test.cpp；tests/monitor_export_test.cpp

行为：专用导出线程创建、使用、销毁自己的 read-only SQLite 连接；既有提交屏障后启动；最多3秒启动交接固定 WAL read transaction，然后释放写线程；每页/计数不占用写线程；WAL 事务固定数据视图，cursor maxId 不纳入屏障后的新记录，并发清理不删除快照中的可见行；应用同时最多一个数据库导出；每页取消检查与最终 QSaveFile commit 取消检查，原目标文件保持原子语义；异常完成不会穿透导出线程；连接/事务 RAII 清理

兼容性：并发数据库导出要求 WAL；非 WAL 明确拒绝，内存来源导出仍保留原接口

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R "MonitorHistoryExportIntegrationTest|MonitorExportTest|AsyncDatabaseWorkerTest" --output-on-failure`
- `总控持续写入+慢大导出，验证队列有界、提交持续、快照/清理/取消/退出`

剩余问题：新增快照插入/清理和提交取消回归未执行；启动交接之外的真实持续吞吐、退出生命周期仍需总控专项验收

## T26 · 已实现待总控验收 · Classic OPC 定时器逐点同步轮询，超时成本随点数累积

文件：src/communication/ClassicOpcServer.h；src/communication/ClassicOpcServer.cpp；src/communication/ClassicOpcPollWorker.h；src/communication/ClassicOpcPollWorker.cpp；src/communication/ModbusInterface.cpp；src/communication/CMakeLists.txt；tests/classic_poll_plan_test.cpp；tests/CMakeLists.txt

行为：独立 I/O actor 持有 Modbus QObject/连接，GUI timer 仅提交不可变请求和订阅结果；单在途轮询；同装置同区连续/重叠地址合并，无缺口跨读；125 寄存器/2000 位及更小配置上限；每轮总 deadline 包含连接等待；10ms 取消检查；停止/映射/配置代次阻断迟到更新；使用进程 RTU owner 协调端口；开失败、停止、重配置和析构释放；失败返回 Bad 空值

兼容性：start 返回配置有效的异步会话接纳结果；实际 Modbus 连接通过 statusSnapshot.online/modbusConnected 和 errorOccurred 更新；不再在 GUI 同步等待开串口

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R ClassicPollPlanTest --output-on-failure`
- `总控用 mock 覆盖 GUI 心跳、超时/取消/重入、端口冲突及停启迟到回调`

剩余问题：actor/串口测试未执行；析构等待 I/O 返回，连接取消和驱动打开延迟需总控验收；没有打开设备

## C04 · 资料不足 · 实现标量运行时表达式、拷贝和成员读写

文件：review_execution/compiler-pending-contract-boundaries.md；review_execution/function-contract-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py

行为：核对既有 AST 与拒绝边界；未知目标不生成伪运算、分支/循环、重复组或数组元素写回；旧调用顺序/成员引用仅保留于离线证据，不将地址编码为数值常量

兼容性：明确拒绝保留；本项生产 lowering/序列化仍未实现

总控检查（本对话未执行）：
- `总控确认缺契约子集的错误路径及旧产物清理`
- `契约明确后再执行本项正向语义与目标编码验收`

剩余问题：相关 LH 运算与写回操作数契约

## C05 · 资料不足 · 实现 IF/CASE/循环与跳转的正确代码生成

文件：review_execution/compiler-pending-contract-boundaries.md；review_execution/function-contract-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py

行为：核对既有 AST 与拒绝边界；未知目标不生成伪运算、分支/循环、重复组或数组元素写回；旧调用顺序/成员引用仅保留于离线证据，不将地址编码为数值常量

兼容性：明确拒绝保留；本项生产 lowering/序列化仍未实现

总控检查（本对话未执行）：
- `总控确认缺契约子集的错误路径及旧产物清理`
- `契约明确后再执行本项正向语义与目标编码验收`

剩余问题：分支/循环/跳转的 LH 目标契约

## FB12 · 资料不足 · 恢复并核实 TaskLock 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB12-TaskLock.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_task.py；resources/snippets/default_snippets.json

行为：本轮归档取证：1 处候选源调用，0 条跨实例字段记录，0 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB13 · 资料不足 · 恢复并核实 TaskUnlock 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB13-TaskUnlock.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_task.py；resources/snippets/default_snippets.json

行为：本轮归档取证：1 处候选源调用，0 条跨实例字段记录，0 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB17 · 资料不足 · 恢复并核实 TaskEnd 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB17-TaskEnd.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_task.py；resources/snippets/default_snippets.json

行为：本轮归档取证：7 处候选源调用，0 条跨实例字段记录，0 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## T27 · 已实现待总控验收 · 多个界面复制相同样式和显示辅助逻辑

文件：src/designer/ui/ThemeManager.h；src/designer/ui/ThemeManager.cpp；src/designer/SettingsDialog.cpp；src/designer/ui/InspectorPanel.cpp；src/designer/DeviceWorkspaceWidget.cpp

行为：QGroupBox 公共边框、标题、padding 和颜色规则由 ThemeManager 一处维护；设置/属性使用 muted 外观，设备页保留 bold 局部覆盖

兼容性：不合并 OPC 后端特有节点路径或不等价样式

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R DesignerP1RobustnessTest --output-on-failure`
- `总控截图回归设置/属性/设备分组`

剩余问题：本轮只提取已证实相同的 QGroupBox 规则，其它相似显示逻辑不强行合并

## T28 · 已实现待总控验收 · 旧功能块生成器仍能直接覆盖当前定义目录

文件：third_party/custom_dsp_language/compile/generate_all_blocks.py

行为：默认输出 legacy_exports 隔离目录；输出必须避开活跃 definitions 树及祖先；非空输出拒绝覆写；清单写同一导出目录；保留旧工具作为证据入口，禁止其输出被视为现行契约

兼容性：旧命令不会再覆写生产定义；兼容接口和 schema 未删除

总控检查（本对话未执行）：
- `python generate_all_blocks.py --output <temporary-new-directory>`
- `检查活跃 definitions 内容哈希不变`

剩余问题：其它废弃接口仍需用途审计

## T32 · 已实现待总控验收 · 同一秒导出的诊断快照使用相同文件名，会覆盖前一次快照

文件：src/diagnostics/DiagnosticSnapshotService.cpp；tests/diagnostic_snapshot_test.cpp

行为：UTC 毫秒时间加 UUID 命名，连续/多实例导出不会使用同名文件；保持 QSaveFile 原子提交

兼容性：文件名增加毫秒及操作标识；generatedAt 仍 UTC

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R DiagnosticSnapshotTest --output-on-failure`

剩余问题：总控增加并发和冻结时间测试

## C08 · 已实现待总控验收 · 冻结 STRING/TIME/日期/POINTER/LREAL/VAR_CONSTANT 的真实支持范围

文件：third_party/custom_dsp_language/compile/src/lh_compiler/frontend/ast_nodes.py；third_party/custom_dsp_language/compile/src/lh_compiler/frontend/ast_builder.py；third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py；third_party/custom_dsp_language/compile/tests/unit/test_language_boundaries.py；review_execution/target-contract-and-language-boundaries-v1.md

行为：VAR_CONSTANT 保留 read_only，要求初值并拒绝写入/常量 FB 实例；LREAL 在声明/参数/常量构建/字面量处明确拒绝，不再按 float32 缩窄；冻结 STRING/TIME/日期/POINTER/ARRAY/LREAL 支持矩阵与各自 ABI 前置

兼容性：语法可解析不等同产物支持；常量初始化实际写回仍受 T03 目标契约约束

总控检查（本对话未执行）：
- `python -m pytest third_party/custom_dsp_language/compile/tests/unit/test_language_boundaries.py`

剩余问题：新类型 ABI 和初始化写回不在 C08 里伪造；测试未执行

## FB01 · 资料不足 · 恢复并核实 KeyScan 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB01-KeyScan.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_display.py；resources/snippets/default_snippets.json

行为：本轮归档取证：1 处候选源调用，2 条跨实例字段记录，2 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB02 · 资料不足 · 恢复并核实 SCIDispTrans 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB02-SCIDispTrans.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_display.py；resources/snippets/default_snippets.json

行为：本轮归档取证：35 处候选源调用，0 条跨实例字段记录，0 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB07 · 资料不足 · 恢复并核实 FilterBW 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB07-FilterBW.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_filter.py；resources/snippets/default_snippets.json

行为：本轮归档取证：24 处候选源调用，96 条跨实例字段记录，24 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB08 · 资料不足 · 恢复并核实 Task 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB08-Task.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_task.py；resources/snippets/default_snippets.json

行为：本轮归档取证：7 处候选源调用，14 条跨实例字段记录，7 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB09 · 资料不足 · 恢复并核实 TaskPeriodic 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB09-TaskPeriodic.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_task.py；resources/snippets/default_snippets.json

行为：本轮归档取证：7 处候选源调用，14 条跨实例字段记录，7 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB10 · 资料不足 · 恢复并核实 TaskWake 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB10-TaskWake.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_task.py；resources/snippets/default_snippets.json

行为：本轮归档取证：0 处候选源调用，0 条跨实例字段记录，0 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB11 · 资料不足 · 恢复并核实 TaskDataWake 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB11-TaskDataWake.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_task.py；resources/snippets/default_snippets.json

行为：本轮归档取证：0 处候选源调用，0 条跨实例字段记录，0 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB14 · 资料不足 · 恢复并核实 TaskSemDef 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB14-TaskSemDef.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_task.py；resources/snippets/default_snippets.json

行为：本轮归档取证：0 处候选源调用，0 条跨实例字段记录，0 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB15 · 资料不足 · 恢复并核实 TaskSemWait 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB15-TaskSemWait.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_task.py；resources/snippets/default_snippets.json

行为：本轮归档取证：0 处候选源调用，0 条跨实例字段记录，0 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB16 · 资料不足 · 恢复并核实 TaskSemPost 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB16-TaskSemPost.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_task.py；resources/snippets/default_snippets.json

行为：本轮归档取证：0 处候选源调用，0 条跨实例字段记录，0 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB20 · 资料不足 · 恢复并核实 TwoPosition 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB20-TwoPosition.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_tso.py；resources/snippets/default_snippets.json

行为：本轮归档取证：0 处候选源调用，0 条跨实例字段记录，0 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## T29 · 已实现待总控验收 · PUBLIC 暴露整个 src，模块边界主要靠约定

文件：CMakeLists.txt；src/core/CMakeLists.txt；src/compiler/CMakeLists.txt；src/communication/CMakeLists.txt；src/designer/CMakeLists.txt；src/monitor/CMakeLists.txt；src/*/*.h；tests/CMakeLists.txt；tests/check_module_boundaries.py

行为：src 根只作为模块/主程序/测试 PRIVATE 包含目录；公共头文件显式相对包含跨模块类型；移除 designer 额外 PUBLIC monitor 路径；通信安装补齐公共 common 头文件；静态依赖检查禁止 common/core/compiler/communication 向 UI 反向依赖并检查公开根目录

兼容性：直接依赖模块的消费者应使用其公开头名；历史根命名空间包含仅在显式 PRIVATE src 环境兼容

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R ModuleBoundaryCheck --output-on-failure`
- `总控检查全部构建与通信头文件安装消费`

剩余问题：静态检查未运行；模块目录中内部头文件仍需后续细化公开白名单

## C07 · 资料不足 · 补齐数组布局、下标与元素读写

文件：review_execution/compiler-pending-contract-boundaries.md；review_execution/function-contract-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py

行为：核对既有 AST 与拒绝边界；未知目标不生成伪运算、分支/循环、重复组或数组元素写回；旧调用顺序/成员引用仅保留于离线证据，不将地址编码为数值常量

兼容性：明确拒绝保留；本项生产 lowering/序列化仍未实现

总控检查（本对话未执行）：
- `总控确认缺契约子集的错误路径及旧产物清理`
- `契约明确后再执行本项正向语义与目标编码验收`

剩余问题：相关数据宽度/寻址模式的 LH 目标契约

## FB18 · 资料不足 · 恢复并核实 TSO 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB18-TSO.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_tso.py；resources/snippets/default_snippets.json

行为：本轮归档取证：0 处候选源调用，0 条跨实例字段记录，0 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB19 · 资料不足 · 恢复并核实 TSOAutoTune 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB19-TSOAutoTune.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_tso.py；resources/snippets/default_snippets.json

行为：本轮归档取证：5 处候选源调用，185 条跨实例字段记录，6 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## T30 · 已实现待总控验收 · 运行时会话与采集服务打包在 UI 模块并依赖全局实例

文件：src/runtime/CMakeLists.txt；src/runtime/RuntimeSessionService.h；src/runtime/RuntimeSessionService.cpp；src/runtime/RuntimeSessionTypes.h；src/runtime/ParameterController.h；src/runtime/ParameterController.cpp；src/runtime/RunController.h；src/runtime/RunController.cpp；src/designer/ParameterController.h；src/designer/RunController.h；src/designer/RuntimeSessionController.h；src/designer/RuntimeSessionController.cpp；src/designer/RuntimeSessionDownload.cpp；src/designer/RuntimeSessionDebug.cpp；src/designer/RuntimeSessionOpc.cpp；src/designer/RuntimeMonitorAdapter.h；src/designer/RuntimeMonitorAdapter.cpp；src/designer/MainWindow.cpp；CMakeLists.txt；src/designer/CMakeLists.txt；tests/runtime_session_service_test.cpp；tests/parameter_controller_test.cpp；tests/CMakeLists.txt

行为：独立 runtime_session_service 无 Widgets/Charts，拥有会话/下载状态、取消代次及参数与下载前检入口；backend/history/monitor 显式注入；Core 服务没有 singleton lookup；监控服务 facade 由 UI adapter 兼容；MainWindow 组合根注入 RuntimeSessionService；GUI 控制器订阅状态并委托监控/取消/前检；ParameterController/RunController 源移出 designer target，旧头文件保留转发；参数和服务测试采用 QCoreApplication

兼容性：保留 GUI RuntimeSessionController 的项目/构建/OPC 编排及旧构造入口适配单例；没有一次重写所有监控 UI

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R "RuntimeSessionServiceTest|ParameterControllerTest|RuntimeSessionControllerTest|ModuleBoundaryCheck" --output-on-failure`

剩余问题：两服务状态/取消/参数隔离、后端销毁、下载前检用例未运行；总控检查现有 GUI/OPC/监控流程无回退

## T31 · 已实现待总控验收 · MainWindow 仍同时承接大量工程、运行、参数和诊断编排

文件：src/core/ProjectCommandCoordinator.h；src/core/ProjectCommandCoordinator.cpp；src/core/CMakeLists.txt；src/designer/MainWindow.h；src/designer/MainWindow.cpp；src/designer/CMakeLists.txt；tests/project_command_coordinator_test.cpp；tests/CMakeLists.txt

行为：保存当前/全部工程命令移入独立仅 QtCore 链接的协调器；文档保存失败阻断工程保存；工程保存失败返回明确结果；嵌套事件循环重入同一协调器返回 Busy；MainWindow 仅注入文档/工程操作回调，无协调器 QWidget 所有权

兼容性：按任务要求只迁移保存工作流；其他工程/运行命令保持现有路径

总控检查（本对话未执行）：
- `ctest --test-dir build_current_mingw -R ProjectCommandCoordinatorTest --output-on-failure`

剩余问题：仅源码和用例已实现；总控执行 QtCore 无界面顺序/失败/重入验收

## FB03 · 资料不足 · 恢复并核实 SCIDispInit 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB03-SCIDispInit.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_display.py；resources/snippets/default_snippets.json

行为：本轮归档取证：1 处候选源调用，6 条跨实例字段记录，1 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；可完成契约取证表，完整启用仍缺波特率枚举、接收终止规则、缓冲区与错误时序说明。；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB04 · 资料不足 · 恢复并核实 M600TextDisp 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB04-M600TextDisp.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_display.py；resources/snippets/default_snippets.json

行为：本轮归档取证：26 处候选源调用，25 条跨实例字段记录，25 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；需有序重复参数和字符串/整数/浮点格式契约；指南明确转引 HMI 指令集与 LM 工程机械软件专题，现有材料不足以声称全协议完整。；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB05 · 资料不足 · 恢复并核实 M600ProgressBar 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB05-M600ProgressBar.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_display.py；resources/snippets/default_snippets.json

行为：本轮归档取证：1 处候选源调用，1 条跨实例字段记录，1 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；可取证和实现明确子集；ModeAdd 在表头写为 Const，而程序用成员地址，限幅/数据类型/项数/序列化规则须核对。；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB06 · 资料不足 · 恢复并核实 EXCACycDisp 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB06-EXCACycDisp.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_display.py；resources/snippets/default_snippets.json

行为：本轮归档取证：2 处候选源调用，63 条跨实例字段记录，2 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；只有调用线索，缺完整模式枚举、轮显推进与输出/越界/错误语义；不能据 2 个样例补齐任意组合。；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成

## FB21 · 资料不足 · 恢复并核实 RelayCtrl2 的 LH 契约，补齐已证明的能力

文件：function-contract-evidence/FB21-RelayCtrl2.json；review_execution/collect-lm-function-evidence.ps1；review_execution/collect-manual-evidence.py；review_execution/manual-evidence；third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_tso.py；resources/snippets/default_snippets.json

行为：本轮归档取证：1 处候选源调用，7 条跨实例字段记录，1 条候选代码关联，带条目/行/SHA256；手册页、字段/初值/范围候选原文保存；别名仅为候选，地址/类型/方向/编码未认证为 LH；当前仍 incomplete；UI 插入及生成控制代码继续拒绝，错误原因具体说明未核实的版本契约

兼容性：仅建立候选证据，不复制 LM 指令号/枚举/绝对地址，也不宣称硬件支持

总控检查（本对话未执行）：
- `总控独立复核本项源/list/typ/code 与手册物理页`
- `目标契约明确后分别建立编译侧用例与硬件验收`

剩余问题：用于生产编码的 LH 目标指令/操作数/数值版本支持证据；不能证明 RelayCtrl2 与 RelayCtrl/TwoPosition 仅是别名；必须核实变体差异、字段布局、状态与使能/故障行为。；本轮没有将候选字段表序列化为已认证 LH 功能块；完整可下载实现未完成
