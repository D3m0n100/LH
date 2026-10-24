# LH 相对 LM 的功能审核报告

审核日期：2026-10-24。结论：**LH 尚未达到两本手册所描述的 LM 全部功能，不能按“LM 完整替代平台”验收。**

LH 已有工程/编辑器、构建产物管理、Modbus RTU 会话、参数应用与回读、OPC 适配、多曲线/历史/导出等上位机能力。主要缺口位于真正可执行的控制语言、实时任务、参数语义/保持、完整功能块库、现场总线、显示屏和应用程序闭环。界面入口、注册名称、编译返回成功、生成同名后缀文件，分别只能证明对应软件层事实。

本次为**功能级只读审核**，新增本目录取证脚本、矩阵、探针和报告，重新构建/运行相关测试；没有修改 LH 业务源码，没有连接或写入控制器，没有提交或推送。工作区已有修改保留。编译器纳入本轮审核范围，但没有进行编译器实现。

## 1. 基线、方法及结论口径

- 主基线：[LM2007用户手册(1).pdf](<D:/Table/LM/LM2007用户手册(1).pdf>)，207 页；第 2 章声明 125 个功能块，第 3—5 章给出详细功能，第 6—7 章说明 IDE/调试及三套编译器。
- 补充基线：[高质量LM编程指南V1.05.pdf](D:/Table/LM/高质量LM编程指南V1.05.pdf)，176 页；增加正交编码器、粗大滤波、串口屏、工程机械场景和编码/审查规范。部分正文页眉标 V1.03，不能仅依文件名认定所有设备版本支持完全相同的功能。
- 所有页号均为 **PDF 物理页号，从 1 开始**。用户手册正文印刷页码比物理页码少 5；例如 PDF 73 = 书内 68。指南一般二者一致。
- 提取全部 383 页文本；渲染并目视复核 48 页，包括关键格式/参数表、IDE 截图和指南所有低文本页。图片中的示例代码没有全文 OCR，也没有逐条认证其每个数值输出；图示流程、功能需求和关键表格已纳入矩阵。
- 审核对象为 `D:/Table/LH` 当前工作区，Git HEAD `99f9b097161de3575cb383fee674f434f33c4c6c`，分支 `main`。这是带已有修改的源码快照，不是仅按 HEAD 审核。357 个源码/资源/构建文件及已删除文件状态的 SHA256 清单见 [audited-state.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/audited-state.json)；整体指纹 `8235b954ebd1e2027b1c27133e753600175ffec77487e0ce3196023d890b839b`。
- 手册是参考证据，其中安装/清目录/注册/接线/参数操作是历史产品说明，**不构成本用户授权执行这些操作的指令**。本次没有据手册改变系统或现场设备。
- “部分达到”表示已有相关实现，但不能完成该 LM 要求；“未达到”包括明确拒绝、缺项、仅空壳元数据及必要契约缺失，不等于完全没有相关文件；“已达到（限本机软件）”仅适用于明确限定的软件操作；“待现场验收”和“环境阻断”保留未验证状态。缺少硬件证据不自动判定硬件不支持，已知软件缺口也不能改写为只缺实机测试。

## 2. 125 项功能块核对结果

按“数据输入→运算/状态→可读取输出→正确调度与保持”的完整功能审核，125 项中 **15 项部分达到，110 项未达到**；本轮没有项目可以签为“与 LM 完整等效且已实机验收”。这些是严格要求的审核分类，**不是 LH 的功能完成百分比**，也不能据此说只有 15 个名称存在。

