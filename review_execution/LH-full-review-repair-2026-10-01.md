# 本轮全量 Review 修复与本地验收台账

审查批次：2026-10-01；本地验收完成：2026-10-02。工作区与 CMake 根目录：D:\Table\LH；主构建目录：build_current_mingw。

57 项中，28 项已实现并通过相应本地验收。另有 28 项编译器目标契约不足、1 项现场验收，均未标为完成。用户已明确只支持 Windows，并需要网络共享目录；原支持范围待定项已关闭。Linux/macOS 不属于当前范围。真实服务/设备/磁盘/物理桌面及实际 NAS/远端共享的现场验证仍保留，本机结果不能代替。

保留执行前已有修改；HEAD 未变化，未提交、推送或修改模型。未知编译器能力继续安全拒绝，未把旧 LM 指令、地址或 ABI 当作已确认 LH 契约。

## 验收证据

- UNC 修复后的最终 OFF 完整构建通过：[构建日志](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/build-unc-full-off.log)。
- UNC 修复后 OFF 全量 CTest **45/45**，46.74 秒（本次配置了真实 SMB 测试环境）：[日志](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/ctest-unc-final-off.log)，[JUnit](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/ctest-unc-final-off.xml)。
- UNC 保护回归 18 passed / 0 failed / 0 skipped；工程打开/编辑保存/删除失败回滚/成功删除/重开回归 3 passed / 0 failed / 0 skipped（均含初始化/清理）：[UNC保护](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/unc-guard-final.txt)、[工程回归](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/unc-project-final.txt)。本机现有 SMB 共享映射到构建目录，未创建共享或修改网络配置。
- 直接 lmc.py 和安装 lmc.exe 使用含中文/空格的 UNC 源路径、产物路径及工作目录，两者成功且 code 与本地一致，源文件保持：[UNC编译冒烟](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/unc-cli-results.json)。
- ON 三个真实发布失败分支通过，Qt totals 5 passed / 0 failed / 0 skipped（含初始化、清理）：[注入日志](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/injection-on.txt)。OFF 属性无效用例实际执行，交付缓存已恢复 OFF。
- 仓库默认 Python **97 passed / 1 skipped**，20.41 秒；符号链接用例因 Windows 权限跳过，硬链接与其他源文件碰撞保护实际执行：[Python日志](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/python-final.log)。
- 实际安装 lmc.exe 与直接 lmc.py 编译 Unicode 源文件，code 一致：[CLI证据](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/cli-installed/results.json)；恢复脚本、pip check 与版本：[环境日志](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/python-bootstrap-smoke.log)。
- 新审查回归 26 passed / 0 failed，会话 72 passed / 0 failed：[审查回归](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/full_review_regression_test-final.txt)，[会话回归](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/runtime_session_controller_test-final.txt)（Qt totals 含初始化、清理）。
- 合成旧库含 100000 条 runtime_data、10000 条 system_logs 及真实索引；约 2.6 秒完成，界面心跳持续；取消、无时区输入与进度异常完整回滚：[数据库回归](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/database_startup_migration_test-rework.txt)。
- 独立采样、后端重绑/销毁/迟到回调、诊断及日志回归通过。原独立探针以当前库重建：[12项探针对照](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/probes-results.json)。
- OFF 安装布局及独立公共接口消费者通过；保留安装目录的 --version、--smoke-test 成功：[安装/冒烟](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/installed-smoke-results.json)。子进程 PATH 仅保留 Windows 系统目录，使用包内 qwindows，版本查询提供隐藏控制台；冒烟前后原库 76 条运行记录保持。最初强制包内未部署的 offscreen 插件时超时，作为验证环境诊断保留，未修改产品代码。本机结果不代表远程 MSVC CI 已执行。
- Light/Dark 共 10 张 offscreen 截图，检查设置、属性、设备、图表及错误提示：[截图目录](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/ui-preview)。内联样式随主题切换并保留原样式，曲线数据保持；图表用软件渲染，错误框仅隐藏渲染真实内容。原生窗口边框、交互及物理桌面仍待现场。

