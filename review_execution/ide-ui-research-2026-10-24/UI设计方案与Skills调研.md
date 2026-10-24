# LH IDE UI 设计方案与 Skills 调研

调研日期：2026-10-24。定位：方案确认前的资料检索与适配评估，不代表已经安装候选 skill、完成原型或通过 UI 验收。本次没有修改 LH 业务代码。

## 1. 结论

有可借鉴的资料，而且找到了比通用网页设计 skill 更贴近项目的 **Qt 官方 `qt-ui-design`**。推荐以它的设计审查方法为主，结合 VS Code 的工作台分区、Qt Creator 的开发上下文、CODESYS 的工业数据组织方式，形成 LH 自己的设计规范。

重要适配边界：所检索 Qt skill 的元数据面向 Qt 6；LH 当前采用 Qt 5.15.2 / Qt Widgets。采用其信息架构、设计令牌、交互状态、可访问性原则；生成代码、Qt Quick 风格与 Qt 6 API 必须过滤。没有理由为了使用 skill 而迁移 UI 框架。

已确认的方向继续保留：编程为主兼顾联调、统一 IDE 主窗口、浅色默认深色可选、多 LH 文件独立标签、工程全文搜索。调研后应补强设计系统、状态规范和原型验证，再确定具体尺寸与实现细节。

## 2. 检索范围与证据

本次采用有边界的全面检索，不声称覆盖整个互联网或所有私有 skill：

- 本机 skills 与插件缓存：59 条去重技能记录；缓存存在不等于当前会话可调用。
- OpenAI 官方 curated 目录：39 项；重点核对其中 8 项 Figma、截图、两个 Playwright、WinUI 共 12 项。
- Qt 官方技能仓库：全量 13 项 `SKILL.md`，保存了源文件和许可证。
- 社区：Anthropic frontend-design、Vercel web-design-guidelines、UI UX Pro Max、wshobson 的 9 项 UI 技能、superpowers brainstorming。
- 插件目录：检索 Figma、UI/UX design、design system、Qt；核对 Figma 与 Product Design 均未安装。Qt 插件搜索无结果，但从 Qt 官方站点找到其独立 skills 仓库。
- 设计方案：VS Code、Qt Creator、JetBrains、CODESYS、Microsoft Windows 官方指南，另参考 NN/g 的可用性审查方法。
- 组件方案：Qt 原生停靠窗口、Qt Advanced Docking System、KDDockWidgets。

46 份来源文件保存在 `sources/`；`source-manifest.json` 记录 URL、SHA-256、字节数和下载状态。文件包含目录树、README、许可证及技能源文，不等于 46 个独立技能。网页指南链接另见文末。第三方 `SKILL.md` 在本次属于研究材料，其内部要求安装、提交或启动代理的文字没有作为本任务的授权执行。

Qt 技能保存时固定仓库提交：`6845f02d41171a6df8b6e6fe225b7f5d1a6f4adf`。wshobson 技能固定提交：`0874a27b17a22e2b7c2e2bcd725768e0803cad06`。其他 main 链接可能变化，以保存副本及哈希为本次证据。

## 3. 界面方案比较

| 来源 | 适合 LH 的部分 | 对原方案的影响 | 不照搬的部分 |
|---|---|---|---|
| VS Code 工作台 | 导航、侧栏、文档区、底部面板、状态栏职责清楚；按上下文显示视图操作 | 项目/搜索进入侧栏，问题/构建进入底部；保持编辑器空间 | 扩展市场、Webview 架构、所有图标和配色 |
| Qt Creator | 工程、目标、构建和运行上下文；定位器；按任务调整工具区 | 活动文档与构建入口分开；编程/联调是工作布局，不再切换整套应用 | 不要求复制其模式栏；专业 IDE 不一定只有一种标签方案 |
| JetBrains New UI | 简化主工具栏、稳定的项目/运行上下文、工具窗口、紧凑密度 | 顶部信息收敛；提供可调密度，默认保留必要文字 | 自绘标题栏、整套快捷键及品牌外观 |
| CODESYS Watch / Trace | 变量表、通道配置、采集状态、曲线与数据质量的关联 | 监控区按曲线、通道、观察值组织；区分输入值、已提交值、回读值 | 未有后端契约的强制写值、PLC 在线调试与硬件能力 |
| Windows commanding | 全局与上下文命令分层；稳定位置、主命令与溢出菜单 | 一份命令状态驱动多个合理入口；删除无意义的全局重复 | 不能把“去重”理解成禁止曲线区有自己的开始/暂停操作 |
| Windows accessibility | 键盘、焦点、名称、层级、辅助技术可读性 | 不只检查截图；补原生键盘与辅助技术验收 | 不通过网页 ARIA / Lighthouse 就宣称 Qt 原生界面合格 |