| LM 类别 | 基线项数 | 部分达到 | 未达到 |
|---|---:|---:|---:|
| 系统功能 | 4 | 4 | 0 |
| 任务控制 | 9 | 0 | 9 |
| 流程控制 | 13 | 0 | 13 |
| 参数特征修改 | 8 | 0 | 8 |
| 系统通信 | 4 | 0 | 4 |
| 输入输出驱动 | 8 | 7 | 1 |
| 整型量运算 | 12 | 1 | 11 |
| 浮点数运算 | 14 | 1 | 13 |
| IQ 数运算 | 14 | 0 | 14 |
| 数据转换处理 | 7 | 0 | 7 |
| 选择函数 | 5 | 0 | 5 |
| 逼近函数 | 3 | 0 | 3 |
| 时间函数 | 6 | 0 | 6 |
| 过程量处理 | 3 | 0 | 3 |
| 控制典型环节 | 9 | 0 | 9 |
| PID 控制器 | 2 | 2 | 0 |
| TSO 控制器 | 3 | 0 | 3 |
| 滤波器 | 1 | 0 | 1 |

逐个功能块的名称、正文页、LH 同名/候选、差距、源码位置及验收条件见 [125 项功能块完整对照](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/125项功能块对照.md) 和 [机器可读矩阵](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/block-parity-matrix.json)。FloatUnderWatch/FloatBelowWatch、IQUnderWatch/IQBelowWatch 是手册总览与正文的拼写差异，各按一个功能计算；没有重复凑数。候选名称映射仅用于审核检索，尚未认证为等价别名。

## 3. 关键发现及可复核证据

### F01｜阻断：功能块之间的动态数据连接尚未形成

手册 PDF 8—13 要求整数/浮点/IQ 数据项和 `功能块.参数` 引用。当前 LH 可以编码立即数，但 `x := y`、`x := y + 1`、成员读写、功能块输入引用、输出绑定都被后端拒绝。140 个内置注册项的运行时字段表均为空；`.list/.typ` 有构造参数信息，但不会为没有证明的字段编造地址。因此即使某个 PID 常量调用生成了 `.code`，也不能将真实输入连到 PID 再连到输出。

证据：[third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py:470)、[third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py:598)、[third_party/custom_dsp_language/compile/src/lh_compiler/backend/emitter.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/backend/emitter.py:206)；独立探针 `runtime_variable_copy / runtime_expression / member_reference / block_input_reference / block_output_reference`，详见 [compiler-probes.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/compiler-probes.json)。

### F02｜阻断：任务调度、控制流、初始化与 IQ 缺失

手册 PDF 29—46 要求一次初始化、周期/事件任务、抢占优先级、锁/信号量和嵌套控制流。LH 的 Task、TaskEnd、唤醒/锁/信号量等 10 个候选全部 `incomplete`。结构化 IF/CASE/FOR/WHILE/REPEAT 虽能进 AST，后端明确报“不支持控制流”；ARRAY、IQ 也不能生成。变量声明初始化被拒绝。标量常量写回可以输出，但带 `LH-EXECUTION-UNCONFIRMED` 标记，并被禁止下载/运行。

证据：[third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_task.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_task.py:26)、[third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py:456)、[third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/backend/codegen.py:360)、[src/common/CompileArtifactPolicy.h](D:/Table/LH/src/common/CompileArtifactPolicy.h:14)。这是必要边界保护，目前不能当作已实现 LM 语义。旧 LM 的指令号、绝对地址、Q 值和内存布局不能直接复制为当前 LH 固件契约。

### F03｜重大：140 个注册项不能作为库覆盖率

实时加载当前注册表得到：140 项中 119 项标 `supported`，21 项标 `incomplete`；其中 **105 项 supported 的参数表为空，只有 14 项有构造参数，0 项有显式运行时字段**。需要参数的运算、数组构建、归一化、CAN、保养等块，只能空调用或被拒绝，无法按手册装配。对 Nop 等合法无数据项操作，空参数本身不是缺陷，仍需确认无实例操作形式与实际执行。

默认/空调用探测得到 117 项可输出、23 项拒绝；这 117 项只表示本地序列化成功，绝非 117 项功能通过。23 项包括 21 个 incomplete 以及 2 个默认参数自相冲突的 IO 块。

