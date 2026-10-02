# 演示界面文案清理

日期：2026-10-02。按用户要求清理软件界面中的开发状态说明，保留当前工作区其他修复。未提交、未推送、未修改模型选择。

## 显示结果

- 编译成功行显示 `编译成功：<输出目录>`，不再附加赋值写回或目标固件验证说明。
- 该成功行记为普通信息，不再作为警告；对应界面回归确认错误 0、警告 0、信息 1。
- 编译输出页只过滤本条内部状态通知；其他警告和错误继续正常显示。原始编译结果及产物报告仍保留内部状态。
- 函数库与编辑器函数列表保留全部函数及原名称，悬浮提示不显示开发状态或待完善原因。
- 选中函数块的右侧属性保留 ID、分类、单位、描述及采样周期，去掉“契约状态”和“待完善原因”。
- 操作被阻止时使用面向用户的说明，如“此编译结果不适用于控制器下载或运行”，不再显示固件开发状态文字。

本次调整没有改变指令生成、类型检查、产物状态标记、旧下载引用清理、下载 manifest 发布条件或串口写入限制。不可插入的函数块仍不能通过双击或拖拽插入。

## 验证

完整原生构建通过；`git diff --check` 通过。

相关回归包括 MainWindowIntegrationTest、DownloadManagerLifecycleTest、DslCompilerSemanticsTest、ControllerDeviceBackendTest、RuntimeSessionControllerTest、DslCompletionEngineTest、DslScriptEditorSaveTest、SnippetRepositoryTest 和 ProjectSaveCloseTest。

首次 9 个目标运行中，8 个通过；界面测试的新增断言要求成功信息必须生成详情文件，因此失败。正常短信息不生成详情文件，这不是产品故障。修正检查条件后，只复跑受影响的完整界面目标，33 项通过、0 失败、3 项跳过，CTest 目标通过。其余 8 个目标的生产代码未再改变，无需重复执行。本轮没有重新运行整个 45 目标套件。

界面回归实际触发普通编译，核对成功行的完整文本、输出页无附加说明、旧下载引用清理、配置持久化和真实警告／错误未被过滤。函数库回归核对全部条目、悬浮提示、实际鼠标选择后的属性，以及插入限制。Qt offscreen 截图与测试日志保留；未据此宣称物理桌面或控制器验收完成。

新程序 `--version`、`--smoke-test` 均通过。启动检查前后数据库保持 `schemaVersion=5`、`runtimeRecords=16531`。

## 部署

原启动路径已更新：[LH.exe](D:/Table/LH/build_current_mingw/bin/LH.exe)。保存并关闭当前旧窗口后，从此路径重启即可。

旧程序保留为 [LH.before_demo_ui_20261002_221756.exe](D:/Table/LH/build_current_mingw/bin/LH.before_demo_ui_20261002_221756.exe)。更新时未停止当前用户进程；已核对该进程仍在运行。

新程序 SHA-256：`02EA351D5E44CC1A72BFAE6E08713DA5B3EE40F986B9419AD6723DA3DF7AE29A`。

旧程序备份 SHA-256：`ABFAED99933B98DBC345E4637CAB65789E147A26C1931B8D24281B31027C4140`。

原 CMake 链接脚本已逐字节恢复，Git HEAD 保持 `bed4d849ebebd9e6232a8d727e037005ec507d00`。

[验证证据目录](C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f/LH-demo-ui-2026-10-02) 包含源码与程序基线、构建日志、首次回归与界面复查日志、截图以及 `deployment-verification.json`。