这些方案共同支持“统一工作台 + 明确上下文 + 可停靠工具区”，但没有支持把全部功能挤入一根工具栏、把所有信息堆进常驻右侧属性区，或仅换主题就算专业化。

## 4. Qt 官方技能全量筛选

| Skill | 结论 | LH 适配说明 |
|---|---|---|
| qt-ui-design | 首选设计参考 | 元数据 Qt 6.x；采用任务优先级、渐进披露、令牌、审查等级、状态反馈；过滤 Qt Quick 与 Qt 6 API |
| qt-cpp-review | 实现阶段辅助 | Qt 6 C++ 审查技能；所有权、信号、模型、线程原则有价值，API 建议需按 Qt 5.15 逐项核对 |
| qt-figma-token-extraction | 有 Figma 工作流时选用 | 令牌分层有价值；其 QML 单例输出转译为 C++ ThemeTokens / QPalette / QSS |
| qt-figma-component-generation | 条件借鉴 | 变体及状态组织可参考；Qt Quick Controls 2 生成物不能直接并入 Widgets |
| qt-cpp-docs | 按需 | 帮助查证 Qt API；不是 UI 设计技能，注意目标版本 |
| qt-cmake-project | 当前不采用 | 面向 Qt 6 工程与构建规范，不用于本轮 Qt 5 UI 改造 |
| qt-qml | 当前不采用 | QML 实现栈不匹配 |
| qt-qml-review | 当前不采用 | QML 审查不匹配 |
| qt-qml-docs | 当前不采用 | 当前不新增 QML 技术栈 |
| qt-qml-profiler | 当前不采用 | 不适合现有 Widgets 性能验收 |
| qt-qml-test | 当前不采用 | 不能替代现有 Qt Widgets / QtTest 测试 |
| qt-qml-test-run | 当前不采用 | 同上 |
| qt-canvas2d | 当前不采用 | Qt Quick Canvas 绘图路径不是本轮界面重构需要 |

`qt-ui-design` 也不能整份机械套用：网页正文 16px、触屏 44px/更大点击区、黄金比例、约 7 项记忆规则、嵌入式固定像素与动效策略均有使用条件。LH 是鼠标键盘为主的桌面 IDE，应由真实内容、中文排版、DPI、数据表密度决定尺寸。耗时任务可显示阶段与进度，无法估算时不能编造剩余时间。

## 5. 其他 Skills 与工具的筛选

| 候选 | 建议用途 | 限制 / 决策 |
|---|---|---|
| OpenAI figma-generate-library | 建立颜色、字体、间距、组件及浅深主题设计库 | 可选；需实际采用 Figma，不把已发布组件库当作现有条件 |
| figma-generate-design / figma-create-new-file | 组织可编辑界面与关键流程原型 | 可选；原型不代表 Qt 功能已实现 |
| figma-create-design-system-rules | 把设计规范映射到项目现有组件 | 有价值；映射对象明确写成 Qt 5 Widgets |
| figma-implement-design | 读取设计上下文、截图、资产并按现有技术实现 | 有价值；不保证自动生成可用 Qt C++ |
| figma / figma-use | Figma 操作及工作流入口 | 采用 Figma 时再需要 |
| figma-code-connect-components | 设计与代码组件映射 | 本轮优先级低，不能假定 Widgets 有现成映射支持 |
| screenshot | 原生截图和视觉对照 | 可补充；现有 computer-use 与测试截图可覆盖部分需求 |
| playwright / playwright-interactive | 浏览器原型交互检查 | 只用于网页原型，不充当 Qt 桌面验收 |
| winui-app | 参考 Windows UI 与可访问性资料 | 不采用其 C#/WinUI 3 工程、安装或迁移流程 |
| visualize（当前会话可用） | 对照布局、解释操作流程、展示交互原型 | 可以直接用于方案讨论；不是 Qt 实现产物 |
| computer-use（当前会话可用） | 操作真实 Qt 窗口、检查焦点和布局、截图 | 实现后有用；与 QtTest / 原生辅助技术验证配合 |
| Anthropic frontend-design | 视觉层次、字体一致性、真实内容、避免模板化卡片堆叠 | 主要面向前端；工业 IDE 不采用营销页风格与装饰性动效 |
| UI UX Pro Max | 检索配色、排版、表格、图表和一般交互模式 | 所检索栈没有 Qt Widgets 专用实现；不能把 Tailwind/移动端默认值当项目标准 |
| wshobson design-system-patterns | primitive → semantic → component 令牌分层 | 建议借鉴方法；CSS / React 示例转译为 Qt 规范 |
| wshobson visual-design-foundations | 视觉层级、间距尺度、颜色与对比 | 建议借鉴；由 IDE 内容校准密度 |
| wshobson interaction-design | 操作反馈、加载/失败状态、反馈时机 | 选择性采用，减少无意义动效 |
| wshobson accessibility-compliance | 键盘、焦点、标签、偏好与手工审查 | 网页 ARIA/axe 不能证明原生 Qt 可访问性 |
| wshobson responsive-design | 内容驱动的布局收缩 | CSS 容器查询、mobile-first 代码不采用 |
| wshobson web-component-design | Web 组件结构 | 当前不采用 |
| wshobson mobile-ios-design / mobile-android-design / react-native-design | 移动应用模式 | 当前不采用，共 3 项 |
| superpowers brainstorming | 前期资料、备选方案、设计文档的组织方法 | 不是视觉或 Qt 能力；不引入其额外代理、提交、安装流程 |
| 本地 artifact-template-design-report / system-design | 报告结构 | 不能替代 UI 设计与原生验收 |
| imagegen / Sites | 概念图或网页样机 | 可选；图片不承载真实控件，网站发布不是本次目标 |