证据：[third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/registry.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/registry.py:54)、[current-registry.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/current-registry.json)、[registry-summary.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/registry-summary.json)、[registry-default-emission-probes.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/registry-default-emission-probes.json)。需要参数的典型反例：`IntBuild(Paramts := 4)`、`Add(Input1 := 1, Input2 := 2)`、`CommCANInit(Baud := 250)` 均失败，而对应空调用可以成功。

### F04｜重大且已复现：IO 参数与 LM 不符

手册 PDF 72—73 规定 CAP 的 Scale 在 0—7，表示分频 `2^Scale`。LH 的 `DrvT2PeriodIn.Scale` 默认却是 1000，设置 `Mask := 255` 后 Scale=1000 仍编译成功。另一方面 `DrvT2PeriodIn` 与 `DrvTimeIn` 的默认 Mask=65535，被自身 INT 的 -32768—32767 校验拒绝，默认调用失败；`DrvDI(Mask := 16#FFFF)` 也失败。

模拟输出只有通用 `DrvAO(channel,value)` 候选，不能证明覆盖 LM T1/T2 PWM 的 PWMIO 结构指针、通道屏蔽、正反作用和外设互斥。QEP 不在当前内置库中。

证据：[third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_io.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_io.py:135)；探针 `period_input_defaults / period_input_scale_1000 / digital_input_mask_ffff` 以及两个默认调用失败记录。验收必须覆盖 16 位位图的 0x7FFF/0x8000/0xFFFF、Scale=0/7/越界、通道与 T2/T4 资源冲突，不能只把默认值改到能通过。

### F05｜重大：参数语义和永久/掉电保持没有闭环

手册 PDF 11—13、47—53 定义工程量序号、内部归一化、外部写入语义、保存/永久保存、掉电保护、任务唤醒和语义传送。LH 有项目参数/单位/上下限/在线编辑及回读状态，但相关 LM 参数块大多缺失，仅 `ParamtEngBuild` 为空定义。本机 JSON/数据库持久化、参数回读确认，不能替代控制器的非易失保存、重刷后恢复与任务唤醒。

证据：[src/common/ConfigTypes.h](D:/Table/LH/src/common/ConfigTypes.h:134)、[third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_param.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_param.py:11)、[src/runtime/ParameterController.h](D:/Table/LH/src/runtime/ParameterController.h:41)；详见跨项 L05—L08、E08—E09。此缺口还阻断工作时间累计、保养、标定结果保留等应用。

### F06｜重大：PID、TSO、逼近和过程控制库不完整

手册 PDF 157—158 的 PID 有 21 个运行时参数，包括手自动、正反作用、内/外给定、P/Ti/Td、积分分离、四段非线性、前馈和限幅。LH `PID` 只有 10 个构造参数；`PIDAutoTune` 只有 6 个，不能覆盖完整 PID 加整定状态、保护、使能和结果接收。`PID(EnableNL := 1)` 实际被拒绝，反馈引用也被拒绝。Kp/Ki/Kd 调参窗口只能证明参数工具存在。

TSO、自整定 TSO、两位继电及 FilterBW 明确 incomplete；5 项选择函数、3 项逼近函数、3 项过程量和 9 项典型环节没有完整实现。其影响不仅是少几个控件，而是传感器标定、发动机/泵复合控制等程序不能形成可执行链。

证据：[third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_pid.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_pid.py:19)、[third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_tso.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_tso.py:10)、[third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_filter.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_filter.py:17) 和完整 125 项矩阵；指南 PDF 73—80、100—128 的应用依赖关系已分别列入 G08—G19。

### F07｜重大：CAN 执行链和三套工具链产物仍不等效

手册 PDF 54—67、指南 PDF 36—37 的 CAN 包含装置号/波特率、优先级、收发缓冲区、变化发送与数据组。当前 CommCANInit/CommTrans 无参数，变化发送/接收缓冲区缺项；PC 生产控制器后端正式支持 Modbus RTU，底层另有 CAN/以太网类不能替代这条执行链。