## 已实现任务（按严重程度、成本排序）

成本：S 小、M 中、L 大、U 待外部条件后评估。独立任务 ID 沿用审查基线；同文件修复已串行处理。

| ID | 严重程度 / 成本 | 任务 | 最终实现与证据 |
|---|---|---|---|
| FR01 | P1 / M | 编译器辅属产物可覆盖源文件 | 统一产物身份预检与随机独占临时文件原子发布；所有辅属产物及失败清理保护源文件。 [源码](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/backend/artifacts.py)；Python output_safety + CLI parity；真实 lmc.exe 与直接入口。 |
| ENV01 | P2 / S | 恢复默认启动器使用的 DSL Python 环境 | 可恢复归档坏 venv，重建仓库默认解释器并安装受控依赖；提供环境恢复脚本。 [源码](D:/Table/LH/tools/setup_compiler_env.ps1)；默认 venv 的 97 passed / 1 skipped，pip check、启动器与离线编译。 |
| FR02 | P2 / S | JSON 文本字符串中的凭据未脱敏 | 递归处理 JSON 文本与嵌套字符串、转义引号及凭据别名；日志和诊断真实接收端回归。 [源码](D:/Table/LH/src/common/LogSafety.h)；FullReviewRegressionTest + DiagnosticSnapshotTest + AppLoggingOverloadTest。 |
| FR03 | P2 / S | CSV 两种布局输出不同时间语义 | 分页/非分页 CSV、TSV 两布局统一 UTC，默认格式标注 Z；保留 epoch。 [源码](D:/Table/LH/src/monitor/MonitorExportHelper.cpp)；DST 两侧时间与两布局回归；独立 export-time 探针。 |
| FR04 | P2 / S | UDP 接收队列仅限字节，空报文可无限堆积 | UDP 队列同时限制 1024 报文和 1 MiB；过载丢弃可计数。 [源码](D:/Table/LH/src/communication/EthernetInterface.cpp)；空/一字节混合洪泛，关闭清理；独立空报文探针。 |
| FR07 | P2 / S | 工程原生文件操作允许 Windows 保留名 | 原生写入 guard 拒绝 Windows 保留设备名、扩展变体及非法字符。 [源码](D:/Table/LH/src/common/ProjectMutationGuard.h)；ProjectMutationGuardTest + 保留名零残留回归 + 独立探针。 |
| FR09 | P2 / S | OPC 成功终态 confirmed 未获得日志保留容量 | confirmed 等成功/失败/取消终态使用保留容量或 stderr 兜底；按结构化状态识别。 [源码](D:/Table/LH/src/core/AppLogging.cpp)；AppLoggingOverloadTest + 独立 terminal 探针。 |
| FR10 | P2 / S | 专家下载日志未限容量且按富文本解释输入 | 专家日志使用纯文本插入，最多 2000 块，并说明裁剪策略。 [源码](D:/Table/LH/src/designer/DownloadDockWidget.cpp)；5000 行永久回归 + 25000 行独立探针；HTML 字面显示。 |
| FR13 | P2 / S | CI 安装包沿用开启故障注入的测试构建 | CI 注入回归之后重新配置 OFF 并重建，再测试、安装和上传。 [源码](D:/Table/LH/.github/workflows/windows-ci.yml)；本机 ON 三个注入分支；OFF 属性无效回归；最终 OFF 全量和安装。 |
| FR15 | P2 / S | 导出 DTO 公共头在 Qt 5 中不能独立使用 | 公共导出 DTO 头直接包含完整 Sample 类型。 [源码](D:/Table/LH/src/monitor/MonitorExportHelper.h)；Qt 5.15 完整构建及永久测试；安装公共消费者。 |
| FR05 | P2 / M | CAN 在设备所属线程同步等待时无法处理到帧事件 | 所属线程用短事件等待片段；其他线程使用绝对期限条件等待；断开和关闭立即唤醒。 [源码](D:/Table/LH/src/communication/CANInterface.cpp)；同线程到帧/超时，跨线程到帧/断开/关闭/虚假唤醒；独立 can-wait。 |
| FR06 | P2 / M | CANopen/J1939 非法配置被截断后仍通过校验 | 配置转换先验证类型、整数和协议位范围；应用原生 CAN 过滤器，发送拒绝无效帧。 [源码](D:/Table/LH/src/communication/CommTypes.h)；非法/边界 CANopen/J1939 配置，帧拒绝；独立 can-config。 |
| FR08 | P2 / M | 调试命令通过 BlockingQueuedConnection 阻塞界面 | 调试命令异步排队，单次终态与绝对期限；会话在完成后更新暂停状态，等待期间禁用命令。 [源码](D:/Table/LH/src/communication/ControllerDeviceBackendDebug.cpp)；五种慢命令心跳、超时、取消/断开与重连；RuntimeSessionControllerTest。 |
| FR11 | P2 / M | Matrikon OPC 回调到 Qt 的积压无上限 | 回调在复制前限制 128 条、4096 项和 4 MiB；代际隔离，失败释放计数；修复入口重复加锁。 [源码](D:/Table/LH/src/communication/MatrikonOpcServer.cpp)；20000 回调最多应用 128；队列归零；独立 callback-burst。 |
| RK01 | P2 / M | 日志接收端停顿可延长关键日志、flush 与退出等待 | 关键日志和 flush 有等待预算；退出等待 1000 ms，超时 writer 使用进程所有状态及可观察兜底。 [源码](D:/Table/LH/src/core/AppLogging.cpp)；暂停接收端、过载、flush 超时、退出及重新安装子进程；物理 OS 磁盘停顿待现场。 |
| RK02 | P2 / M | 导出线程退出是否先于应用全局资源销毁尚缺闭环 | 导出由进程级注册表持有，关闭取消并等待；应用全局销毁前确认结束，超时采用有日志的进程退出。 [源码](D:/Table/LH/src/core/BackgroundTaskRegistry.cpp)；窗口关闭覆盖快照前/数据库页/原子发布；原文件不变，连接与线程结束。 |
| RK03 | P2 / M | 旧数据库启动迁移仍同步扫描全表 | 启动数据库预检/迁移在独立 SQL 线程执行，进度及取消可反馈；逐版本事务回滚，按主键稳定扫描。 [源码](D:/Table/LH/src/core/DataManager.cpp)；新/小/10万行旧库；取消/无时区/回调异常回滚；GUI 心跳。 |
| T05-PORT | P2 / M | Windows 网络共享目录（UNC）的工程安全文件操作 | Windows UNC 共享根句柄、相对打开与原子发布；按文件身份核对父子目录，自动清理的保护文件阻止服务器端目录重解析竞争。 [源码](D:/Table/LH/src/common/ProjectMutationGuard.h)；真实本机 SMB：创建/替换/重命名/递归删除、冲突清理、既有 junction、普通与仅属性写入的竞争攻击；工程保存和删除回滚。实际 NAS/远端服务器故障待现场。 |
| FR12 | P2 / L | Matrikon COM 建链、探测和写入仍在界面调用链同步执行 | 原生 OPC 由独立 STA 线程创建、调用和销毁；异步状态、期限、取消、队列与值内存限制；退出保留超时线程。 [源码](D:/Table/LH/src/communication/AsyncOpcServer.cpp)；慢/失败测试桩、同线程创建/调用/销毁、取消与旧代隔离；真实 DA 待现场。 |
| DQ01 | P3 / S | 合并 OPC 写结果状态更新策略 | 共享 OPC 写结果状态策略，协议映射保持各自实现。 [源码](D:/Table/LH/src/communication/OpcWriteResultState.h)；两个后端成功/失败计数、时间和快照一致。 |
| DQ02 | P3 / S | 统一编译与运行校验的祖先路径策略 | 编译与运行复用最近存在祖先的规范路径策略。 [源码](D:/Table/LH/src/common/PathSecurityUtils.h)；DSL 路径安全与运行现有回归；ModuleBoundaryCheck。 |
| DQ03 | P3 / S | 统一 Monitor 后端连接释放逻辑 | 采样组件统一断开旧后端信号与代际取消。 [源码](D:/Table/LH/src/monitor/BackendSampler.cpp)；重绑、停止、后端销毁、迟到回调回归。 |
| DQ05 | P3 / S | 核实旧 QProcess 兼容槽的保留价值 | 核实后删除未使用的旧 QProcess 兼容槽及成员依赖。 [源码](D:/Table/LH/src/designer/BuildController.h)；完整构建、异步编译与取消回归。 |
| DQ07 | P3 / S | 建立当前审查状态索引，避免历史目录与结论误用 | 建立当前状态入口，指向审查基线、当前修复与历史证据，并说明工作区与验收界限。 [源码](D:/Table/LH/review_execution/CURRENT_REVIEW.md)；当前 57 项状态与实际本轮证据交叉校验。 |
| FR14 | P3 / S | JSON 导出将无符号 64 位值转换为负数 | JSON 大整数超过 IEEE-754 安全范围时用十进制字符串，无符号和嵌套字段保持精度。 [源码](D:/Table/LH/src/monitor/MonitorExportHelper.cpp)；0/负数/LLONG_MAX/MIN/以上值/ULLONG_MAX及嵌套；独立 export-time。 |
| DQ06 | P3 / M | 共享两个编译器 CLI 的业务入口 | 两个 CLI 使用共同编译业务服务，保留各自公开命令契约。 [源码](D:/Table/LH/third_party/custom_dsp_language/compile/src/lh_compiler/cli/service.py)；四组合成功/失败/check/compile及硬链接；实际安装 lmc.exe。 |
| UI-DARK | P3 / M | 实现真正的 Dark 主题 | 独立 Dark palette，动态内联样式自动适配并保留原样式；图表同步背景、轴、图例与工具条，切回 Light 恢复。 [源码](D:/Table/LH/src/designer/ui/ThemeManager.cpp)；已有/新建控件、内联样式变化、Light 恢复及图表数据回归；10 张截图；物理桌面待现场。 |
| DQ04 | P3 / L | 继续收敛 MonitorManager 的采样与存储协调职责 | 抽出 QtCore BackendSampler；MonitorManager 保持协调门面，存储端口可显式注入。 [源码](D:/Table/LH/src/monitor/BackendSampler.h)；独立 sampler 假后端；MonitorManager/历史时序回归；模块边界。 |