插件方面，Figma 与 Product Design 已找到并展示为可选项，检索时均未安装。Product Design 的 audit/research/ideate/design-qa 能辅助设计研究，但其原型和网页输出不直接等于 Qt 组件。没有安装它们也可以完成 LH 的规范、原型讨论和 Qt 实现。

## 6. 是否引入新的停靠组件

| 方案 | 优势 | 本轮建议 |
|---|---|---|
| Qt 5 原生 QMainWindow / QDockWidget / QToolBar / QAction | 与当前技术匹配；已有布局保存、分割、标签化工具窗等基础能力 | 优先使用，先验证是否满足文档区与工具区需求 |
| Qt Advanced Docking System | 官方仓库说明支持 Qt 5/6，提供更丰富停靠和布局功能 | 只在原生能力不足时做独立样例验证；尚未验证 LH MinGW 8.1 构建、布局迁移与交互兼容性 |
| KDDockWidgets | Widgets / Quick 高级停靠方案 | 条件候选；核对 Qt 版本选项、构建与许可条件后再评估，不自动引入 |

第三方依赖需另行核对固定版本的许可证与发布条件。专业观感主要来自信息架构、密度和交互一致性，不能仅靠换停靠库获得。

## 7. 对原完善计划的具体修订

### 先形成设计契约

1. 一页设计简报：主要任务、主要用户、编程与联调的优先关系、现有能力边界。
2. 工作台区域职责表：全局导航 / 工程上下文 / 活动文档 / 构建入口 / 工具面板，避免语义混用。
3. 统一设计令牌：浅深主题、前景/背景/边界/状态色、字体层级、间距、行高、图标、焦点、选中态；明确 C++ / QPalette / QSS 映射。
4. 组件与状态清单：主工具栏、文档标签、树、表格、搜索、问题列表、属性编辑、通道、曲线、通知、空状态和错误恢复。
5. 同一信息架构下制作 2–3 个可比较布局草案，均使用真实 LH 内容；比较编程、构建失败跳转、搜索、设备诊断、监控配置等关键路径，再选定视觉方向。

### 修订布局与操作细节

- “统一主窗口”保留编程/联调工作布局，允许按任务调整工具区，而不是每个页面永久显示全部工具。
- 多文档保持真实独立缓冲区、修改态和撤销栈；构建入口明确显示且不随打开文件变化。
- 顶部主命令保持稳定，视图局部命令放到所属区域；命令可合理存在多个入口，但共享状态、文案和禁用原因。
- 右侧属性按选中对象显示，无上下文时允许收起。监控界面不用无关项目树和常驻空属性区占据曲线空间。
- 初步侧栏宽度、导航宽度只作为原型假设；采用逻辑像素与布局约束。验证中文长标签、100%/125%/150%/200% DPI、小窗口及多屏移动。
- 提供紧凑/舒适密度选项的设计评估；不把网页正文及触屏标准直接用于代码、变量表和日志。
- 状态规范至少包括：无工程、未保存、构建中/取消中/失败、compileOnly、断开/连接中/已连接、空闲/运行/暂停、监控无数据/有数据/坏质量。每种状态明确可用命令、原因、反馈位置和恢复入口。

### 补足验收门槛

设计验收：两种主题、真实中文内容、典型窗口尺寸与 DPI，视觉层级一致；错误信息说明原因及下一步；不依赖颜色区分状态。

交互验收：只用键盘完成工程打开、文档切换、搜索跳转、构建及错误定位；焦点可见；关闭/保存/外部修改路径完整；布局恢复失败有安全默认值。