手册 PDF 205—207 的参数编译需要符号到设备地址、命令/地址/数值以及复位/设时/保存等操作；LH `.data` 是 `SET + name + type + value…` 自有文本。通信 `.xml` 根节点为 `lhCommunicationConfig`，`.tag/.act/.tx/.rx/.rt/.eng/.comm/.msg` 为项目模型的导出视图。它们确实被生成并有本机测试，但没有“可由真实 LM/Matrikon/LabVIEW 工具消费”的证明。需要旧工具兼容时必须验证格式；采用新工具链时也必须提供等效闭环。

证据：[third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_comm.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_comm.py:10)、[src/communication/ControllerDeviceBackend.cpp](D:/Table/LH/src/communication/ControllerDeviceBackend.cpp:90)、[src/designer/BuildController.cpp](D:/Table/LH/src/designer/BuildController.cpp:962)、[src/designer/BuildController.cpp](D:/Table/LH/src/designer/BuildController.cpp:1025)；`generatedBuildArtifactsUseLhProjectModel` 本轮通过，但它验证 LH 模型的产物，不能验证第三方导入。

### F08｜重大：调试控制只覆盖了一部分

手册 PDF 198—200 还要求工作/调试模式、运行位置、断点增删/清空、可调周期连续单步、复位/数据保存/永久备份恢复。LH 有暂停/继续/单步/运行到光标和两个断点的底层接口，fake transport 协议测试通过；主界面没有完整的断点管理/连续单步等操作闭环。运行到光标直接发送编辑器行号，没有证明该行号对应控制器“构件执行位置”；多文件合并、跨行调用及自动 System 使两者不能默认等同。

证据：[src/communication/ControllerDebugClient.h](D:/Table/LH/src/communication/ControllerDebugClient.h:90)、[src/designer/MainWindow.cpp](D:/Table/LH/src/designer/MainWindow.cpp:1407)、[src/designer/RuntimeSessionDebug.cpp](D:/Table/LH/src/designer/RuntimeSessionDebug.cpp:28)。现有编译错误源映射是诊断映射，不是运行时指令/断点映射。真实控制器上的模式、状态和位置都还需要验收。

### F09｜重大：指南中的显示屏和完整应用场景尚未实现

KeyScan、SCIDispTrans/Init、M600TextDisp、M600ProgressBar、EXCACycDisp 均 incomplete；显示模块界面入口仅打开并筛选 display 分类，不是可工作的 HMI 编辑/烧录/执行系统。指南 PDF 56—63 的图片制作/烧录使用外部工具，LH 可以提供外部协作工作流而不必内置绘图，但目前没有完整等效流程或实际校验记录。

指南的报警/8 秒屏蔽、16 项保养、自动档位/转速标定、泵发动机复合控制、阀控制、怠速/升速、总工时标定/保持、通信异常恢复和两小时触发，不可用 PC 阈值告警或“EXCA”名称替代。当前缺少完整应用程序与其依赖的任务、实时数据、保存和显示语义，按场景判未达到。

证据：[third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_display.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_display.py:10)、[src/designer/MainWindowWorkspace.cpp](D:/Table/LH/src/designer/MainWindowWorkspace.cpp:376)、[third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_application.py](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/function_blocks/definitions/_application.py:10)；逐场景页码和验收条件见 G01—G19。

### F10｜验收阻断：本轮 Qt 对话框环境异常

本轮重建后 11 个 CTest 目标有 9 个通过，MainWindowIntegrationTest 与 ProjectSaveCloseTest 在 QMessageBox 路径发生 `0xC0000005`。主窗口第一个普通编译用例单独重跑仍失败；配置 Windows 字体目录也未消除。随后编译**不包含任何 LH 业务对象**的独立 QApplication+QMessageBox 小程序，offscreen 与 minimal 下都以同一异常退出。因此应记录为本机 Qt 运行环境共性问题阻断验收，精确根因尚未确定，不能直接判成 LH 业务源码缺陷，也不能忽略失败宣布全绿。