## 未完成任务（继续独立记账）

| ID | 严重程度 / 成本 | 任务 | 阻塞与下一步 |
|---|---|---|---|
| C02 | P1 / U | 分离构造参数、运行时字段、地址引用和无实例操作 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| T03 | P1 / U | 实现标量初始化的 LH 目标写回能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| T04 | P1 / U | 实现赋值对目标变量的真实写回 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| C04 | P2 / U | 实现标量运行时表达式、拷贝和成员读写 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| C05 | P2 / U | 实现 IF/CASE/循环与跳转的正确代码生成 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| C06 | P2 / U | 支持明确声明的重复参数组和长度可变契约 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FIELD-ACCEPTANCE | P2 / U | 设备、真实 OPC DA、远端网络共享、物理桌面和长时间故障验收 | 待现场条件；按固定设备/固件/服务版本执行独立探针、断线/重连、压力、退出和物理桌面检查，记录实际证据；UNC 本机 SMB 已验证，实际 NAS/远端服务器还需验证协议能力、ACL 权限、断线/重连与长期运行。 |
| C07 | P3 / U | 补齐数组布局、下标与元素读写 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB01 | P3 / U | 恢复并核实 KeyScan 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB02 | P3 / U | 恢复并核实 SCIDispTrans 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB03 | P3 / U | 恢复并核实 SCIDispInit 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB04 | P3 / U | 恢复并核实 M600TextDisp 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB05 | P3 / U | 恢复并核实 M600ProgressBar 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB06 | P3 / U | 恢复并核实 EXCACycDisp 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB07 | P3 / U | 恢复并核实 FilterBW 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB08 | P3 / U | 恢复并核实 Task 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB09 | P3 / U | 恢复并核实 TaskPeriodic 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB10 | P3 / U | 恢复并核实 TaskWake 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB11 | P3 / U | 恢复并核实 TaskDataWake 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB12 | P3 / U | 恢复并核实 TaskLock 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB13 | P3 / U | 恢复并核实 TaskUnlock 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB14 | P3 / U | 恢复并核实 TaskSemDef 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB15 | P3 / U | 恢复并核实 TaskSemWait 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB16 | P3 / U | 恢复并核实 TaskSemPost 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB17 | P3 / U | 恢复并核实 TaskEnd 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB18 | P3 / U | 恢复并核实 TSO 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB19 | P3 / U | 恢复并核实 TSOAutoTune 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB20 | P3 / U | 恢复并核实 TwoPosition 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |
| FB21 | P3 / U | 恢复并核实 RelayCtrl2 的 LH 契约，补齐已证明的能力 | 外部 LH 契约不足；取得适用 LH 固件版本的指令、操作数、地址/数值单位及执行语义证据，再实现正反例并验证目标设备。LM 历史资料仅作线索。 |

