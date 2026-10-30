# LH 对齐 LM：软件功能与使用约定

2026-10-24。本轮以 LM 手册为功能需求证据，沿用 LH 语法、项目格式及运行默认设置。不要求 LM 旧项目迁移，不新增 PC 离线执行引擎；不执行手册中的安装、注册、烧录和控制器操作步骤。

## 目录、表达式和编译结果

模板存在、构造参数可编译、目标执行通过是三个层次。空参数表仅对确认的无参数操作 Nop 保留可调用；其他空定义展示缺少契约，禁止插入和直接调用。之前能生成空指令不构成该功能已实现。

`lmc.py --capabilities-json` 与 `python -m lh_compiler.cli.commands list-blocks --json` 输出同一状态模型：definitionValid、definitionErrors、compilerHandles、targetExecutionVerified。compilerHandles 仅指构造参数契约，不表示支持运行时输入/输出引用；本轮 targetExecutionVerified 均为 false。项目覆盖或别名不可提升内置能力。DrvT2PeriodIn/DrvTimeIn 的 Mask=65535 超出声明 INT 范围会明确报告；类型、默认值及 Scale 语义等待目标契约确认，不猜测编码。

优先级从高到低：括号/访问、一元 NOT/正负号、乘除/MOD、加减、比较、AND、XOR、OR；同层二元左结合。混合优先级未用括号明确时提示复核旧程序，源文件不会改写。BOOL 为逻辑运算，整数为位运算，禁止混用。整数 NOT 按目标位宽计算补码，无上下文用有符号 32 位。整数二元常量中间结果限定于有符号/无符号 32 位可表示范围，最终检查目标类型范围。整数除法向零截断，MOD 余数与被除数同号。除零、非有限数、错误类型和溢出拒绝。

常量赋值产物保留 compileOnly 和下载保护，界面区分编译成功与可运行。缺少目标指令契约的引用、控制流、数组等继续明确拒绝。新项目为可编译的最小 System 示例；运行需要实际配置和验证目标。

## 参数和本地快照

编辑及同步/异步下发检查权限、类型、有限数、类型范围和配置上下限；反向或非法上下限拒绝，错误编辑保留上次合法值并显示原因。整批预校验通过后才写入；一个非法值阻止整批。设备接收阶段仍可能部分失败，沿用逐点错误与异步回读确认。

监控菜单可导出/恢复本地项目参数快照，格式 lh-project-parameters、版本 1，条目为 id/dataType/字符串 value。仅导出可编辑参数的项目 currentValue（空时取 defaultValue）；不含尚未应用的调参草稿或设备内存。

恢复允许匹配的参数子集，先校验版本、ID 唯一性、类型、权限和范围，再预览，确认后一次更新本地项目并标记未保存、未设备确认。任意条目失败整个项目原值不变。保存项目和实际下发另行执行；不是控制器掉电数据备份。

## 调试、监控和产物

运行菜单可设置/清除两个控制器组件位置断点，0 使用已有协议清除相应断点。位置不是源码行号；无可靠映射时禁用运行到光标，不把源码行直接发送。

连续单步默认 500ms，可选 50–60000ms，每次完成后等待间隔再发送。停止、断线、项目/后端切换、失败及对象销毁终止后续调度。复用现有协议，不新增寄存器；断点和连续执行的真实目标效果延期验证。

字段浏览、调参、曲线及历史使用已配置 RuntimePoint；未知字段地址和位置映射留空。多曲线、图片与序列导出沿用现有 LH 流程，不声明实机 30ms 采样或现场 OPC/DCOM 已验收。

参数 .data 为 LH 自有清单；通信 .xml 根为 lhCommunicationConfig，.tag/.act/.tx/.rx/.rt/.eng/.comm/.msg 为 LH 工程视图。不声明为 LM/Matrikon/LabVIEW 原始兼容格式。.code/.list/.typ/.rep 同样区分本地产物检查与目标契约验证。

## 验证环境和证据

运行 tools/prepare_qt_test_runtime.ps1，只在工作区构建输出复制同一 Qt 安装的 DLL、插件和 qt.conf。GUI 验收使用 QT_QPA_PLATFORM=windows。Qt 5.15.2 的 Windows offscreen/minimal 在独立 QMessageBox 可于 qt_getWindowsSystemMenu 崩溃；保留探针堆栈，不跳过 GUI 测试、不改全局 Qt。原生插件通过不证明 offscreen 已修复。

验证包含 Python 全套、全量构建、软件专项、完整 CTest、安装布局及隔离安装 smoke。物理控制器、磁盘故障和长期稳定性需独立证据。详细结果见 build_dev_artifacts/review_execution/lm-parity-repair-2026-10-24。