独立证据：[probes/qmessagebox_environment_probe.cpp](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/probes/qmessagebox_environment_probe.cpp)、[qmessagebox-environment.log](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/qmessagebox-environment.log)、[qmessagebox-minimal-environment.log](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/qmessagebox-minimal-environment.log)、[ctest-focused-results.xml](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/ctest-focused-results.xml)、[ctest-editor-results.xml](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/ctest-editor-results.xml)。三个不涉及该失败路径的主窗口业务用例单独通过；这不覆盖完整两套集成测试。

## 4. 已有能力与验证边界

| 能力 | 本轮可确认的范围 | 未完成的等效验收 |
|---|---|---|
| 脚本编辑/保存/补全 | DslScriptEditorSaveTest、DslCompletionEngineTest 通过，支持语法高亮、片段和保护未完成块 | 完整工程关闭/切换因 Qt 对话框环境阻断 |
| 参数应用与回读 | ParameterControllerTest 通过；主窗口参数同步用例单独通过 | 参数保存类别、语义动作、永久恢复与真实设备 |
| 通信和调试客户端 | ControllerDebugProtocolTest、RuntimePointTableTest、OpcServerConfigTest 通过 | 真实控制器、CAN 组态、Matrikon/DCOM、多机/失联行为 |
| 曲线和导出 | MonitorWidgetUiTest、MonitorExportTest 通过；多通道、显隐、历史和多种导出格式已实现 | 指南四变量 30ms 实际采样、丢点/抖动、持续运行；旧 BMP/.log 工具兼容 |
| 编译与生成 | DslCompilerSemanticsTest 通过；独立探针明确区分支持和拒绝 | 实时连接、任务/控制流/IQ 等执行语义；目标固件的完整等效程序 |
| 虚拟后端 | 有点值/故障/延迟/下载模拟 | 不等同逐周期执行完整控制程序的仿真器 |

“可设 1ms 采样周期”和“图上有曲线”不等于 30ms 实际硬件能力；“能建立 OPC 会话”不等于所有设备端点、外部客户及 DCOM 组合已经验收。

## 5. 测试和取证清单

| 验证 | 本轮结果 | 证据 |
|---|---|---|
| 两本原文 | SHA256、页数、逐页文本；48 页渲染复核 | [sources.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/sources.json)、`extracted/`、`rendered/` |
| 当前内置库 | 140 项；119 supported/21 incomplete；14 项有参数，105 项 supported 无参数，运行时字段 0 | [registry-summary.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/registry-summary.json)、[current-registry.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/current-registry.json) |
| 独立手册语义探针 | 30 个；5 个生成、25 个拒绝。通过的包含不符合 LM 范围的 Scale=1000，故不是“5 个功能合格” | [compiler-probes.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/compiler-probes.json)、[compiler-probes.log](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/compiler-probes.log)、`probes/` |
| 全库默认调用探针 | 117 生成、23 拒绝；仅本地编码事实 | [registry-default-emission-probes.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/registry-default-emission-probes.json) |
| Python 编译器单元测试 | 112 个收集，111 通过、1 跳过、0 失败；跳过原因是 Windows 符号链接权限 | [compiler-unit-tests.log](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/compiler-unit-tests.log)、[compiler-unit-results.xml](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/compiler-unit-results.xml) |
| Qt 聚焦构建/CTest | 11 个目标重建成功；9 个目标通过，2 个被上述 Qt 共性问题阻断。这里是目标数，不是 Qt 内部 case 数 | [build-focused-tests.log](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/build-focused-tests.log)、[build-editor-tests.log](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/build-editor-tests.log)、两份 CTest XML |
| 主窗口选定业务用例 | 产物生成、参数回读同步、统一 QAction 一致性 3 个业务用例通过（另有 init/cleanup） | [mainwindow-selected-results.xml](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/mainwindow-selected-results.xml) |
| 完整 GUI 重跑与独立 Qt 对照 | 同样异常；offscreen/minimal 与字体重设均未消除 | [mainwindow-compile-recheck.log](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/mainwindow-compile-recheck.log)、两份独立 Qt 日志 |
| 工作区检查 | `git diff --check` 返回 0；没有修改业务源码或提交 | [audited-state.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/audited-state.json) |