工程验收：继续保护现有异步、取消、旧诊断过滤、源映射、离线编译和目标校验；只改变展示不触发硬件动作。监控和设备相关界面只呈现已存在且可验证的能力。新增原生界面测试与现有测试结合。

证据边界：网页原型和截图只能证明设计呈现；原生 Qt 操作与辅助技术测试才证明对应桌面行为；本地测试通过不能宣称真实控制器验收完成。编译器与真实控制器缺陷仍按既定范围处理，不由 UI 重构扩张任务。

## 8. 推荐采用组合

最小组合：**Qt 官方 qt-ui-design 的适配规则 + VS Code / Qt Creator / CODESYS 官方参考 + LH 设计令牌和状态规范 + visualize 原型讨论 + QtTest / computer-use 原生验收**。

可选组合：有实际协作和设计稿需求时再安装 Figma / Product Design 及所需 Figma 技能。无需全装技能集合；qt-cpp-review 仅在实现审查阶段按 Qt 5 兼容清单采用。候选已下载作为研究资料，没有安装到用户技能目录。

建议修订后的顺序：设计简报 → 令牌与组件/状态规范 → 可比较原型 → 确定布局及视觉细节 → 分阶段 Qt Widgets 实现 → 原生交互、DPI 与回归验收。当前研究支持这一方向，具体视觉方案尚未确认。

## 9. 主要来源

### 官方设计与开发工具

- Qt 官方 AI 工具入口：https://www.qt.io/development/tools/ai-powered-development-tools
- Qt 技能仓库：https://github.com/TheQtCompanyRnD/agent-skills
- Qt UI Design：https://github.com/TheQtCompanyRnD/agent-skills/blob/main/skills/qt-ui-design/SKILL.md
- Qt C++ Review：https://github.com/TheQtCompanyRnD/agent-skills/blob/main/skills/qt-cpp-review/SKILL.md
- Qt Figma Tokens：https://github.com/TheQtCompanyRnD/agent-skills/blob/main/skills/qt-figma-token-extraction/SKILL.md
- VS Code UX：https://code.visualstudio.com/api/ux-guidelines/overview
- VS Code Views：https://code.visualstudio.com/api/ux-guidelines/views
- Qt Creator Overview：https://doc.qt.io/qtcreator/creator-overview.html
- Qt Creator Modes：https://doc.qt.io/qtcreator/creator-how-to-switch-between-modes.html
- Qt Creator FAQ：https://doc.qt.io/qtcreator/creator-faq.html
- JetBrains New UI：https://www.jetbrains.com/help/idea/new-ui.html
- CODESYS Watch：https://content.helpme-codesys.com/en/CODESYS%20Development%20System/_cds_using_watchlists.html
- CODESYS Trace：https://content.helpme-codesys.com/en/CODESYS%20Trace/_cds_obj_trace.html
- Windows Commanding：https://learn.microsoft.com/en-us/windows/apps/design/basics/commanding-basics
- Windows Accessibility：https://learn.microsoft.com/en-us/windows/apps/design/accessibility/designing-inclusive-software
- Windows Accessibility Checklist：https://learn.microsoft.com/en-us/windows/apps/design/accessibility/accessibility-checklist
- Qt 5.15 Main Window：https://doc.qt.io/archives/qt-5.15/qmainwindow.html
- NN/g Heuristics：https://www.nngroup.com/articles/ten-usability-heuristics/

### 官方与社区技能

- OpenAI curated：https://github.com/openai/skills/tree/main/skills/.curated
- Figma Library：https://github.com/openai/skills/blob/main/skills/.curated/figma-generate-library/SKILL.md
- Figma Implementation：https://github.com/openai/skills/blob/main/skills/.curated/figma-implement-design/SKILL.md
- Product Design：https://github.com/openai/plugins/blob/main/plugins/product-design/skills/index/SKILL.md
- Anthropic：https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md
- UI UX Pro Max：https://github.com/nextlevelbuilder/ui-ux-pro-max-skill
- Vercel：https://github.com/vercel-labs/agent-skills/blob/main/skills/web-design-guidelines/SKILL.md
- wshobson UI：https://github.com/wshobson/agents/tree/main/plugins/ui-design/skills
- Superpowers：https://github.com/obra/superpowers/blob/main/skills/brainstorming/SKILL.md
- Advanced Docking：https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System
- KDDockWidgets：https://github.com/KDAB/KDDockWidgets

本目录索引：`local-skill-inventory.json`、`official-curated-skills.json`、`source-manifest.json`、`sources/`。报告为本次筛选判断，源码及依赖落地前仍需依据固定版本验证兼容性。