## 工程与验收边界

- Windows UNC：打开共享根后沿句柄获取子项，核对目录父子文件身份；以独占、自动清理的保护文件保持父目录非空，拒绝服务器端普通写或仅属性写权限的 reparse 竞争。服务端缺少安全身份/枚举/重命名/删除能力时拒绝操作。实际 NAS/远端服务端及断线行为仍需现场验证。协议依据：[SMB2重命名约束](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/52aa0b70-8094-4971-862d-79793f41e6a8)、[目录重解析规则](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fsa/4aeefef8-92c3-4abc-af7a-a610caf8a165)、[按名称查询目录身份](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntquerydirectoryfile)。
- FR12 原生 COM 在工作线程建立 STA、启用调用取消并平衡释放；慢/失败桩验证线程一致性。真实 DA 版本、非协作 RPC 取消及长时重连须现场验证。契约参考：[Microsoft COM初始化](https://learn.microsoft.com/en-us/windows/win32/learnwin32/initializing-the-com-library)、[调用取消启用](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coenablecallcancellation)、[CoCancelCall](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cocancelcall)。
- RK01/RK02 预算覆盖队列、writer 等待与导出任务注册/清理；不可中断 OS 文件调用、stderr 物理接收端及磁盘故障未由暂停 writer 测试证明。退出超时保留进程所有状态，主程序在 Qt 全局销毁前采取有诊断的进程退出。
- FR14 JSON 整数绝对值超过 2^53-1 时用十进制字符串（含嵌套），消费者应按十进制整数解析。默认 CSV/TSV UTC 文本含 Z，自定义格式也接收 UTC。
- 默认 venv 已恢复，损坏环境保留于证据目录 archived-venv。恢复入口：[setup_compiler_env.ps1](D:/Table/LH/tools/setup_compiler_env.ps1)。
- 复测捕获并修正 OPC 回调重复锁、工程对象先销毁后的会话悬空访问及异步 OPC 默认快照标识。早期失败/终止日志保留用于诊断，未作为通过证据。
- 完整性：基线哈希 348 项，本轮改变 63 项，新增哈希 17 项；基线文件无丢失。[哈希证据](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-repair-2026-10-01/source-hashes-after.json)。

[机器可读57项台账](D:/Table/LH/review_execution/LH-full-review-repair-2026-10-01.json)；[当前状态入口](D:/Table/LH/review_execution/CURRENT_REVIEW.md)；[修复前审查](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-full-review-2026-10-01/LH-full-review-2026-10-01.md)。