完整结构化结果见 [verification-summary.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/verification-summary.json)。Python 单元测试包含“未实现能力必须拒绝”的边界用例，绿灯不能换算为 LM 功能覆盖。以上没有用 fake/虚拟设备、离屏 Qt 或历史通过记录代替本轮实机验收。

## 6. 后续差距与验收包（本次未实施）

| 顺序 | 必须交付的能力 | 验收出口 |
|---|---|---|
| A01 | 绑定 LH 控制器/显示屏、固件版本与目标契约：ID、布局、地址单位、引用/写回、无实例操作、IQ、事件与任务 | 由真实固件/目标证据确认；不照抄 LM 数字；不支持的版本明确拒绝 |
| A02 | 可执行的数据连接、初始化/保持、表达式、分支循环、数组/可变组和任务调度 | 一个有真实 IO、滤波/算法、输出的多任务程序端到端运行；边界/错误/重启测试通过 |
| A03 | 完整 125 项库和指南增补块；修正 IO 位图/范围/default 冲突 | 每项按手册参数、状态、数值、边界单独 fixture 与实机验收；不能以空调用或注册名签核 |
| A04 | 参数工程量/语义、保存/永久/掉电类别、备份恢复和总工时 | 冷启动、监控复位、下载、新程序及断电后的预期值都正确 |
| A05 | 装配/参数/通信的完整等效工具链，含 CAN/多装置、OPC/LabVIEW 导入或替代方案 | 文件被消费者实际使用，在线读写、质量、地址、更新率和故障恢复通过 |
| A06 | 调试模式、构件执行位置/源码映射、断点和连续单步及系统工具 | 真机按构件停/走/跳转/断点，跨文件和跨行语句正确；有清除/退出动作 |
| A07 | 显示屏/图片外部协作、按键、文本/棒图和异常恢复 | 真实屏幕握手/烧录校验、按键状态、刷新与故障恢复完整通过 |
| A08 | 指南 G10—G19 应用工程，含明确目标和输入轨迹 | 各场景按其流程与参数分别验收；不是强制将特定挖机参数写死到通用平台 |
| A09 | 修复本机 Qt 验证环境并补齐现场性能/长时/多机测试 | 两套完整 GUI 测试重跑；实体桌面、四变量 30ms、DCOM、掉电和长时记录均有原始证据 |
| A10 | LH 适用的编码/设计/审查规范及应用交付表 | 指南六表、版本追踪、命名/风格/控制语句检查、缺陷回归和责任签字齐全 |

A01—A03 是当前替代能力的基础依赖，单独补几个按钮或 snippet 不能解锁后续应用。编码指南中的目录、版式、命名及组织审查条款属于工程过程要求，已在 P01—P08 分开列出，不机械当成 IDE 缺功能，也未据此向其他人员发消息或执行安装/删除操作。

## 7. 交付索引

- 本报告：审核结论、关键证据、验证范围和后续验收包。
- [125项功能块对照.md](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/125项功能块对照.md)：125 项逐项对照；[block-parity-matrix.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/block-parity-matrix.json) 保存各项来源与源码证据。
- [跨项功能与指南对照.md](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/跨项功能与指南对照.md)：63 条语言/IDE/指南应用/过程要求及逐项验收条件；[cross-cutting-requirements.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/cross-cutting-requirements.json) 为机器可读版本。此 63 条与功能块基线有依赖交叉，不能相加为覆盖率分母。
- [sources.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/sources.json)、[evidence-index.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/evidence-index.json)、[audited-state.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/audited-state.json)：来源、代码锚点及审计快照。
- [verification-summary.json](D:/Table/LH/review_execution/lm-parity-audit-2026-10-24/verification-summary.json) 与原始 XML/log/probe 源文件：本轮复核证据。

**审核结论可交付；LM 全功能等效验收未通过。** 后续实施与实机验收需按上述差距逐项完成，现有本机测试和保护性拒绝不会自动转为总体验收。
